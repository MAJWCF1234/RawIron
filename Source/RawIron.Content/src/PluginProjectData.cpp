#include "RawIron/Content/PluginProjectData.h"

#include "RawIron/Content/PluginHookBindingParser.h"
#include "RawIron/Content/PluginRuntime.h"
#include "RawIron/Content/ScriptScalars.h"
#include "RawIron/Core/Detail/JsonScan.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <utility>

namespace ri::content {

namespace {

namespace fs = std::filesystem;

std::string Trim(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1U])) != 0) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

std::vector<std::string> SplitCsvLine(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;
    for (const char ch : line) {
        if (ch == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (ch == ',' && !inQuotes) {
            tokens.push_back(Trim(current));
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    tokens.push_back(Trim(current));
    return tokens;
}

void AppendUniqueIssue(std::vector<PluginValidationIssue>& issues, const std::string& message) {
    if (std::find_if(issues.begin(), issues.end(), [&](const PluginValidationIssue& issue) {
            return issue.message == message;
        }) != issues.end()) {
        return;
    }
    issues.push_back(PluginValidationIssue{.message = message});
}

[[nodiscard]] int ParseLoadOrderValue(const std::string& text) {
    try {
        return std::stoi(text);
    } catch (...) {
        return 0;
    }
}

[[nodiscard]] std::string NormalizePathToken(std::string_view text) {
    std::string normalized{};
    normalized.reserve(text.size());
    for (const char ch : text) {
        const char lowered = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        normalized.push_back(lowered == '\\' ? '/' : lowered);
    }
    return normalized;
}

[[nodiscard]] bool LooksLikeManifestHeader(const std::vector<std::string>& tokens) {
    if (tokens.size() < 4U) {
        return false;
    }
    const std::string first = NormalizePathToken(tokens[0]);
    const std::string second = NormalizePathToken(tokens[1]);
    const std::string third = NormalizePathToken(tokens[2]);
    const std::string fourth = NormalizePathToken(tokens[3]);
    return first == "plugin_id" || first == "id" || second == "version" || third == "category"
        || fourth == "entry" || fourth == "entrypath";
}

[[nodiscard]] PluginSourceKind ClassifyPluginSource(const PluginManifestEntry& entry) {
    const std::string normalized = NormalizePathToken(entry.entryPath);
    if (normalized.empty()) {
        return PluginSourceKind::External;
    }
    if (normalized.rfind("plugins/", 0) == 0 || normalized == "plugins" || normalized.find("/plugins/") != std::string::npos) {
        return PluginSourceKind::Project;
    }
    if (normalized.rfind("mods/", 0) == 0 || normalized == "mods" || normalized.find("/mods/") != std::string::npos) {
        return PluginSourceKind::Mod;
    }
    if (normalized.find("://") != std::string::npos || normalized.find(':') != std::string::npos || normalized.rfind("../", 0) == 0) {
        return PluginSourceKind::External;
    }
    return PluginSourceKind::Project;
}

[[nodiscard]] bool RelativePathEscapesRoot(const fs::path& relative) {
    if (relative.empty() || relative.is_absolute()) {
        return true;
    }
    const auto first = relative.begin();
    return first != relative.end() && *first == "..";
}

[[nodiscard]] bool IsPluginTreatedAsUnsigned(const PluginManifestEntry& entry) {
    return ClassifyPluginSource(entry) == PluginSourceKind::External;
}

[[nodiscard]] std::string PolicyBlockReason(const PluginPolicy& policy, const PluginManifestEntry& entry) {
    const PluginSourceKind sourceKind = ClassifyPluginSource(entry);
    if (!policy.allowProjectPlugins && sourceKind == PluginSourceKind::Project) {
        return "project plugins disabled";
    }
    if (!policy.allowModPlugins && sourceKind == PluginSourceKind::Mod) {
        return "mod plugins disabled";
    }
    if (!policy.allowUnsignedPlugins && IsPluginTreatedAsUnsigned(entry)) {
        return "unsigned plugins disabled";
    }
    return {};
}

[[nodiscard]] std::map<std::string, int, std::less<>> LoadPluginLoadOrder(const fs::path& path) {
    std::map<std::string, int, std::less<>> order{};
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return order;
    }
    std::string line{};
    while (std::getline(stream, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            if (!line.empty()) {
                order[line] = static_cast<int>(order.size());
            }
            continue;
        }
        const std::string pluginId = Trim(line.substr(0, equals));
        const int value = ParseLoadOrderValue(Trim(line.substr(equals + 1U)));
        if (!pluginId.empty()) {
            order[pluginId] = value;
        }
    }
    return order;
}

void LoadManifestPlugins(const fs::path& path, std::vector<PluginManifestEntry>& out) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return;
    }
    std::string line{};
    while (std::getline(stream, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::vector<std::string> tokens = SplitCsvLine(line);
        if (tokens.size() < 4U) {
            continue;
        }
        if (LooksLikeManifestHeader(tokens)) {
            continue;
        }
        out.push_back(PluginManifestEntry{
            .id = tokens[0],
            .version = tokens[1],
            .category = tokens[2],
            .entryPath = tokens[3],
        });
    }
}

void LoadRegistryJson(const fs::path& path, std::vector<PluginRegistryEntry>& out) {
    const std::string text = ri::core::detail::ReadTextFile(path);
    if (text.empty()) {
        return;
    }
    for (const std::string& id : ri::core::detail::ExtractJsonStringArray(text, "plugins")) {
        if (id.empty()) {
            continue;
        }
        out.push_back(PluginRegistryEntry{.id = id});
    }
    if (!out.empty()) {
        return;
    }
    for (const std::string_view objectText : ri::core::detail::SplitJsonArrayObjects(text, "plugins")) {
        PluginRegistryEntry entry{};
        if (const std::optional<std::string> id = ri::core::detail::ExtractJsonString(objectText, "id")) {
            entry.id = *id;
        }
        if (entry.id.empty()) {
            continue;
        }
        entry.enabled = ri::core::detail::ExtractJsonBool(objectText, "enabled").value_or(true);
        if (const std::optional<std::string> hookGroup = ri::core::detail::ExtractJsonString(objectText, "hookGroup")) {
            entry.hookGroup = *hookGroup;
        }
        if (const std::optional<std::string> description =
                ri::core::detail::ExtractJsonString(objectText, "description")) {
            entry.description = *description;
        }
        if (const std::optional<std::string> author = ri::core::detail::ExtractJsonString(objectText, "author")) {
            entry.author = *author;
        }
        out.push_back(std::move(entry));
    }
}

PluginPolicy LoadPluginPolicy(const fs::path& path) {
    PluginPolicy policy{};
    const ScriptScalarMap scalars = LoadScriptScalars(path);
    policy.allowRuntimeOverrides = ScriptScalarOrBool(scalars, "allow_runtime_plugin_overrides", true);
    policy.allowUnsignedPlugins = ScriptScalarOrBool(scalars, "allow_unsigned_plugins", false);
    policy.allowModPlugins = ScriptScalarOrBool(scalars, "allow_mod_plugins", true);
    policy.allowProjectPlugins = ScriptScalarOrBool(scalars, "allow_project_plugins", true);
    policy.maxHookChain = ScriptScalarOrIntClamped(scalars, "max_plugin_hook_chain", 16, 1, 128);
    policy.startupTimeoutMs = ScriptScalarOrIntClamped(scalars, "plugin_startup_timeout_ms", 250, 16, 10000);
    policy.sandboxLevel = ScriptScalarOrIntClamped(scalars, "plugin_sandbox_level", 2, 0, 4);
    return policy;
}

void ValidatePluginProjectDataInternal(PluginProjectData& data) {
    const std::map<std::string, int, std::less<>> loadOrder =
        LoadPluginLoadOrder(data.gameRoot / "plugins" / "load_order.cfg");

    std::map<std::string, PluginRegistryEntry, std::less<>> registryById{};
    for (const PluginRegistryEntry& entry : data.registryEntries) {
        registryById[entry.id] = entry;
    }

    std::map<std::string, PluginManifestEntry, std::less<>> manifestById{};
    for (const PluginManifestEntry& entry : data.manifestEntries) {
        if (manifestById.contains(entry.id)) {
            AppendUniqueIssue(data.issues, "Duplicate manifest plugin id: " + entry.id);
        }
        manifestById[entry.id] = entry;
    }

    for (const PluginRegistryEntry& entry : data.registryEntries) {
        if (!manifestById.contains(entry.id)) {
            AppendUniqueIssue(data.issues, "Registry plugin missing from manifest: " + entry.id);
        }
    }

    for (const PluginManifestEntry& entry : data.manifestEntries) {
        if (!registryById.contains(entry.id)) {
            AppendUniqueIssue(data.issues, "Manifest plugin missing from registry: " + entry.id);
        }
        if (!loadOrder.contains(entry.id)) {
            AppendUniqueIssue(data.issues, "Manifest plugin missing load order: " + entry.id);
        }
    }

    for (PluginManifestEntry& entry : data.manifestEntries) {
        const PluginEntryResolution resolution = ResolvePluginEntryPath(data.gameRoot, entry.entryPath);
        entry.sourceKind = resolution.sourceKind;
        entry.resolvedEntryPath = resolution.resolvedPath;
        entry.entryPathValid = resolution.valid;
        entry.entryExists = resolution.exists;
        entry.entryIsRemote = resolution.remoteReference;
        if (!resolution.valid) {
            AppendUniqueIssue(data.issues, "Plugin entry rejected: " + entry.id + " (" + resolution.issue + ")");
        } else if (!resolution.remoteReference && !resolution.exists) {
            AppendUniqueIssue(data.issues, "Plugin entry file missing: " + entry.id + " (" + entry.entryPath + ")");
        }
        entry.policyBlockReason = PolicyBlockReason(data.policy, entry);
        entry.blockedByPolicy = !entry.policyBlockReason.empty();
        if (entry.blockedByPolicy) {
            AppendUniqueIssue(data.issues, "Plugin blocked by policy: " + entry.id + " (" + entry.policyBlockReason + ")");
        }
    }

    for (const PluginHookBinding& hook : data.hookBindings) {
        if (!manifestById.contains(hook.pluginId)) {
            AppendUniqueIssue(data.issues, "Hook references unknown plugin: " + hook.pluginId);
        }
    }

    AppendPluginHookHandlerIssues(data, data.issues);

    data.activePlugins = BuildActivePlugins(data);
}

} // namespace

