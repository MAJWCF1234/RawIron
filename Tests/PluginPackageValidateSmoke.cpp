#include "RawIron/Content/PluginPackage.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc < 2) {
        return EXIT_FAILURE;
    }
    const fs::path workspace(argv[1]);
    const fs::path templatePackage = workspace / "Plugins" / "Templates" / "PublicPluginPackage";

    const auto byDir = ri::content::ValidatePluginPackage(templatePackage);
    if (!byDir.valid || byDir.descriptor.id != "example_public_plugin") {
        return EXIT_FAILURE;
    }

    const auto byJson = ri::content::ValidatePluginPackage(templatePackage / "package.riplugin.json");
    if (!byJson.valid || byJson.packageRoot != byDir.packageRoot) {
        return EXIT_FAILURE;
    }

    const auto missing = ri::content::ValidatePluginPackage(workspace / "Plugins" / "Templates");
    if (missing.valid || missing.issues.empty()) {
        return EXIT_FAILURE;
    }

    // A directory (or reparse) named package.riplugin.json must not resolve as a package root.
    const fs::path decoyRoot = fs::temp_directory_path() / "rawiron_plugin_package_validate_decoy";
    std::error_code error;
    fs::remove_all(decoyRoot, error);
    fs::create_directories(decoyRoot / "package.riplugin.json", error);
    if (error) {
        return EXIT_FAILURE;
    }
    std::ofstream(decoyRoot / "package.riplugin.json" / "not-the-descriptor.txt") << "decoy";
    const auto decoy = ri::content::ValidatePluginPackage(decoyRoot);
    const auto decoyByName = ri::content::ValidatePluginPackage(decoyRoot / "package.riplugin.json");
    fs::remove_all(decoyRoot, error);
    if (decoy.valid || decoy.issues.empty() || decoyByName.valid) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
