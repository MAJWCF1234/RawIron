#pragma once

#include "RawIron/Content/PackageMountRegistry.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ri::content {

/// One authored package root from `assets/dependencies.json`.
struct GamePackageRequirement {
    std::string packageId{};
    std::string versionRequirement = "*";
    bool optional = false;
};

/// Game-owned package policy. Existing dependency graphs that do not declare `packages` remain valid.
struct GamePackageRequirements {
    std::vector<GamePackageRequirement> packages{};
    std::vector<std::string> engineCapabilities{};
    std::vector<std::string> grantedPermissions{};
    std::string engineApiVersion = "1.0.0";
    std::string platform{};
    std::vector<std::string> issues{};

    [[nodiscard]] bool valid() const noexcept { return issues.empty(); }
};

struct GamePackageMountReport {
    GamePackageRequirements requirements{};
    bool requiredPackagesMounted = true;
    std::vector<PackageActivationResult> activations{};
    std::vector<std::string> issues{};
};

/// Reads package roots from `<gameRoot>/assets/dependencies.json`.
/// `packages` may contain package-id strings or objects with `id`, `version`, and `optional` fields.
[[nodiscard]] GamePackageRequirements LoadGamePackageRequirements(
    const std::filesystem::path& gameRoot);

/// Finds game-local packages first, then shared workspace packages. Duplicate id/version pairs use the
/// first root so a project can intentionally shadow a shared package during development.
[[nodiscard]] std::vector<InstalledAssetPackage> DiscoverGamePackageCatalog(
    const std::filesystem::path& gameRoot);

/// Atomically mounts required package roots, then attempts optional roots independently. Optional failures
/// are reported but do not invalidate a game boot. No executable package code is run here.
[[nodiscard]] GamePackageMountReport MountDeclaredGamePackages(
    PackageMountRegistry& registry,
    const std::filesystem::path& gameRoot);

void ReleaseDeclaredGamePackages(
    PackageMountRegistry& registry,
    const GamePackageMountReport& report);

} // namespace ri::content
