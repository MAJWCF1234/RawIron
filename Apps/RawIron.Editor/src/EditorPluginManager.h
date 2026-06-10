#pragma once

#include "RawIron/Content/ExtensionDescriptor.h"
#include "RawIron/Content/PluginProjectData.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ri::editor {

struct PluginStorePackage {
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string category;
    std::string description;
    std::string tagLine;
    std::string badge;
    std::vector<std::string> tags;
    std::filesystem::path packageRoot;
    int loadOrder = 100;
    std::string hookGroup;
    std::vector<std::string> hookLines;
    std::string manifestLine;
    ri::content::ExtensionDescriptor extension{};
};

struct PluginInstallResult {
    bool success = false;
    std::string message;
};

[[nodiscard]] std::filesystem::path ResolvePluginStoreRoot(const std::filesystem::path& workspaceRoot);

[[nodiscard]] std::vector<PluginStorePackage> ListPluginStorePackages(const std::filesystem::path& workspaceRoot);

[[nodiscard]] bool IsPluginInstalled(const ri::content::PluginProjectData& projectData, std::string_view pluginId);

[[nodiscard]] PluginInstallResult InstallPluginStorePackage(const std::filesystem::path& gameRoot,
                                                            const PluginStorePackage& package);

[[nodiscard]] PluginInstallResult SetPluginEnabled(const std::filesystem::path& gameRoot,
                                                   std::string_view pluginId,
                                                   bool enabled);

[[nodiscard]] PluginInstallResult UninstallPluginStorePackage(const std::filesystem::path& gameRoot,
                                                              std::string_view pluginId);

} // namespace ri::editor
