#pragma once

#include "RawIron/Content/AssetPackageManifest.h"
#include "RawIron/Core/SessionExtensions.h"

#include <span>

namespace ri::content {

/// Derives a session-visible extension descriptor from a validated installed package.
/// The package manifest file is fingerprinted again so a contract describes the exact package revision.
[[nodiscard]] bool BuildSessionExtensionDescriptor(const InstalledAssetPackage& package,
                                                    ri::core::SessionExtensionDescriptor& outDescriptor);

/// Builds a deterministic session contract for the packages a host intends to make session-visible.
/// Callers deliberately choose the input set: data-only local/editor packages need not enter a game session.
[[nodiscard]] ri::core::SessionExtensionContract BuildSessionExtensionContract(
    std::span<const InstalledAssetPackage> packages);

} // namespace ri::content
