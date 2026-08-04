#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::content {

struct AssetPackageEntry {
    std::string id{};
    std::string type{};
    std::string path{};
    /// Optional project-relative destination used by install-on-project mode.
    /// Empty means tooling derives a destination from type and package id.
    std::string installPath{};
    std::string sourcePath{};
    std::uint64_t sizeBytes = 0;
    std::string signature{};
};

struct AssetPackageDependency {
    std::string packageId{};
    std::string versionRequirement{};
    bool optional = false;
};

struct AssetPackageRuntime {
    /// data, native, wasm, lua, process, or script. Only data packages require no executable entry point.
    std::string executionMode = "data";
    std::string entryPoint{};
    std::uint32_t abiVersion = 0;
};

struct AssetPackageManifest {
    static constexpr int kLegacyFormatVersion = 1;
    static constexpr int kFormatVersion = 2;

    int formatVersion = kFormatVersion;
    std::string packageId{};
    std::string displayName{};
    /// content, world, avatar, system, script, plugin, or mixed.
    /// Legacy v1 package-kind names remain accepted when loading old archives.
    std::string packageKind = "content";
    std::string packageVersion = "0.1.0";
    std::string author{};
    std::string description{};
    std::string installScope = "project";
    std::string mountPoint{};
    std::string sourceRoot{};
    std::string generatedAtUtc{};
    /// Semantic-version requirement for the public Raw Iron package/runtime API.
    std::string engineApiRequirement = "*";
    /// Empty means platform-neutral. Tokens use names such as windows-x64 or linux-x64.
    std::vector<std::string> supportedPlatforms{};
    std::vector<std::string> tags{};
    /// Engine/package capabilities made available after activation.
    std::vector<std::string> providesCapabilities{};
    /// Engine capabilities required before the package may activate.
    std::vector<std::string> requiredCapabilities{};
    /// Requested host permissions. Granting is a resolver/host policy decision.
    std::vector<std::string> permissions{};
    std::vector<AssetPackageDependency> dependencies{};
    std::vector<std::string> conflicts{};
    AssetPackageRuntime runtime{};
    std::vector<AssetPackageEntry> assets{};
};

struct AssetPackageValidationReport {
    bool valid = false;
    std::vector<std::string> issues{};
};

struct PackageInstallPathResolution {
    bool safe = false;
    /// Absolute, canonical destination. Empty when safe is false.
    std::filesystem::path destination{};
    /// User-facing reason for rejection. Empty when safe is true.
    std::string issue{};
};

struct InstalledAssetPackage {
    std::filesystem::path manifestPath{};
    std::filesystem::path packageRoot{};
    AssetPackageManifest manifest{};
    AssetPackageValidationReport validation{};
};

[[nodiscard]] std::string ComputeFileSignature(const std::filesystem::path& path);

[[nodiscard]] std::string SerializeAssetPackageManifest(const AssetPackageManifest& manifest);
[[nodiscard]] std::optional<AssetPackageManifest> ParseAssetPackageManifest(std::string_view jsonText);
[[nodiscard]] std::optional<AssetPackageManifest> LoadAssetPackageManifest(const std::filesystem::path& path);
[[nodiscard]] bool SaveAssetPackageManifest(const std::filesystem::path& path,
                                            const AssetPackageManifest& manifest);

[[nodiscard]] AssetPackageManifest BuildAssetPackageManifest(const std::filesystem::path& packageRoot,
                                                             std::string packageId,
                                                             std::string displayName,
                                                             std::string sourceRoot,
                                                             std::string generatedAtUtc);

[[nodiscard]] AssetPackageValidationReport ValidateAssetPackageManifest(const AssetPackageManifest& manifest,
                                                                        const std::filesystem::path& packageRoot);

/// Resolves a manifest installPath beneath an existing project root without
/// mutating the filesystem. Package paths use portable forward-slash syntax;
/// absolute, drive-relative, device, traversal, ambiguous, and escaping
/// symlink/reparse destinations are rejected.
[[nodiscard]] PackageInstallPathResolution ResolvePackageInstallPath(
    const std::filesystem::path& projectRoot,
    std::string_view relativeInstallPath);

/// Orders resolved install destinations using the collision policy below.
struct PackageInstallDestinationLess {
    [[nodiscard]] bool operator()(
        const std::filesystem::path& left,
        const std::filesystem::path& right) const;
};

/// Returns true when two resolved install destinations would name the same
/// destination under the host filesystem's case rules. Windows uses ordinal
/// Unicode case-insensitive comparison; other hosts additionally preserve the
/// package format's portable ASCII case-insensitive collision policy.
[[nodiscard]] bool PackageInstallDestinationsCollide(
    const std::filesystem::path& left,
    const std::filesystem::path& right);

[[nodiscard]] std::vector<std::filesystem::path> FindAssetPackageManifestPaths(
    const std::filesystem::path& projectRoot);

[[nodiscard]] std::vector<InstalledAssetPackage> DiscoverInstalledAssetPackages(
    const std::filesystem::path& projectRoot);

[[nodiscard]] const InstalledAssetPackage* FindInstalledAssetPackage(
    const std::vector<InstalledAssetPackage>& packages,
    std::string_view packageId) noexcept;

} // namespace ri::content
