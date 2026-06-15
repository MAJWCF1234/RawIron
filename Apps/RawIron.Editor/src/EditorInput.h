#pragma once

#include "EditorFilesInspector.h"
#include "EditorInspectorPanels.h"
#include "EditorLeftPanel.h"
#include "EditorViewportRenderer.h"

#include <cstdint>
#include <functional>

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

enum class EditorToolbarHit {
    None,
    Select,
    Create,
    Camera,
    Translate,
    Rotate,
    Scale,
    AxisX,
    AxisY,
    AxisZ,
    SnapToggle,
    SnapStepDown,
    SnapStepUp,
    ResolutionScale,
    AddCube,
    AddPlane,
    AddTrigger,
    AddLight,
    Duplicate,
    ExportCsv,
    Play,
};

enum class EditorLeftPanelHitType {
    None,
    SceneTab,
    CreateTab,
    ResourcesTab,
    FocusedGamePrev,
    FocusedGameNext,
    SceneSearch,
    SceneSearchClear,
    ResourceCategoryChip,
    ResourceSearch,
    ResourceSearchClear,
    SceneRow,
    ResourceRow,
};

enum class EditorTopChromeHit {
    None,
    NewGame,
    Save,
    Scaffold,
    ExportScene,
    Play,
    Files,
};

enum class EditorInspectorTabHit {
    None,
    Node,
    Brush,
    Gameplay,
    Files,
    Store,
    UiWorkbench,
};

enum class EditorInspectorPanelHitType {
    None,
    BrushPresetPrev,
    BrushPresetNext,
    SaveResource,
    Explorer,
    ShortcutManifest,
    ShortcutLevel,
    ShortcutGameplay,
    ShortcutRendering,
    ShortcutUiLayout,
    ShortcutUiStyle,
    ShortcutMenu,
    ShortcutAi,
    ShortcutNetwork,
    ShortcutPlugins,
    GameplayInventoryMode,
    GameplayOffHand,
    GameplayAddTrigger,
    GameplayExport,
    GameplayPlaytest,
    UiWorkbenchPrevScreen,
    UiWorkbenchNextScreen,
    UiWorkbenchUseAuto,
    UiWorkbenchUseMenuSample,
    UiWorkbenchUseVnSample,
    UiWorkbenchNewScreen,
    UiWorkbenchNewMenuScreen,
    UiWorkbenchDuplicateScreen,
    UiWorkbenchAddButtonBlock,
    UiWorkbenchAddHeadingBlock,
    UiWorkbenchAddParagraphBlock,
    UiWorkbenchAddSpacerBlock,
    UiWorkbenchAddChoiceBlock,
    UiWorkbenchSetStartScreen,
    UiWorkbenchAddDialogueBlock,
    UiWorkbenchAddNarrationBlock,
    UiWorkbenchMoveBlockUp,
    UiWorkbenchMoveBlockDown,
    UiWorkbenchDeleteBlock,
    PluginStoreRefresh,
    PluginStoreOpenFolder,
    PluginStoreScrollPrev,
    PluginStoreScrollNext,
    PluginStoreAction,
    PluginStoreUninstall,
};

struct EditorLeftPanelHit {
    EditorLeftPanelHitType type = EditorLeftPanelHitType::None;
    int index = -1;
};

struct EditorInspectorPanelHit {
    EditorInspectorPanelHitType type = EditorInspectorPanelHitType::None;
    int index = -1;
};

struct EditorTopChromeDispatchCallbacks {
    std::function<void()> onNewGame;
    std::function<void()> onSave;
    std::function<void()> onScaffold;
    std::function<void()> onExportScene;
    std::function<void()> onPlay;
    std::function<void()> onFiles;
};

struct EditorToolbarDispatchCallbacks {
    std::function<void()> onSelectMode;
    std::function<void()> onCreateMode;
    std::function<void()> onCameraMode;
    std::function<void()> onTranslate;
    std::function<void()> onRotate;
    std::function<void()> onScale;
    std::function<void()> onAxisX;
    std::function<void()> onAxisY;
    std::function<void()> onAxisZ;
    std::function<void()> onSnapToggle;
    std::function<void()> onSnapStepDown;
    std::function<void()> onSnapStepUp;
    std::function<void()> onResolutionScaleToggle;
    std::function<void()> onAddCube;
    std::function<void()> onAddPlane;
    std::function<void()> onAddTrigger;
    std::function<void()> onAddLight;
    std::function<void()> onDuplicate;
    std::function<void()> onExportCsv;
    std::function<void()> onPlay;
};

