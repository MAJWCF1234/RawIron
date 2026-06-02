#include "EditorWorkspace.h"

#include "RawIron/Content/GameRuntimeSupport.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string_view>

namespace ri::editor {

namespace fs = std::filesystem;

std::string WorkspaceCategoryLabel(const WorkspaceResourceCategory category) {
    switch (category) {
        case WorkspaceResourceCategory::Manifest:
            return "Manifest";
        case WorkspaceResourceCategory::Level:
            return "Level";
        case WorkspaceResourceCategory::Script:
            return "Script";
        case WorkspaceResourceCategory::Test:
            return "Test";
        case WorkspaceResourceCategory::UiScreen:
            return "Screen / UI";
        case WorkspaceResourceCategory::Menu:
            return "Menu";
        case WorkspaceResourceCategory::Asset:
            return "Asset";
        case WorkspaceResourceCategory::Other:
            return "Other";
    }
    return "Other";
}

std::uint32_t WorkspaceCategoryBit(const WorkspaceResourceCategory category) {
    switch (category) {
        case WorkspaceResourceCategory::Manifest:
            return 1u << 0u;
        case WorkspaceResourceCategory::Level:
            return 1u << 1u;
        case WorkspaceResourceCategory::Script:
            return 1u << 2u;
        case WorkspaceResourceCategory::Test:
            return 1u << 3u;
        case WorkspaceResourceCategory::UiScreen:
            return 1u << 4u;
        case WorkspaceResourceCategory::Menu:
            return 1u << 5u;
        case WorkspaceResourceCategory::Asset:
            return 1u << 6u;
        case WorkspaceResourceCategory::Other:
            return 1u << 7u;
    }
    return 0u;
}

std::string WorkspaceCategoryShortLabel(const WorkspaceResourceCategory category) {
    switch (category) {
        case WorkspaceResourceCategory::Manifest:
            return "Manifest";
        case WorkspaceResourceCategory::Level:
            return "Levels";
        case WorkspaceResourceCategory::Script:
            return "Scripts";
        case WorkspaceResourceCategory::Test:
            return "Tests";
        case WorkspaceResourceCategory::UiScreen:
            return "UI";
        case WorkspaceResourceCategory::Menu:
            return "Menus";
        case WorkspaceResourceCategory::Asset:
            return "Assets";
        case WorkspaceResourceCategory::Other:
            return "Other";
    }
    return "Other";
}

WorkspaceResourceCategory ClassifyRelativeGamePath(const fs::path& relativePath) {
    const fs::path norm = relativePath.lexically_normal();
    if (norm.filename() == "manifest.json") {
        return WorkspaceResourceCategory::Manifest;
    }
    if (norm.begin() == norm.end()) {
        return WorkspaceResourceCategory::Other;
    }
    fs::path firstComponent = *norm.begin();
    std::string low = firstComponent.string();
    for (char& ch : low) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    if (low == "levels") {
        return WorkspaceResourceCategory::Level;
    }
    if (low == "scripts") {
        return WorkspaceResourceCategory::Script;
    }
    if (low == "tests") {
        return WorkspaceResourceCategory::Test;
    }
    if (low == "assets") {
        return WorkspaceResourceCategory::Asset;
    }
    if (low == "ui" || low == "screens") {
        return WorkspaceResourceCategory::UiScreen;
    }
    if (low == "menus") {
        return WorkspaceResourceCategory::Menu;
    }
    return WorkspaceResourceCategory::Other;
}

bool IsLikelyTextResourcePath(const fs::path& path) {
    std::string ext = path.extension().string();
    for (char& ch : ext) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    static constexpr const char* kTextExt[] = {".csv",  ".riscript", ".json", ".md",    ".txt", ".xml",
                                               ".yaml", ".yml",      ".ini",  ".hlsl", ".glsl", ".ripalette",
                                               ".css"};
    for (const char* e : kTextExt) {
        if (ext == e) {
            return true;
        }
    }
    return false;
}

namespace {

void CollectResourcesUnderTree(const fs::path& gameRoot,
                               const fs::path& subdir,
                               const WorkspaceResourceCategory forcedCategory,
                               std::vector<WorkspaceResourceEntry>& out) {
    std::error_code ec{};
    const fs::path base = gameRoot / subdir;
    if (!fs::exists(base, ec)) {
        return;
    }
    const fs::directory_options opts = fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator end{};
    for (fs::recursive_directory_iterator it(base, opts, ec); !ec && it != end; ++it) {
        if (!it->is_regular_file()) {
            continue;
        }
        const fs::path abs = it->path();
        std::error_code relEc{};
        const fs::path rel = fs::relative(abs, gameRoot, relEc);
        if (relEc) {
            continue;
        }
        const WorkspaceResourceCategory cat = (forcedCategory == WorkspaceResourceCategory::Other)
            ? ClassifyRelativeGamePath(rel)
            : forcedCategory;
        out.push_back(WorkspaceResourceEntry{
            .absolutePath = abs,
            .relativePathUtf8 = rel.generic_string(),
            .category = cat,
        });
    }
}

} // namespace

std::vector<WorkspaceGameEntry> EnumerateWorkspaceGames(const fs::path& workspaceRoot) {
    std::vector<WorkspaceGameEntry> games;
    std::error_code ec{};
    const fs::path gamesRoot = workspaceRoot / "Games";
    if (!fs::exists(gamesRoot, ec)) {
        return games;
    }
    for (const fs::directory_entry& entry : fs::directory_iterator(gamesRoot, ec)) {
        if (!entry.is_directory()) {
            continue;
        }
        const fs::path manifestPath = entry.path() / "manifest.json";
        if (!fs::exists(manifestPath, ec)) {
            continue;
        }
        const std::optional<ri::content::GameManifest> manifest = ri::content::LoadGameManifest(manifestPath);
        if (!manifest.has_value()) {
            continue;
        }
        games.push_back(WorkspaceGameEntry{
            .id = manifest->id,
            .displayName = manifest->name.empty() ? manifest->id : manifest->name,
            .rootPath = manifest->rootPath,
        });
    }
    std::sort(games.begin(), games.end(), [](const WorkspaceGameEntry& a, const WorkspaceGameEntry& b) {
        return a.displayName < b.displayName;
    });
    return games;
}

std::vector<WorkspaceResourceEntry> CollectWorkspaceGameResources(const fs::path& gameRoot) {
    std::vector<WorkspaceResourceEntry> resources;
    const ri::content::GameRuntimeSupportData supportData = ri::content::LoadGameRuntimeSupportData(gameRoot);
    std::error_code ec{};
    const fs::path manifestPath = gameRoot / "manifest.json";
    if (fs::exists(manifestPath, ec)) {
        const fs::path rel = fs::relative(manifestPath, gameRoot, ec);
        resources.push_back(WorkspaceResourceEntry{
            .absolutePath = manifestPath,
            .relativePathUtf8 = rel.generic_string(),
            .category = WorkspaceResourceCategory::Manifest,
        });
    }
    CollectResourcesUnderTree(gameRoot, "levels", WorkspaceResourceCategory::Level, resources);
    CollectResourcesUnderTree(gameRoot, "scripts", WorkspaceResourceCategory::Script, resources);
    CollectResourcesUnderTree(gameRoot, "tests", WorkspaceResourceCategory::Test, resources);
    CollectResourcesUnderTree(gameRoot, "assets", WorkspaceResourceCategory::Asset, resources);
    CollectResourcesUnderTree(gameRoot, "ui", WorkspaceResourceCategory::UiScreen, resources);
    CollectResourcesUnderTree(gameRoot, "screens", WorkspaceResourceCategory::UiScreen, resources);
    CollectResourcesUnderTree(gameRoot, "menus", WorkspaceResourceCategory::Menu, resources);
    CollectResourcesUnderTree(gameRoot, "config", WorkspaceResourceCategory::Other, resources);
    CollectResourcesUnderTree(gameRoot, "data", WorkspaceResourceCategory::Other, resources);
    CollectResourcesUnderTree(gameRoot, "plugins", WorkspaceResourceCategory::Other, resources);
    CollectResourcesUnderTree(gameRoot, "ai", WorkspaceResourceCategory::Other, resources);

    std::sort(resources.begin(), resources.end(), [&supportData](const WorkspaceResourceEntry& a, const WorkspaceResourceEntry& b) {
        const int aScore = ri::content::ComputeResourcePriorityScore(supportData, a.relativePathUtf8);
        const int bScore = ri::content::ComputeResourcePriorityScore(supportData, b.relativePathUtf8);
        if (aScore != bScore) {
            return aScore > bScore;
        }
        return a.relativePathUtf8 < b.relativePathUtf8;
    });
    resources.erase(std::unique(resources.begin(),
                                resources.end(),
                                [](const WorkspaceResourceEntry& a, const WorkspaceResourceEntry& b) {
                                    return a.absolutePath == b.absolutePath;
                                }),
                    resources.end());
    return resources;
}

void EnsureProjectDevConfig(const fs::path& gameRoot) {
    if (gameRoot.empty()) {
        return;
    }
    std::error_code ec{};
    const fs::path configDir = gameRoot / "config";
    fs::create_directories(configDir, ec);
    if (ec) {
        return;
    }
    const fs::path projectDevPath = configDir / "project.dev";
    if (fs::exists(projectDevPath, ec)) {
        return;
    }
    std::ofstream stream(projectDevPath, std::ios::out | std::ios::trunc);
    if (!stream.is_open()) {
        return;
    }
    stream << "# RawIron Project Dev Config v1\n"
           << "last_opened_profile=1\n"
           << "local_debug_overlay=0\n";
}

} // namespace ri::editor
