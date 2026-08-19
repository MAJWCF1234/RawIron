#include "RawIron/Content/SessionExtensionPackage.h"
#include "RawIron/Content/AssetDocument.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

int main() {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "RawIronSessionExtensionPackageSmoke";
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root, error);
    if (error) {
        return EXIT_FAILURE;
    }

    const fs::path assetPath = root / "assets" / "midi.ri_asset.json";
    fs::create_directories(assetPath.parent_path(), error);
    ri::content::AssetDocument asset{};
    asset.id = "studio.midi-bridge.mapping";
    asset.type = "data";
    asset.displayName = "MIDI Mapping";
    asset.sourcePath = "generated/midi.json";
    if (error || !ri::content::SaveAssetDocument(assetPath, asset)) {
        return EXIT_FAILURE;
    }
    ri::content::AssetPackageManifest manifest = ri::content::BuildAssetPackageManifest(
        root, "studio.midi-bridge", "MIDI Bridge", "Packages/studio.midi-bridge", "2026-08-19T00:00:00Z");
    manifest.packageKind = "plugin";
    manifest.packageVersion = "1.2.3";
    manifest.providesCapabilities = {"input.midi", "session.events"};
    const fs::path manifestPath = root / "package.ri_package.json";
    const ri::content::AssetPackageValidationReport validation =
        ri::content::ValidateAssetPackageManifest(manifest, root);
    if (!validation.valid || !ri::content::SaveAssetPackageManifest(manifestPath, manifest)) {
        return EXIT_FAILURE;
    }
    ri::content::InstalledAssetPackage installed{};
    installed.manifestPath = manifestPath;
    installed.packageRoot = root;
    installed.manifest = manifest;
    installed.validation = validation;
    ri::core::SessionExtensionDescriptor descriptor{};
    const bool descriptorBuilt = ri::content::BuildSessionExtensionDescriptor(installed, descriptor);
    const ri::core::SessionExtensionContract contract = ri::content::BuildSessionExtensionContract({&installed, 1U});
    std::ofstream(assetPath, std::ios::app) << "\nchanged-after-validation";
    ri::core::SessionExtensionDescriptor tampered{};
    const bool tamperedAccepted = ri::content::BuildSessionExtensionDescriptor(installed, tampered);
    fs::remove_all(root, error);
    if (!installed.validation.valid || !descriptorBuilt || descriptor.id != "studio.midi-bridge"
        || descriptor.kind != ri::core::SessionExtensionKind::Plugin
        || descriptor.reloadPolicy != ri::core::SessionExtensionReloadPolicy::FrameBoundary
        || contract.fingerprint.empty() || contract.extensions.size() != 1U
        || contract.extensions.front().fingerprint != descriptor.fingerprint) {
        return EXIT_FAILURE;
    }
    return tamperedAccepted ? EXIT_FAILURE : EXIT_SUCCESS;
}
