#include "RawIron/Content/AssetPackageManifest.h"

#include "RawIron/Content/AssetDocument.h"
#include "RawIron/Content/PackageResolver.h"
#include "RawIron/Core/Detail/JsonScan.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <system_error>

namespace ri::content {
namespace {

namespace detail_scan = ri::core::detail;
namespace fs = std::filesystem;

[[nodiscard]] bool IsSemanticVersionTriplet(std::string_view value) {
    int componentCount = 0;
    std::size_t cursor = 0;
    while (cursor < value.size()) {
        const std::size_t nextDot = value.find('.', cursor);
        const std::size_t end = nextDot == std::string_view::npos ? value.size() : nextDot;
        if (end == cursor) {
            return false;
        }
        for (std::size_t index = cursor; index < end; ++index) {
            if (value[index] < '0' || value[index] > '9') {
                return false;
            }
        }
        ++componentCount;
        if (nextDot == std::string_view::npos) {
            break;
        }
        cursor = nextDot + 1;
    }
    return componentCount == 3;
}

[[nodiscard]] bool IsAllowedPackageKind(std::string_view value) {
    return value == "content" || value == "world" || value == "avatar" || value == "system"
        || value == "script" || value == "plugin" || value == "mixed"
        || value == "asset-pack" || value == "resource-pack"
        || value == "script-pack" || value == "mixed-pack";
}

[[nodiscard]] bool IsAllowedInstallScope(std::string_view value) {
    return value == "mounted" || value == "project" || value == "either";
}

[[nodiscard]] bool IsAllowedExecutionMode(std::string_view value) {
    return value == "data" || value == "native" || value == "wasm" || value == "lua"
        || value == "process" || value == "script";
}

[[nodiscard]] bool IsSafeToken(std::string_view value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](const char ch) {
        const unsigned char code = static_cast<unsigned char>(ch);
        return std::isalnum(code) != 0 || ch == '.' || ch == '_' || ch == '-';
    });
}

[[nodiscard]] std::optional<fs::path> TryRelativeToRoot(const fs::path& absolutePath, const fs::path& root) {
    std::error_code ec{};
    const fs::path relative = fs::relative(absolutePath, root, ec);
    if (!ec && !relative.empty()) {
        return relative.lexically_normal();
    }
    return std::nullopt;
}

[[nodiscard]] std::string FormatFnv64(std::uint64_t value) {
    std::ostringstream stream;
    stream << "fnv1a64:" << std::hex << std::nouppercase << std::setw(16) << std::setfill('0') << value;
    return stream.str();
}

struct PackageFileMetrics {
    std::uint64_t logicalSize = 0U;
    std::uint64_t hash = 14695981039346656037ULL;
    bool readable = false;
};

[[nodiscard]] bool UsesCanonicalTextBytes(const fs::path& path) {
    return path.filename().string().ends_with(".ri_asset.json");
}

[[nodiscard]] PackageFileMetrics MeasurePackageFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    PackageFileMetrics metrics{};
    if (!input.is_open()) {
        return metrics;
    }
    metrics.readable = true;
    const bool normalizeNewlines = UsesCanonicalTextBytes(path);
    bool pendingCarriageReturn = false;
    const auto consume = [&](const unsigned char byte) {
        metrics.hash ^= byte;
        metrics.hash *= 1099511628211ULL;
        ++metrics.logicalSize;
    };
    char buffer[64 * 1024]{};
    while (input.good()) {
        input.read(buffer, sizeof(buffer));
        const std::streamsize count = input.gcount();
        for (std::streamsize index = 0; index < count; ++index) {
            const unsigned char byte = static_cast<unsigned char>(buffer[index]);
            if (!normalizeNewlines) {
                consume(byte);
                continue;
            }
            if (pendingCarriageReturn) {
                if (byte == '\n') {
                    consume('\n');
                    pendingCarriageReturn = false;
                    continue;
                }
                consume('\r');
                pendingCarriageReturn = false;
            }
            if (byte == '\r') {
                pendingCarriageReturn = true;
            } else {
                consume(byte);
            }
        }
    }
    if (pendingCarriageReturn) {
        consume('\r');
    }
    return metrics;
}

