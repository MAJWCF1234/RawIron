#include "RawIron/Content/PluginProjectData.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

void WriteTextFile(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::trunc);
    stream << text;
}

bool Expect(bool condition) {
    return condition;
}

} // namespace

int main() {
    const fs::path root = fs::temp_directory_path() / "rawiron_plugin_project_data_smoke";
    std::error_code ec{};
    fs::remove_all(root, ec);

    WriteTextFile(root / "plugins" / "manifest.plugins",
                  "legacy.telemetry,1.0,telemetry,plugins/hooks.riplugin\n"
                  "template.runtime,1.0,utility,plugins/hooks.riplugin\n"
                  "external.tool,1.0,utility,https://example.invalid/external.plugin\n");
    WriteTextFile(root / "plugins" / "load_order.cfg",
                  "legacy.telemetry\n"
                  "template.runtime=10\n"
                  "external.tool=20\n");
    WriteTextFile(root / "plugins" / "registry.json",
                  "{ \"plugins\": [\"legacy.telemetry\", \"template.runtime\", \"external.tool\"] }\n");
    WriteTextFile(root / "plugins" / "hooks.riplugin",
                  "on_startup=legacy.telemetry\n"
                  "on_runtime=template.runtime\n"
                  "runtime,external.tool,frame_sample,20\n");
    WriteTextFile(root / "config" / "plugins.policy",
                  "allow_unsigned_plugins=0\n"
                  "allow_mod_plugins=1\n"
                  "allow_project_plugins=1\n");
    WriteTextFile(root / "scripts" / "plugins.riscript", "plugin_hook_batch_size=4\n");

    const ri::content::PluginProjectData data = ri::content::LoadPluginProjectData(root);
    if (!Expect(data.manifestEntries.size() == 3U)) {
        return EXIT_FAILURE;
    }
    if (!Expect(data.registryEntries.size() == 3U)) {
        return EXIT_FAILURE;
    }
    if (!Expect(data.hookBindings.size() == 3U)) {
        return EXIT_FAILURE;
    }
    if (!Expect(data.activePlugins.size() == 2U)) {
        return EXIT_FAILURE;
    }
    if (!Expect(data.activePlugins.front().manifest.id == "legacy.telemetry")) {
        return EXIT_FAILURE;
    }

    bool sawUnsignedPolicyIssue = false;
    bool sawLegacyBootstrap = false;
    bool sawTemplateRuntime = false;
    bool sawBlockedManifest = false;
    for (const ri::content::PluginValidationIssue& issue : data.issues) {
        if (issue.message.find("Plugin blocked by policy: external.tool (unsigned plugins disabled)") != std::string::npos) {
            sawUnsignedPolicyIssue = true;
        }
    }
    for (const ri::content::PluginHookBinding& hook : data.hookBindings) {
        if (hook.pluginId == "legacy.telemetry" && hook.hookPhase == "startup" && hook.eventName == "bootstrap") {
            sawLegacyBootstrap = true;
        }
        if (hook.pluginId == "template.runtime" && hook.hookPhase == "runtime" && hook.eventName == "frame_sample") {
            sawTemplateRuntime = true;
        }
    }
    if (const ri::content::PluginManifestEntry* blocked = ri::content::FindPluginManifestEntry(data, "external.tool")) {
        sawBlockedManifest = blocked->blockedByPolicy
            && blocked->policyBlockReason == "unsigned plugins disabled"
            && blocked->sourceKind == ri::content::PluginSourceKind::External;
    }

    fs::remove_all(root, ec);
    if (!Expect(sawUnsignedPolicyIssue) || !Expect(sawLegacyBootstrap) || !Expect(sawTemplateRuntime)
        || !Expect(sawBlockedManifest)) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
