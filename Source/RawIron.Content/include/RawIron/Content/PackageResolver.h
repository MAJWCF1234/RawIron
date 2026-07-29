#pragma once

#include "RawIron/Content/AssetPackageManifest.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ri::content {

struct PackageRequest {
    std::string packageId{};
    std::string versionRequirement = "*";
};

struct PackageResolverOptions {
    std::string engineApiVersion = "1.0.0";
    /// Empty leaves platform filtering to a later host stage. CLI callers supply the host platform.
    std::string platform{};
    std::vector<std::string> engineCapabilities{};
    std::vector<std::string> grantedPermissions{};
    bool enforcePermissions = true;
    bool includeOptionalDependencies = false;
};

struct ResolvedPackage {
    std::size_t catalogIndex = 0;
    std::string packageId{};
    std::string packageVersion{};
};

struct PackageResolutionResult {
    bool resolved = false;
    /// Dependency-first deterministic activation order.
    std::vector<ResolvedPackage> loadOrder{};
    std::vector<std::string> issues{};
};

/// Supports exact semantic versions, comparison sets (`>=1.2.0 <2.0.0`), caret, tilde, and `*`.
[[nodiscard]] bool IsValidPackageVersionRequirement(std::string_view requirement);
[[nodiscard]] bool PackageVersionSatisfies(
    std::string_view version,
    std::string_view requirement);

/// Selects one version per package, backtracks across competing constraints, rejects cycles/conflicts,
/// and returns a dependency-first activation order. The catalog remains owned by the caller.
[[nodiscard]] PackageResolutionResult ResolvePackages(
    std::span<const AssetPackageManifest> catalog,
    std::span<const PackageRequest> roots,
    const PackageResolverOptions& options = {});

} // namespace ri::content
