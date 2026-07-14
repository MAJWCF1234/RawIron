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
    const fs::path duplicateRoot = fs::temp_directory_path() / "rawiron_plugin_project_duplicate_smoke";
    std::error_code ec{};
    fs::remove_all(root, ec);
    fs::remove_all(duplicateRoot, ec);

    WriteTextFile(root / "plugins" / "manifest.plugins",
                  "legacy.telemetry,1.0,telemetry,plugins/hooks.riplugin\n"
                  "template.runtime,1.0,utility,plugins/hooks.riplugin\n"
                  "descriptor.mod,1.0,utility,plugins/hooks.riplugin\n"
                  "missing.local,1.0,utility,plugins/missing.riplugin\n"
                  "escape.local,1.0,utility,plugins/../../escape.riplugin\n"
                  "external.tool,1.0,utility,https://example.invalid/external.plugin\n");
    WriteTextFile(root / "plugins" / "load_order.cfg",
                  "legacy.telemetry\n"
                  "template.runtime=10\n"
                  "descriptor.mod=15\n"
                  "missing.local=16\n"
                  "escape.local=17\n"
                  "external.tool=20\n");
    WriteTextFile(root / "plugins" / "registry.json",
                  "{ \"plugins\": [\"legacy.telemetry\", \"template.runtime\", \"descriptor.mod\", \"missing.local\", \"escape.local\", \"external.tool\"] }\n");
    WriteTextFile(root / "plugins" / "hooks.riplugin",
                  "on_startup=legacy.telemetry\n"
                  "on_runtime=template.runtime\n"
                  "runtime.mod=descriptor.mod\n"
                  "runtime,external.tool,frame_sample,20\n");
    WriteTextFile(root / "config" / "plugins.policy",
                  "allow_unsigned_plugins=0\n"
                  "allow_mod_plugins=1\n"
                  "allow_project_plugins=1\n");
    WriteTextFile(root / "scripts" / "plugins.riscript", "plugin_hook_batch_size=4\n");
    WriteTextFile(root / "mods" / "safe.riplugin", "# mod entry\n");

    const ri::content::PluginEntryResolution normalizedMod =
        ri::content::ResolvePluginEntryPath(root, "plugins/../mods/safe.riplugin");
    if (!normalizedMod.valid || !normalizedMod.exists
        || normalizedMod.sourceKind != ri::content::PluginSourceKind::Mod) {
        return EXIT_FAILURE;
    }

    const ri::content::PluginProjectData data = ri::content::LoadPluginProjectData(root);
    if (!Expect(data.manifestEntries.size() == 6U)) {
        return EXIT_FAILURE;
    }
    if (!Expect(data.registryEntries.size() == 6U)) {
        return EXIT_FAILURE;
    }
    if (!Expect(data.hookBindings.size() == 4U)) {
        return EXIT_FAILURE;
    }
    if (!Expect(data.activePlugins.size() == 3U)) {
        return EXIT_FAILURE;
    }
    if (!Expect(data.activePlugins.front().manifest.id == "legacy.telemetry")) {
        return EXIT_FAILURE;
    }

    bool sawUnsignedPolicyIssue = false;
    bool sawLegacyBootstrap = false;
    bool sawTemplateRuntime = false;
    bool sawDescriptorHook = false;
    bool sawBlockedManifest = false;
    bool sawMissingEntry = false;
    bool sawRejectedEscape = false;
    for (const ri::content::PluginValidationIssue& issue : data.issues) {
        if (issue.message.find("Plugin blocked by policy: external.tool (unsigned plugins disabled)") != std::string::npos) {
            sawUnsignedPolicyIssue = true;
        }
        sawMissingEntry = sawMissingEntry || issue.message.find("Plugin entry file missing: missing.local") != std::string::npos;
        sawRejectedEscape = sawRejectedEscape || issue.message.find("Plugin entry rejected: escape.local") != std::string::npos;
    }
    for (const ri::content::PluginHookBinding& hook : data.hookBindings) {
        if (hook.pluginId == "legacy.telemetry" && hook.hookPhase == "startup" && hook.eventName == "bootstrap") {
            sawLegacyBootstrap = true;
        }
        if (hook.pluginId == "template.runtime" && hook.hookPhase == "runtime" && hook.eventName == "frame_sample") {
            sawTemplateRuntime = true;
        }
        if (hook.pluginId == "descriptor.mod" && hook.hookPhase == "runtime.mod" && hook.eventName == "default") {
            sawDescriptorHook = true;
        }
    }
    if (const ri::content::PluginManifestEntry* blocked = ri::content::FindPluginManifestEntry(data, "external.tool")) {
        sawBlockedManifest = blocked->blockedByPolicy
            && blocked->policyBlockReason == "unsigned plugins disabled"
            && blocked->sourceKind == ri::content::PluginSourceKind::External;
    }

    if (!Expect(sawUnsignedPolicyIssue) || !Expect(sawLegacyBootstrap) || !Expect(sawTemplateRuntime)
        || !Expect(sawDescriptorHook) || !Expect(sawBlockedManifest) || !Expect(sawMissingEntry)
        || !Expect(sawRejectedEscape)) {
        return EXIT_FAILURE;
    }

    WriteTextFile(duplicateRoot / "plugins" / "manifest.plugins",
                  "duplicate.test,1.0,utility,plugins/hooks.riplugin\n");
    WriteTextFile(duplicateRoot / "plugins" / "load_order.cfg", "duplicate.test=10\n");
    WriteTextFile(duplicateRoot / "plugins" / "registry.json",
                  "{\"plugins\":[{\"id\":\"duplicate.test\",\"enabled\":false},"
                  "{\"id\":\"duplicate.test\",\"enabled\":true}]}\n");
    WriteTextFile(duplicateRoot / "plugins" / "hooks.riplugin",
                  "runtime,duplicate.test,frame_sample,10\n");
    WriteTextFile(duplicateRoot / "config" / "plugins.policy", "allow_project_plugins=1\n");

    const ri::content::PluginProjectData duplicateData = ri::content::LoadPluginProjectData(duplicateRoot);
    bool sawDuplicateRegistryIssue = false;
    for (const ri::content::PluginValidationIssue& issue : duplicateData.issues) {
        sawDuplicateRegistryIssue = sawDuplicateRegistryIssue
            || issue.message.find("Duplicate registry plugin id: duplicate.test") != std::string::npos;
    }
    if (!sawDuplicateRegistryIssue || !duplicateData.activePlugins.empty()) {
        return EXIT_FAILURE;
    }

    fs::remove_all(root, ec);
    fs::remove_all(duplicateRoot, ec);
    return EXIT_SUCCESS;
}
