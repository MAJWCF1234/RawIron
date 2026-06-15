#include "EditorInput.h"

#include "EditorCreatorPalette.h"
#include "EditorFilesInspector.h"
#include "EditorLeftPanel.h"
#include "EditorViewportRenderer.h"

#include <array>
#include <algorithm>
#include <cstdlib>

namespace ri::editor {

EditorTopChromeHit HitTestEditorTopChrome(const RECT& clientRect, const POINT& point) {
    const auto hitRect = [&point](const RECT& rect) {
        return PtInRect(&rect, point) != FALSE;
    };
    const RECT topBar{0, 0, clientRect.right, 56};
    const TopChromeRects topChrome = ComputeTopChromeRects(topBar);
    if (hitRect(topChrome.newGame)) return EditorTopChromeHit::NewGame;
    if (hitRect(topChrome.save)) return EditorTopChromeHit::Save;
    if (hitRect(topChrome.scaffold)) return EditorTopChromeHit::Scaffold;
    if (hitRect(topChrome.exportScene)) return EditorTopChromeHit::ExportScene;
    if (hitRect(topChrome.play)) return EditorTopChromeHit::Play;
    if (hitRect(topChrome.files)) return EditorTopChromeHit::Files;
    return EditorTopChromeHit::None;
}

EditorToolbarHit HitTestEditorToolbar(const RECT& toolStrip, const POINT& point) {
    const auto hitRect = [&point](const RECT& rect) {
        return PtInRect(&rect, point) != FALSE;
    };

    const EditorToolbarLayout layout = ComputeEditorToolbarLayout(toolStrip);
    const AuthoringToolbarRects& authoringTools = layout.authoring;

    if (hitRect(layout.select)) return EditorToolbarHit::Select;
    if (hitRect(layout.create)) return EditorToolbarHit::Create;
    if (hitRect(layout.camera)) return EditorToolbarHit::Camera;
    if (hitRect(layout.translate)) return EditorToolbarHit::Translate;
    if (hitRect(layout.rotate)) return EditorToolbarHit::Rotate;
    if (hitRect(layout.scale)) return EditorToolbarHit::Scale;
    if (hitRect(layout.axisX)) return EditorToolbarHit::AxisX;
    if (hitRect(layout.axisY)) return EditorToolbarHit::AxisY;
    if (hitRect(layout.axisZ)) return EditorToolbarHit::AxisZ;
    if (hitRect(layout.snapToggle)) return EditorToolbarHit::SnapToggle;
    if (hitRect(layout.snapStepDown)) return EditorToolbarHit::SnapStepDown;
    if (hitRect(layout.snapStepUp)) return EditorToolbarHit::SnapStepUp;
    if (hitRect(layout.resolutionScale)) return EditorToolbarHit::ResolutionScale;
    if (hitRect(authoringTools.addCube)) return EditorToolbarHit::AddCube;
    if (hitRect(authoringTools.addPlane)) return EditorToolbarHit::AddPlane;
    if (hitRect(authoringTools.addTrigger)) return EditorToolbarHit::AddTrigger;
    if (hitRect(authoringTools.addLight)) return EditorToolbarHit::AddLight;
    if (hitRect(authoringTools.duplicate)) return EditorToolbarHit::Duplicate;
    if (hitRect(authoringTools.exportCsv)) return EditorToolbarHit::ExportCsv;
    if (hitRect(authoringTools.play)) return EditorToolbarHit::Play;
    return EditorToolbarHit::None;
}

EditorLeftPanelHit HitTestEditorLeftPanel(const RECT& hierarchyInner,
                                          const EditorLeftPanelMode mode,
                                          const bool hasWorkspaceGames,
                                          const POINT& point,
                                          const int hierarchyScrollTopRow,
                                          const int resourceCatalogScrollTopRow) {
    const auto hitRect = [&point](const RECT& rect) {
        return PtInRect(&rect, point) != FALSE;
    };

    if (hitRect(SceneTabRect(hierarchyInner))) {
        return {.type = EditorLeftPanelHitType::SceneTab};
    }
    if (hitRect(CreateTabRect(hierarchyInner))) {
        return {.type = EditorLeftPanelHitType::CreateTab};
    }
    if (hitRect(ResourcesTabRect(hierarchyInner))) {
        return {.type = EditorLeftPanelHitType::ResourcesTab};
    }

    if (mode == EditorLeftPanelMode::Resources && hasWorkspaceGames) {
        if (hitRect(FocusedGamePrevRect(hierarchyInner))) {
            return {.type = EditorLeftPanelHitType::FocusedGamePrev};
        }
        if (hitRect(FocusedGameNextRect(hierarchyInner))) {
            return {.type = EditorLeftPanelHitType::FocusedGameNext};
        }
    }

    if (mode == EditorLeftPanelMode::Create) {
        return {};
    }

    if (mode == EditorLeftPanelMode::Scene) {
        if (hitRect(SceneSearchBoxRect(hierarchyInner))) {
            return {.type = EditorLeftPanelHitType::SceneSearch};
        }
        if (hitRect(SceneSearchClearRect(hierarchyInner))) {
            return {.type = EditorLeftPanelHitType::SceneSearchClear};
        }
    } else {
        static constexpr std::array<WorkspaceResourceCategory, 8> kCategories = {
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
        for (int index = 0; index < static_cast<int>(kCategories.size()); ++index) {
            if (hitRect(ResourceCategoryChipRect(hierarchyInner, index, chipLayout))) {
                return {.type = EditorLeftPanelHitType::ResourceCategoryChip, .index = index};
            }
        }
        if (hitRect(ResourceSearchBoxRect(hierarchyInner))) {
            return {.type = EditorLeftPanelHitType::ResourceSearch};
        }
        if (hitRect(ResourceSearchClearRect(hierarchyInner))) {
            return {.type = EditorLeftPanelHitType::ResourceSearchClear};
        }
    }

    if (PtInRect(&hierarchyInner, point) == FALSE) {
        return {};
    }

    if (mode == EditorLeftPanelMode::Resources) {
        const int row = HitTestResourceRow(hierarchyInner, point.y, resourceCatalogScrollTopRow);
        if (row >= 0) {
            return {.type = EditorLeftPanelHitType::ResourceRow, .index = row};
        }
        return {};
    }

    const int row = HitTestSceneRow(hierarchyInner, point.y, hierarchyScrollTopRow);
    if (row >= 0) {
        return {.type = EditorLeftPanelHitType::SceneRow, .index = row};
    }
    return {};
}

EditorInspectorTabHit HitTestEditorInspectorTabs(const RECT& inspectorInner, const POINT& point) {
    const auto hitRect = [&point](const RECT& rect) {
        return PtInRect(&rect, point) != FALSE;
    };
    const InspectorTabLayout tabs = ComputeInspectorTabLayout(inspectorInner);
    if (hitRect(tabs.nodeTab)) return EditorInspectorTabHit::Node;
    if (hitRect(tabs.brushTab)) return EditorInspectorTabHit::Brush;
    if (hitRect(tabs.gameplayTab)) return EditorInspectorTabHit::Gameplay;
    if (hitRect(tabs.filesTab)) return EditorInspectorTabHit::Files;
    if (hitRect(tabs.storeTab)) return EditorInspectorTabHit::Store;
    if (hitRect(tabs.uiWorkbenchTab)) return EditorInspectorTabHit::UiWorkbench;
    return EditorInspectorTabHit::None;
}

EditorInspectorPanelHit HitTestBrushInspectorPanel(const RECT& inspectorInner, const POINT& point) {
    const auto hitRect = [&point](const RECT& rect) {
        return PtInRect(&rect, point) != FALSE;
    };
    const BrushPanelLayout brushLayout = ComputeBrushPanelLayout(inspectorInner);
    if (hitRect(brushLayout.presetPrevBtn)) return {.type = EditorInspectorPanelHitType::BrushPresetPrev};
    if (hitRect(brushLayout.presetNextBtn)) return {.type = EditorInspectorPanelHitType::BrushPresetNext};
    return {};
}

EditorInspectorPanelHit HitTestFilesInspectorPanel(const RECT& inspectorInner,
                                                   const POINT& point,
                                                   const ProjectShortcutLayout& shortcuts) {
    const auto hitRect = [&point](const RECT& rect) {
        return PtInRect(&rect, point) != FALSE;
    };
    const FilesInspectorLayout filesLayout = ComputeFilesInspectorLayout(inspectorInner);
    if (hitRect(filesLayout.saveBtn)) return {.type = EditorInspectorPanelHitType::SaveResource};
    if (hitRect(filesLayout.explorerBtn)) return {.type = EditorInspectorPanelHitType::Explorer};
    if (hitRect(shortcuts.manifest)) return {.type = EditorInspectorPanelHitType::ShortcutManifest};
    if (hitRect(shortcuts.level)) return {.type = EditorInspectorPanelHitType::ShortcutLevel};
    if (hitRect(shortcuts.gameplay)) return {.type = EditorInspectorPanelHitType::ShortcutGameplay};
    if (hitRect(shortcuts.rendering)) return {.type = EditorInspectorPanelHitType::ShortcutRendering};
    if (hitRect(shortcuts.uiLayout)) return {.type = EditorInspectorPanelHitType::ShortcutUiLayout};
    if (hitRect(shortcuts.uiStyle)) return {.type = EditorInspectorPanelHitType::ShortcutUiStyle};
    if (hitRect(shortcuts.menu)) return {.type = EditorInspectorPanelHitType::ShortcutMenu};
    if (hitRect(shortcuts.ai)) return {.type = EditorInspectorPanelHitType::ShortcutAi};
    if (hitRect(shortcuts.network)) return {.type = EditorInspectorPanelHitType::ShortcutNetwork};
    if (hitRect(shortcuts.plugins)) return {.type = EditorInspectorPanelHitType::ShortcutPlugins};
    return {};
}

EditorInspectorPanelHit HitTestGameplayInspectorPanel(const GameplayPanelLayout& layout, const POINT& point) {
    const auto hitRect = [&point](const RECT& rect) {
        return PtInRect(&rect, point) != FALSE;
    };
    if (hitRect(layout.inventoryModeRow)) return {.type = EditorInspectorPanelHitType::GameplayInventoryMode};
    if (hitRect(layout.offHandRow)) return {.type = EditorInspectorPanelHitType::GameplayOffHand};
    if (hitRect(layout.addTriggerBtn)) return {.type = EditorInspectorPanelHitType::GameplayAddTrigger};
    if (hitRect(layout.exportBtn)) return {.type = EditorInspectorPanelHitType::GameplayExport};
    if (hitRect(layout.playtestBtn)) return {.type = EditorInspectorPanelHitType::GameplayPlaytest};
    return {};
}

EditorInspectorPanelHit HitTestPluginStorePanel(const PluginStoreLayout& layout, const POINT& point) {
    const auto hitRect = [&point](const RECT& rect) {
        return PtInRect(&rect, point) != FALSE;
    };
    if (hitRect(layout.refreshBtn)) {
        return {.type = EditorInspectorPanelHitType::PluginStoreRefresh};
    }
    if (hitRect(layout.openFolderBtn)) {
        return {.type = EditorInspectorPanelHitType::PluginStoreOpenFolder};
    }
    if (hitRect(layout.scrollPrevBtn)) {
        return {.type = EditorInspectorPanelHitType::PluginStoreScrollPrev};
    }
    if (hitRect(layout.scrollNextBtn)) {
        return {.type = EditorInspectorPanelHitType::PluginStoreScrollNext};
    }
    for (std::size_t index = 0; index < layout.actionBtns.size(); ++index) {
        if (hitRect(layout.actionBtns[index])) {
            const int packageIndex =
                index < layout.cardPackageIndices.size() ? layout.cardPackageIndices[index] : static_cast<int>(index);
            return {.type = EditorInspectorPanelHitType::PluginStoreAction, .index = packageIndex};
        }
    }
    for (std::size_t index = 0; index < layout.secondaryActionBtns.size(); ++index) {
        if (hitRect(layout.secondaryActionBtns[index])) {
            const int packageIndex =
                index < layout.cardPackageIndices.size() ? layout.cardPackageIndices[index] : static_cast<int>(index);
            return {.type = EditorInspectorPanelHitType::PluginStoreUninstall, .index = packageIndex};
        }
    }
    return {};
}

EditorInspectorPanelHit HitTestUiWorkbenchInspectorPanel(const UiWorkbenchLayout& layout, const POINT& point) {
    const auto hitRect = [&point](const RECT& rect) {
        return PtInRect(&rect, point) != FALSE;
    };
    if (hitRect(layout.prevScreenBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchPrevScreen};
    if (hitRect(layout.nextScreenBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchNextScreen};
    if (hitRect(layout.useAutoBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchUseAuto};
    if (hitRect(layout.useMenuSampleBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchUseMenuSample};
    if (hitRect(layout.useVnSampleBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchUseVnSample};
    if (hitRect(layout.newScreenBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchNewScreen};
    if (hitRect(layout.newMenuScreenBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchNewMenuScreen};
    if (hitRect(layout.duplicateScreenBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchDuplicateScreen};
    if (hitRect(layout.addButtonBlockBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchAddButtonBlock};
    if (hitRect(layout.addHeadingBlockBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchAddHeadingBlock};
    if (hitRect(layout.addParagraphBlockBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchAddParagraphBlock};
    if (hitRect(layout.addSpacerBlockBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchAddSpacerBlock};
    if (hitRect(layout.addChoiceBlockBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchAddChoiceBlock};
    if (hitRect(layout.setStartScreenBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchSetStartScreen};
    if (hitRect(layout.addDialogueBlockBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchAddDialogueBlock};
    if (hitRect(layout.addNarrationBlockBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchAddNarrationBlock};
    if (hitRect(layout.moveBlockUpBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchMoveBlockUp};
    if (hitRect(layout.moveBlockDownBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchMoveBlockDown};
    if (hitRect(layout.deleteBlockBtn)) return {.type = EditorInspectorPanelHitType::UiWorkbenchDeleteBlock};
    return {};
}

bool DispatchEditorTopChromeClick(const RECT& clientRect,
                                  const POINT& point,
                                  const EditorTopChromeDispatchCallbacks& callbacks) {
    switch (HitTestEditorTopChrome(clientRect, point)) {
        case EditorTopChromeHit::NewGame:
            if (callbacks.onNewGame) callbacks.onNewGame();
            return true;
        case EditorTopChromeHit::Save:
            if (callbacks.onSave) callbacks.onSave();
            return true;
        case EditorTopChromeHit::Scaffold:
            if (callbacks.onScaffold) callbacks.onScaffold();
            return true;
        case EditorTopChromeHit::ExportScene:
            if (callbacks.onExportScene) callbacks.onExportScene();
            return true;
        case EditorTopChromeHit::Play:
            if (callbacks.onPlay) callbacks.onPlay();
            return true;
        case EditorTopChromeHit::Files:
            if (callbacks.onFiles) callbacks.onFiles();
            return true;
        case EditorTopChromeHit::None:
            break;
    }
    return false;
}

bool DispatchEditorToolbarClick(const RECT& toolStrip,
                                const POINT& point,
                                const EditorToolbarDispatchCallbacks& callbacks) {
    switch (HitTestEditorToolbar(toolStrip, point)) {
        case EditorToolbarHit::Select:
            if (callbacks.onSelectMode) callbacks.onSelectMode();
            return true;
        case EditorToolbarHit::Create:
            if (callbacks.onCreateMode) callbacks.onCreateMode();
            return true;
        case EditorToolbarHit::Camera:
            if (callbacks.onCameraMode) callbacks.onCameraMode();
            return true;
        case EditorToolbarHit::Translate:
            if (callbacks.onTranslate) callbacks.onTranslate();
            return true;
        case EditorToolbarHit::Rotate:
            if (callbacks.onRotate) callbacks.onRotate();
            return true;
        case EditorToolbarHit::Scale:
            if (callbacks.onScale) callbacks.onScale();
            return true;
        case EditorToolbarHit::AxisX:
            if (callbacks.onAxisX) callbacks.onAxisX();
            return true;
        case EditorToolbarHit::AxisY:
            if (callbacks.onAxisY) callbacks.onAxisY();
            return true;
        case EditorToolbarHit::AxisZ:
            if (callbacks.onAxisZ) callbacks.onAxisZ();
            return true;
        case EditorToolbarHit::SnapToggle:
            if (callbacks.onSnapToggle) callbacks.onSnapToggle();
            return true;
        case EditorToolbarHit::SnapStepDown:
            if (callbacks.onSnapStepDown) callbacks.onSnapStepDown();
            return true;
        case EditorToolbarHit::SnapStepUp:
            if (callbacks.onSnapStepUp) callbacks.onSnapStepUp();
            return true;
        case EditorToolbarHit::ResolutionScale:
            if (callbacks.onResolutionScaleToggle) callbacks.onResolutionScaleToggle();
            return true;
        case EditorToolbarHit::AddCube:
            if (callbacks.onAddCube) callbacks.onAddCube();
            return true;
        case EditorToolbarHit::AddPlane:
            if (callbacks.onAddPlane) callbacks.onAddPlane();
            return true;
        case EditorToolbarHit::AddTrigger:
            if (callbacks.onAddTrigger) callbacks.onAddTrigger();
            return true;
        case EditorToolbarHit::AddLight:
            if (callbacks.onAddLight) callbacks.onAddLight();
            return true;
        case EditorToolbarHit::Duplicate:
            if (callbacks.onDuplicate) callbacks.onDuplicate();
            return true;
        case EditorToolbarHit::ExportCsv:
            if (callbacks.onExportCsv) callbacks.onExportCsv();
            return true;
        case EditorToolbarHit::Play:
            if (callbacks.onPlay) callbacks.onPlay();
            return true;
        case EditorToolbarHit::None:
            break;
    }
    return false;
}

bool DispatchEditorLeftPanelClick(const EditorLeftPanelDispatchContext& context) {
    const EditorLeftPanelHit leftHit = HitTestEditorLeftPanel(
        context.hierarchyInner,
        context.mode,
        context.hasWorkspaceGames,
        context.point,
        context.hierarchyScrollTopRow,
        context.resourceCatalogScrollTopRow);

    switch (leftHit.type) {
        case EditorLeftPanelHitType::SceneTab:
            if (context.onSceneTab) context.onSceneTab();
            return true;
        case EditorLeftPanelHitType::CreateTab:
            if (context.onCreateTab) context.onCreateTab();
            return true;
        case EditorLeftPanelHitType::ResourcesTab:
            if (context.onResourcesTab) context.onResourcesTab();
            return true;
        case EditorLeftPanelHitType::FocusedGamePrev:
            if (context.workspaceGameCount <= 0) {
                return true;
            }
            if (context.tryResolveDirtyResourceBeforeContextSwitch
                && !context.tryResolveDirtyResourceBeforeContextSwitch("switching games")) {
                if (context.onInvalidate) context.onInvalidate();
                return true;
            }
            if (context.onFocusedWorkspaceGameIndexChanged) {
                const int next =
                    (context.focusedWorkspaceGameIndex - 1 + context.workspaceGameCount) % context.workspaceGameCount;
                context.onFocusedWorkspaceGameIndexChanged(next);
            }
            return true;
        case EditorLeftPanelHitType::FocusedGameNext:
            if (context.workspaceGameCount <= 0) {
                return true;
            }
            if (context.tryResolveDirtyResourceBeforeContextSwitch
                && !context.tryResolveDirtyResourceBeforeContextSwitch("switching games")) {
                if (context.onInvalidate) context.onInvalidate();
                return true;
            }
            if (context.onFocusedWorkspaceGameIndexChanged) {
                const int next = (context.focusedWorkspaceGameIndex + 1) % context.workspaceGameCount;
                context.onFocusedWorkspaceGameIndexChanged(next);
            }
            return true;
        case EditorLeftPanelHitType::SceneSearch:
        case EditorLeftPanelHitType::ResourceSearch:
            if (context.onSearchActivate) context.onSearchActivate();
            return true;
        case EditorLeftPanelHitType::SceneSearchClear:
            if (context.onSceneSearchClear) context.onSceneSearchClear();
            return true;
        case EditorLeftPanelHitType::ResourceSearchClear:
            if (context.onResourceSearchClear) context.onResourceSearchClear();
            return true;
        case EditorLeftPanelHitType::ResourceCategoryChip: {
            static constexpr std::array<WorkspaceResourceCategory, 8> kCategories = {
                WorkspaceResourceCategory::Manifest,
                WorkspaceResourceCategory::Level,
                WorkspaceResourceCategory::Script,
                WorkspaceResourceCategory::Test,
                WorkspaceResourceCategory::UiScreen,
                WorkspaceResourceCategory::Menu,
                WorkspaceResourceCategory::Asset,
                WorkspaceResourceCategory::Other,
            };
            if (leftHit.index >= 0 && leftHit.index < static_cast<int>(kCategories.size())) {
                const WorkspaceResourceCategory category = kCategories[static_cast<std::size_t>(leftHit.index)];
                const std::uint32_t bit = WorkspaceCategoryBit(category);
                const bool enabled = (context.resourceCategoryMask & bit) != 0u;
                const std::uint32_t nextMask =
                    enabled ? (context.resourceCategoryMask & ~bit) : (context.resourceCategoryMask | bit);
                if (nextMask == 0u) {
                    if (context.onStatus) context.onStatus("Resource filter keeps at least one category enabled.");
                    if (context.onInvalidate) context.onInvalidate();
                    return true;
                }
                if (context.onResourceCategoryMaskChanged) context.onResourceCategoryMaskChanged(nextMask);
                if (context.onRebuildFilteredResourceRows) context.onRebuildFilteredResourceRows();
                if (context.onEnsureSelectedResourceVisible) context.onEnsureSelectedResourceVisible();
                if (context.onStatus) context.onStatus("Resource category filters updated.");
                if (context.onInvalidate) context.onInvalidate();
                return true;
            }
            break;
        }
        case EditorLeftPanelHitType::ResourceRow:
            if (leftHit.index >= 0 && leftHit.index < context.filteredResourceRowCount) {
                if (context.onSelectResourceRow) context.onSelectResourceRow(leftHit.index);
                return true;
            }
            break;
        case EditorLeftPanelHitType::SceneRow:
            if (leftHit.index >= 0 && leftHit.index < context.hierarchyRowCount) {
                if (context.onSelectSceneRow) context.onSelectSceneRow(leftHit.index);
                return true;
            }
            break;
        case EditorLeftPanelHitType::None:
            break;
    }

    return false;
}

bool DispatchEditorInspectorTabClick(const RECT& inspectorInner,
                                     const POINT& point,
                                     const EditorInspectorTabDispatchCallbacks& callbacks) {
    switch (HitTestEditorInspectorTabs(inspectorInner, point)) {
        case EditorInspectorTabHit::Node:
            if (callbacks.onNode) callbacks.onNode();
            return true;
        case EditorInspectorTabHit::Brush:
            if (callbacks.onBrush) callbacks.onBrush();
            return true;
        case EditorInspectorTabHit::Gameplay:
            if (callbacks.onGameplay) callbacks.onGameplay();
            return true;
        case EditorInspectorTabHit::Files:
            if (callbacks.onFiles) callbacks.onFiles();
            return true;
        case EditorInspectorTabHit::Store:
            if (callbacks.onStore) callbacks.onStore();
            return true;
        case EditorInspectorTabHit::UiWorkbench:
            if (callbacks.onUiWorkbench) callbacks.onUiWorkbench();
            return true;
        case EditorInspectorTabHit::None:
            break;
    }
    return false;
}

bool DispatchEditorInspectorPanelClick(const POINT& point,
                                       const EditorInspectorPanelDispatchContext& context) {
    if (context.tryHandleNudge && context.tryHandleNudge(point)) {
        return true;
    }

    if (context.brushPanelActive) {
        switch (HitTestBrushInspectorPanel(context.inspectorInner, point).type) {
            case EditorInspectorPanelHitType::BrushPresetPrev:
                if (context.onCycleBrushPreset) context.onCycleBrushPreset(-1);
                return true;
            case EditorInspectorPanelHitType::BrushPresetNext:
                if (context.onCycleBrushPreset) context.onCycleBrushPreset(1);
                return true;
            case EditorInspectorPanelHitType::None:
            default:
                break;
        }
    }

    if (context.filesPanelActive) {
        switch (HitTestFilesInspectorPanel(context.inspectorInner, point, context.projectShortcuts).type) {
            case EditorInspectorPanelHitType::SaveResource:
                if (context.onSaveResource) context.onSaveResource();
                return true;
            case EditorInspectorPanelHitType::Explorer:
                if (context.onOpenExplorer) context.onOpenExplorer();
                return true;
            case EditorInspectorPanelHitType::ShortcutManifest:
                if (context.onOpenProjectShortcut) context.onOpenProjectShortcut("manifest.json");
                return true;
            case EditorInspectorPanelHitType::ShortcutLevel:
                if (context.onOpenProjectShortcut) {
                    context.onOpenProjectShortcut(context.primaryLevelShortcutPath.empty()
                                                      ? "levels/assembly.primitives.csv"
                                                      : context.primaryLevelShortcutPath);
                }
                return true;
            case EditorInspectorPanelHitType::ShortcutGameplay:
                if (context.onOpenProjectShortcut) context.onOpenProjectShortcut("scripts/gameplay.riscript");
                return true;
            case EditorInspectorPanelHitType::ShortcutRendering:
                if (context.onOpenProjectShortcut) context.onOpenProjectShortcut("scripts/rendering.riscript");
                return true;
            case EditorInspectorPanelHitType::ShortcutUiLayout:
                if (context.onOpenProjectShortcut) context.onOpenProjectShortcut("ui/main.ui.json");
                return true;
            case EditorInspectorPanelHitType::ShortcutUiStyle:
                if (context.onOpenProjectShortcut) context.onOpenProjectShortcut("ui/vn_intro.ui.json");
                return true;
            case EditorInspectorPanelHitType::ShortcutMenu:
                if (context.onOpenProjectShortcut) context.onOpenProjectShortcut("menus/main.menu");
                return true;
            case EditorInspectorPanelHitType::ShortcutAi:
                if (context.onOpenProjectShortcut) context.onOpenProjectShortcut("ai/behavior.tree");
                return true;
            case EditorInspectorPanelHitType::ShortcutNetwork:
                if (context.onOpenProjectShortcut) context.onOpenProjectShortcut("scripts/network.riscript");
                return true;
            case EditorInspectorPanelHitType::ShortcutPlugins:
                if (context.onOpenProjectShortcut) context.onOpenProjectShortcut("plugins/manifest.plugins");
                return true;
            case EditorInspectorPanelHitType::None:
            default:
                break;
        }
    }

    if (context.gameplayPanelActive) {
        switch (HitTestGameplayInspectorPanel(context.gameplayLayout, point).type) {
            case EditorInspectorPanelHitType::GameplayInventoryMode:
                if (context.onCycleInventoryMode) context.onCycleInventoryMode();
                return true;
            case EditorInspectorPanelHitType::GameplayOffHand:
                if (context.onToggleOffHand) context.onToggleOffHand();
                return true;
            case EditorInspectorPanelHitType::GameplayAddTrigger:
                if (context.onAddTrigger) context.onAddTrigger();
                return true;
            case EditorInspectorPanelHitType::GameplayExport:
                if (context.onExportGameplay) context.onExportGameplay();
                return true;
            case EditorInspectorPanelHitType::GameplayPlaytest:
                if (context.onGameplayPlaytest) context.onGameplayPlaytest();
                return true;
            case EditorInspectorPanelHitType::None:
            default:
                break;
        }
    }

    if (context.pluginStoreActive) {
        const EditorInspectorPanelHit storeHit = HitTestPluginStorePanel(context.pluginStoreLayout, point);
        switch (storeHit.type) {
            case EditorInspectorPanelHitType::PluginStoreRefresh:
                if (context.onRefreshPluginStore) context.onRefreshPluginStore();
                return true;
            case EditorInspectorPanelHitType::PluginStoreOpenFolder:
                if (context.onOpenPluginStoreFolder) context.onOpenPluginStoreFolder();
                return true;
            case EditorInspectorPanelHitType::PluginStoreScrollPrev:
                if (context.onPluginStoreScrollPrev) context.onPluginStoreScrollPrev();
                return true;
            case EditorInspectorPanelHitType::PluginStoreScrollNext:
                if (context.onPluginStoreScrollNext) context.onPluginStoreScrollNext();
                return true;
            case EditorInspectorPanelHitType::PluginStoreAction:
                if (context.onPluginStoreAction && storeHit.index >= 0) {
                    context.onPluginStoreAction(storeHit.index);
                }
                return true;
            case EditorInspectorPanelHitType::PluginStoreUninstall:
                if (context.onPluginStoreUninstall && storeHit.index >= 0) {
                    context.onPluginStoreUninstall(storeHit.index);
                }
                return true;
            case EditorInspectorPanelHitType::None:
            default:
                break;
        }
    }

    if (context.uiWorkbenchActive) {
        switch (HitTestUiWorkbenchInspectorPanel(context.uiWorkbenchLayout, point).type) {
            case EditorInspectorPanelHitType::UiWorkbenchPrevScreen:
                if (context.onCycleUiWorkbenchScreen) context.onCycleUiWorkbenchScreen(-1);
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchNextScreen:
                if (context.onCycleUiWorkbenchScreen) context.onCycleUiWorkbenchScreen(1);
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchUseAuto:
                if (context.onUseAutoUiWorkbenchSource) context.onUseAutoUiWorkbenchSource();
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchUseMenuSample:
                if (context.onUseMenuSampleUiWorkbenchSource) context.onUseMenuSampleUiWorkbenchSource();
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchUseVnSample:
                if (context.onUseVnSampleUiWorkbenchSource) context.onUseVnSampleUiWorkbenchSource();
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchNewScreen:
                if (context.onUiWorkbenchNewScreen) context.onUiWorkbenchNewScreen();
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchNewMenuScreen:
                if (context.onUiWorkbenchNewMenuScreen) context.onUiWorkbenchNewMenuScreen();
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchDuplicateScreen:
                if (context.onUiWorkbenchDuplicateScreen) context.onUiWorkbenchDuplicateScreen();
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchAddButtonBlock:
                if (context.onUiWorkbenchAddButtonBlock) context.onUiWorkbenchAddButtonBlock();
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchAddHeadingBlock:
                if (context.onUiWorkbenchAddHeadingBlock) context.onUiWorkbenchAddHeadingBlock();
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchAddParagraphBlock:
                if (context.onUiWorkbenchAddParagraphBlock) context.onUiWorkbenchAddParagraphBlock();
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchAddSpacerBlock:
                if (context.onUiWorkbenchAddSpacerBlock) context.onUiWorkbenchAddSpacerBlock();
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchAddChoiceBlock:
                if (context.onUiWorkbenchAddChoiceBlock) context.onUiWorkbenchAddChoiceBlock();
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchSetStartScreen:
                if (context.onUiWorkbenchSetStartScreen) context.onUiWorkbenchSetStartScreen();
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchAddDialogueBlock:
                if (context.onUiWorkbenchAddDialogueBlock) context.onUiWorkbenchAddDialogueBlock();
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchAddNarrationBlock:
                if (context.onUiWorkbenchAddNarrationBlock) context.onUiWorkbenchAddNarrationBlock();
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchMoveBlockUp:
                if (context.onUiWorkbenchMoveBlockUp) context.onUiWorkbenchMoveBlockUp();
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchMoveBlockDown:
                if (context.onUiWorkbenchMoveBlockDown) context.onUiWorkbenchMoveBlockDown();
                return true;
            case EditorInspectorPanelHitType::UiWorkbenchDeleteBlock:
                if (context.onUiWorkbenchDeleteBlock) context.onUiWorkbenchDeleteBlock();
                return true;
            case EditorInspectorPanelHitType::None:
            default:
                break;
        }
    }

    return false;
}

bool DispatchEditorCommandHotkey(const EditorCommandHotkeyContext& context) {
    const WPARAM key = context.key;
    const bool controlHeld = context.controlHeld;
    const bool shiftHeld = context.shiftHeld;

    if (controlHeld && key == 'Z') {
        if (context.onUndo) context.onUndo();
        return true;
    }
    if (controlHeld && key == 'Y') {
        if (context.onRedo) context.onRedo();
        return true;
    }
    if (controlHeld && shiftHeld && key == 'S') {
        if (context.onSaveTimestampedSnapshot) context.onSaveTimestampedSnapshot();
        return true;
    }
    if (controlHeld && shiftHeld && key == 'M') {
        if (context.onScaffoldMountedGame) context.onScaffoldMountedGame();
        return true;
    }
    if (controlHeld && key == 'S') {
        if (context.filesInspectorActive && context.resourceFileDirty) {
            if (context.onSaveActiveResource) context.onSaveActiveResource();
            return true;
        }
        if (context.onSaveEditorScene) context.onSaveEditorScene();
        return true;
    }
    if (controlHeld && key == 'E') {
        if (context.onExportAssemblyCsv) context.onExportAssemblyCsv();
        return true;
    }
    if (controlHeld && key == 'R') {
        if (context.onResetSelectedTransform) context.onResetSelectedTransform();
        return true;
    }
    if (controlHeld && shiftHeld && key == 'C') {
        if (context.onAddCube) context.onAddCube();
        return true;
    }
    if (controlHeld && shiftHeld && key == 'P') {
        if (context.onAddPlane) context.onAddPlane();
        return true;
    }
    if (controlHeld && shiftHeld && key == 'T') {
        if (context.onAddTrigger) context.onAddTrigger();
        return true;
    }
    if (controlHeld && shiftHeld && key == 'O') {
        if (context.onAddLight) context.onAddLight();
        return true;
    }
    if (controlHeld && shiftHeld && key == 'B') {
        if (context.onSpawnStructuralBrush) context.onSpawnStructuralBrush();
        return true;
    }
    if (controlHeld && shiftHeld && key >= '1' && key <= '9') {
        if (context.onSelectStructuralPresetDigit) {
            context.onSelectStructuralPresetDigit(static_cast<int>(key - '0'));
        }
        return true;
    }
    if (controlHeld && key == 'D') {
        if (context.onDuplicateSelectedNode) context.onDuplicateSelectedNode();
        return true;
    }
    if (key == VK_F2) {
        if (context.onBeginNodeRename) context.onBeginNodeRename();
        return true;
    }
    if (controlHeld && shiftHeld && key == 'N') {
        if (context.onCreateGroupNode) context.onCreateGroupNode();
        return true;
    }
    if (controlHeld && shiftHeld && key == 'G') {
        if (context.onUngroupSelectedNode) context.onUngroupSelectedNode();
        return true;
    }
    if (controlHeld && key == 'G') {
        if (context.onGroupSelectedNode) context.onGroupSelectedNode();
        return true;
    }
    if (controlHeld && shiftHeld && key == 'W') {
        if (context.onReparentSelectedToWorldRoot) context.onReparentSelectedToWorldRoot();
        return true;
    }
    if (key == VK_OEM_4) {
        if (context.onCycleStructuralPreset) context.onCycleStructuralPreset(-1);
        return true;
    }
    if (key == VK_OEM_6) {
        if (context.onCycleStructuralPreset) context.onCycleStructuralPreset(1);
        return true;
    }
    if (key == VK_DELETE) {
        if (context.onDeleteSelectedNode) context.onDeleteSelectedNode();
        return true;
    }
    if (controlHeld && shiftHeld && key == 'I') {
        if (context.onImportPrimaryLevelCsv) context.onImportPrimaryLevelCsv();
        return true;
    }
    if (key == VK_F5) {
        if (context.onReloadFocusedGameScene) context.onReloadFocusedGameScene();
        return true;
    }
    if (controlHeld && shiftHeld && key == 'L') {
        if (context.onLoadAutosaveState) context.onLoadAutosaveState();
        return true;
    }
    if (controlHeld && key == 'L') {
        if (context.onLoadPersistentEditorScene) context.onLoadPersistentEditorScene();
        return true;
    }
    if (shiftHeld && key == 'F') {
        if (context.onFrameAllRenderables) context.onFrameAllRenderables();
        return true;
    }
    if (key == VK_HOME) {
        if (context.onFrameAllRenderables) context.onFrameAllRenderables();
        return true;
    }
    if (key == 'F') {
        if (context.onFrameSelection) context.onFrameSelection();
        return true;
    }
    if (key == 'T') {
        if (context.onSetEditModeTranslate) context.onSetEditModeTranslate();
        return true;
    }
    if (key == 'R') {
        if (context.onSetEditModeRotate) context.onSetEditModeRotate();
        return true;
    }
    if (key == 'U') {
        if (context.onSetEditModeScale) context.onSetEditModeScale();
        return true;
    }
    if (key == 'X') {
        if (context.onSetAxisX) context.onSetAxisX();
        return true;
    }
    if (key == 'Y') {
        if (context.onSetAxisY) context.onSetAxisY();
        return true;
    }
    if (key == 'Z') {
        if (context.onSetAxisZ) context.onSetAxisZ();
        return true;
    }
    if (key == '1') {
        if (context.onSelectInspectorNode) context.onSelectInspectorNode();
        return true;
    }
    if (key == '2') {
        if (context.onSelectInspectorBrush) context.onSelectInspectorBrush();
        return true;
    }
    if (key == '3') {
        if (context.onSelectInspectorGameplay) context.onSelectInspectorGameplay();
        return true;
    }
    if (key == '4') {
        if (context.onSelectInspectorFiles) context.onSelectInspectorFiles();
        return true;
    }
    if (key == '5') {
        if (context.onSelectInspectorStore) context.onSelectInspectorStore();
        return true;
    }
    if (key == '6') {
        if (context.onSelectInspectorUiWorkbench) context.onSelectInspectorUiWorkbench();
        return true;
    }
    if (!controlHeld && !shiftHeld && key == VK_OEM_COMMA) {
        if (context.onSelectAdjacentAuthoredNode) context.onSelectAdjacentAuthoredNode(-1);
        return true;
    }
    if (!controlHeld && !shiftHeld && key == VK_OEM_PERIOD) {
        if (context.onSelectAdjacentAuthoredNode) context.onSelectAdjacentAuthoredNode(1);
        return true;
    }
    if (key == 'I' && context.gameplayInspectorActive) {
        if (context.onCycleGameplayInventoryMode) context.onCycleGameplayInventoryMode();
        return true;
    }
    if (key == 'O' && context.gameplayInspectorActive) {
        if (context.onToggleGameplayOffHand) context.onToggleGameplayOffHand();
        return true;
    }

    return false;
}

int ComputeWheelScrollTop(const int currentScrollTop,
                          const int totalRows,
                          const int visibleRows,
                          const short wheelDelta,
                          const int rowsPerNotch) {
    const int notches = std::max(1, std::abs(static_cast<int>(wheelDelta)) / WHEEL_DELTA);
    const int step = std::max(1, rowsPerNotch) * notches;
    const int maxScroll = std::max(0, totalRows - std::max(1, visibleRows));
    int next = currentScrollTop + (wheelDelta > 0 ? -step : step);
    return std::clamp(next, 0, maxScroll);
}

} // namespace ri::editor
