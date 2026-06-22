#include "Apps/RawIron.Editor/src/EditorPluginManager.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void WriteTextFile(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::trunc);
    stream << text;
}

std::string ReadTextFile(const fs::path& path) {
    std::ifstream stream(path);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

bool ContainsLine(const std::string& text, const std::string& line) {
    return text.find(line) != std::string::npos;
}

} // namespace

int main() {
    const fs::path root = fs::temp_directory_path() / "rawiron_editor_plugin_manager_smoke";
    const fs::path blockedRoot = fs::temp_directory_path() / "rawiron_editor_plugin_manager_blocked";
    const fs::path freshRoot = fs::temp_directory_path() / "rawiron_editor_plugin_manager_fresh";
    std::error_code ec{};
    fs::remove_all(root, ec);
    fs::remove_all(blockedRoot, ec);
    fs::remove_all(freshRoot, ec);

    WriteTextFile(root / "plugins" / "manifest.plugins",
                  "# plugin_id,version,category,entry\n"
                  "rawiron.quest,1.0,gameplay,plugins/hooks.riplugin\n"
                  "rawiron.quest-beacons,1.0,gameplay,plugins/hooks.riplugin\n");
    WriteTextFile(root / "plugins" / "load_order.cfg",
                  "rawiron.quest=10\n"
                  "rawiron.quest-beacons=20\n");
    WriteTextFile(root / "plugins" / "registry.json",
                  "{\n"
                  "  \"plugins\": [\n"
                  "    {\"id\":\"rawiron.quest\",\"enabled\":true},\n"
                  "    {\"id\":\"rawiron.quest-beacons\",\"enabled\":true}\n"
                  "  ]\n"
                  "}\n");
    WriteTextFile(root / "plugins" / "hooks.riplugin",
                  "# hook,plugin,event,priority\n"
                  "startup,rawiron.quest,bootstrap,10\n"
                  "runtime,rawiron.quest-beacons,quest_marker_refresh,20\n");
    WriteTextFile(root / "config" / "plugins.policy",
                  "allow_unsigned_plugins=0\n"
                  "allow_mod_plugins=1\n"
                  "allow_project_plugins=1\n");
    WriteTextFile(root / "scripts" / "plugins.riscript", "plugins_enabled=1\n");

    const ri::editor::PluginStorePackage package{
        .id = "rawiron.telemetry-lite",
        .name = "Telemetry Lite",
        .version = "1.0.0",
        .author = "RawIron",
        .category = "telemetry",
        .description = "Adds runtime frame sampling.",
        .tagLine = "Lightweight telemetry",
        .badge = "Telemetry",
        .tags = {"telemetry"},
        .packageRoot = {},
        .loadOrder = 30,
        .hookGroup = "runtime.telemetry",
        .hookLines = {
            "startup,rawiron.telemetry-lite,bootstrap,8",
            "runtime,rawiron.telemetry-lite,frame_sample,35",
        },
        .manifestLine = "rawiron.telemetry-lite,1.0.0,telemetry,plugins/hooks.riplugin",
    };

    const ri::editor::PluginInstallResult install = ri::editor::InstallPluginStorePackage(root, package);
    if (!install.success) {
        return EXIT_FAILURE;
    }

    const std::string manifestAfterInstall = ReadTextFile(root / "plugins" / "manifest.plugins");
    const std::string hooksAfterInstall = ReadTextFile(root / "plugins" / "hooks.riplugin");
    if (!ContainsLine(manifestAfterInstall, "rawiron.telemetry-lite,1.0.0,telemetry,plugins/hooks.riplugin")) {
        return EXIT_FAILURE;
    }
    if (!ContainsLine(hooksAfterInstall, "runtime,rawiron.telemetry-lite,frame_sample,35")) {
        return EXIT_FAILURE;
    }

    const ri::editor::PluginInstallResult freshInstall = ri::editor::InstallPluginStorePackage(freshRoot, package);
    if (!freshInstall.success) {
        return EXIT_FAILURE;
    }
    const std::string freshManifest = ReadTextFile(freshRoot / "plugins" / "manifest.plugins");
    const std::string freshScript = ReadTextFile(freshRoot / "scripts" / "plugins.riscript");
    if (!ContainsLine(freshManifest, "rawiron.telemetry-lite,1.0.0,telemetry,plugins/hooks.riplugin")) {
        return EXIT_FAILURE;
    }
    if (!ContainsLine(freshScript, "plugin.rawiron.telemetry-lite.installed=1")) {
        return EXIT_FAILURE;
    }

    const ri::editor::PluginInstallResult uninstall =
        ri::editor::UninstallPluginStorePackage(root, "rawiron.quest");
    if (!uninstall.success) {
        return EXIT_FAILURE;
    }

    const std::string manifestAfterUninstall = ReadTextFile(root / "plugins" / "manifest.plugins");
    const std::string loadOrderAfterUninstall = ReadTextFile(root / "plugins" / "load_order.cfg");
    const std::string hooksAfterUninstall = ReadTextFile(root / "plugins" / "hooks.riplugin");
    const std::string registryAfterUninstall = ReadTextFile(root / "plugins" / "registry.json");

    if (ContainsLine(manifestAfterUninstall, "rawiron.quest,1.0,gameplay,plugins/hooks.riplugin")) {
        return EXIT_FAILURE;
    }
    if (!ContainsLine(manifestAfterUninstall, "rawiron.quest-beacons,1.0,gameplay,plugins/hooks.riplugin")) {
        return EXIT_FAILURE;
    }
    if (ContainsLine(loadOrderAfterUninstall, "rawiron.quest=10")) {
        return EXIT_FAILURE;
    }
    if (!ContainsLine(loadOrderAfterUninstall, "rawiron.quest-beacons=20")) {
        return EXIT_FAILURE;
    }
    if (ContainsLine(hooksAfterUninstall, "startup,rawiron.quest,bootstrap,10")) {
        return EXIT_FAILURE;
    }
    if (!ContainsLine(hooksAfterUninstall, "runtime,rawiron.quest-beacons,quest_marker_refresh,20")) {
        return EXIT_FAILURE;
    }
    if (registryAfterUninstall.find("\"id\":\"rawiron.quest\"") != std::string::npos) {
        return EXIT_FAILURE;
    }
    if (registryAfterUninstall.find("\"id\":\"rawiron.quest-beacons\"") == std::string::npos) {
        return EXIT_FAILURE;
    }

    WriteTextFile(blockedRoot / "plugins" / "manifest.plugins", "# plugin_id,version,category,entry\n");
    WriteTextFile(blockedRoot / "plugins" / "load_order.cfg", "# plugin load order\n");
    WriteTextFile(blockedRoot / "plugins" / "registry.json", "{\n  \"plugins\": []\n}\n");
    WriteTextFile(blockedRoot / "plugins" / "hooks.riplugin", "# hook,plugin,event,priority\n");
    WriteTextFile(blockedRoot / "config" / "plugins.policy",
                  "allow_unsigned_plugins=0\n"
                  "allow_mod_plugins=1\n"
                  "allow_project_plugins=0\n");
    WriteTextFile(blockedRoot / "scripts" / "plugins.riscript", "plugins_enabled=1\n");

    const ri::editor::PluginInstallResult blockedInstall = ri::editor::InstallPluginStorePackage(blockedRoot, package);
    if (blockedInstall.success) {
        return EXIT_FAILURE;
    }
    if (blockedInstall.message.find("Install blocked by policy: project plugins disabled.") == std::string::npos) {
        return EXIT_FAILURE;
    }
    const std::string blockedManifest = ReadTextFile(blockedRoot / "plugins" / "manifest.plugins");
    if (ContainsLine(blockedManifest, "rawiron.telemetry-lite,1.0.0,telemetry,plugins/hooks.riplugin")) {
        return EXIT_FAILURE;
    }

    const fs::path workspaceRoot = fs::temp_directory_path() / "rawiron_editor_plugin_store_workspace";
    fs::remove_all(workspaceRoot, ec);
    WriteTextFile(workspaceRoot / "Plugins" / "Store" / "rawiron.pipe-smoke" / "package.riplugin.json",
                  "{\n"
                  "  \"id\": \"rawiron.pipe-smoke\",\n"
                  "  \"name\": \"Pipe Smoke\",\n"
                  "  \"version\": \"2.0.0\",\n"
                  "  \"author\": \"RawIron\",\n"
                  "  \"category\": \"tooling\",\n"
                  "  \"description\": \"Editor bridge package.\",\n"
                  "  \"tagLine\": \"Bridge to external tools\",\n"
                  "  \"loadOrder\": 5,\n"
                  "  \"hookGroup\": \"runtime.tooling\",\n"
                  "  \"manifestLine\": \"rawiron.pipe-smoke,2.0.0,tooling,plugins/hooks.riplugin\",\n"
                  "  \"hooks\": [\"startup,rawiron.pipe-smoke,bootstrap,5\"],\n"
                  "  \"extension\": {\n"
                  "    \"id\": \"rawiron.pipe-smoke\",\n"
                  "    \"displayName\": \"Pipe Smoke\",\n"
                  "    \"version\": \"2.0.0\",\n"
                  "    \"kind\": \"pipe\",\n"
                  "    \"scope\": \"editor\",\n"
                  "    \"host\": \"external\",\n"
                  "    \"entry\": \"Tools/PipeSmoke/pipe-smoke.exe\",\n"
                  "    \"capabilities\": [\"editor.import_export\", \"external.rpc\"],\n"
                  "    \"tags\": [\"tooling\", \"bridge\"]\n"
                  "  }\n"
                  "}\n");
    const std::vector<ri::editor::PluginStorePackage> listed = ri::editor::ListPluginStorePackages(workspaceRoot);
    if (listed.size() != 1U) {
        return EXIT_FAILURE;
    }
    if (listed.front().extension.kind != ri::content::ExtensionKind::Pipe
        || listed.front().extension.scope != ri::content::ExtensionScope::Editor
        || listed.front().extension.host != ri::content::ExtensionHost::External
        || listed.front().extension.entry != "Tools/PipeSmoke/pipe-smoke.exe"
        || listed.front().extension.capabilities.size() != 2U) {
        return EXIT_FAILURE;
    }

    fs::remove_all(root, ec);
    fs::remove_all(blockedRoot, ec);
    fs::remove_all(freshRoot, ec);
    fs::remove_all(workspaceRoot, ec);
    return EXIT_SUCCESS;
}