[[nodiscard]] bool IsSafePackageRelativePath(const std::string& rawPath) {
    if (rawPath.empty()) {
        return false;
    }
    const fs::path path(rawPath);
    if (path.is_absolute()) {
        return false;
    }
    for (const fs::path& part : path) {
        const std::string token = part.string();
        if (token == "." || token == ".." || token.empty()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool IsPathWithin(
    const fs::path& canonicalRoot,
    const fs::path& canonicalCandidate) {
    auto rootIt = canonicalRoot.begin();
    auto candidateIt = canonicalCandidate.begin();
    for (; rootIt != canonicalRoot.end(); ++rootIt, ++candidateIt) {
        if (candidateIt == canonicalCandidate.end() || *rootIt != *candidateIt) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string AssetLabel(const AssetPackageEntry& asset) {
    return asset.id.empty() ? std::string("<empty-id>") : asset.id;
}

void WriteStringArray(std::ostringstream& json, const char* key, const std::vector<std::string>& values, std::string indent) {
    json << indent << "\"" << key << "\": [";
    for (std::size_t index = 0; index < values.size(); ++index) {
        json << "\"" << detail_scan::EscapeJsonString(values[index]) << "\"";
        if (index + 1U < values.size()) {
            json << ", ";
        }
    }
    json << "]";
}

} // namespace

std::string ComputeFileSignature(const fs::path& path) {
    const PackageFileMetrics metrics = MeasurePackageFile(path);
    if (!metrics.readable) {
        return {};
    }
    return FormatFnv64(metrics.hash);
}

std::string SerializeAssetPackageManifest(const AssetPackageManifest& manifest) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"formatVersion\": " << manifest.formatVersion << ",\n";
    json << "  \"packageId\": \"" << detail_scan::EscapeJsonString(manifest.packageId) << "\",\n";
    json << "  \"displayName\": \"" << detail_scan::EscapeJsonString(manifest.displayName) << "\",\n";
    json << "  \"packageKind\": \"" << detail_scan::EscapeJsonString(manifest.packageKind) << "\",\n";
    json << "  \"packageVersion\": \"" << detail_scan::EscapeJsonString(manifest.packageVersion) << "\",\n";
    json << "  \"author\": \"" << detail_scan::EscapeJsonString(manifest.author) << "\",\n";
    json << "  \"description\": \"" << detail_scan::EscapeJsonString(manifest.description) << "\",\n";
    json << "  \"installScope\": \"" << detail_scan::EscapeJsonString(manifest.installScope) << "\",\n";
    json << "  \"mountPoint\": \"" << detail_scan::EscapeJsonString(manifest.mountPoint) << "\",\n";
    json << "  \"sourceRoot\": \"" << detail_scan::EscapeJsonString(manifest.sourceRoot) << "\",\n";
    json << "  \"generatedAtUtc\": \"" << detail_scan::EscapeJsonString(manifest.generatedAtUtc) << "\",\n";
    json << "  \"engineApiRequirement\": \"" << detail_scan::EscapeJsonString(manifest.engineApiRequirement) << "\",\n";
    WriteStringArray(json, "supportedPlatforms", manifest.supportedPlatforms, "  ");
    json << ",\n";
    WriteStringArray(json, "tags", manifest.tags, "  ");
    json << ",\n";
    WriteStringArray(json, "providesCapabilities", manifest.providesCapabilities, "  ");
    json << ",\n";
    WriteStringArray(json, "requiredCapabilities", manifest.requiredCapabilities, "  ");
    json << ",\n";
    WriteStringArray(json, "permissions", manifest.permissions, "  ");
    json << ",\n";
    json << "  \"dependencies\": [\n";
    for (std::size_t index = 0; index < manifest.dependencies.size(); ++index) {
        const AssetPackageDependency& dependency = manifest.dependencies[index];
        json << "    {\n";
        json << "      \"packageId\": \"" << detail_scan::EscapeJsonString(dependency.packageId) << "\",\n";
        json << "      \"versionRequirement\": \"" << detail_scan::EscapeJsonString(dependency.versionRequirement) << "\",\n";
        json << "      \"optional\": " << (dependency.optional ? "true" : "false") << "\n";
        json << "    }";
        if (index + 1U < manifest.dependencies.size()) {
            json << ",";
        }
        json << "\n";
    }
    json << "  ],\n";
    WriteStringArray(json, "conflicts", manifest.conflicts, "  ");
    json << ",\n";
    json << "  \"runtime\": {\n";
    json << "    \"executionMode\": \"" << detail_scan::EscapeJsonString(manifest.runtime.executionMode) << "\",\n";
    json << "    \"entryPoint\": \"" << detail_scan::EscapeJsonString(manifest.runtime.entryPoint) << "\",\n";
    json << "    \"abiVersion\": " << manifest.runtime.abiVersion << "\n";
    json << "  },\n";
    json << "  \"assets\": [\n";
    for (std::size_t index = 0; index < manifest.assets.size(); ++index) {
        const AssetPackageEntry& asset = manifest.assets[index];
        json << "    {\n";
        json << "      \"id\": \"" << detail_scan::EscapeJsonString(asset.id) << "\",\n";
        json << "      \"type\": \"" << detail_scan::EscapeJsonString(asset.type) << "\",\n";
        json << "      \"path\": \"" << detail_scan::EscapeJsonString(asset.path) << "\",\n";
        json << "      \"installPath\": \"" << detail_scan::EscapeJsonString(asset.installPath) << "\",\n";
        json << "      \"sourcePath\": \"" << detail_scan::EscapeJsonString(asset.sourcePath) << "\",\n";
        json << "      \"sizeBytes\": " << asset.sizeBytes << ",\n";
        json << "      \"signature\": \"" << detail_scan::EscapeJsonString(asset.signature) << "\"\n";
        json << "    }";
        if (index + 1U < manifest.assets.size()) {
            json << ",";
        }
        json << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

std::optional<AssetPackageManifest> ParseAssetPackageManifest(const std::string_view jsonText) {
    AssetPackageManifest manifest{};
    manifest.formatVersion = detail_scan::ExtractJsonInt(jsonText, "formatVersion").value_or(AssetPackageManifest::kFormatVersion);
    manifest.packageId = detail_scan::ExtractJsonString(jsonText, "packageId").value_or("");
    manifest.displayName = detail_scan::ExtractJsonString(jsonText, "displayName").value_or("");
    manifest.packageKind = detail_scan::ExtractJsonString(jsonText, "packageKind").value_or("asset-pack");
    manifest.packageVersion = detail_scan::ExtractJsonString(jsonText, "packageVersion").value_or("0.1.0");
    manifest.author = detail_scan::ExtractJsonString(jsonText, "author").value_or("");
    manifest.description = detail_scan::ExtractJsonString(jsonText, "description").value_or("");
    manifest.installScope = detail_scan::ExtractJsonString(jsonText, "installScope").value_or("project");
    manifest.mountPoint = detail_scan::ExtractJsonString(jsonText, "mountPoint").value_or("");
    manifest.sourceRoot = detail_scan::ExtractJsonString(jsonText, "sourceRoot").value_or("");
    manifest.generatedAtUtc = detail_scan::ExtractJsonString(jsonText, "generatedAtUtc").value_or("");
    manifest.engineApiRequirement = detail_scan::ExtractJsonString(jsonText, "engineApiRequirement").value_or("*");
    manifest.supportedPlatforms = detail_scan::ExtractJsonStringArray(jsonText, "supportedPlatforms");
    manifest.tags = detail_scan::ExtractJsonStringArray(jsonText, "tags");
    manifest.providesCapabilities = detail_scan::ExtractJsonStringArray(jsonText, "providesCapabilities");
    manifest.requiredCapabilities = detail_scan::ExtractJsonStringArray(jsonText, "requiredCapabilities");
    manifest.permissions = detail_scan::ExtractJsonStringArray(jsonText, "permissions");
    manifest.conflicts = detail_scan::ExtractJsonStringArray(jsonText, "conflicts");
    manifest.runtime.executionMode = detail_scan::ExtractJsonString(jsonText, "executionMode").value_or("data");
    manifest.runtime.entryPoint = detail_scan::ExtractJsonString(jsonText, "entryPoint").value_or("");
    manifest.runtime.abiVersion = static_cast<std::uint32_t>(
        std::max(0, detail_scan::ExtractJsonInt(jsonText, "abiVersion").value_or(0)));

    const std::vector<std::string_view> dependencyObjects = detail_scan::SplitJsonArrayObjects(jsonText, "dependencies");
    manifest.dependencies.reserve(dependencyObjects.size());
    for (const std::string_view dependencyText : dependencyObjects) {
        AssetPackageDependency dependency{};
        dependency.packageId = detail_scan::ExtractJsonString(dependencyText, "packageId").value_or("");
        dependency.versionRequirement = detail_scan::ExtractJsonString(dependencyText, "versionRequirement").value_or("");
        dependency.optional = detail_scan::ExtractJsonBool(dependencyText, "optional").value_or(false);
        manifest.dependencies.push_back(std::move(dependency));
    }

    const std::vector<std::string_view> assetObjects = detail_scan::SplitJsonArrayObjects(jsonText, "assets");
    manifest.assets.reserve(assetObjects.size());
    for (const std::string_view assetText : assetObjects) {
        AssetPackageEntry entry{};
        entry.id = detail_scan::ExtractJsonString(assetText, "id").value_or("");
        entry.type = detail_scan::ExtractJsonString(assetText, "type").value_or("");
        entry.path = detail_scan::ExtractJsonString(assetText, "path").value_or("");
        entry.installPath = detail_scan::ExtractJsonString(assetText, "installPath").value_or("");
        entry.sourcePath = detail_scan::ExtractJsonString(assetText, "sourcePath").value_or("");
        entry.sizeBytes = detail_scan::ExtractJsonUInt64(assetText, "sizeBytes").value_or(0ULL);
        entry.signature = detail_scan::ExtractJsonString(assetText, "signature").value_or("");
        manifest.assets.push_back(std::move(entry));
    }

    if (manifest.packageId.empty()) {
        return std::nullopt;
    }
    return manifest;
}

std::optional<AssetPackageManifest> LoadAssetPackageManifest(const fs::path& path) {
    const std::string text = detail_scan::ReadTextFile(path);
    if (text.empty()) {
        return std::nullopt;
    }
    return ParseAssetPackageManifest(text);
}

bool SaveAssetPackageManifest(const fs::path& path, const AssetPackageManifest& manifest) {
    return detail_scan::WriteTextFile(path, SerializeAssetPackageManifest(manifest));
}

AssetPackageManifest BuildAssetPackageManifest(const fs::path& packageRoot,
                                               std::string packageId,
                                               std::string displayName,
                                               std::string sourceRoot,
                                               std::string generatedAtUtc) {
    AssetPackageManifest manifest{};
    manifest.packageId = std::move(packageId);
    manifest.displayName = std::move(displayName);
    manifest.packageKind = "content";
    manifest.packageVersion = "0.1.0";
    manifest.installScope = "either";
    manifest.mountPoint = "Packages/" + manifest.packageId;
    manifest.sourceRoot = std::move(sourceRoot);
    manifest.generatedAtUtc = std::move(generatedAtUtc);

    std::vector<fs::path> assetDocuments;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(packageRoot)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const fs::path path = entry.path();
        if (path.filename().string().ends_with(".ri_asset.json")) {
            assetDocuments.push_back(path);
        }
    }
    std::sort(assetDocuments.begin(), assetDocuments.end());

    for (const fs::path& documentPath : assetDocuments) {
        const std::optional<AssetDocument> document = LoadAssetDocument(documentPath);
        if (!document.has_value()) {
            continue;
        }
        const std::optional<fs::path> relative = TryRelativeToRoot(documentPath, packageRoot);
        AssetPackageEntry entry{};
        entry.id = document->id;
        entry.type = document->type;
        entry.path = relative.has_value() ? relative->generic_string() : documentPath.generic_string();
        entry.installPath = {};
        entry.sourcePath = document->sourcePath;
        const PackageFileMetrics metrics = MeasurePackageFile(documentPath);
        entry.sizeBytes = metrics.logicalSize;
        entry.signature = metrics.readable ? FormatFnv64(metrics.hash) : std::string{};
        manifest.assets.push_back(std::move(entry));
    }

    return manifest;
}

AssetPackageValidationReport ValidateAssetPackageManifest(const AssetPackageManifest& manifest,
                                                          const fs::path& packageRoot) {
    AssetPackageValidationReport report{};
    if (manifest.formatVersion != AssetPackageManifest::kLegacyFormatVersion
        && manifest.formatVersion != AssetPackageManifest::kFormatVersion) {
        report.issues.push_back("package formatVersion is unsupported.");
    }
    if (!IsSafeToken(manifest.packageId)) {
        report.issues.push_back("package packageId must use letters, digits, '.', '_', or '-'.");
    }
    if (manifest.displayName.empty()) {
        report.issues.push_back("package displayName must be non-empty.");
    }
    if (!IsAllowedPackageKind(manifest.packageKind)) {
        report.issues.push_back(
            "package packageKind must be content, world, avatar, system, script, plugin, mixed, or a legacy v1 kind.");
    }
    if (manifest.packageVersion.empty()) {
        report.issues.push_back("package packageVersion must be non-empty.");
    } else if (!IsSemanticVersionTriplet(manifest.packageVersion)) {
        report.issues.push_back("package packageVersion must use semantic triplet format (e.g. \"1.0.0\").");
    }
    if (!IsAllowedInstallScope(manifest.installScope)) {
        report.issues.push_back("package installScope must be one of: mounted, project, either.");
    }
    if (!IsValidPackageVersionRequirement(manifest.engineApiRequirement)) {
        report.issues.push_back("package engineApiRequirement is not a supported semantic-version requirement.");
    }
    if (!manifest.mountPoint.empty() && !IsSafePackageRelativePath(manifest.mountPoint)) {
        report.issues.push_back("package mountPoint must be package/project-relative and cannot contain '.' or '..'.");
    }
    if (manifest.generatedAtUtc.empty()) {
        report.issues.push_back("package generatedAtUtc must be non-empty.");
    }
    if (!IsAllowedExecutionMode(manifest.runtime.executionMode)) {
        report.issues.push_back("package runtime executionMode must be data, native, wasm, lua, process, or script.");
    } else if (manifest.runtime.executionMode == "data") {
        if (!manifest.runtime.entryPoint.empty() || manifest.runtime.abiVersion != 0U) {
            report.issues.push_back("data packages cannot declare a runtime entryPoint or ABI version.");
        }
    } else {
        if (!IsSafePackageRelativePath(manifest.runtime.entryPoint)) {
            report.issues.push_back("executable package runtime entryPoint must be a safe package-relative path.");
        } else {
            const fs::path entryPointPath = packageRoot / fs::path(manifest.runtime.entryPoint);
            if (!fs::is_regular_file(entryPointPath)) {
                report.issues.push_back("executable package runtime entryPoint is missing.");
            } else {
                std::error_code rootError;
                std::error_code entryError;
                const fs::path canonicalRoot = fs::weakly_canonical(packageRoot, rootError);
                const fs::path canonicalEntry = fs::weakly_canonical(entryPointPath, entryError);
                if (rootError || entryError || !IsPathWithin(canonicalRoot, canonicalEntry)) {
                    report.issues.push_back("executable package runtime entryPoint escapes the package root.");
                }
            }
        }
        if (manifest.runtime.abiVersion == 0U) {
            report.issues.push_back("executable package runtime abiVersion must be greater than zero.");
        }
    }
    if (manifest.assets.empty() && manifest.runtime.executionMode == "data") {
        report.issues.push_back("data package must contain at least one asset.");
    }

    std::set<std::string> seenIds;
    std::set<std::string> seenPaths;
    std::set<std::string> seenInstallPaths;
    std::set<std::string> manifestPaths;
    std::set<std::string> seenDependencies;
    std::set<std::string> seenConflicts;
    auto validateTokenArray = [&](const std::vector<std::string>& values, const char* field) {
        std::set<std::string> seen;
        for (const std::string& value : values) {
            if (!IsSafeToken(value)) {
                report.issues.push_back(std::string("package ") + field + " contains an invalid token.");
            } else if (!seen.insert(value).second) {
                report.issues.push_back(std::string("package ") + field + " contains duplicate token " + value + ".");
            }
        }
    };

    validateTokenArray(manifest.tags, "tags");
    validateTokenArray(manifest.supportedPlatforms, "supportedPlatforms");
    validateTokenArray(manifest.providesCapabilities, "providesCapabilities");
    validateTokenArray(manifest.requiredCapabilities, "requiredCapabilities");
    validateTokenArray(manifest.permissions, "permissions");
    for (const AssetPackageDependency& dependency : manifest.dependencies) {
        if (!IsSafeToken(dependency.packageId)) {
            report.issues.push_back("package dependency packageId is invalid.");
        } else if (!seenDependencies.insert(dependency.packageId).second) {
            report.issues.push_back("package dependency " + dependency.packageId + " is listed more than once.");
        }
        if (!IsValidPackageVersionRequirement(dependency.versionRequirement)) {
            report.issues.push_back("package dependency " + dependency.packageId + " has an invalid version requirement.");
        }
        if (dependency.packageId == manifest.packageId) {
            report.issues.push_back("package cannot depend on itself.");
        }
    }
    for (const std::string& conflict : manifest.conflicts) {
        if (!IsSafeToken(conflict)) {
            report.issues.push_back("package conflict id is invalid.");
        } else if (!seenConflicts.insert(conflict).second) {
            report.issues.push_back("package conflict " + conflict + " is listed more than once.");
        }
        if (conflict == manifest.packageId) {
            report.issues.push_back("package cannot conflict with itself.");
        }
    }

    for (const AssetPackageEntry& asset : manifest.assets) {
        const std::string label = AssetLabel(asset);
        if (asset.id.empty()) {
            report.issues.push_back("asset entry has empty id.");
        } else if (!seenIds.insert(asset.id).second) {
            report.issues.push_back("asset " + asset.id + " duplicates another asset id.");
        }
        if (asset.type.empty()) {
            report.issues.push_back("asset " + label + " has empty type.");
        }
        if (asset.path.empty()) {
            report.issues.push_back("asset " + label + " has empty path.");
            continue;
        }
        if (!IsSafePackageRelativePath(asset.path)) {
            report.issues.push_back("asset " + label + " path must be package-relative and cannot contain '.' or '..'.");
            continue;
        }
        if (!seenPaths.insert(asset.path).second) {
            report.issues.push_back("asset " + label + " duplicates another asset path.");
        }
        if (!asset.installPath.empty()) {
            if (!IsSafePackageRelativePath(asset.installPath)) {
                report.issues.push_back("asset " + label + " installPath must be project-relative and cannot contain '.' or '..'.");
            } else if (!seenInstallPaths.insert(asset.installPath).second) {
                report.issues.push_back("asset " + label + " duplicates another asset installPath.");
            }
        }
        manifestPaths.insert(fs::path(asset.path).lexically_normal().generic_string());

        const fs::path assetPath = packageRoot / fs::path(asset.path);
        if (!fs::exists(assetPath)) {
            report.issues.push_back("asset " + label + " is missing file " + asset.path + ".");
            continue;
        }
        if (!fs::is_regular_file(assetPath)) {
            report.issues.push_back("asset " + label + " path is not a file.");
            continue;
        }
        std::error_code rootError;
        std::error_code assetError;
        const fs::path canonicalRoot = fs::weakly_canonical(packageRoot, rootError);
        const fs::path canonicalAsset = fs::weakly_canonical(assetPath, assetError);
        if (rootError || assetError || !IsPathWithin(canonicalRoot, canonicalAsset)) {
            report.issues.push_back("asset " + label + " resolves outside the package root.");
            continue;
        }
        if (!assetPath.filename().string().ends_with(".ri_asset.json")) {
            report.issues.push_back("asset " + label + " path must point to a .ri_asset.json document.");
        }
        if (asset.sourcePath.empty()) {
            report.issues.push_back("asset " + label + " sourcePath must be non-empty.");
        }

        const PackageFileMetrics metrics = MeasurePackageFile(assetPath);
        const std::uint64_t actualSize = metrics.logicalSize;
        if (asset.sizeBytes != actualSize) {
            report.issues.push_back("asset " + label + " size does not match manifest.");
        }
        const std::string actualSignature = metrics.readable ? FormatFnv64(metrics.hash) : std::string{};
        if (asset.signature.empty() || asset.signature != actualSignature) {
            report.issues.push_back("asset " + label + " signature does not match manifest.");
        }

        const std::optional<AssetDocument> document = LoadAssetDocument(assetPath);
        if (!document.has_value()) {
            report.issues.push_back("asset " + label + " document cannot be parsed.");
            continue;
        }
        if (document->id != asset.id) {
            report.issues.push_back("asset " + label + " document id does not match manifest.");
        }
        if (document->type != asset.type) {
            report.issues.push_back("asset " + label + " document type does not match manifest.");
        }
        if (document->sourcePath != asset.sourcePath) {
            report.issues.push_back("asset " + label + " document sourcePath does not match manifest.");
        }
    }

    if (fs::exists(packageRoot)) {
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(packageRoot)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const fs::path path = entry.path();
            if (!path.filename().string().ends_with(".ri_asset.json")) {
                continue;
            }
            const std::optional<fs::path> relative = TryRelativeToRoot(path, packageRoot);
            if (!relative.has_value()) {
                continue;
            }
            const std::string relativeText = relative->generic_string();
            if (!manifestPaths.contains(relativeText)) {
                report.issues.push_back("package contains unlisted asset document " + relativeText + ".");
            }
        }
    }

    report.valid = report.issues.empty();
    return report;
}

std::vector<fs::path> FindAssetPackageManifestPaths(const fs::path& projectRoot) {
    std::vector<fs::path> manifests;
    auto addManifest = [&manifests](const fs::path& manifestPath) {
        std::error_code errorCode;
        const fs::path normalized = fs::weakly_canonical(manifestPath, errorCode);
        const fs::path candidate = errorCode ? manifestPath.lexically_normal() : normalized;
        for (const fs::path& existing : manifests) {
            std::error_code equivalentError;
            if (fs::equivalent(existing, candidate, equivalentError)) {
                return;
            }
            if (equivalentError && existing == candidate) {
                return;
            }
        }
        manifests.push_back(candidate);
    };

    const fs::path rootManifest = projectRoot / "package.ri_package.json";
    if (fs::exists(rootManifest) && fs::is_regular_file(rootManifest)) {
        addManifest(rootManifest);
    }

    for (const fs::path packageRoot : {projectRoot / "Packages", projectRoot / "packages"}) {
        if (!fs::exists(packageRoot) || !fs::is_directory(packageRoot)) {
            continue;
        }
        for (const fs::directory_entry& entry : fs::directory_iterator(packageRoot)) {
            if (!entry.is_directory()) {
                continue;
            }
            const fs::path manifestPath = entry.path() / "package.ri_package.json";
            if (fs::exists(manifestPath) && fs::is_regular_file(manifestPath)) {
                addManifest(manifestPath);
            }
        }
    }

    std::sort(manifests.begin(), manifests.end());
    manifests.erase(std::unique(manifests.begin(), manifests.end()), manifests.end());
    return manifests;
}

std::vector<InstalledAssetPackage> DiscoverInstalledAssetPackages(const fs::path& projectRoot) {
    std::vector<InstalledAssetPackage> packages;
    for (const fs::path& manifestPath : FindAssetPackageManifestPaths(projectRoot)) {
        const std::optional<AssetPackageManifest> manifest = LoadAssetPackageManifest(manifestPath);
        if (!manifest.has_value()) {
            continue;
        }
        InstalledAssetPackage installed{};
        installed.manifestPath = manifestPath;
        installed.packageRoot = manifestPath.parent_path();
        installed.manifest = *manifest;
        installed.validation = ValidateAssetPackageManifest(installed.manifest, installed.packageRoot);
        packages.push_back(std::move(installed));
    }
    return packages;
}

const InstalledAssetPackage* FindInstalledAssetPackage(const std::vector<InstalledAssetPackage>& packages,
                                                       const std::string_view packageId) noexcept {
    const auto found = std::find_if(
        packages.begin(),
        packages.end(),
        [packageId](const InstalledAssetPackage& package) {
            return package.manifest.packageId == packageId;
        });
    return found == packages.end() ? nullptr : &(*found);
}

} // namespace ri::content