struct EditorLeftPanelDispatchContext {
    RECT hierarchyInner{};
    EditorLeftPanelMode mode = EditorLeftPanelMode::Scene;
    bool hasWorkspaceGames = false;
    POINT point{};
    int hierarchyScrollTopRow = 0;
    int resourceCatalogScrollTopRow = 0;
    int workspaceGameCount = 0;
    int focusedWorkspaceGameIndex = 0;
    std::uint32_t resourceCategoryMask = 0;
    int filteredResourceRowCount = 0;
    int hierarchyRowCount = 0;
    std::function<bool(const char*)> tryResolveDirtyResourceBeforeContextSwitch;
    std::function<void(int)> onFocusedWorkspaceGameIndexChanged;
    std::function<void()> onSceneTab;
    std::function<void()> onCreateTab;
    std::function<void()> onResourcesTab;
    std::function<void()> onSearchActivate;
    std::function<void()> onSceneSearchClear;
    std::function<void()> onResourceSearchClear;
    std::function<void(std::uint32_t)> onResourceCategoryMaskChanged;
    std::function<void(int)> onSelectResourceRow;
    std::function<void(int)> onSelectSceneRow;
    std::function<void(const std::string&)> onStatus;
    std::function<void()> onRebuildFilteredHierarchyOrder;
    std::function<void()> onRebuildFilteredResourceRows;
    std::function<void()> onEnsureSelectedResourceVisible;
    std::function<void()> onInvalidate;
};

struct EditorInspectorTabDispatchCallbacks {
    std::function<void()> onNode;
    std::function<void()> onBrush;
    std::function<void()> onGameplay;
    std::function<void()> onFiles;
    std::function<void()> onStore;
    std::function<void()> onUiWorkbench;
};

struct EditorInspectorPanelDispatchContext {
    RECT inspectorInner{};
    GameplayPanelLayout gameplayLayout{};
    UiWorkbenchLayout uiWorkbenchLayout{};
    PluginStoreLayout pluginStoreLayout{};
    ProjectShortcutLayout projectShortcuts{};
    std::string primaryLevelShortcutPath;
    bool brushPanelActive = false;
    bool filesPanelActive = false;
    bool gameplayPanelActive = false;
    bool pluginStoreActive = false;
    bool uiWorkbenchActive = false;
    std::function<bool(const POINT&)> tryHandleNudge;
    std::function<void(int)> onCycleBrushPreset;
    std::function<void()> onSaveResource;
    std::function<void()> onOpenExplorer;
    std::function<void(const std::string&)> onOpenProjectShortcut;
    std::function<void()> onCycleInventoryMode;
    std::function<void()> onToggleOffHand;
    std::function<void()> onAddTrigger;
    std::function<void()> onExportGameplay;
    std::function<void()> onGameplayPlaytest;
    std::function<void()> onRefreshPluginStore;
    std::function<void()> onOpenPluginStoreFolder;
    std::function<void()> onPluginStoreScrollPrev;
    std::function<void()> onPluginStoreScrollNext;
    std::function<void(int)> onPluginStoreAction;
    std::function<void(int)> onPluginStoreUninstall;
    std::function<void(int)> onCycleUiWorkbenchScreen;
    std::function<void()> onUseAutoUiWorkbenchSource;
    std::function<void()> onUseMenuSampleUiWorkbenchSource;
    std::function<void()> onUseVnSampleUiWorkbenchSource;
    std::function<void()> onUiWorkbenchNewScreen;
    std::function<void()> onUiWorkbenchNewMenuScreen;
    std::function<void()> onUiWorkbenchDuplicateScreen;
    std::function<void()> onUiWorkbenchAddButtonBlock;
    std::function<void()> onUiWorkbenchAddHeadingBlock;
    std::function<void()> onUiWorkbenchAddParagraphBlock;
    std::function<void()> onUiWorkbenchAddSpacerBlock;
    std::function<void()> onUiWorkbenchAddChoiceBlock;
    std::function<void()> onUiWorkbenchSetStartScreen;
    std::function<void()> onUiWorkbenchAddDialogueBlock;
    std::function<void()> onUiWorkbenchAddNarrationBlock;
    std::function<void()> onUiWorkbenchMoveBlockUp;
    std::function<void()> onUiWorkbenchMoveBlockDown;
    std::function<void()> onUiWorkbenchDeleteBlock;
};

