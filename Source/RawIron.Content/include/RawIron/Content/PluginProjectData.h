#pragma once

#include "RawIron/Content/ScriptScalars.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace ri::content {

enum class PluginSourceKind {
    Project,
    Mod,
    External,
};

struct PluginEntryResolution {
    PluginSourceKind sourceKind = PluginSourceKind::Project;
    std::filesystem::path resolvedPath{};
    bool valid = false;
    bool exists = false;
    bool remoteReference = false;
    std::string issue{};
};

/// Inventory row from `plugins/manifest.plugins`.
struct PluginManifestEntry {
    std::string id;
    std::string version;
    std::string category;
    std::string entryPath;
    PluginSourceKind sourceKind = PluginSourceKind::Project;
    std::filesystem::path resolvedEntryPath{};
    /// Defaults preserve manually assembled `PluginProjectData`; disk-loaded manifests always overwrite these.
    bool entryPathValid = true;
    bool entryExists = true;
    bool entryIsRemote = false;
    bool blockedByPolicy = false;
    std::string policyBlockReason;
};

/// Enable/metadata row from `plugins/registry.json`.
struct PluginRegistryEntry {
    std::string id;
    bool enabled = true;
    std::string hookGroup;
    std::string description;
    std::string author;
    /// Explicit engine surfaces granted to this plugin (for example `audio.runtime`).
    std::vector<std::string> capabilities;
};

/// Hook binding from `plugins/hooks.riplugin`.
struct PluginHookBinding {
    std::string hookPhase;
    std::string pluginId;
    std::string eventName;
    int priority = 0;
};

/// Parsed policy scalars from `config/plugins.policy`.
struct PluginPolicy {
    bool allowRuntimeOverrides = true;
    bool allowUnsignedPlugins = false;
    bool allowModPlugins = true;
    bool allowProjectPlugins = true;
    /// When enabled, hooks may only call handlers whose required capability is granted in registry.json.
    bool enforceDeclaredCapabilities = false;
    int maxHookChain = 16;
    int startupTimeoutMs = 250;
    int sandboxLevel = 2;
};

struct PluginValidationIssue {
    std::string message;
};

/// Resolved plugin ready for load-order execution.
struct ActivePlugin {
    PluginManifestEntry manifest{};
    PluginRegistryEntry registry{};
    int loadOrder = 0;
    std::vector<PluginHookBinding> hooks;
};

struct PluginProjectData {
    std::filesystem::path gameRoot;
    std::vector<PluginManifestEntry> manifestEntries;
    std::vector<PluginRegistryEntry> registryEntries;
    std::vector<PluginHookBinding> hookBindings;
    PluginPolicy policy{};
    ScriptScalarMap tuningScalars;
    std::vector<PluginValidationIssue> issues;
    std::vector<ActivePlugin> activePlugins;

    [[nodiscard]] bool ok() const { return issues.empty(); }
};

[[nodiscard]] PluginProjectData LoadPluginProjectData(const std::filesystem::path& gameRoot);

/// Resolves a manifest entry without allowing project/mod paths to escape `gameRoot`.
[[nodiscard]] PluginEntryResolution ResolvePluginEntryPath(const std::filesystem::path& gameRoot,
                                                           std::string_view entryPath);

[[nodiscard]] std::vector<ActivePlugin> BuildActivePlugins(const PluginProjectData& data);

[[nodiscard]] std::vector<PluginHookBinding> CollectHooksForPhase(const PluginProjectData& data,
                                                                  std::string_view hookPhase);

[[nodiscard]] std::string SummarizePluginProjectData(const PluginProjectData& data);

[[nodiscard]] std::string_view ToString(PluginSourceKind kind);

[[nodiscard]] bool IsPluginBlockedByPolicy(const PluginManifestEntry& entry);

[[nodiscard]] const PluginManifestEntry* FindPluginManifestEntry(const PluginProjectData& data, std::string_view pluginId);

} // namespace ri::content
