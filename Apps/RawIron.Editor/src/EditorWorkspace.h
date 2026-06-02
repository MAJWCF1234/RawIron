#pragma once

#include "RawIron/Content/GameManifest.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ri::editor {

enum class WorkspaceResourceCategory {
    Manifest,
    Level,
    Script,
    Test,
    UiScreen,
    Menu,
    Asset,
    Other,
};

struct WorkspaceGameEntry {
    std::string id;
    std::string displayName;
    std::filesystem::path rootPath;
};

struct WorkspaceResourceEntry {
    std::filesystem::path absolutePath;
    std::string relativePathUtf8;
    WorkspaceResourceCategory category = WorkspaceResourceCategory::Other;
};

[[nodiscard]] std::string WorkspaceCategoryLabel(WorkspaceResourceCategory category);
[[nodiscard]] std::uint32_t WorkspaceCategoryBit(WorkspaceResourceCategory category);
[[nodiscard]] std::string WorkspaceCategoryShortLabel(WorkspaceResourceCategory category);
[[nodiscard]] WorkspaceResourceCategory ClassifyRelativeGamePath(const std::filesystem::path& relativePath);
[[nodiscard]] bool IsLikelyTextResourcePath(const std::filesystem::path& path);
[[nodiscard]] std::vector<WorkspaceGameEntry> EnumerateWorkspaceGames(const std::filesystem::path& workspaceRoot);
[[nodiscard]] std::vector<WorkspaceResourceEntry> CollectWorkspaceGameResources(const std::filesystem::path& gameRoot);
void EnsureProjectDevConfig(const std::filesystem::path& gameRoot);

} // namespace ri::editor
