#include "RawIron/Content/GamePackageRequirements.h"

#include "RawIron/Content/GameManifest.h"
#include "RawIron/Core/Detail/JsonScan.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace ri::content {
namespace {

namespace fs = std::filesystem;
namespace detail_scan = ri::core::detail;

[[nodiscard]] bool IsSafeToken(const std::string_view value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](const char character) {
        const unsigned char code = static_cast<unsigned char>(character);
        return std::isalnum(code) != 0 || character == '.' || character == '_' || character == '-';
    });
}

[[nodiscard]] std::string HostPlatform() {
#if defined(_WIN32)
    return "windows-x64";
#elif defined(__linux__)
    return "linux-x64";
#elif defined(__APPLE__)
    return "macos";
#else
    return "unknown";
#endif
}

void AppendUniqueTokens(
    std::vector<std::string>& destination,
    const std::vector<std::string>& source,
    const char* field,
    std::vector<std::string>& issues) {
    std::set<std::string, std::less<>> seen(destination.begin(), destination.end());
    for (const std::string& value : source) {
        if (!IsSafeToken(value)) {
            issues.push_back(std::string("dependencies.json ") + field + " contains an invalid token.");
        } else if (seen.insert(value).second) {
            destination.push_back(value);
        } else {
            issues.push_back(std::string("dependencies.json ") + field + " contains duplicate token '" + value + "'.");
        }
    }
}

} // namespace

GamePackageRequirements LoadGamePackageRequirements(const fs::path& gameRoot) {
    GamePackageRequirements requirements{};
    requirements.platform = HostPlatform();
    const std::string text = detail_scan::ReadTextFile(gameRoot / "assets" / "dependencies.json");
    if (text.empty()) {
        // Package declarations are additive. Older games have no dependencies document,
        // and an empty document simply means that this game has no package roots.
        return requirements;
    }

    std::set<std::string, std::less<>> seenPackages;
    const auto appendRequirement = [&](GamePackageRequirement requirement) {
        if (!IsSafeToken(requirement.packageId)) {
            requirements.issues.push_back("dependencies.json package id is invalid.");
            return;
        }
        if (!IsValidPackageVersionRequirement(requirement.versionRequirement)) {
            requirements.issues.push_back(
                "dependencies.json package '" + requirement.packageId + "' has an invalid version requirement.");
            return;
        }
        if (!seenPackages.insert(requirement.packageId).second) {
            requirements.issues.push_back(
                "dependencies.json lists package '" + requirement.packageId + "' more than once.");
            return;
        }
        requirements.packages.push_back(std::move(requirement));
    };

    for (const std::string& packageId : detail_scan::ExtractJsonStringArray(text, "packages")) {
        appendRequirement({.packageId = packageId});
    }
    for (const std::string_view object : detail_scan::SplitJsonArrayObjects(text, "packages")) {
        appendRequirement({
            .packageId = detail_scan::ExtractJsonString(object, "id").value_or(
                detail_scan::ExtractJsonString(object, "packageId").value_or("")),
            .versionRequirement = detail_scan::ExtractJsonString(object, "version").value_or(
                detail_scan::ExtractJsonString(object, "versionRequirement").value_or("*")),
            .optional = detail_scan::ExtractJsonBool(object, "optional").value_or(false),
        });
    }

    AppendUniqueTokens(
        requirements.engineCapabilities,
        detail_scan::ExtractJsonStringArray(text, "capabilities"),
        "capabilities",
        requirements.issues);
    AppendUniqueTokens(
        requirements.grantedPermissions,
        detail_scan::ExtractJsonStringArray(text, "permissions"),
        "permissions",
        requirements.issues);
    requirements.engineApiVersion =
        detail_scan::ExtractJsonString(text, "engineApiVersion").value_or("1.0.0");
    requirements.platform =
        detail_scan::ExtractJsonString(text, "platform").value_or(requirements.platform);
    if (!PackageVersionSatisfies(requirements.engineApiVersion, requirements.engineApiVersion)) {
        requirements.issues.push_back("dependencies.json engineApiVersion must be a semantic version triplet.");
    }
    if (!IsSafeToken(requirements.platform)) {
        requirements.issues.push_back("dependencies.json platform is invalid.");
    }
    return requirements;
}

std::vector<InstalledAssetPackage> DiscoverGamePackageCatalog(const fs::path& gameRoot) {
    const fs::path workspaceRoot = DetectWorkspaceRoot(gameRoot);
    const std::vector<fs::path> roots = {gameRoot, workspaceRoot / "Assets"};
    std::set<std::string, std::less<>> identities;
    std::vector<InstalledAssetPackage> catalog;
    for (const fs::path& root : roots) {
        for (const fs::path& manifestPath : FindAssetPackageManifestPaths(root)) {
            const std::optional<AssetPackageManifest> manifest = LoadAssetPackageManifest(manifestPath);
            if (!manifest.has_value()) {
                continue;
            }
            const std::string identity = manifest->packageId + "@" + manifest->packageVersion;
            if (!identities.insert(identity).second) {
                continue;
            }
            catalog.push_back({
                .manifestPath = manifestPath,
                .packageRoot = manifestPath.parent_path(),
                .manifest = *manifest,
            });
        }
    }
    return catalog;
}

GamePackageMountReport MountDeclaredGamePackages(
    PackageMountRegistry& registry,
    const fs::path& gameRoot) {
    GamePackageMountReport report{};
    report.requirements = LoadGamePackageRequirements(gameRoot);
    report.issues = report.requirements.issues;
    if (!report.requirements.valid()) {
        report.requiredPackagesMounted = false;
        return report;
    }

    PackageResolverOptions options{};
    options.engineApiVersion = report.requirements.engineApiVersion;
    options.platform = report.requirements.platform;
    options.engineCapabilities = report.requirements.engineCapabilities;
    options.grantedPermissions = report.requirements.grantedPermissions;
    const std::vector<InstalledAssetPackage> catalog = DiscoverGamePackageCatalog(gameRoot);
    std::vector<PackageRequest> requiredRoots;
    std::vector<GamePackageRequirement> optionalRoots;
    for (const GamePackageRequirement& requirement : report.requirements.packages) {
        if (requirement.optional) {
            optionalRoots.push_back(requirement);
        } else {
            requiredRoots.push_back({requirement.packageId, requirement.versionRequirement});
        }
    }

    if (!requiredRoots.empty()) {
        PackageActivationResult activation = registry.Activate(catalog, requiredRoots, options);
        report.activations.push_back(activation);
        if (!activation.activated) {
            report.requiredPackagesMounted = false;
            report.issues.insert(report.issues.end(), activation.issues.begin(), activation.issues.end());
            return report;
        }
    }
    for (const GamePackageRequirement& requirement : optionalRoots) {
        PackageActivationResult activation = registry.Activate(
            catalog,
            std::vector<PackageRequest>{{requirement.packageId, requirement.versionRequirement}},
            options);
        if (!activation.activated) {
            report.issues.push_back("Optional package '" + requirement.packageId + "' was not mounted.");
            report.issues.insert(report.issues.end(), activation.issues.begin(), activation.issues.end());
            continue;
        }
        report.activations.push_back(std::move(activation));
    }
    return report;
}

void ReleaseDeclaredGamePackages(
    PackageMountRegistry& registry,
    const GamePackageMountReport& report) {
    for (auto activation = report.activations.rbegin(); activation != report.activations.rend(); ++activation) {
        if (activation->activated) {
            (void)registry.Deactivate(activation->activationId);
        }
    }
}

} // namespace ri::content
