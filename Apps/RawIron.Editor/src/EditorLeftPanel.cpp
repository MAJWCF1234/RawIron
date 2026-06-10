#include "EditorLeftPanel.h"

#include "EditorCreatorPalette.h"
#include "EditorRenderer.h"

#include <algorithm>
#include <array>

namespace ri::editor {

namespace {

[[nodiscard]] int ComputeNodeDepth(const ri::scene::Scene& scene, int nodeIndex) {
    int depth = 0;
    int parent = scene.GetNode(nodeIndex).parent;
    while (parent != ri::scene::kInvalidHandle) {
        ++depth;
        parent = scene.GetNode(parent).parent;
    }
    return depth;
}

[[nodiscard]] std::string NodeKindLabel(const ri::scene::Node& node) {
    if (node.camera != ri::scene::kInvalidHandle) {
        return "Camera";
    }
    if (node.light != ri::scene::kInvalidHandle) {
        return "Light";
    }
    if (node.mesh != ri::scene::kInvalidHandle) {
        return "Mesh";
    }
    return "Transform";
}

} // namespace

ResourceCategoryChipLayout ComputeResourceCategoryChipLayout(const RECT& hierarchyInner) {
    ResourceCategoryChipLayout layout{};
    const int innerWidth = std::max(40, static_cast<int>(hierarchyInner.right - hierarchyInner.left - 12));
    constexpr int kMinChipWidth = 44;
    constexpr int kChipGap = 4;
    layout.chipsPerRow =
        std::max(1, (innerWidth + kChipGap) / (kMinChipWidth + kChipGap));
    layout.chipsPerRow = std::min(layout.chipsPerRow, kResourceCategoryChipCount);
    layout.chipWidth =
        std::max(kMinChipWidth, (innerWidth - (layout.chipsPerRow - 1) * kChipGap) / layout.chipsPerRow);
    layout.rowCount = (kResourceCategoryChipCount + layout.chipsPerRow - 1) / layout.chipsPerRow;
    layout.stripHeight = layout.rowCount * kResourceFilterStripHeight + (layout.rowCount - 1) * kChipGap;
    return layout;
}

int LeftPanelContentTop(const RECT& hierarchyInner, const EditorLeftPanelMode mode) {
    int top = hierarchyInner.top + 6 + kLeftPanelTabHeight;
    if (mode == EditorLeftPanelMode::Create) {
        return top + 8;
    }
    if (mode == EditorLeftPanelMode::Resources) {
        top += kLeftPanelGameStripHeight + 4;
        top += ComputeResourceCategoryChipLayout(hierarchyInner).stripHeight + 6;
        top += 24 + 6;
    } else {
        top += 24 + 6;
    }
    return top;
}

RECT SceneTabRect(const RECT& hierarchyInner) {
    return RECT{
        hierarchyInner.left + 6,
        hierarchyInner.top + 4,
        hierarchyInner.left + 68,
        hierarchyInner.top + 4 + kLeftPanelTabHeight,
    };
}

RECT ResourcesTabRect(const RECT& hierarchyInner) {
    return RECT{
        hierarchyInner.left + 140,
        hierarchyInner.top + 4,
        hierarchyInner.left + 248,
        hierarchyInner.top + 4 + kLeftPanelTabHeight,
    };
}

RECT SceneSearchBoxRect(const RECT& hierarchyInner) {
    const int tabStripBottom = hierarchyInner.top + 4 + kLeftPanelTabHeight;
    return RECT{
        hierarchyInner.left + 6,
        tabStripBottom + 4,
        hierarchyInner.right - 40,
        tabStripBottom + 26,
    };
}

RECT SceneSearchClearRect(const RECT& hierarchyInner) {
    const RECT searchRect = SceneSearchBoxRect(hierarchyInner);
    return RECT{searchRect.right + 4, searchRect.top, searchRect.right + 30, searchRect.bottom};
}

int LeftPanelSceneListBottom(const RECT& hierarchyInner) {
    return hierarchyInner.bottom - kHierarchyBottomGutter;
}

RECT ResourceSearchBoxRect(const RECT& hierarchyInner) {
    const int tabStripBottom = hierarchyInner.top + 4 + kLeftPanelTabHeight;
    const int filterTop = tabStripBottom + 4 + kLeftPanelGameStripHeight + 4;
    const ResourceCategoryChipLayout chipLayout = ComputeResourceCategoryChipLayout(hierarchyInner);
    return RECT{
        hierarchyInner.left + 6,
        filterTop + chipLayout.stripHeight + 4,
        hierarchyInner.right - 40,
        filterTop + chipLayout.stripHeight + 26,
    };
}

RECT ResourceSearchClearRect(const RECT& hierarchyInner) {
    const RECT searchRect = ResourceSearchBoxRect(hierarchyInner);
    return RECT{searchRect.right + 4, searchRect.top, searchRect.right + 30, searchRect.bottom};
}

RECT FocusedGamePrevRect(const RECT& hierarchyInner) {
    const int tabStripBottom = hierarchyInner.top + 4 + kLeftPanelTabHeight;
    return RECT{
        hierarchyInner.left + 6,
        tabStripBottom + 4,
        hierarchyInner.left + 34,
        tabStripBottom + 4 + kLeftPanelGameStripHeight,
    };
}

RECT FocusedGameNextRect(const RECT& hierarchyInner) {
    const int tabStripBottom = hierarchyInner.top + 4 + kLeftPanelTabHeight;
    return RECT{
        hierarchyInner.right - 34,
        tabStripBottom + 4,
        hierarchyInner.right - 6,
        tabStripBottom + 4 + kLeftPanelGameStripHeight,
    };
}

RECT ResourceCategoryChipRect(const RECT& hierarchyInner,
                              const int chipIndex,
                              const ResourceCategoryChipLayout& layout) {
    const int tabStripBottom = hierarchyInner.top + 4 + kLeftPanelTabHeight;
    const int filterTop = tabStripBottom + 4 + kLeftPanelGameStripHeight + 4;
    const int row = chipIndex / layout.chipsPerRow;
    const int col = chipIndex % layout.chipsPerRow;
    const int left = hierarchyInner.left + 6 + col * (layout.chipWidth + 4);
    const int top = filterTop + row * (kResourceFilterStripHeight + 4);
    return RECT{left, top, left + layout.chipWidth, top + kResourceFilterStripHeight};
}

int CountVisibleSceneRows(const RECT& hierarchyInner) {
    const int h = std::max(0, LeftPanelSceneListBottom(hierarchyInner) -
                                  LeftPanelContentTop(hierarchyInner, EditorLeftPanelMode::Scene) - 8);
    return std::max(1, h / kHierarchyRowHeight);
}

int CountVisibleResourceRows(const RECT& hierarchyInner) {
    const int h = std::max(0, LeftPanelSceneListBottom(hierarchyInner) -
                                  LeftPanelContentTop(hierarchyInner, EditorLeftPanelMode::Resources) - 8);
    return std::max(1, h / kResourceListRowHeight);
}

int HitTestSceneRow(const RECT& hierarchyInner, const int y, const int scrollTopRow) {
    const int listTop = LeftPanelContentTop(hierarchyInner, EditorLeftPanelMode::Scene);
    const int listBottom = LeftPanelSceneListBottom(hierarchyInner);
    const int listHeight = std::max(0, listBottom - listTop - 8);
    const int relativeY = y - listTop;
    if (relativeY < 0 || relativeY >= listHeight) {
        return -1;
    }
    return scrollTopRow + (relativeY / kHierarchyRowHeight);
}

int HitTestResourceRow(const RECT& hierarchyInner, const int y, const int scrollTopRow) {
    const int listTop = LeftPanelContentTop(hierarchyInner, EditorLeftPanelMode::Resources);
    const int listBottom = LeftPanelSceneListBottom(hierarchyInner);
    const int listHeight = std::max(0, listBottom - listTop - 8);
    const int relativeY = y - listTop;
    if (relativeY < 0 || relativeY >= listHeight) {
        return -1;
    }
    return scrollTopRow + (relativeY / kResourceListRowHeight);
}

#if defined(_WIN32)
void RenderEditorLeftPanel(HDC dc,
                           const RECT& hierarchyInner,
                           const EditorLeftPanelPaintModel& model,
                           const EditorLeftPanelTheme& theme) {
    EditorRenderer::DrawToolbarButton(
        dc, SceneTabRect(hierarchyInner), "Scene", model.mode == EditorLeftPanelMode::Scene, theme.smallFont);
    EditorRenderer::DrawToolbarButton(
        dc, CreateTabRect(hierarchyInner), "Create", model.mode == EditorLeftPanelMode::Create, theme.smallFont);
    EditorRenderer::DrawToolbarButton(
        dc, ResourcesTabRect(hierarchyInner), "Resources", model.mode == EditorLeftPanelMode::Resources, theme.smallFont);

    const int tabBottom = hierarchyInner.top + 4 + kLeftPanelTabHeight;
    if (model.mode == EditorLeftPanelMode::Resources
        && model.workspaceGames != nullptr
        && !model.workspaceGames->empty()
        && model.focusedWorkspaceGameIndex >= 0
        && model.focusedWorkspaceGameIndex < static_cast<int>(model.workspaceGames->size())) {
        const WorkspaceGameEntry& focus =
            (*model.workspaceGames)[static_cast<std::size_t>(model.focusedWorkspaceGameIndex)];
        EditorRenderer::DrawToolbarButton(dc, FocusedGamePrevRect(hierarchyInner), "<", false, theme.smallFont);
        EditorRenderer::DrawToolbarButton(dc, FocusedGameNextRect(hierarchyInner), ">", false, theme.smallFont);
        EditorRenderer::DrawTextLine(dc,
                                     RECT{hierarchyInner.left + 40,
                                          tabBottom + 6,
                                          hierarchyInner.right - 40,
                                          tabBottom + kLeftPanelGameStripHeight},
                                     focus.displayName,
                                     RGB(255, 221, 154),
                                     theme.bodyFont,
                                     DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }

    if (model.mode == EditorLeftPanelMode::Create) {
        return;
    }

    if (model.mode == EditorLeftPanelMode::Resources) {
        const std::array<WorkspaceResourceCategory, 8> categories = {
            WorkspaceResourceCategory::Manifest,
            WorkspaceResourceCategory::Level,
            WorkspaceResourceCategory::Script,
            WorkspaceResourceCategory::Test,
            WorkspaceResourceCategory::UiScreen,
            WorkspaceResourceCategory::Menu,
            WorkspaceResourceCategory::Asset,
            WorkspaceResourceCategory::Other,
        };
        const ResourceCategoryChipLayout chipLayout = ComputeResourceCategoryChipLayout(hierarchyInner);
        for (int index = 0; index < static_cast<int>(categories.size()); ++index) {
            const WorkspaceResourceCategory category = categories[static_cast<std::size_t>(index)];
            const bool active = (model.resourceCategoryMask & WorkspaceCategoryBit(category)) != 0u;
            EditorRenderer::DrawToolbarButton(dc,
                                              ResourceCategoryChipRect(hierarchyInner, index, chipLayout),
                                              WorkspaceCategoryShortLabel(category),
                                              active,
                                              theme.smallFont);
        }

        const RECT searchRect = ResourceSearchBoxRect(hierarchyInner);
        const RECT clearRect = ResourceSearchClearRect(hierarchyInner);
        EditorRenderer::DrawInsetFrame(dc,
                                       searchRect,
                                       model.searchActive ? RGB(36, 54, 88) : RGB(62, 68, 78),
                                       model.searchActive ? RGB(220, 230, 252) : RGB(162, 168, 178),
                                       RGB(22, 24, 30));
        const std::string shownFilter =
            model.searchQuery.empty() ? std::string("Find project files... (Ctrl+F)") : model.searchQuery;
        EditorRenderer::DrawTextLine(dc,
                                     RECT{searchRect.left + 8, searchRect.top + 2, searchRect.right - 8, searchRect.bottom - 2},
                                     shownFilter,
                                     model.searchQuery.empty() ? RGB(188, 196, 208) : RGB(244, 248, 255),
                                     theme.smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        EditorRenderer::DrawToolbarButton(dc, clearRect, "x", !model.searchQuery.empty(), theme.smallFont);
    } else if (model.mode == EditorLeftPanelMode::Scene) {
        const RECT searchRect = SceneSearchBoxRect(hierarchyInner);
        const RECT clearRect = SceneSearchClearRect(hierarchyInner);
        EditorRenderer::DrawInsetFrame(dc,
                                       searchRect,
                                       model.searchActive ? RGB(36, 54, 88) : RGB(62, 68, 78),
                                       model.searchActive ? RGB(220, 230, 252) : RGB(162, 168, 178),
                                       RGB(22, 24, 30));
        const std::string shownFilter =
            model.searchQuery.empty() ? std::string("Filter hierarchy... (Ctrl+F)") : model.searchQuery;
        EditorRenderer::DrawTextLine(dc,
                                     RECT{searchRect.left + 8, searchRect.top + 2, searchRect.right - 8, searchRect.bottom - 2},
                                     shownFilter,
                                     model.searchQuery.empty() ? RGB(188, 196, 208) : RGB(244, 248, 255),
                                     theme.smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        EditorRenderer::DrawToolbarButton(dc, clearRect, "x", !model.searchQuery.empty(), theme.smallFont);
    }

    const int listTop = LeftPanelContentTop(hierarchyInner, model.mode);
    const int listBottom = LeftPanelSceneListBottom(hierarchyInner);
    const int listPixels = std::max(0, listBottom - listTop - 8);

    if (model.mode == EditorLeftPanelMode::Scene && model.scene != nullptr && model.hierarchyOrder != nullptr) {
        const auto& nodes = model.scene->Nodes();
        const int visibleHierarchyRows = std::max(0, listPixels / kHierarchyRowHeight);
        const int maxHierarchyScroll =
            std::max(0, static_cast<int>(model.hierarchyOrder->size()) - visibleHierarchyRows);
        const int hierarchyScrollTopRow = std::clamp(model.hierarchyScrollTopRow, 0, maxHierarchyScroll);

        if (!model.hierarchyOrder->empty() && visibleHierarchyRows > 0 && maxHierarchyScroll > 0) {
            const std::string scrollHint =
                "Rows " + std::to_string(hierarchyScrollTopRow + 1) + "-" +
                std::to_string(std::min(static_cast<int>(model.hierarchyOrder->size()),
                                        hierarchyScrollTopRow + visibleHierarchyRows)) +
                " of " + std::to_string(model.hierarchyOrder->size()) + "  |  wheel / PgUp PgDn";
            EditorRenderer::DrawTextLine(dc,
                                         RECT{hierarchyInner.left + 6,
                                              hierarchyInner.bottom - 22,
                                              hierarchyInner.right - 6,
                                              hierarchyInner.bottom - 4},
                                         scrollHint,
                                         RGB(36, 38, 42),
                                         theme.smallFont,
                                         DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }

        int y = listTop;
        for (int row = 0; row < visibleHierarchyRows; ++row) {
            const int orderIndex = hierarchyScrollTopRow + row;
            if (orderIndex >= static_cast<int>(model.hierarchyOrder->size())) {
                break;
            }
            const int nodeIndex = (*model.hierarchyOrder)[static_cast<std::size_t>(orderIndex)];
            if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= nodes.size()) {
                continue;
            }
            const ri::scene::Node& node = nodes[static_cast<std::size_t>(nodeIndex)];
            const int depth = ComputeNodeDepth(*model.scene, nodeIndex);
            const int indent = 8 + depth * 14;
            RECT rowRect{hierarchyInner.left + 6, y, hierarchyInner.right - 6, y + kHierarchyRowHeight};
            if (static_cast<std::size_t>(nodeIndex) == model.selectedNode) {
                EditorRenderer::FillRectColor(dc, rowRect, RGB(124, 89, 40));
            }
            EditorRenderer::DrawTextLine(dc,
                                         RECT{rowRect.left + indent, rowRect.top, rowRect.right - 90, rowRect.bottom},
                                         std::to_string(nodeIndex) + "  " + node.name,
                                         static_cast<std::size_t>(nodeIndex) == model.selectedNode
                                             ? RGB(255, 246, 214)
                                             : RGB(236, 240, 244),
                                         theme.bodyFont,
                                         DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            EditorRenderer::DrawTextLine(dc,
                                         RECT{rowRect.right - 84, rowRect.top, rowRect.right - 8, rowRect.bottom},
                                         NodeKindLabel(node),
                                         static_cast<std::size_t>(nodeIndex) == model.selectedNode
                                             ? RGB(255, 231, 182)
                                             : RGB(186, 194, 204),
                                         theme.smallFont,
                                         DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
            y += kHierarchyRowHeight;
        }
        return;
    }

    const int visibleResourceRows = std::max(0, listPixels / kResourceListRowHeight);
    const int filteredCount = model.filteredResourceRows == nullptr ? 0 : static_cast<int>(model.filteredResourceRows->size());
    const int maxResourceScroll = std::max(0, filteredCount - visibleResourceRows);
    const int resourceCatalogScrollTopRow = std::clamp(model.resourceCatalogScrollTopRow, 0, maxResourceScroll);

    if (filteredCount > 0 && visibleResourceRows > 0 && maxResourceScroll > 0) {
        const std::string scrollHint =
            "Rows " + std::to_string(resourceCatalogScrollTopRow + 1) + "-" +
            std::to_string(std::min(filteredCount, resourceCatalogScrollTopRow + visibleResourceRows)) +
            " of " + std::to_string(filteredCount) + " (filtered)";
        EditorRenderer::DrawTextLine(dc,
                                     RECT{hierarchyInner.left + 6,
                                          hierarchyInner.bottom - 22,
                                          hierarchyInner.right - 6,
                                          hierarchyInner.bottom - 4},
                                     scrollHint,
                                     RGB(36, 38, 42),
                                     theme.smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }

    if (model.workspaceGames == nullptr || model.workspaceGames->empty()) {
        EditorRenderer::DrawTextLine(dc,
                                     RECT{hierarchyInner.left + 8, listTop, hierarchyInner.right - 8, listTop + 48},
                                     "No workspace game is mounted. Launch with --game=<id> or open a registered project.",
                                     RGB(180, 90, 90),
                                     theme.smallFont,
                                     DT_LEFT | DT_WORDBREAK);
        return;
    }

    if (model.filteredResourceRows == nullptr || model.resourceCatalogEntries == nullptr) {
        return;
    }

    int y = listTop;
    for (int row = 0; row < visibleResourceRows; ++row) {
        const int visibleIdx = resourceCatalogScrollTopRow + row;
        if (visibleIdx >= static_cast<int>(model.filteredResourceRows->size())) {
            break;
        }
        const int idx = (*model.filteredResourceRows)[static_cast<std::size_t>(visibleIdx)];
        const WorkspaceResourceEntry& entry = (*model.resourceCatalogEntries)[static_cast<std::size_t>(idx)];
        RECT rowRect{hierarchyInner.left + 6, y, hierarchyInner.right - 6, y + kResourceListRowHeight};
        if (visibleIdx == model.selectedResourceVisibleRow) {
            EditorRenderer::FillRectColor(dc, rowRect, RGB(64, 84, 118));
        }
        EditorRenderer::DrawTextLine(dc,
                                     RECT{rowRect.left + 6, rowRect.top, rowRect.right - 110, rowRect.bottom},
                                     entry.relativePathUtf8,
                                     visibleIdx == model.selectedResourceVisibleRow ? RGB(255, 247, 220) : RGB(236, 240, 244),
                                     theme.smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        EditorRenderer::DrawTextLine(dc,
                                     RECT{rowRect.right - 102, rowRect.top, rowRect.right - 8, rowRect.bottom},
                                     WorkspaceCategoryLabel(entry.category),
                                     visibleIdx == model.selectedResourceVisibleRow ? RGB(214, 228, 248) : RGB(186, 194, 204),
                                     theme.smallFont,
                                     DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        y += kResourceListRowHeight;
    }
}
#endif

} // namespace ri::editor
