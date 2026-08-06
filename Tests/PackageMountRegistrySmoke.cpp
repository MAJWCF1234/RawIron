#include "RawIron/Content/AssetDocument.h"
#include "RawIron/Content/PackageMountRegistry.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

namespace fs = std::filesystem;

int Fail(const std::string& message) {
    std::cerr << "PackageMountRegistrySmoke: " << message << '\n';
    return EXIT_FAILURE;
}

ri::content::InstalledAssetPackage CreatePackage(
    const fs::path& fixtureRoot,
    const std::string& id,
    const std::string& version,
    std::vector<ri::content::AssetPackageDependency> dependencies = {},
    std::string mountPoint = {}) {
    const fs::path packageRoot = fixtureRoot / (id + "-" + version);
    const fs::path assetPath = packageRoot / "assets" / "payload.ri_asset.json";
    fs::create_directories(assetPath.parent_path());

    ri::content::AssetDocument document{};
    document.id = id + ".payload";
    document.type = "data";
    document.displayName = id + " payload";
    document.sourcePath = "generated/" + id + "/payload.json";
    document.payloadJson = R"({"ready":true})";
    if (!ri::content::SaveAssetDocument(assetPath, document)) {
        return {};
    }

    ri::content::InstalledAssetPackage installed{};
    installed.packageRoot = packageRoot;
    installed.manifestPath = packageRoot / "package.ri_package.json";
    installed.manifest = ri::content::BuildAssetPackageManifest(
        packageRoot,
        id,
        id,
        "generated/" + id,
        "2026-07-29T00:00:00Z");
    installed.manifest.packageVersion = version;
    installed.manifest.dependencies = std::move(dependencies);
    installed.manifest.mountPoint = std::move(mountPoint);
    installed.validation =
        ri::content::ValidateAssetPackageManifest(installed.manifest, installed.packageRoot);
    return installed;
}

} // namespace

