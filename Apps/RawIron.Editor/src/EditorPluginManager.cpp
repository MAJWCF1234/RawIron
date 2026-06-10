#include "EditorPluginManager.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>

namespace ri::editor {

namespace fs = std::filesystem;

namespace {

enum class AppendLineResult {
    Appended,
    AlreadyPresent,
    WriteFailed,
};

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

std::vector<std::string> SplitCsvColumns(std::string_view line) {
    std::vector<std::string> columns{};
    std::stringstream stream{std::string(line)};
    std::string cell{};
    while (std::getline(stream, cell, ',')) {
        columns.push_back(Trim(cell));
    }
    return columns;
}

AppendLineResult AppendLineIfMissing(const fs::path& path, const std::string& line) {
    std::ifstream input(path);
    if (input.is_open()) {
        std::string row{};
        while (std::getline(input, row)) {
            if (Trim(row) == line) {
                return AppendLineResult::AlreadyPresent;
            }
        }
    }
    std::ofstream output(path, std::ios::out | std::ios::app);
    if (!output.is_open()) {
        return AppendLineResult::WriteFailed;
    }
    output << line << '\n';
    return output.good() ? AppendLineResult::Appended : AppendLineResult::WriteFailed;
}

bool RewriteRegistryEnabled(const fs::path& registryPath, const std::string& pluginId, const bool enabled) {
    std::string text = ri::core::detail::ReadTextFile(registryPath);
    if (text.empty()) {
        text = "{\n  \"plugins\": []\n}\n";
    }
    const std::optional<std::size_t> pluginsKey = ri::core::detail::FindJsonKey(text, "plugins");
    if (!pluginsKey.has_value()) {
        return false;
    }

    bool updated = false;
    std::string rebuilt = text;
    for (const std::string_view objectText : ri::core::detail::SplitJsonArrayObjects(text, "plugins")) {
        const std::optional<std::string> id = ri::core::detail::ExtractJsonString(objectText, "id");
        if (!id.has_value() || *id != pluginId) {
            continue;
        }
        const std::string enabledToken = enabled ? "true" : "false";
        const std::string searchTrue = "\"enabled\": true";
        const std::string searchFalse = "\"enabled\": false";
        const std::size_t objectOffset = static_cast<std::size_t>(objectText.data() - text.data());
        const std::size_t truePos = text.find(searchTrue, objectOffset);
        const std::size_t falsePos = text.find(searchFalse, objectOffset);
        if (truePos != std::string::npos && truePos < objectOffset + objectText.size()) {
            rebuilt.replace(truePos, searchTrue.size(), "\"enabled\": " + enabledToken);
            updated = true;
            break;
        }
        if (falsePos != std::string::npos && falsePos < objectOffset + objectText.size()) {
            rebuilt.replace(falsePos, searchFalse.size(), "\"enabled\": " + enabledToken);
            updated = true;
            break;
        }
    }
    if (!updated) {
        return false;
    }
    return ri::core::detail::WriteTextFile(registryPath, rebuilt);
}

bool RewriteTextFileFiltered(const fs::path& path,
                             const std::function<bool(std::string_view line)>& keepLine) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }
    std::vector<std::string> kept{};
    std::string row{};
    while (std::getline(input, row)) {
        if (keepLine(row)) {
            kept.push_back(row);
        }
    }
    std::ofstream output(path, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    for (std::size_t index = 0; index < kept.size(); ++index) {
        output << kept[index];
        if (index + 1U < kept.size()) {
            output << '\n';
        }
    }
    if (!kept.empty()) {
        output << '\n';
    }
    return output.good();
}

bool RemovePluginFromRegistryJson(const fs::path& registryPath, const std::string& pluginId) {
    std::string text = ri::core::detail::ReadTextFile(registryPath);
    if (text.empty()) {
        return false;
    }

    std::string rebuilt = text;
    bool removed = false;
    for (const std::string_view objectText : ri::core::detail::SplitJsonArrayObjects(text, "plugins")) {
        const std::optional<std::string> id = ri::core::detail::ExtractJsonString(objectText, "id");
        if (!id.has_value() || *id != pluginId) {
            continue;
        }
        const std::size_t objectOffset = static_cast<std::size_t>(objectText.data() - text.data());
        std::size_t eraseBegin = objectOffset;
        while (eraseBegin > 0 && (text[eraseBegin - 1] == '\n' || text[eraseBegin - 1] == '\r' || text[eraseBegin - 1] == ' ')) {
            --eraseBegin;
        }
        if (eraseBegin > 0 && text[eraseBegin - 1] == ',') {
            --eraseBegin;
        }
        std::size_t eraseEnd = objectOffset + objectText.size();
        while (eraseEnd < text.size() && (text[eraseEnd] == '\n' || text[eraseEnd] == '\r' || text[eraseEnd] == ' ')) {
            ++eraseEnd;
        }
        if (eraseEnd < text.size() && text[eraseEnd] == ',') {
            ++eraseEnd;
        }
        rebuilt.erase(eraseBegin, eraseEnd - eraseBegin);
        removed = true;
        break;
    }
    if (!removed) {
        return false;
    }
    return ri::core::detail::WriteTextFile(registryPath, rebuilt);
}

bool ManifestLineReferencesPluginId(std::string_view line, const std::string& pluginId) {
    const std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
        return false;
    }
    std::stringstream stream(trimmed);
    std::string cell{};
    if (!std::getline(stream, cell, ',')) {
        return false;
    }
    return Trim(cell) == pluginId;
}

