#pragma once

#include "RawIron/Content/GameManifest.h"

#include <cstdint>
#include <filesystem>
#include <future>
#include <optional>
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

/// Serial background resource scanner. New requests supersede older results
/// without launching overlapping recursive directory walks.
class WorkspaceResourceIndex {
public:
    void Request(std::filesystem::path gameRoot);
    [[nodiscard]] std::optional<std::vector<WorkspaceResourceEntry>> Poll();
    [[nodiscard]] bool Busy() const noexcept;

private:
    struct ScanResult {
        std::uint64_t generation = 0;
        std::vector<WorkspaceResourceEntry> entries;
    };

    void StartRequestedScan();

    std::future<ScanResult> activeScan_;
    std::filesystem::path requestedRoot_;
    std::uint64_t requestedGeneration_ = 0;
    std::uint64_t activeGeneration_ = 0;
};

void EnsureProjectDevConfig(const std::filesystem::path& gameRoot);

} // namespace ri::editor
