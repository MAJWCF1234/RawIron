#pragma once

#include <functional>
#include <optional>
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

inline constexpr int kInspectorSessionCardHeight = 72;

[[nodiscard]] inline int InspectorContentBottom(const RECT& inspectorInner) {
    return static_cast<int>(inspectorInner.bottom) - kInspectorSessionCardHeight;
}

#if defined(_WIN32)
struct GameplayPanelLayout {
    RECT inventoryModeRow{};
    RECT offHandRow{};
    RECT addTriggerBtn{};
    RECT exportBtn{};
    RECT playtestBtn{};
};

struct BrushPanelLayout {
    RECT presetPrevBtn{};
    RECT presetNextBtn{};
};

struct InspectorTabLayout {
    RECT nodeTab{};
    RECT brushTab{};
    RECT gameplayTab{};
    RECT filesTab{};
    RECT storeTab{};
    RECT uiWorkbenchTab{};
    int contentTop = 0;
};

struct UiWorkbenchLayout {
    RECT prevScreenBtn{};
    RECT nextScreenBtn{};
    RECT useAutoBtn{};
    RECT useMenuSampleBtn{};
    RECT useVnSampleBtn{};
    RECT newScreenBtn{};
    RECT newMenuScreenBtn{};
    RECT duplicateScreenBtn{};
    RECT addButtonBlockBtn{};
    RECT addHeadingBlockBtn{};
    RECT addParagraphBlockBtn{};
    RECT addSpacerBlockBtn{};
    RECT addChoiceBlockBtn{};
    RECT setStartScreenBtn{};
    RECT addDialogueBlockBtn{};
    RECT addNarrationBlockBtn{};
    RECT moveBlockUpBtn{};
    RECT moveBlockDownBtn{};
    RECT deleteBlockBtn{};
};

struct NodeInspectorPanelModel {
    bool renameTypingActive = false;
    std::string renameDraft;
    std::string nameLine;
    std::string pathLine;
    std::string kindLine;
    std::optional<std::string> meshPrimitiveLine;
    std::string localPosLine;
    std::string localRotLine;
    std::string localScaleLine;
    bool editableAuthored = false;
    std::string editModeLine;
    std::string groupingLine;
    std::string opsLine;
    std::string worldPosLine;
    bool hasMaterial = false;
    bool materialEditable = false;
    std::string materialNameLine;
    std::string materialColorLine;
    std::string materialRoughnessLine;
    std::string materialMetallicLine;
    std::string materialOpacityLine;
    std::string materialTextureLine;
    std::string materialFlagsLine;
    bool hasLight = false;
    bool lightEditable = false;
    std::string lightTypeLine;
    std::string lightColorLine;
    std::string lightIntensityLine;
    std::string lightRangeLine;
    bool hasTrigger = false;
    std::string triggerBoundsLine;
    std::string triggerHelpLine;
};

struct BrushInspectorPanelModel {
    std::string presetTitleLine;
    std::string headingLine;
    std::string helpLineA;
    std::string helpLineB;
    std::string selectionLine;
    std::string meshAttachedLine;
    std::string boundsSizeLine;
    std::string boundsCenterLine;
    bool hasStructuralMetadata = false;
    bool structuralMetadataValid = false;
    std::string semanticIdentityLine;
    std::string semanticPolicyLine;
    std::string semanticChannelsLine;
    std::string semanticValidationLine;
};

struct GameplayInspectorPanelModel {
    GameplayPanelLayout layout{};
    std::string headingLine;
    std::string summaryLine;
    std::string inventoryModeLine;
    std::string offHandLine;
    std::string gameplayStorageLine;
    std::string hotbarLine;
    std::string controlsLine;
};

struct PluginStoreCardModel {
    int packageIndex = -1;
    std::string titleLine;
    std::string metaLine;
    std::string tagLine;
    std::string descriptionLine;
    std::string statusLine;
    std::string policyLine;
    bool installed = false;
    bool enabled = false;
    bool blocked = false;
    std::string actionLabel;
    std::string secondaryActionLabel;
};

struct PluginStoreLayout {
    RECT refreshBtn{};
    RECT openFolderBtn{};
    RECT scrollPrevBtn{};
    RECT scrollNextBtn{};
    std::vector<RECT> cardRects{};
    std::vector<RECT> actionBtns{};
    std::vector<RECT> secondaryActionBtns{};
    std::vector<int> cardPackageIndices{};
    int scrollTopRow = 0;
    int totalCards = 0;
    int visibleCards = 0;
};

struct PluginStorePanelModel {
    PluginStoreLayout layout{};
    std::string headingLine;
    std::string summaryLine;
    std::string modelHelpLine;
    std::string storePathLine;
    std::string statusLine;
    std::string scrollLine;
    bool hasMountedGame = false;
    int scrollTopRow = 0;
    std::vector<PluginStoreCardModel> cards;
};