bool LoadOrderLineReferencesPluginId(std::string_view line, const std::string& pluginId) {
    const std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
        return false;
    }
    const std::size_t equals = trimmed.find('=');
    if (equals == std::string::npos) {
        return trimmed == pluginId;
    }
    return Trim(trimmed.substr(0, equals)) == pluginId;
}

bool HookLineReferencesPluginId(std::string_view line, const std::string& pluginId) {
    const std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
        return false;
    }
    const std::size_t equals = trimmed.find('=');
    if (equals != std::string::npos) {
        return Trim(trimmed.substr(equals + 1U)) == pluginId;
    }
    std::stringstream stream(trimmed);
    std::string cell{};
    std::vector<std::string> columns{};
    while (std::getline(stream, cell, ',')) {
        columns.push_back(Trim(cell));
    }
    return columns.size() >= 2U && columns[1] == pluginId;
}

std::string PolicyBlockReasonForPackage(const ri::content::PluginPolicy& policy, const PluginStorePackage& package) {
    const std::vector<std::string> columns = SplitCsvColumns(package.manifestLine);
    ri::content::PluginManifestEntry manifest{};
    manifest.id = package.id;
    if (columns.size() >= 4U) {
        manifest.entryPath = columns[3];
    } else {
        manifest.entryPath = "plugins/hooks.riplugin";
    }
    manifest.sourceKind = manifest.entryPath.rfind("mods/", 0) == 0
        ? ri::content::PluginSourceKind::Mod
        : ri::content::PluginSourceKind::Project;
    if (!policy.allowProjectPlugins && manifest.sourceKind == ri::content::PluginSourceKind::Project) {
        return "project plugins disabled";
    }
    if (!policy.allowModPlugins && manifest.sourceKind == ri::content::PluginSourceKind::Mod) {
        return "mod plugins disabled";
    }
    if (!policy.allowUnsignedPlugins && manifest.entryPath.find("://") != std::string::npos) {
        return "unsigned plugins disabled";
    }
    return {};
}

