#include "RawIron/Content/AssetDocument.h"
#include "RawIron/Content/GamePackageRequirements.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

namespace fs = std::filesystem;

int Fail(const std::string& message) {
    std::cerr << "GamePackageRequirementsSmoke: " << message << '\n';
    return EXIT_FAILURE;
}

ri::content::InstalledAssetPackage CreateLocalPackage(
    const fs::path& gameRoot,
    const std::string& packageId) {
    const fs::path packageRoot = gameRoot / "Packages" / packageId;
    const fs::path documentPath = packageRoot / "assets" / "payload.ri_asset.json";
    fs::create_directories(documentPath.parent_path());
    ri::content::AssetDocument document{};
    document.id = packageId + ".payload";
    document.type = "data";
    document.displayName = packageId;
    document.sourcePath = "generated/payload";
    if (!ri::content::SaveAssetDocument(documentPath, document)) {
        return {};
    }
    ri::content::InstalledAssetPackage package{};
    package.packageRoot = packageRoot;
    package.manifestPath = packageRoot / "package.ri_package.json";
    package.manifest = ri::content::BuildAssetPackageManifest(
        packageRoot,
        packageId,
        packageId,
        "generated",
        "2026-07-29T00:00:00Z");
    package.manifest.packageKind = "system";
    package.manifest.packageVersion = "1.0.0";
    package.manifest.permissions = {"world.spawn"};
    package.validation = ri::content::ValidateAssetPackageManifest(package.manifest, packageRoot);
    if (package.validation.valid) {
        (void)ri::content::SaveAssetPackageManifest(package.manifestPath, package.manifest);
    }
    return package;
}

} // namespace

int main() {
    const fs::path root = fs::temp_directory_path() / "RawIronGamePackageRequirementsSmoke";
    std::error_code error;
    fs::remove_all(root, error);
    const fs::path gameRoot = root / "Game";
    fs::create_directories(gameRoot / "assets", error);
    if (error) {
        return Fail("could not create game fixture");
    }
    const ri::content::InstalledAssetPackage local =
        CreateLocalPackage(gameRoot, "game.required");
    if (!local.validation.valid) {
        fs::remove_all(root, error);
        return Fail("could not create valid local package");
    }

    std::ofstream(gameRoot / "assets" / "dependencies.json")
        << R"({
  "engineApiVersion": "1.0.0",
  "capabilities": ["game.runtime"],
  "permissions": ["world.spawn"],
  "packages": [
    {"id": "game.required", "version": "^1.0.0"},
    {"id": "game.optional", "version": "^1.0.0", "optional": true}
  ]
})";

    ri::content::PackageMountRegistry registry;
    const ri::content::GamePackageMountReport mounted =
        ri::content::MountDeclaredGamePackages(registry, gameRoot);
    if (!mounted.requirements.valid() || !mounted.requiredPackagesMounted
        || mounted.activations.size() != 1U
        || !registry.IsMounted("game.required")
        || mounted.issues.empty()) {
        fs::remove_all(root, error);
        return Fail("required/optional game package mount behavior regressed");
    }
    ri::content::ReleaseDeclaredGamePackages(registry, mounted);
    if (!registry.MountedPackages().empty()) {
        fs::remove_all(root, error);
        return Fail("game package release retained a mount");
    }

    std::ofstream(gameRoot / "assets" / "dependencies.json", std::ios::trunc)
        << R"({"packages":["game.required", "game.required"]})";
    const ri::content::GamePackageRequirements malformed =
        ri::content::LoadGamePackageRequirements(gameRoot);
    const ri::content::GamePackageMountReport rejected =
        ri::content::MountDeclaredGamePackages(registry, gameRoot);
    fs::remove_all(root, error);
    if (malformed.valid() || rejected.requiredPackagesMounted
        || !registry.MountedPackages().empty()) {
        return Fail("invalid game package declarations entered the registry");
    }
    return EXIT_SUCCESS;
}