PluginProjectData LoadPluginProjectData(const fs::path& gameRoot) {
    PluginProjectData data{};
    data.gameRoot = gameRoot;
    LoadManifestPlugins(gameRoot / "plugins" / "manifest.plugins", data.manifestEntries);
    LoadRegistryJson(gameRoot / "plugins" / "registry.json", data.registryEntries);
    data.hookBindings = LoadPluginHookBindings(gameRoot / "plugins" / "hooks.riplugin");
    data.policy = LoadPluginPolicy(gameRoot / "config" / "plugins.policy");
    data.tuningScalars = LoadScriptScalars(gameRoot / "scripts" / "plugins.riscript");
    ValidatePluginProjectDataInternal(data);
    return data;
}

PluginEntryResolution ResolvePluginEntryPath(const fs::path& gameRoot, const std::string_view entryPath) {
    PluginEntryResolution resolution{};
    const std::string entry(entryPath);
    if (entry.empty()) {
        resolution.issue = "entry path is empty";
        return resolution;
    }

    const std::string normalizedToken = NormalizePathToken(entry);
    if (normalizedToken.find("://") != std::string::npos) {
        resolution.sourceKind = PluginSourceKind::External;
        resolution.remoteReference = true;
        resolution.valid = true;
        return resolution;
    }

    const fs::path rawPath(entry);
    if (rawPath.is_absolute() || normalizedToken.find(':') != std::string::npos) {
        resolution.sourceKind = PluginSourceKind::External;
        std::error_code error{};
        resolution.resolvedPath = fs::weakly_canonical(rawPath, error);
        if (error) {
            resolution.resolvedPath = rawPath.lexically_normal();
        }
        resolution.exists = fs::is_regular_file(resolution.resolvedPath, error);
        resolution.valid = true;
        return resolution;
    }

    const fs::path normalizedRelative = rawPath.lexically_normal();
    if (RelativePathEscapesRoot(normalizedRelative)) {
        resolution.issue = "relative entry escapes the game root";
        return resolution;
    }
    PluginManifestEntry classificationProbe{};
    classificationProbe.entryPath = normalizedRelative.generic_string();
    resolution.sourceKind = ClassifyPluginSource(classificationProbe);

    std::error_code error{};
    const fs::path canonicalRoot = fs::weakly_canonical(gameRoot, error);
    if (error || canonicalRoot.empty()) {
        resolution.issue = "game root cannot be resolved";
        return resolution;
    }
    const fs::path candidate = fs::weakly_canonical(canonicalRoot / normalizedRelative, error);
    resolution.resolvedPath = error ? (canonicalRoot / normalizedRelative).lexically_normal() : candidate;
    error.clear();
    const fs::path relativeToRoot = fs::relative(resolution.resolvedPath, canonicalRoot, error);
    if (error || RelativePathEscapesRoot(relativeToRoot)) {
        resolution.issue = "resolved entry escapes the game root";
        return resolution;
    }
    resolution.exists = fs::is_regular_file(resolution.resolvedPath, error);
    resolution.valid = true;
    return resolution;
}

