#pragma once

#include "EditorWorkspace.h"
#include "RawIron/Scene/Scene.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace ri::editor {

inline constexpr int kHierarchyRowHeight = 25;
inline constexpr int kHierarchyBottomGutter = 26;
inline constexpr int kLeftPanelTabHeight = 24;
inline constexpr int kLeftPanelGameStripHeight = 28;
inline constexpr int kResourceFilterStripHeight = 26;
inline constexpr int kResourceListRowHeight = 22;
inline constexpr int kResourceCategoryChipCount = 8;

struct ResourceCategoryChipLayout {
    int chipsPerRow = 4;
    int chipWidth = 56;
    int rowCount = 2;
    int stripHeight = 26;
};

[[nodiscard]] ResourceCategoryChipLayout ComputeResourceCategoryChipLayout(const RECT& hierarchyInner);

enum class EditorLeftPanelMode {
    Scene,
    Create,
    Resources,
};

struct EditorLeftPanelTheme {
#if defined(_WIN32)
    HFONT bodyFont = nullptr;
    HFONT smallFont = nullptr;
#endif
};

struct EditorLeftPanelPaintModel {
    EditorLeftPanelMode mode = EditorLeftPanelMode::Scene;
    bool searchActive = false;
    std::string searchQuery;

    const ri::scene::Scene* scene = nullptr;
    const std::vector<int>* hierarchyOrder = nullptr;
    std::size_t selectedNode = 0;
    int hierarchyScrollTopRow = 0;

    const std::vector<WorkspaceGameEntry>* workspaceGames = nullptr;
    int focusedWorkspaceGameIndex = 0;
    std::uint32_t resourceCategoryMask = 0;
    const std::vector<int>* filteredResourceRows = nullptr;
    const std::vector<WorkspaceResourceEntry>* resourceCatalogEntries = nullptr;
    int selectedResourceVisibleRow = -1;
    int resourceCatalogScrollTopRow = 0;
};

[[nodiscard]] int LeftPanelContentTop(const RECT& hierarchyInner, EditorLeftPanelMode mode);
[[nodiscard]] RECT SceneTabRect(const RECT& hierarchyInner);
[[nodiscard]] RECT ResourcesTabRect(const RECT& hierarchyInner);
[[nodiscard]] RECT SceneSearchBoxRect(const RECT& hierarchyInner);
[[nodiscard]] RECT SceneSearchClearRect(const RECT& hierarchyInner);
[[nodiscard]] int LeftPanelSceneListBottom(const RECT& hierarchyInner);
[[nodiscard]] RECT ResourceSearchBoxRect(const RECT& hierarchyInner);
[[nodiscard]] RECT ResourceSearchClearRect(const RECT& hierarchyInner);
[[nodiscard]] RECT FocusedGamePrevRect(const RECT& hierarchyInner);
[[nodiscard]] RECT FocusedGameNextRect(const RECT& hierarchyInner);
[[nodiscard]] RECT ResourceCategoryChipRect(const RECT& hierarchyInner,
                                              int chipIndex,
                                              const ResourceCategoryChipLayout& layout);
[[nodiscard]] int CountVisibleSceneRows(const RECT& hierarchyInner);
[[nodiscard]] int CountVisibleResourceRows(const RECT& hierarchyInner);
[[nodiscard]] int HitTestSceneRow(const RECT& hierarchyInner, int y, int scrollTopRow);
[[nodiscard]] int HitTestResourceRow(const RECT& hierarchyInner, int y, int scrollTopRow);

#if defined(_WIN32)
void RenderEditorLeftPanel(HDC dc,
                           const RECT& hierarchyInner,
                           const EditorLeftPanelPaintModel& model,
                           const EditorLeftPanelTheme& theme);
#endif

} // namespace ri::editor