std::optional<PluginStorePackage> LoadStorePackage(const fs::path& packageRoot) {
    const fs::path packageJson = packageRoot / "package.riplugin.json";
    if (!fs::exists(packageJson)) {
        return std::nullopt;
    }
    const std::string text = ri::core::detail::ReadTextFile(packageJson);
    if (text.empty()) {
        return std::nullopt;
    }

    PluginStorePackage package{};
    package.packageRoot = packageRoot;
    package.id = ri::core::detail::ExtractJsonString(text, "id").value_or(packageRoot.filename().string());
    package.name = ri::core::detail::ExtractJsonString(text, "name").value_or(package.id);
    package.version = ri::core::detail::ExtractJsonString(text, "version").value_or("1.0");
    package.author = ri::core::detail::ExtractJsonString(text, "author").value_or("RawIron Community");
    package.category = ri::core::detail::ExtractJsonString(text, "category").value_or("utility");
    package.description = ri::core::detail::ExtractJsonString(text, "description").value_or("");
    package.tagLine = ri::core::detail::ExtractJsonString(text, "tagLine").value_or("");
    package.badge = ri::core::detail::ExtractJsonString(text, "badge").value_or("");
    package.manifestLine = ri::core::detail::ExtractJsonString(text, "manifestLine").value_or(
        package.id + "," + package.version + "," + package.category + ",plugins/hooks.riplugin");
    package.hookGroup = ri::core::detail::ExtractJsonString(text, "hookGroup").value_or("runtime.mod");
    package.loadOrder = static_cast<int>(
        ri::core::detail::ExtractJsonInt(text, "loadOrder").value_or(100));
    package.tags = ri::core::detail::ExtractJsonStringArray(text, "tags");
    package.hookLines = ri::core::detail::ExtractJsonStringArray(text, "hooks");
    if (const std::optional<ri::content::ExtensionDescriptor> extension = ri::content::ExtractExtensionDescriptor(text)) {
        package.extension = *extension;
    } else {
        package.extension.id = package.id;
        package.extension.displayName = package.name;
        package.extension.version = package.version;
        package.extension.kind = ri::content::ExtensionKind::Plugin;
        package.extension.scope = ri::content::ExtensionScope::Shared;
        package.extension.host = ri::content::ExtensionHost::Runtime;
        package.extension.entry = "plugins/hooks.riplugin";
        package.extension.description = package.description;
        package.extension.capabilities = package.tags;
        package.extension.tags = package.tags;
    }
    return package;
}

} // namespace

fs::path ResolvePluginStoreRoot(const fs::path& workspaceRoot) {
    return workspaceRoot / "Plugins" / "Store";
}

std::vector<PluginStorePackage> ListPluginStorePackages(const fs::path& workspaceRoot) {
    std::vector<PluginStorePackage> packages{};
    const fs::path storeRoot = ResolvePluginStoreRoot(workspaceRoot);
    std::error_code ec{};
    if (!fs::exists(storeRoot, ec)) {
        return packages;
    }
    for (const fs::directory_entry& entry : fs::directory_iterator(storeRoot, ec)) {
        if (ec || !entry.is_directory()) {
            continue;
        }
        if (const std::optional<PluginStorePackage> loaded = LoadStorePackage(entry.path())) {
            packages.push_back(*loaded);
        }
    }
    std::sort(packages.begin(), packages.end(), [](const PluginStorePackage& a, const PluginStorePackage& b) {
        return a.name < b.name;
    });
    return packages;
}

bool IsPluginInstalled(const ri::content::PluginProjectData& projectData, const std::string_view pluginId) {
    for (const ri::content::PluginManifestEntry& entry : projectData.manifestEntries) {
        if (entry.id == pluginId) {
            return true;
        }
    }
    return false;
}

PluginInstallResult InstallPluginStorePackage(const fs::path& gameRoot, const PluginStorePackage& package) {
    PluginInstallResult result{};
    if (gameRoot.empty()) {
        result.message = "No game root selected.";
        return result;
    }

    const fs::path pluginsDir = gameRoot / "plugins";
    std::error_code ec{};
    fs::create_directories(pluginsDir, ec);

    const ri::content::PluginProjectData existing = ri::content::LoadPluginProjectData(gameRoot);
    if (IsPluginInstalled(existing, package.id)) {
        result.message = "Plugin already installed: " + package.id;
        return result;
    }
    if (const std::string blocked = PolicyBlockReasonForPackage(existing.policy, package); !blocked.empty()) {
        result.message = "Install blocked by policy: " + blocked + ".";
        return result;
    }

    if (AppendLineIfMissing(pluginsDir / "manifest.plugins", package.manifestLine) == AppendLineResult::WriteFailed) {
        result.message = "Failed to update manifest.plugins.";
        return result;
    }
    if (AppendLineIfMissing(pluginsDir / "load_order.cfg", package.id + "=" + std::to_string(package.loadOrder))
        == AppendLineResult::WriteFailed) {
        result.message = "Failed to update load_order.cfg.";
        return result;
    }
    for (const std::string& hookLine : package.hookLines) {
        if (AppendLineIfMissing(pluginsDir / "hooks.riplugin", hookLine) == AppendLineResult::WriteFailed) {
            result.message = "Failed to update hooks.riplugin.";
            return result;
        }
    }

    const fs::path registryPath = pluginsDir / "registry.json";
    std::string registryText = ri::core::detail::ReadTextFile(registryPath);
    if (registryText.empty()) {
        registryText = "{\n  \"plugins\": []\n}\n";
    }
    const std::string registryObject =
        "    {\n"
        "      \"id\": \"" + ri::core::detail::EscapeJsonString(package.id) + "\",\n"
        "      \"enabled\": true,\n"
        "      \"hookGroup\": \"" + ri::core::detail::EscapeJsonString(package.hookGroup) + "\",\n"
        "      \"description\": \"" + ri::core::detail::EscapeJsonString(package.description) + "\",\n"
        "      \"author\": \"" + ri::core::detail::EscapeJsonString(package.author) + "\"\n"
        "    }";
    const std::size_t insertPos = registryText.rfind(']');
    if (insertPos == std::string::npos) {
        result.message = "registry.json is malformed.";
        return result;
    }
    const bool hasEntries = registryText.find("\"id\"", 0) != std::string::npos;
    registryText.insert(insertPos, (hasEntries ? ",\n" : "\n") + registryObject + "\n  ");
    if (!ri::core::detail::WriteTextFile(registryPath, registryText)) {
        result.message = "Failed to update registry.json.";
        return result;
    }

    if (!package.tagLine.empty()) {
        const fs::path scriptPath = gameRoot / "scripts" / "plugins.riscript";
        if (AppendLineIfMissing(scriptPath, "# " + package.id) == AppendLineResult::WriteFailed
            || AppendLineIfMissing(scriptPath, "plugin." + package.id + ".installed=1") == AppendLineResult::WriteFailed) {
            result.message = "Installed plugin metadata but failed to update scripts/plugins.riscript.";
            return result;
        }
    }

    result.success = true;
    result.message = "Installed " + package.name + " (" + package.id + "). Reload project to dispatch hooks.";
    return result;
}

