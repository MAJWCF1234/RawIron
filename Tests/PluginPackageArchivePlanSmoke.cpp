#include "RawIron/Content/PluginPackage.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc < 2) {
        return EXIT_FAILURE;
    }
    const fs::path workspace(argv[1]);
    const fs::path templatePackage = workspace / "Plugins" / "Templates" / "PublicPluginPackage";

    const ri::content::PluginPackageArchivePlan plan =
        ri::content::PlanPluginPackageArchive(templatePackage);
    if (!plan.valid || plan.descriptor.id != "example_public_plugin") {
        return EXIT_FAILURE;
    }

    bool sawDescriptor = false;
    bool sawManifest = false;
    bool sawRegistry = false;
    bool sawHooks = false;
    bool sawLoadOrder = false;
    bool sawReadme = false;
    bool sawScript = false;
    for (const std::string& entry : plan.relativeEntries) {
        sawDescriptor = sawDescriptor || entry == "package.riplugin.json";
        sawManifest = sawManifest || entry == "plugins/manifest.plugins";
        sawRegistry = sawRegistry || entry == "plugins/registry.json";
        sawHooks = sawHooks || entry == "plugins/hooks.riplugin";
        sawLoadOrder = sawLoadOrder || entry == "plugins/load_order.cfg";
        sawReadme = sawReadme || entry == "README.md";
        sawScript = sawScript || entry == "scripts/plugins.riscript";
    }
    if (!sawDescriptor || !sawManifest || !sawRegistry || !sawHooks || !sawLoadOrder
        || !sawReadme || !sawScript) {
        return EXIT_FAILURE;
    }

    const fs::path staging = fs::temp_directory_path() / "rawiron_plugin_package_archive_plan_smoke";
    std::error_code error;
    fs::remove_all(staging, error);
    std::vector<std::string> issues;
    if (!ri::content::StagePluginPackageArchive(plan, staging, issues)) {
        return EXIT_FAILURE;
    }
    if (!fs::is_regular_file(staging / "package.riplugin.json", error)
        || !ri::content::ValidatePluginPackage(staging).valid) {
        fs::remove_all(staging, error);
        return EXIT_FAILURE;
    }

    const auto invalid = ri::content::PlanPluginPackageArchive(workspace / "Plugins" / "Templates");
    if (invalid.valid) {
        fs::remove_all(staging, error);
        return EXIT_FAILURE;
    }

    fs::remove_all(staging, error);
    return EXIT_SUCCESS;
}