struct EditorCommandHotkeyContext {
    WPARAM key = 0;
    bool controlHeld = false;
    bool shiftHeld = false;
    bool filesInspectorActive = false;
    bool gameplayInspectorActive = false;
    bool resourceFileDirty = false;
    std::function<void()> onUndo;
    std::function<void()> onRedo;
    std::function<void()> onSaveTimestampedSnapshot;
    std::function<void()> onScaffoldMountedGame;
    std::function<void()> onSaveActiveResource;
    std::function<void()> onSaveEditorScene;
    std::function<void()> onExportAssemblyCsv;
    std::function<void()> onResetSelectedTransform;
    std::function<void()> onAddCube;
    std::function<void()> onAddPlane;
    std::function<void()> onAddTrigger;
    std::function<void()> onAddLight;
    std::function<void()> onSpawnStructuralBrush;
    std::function<void(int)> onSelectStructuralPresetDigit;
    std::function<void()> onDuplicateSelectedNode;
    std::function<void()> onBeginNodeRename;
    std::function<void()> onCreateGroupNode;
    std::function<void()> onUngroupSelectedNode;
    std::function<void()> onGroupSelectedNode;
    std::function<void()> onReparentSelectedToWorldRoot;
    std::function<void(int)> onCycleStructuralPreset;
    std::function<void()> onDeleteSelectedNode;
    std::function<void()> onImportPrimaryLevelCsv;
    std::function<void()> onReloadFocusedGameScene;
    std::function<void()> onLoadAutosaveState;
    std::function<void()> onLoadPersistentEditorScene;
    std::function<void()> onFrameAllRenderables;
    std::function<void()> onFrameSelection;
    std::function<void()> onSetEditModeTranslate;
    std::function<void()> onSetEditModeRotate;
    std::function<void()> onSetEditModeScale;
    std::function<void()> onSetAxisX;
    std::function<void()> onSetAxisY;
    std::function<void()> onSetAxisZ;
    std::function<void()> onSelectInspectorNode;
    std::function<void()> onSelectInspectorBrush;
    std::function<void()> onSelectInspectorGameplay;
    std::function<void()> onSelectInspectorFiles;
    std::function<void()> onSelectInspectorStore;
    std::function<void()> onSelectInspectorUiWorkbench;
    std::function<void(int)> onSelectAdjacentAuthoredNode;
    std::function<void()> onCycleGameplayInventoryMode;
    std::function<void()> onToggleGameplayOffHand;
};

[[nodiscard]] EditorTopChromeHit HitTestEditorTopChrome(const RECT& clientRect, const POINT& point);
[[nodiscard]] EditorToolbarHit HitTestEditorToolbar(const RECT& toolStrip, const POINT& point);
[[nodiscard]] EditorLeftPanelHit HitTestEditorLeftPanel(const RECT& hierarchyInner,
                                                        EditorLeftPanelMode mode,
                                                        bool hasWorkspaceGames,
                                                        const POINT& point,
                                                        int hierarchyScrollTopRow,
                                                        int resourceCatalogScrollTopRow);
[[nodiscard]] EditorInspectorTabHit HitTestEditorInspectorTabs(const RECT& inspectorInner, const POINT& point);
[[nodiscard]] EditorInspectorPanelHit HitTestBrushInspectorPanel(const RECT& inspectorInner, const POINT& point);
[[nodiscard]] EditorInspectorPanelHit HitTestFilesInspectorPanel(const RECT& inspectorInner,
                                                                 const POINT& point,
                                                                 const ProjectShortcutLayout& shortcuts);
[[nodiscard]] EditorInspectorPanelHit HitTestGameplayInspectorPanel(const GameplayPanelLayout& layout,
                                                                    const POINT& point);
[[nodiscard]] EditorInspectorPanelHit HitTestPluginStorePanel(const PluginStoreLayout& layout, const POINT& point);
[[nodiscard]] EditorInspectorPanelHit HitTestUiWorkbenchInspectorPanel(const UiWorkbenchLayout& layout,
                                                                       const POINT& point);
[[nodiscard]] bool DispatchEditorTopChromeClick(const RECT& clientRect,
                                                const POINT& point,
                                                const EditorTopChromeDispatchCallbacks& callbacks);
[[nodiscard]] bool DispatchEditorToolbarClick(const RECT& toolStrip,
                                              const POINT& point,
                                              const EditorToolbarDispatchCallbacks& callbacks);
[[nodiscard]] bool DispatchEditorLeftPanelClick(const EditorLeftPanelDispatchContext& context);
[[nodiscard]] bool DispatchEditorInspectorTabClick(const RECT& inspectorInner,
                                                   const POINT& point,
                                                   const EditorInspectorTabDispatchCallbacks& callbacks);
[[nodiscard]] bool DispatchEditorInspectorPanelClick(const POINT& point,
                                                     const EditorInspectorPanelDispatchContext& context);
[[nodiscard]] bool DispatchEditorCommandHotkey(const EditorCommandHotkeyContext& context);
[[nodiscard]] int ComputeWheelScrollTop(int currentScrollTop,
                                        int totalRows,
                                        int visibleRows,
                                        short wheelDelta,
                                        int rowsPerNotch);

} // namespace ri::editor