int main() {
    const fs::path fixtureRoot =
        fs::temp_directory_path() / "RawIronPackageMountRegistrySmoke";
    std::error_code error;
    fs::remove_all(fixtureRoot, error);
    fs::create_directories(fixtureRoot, error);
    if (error) {
        return Fail("could not create fixture root");
    }

    ri::content::InstalledAssetPackage shared =
        CreatePackage(fixtureRoot, "rawiron.shared", "1.0.0");
    ri::content::InstalledAssetPackage avatar = CreatePackage(
        fixtureRoot,
        "creator.avatar",
        "1.0.0",
        {{"rawiron.shared", "^1.0.0", false}});
    fs::create_directories(avatar.packageRoot / "runtime", error);
    std::ofstream(avatar.packageRoot / "runtime" / "avatar.wasm", std::ios::binary)
        << "wasm-fixture";
    avatar.manifest.packageKind = "avatar";
    avatar.manifest.runtime = {
        .executionMode = "wasm",
        .entryPoint = "runtime/avatar.wasm",
        .abiVersion = 1U,
    };
    avatar.validation =
        ri::content::ValidateAssetPackageManifest(avatar.manifest, avatar.packageRoot);
    if (!shared.validation.valid || !avatar.validation.valid) {
        fs::remove_all(fixtureRoot, error);
        return Fail("fixture package validation failed");
    }

    std::vector<ri::content::InstalledAssetPackage> catalog{shared, avatar};
    const std::vector<ri::content::PackageRequest> roots{{"creator.avatar", "^1.0.0"}};
    ri::content::PackageMountRegistry registry;
    const ri::content::PackageActivationResult first =
        registry.Activate(catalog, roots);
    const std::vector<ri::content::MountedPackageInfo> firstMounts =
        registry.MountedPackages();
    if (!first.activated || first.activationId == 0U
        || first.newlyMounted != std::vector<std::string>({"rawiron.shared", "creator.avatar"})
        || firstMounts.size() != 2U
        || firstMounts[0].packageId != "rawiron.shared"
        || firstMounts[1].packageId != "creator.avatar"
        || firstMounts[0].referenceCount != 1U
        || registry.ActiveActivationCount() != 1U) {
        fs::remove_all(fixtureRoot, error);
        return Fail("initial graph activation or mount order regressed");
    }

    const auto sharedAsset = registry.ResolveAsset("rawiron.shared", "rawiron.shared.payload");
    const auto virtualAsset = registry.ResolveVirtualPath(
        "Packages/rawiron.shared/assets/payload.ri_asset.json");
    const auto runtimeEntry = registry.ResolveRuntimeEntry("creator.avatar");
    if (!sharedAsset.has_value() || !virtualAsset.has_value()
        || *sharedAsset != *virtualAsset || !runtimeEntry.has_value()
        || runtimeEntry->filename() != "avatar.wasm"
        || registry.ResolveAsset("rawiron.shared", "missing").has_value()
        || registry.ResolveVirtualPath("./Packages/rawiron.shared/assets/payload.ri_asset.json").has_value()
        || registry.ResolveVirtualPath("../payload.ri_asset.json").has_value()) {
        fs::remove_all(fixtureRoot, error);
        return Fail("declared asset or runtime entry resolution regressed");
    }
    for (int lookup = 0; lookup < 10'000; ++lookup) {
        if (!registry.ResolveAsset("rawiron.shared", "rawiron.shared.payload").has_value()
            || !registry.ResolveVirtualPath(
                "Packages/rawiron.shared/assets/payload.ri_asset.json").has_value()) {
            fs::remove_all(fixtureRoot, error);
            return Fail("indexed mounted-asset lookup became unstable");
        }
    }

    const ri::content::PackageActivationResult second =
        registry.Activate(catalog, roots);
    const std::vector<ri::content::MountedPackageInfo> retainedMounts =
        registry.MountedPackages();
    if (!second.activated || second.retained.size() != 2U
        || retainedMounts[0].referenceCount != 2U
        || retainedMounts[1].referenceCount != 2U
        || registry.ActiveActivationCount() != 2U) {
        fs::remove_all(fixtureRoot, error);
        return Fail("shared graph reference counting regressed");
    }
    if (!registry.Deactivate(first.activationId)
        || registry.Deactivate(first.activationId)
        || registry.MountedPackages()[0].referenceCount != 1U) {
        fs::remove_all(fixtureRoot, error);
        return Fail("activation release semantics regressed");
    }

    ri::content::InstalledAssetPackage alternate =
        CreatePackage(fixtureRoot, "creator.avatar", "2.0.0");
    std::vector<ri::content::InstalledAssetPackage> alternateCatalog{alternate};
    const std::vector<ri::content::PackageRequest> alternateRoot{
        {"creator.avatar", "2.0.0"},
    };
    const ri::content::PackageActivationResult versionCollision =
        registry.Activate(alternateCatalog, alternateRoot);
    if (versionCollision.activated || versionCollision.issues.empty()
        || registry.ActiveActivationCount() != 1U) {
        fs::remove_all(fixtureRoot, error);
        return Fail("active version collision was not rejected atomically");
    }

    ri::content::InstalledAssetPackage collisionA =
        CreatePackage(fixtureRoot, "collision.a", "1.0.0", {}, "Shared/Collision");
    ri::content::InstalledAssetPackage collisionB =
        CreatePackage(fixtureRoot, "collision.b", "1.0.0", {}, "Shared/Collision");
    const std::vector<ri::content::InstalledAssetPackage> collisionCatalog{
        collisionA,
        collisionB,
    };
    const std::vector<ri::content::PackageRequest> collisionRoots{
        {"collision.a", "*"},
        {"collision.b", "*"},
    };
    const ri::content::PackageActivationResult mountCollision =
        registry.Activate(collisionCatalog, collisionRoots);
    if (mountCollision.activated || mountCollision.issues.empty()
        || registry.IsMounted("collision.a") || registry.IsMounted("collision.b")) {
        fs::remove_all(fixtureRoot, error);
        return Fail("mount-point collision changed registry state");
    }

    ri::content::InstalledAssetPackage fallbackV1 =
        CreatePackage(fixtureRoot, "fallback.package", "1.0.0");
    ri::content::InstalledAssetPackage invalidV2 =
        CreatePackage(fixtureRoot, "fallback.package", "2.0.0");
    fs::remove(invalidV2.packageRoot / invalidV2.manifest.assets.front().path, error);
    const std::vector<ri::content::InstalledAssetPackage> fallbackCatalog{
        fallbackV1,
        invalidV2,
    };
    const std::vector<ri::content::PackageRequest> fallbackRoots{
        {"fallback.package", "*"},
    };
    const ri::content::PackageActivationResult fallbackActivation =
        registry.Activate(fallbackCatalog, fallbackRoots);
    const std::vector<ri::content::MountedPackageInfo> fallbackMounts =
        registry.MountedPackages();
    const auto fallbackMount = std::find_if(
        fallbackMounts.begin(),
        fallbackMounts.end(),
        [](const ri::content::MountedPackageInfo& mount) {
            return mount.packageId == "fallback.package";
        });
    if (!fallbackActivation.activated || fallbackMount == fallbackMounts.end()
        || fallbackMount->packageVersion != "1.0.0"
        || !registry.Deactivate(fallbackActivation.activationId)) {
        fs::remove_all(fixtureRoot, error);
        return Fail("resolver did not fall back from invalid newest package");
    }

    ri::content::InstalledAssetPackage invalid =
        CreatePackage(fixtureRoot, "invalid.package", "1.0.0");
    fs::remove(invalid.packageRoot / invalid.manifest.assets.front().path, error);
    const std::vector<ri::content::InstalledAssetPackage> invalidCatalog{invalid};
    const std::vector<ri::content::PackageRequest> invalidRoots{
        {"invalid.package", "*"},
    };
    const ri::content::PackageActivationResult invalidActivation =
        registry.Activate(invalidCatalog, invalidRoots);
    if (invalidActivation.activated || invalidActivation.issues.empty()
        || registry.IsMounted("invalid.package")) {
        fs::remove_all(fixtureRoot, error);
        return Fail("invalid package entered the live mount table");
    }

#if defined(_WIN32)
    {
        // Post-mount swap: replace a declared asset with a junction that escapes the package.
        // Resolve must fail closed rather than return the escaped target.
        const fs::path payloadPath = shared.packageRoot / "assets" / "payload.ri_asset.json";
        const fs::path escapeRoot = fixtureRoot / "escape-target";
        const fs::path escapeFile = escapeRoot / "secret.txt";
        fs::create_directories(escapeRoot, error);
        std::ofstream(escapeFile) << "secret";
        fs::remove(payloadPath, error);
        const std::wstring link = payloadPath.wstring();
        const std::wstring target = escapeFile.wstring();
        if (!CreateSymbolicLinkW(link.c_str(), target.c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)
            && !CreateSymbolicLinkW(link.c_str(), target.c_str(), 0)) {
            // Symlink creation may be denied on locked-down hosts; skip this hostile case.
            fs::remove_all(escapeRoot, error);
        } else if (registry.ResolveAsset("rawiron.shared", "rawiron.shared.payload").has_value()
                   || registry.ResolveVirtualPath(
                       "Packages/rawiron.shared/assets/payload.ri_asset.json").has_value()) {
            fs::remove_all(fixtureRoot, error);
            return Fail("post-mount symlink escape was not rejected on resolve");
        } else {
            fs::remove(payloadPath, error);
            fs::remove_all(escapeRoot, error);
            // Restore a real payload so teardown deactivation stays clean.
            ri::content::AssetDocument restored{};
            restored.id = "rawiron.shared.payload";
            restored.type = "data";
            restored.displayName = "rawiron.shared payload";
            restored.sourcePath = "generated/rawiron.shared/payload.json";
            restored.payloadJson = R"({"ready":true})";
            (void)ri::content::SaveAssetDocument(payloadPath, restored);
        }
    }
#endif

    {
        // Hard-link alias of an outside inode must not resolve through the mount table.
        const fs::path payloadPath = shared.packageRoot / "assets" / "payload.ri_asset.json";
        const fs::path outsideRoot = fixtureRoot / "hardlink-outside";
        const fs::path outsideFile = outsideRoot / "aliased.ri_asset.json";
        fs::create_directories(outsideRoot, error);
        fs::copy_file(payloadPath, outsideFile, fs::copy_options::overwrite_existing, error);
        fs::remove(payloadPath, error);
        fs::create_hard_link(outsideFile, payloadPath, error);
        if (error) {
            fs::remove_all(outsideRoot, error);
            // Restore payload if hard links are unavailable on this volume.
            ri::content::AssetDocument restored{};
            restored.id = "rawiron.shared.payload";
            restored.type = "data";
            restored.displayName = "rawiron.shared payload";
            restored.sourcePath = "generated/rawiron.shared/payload.json";
            restored.payloadJson = R"({"ready":true})";
            (void)ri::content::SaveAssetDocument(payloadPath, restored);
        } else if (registry.ResolveAsset("rawiron.shared", "rawiron.shared.payload").has_value()
                   || registry.ResolveVirtualPath(
                       "Packages/rawiron.shared/assets/payload.ri_asset.json").has_value()) {
            fs::remove_all(fixtureRoot, error);
            return Fail("post-mount hard-link alias was not rejected on resolve");
        } else {
            fs::remove(payloadPath, error);
            fs::remove_all(outsideRoot, error);
            ri::content::AssetDocument restored{};
            restored.id = "rawiron.shared.payload";
            restored.type = "data";
            restored.displayName = "rawiron.shared payload";
            restored.sourcePath = "generated/rawiron.shared/payload.json";
            restored.payloadJson = R"({"ready":true})";
            (void)ri::content::SaveAssetDocument(payloadPath, restored);
        }
    }

    if (!registry.Deactivate(second.activationId)
        || !registry.MountedPackages().empty()
        || registry.ActiveActivationCount() != 0U
        || registry.ResolveAsset("rawiron.shared", "rawiron.shared.payload").has_value()
        || registry.ResolveVirtualPath(
            "Packages/rawiron.shared/assets/payload.ri_asset.json").has_value()) {
        fs::remove_all(fixtureRoot, error);
        return Fail("final dependency graph did not unload");
    }
    registry.DeactivateAll();
    fs::remove_all(fixtureRoot, error);
    return EXIT_SUCCESS;
}
