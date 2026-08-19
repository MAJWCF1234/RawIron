#include "RawIron/Content/SessionExtensionPackage.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace ri::content {
namespace {

[[nodiscard]] ri::core::SessionExtensionKind ClassifyKind(const AssetPackageManifest& manifest) noexcept {
    if (manifest.packageKind == "plugin" || manifest.packageKind == "plugin-pack") {
        return ri::core::SessionExtensionKind::Plugin;
    }
    if (manifest.packageKind == "system" || manifest.packageKind == "mixed" || manifest.packageKind == "mixed-pack"
        || manifest.runtime.executionMode != "data") {
        return ri::core::SessionExtensionKind::Gameplay;
    }
    if (manifest.packageKind == "script" || manifest.packageKind == "script-pack") {
        return ri::core::SessionExtensionKind::Mod;
    }
    return ri::core::SessionExtensionKind::Data;
}

[[nodiscard]] ri::core::SessionExtensionReloadPolicy ClassifyReloadPolicy(
    const AssetPackageManifest& manifest) noexcept {
    if (manifest.runtime.executionMode == "native") {
        return ri::core::SessionExtensionReloadPolicy::SessionRestart;
    }
    if (manifest.runtime.executionMode != "data" || manifest.packageKind == "system"
        || manifest.packageKind == "mixed" || manifest.packageKind == "mixed-pack") {
        return ri::core::SessionExtensionReloadPolicy::SimulationBoundary;
    }
    return ri::core::SessionExtensionReloadPolicy::FrameBoundary;
}

[[nodiscard]] std::string ComputePackageSessionFingerprint(const InstalledAssetPackage& package) {
    constexpr std::uint64_t offset = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = offset;
    const auto add = [&hash](const std::string_view value) {
        for (const unsigned char byte : value) {
            hash ^= byte;
            hash *= prime;
        }
        hash ^= 0xFFU;
        hash *= prime;
    };
    add(ComputeFileSignature(package.manifestPath));
    std::vector<AssetPackageEntry> assets = package.manifest.assets;
    std::sort(assets.begin(), assets.end(), [](const auto& left, const auto& right) { return left.path < right.path; });
    for (const AssetPackageEntry& asset : assets) {
        add(asset.path);
        add(ComputeFileSignature(package.packageRoot / asset.path));
    }
    std::ostringstream stream;
    stream << "fnv1a64:" << std::hex << std::nouppercase << std::setw(16) << std::setfill('0') << hash;
    return stream.str();
}

} // namespace

bool BuildSessionExtensionDescriptor(const InstalledAssetPackage& package,
                                     ri::core::SessionExtensionDescriptor& outDescriptor) {
    if (!package.validation.valid || package.manifest.packageId.empty() || package.manifest.packageVersion.empty()
        || package.manifestPath.empty()) {
        return false;
    }
    if (!ValidateAssetPackageManifest(package.manifest, package.packageRoot).valid) {
        return false;
    }
    const std::string packageFingerprint = ComputePackageSessionFingerprint(package);
    outDescriptor = ri::core::SessionExtensionDescriptor{
        .id = package.manifest.packageId,
        .version = package.manifest.packageVersion,
        .fingerprint = packageFingerprint,
        .kind = ClassifyKind(package.manifest),
        .reloadPolicy = ClassifyReloadPolicy(package.manifest),
        .capabilities = package.manifest.providesCapabilities,
    };
    std::sort(outDescriptor.capabilities.begin(), outDescriptor.capabilities.end());
    return true;
}

ri::core::SessionExtensionContract BuildSessionExtensionContract(
    const std::span<const InstalledAssetPackage> packages) {
    ri::core::SessionExtensionContract contract{};
    contract.extensions.reserve(packages.size());
    for (const InstalledAssetPackage& package : packages) {
        ri::core::SessionExtensionDescriptor descriptor{};
        if (BuildSessionExtensionDescriptor(package, descriptor)) {
            contract.extensions.push_back(std::move(descriptor));
        }
    }
    static_cast<void>(ri::core::NormalizeSessionExtensionContract(contract));
    return contract;
}

} // namespace ri::content