enum class UiWorkbenchBlockTone {
    Heading,
    Say,
    Narration,
    Choices,
    Button,
    Image,
    Note,
    Other,
};

struct UiWorkbenchScreenSummary {
    std::string titleLine;
    std::string metaLine;
    bool selected = false;
};

struct UiWorkbenchPreviewBlock {
    UiWorkbenchBlockTone tone = UiWorkbenchBlockTone::Other;
    std::string titleLine;
    std::string detailLine;
    int preferredHeight = 0;
    bool selected = false;
};

struct UiWorkbenchPanelModel {
    UiWorkbenchLayout layout{};
    bool manifestResolved = false;
    bool manifestParsed = false;
    bool usingAutoSource = true;
    bool usingMenuSample = false;
    bool usingVnSample = false;
    std::string headingLine;
    std::string sourceLine;
    std::string statusLine;
    std::string hintLine;
    std::string errorLine;
    std::string screenHeaderLine;
    std::string actionsHeaderLine;
    std::string blockActionsHeaderLine;
    std::string previewTitleLine;
    std::string previewMetaLine;
    std::string previewFooterLine;
    std::vector<UiWorkbenchScreenSummary> screens;
    std::vector<UiWorkbenchPreviewBlock> previewBlocks;
};

struct UiWorkbenchInspectorLayout {
    UiWorkbenchLayout toolbar{};
    RECT manifestCard{};
    RECT railCard{};
    RECT previewCard{};
    RECT stageRect{};
};

struct UiWorkbenchViewportLayout {
    RECT headerRect{};
    RECT shelfRect{};
    RECT stageCard{};
    RECT stageRect{};
    bool stackLayout = false;
};

[[nodiscard]] GameplayPanelLayout ComputeGameplayPanelLayout(const RECT& inspectorInner);
[[nodiscard]] BrushPanelLayout ComputeBrushPanelLayout(const RECT& inspectorInner);
[[nodiscard]] InspectorTabLayout ComputeInspectorTabLayout(const RECT& inspectorInner);
[[nodiscard]] PluginStoreLayout ComputePluginStoreLayout(const RECT& inspectorInner,
                                                         int cardCount,
                                                         int scrollTopRow);
[[nodiscard]] UiWorkbenchLayout ComputeUiWorkbenchLayout(const RECT& inspectorInner);
[[nodiscard]] UiWorkbenchInspectorLayout ComputeUiWorkbenchInspectorLayout(const RECT& inspectorInner);
[[nodiscard]] std::vector<RECT> ComputeUiWorkbenchScreenRowRects(const UiWorkbenchInspectorLayout& layout,
                                                                   int screenCount);
[[nodiscard]] std::vector<RECT> ComputeUiWorkbenchInspectorPreviewBlockRects(
    const UiWorkbenchInspectorLayout& layout,
    const std::vector<UiWorkbenchPreviewBlock>& blocks);
[[nodiscard]] UiWorkbenchViewportLayout ComputeUiWorkbenchViewportLayout(const RECT& viewportInner);
[[nodiscard]] std::vector<RECT> ComputeUiWorkbenchViewportBlockRects(
    const UiWorkbenchViewportLayout& layout,
    const std::vector<UiWorkbenchPreviewBlock>& blocks);
void RenderNodeInspectorPanel(HDC dc,
                              const RECT& inspectorInner,
                              const NodeInspectorPanelModel& model,
                              HFONT headerFont,
                              HFONT bodyFont,
                              HFONT smallFont,
                              const std::function<void(HDC, int&, const RECT&, const char*, int)>& drawNudgeRow,
                              const std::function<void(HDC, int&, const RECT&, const char*, int)>& drawMaterialNudgeRow,
                              const std::function<void(HDC, int&, const RECT&, const char*, int)>& drawLightNudgeRow);
void RenderBrushInspectorPanel(HDC dc,
                               const RECT& inspectorInner,
                               const BrushInspectorPanelModel& model,
                               HFONT headerFont,
                               HFONT bodyFont,
                               HFONT smallFont,
                               const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton);
void RenderGameplayInspectorPanel(HDC dc,
                                  const RECT& inspectorInner,
                                  const GameplayInspectorPanelModel& model,
                                  HFONT headerFont,
                                  HFONT bodyFont,
                                  HFONT smallFont,
                                  const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton);
void RenderPluginStorePanel(HDC dc,
                            const RECT& inspectorInner,
                            PluginStorePanelModel& model,
                            HFONT headerFont,
                            HFONT bodyFont,
                            HFONT smallFont,
                            const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton);
void RenderUiWorkbenchPanel(HDC dc,
                            const RECT& inspectorInner,
                            const UiWorkbenchPanelModel& model,
                            HFONT headerFont,
                            HFONT bodyFont,
                            HFONT smallFont,
                            const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton);
#endif

} // namespace ri::editor