std::vector<ActivePlugin> BuildActivePlugins(const PluginProjectData& data) {
    const std::map<std::string, int, std::less<>> loadOrder =
        LoadPluginLoadOrder(data.gameRoot / "plugins" / "load_order.cfg");

    std::map<std::string, PluginRegistryEntry, std::less<>> registryById{};
    for (const PluginRegistryEntry& entry : data.registryEntries) {
        registryById[entry.id] = entry;
    }

    std::vector<ActivePlugin> active{};
    for (const PluginManifestEntry& manifest : data.manifestEntries) {
        const auto registryIt = registryById.find(manifest.id);
        if (registryIt == registryById.end() || !registryIt->second.enabled) {
            continue;
        }
        if (IsPluginBlockedByPolicy(manifest)) {
            continue;
        }
        if (!manifest.entryPathValid || (!manifest.entryIsRemote && !manifest.entryExists)) {
            continue;
        }
        ActivePlugin plugin{
            .manifest = manifest,
            .registry = registryIt->second,
            .loadOrder = 0,
        };
        const auto orderIt = loadOrder.find(manifest.id);
        if (orderIt != loadOrder.end()) {
            plugin.loadOrder = orderIt->second;
        }
        for (const PluginHookBinding& hook : data.hookBindings) {
            if (hook.pluginId == manifest.id) {
                plugin.hooks.push_back(hook);
            }
        }
        std::sort(plugin.hooks.begin(), plugin.hooks.end(), [](const PluginHookBinding& a, const PluginHookBinding& b) {
            if (a.hookPhase != b.hookPhase) {
                return a.hookPhase < b.hookPhase;
            }
            if (a.priority != b.priority) {
                return a.priority < b.priority;
            }
            return a.eventName < b.eventName;
        });
        active.push_back(std::move(plugin));
    }

    std::sort(active.begin(), active.end(), [](const ActivePlugin& a, const ActivePlugin& b) {
        if (a.loadOrder != b.loadOrder) {
            return a.loadOrder < b.loadOrder;
        }
        return a.manifest.id < b.manifest.id;
    });
    return active;
}

