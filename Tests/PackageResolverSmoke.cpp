#include "RawIron/Content/AssetPackageManifest.h"
#include "RawIron/Content/PackageResolver.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

ri::content::AssetPackageManifest Package(
    std::string id,
    std::string version,
    std::vector<ri::content::AssetPackageDependency> dependencies = {}) {
    ri::content::AssetPackageManifest manifest{};
    manifest.packageId = std::move(id);
    manifest.displayName = manifest.packageId;
    manifest.packageVersion = std::move(version);
    manifest.packageKind = "system";
    manifest.engineApiRequirement = "^1.0.0";
    manifest.dependencies = std::move(dependencies);
    return manifest;
}

bool HasIssue(const ri::content::PackageResolutionResult& result, const std::string& token) {
    for (const std::string& issue : result.issues) {
        if (issue.find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

int Fail(const std::string& message) {
    std::cerr << "PackageResolverSmoke: " << message << '\n';
    return EXIT_FAILURE;
}

} // namespace

int main() {
    if (!ri::content::PackageVersionSatisfies("1.4.2", "^1.2.0")
        || ri::content::PackageVersionSatisfies("2.0.0", "^1.2.0")
        || !ri::content::PackageVersionSatisfies("1.4.2", ">=1.0.0 <2.0.0")
        || !ri::content::PackageVersionSatisfies("0.2.8", "~0.2.3")
        || ri::content::PackageVersionSatisfies("0.3.0", "~0.2.3")
        || ri::content::IsValidPackageVersionRequirement(">=broken")) {
        return Fail("semantic-version matching regressed");
    }

    std::vector<ri::content::AssetPackageManifest> catalog;
    ri::content::AssetPackageManifest library1 = Package("rawiron.flight", "1.5.0");
    library1.providesCapabilities = {"physics.flight"};
    catalog.push_back(library1);
    catalog.push_back(Package("rawiron.flight", "2.0.0"));
    catalog.push_back(Package(
        "creator.adapter",
        "1.0.0",
        {{"rawiron.flight", "<2.0.0", false}}));
    catalog.push_back(Package(
        "creator.adapter",
        "2.0.0",
        {{"rawiron.flight", ">=2.0.0", false}}));
    ri::content::AssetPackageManifest dragon = Package(
        "creator.cyber-dragon",
        "1.0.0",
        {
            {"creator.adapter", "*", false},
            {"rawiron.flight", "<2.0.0", false},
            {"creator.voice", "^1.0.0", true},
        });
    dragon.packageKind = "avatar";
    dragon.supportedPlatforms = {"windows-x64"};
    dragon.requiredCapabilities = {"physics.flight"};
    dragon.permissions = {"world.spawn"};
    catalog.push_back(dragon);

    const std::vector<ri::content::PackageRequest> roots{{"creator.cyber-dragon", "^1.0.0"}};
    const ri::content::PackageResolutionResult resolved = ri::content::ResolvePackages(
        catalog,
        roots,
        {
            .engineApiVersion = "1.4.0",
            .platform = "windows-x64",
            .grantedPermissions = {"world.spawn"},
        });
    if (!resolved.resolved || resolved.loadOrder.size() != 3U
        || resolved.loadOrder[0].packageId != "rawiron.flight"
        || resolved.loadOrder[0].packageVersion != "1.5.0"
        || resolved.loadOrder[1].packageId != "creator.adapter"
        || resolved.loadOrder[1].packageVersion != "1.0.0"
        || resolved.loadOrder[2].packageId != "creator.cyber-dragon") {
        return Fail("dependency backtracking or load ordering regressed");
    }

    const ri::content::PackageResolutionResult deniedPermission = ri::content::ResolvePackages(
        catalog,
        roots,
        {.engineApiVersion = "1.4.0", .platform = "windows-x64"});
    if (deniedPermission.resolved || !HasIssue(deniedPermission, "world.spawn")) {
        return Fail("permission denial did not identify world.spawn");
    }
    const ri::content::PackageResolutionResult wrongPlatform = ri::content::ResolvePackages(
        catalog,
        roots,
        {
            .engineApiVersion = "1.4.0",
            .platform = "linux-x64",
            .grantedPermissions = {"world.spawn"},
        });
    if (wrongPlatform.resolved || !HasIssue(wrongPlatform, "linux-x64")) {
        return Fail("platform denial did not identify linux-x64");
    }

    std::vector<ri::content::AssetPackageManifest> cyclic{
        Package("cycle.a", "1.0.0", {{"cycle.b", "*", false}}),
        Package("cycle.b", "1.0.0", {{"cycle.a", "*", false}}),
    };
    const std::vector<ri::content::PackageRequest> cycleRoots{{"cycle.a", "*"}};
    const auto cycleResult = ri::content::ResolvePackages(cyclic, cycleRoots);
    if (cycleResult.resolved || !HasIssue(cycleResult, "cycle")) {
        return Fail("dependency cycle was not rejected");
    }

    std::vector<ri::content::AssetPackageManifest> conflicting{
        Package("conflict.root", "1.0.0", {{"conflict.dep", "*", false}}),
        Package("conflict.dep", "1.0.0"),
    };
    conflicting[0].conflicts = {"conflict.dep"};
    const std::vector<ri::content::PackageRequest> conflictRoots{{"conflict.root", "*"}};
    if (ri::content::ResolvePackages(conflicting, conflictRoots).resolved) {
        return Fail("conflicting packages were not rejected");
    }

    ri::content::AssetPackageManifest serialized = dragon;
    serialized.author = "Raw Iron Creator";
    serialized.description = "System-aware avatar package";
    serialized.generatedAtUtc = "2026-07-29T00:00:00Z";
    serialized.runtime = {
        .executionMode = "wasm",
        .entryPoint = "runtime/avatar.wasm",
        .abiVersion = 1U,
    };
    const std::string json = ri::content::SerializeAssetPackageManifest(serialized);
    const std::optional<ri::content::AssetPackageManifest> parsed =
        ri::content::ParseAssetPackageManifest(json);
    if (!parsed.has_value() || parsed->formatVersion != 2
        || parsed->packageKind != "avatar"
        || parsed->engineApiRequirement != "^1.0.0"
        || parsed->runtime.executionMode != "wasm"
        || parsed->runtime.entryPoint != "runtime/avatar.wasm"
        || parsed->runtime.abiVersion != 1U
        || parsed->requiredCapabilities != dragon.requiredCapabilities
        || parsed->permissions != dragon.permissions) {
        return Fail("manifest v2 serialization round trip regressed");
    }

    const fs::path packageRoot =
        fs::temp_directory_path() / "RawIronPackageResolverSmoke";
    std::error_code error;
    fs::remove_all(packageRoot, error);
    fs::create_directories(packageRoot / "runtime", error);
    if (error) {
        return Fail("could not create temporary package fixture");
    }
    std::ofstream(packageRoot / "runtime" / "avatar.wasm", std::ios::binary) << "wasm-fixture";
    const fs::path lfFile = packageRoot / "lf.ri_asset.json";
    const fs::path crlfFile = packageRoot / "crlf.ri_asset.json";
    std::ofstream(lfFile, std::ios::binary) << "{\n  \"value\": 1\n}\n";
    std::ofstream(crlfFile, std::ios::binary) << "{\r\n  \"value\": 1\r\n}\r\n";
    if (ri::content::ComputeFileSignature(lfFile)
        != ri::content::ComputeFileSignature(crlfFile)) {
        fs::remove_all(packageRoot, error);
        return Fail("canonical package signatures differ across LF and CRLF");
    }
    fs::remove(lfFile, error);
    fs::remove(crlfFile, error);
    if (error) {
        fs::remove_all(packageRoot, error);
        return Fail("could not remove canonical-signature fixtures");
    }
    const ri::content::AssetPackageValidationReport valid =
        ri::content::ValidateAssetPackageManifest(serialized, packageRoot);
    if (!valid.valid) {
        fs::remove_all(packageRoot, error);
        return Fail("valid executable package was rejected");
    }
    serialized.runtime.entryPoint = "../escape.wasm";
    const ri::content::AssetPackageValidationReport escaped =
        ri::content::ValidateAssetPackageManifest(serialized, packageRoot);
    fs::remove_all(packageRoot, error);
    if (escaped.valid) {
        return Fail("unsafe runtime entry point was accepted");
    }

    const auto legacy = ri::content::ParseAssetPackageManifest(
        R"({"formatVersion":1,"packageId":"legacy.content","displayName":"Legacy","packageKind":"asset-pack","packageVersion":"1.0.0"})");
    return legacy.has_value() && legacy->formatVersion == 1
        && legacy->engineApiRequirement == "*"
        && legacy->runtime.executionMode == "data"
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
