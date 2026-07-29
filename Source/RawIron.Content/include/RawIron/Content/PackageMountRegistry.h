#pragma once

#include "RawIron/Content/AssetPackageManifest.h"
#include "RawIron/Content/PackageResolver.h"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::content {

struct MountedPackageInfo {
    std::string packageId{};
    std::string packageVersion{};
    std::string packageKind{};
    std::string mountPoint{};
    std::filesystem::path packageRoot{};
    std::size_t referenceCount = 0;
};

struct PackageActivationResult {
    bool activated = false;
    std::uint64_t activationId = 0;
    std::vector<std::string> newlyMounted{};
    std::vector<std::string> retained{};
    std::vector<std::string> issues{};
};

/// Owns the live package mount table. Activations retain their entire resolved dependency graph;
/// deactivation releases it in reverse order so shared dependencies remain mounted until unused.
class PackageMountRegistry {
public:
    [[nodiscard]] PackageActivationResult Activate(
        std::span<const InstalledAssetPackage> catalog,
        std::span<const PackageRequest> roots,
        const PackageResolverOptions& options = {});

    /// Releases one activation. Returns false for an unknown/already released activation id.
    [[nodiscard]] bool Deactivate(std::uint64_t activationId);
    void DeactivateAll();

    [[nodiscard]] bool IsMounted(std::string_view packageId) const;
    [[nodiscard]] std::size_t ActiveActivationCount() const;
    [[nodiscard]] std::vector<MountedPackageInfo> MountedPackages() const;

    /// Resolves only assets declared by the active package manifest.
    [[nodiscard]] std::optional<std::filesystem::path> ResolveAsset(
        std::string_view packageId,
        std::string_view assetId) const;

    /// Resolves `<mountPoint>/<manifest asset path>` without exposing undeclared package files.
    [[nodiscard]] std::optional<std::filesystem::path> ResolveVirtualPath(
        std::string_view virtualPath) const;

    /// Returns an executable package's validated entry point, or nullopt for data packages.
    [[nodiscard]] std::optional<std::filesystem::path> ResolveRuntimeEntry(
        std::string_view packageId) const;

private:
    struct MountedRecord {
        InstalledAssetPackage package{};
        std::string mountPoint{};
        std::unordered_map<std::string, std::filesystem::path> assetsById{};
        std::optional<std::filesystem::path> runtimeEntry{};
        std::size_t referenceCount = 0;
        std::uint64_t mountOrdinal = 0;
    };

    struct VirtualAssetRecord {
        std::string packageId{};
        std::filesystem::path physicalPath{};
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, MountedRecord> mounted_;
    std::unordered_map<std::string, VirtualAssetRecord> virtualAssets_;
    std::unordered_map<std::uint64_t, std::vector<std::string>> activations_;
    std::uint64_t nextActivationId_ = 1;
    std::uint64_t nextMountOrdinal_ = 1;
};

} // namespace ri::content