std::vector<PluginHookBinding> CollectHooksForPhase(const PluginProjectData& data, const std::string_view hookPhase) {
    std::vector<PluginHookBinding> hooks{};
    for (const ActivePlugin& plugin : data.activePlugins) {
        for (const PluginHookBinding& hook : plugin.hooks) {
            if (hook.hookPhase == hookPhase) {
                hooks.push_back(hook);
            }
        }
    }
    std::sort(hooks.begin(), hooks.end(), [](const PluginHookBinding& a, const PluginHookBinding& b) {
        if (a.priority != b.priority) {
            return a.priority < b.priority;
        }
        return a.pluginId < b.pluginId;
    });
    return hooks;
}

std::string SummarizePluginProjectData(const PluginProjectData& data) {
    std::ostringstream stream;
    stream << data.activePlugins.size() << " active / " << data.manifestEntries.size() << " manifest";
    stream << " · " << data.hookBindings.size() << " hooks";
    const std::size_t blockedCount = static_cast<std::size_t>(std::count_if(
        data.manifestEntries.begin(), data.manifestEntries.end(), [](const PluginManifestEntry& entry) {
            return entry.blockedByPolicy;
        }));
    if (blockedCount > 0U) {
        stream << " · " << blockedCount << " blocked";
    }
    if (!data.issues.empty()) {
        stream << " · " << data.issues.size() << " issue(s)";
    }
    return stream.str();
}

std::string_view ToString(const PluginSourceKind kind) {
    switch (kind) {
        case PluginSourceKind::Project:
            return "project";
        case PluginSourceKind::Mod:
            return "mod";
        case PluginSourceKind::External:
            return "external";
    }
    return "project";
}

bool IsPluginBlockedByPolicy(const PluginManifestEntry& entry) {
    return entry.blockedByPolicy;
}

const PluginManifestEntry* FindPluginManifestEntry(const PluginProjectData& data, const std::string_view pluginId) {
    for (const PluginManifestEntry& entry : data.manifestEntries) {
        if (entry.id == pluginId) {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace ri::content