PluginInstallResult SetPluginEnabled(const fs::path& gameRoot, const std::string_view pluginId, const bool enabled) {
    PluginInstallResult result{};
    const fs::path registryPath = gameRoot / "plugins" / "registry.json";
    if (!RewriteRegistryEnabled(registryPath, std::string(pluginId), enabled)) {
        result.message = "Could not update enabled state for " + std::string(pluginId) + ".";
        return result;
    }
    result.success = true;
    result.message = std::string(enabled ? "Enabled " : "Disabled ") + std::string(pluginId) + ".";
    return result;
}

PluginInstallResult UninstallPluginStorePackage(const fs::path& gameRoot, const std::string_view pluginId) {
    PluginInstallResult result{};
    if (gameRoot.empty()) {
        result.message = "No game root selected.";
        return result;
    }
    const std::string id(pluginId);
    const ri::content::PluginProjectData existing = ri::content::LoadPluginProjectData(gameRoot);
    if (!IsPluginInstalled(existing, id)) {
        result.message = "Plugin not installed: " + id;
        return result;
    }

    const fs::path pluginsDir = gameRoot / "plugins";
    if (!RewriteTextFileFiltered(pluginsDir / "manifest.plugins",
                                 [&id](const std::string_view line) { return !ManifestLineReferencesPluginId(line, id); })) {
        result.message = "Failed to update manifest.plugins.";
        return result;
    }
    if (!RewriteTextFileFiltered(pluginsDir / "load_order.cfg",
                                 [&id](const std::string_view line) { return !LoadOrderLineReferencesPluginId(line, id); })) {
        result.message = "Failed to update load_order.cfg.";
        return result;
    }
    if (!RewriteTextFileFiltered(pluginsDir / "hooks.riplugin",
                                 [&id](const std::string_view line) { return !HookLineReferencesPluginId(line, id); })) {
        result.message = "Failed to update hooks.riplugin.";
        return result;
    }
    if (!RemovePluginFromRegistryJson(pluginsDir / "registry.json", id)) {
        result.message = "Failed to update registry.json.";
        return result;
    }

    const fs::path scriptPath = gameRoot / "scripts" / "plugins.riscript";
    if (fs::exists(scriptPath)) {
        (void)RewriteTextFileFiltered(scriptPath, [&id](const std::string_view line) {
            const std::string trimmed = Trim(line);
            if (trimmed == "# " + id) {
                return false;
            }
            if (trimmed.rfind("plugin." + id + ".", 0) == 0) {
                return false;
            }
            return true;
        });
    }

    result.success = true;
    result.message = "Uninstalled " + id + ". Reload project to refresh hook dispatch.";
    return result;
}

} // namespace ri::editor
