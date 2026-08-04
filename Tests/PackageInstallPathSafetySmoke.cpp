#include "RawIron/Content/AssetPackageManifest.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace fs = std::filesystem;

int Fail(const std::string& message) {
    std::cerr << "PackageInstallPathSafetySmoke: " << message << '\n';
    return EXIT_FAILURE;
}

bool HasIssue(
    const ri::content::AssetPackageValidationReport& report,
    const std::string_view token) {
    for (const std::string& issue : report.issues) {
        if (issue.find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

ri::content::AssetPackageManifest ManifestWithInstallPath(const std::string& installPath) {
    ri::content::AssetPackageManifest manifest{};
    manifest.packageId = "rawiron.path-safety";
    manifest.displayName = "Path Safety";
    manifest.packageKind = "content";
    manifest.packageVersion = "1.0.0";
    manifest.installScope = "project";
    manifest.generatedAtUtc = "2026-08-03T00:00:00Z";
    manifest.assets.push_back({
        .id = "fixture",
        .type = "mesh",
        .path = "assets/fixture.ri_asset.json",
        .installPath = installPath,
        .sourcePath = "source/fixture.glb",
        .sizeBytes = 1U,
        .signature = "fnv1a64:0000000000000000",
    });
    return manifest;
}

} // namespace

int main() {
    const fs::path tempBase = fs::temp_directory_path() / "RawIronPackageInstallPathSafetySmoke";
    const fs::path projectRoot = tempBase / "project";
    const fs::path prefixSibling = tempBase / "project-escape";
    std::error_code error;
    fs::remove_all(tempBase, error);
    error.clear();
    fs::create_directories(projectRoot / "assets", error);
    fs::create_directories(prefixSibling, error);
    if (error) {
        return Fail("could not create temporary project fixture: " + error.message());
    }

    std::string embeddedNul = "assets/";
    embeddedNul.push_back('\0');
    embeddedNul += "escape.ri_asset.json";
    std::string embeddedControl = "assets/";
    embeddedControl.push_back('\x1f');
    embeddedControl += "escape.ri_asset.json";

    std::vector<std::string> unsafePaths{
        "../escape.ri_asset.json",
        "assets/../../escape.ri_asset.json",
        "assets/./escape.ri_asset.json",
        "/absolute/escape.ri_asset.json",
        "//server/share/escape.ri_asset.json",
        "C:/Windows/escape.ri_asset.json",
        "C:drive-relative.ri_asset.json",
        R"(\rooted\escape.ri_asset.json)",
        R"(\\server\share\escape.ri_asset.json)",
        R"(\\?\C:\escape.ri_asset.json)",
        R"(\\.\GLOBALROOT\escape.ri_asset.json)",
        R"(assets/sub\..\escape.ri_asset.json)",
        "assets//escape.ri_asset.json",
        "assets/escape.ri_asset.json/",
        "assets/CON",
        "assets/CON .txt",
        "assets/nul.txt",
        "assets/COM1.bin",
        "assets/lPt9.fixture",
        "assets/COM\xc2\xb9.bin",
        "assets/LPT\xc2\xb2.fixture",
        "assets/name.",
        "assets/name ",
        "assets/file.ri_asset.json:stream",
        "assets/file?.ri_asset.json",
        embeddedNul,
        embeddedControl,
    };
    unsafePaths.push_back("assets/" + std::string(256U, 'a'));
    unsafePaths.push_back(std::string(4097U, 'a'));

    if (ri::content::ResolvePackageInstallPath(projectRoot, {}).safe) {
        fs::remove_all(tempBase, error);
        return Fail("empty direct install path was accepted");
    }

    for (const std::string& unsafePath : unsafePaths) {
        const ri::content::PackageInstallPathResolution resolution =
            ri::content::ResolvePackageInstallPath(projectRoot, unsafePath);
        if (resolution.safe || !resolution.destination.empty() || resolution.issue.empty()) {
            fs::remove_all(tempBase, error);
            return Fail("unsafe path was accepted or lacked a diagnostic");
        }

        const ri::content::AssetPackageValidationReport manifestReport =
            ri::content::ValidateAssetPackageManifest(
                ManifestWithInstallPath(unsafePath), tempBase / "package");
        if (!HasIssue(manifestReport, "installPath")) {
            fs::remove_all(tempBase, error);
            return Fail("manifest validation did not identify an unsafe installPath");
        }
    }

    const std::vector<std::string> validPaths{
        "assets/packages/rawiron.path-safety/fixture.ri_asset.json",
        "levels/packages/rawiron.path-safety/scene.ri_asset.json",
        "scripts/packages/rawiron.path-safety/main.ri_asset.json",
        "assets/package with spaces/.metadata/fixture-name_01.ri_asset.json",
    };
    for (const std::string& validPath : validPaths) {
        const ri::content::PackageInstallPathResolution resolution =
            ri::content::ResolvePackageInstallPath(projectRoot, validPath);
        if (!resolution.safe || resolution.destination.empty() || !resolution.issue.empty()) {
            fs::remove_all(tempBase, error);
            return Fail("valid nested project path was rejected: " + validPath + " (" + resolution.issue + ")");
        }
        const fs::path expectedDestination =
            fs::weakly_canonical(projectRoot / fs::path(validPath));
        if (resolution.destination != expectedDestination) {
            fs::remove_all(tempBase, error);
            return Fail("valid nested project path resolved to an unexpected destination: " + validPath);
        }
        const ri::content::AssetPackageValidationReport manifestReport =
            ri::content::ValidateAssetPackageManifest(
                ManifestWithInstallPath(validPath), tempBase / "package");
        if (HasIssue(manifestReport, "installPath")) {
            fs::remove_all(tempBase, error);
            return Fail("manifest validation rejected a valid installPath: " + validPath);
        }
    }

    const fs::path existingFile = projectRoot / "assets" / "existing.ri_asset.json";
    std::ofstream(existingFile, std::ios::binary) << "fixture";
    if (!ri::content::ResolvePackageInstallPath(
            projectRoot, "assets/existing.ri_asset.json").safe) {
        fs::remove_all(tempBase, error);
        return Fail("existing regular-file destination was rejected");
    }

    const fs::path outsideSentinel = tempBase / "outside-sentinel.txt";
    const fs::path hardLinkedDestination = projectRoot / "assets" / "hard-linked.ri_asset.json";
    std::ofstream(outsideSentinel, std::ios::binary) << "outside-must-not-change";
    error.clear();
    fs::create_hard_link(outsideSentinel, hardLinkedDestination, error);
    if (error) {
        fs::remove_all(tempBase, error);
        return Fail("could not create hard-link trust-boundary fixture");
    }
    const ri::content::PackageInstallPathResolution hardLinkedResolution =
        ri::content::ResolvePackageInstallPath(
            projectRoot, "assets/hard-linked.ri_asset.json");
    if (hardLinkedResolution.safe
        || hardLinkedResolution.issue.find("hard-link") == std::string::npos) {
        fs::remove_all(tempBase, error);
        return Fail("multi-link destination was not rejected with a hard-link diagnostic");
    }
    std::string outsideContents;
    std::ifstream outsideInput(outsideSentinel, std::ios::binary);
    std::getline(outsideInput, outsideContents);
    if (outsideContents != "outside-must-not-change") {
        fs::remove_all(tempBase, error);
        return Fail("hard-link validation modified the outside sentinel");
    }

    fs::create_directories(projectRoot / "assets" / "directory-destination", error);
    if (ri::content::ResolvePackageInstallPath(
            projectRoot, "assets/directory-destination").safe) {
        fs::remove_all(tempBase, error);
        return Fail("existing directory destination was accepted as a package file");
    }

    ri::content::AssetPackageManifest duplicateManifest =
        ManifestWithInstallPath("assets/CaseCollision.ri_asset.json");
    duplicateManifest.assets.push_back({
        .id = "fixture-two",
        .type = "mesh",
        .path = "assets/fixture-two.ri_asset.json",
        .installPath = "assets/casecollision.ri_asset.json",
        .sourcePath = "source/fixture-two.glb",
        .sizeBytes = 1U,
        .signature = "fnv1a64:0000000000000000",
    });
    if (!HasIssue(
            ri::content::ValidateAssetPackageManifest(duplicateManifest, tempBase / "package"),
            "case-insensitive")) {
        fs::remove_all(tempBase, error);
        return Fail("portable case-colliding install destinations were not rejected");
    }

#if defined(_WIN32)
    std::string upperUnicodePath = "assets/";
    upperUnicodePath += "\xc3\x84-collision.ri_asset.json";
    std::string lowerUnicodePath = "assets/";
    lowerUnicodePath += "\xc3\xa4-collision.ri_asset.json";
    ri::content::AssetPackageManifest unicodeDuplicateManifest =
        ManifestWithInstallPath(upperUnicodePath);
    unicodeDuplicateManifest.assets.push_back({
        .id = "fixture-unicode-two",
        .type = "mesh",
        .path = "assets/fixture-unicode-two.ri_asset.json",
        .installPath = lowerUnicodePath,
        .sourcePath = "source/fixture-unicode-two.glb",
        .sizeBytes = 1U,
        .signature = "fnv1a64:0000000000000000",
    });
    if (!HasIssue(
            ri::content::ValidateAssetPackageManifest(
                unicodeDuplicateManifest, tempBase / "package"),
            "case-insensitive")) {
        fs::remove_all(tempBase, error);
        return Fail("Windows Unicode case-colliding install destinations were not rejected");
    }
#endif

    const fs::path escapeLink = projectRoot / "assets" / "external-link";
    fs::create_directory_symlink(prefixSibling, escapeLink, error);
    if (!error) {
        const ri::content::PackageInstallPathResolution escapedLink =
            ri::content::ResolvePackageInstallPath(
                projectRoot, "assets/external-link/escape.ri_asset.json");
        if (escapedLink.safe || escapedLink.issue.find("symlink/reparse") == std::string::npos) {
            fs::remove_all(tempBase, error);
            return Fail("escaping symlink/reparse destination passed containment validation");
        }
    }

    error.clear();
    fs::remove_all(tempBase, error);
    return EXIT_SUCCESS;
}
