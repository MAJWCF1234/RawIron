#include "EditorInspectorPanels.h"

#include "EditorRenderer.h"

#include <algorithm>

namespace ri::editor {

#if defined(_WIN32)
namespace {

void DrawInspectorCard(HDC dc, const RECT& rect, COLORREF fill, COLORREF highlight, COLORREF shadow, COLORREF accent) {
    EditorRenderer::DrawInsetFrame(dc, rect, fill, highlight, shadow);
    const RECT accentBar{rect.left + 1, rect.top + 1, rect.right - 1, std::min(rect.bottom - 1, rect.top + 5)};
    if (accentBar.bottom > accentBar.top) {
        EditorRenderer::FillRectColor(dc, accentBar, accent);
    }
}

} // namespace

GameplayPanelLayout ComputeGameplayPanelLayout(const RECT& inspectorInner) {
    GameplayPanelLayout layout{};
    const int headingTop = inspectorInner.top + 42;
    const int gameplayCardTop = headingTop + 24;
    const int policyCardTop = gameplayCardTop + 88 + 10;
    layout.inventoryModeRow =
        RECT{inspectorInner.left + 18, policyCardTop + 8, inspectorInner.right - 18, policyCardTop + 28};
    layout.offHandRow =
        RECT{inspectorInner.left + 18, policyCardTop + 32, inspectorInner.right - 18, policyCardTop + 52};
    const int actionCardTop = policyCardTop + 102 + 10;
    const int left = inspectorInner.left + 18;
    const int right = inspectorInner.right - 18;
    const int gap = 6;
    const int btnWidth = std::max(72, (right - left - gap * 2) / 3);
    layout.addTriggerBtn = RECT{left, actionCardTop + 8, left + btnWidth, actionCardTop + 36};
    layout.exportBtn = RECT{left + btnWidth + gap, actionCardTop + 8, left + btnWidth * 2 + gap, actionCardTop + 36};
    layout.playtestBtn = RECT{left + btnWidth * 2 + gap * 2, actionCardTop + 8, right, actionCardTop + 36};
    return layout;
}

BrushPanelLayout ComputeBrushPanelLayout(const RECT& inspectorInner) {
    const RECT presetCard{inspectorInner.left + 10, inspectorInner.top + 42, inspectorInner.right - 10, inspectorInner.top + 76};
    BrushPanelLayout layout{};
    layout.presetPrevBtn = RECT{presetCard.left + 8, presetCard.top + 8, presetCard.left + 50, presetCard.top + 32};
    layout.presetNextBtn = RECT{presetCard.left + 54, presetCard.top + 8, presetCard.left + 96, presetCard.top + 32};
    return layout;
}

InspectorTabLayout ComputeInspectorTabLayout(const RECT& inspectorInner) {
    InspectorTabLayout layout{};
    const int left = inspectorInner.left + 12;
    const int right = inspectorInner.right - 6;
    const int innerWidth = std::max(0, right - left);
    const int rowTop = inspectorInner.top + 10;
    constexpr int kTabGap = 4;
    const int tabWidth = std::max(36, (innerWidth - kTabGap * 5) / 6);
    int x = left;
    layout.nodeTab = RECT{x, rowTop, x + tabWidth, rowTop + 24};
    x += tabWidth + kTabGap;
    layout.brushTab = RECT{x, rowTop, x + tabWidth, rowTop + 24};
    x += tabWidth + kTabGap;
    layout.gameplayTab = RECT{x, rowTop, x + tabWidth, rowTop + 24};
    x += tabWidth + kTabGap;
    layout.filesTab = RECT{x, rowTop, x + tabWidth, rowTop + 24};
    x += tabWidth + kTabGap;
    layout.storeTab = RECT{x, rowTop, x + tabWidth, rowTop + 24};
    x += tabWidth + kTabGap;
    layout.uiWorkbenchTab = RECT{x, rowTop, right, rowTop + 24};
    layout.contentTop = inspectorInner.top + 48;
    return layout;
}

PluginStoreLayout ComputePluginStoreLayout(const RECT& inspectorInner, const int cardCount, const int scrollTopRow) {
    PluginStoreLayout layout{};
    const int left = inspectorInner.left + 10;
    const int right = inspectorInner.right - 10;
    const int toolbarTop = inspectorInner.top + 48;
    const int btnWidth = std::max(56, (right - left - 12) / 4);
    layout.refreshBtn = RECT{left, toolbarTop, left + btnWidth, toolbarTop + 24};
    layout.openFolderBtn = RECT{left + btnWidth + 4, toolbarTop, left + btnWidth * 2 + 4, toolbarTop + 24};
    layout.scrollPrevBtn = RECT{left + btnWidth * 2 + 8, toolbarTop, left + btnWidth * 3 + 8, toolbarTop + 24};
    layout.scrollNextBtn = RECT{left + btnWidth * 3 + 12, toolbarTop, right, toolbarTop + 24};

    int cardTop = toolbarTop + 34;
    constexpr int kCardHeight = 108;
    constexpr int kCardGap = 8;
    const int contentBottom = InspectorContentBottom(inspectorInner);
    int maxVisible = 0;
    for (int probeTop = cardTop; probeTop + kCardHeight <= contentBottom - 8; probeTop += kCardHeight + kCardGap) {
        ++maxVisible;
    }
    maxVisible = std::max(1, maxVisible);
    layout.totalCards = cardCount;
    layout.visibleCards = std::min(cardCount, maxVisible);
    layout.scrollTopRow = std::max(0, std::min(scrollTopRow, std::max(0, cardCount - layout.visibleCards)));

    for (int visibleIndex = 0; visibleIndex < layout.visibleCards; ++visibleIndex) {
        const int packageIndex = layout.scrollTopRow + visibleIndex;
        if (packageIndex >= cardCount) {
            break;
        }
        if (cardTop + kCardHeight > contentBottom - 8) {
            break;
        }
        const RECT cardRect{left, cardTop, right, cardTop + kCardHeight};
        layout.cardRects.push_back(cardRect);
        layout.cardPackageIndices.push_back(packageIndex);
        layout.secondaryActionBtns.push_back(
            RECT{cardRect.right - 184, cardRect.bottom - 30, cardRect.right - 98, cardRect.bottom - 8});
        layout.actionBtns.push_back(
            RECT{cardRect.right - 92, cardRect.bottom - 30, cardRect.right - 10, cardRect.bottom - 8});
        cardTop += kCardHeight + kCardGap;
    }
    return layout;
}

UiWorkbenchLayout ComputeUiWorkbenchLayout(const RECT& inspectorInner) {
    UiWorkbenchLayout layout{};
    const int top = inspectorInner.top + 94;
    const int left = inspectorInner.left + 10;
    const int right = inspectorInner.right - 10;
    const int innerWidth = std::max(0, right - left);
    const int gap = 6;
    const int slotWidth = std::max(42, (innerWidth - gap * 4) / 5);
    int x = left;
    layout.prevScreenBtn = RECT{x, top, x + slotWidth, top + 24};
    x += slotWidth + gap;
    layout.nextScreenBtn = RECT{x, top, x + slotWidth, top + 24};
    x += slotWidth + gap;
    layout.useAutoBtn = RECT{x, top, x + slotWidth, top + 24};
    x += slotWidth + gap;
    layout.useMenuSampleBtn = RECT{x, top, x + slotWidth, top + 24};
    x += slotWidth + gap;
    layout.useVnSampleBtn = RECT{x, top, right, top + 24};
    const int actionTop = top + 34;
    const int thirdWidth = std::max(72, (right - left - gap * 2) / 3);
    layout.newScreenBtn = RECT{left, actionTop, left + thirdWidth, actionTop + 24};
    layout.newMenuScreenBtn = RECT{left + thirdWidth + gap, actionTop, left + thirdWidth * 2 + gap, actionTop + 24};
    layout.duplicateScreenBtn = RECT{left + thirdWidth * 2 + gap * 2, actionTop, right, actionTop + 24};
    const int menuRowTop = actionTop + 30;
    const int quarterWidth = std::max(56, (right - left - gap * 3) / 4);
    layout.addButtonBlockBtn = RECT{left, menuRowTop, left + quarterWidth, menuRowTop + 24};
    layout.addHeadingBlockBtn = RECT{left + quarterWidth + gap, menuRowTop, left + quarterWidth * 2 + gap, menuRowTop + 24};
    layout.addParagraphBlockBtn =
        RECT{left + quarterWidth * 2 + gap * 2, menuRowTop, left + quarterWidth * 3 + gap * 2, menuRowTop + 24};
    layout.addSpacerBlockBtn = RECT{left + quarterWidth * 3 + gap * 3, menuRowTop, right, menuRowTop + 24};
    const int flowRowTop = menuRowTop + 30;
    const int halfWidth = (right - left - gap) / 2;
    layout.addChoiceBlockBtn = RECT{left, flowRowTop, left + halfWidth, flowRowTop + 24};
    layout.setStartScreenBtn = RECT{left + halfWidth + gap, flowRowTop, right, flowRowTop + 24};
    const int blockTop = flowRowTop + 34;
    layout.addDialogueBlockBtn = RECT{left, blockTop, left + halfWidth, blockTop + 24};
    layout.addNarrationBlockBtn = RECT{left + halfWidth + gap, blockTop, right, blockTop + 24};
    layout.moveBlockUpBtn = RECT{left, blockTop + 30, left + halfWidth, blockTop + 54};
    layout.moveBlockDownBtn = RECT{left + halfWidth + gap, blockTop + 30, right, blockTop + 54};
    layout.deleteBlockBtn = RECT{left, blockTop + 60, right, blockTop + 84};
    return layout;
}

UiWorkbenchInspectorLayout ComputeUiWorkbenchInspectorLayout(const RECT& inspectorInner) {
    UiWorkbenchInspectorLayout layout{};
    layout.toolbar = ComputeUiWorkbenchLayout(inspectorInner);
    const int manifestTop = layout.toolbar.deleteBlockBtn.bottom + 10;
    const int manifestBottom = std::min<int>(manifestTop + 76, InspectorContentBottom(inspectorInner) - 10);
    layout.manifestCard = RECT{inspectorInner.left + 10, manifestTop, inspectorInner.right - 10, manifestBottom};
    layout.railCard = RECT{inspectorInner.left + 10,
                           layout.manifestCard.bottom + 10,
                           inspectorInner.right - 10,
                           std::min<int>(layout.manifestCard.bottom + 120, InspectorContentBottom(inspectorInner) - 10)};
    layout.previewCard = RECT{inspectorInner.left + 10,
                              layout.railCard.bottom + 10,
                              inspectorInner.right - 10,
                              InspectorContentBottom(inspectorInner) - 10};
    layout.stageRect = RECT{layout.previewCard.left + 14,
                            layout.previewCard.top + 58,
                            layout.previewCard.right - 14,
                            layout.previewCard.bottom - 32};
    return layout;
}

std::vector<RECT> ComputeUiWorkbenchScreenRowRects(const UiWorkbenchInspectorLayout& layout, const int screenCount) {
    std::vector<RECT> rects{};
    rects.reserve(static_cast<std::size_t>(std::max(0, screenCount)));
    int rowTop = layout.railCard.top + 34;
    for (int i = 0; i < screenCount; ++i) {
        RECT rowRect{layout.railCard.left + 10, rowTop, layout.railCard.right - 10, rowTop + 22};
        if (rowRect.bottom > layout.railCard.bottom - 28) {
            break;
        }
        rects.push_back(rowRect);
        rowTop += 24;
    }
    return rects;
}

std::vector<RECT> ComputeUiWorkbenchInspectorPreviewBlockRects(
    const UiWorkbenchInspectorLayout& layout,
    const std::vector<UiWorkbenchPreviewBlock>& blocks) {
    std::vector<RECT> rects{};
    rects.reserve(blocks.size());
    int blockTop = layout.stageRect.top + 12;
    for (const UiWorkbenchPreviewBlock& block : blocks) {
        int blockHeight = block.preferredHeight > 0 ? block.preferredHeight : (block.detailLine.empty() ? 28 : 44);
        if (block.tone == UiWorkbenchBlockTone::Button) {
            blockHeight = std::max(blockHeight, 40);
        }
        RECT blockRect{layout.stageRect.left + 12, blockTop, layout.stageRect.right - 12, blockTop + blockHeight};
        if (blockRect.bottom > layout.stageRect.bottom - 8) {
            break;
        }
        rects.push_back(blockRect);
        blockTop += blockHeight + 8;
    }
    return rects;
}

UiWorkbenchViewportLayout ComputeUiWorkbenchViewportLayout(const RECT& viewportInner) {
    UiWorkbenchViewportLayout layout{};
    layout.headerRect = RECT{viewportInner.left + 18, viewportInner.top + 16, viewportInner.right - 18, viewportInner.top + 54};
    const int innerWidth = std::max(0, static_cast<int>(viewportInner.right - viewportInner.left - 36));
    constexpr int kShelfWidth = 194;
    constexpr int kMinStageWidth = 240;
    layout.stackLayout = innerWidth < kShelfWidth + kMinStageWidth + 24;
    layout.shelfRect = RECT{viewportInner.left + 18,
                            layout.headerRect.bottom + 10,
                            layout.stackLayout ? viewportInner.right - 18 : viewportInner.left + 18 + kShelfWidth,
                            layout.stackLayout ? layout.headerRect.bottom + 10 + 180 : viewportInner.bottom - 18};
    layout.stageCard = RECT{layout.stackLayout ? viewportInner.left + 18 : layout.shelfRect.right + 12,
                            layout.stackLayout ? layout.shelfRect.bottom + 10 : layout.headerRect.bottom + 10,
                            viewportInner.right - 18,
                            viewportInner.bottom - 18};
    layout.stageRect = RECT{layout.stageCard.left + 18,
                            layout.stageCard.top + 52,
                            layout.stageCard.right - 18,
                            layout.stageCard.bottom - 18};
    return layout;
}

std::vector<RECT> ComputeUiWorkbenchViewportBlockRects(const UiWorkbenchViewportLayout& layout,
                                                       const std::vector<UiWorkbenchPreviewBlock>& blocks) {
    std::vector<RECT> rects{};
    rects.reserve(blocks.size());
    int blockTop = layout.stageRect.top + 14;
    for (const UiWorkbenchPreviewBlock& block : blocks) {
        int blockHeight = block.preferredHeight > 0 ? block.preferredHeight : (block.detailLine.empty() ? 34 : 56);
        if (block.tone == UiWorkbenchBlockTone::Button) {
            blockHeight = std::max(blockHeight, 48);
        }
        RECT blockRect{layout.stageRect.left + 16, blockTop, layout.stageRect.right - 16, blockTop + blockHeight};
        if (blockRect.bottom > layout.stageRect.bottom - 10) {
            break;
        }
        rects.push_back(blockRect);
        blockTop += blockHeight + 12;
    }
    return rects;
}

void RenderNodeInspectorPanel(HDC dc,
                              const RECT& inspectorInner,
                              const NodeInspectorPanelModel& model,
                              HFONT /*headerFont*/,
                              HFONT bodyFont,
                              HFONT smallFont,
                              const std::function<void(HDC, int&, const RECT&, const char*, int)>& drawNudgeRow,
                              const std::function<void(HDC, int&, const RECT&, const char*, int)>& drawMaterialNudgeRow,
                              const std::function<void(HDC, int&, const RECT&, const char*, int)>& drawLightNudgeRow) {
    int infoTop = inspectorInner.top + 42;
    if (model.renameTypingActive) {
        EditorRenderer::DrawTextLine(dc,
                                     RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 36},
                                     "Rename (F2): " + model.renameDraft + "  |  Enter apply  Esc cancel",
                                     RGB(255, 248, 180),
                                     bodyFont,
                                     DT_LEFT | DT_WORDBREAK);
        infoTop += 40;
    } else {
        EditorRenderer::DrawTextLine(dc,
                                     RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                                     model.nameLine,
                                     RGB(226, 226, 226),
                                     bodyFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        infoTop += 24;
    }
    const auto drawSmall = [&](const RECT& rect, const std::string& text, COLORREF color, HFONT font, UINT fmt) {
        EditorRenderer::DrawTextLine(dc,
                                     rect,
                                     text,
                                     color,
                                     font,
                                     fmt);
    };

    RECT identityCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 78};
    DrawInspectorCard(dc, identityCard, RGB(56, 62, 72), RGB(170, 176, 186), RGB(22, 26, 32), RGB(204, 145, 60));
    drawSmall(RECT{identityCard.left + 10, identityCard.top + 10, identityCard.right - 10, identityCard.top + 30},
              model.pathLine,
              RGB(228, 232, 238),
              bodyFont,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    drawSmall(RECT{identityCard.left + 10, identityCard.top + 34, identityCard.right - 10, identityCard.top + 52},
              model.kindLine,
              RGB(233, 220, 176),
              smallFont,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    if (model.meshPrimitiveLine.has_value()) {
        drawSmall(RECT{identityCard.left + 10, identityCard.top + 54, identityCard.right - 10, identityCard.bottom - 8},
                  *model.meshPrimitiveLine,
                  RGB(200, 210, 220),
                  smallFont,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
    infoTop = identityCard.bottom + 10;

    const int transformCardHeight = model.editableAuthored ? 144 : 92;
    RECT transformCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + transformCardHeight};
    DrawInspectorCard(dc, transformCard, RGB(53, 58, 68), RGB(162, 168, 176), RGB(20, 24, 30), RGB(124, 150, 198));
    drawSmall(RECT{transformCard.left + 10, transformCard.top + 10, transformCard.right - 10, transformCard.top + 28},
              model.localPosLine,
              RGB(224, 230, 238),
              smallFont,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    drawSmall(RECT{transformCard.left + 10, transformCard.top + 30, transformCard.right - 10, transformCard.top + 48},
              model.localRotLine,
              RGB(216, 222, 230),
              smallFont,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    drawSmall(RECT{transformCard.left + 10, transformCard.top + 50, transformCard.right - 10, transformCard.top + 68},
              model.localScaleLine,
              RGB(216, 222, 230),
              smallFont,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    drawSmall(RECT{transformCard.left + 10, transformCard.top + 70, transformCard.right - 10, transformCard.top + 88},
              model.worldPosLine,
              RGB(190, 224, 190),
              smallFont,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    if (model.editableAuthored) {
        EditorRenderer::DrawTextLine(dc,
                                     RECT{transformCard.left + 10, transformCard.top + 94, transformCard.right - 10, transformCard.top + 112},
                                     "Transform nudge",
                                     RGB(214, 208, 168),
                                     smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        infoTop = transformCard.top + 114;
        drawNudgeRow(dc, infoTop, inspectorInner, "Pos", 0);
        drawNudgeRow(dc, infoTop, inspectorInner, "Rot", 1);
        drawNudgeRow(dc, infoTop, inspectorInner, "Scl", 2);
        infoTop = transformCard.bottom + 10;
    } else {
        EditorRenderer::DrawTextLine(dc,
                                     RECT{transformCard.left + 10, transformCard.top + 94, transformCard.right - 10, transformCard.bottom - 8},
                                     "Protected node - use viewport keys (T/R/U) on authored meshes only.",
                                     RGB(214, 180, 140),
                                     smallFont,
                                     DT_LEFT | DT_WORDBREAK);
        infoTop = transformCard.bottom + 10;
    }

    if (model.hasMaterial) {
        const int materialCardHeight = model.materialEditable ? 156 : 118;
        RECT materialCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + materialCardHeight};
        DrawInspectorCard(dc, materialCard, RGB(52, 58, 66), RGB(156, 168, 182), RGB(20, 24, 30), RGB(118, 168, 128));
        drawSmall(RECT{materialCard.left + 10, materialCard.top + 8, materialCard.right - 10, materialCard.top + 26},
                  "Material",
                  RGB(214, 232, 210),
                  bodyFont,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        drawSmall(RECT{materialCard.left + 10, materialCard.top + 28, materialCard.right - 10, materialCard.top + 44},
                  model.materialNameLine,
                  RGB(228, 232, 238),
                  smallFont,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        drawSmall(RECT{materialCard.left + 10, materialCard.top + 46, materialCard.right - 10, materialCard.top + 62},
                  model.materialColorLine,
                  RGB(216, 222, 230),
                  smallFont,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        drawSmall(RECT{materialCard.left + 10, materialCard.top + 64, materialCard.right - 10, materialCard.top + 80},
                  model.materialRoughnessLine + "  |  " + model.materialMetallicLine,
                  RGB(216, 222, 230),
                  smallFont,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        drawSmall(RECT{materialCard.left + 10, materialCard.top + 82, materialCard.right - 10, materialCard.top + 98},
                  model.materialOpacityLine,
                  RGB(216, 222, 230),
                  smallFont,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        drawSmall(RECT{materialCard.left + 10, materialCard.top + 100, materialCard.right - 10, materialCard.top + 116},
                  model.materialTextureLine,
                  RGB(200, 210, 220),
                  smallFont,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        if (model.materialEditable) {
            int nudgeTop = materialCard.top + 120;
            drawMaterialNudgeRow(dc, nudgeTop, inspectorInner, "Rgh", 0);
            drawMaterialNudgeRow(dc, nudgeTop, inspectorInner, "Met", 1);
            drawMaterialNudgeRow(dc, nudgeTop, inspectorInner, "Opa", 2);
            drawSmall(RECT{materialCard.left + 10, nudgeTop, materialCard.right - 10, materialCard.bottom - 6},
                      model.materialFlagsLine,
                      RGB(200, 208, 188),
                      smallFont,
                      DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        } else {
            drawSmall(RECT{materialCard.left + 10, materialCard.top + 118, materialCard.right - 10, materialCard.bottom - 6},
                      model.materialFlagsLine,
                      RGB(200, 208, 188),
                      smallFont,
                      DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
        infoTop = materialCard.bottom + 10;
    }

    if (model.hasLight) {
        const int lightCardHeight = model.lightEditable ? 118 : 92;
        RECT lightCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + lightCardHeight};
        DrawInspectorCard(dc, lightCard, RGB(58, 54, 48), RGB(176, 168, 150), RGB(24, 22, 20), RGB(204, 168, 96));
        drawSmall(RECT{lightCard.left + 10, lightCard.top + 8, lightCard.right - 10, lightCard.top + 26},
                  "Light",
                  RGB(255, 236, 196),
                  bodyFont,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        drawSmall(RECT{lightCard.left + 10, lightCard.top + 28, lightCard.right - 10, lightCard.top + 44},
                  model.lightTypeLine,
                  RGB(228, 232, 238),
                  smallFont,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        drawSmall(RECT{lightCard.left + 10, lightCard.top + 46, lightCard.right - 10, lightCard.top + 62},
                  model.lightColorLine,
                  RGB(216, 222, 230),
                  smallFont,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        drawSmall(RECT{lightCard.left + 10, lightCard.top + 64, lightCard.right - 10, lightCard.top + 80},
                  model.lightIntensityLine + "  |  " + model.lightRangeLine,
                  RGB(216, 222, 230),
                  smallFont,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        if (model.lightEditable) {
            int nudgeTop = lightCard.top + 86;
            drawLightNudgeRow(dc, nudgeTop, inspectorInner, "Int", 0);
        }
        infoTop = lightCard.bottom + 10;
    }

    if (model.hasTrigger) {
        const RECT triggerCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 88};
        DrawInspectorCard(dc, triggerCard, RGB(48, 58, 52), RGB(150, 176, 158), RGB(20, 24, 22), RGB(96, 176, 118));
        drawSmall(RECT{triggerCard.left + 10, triggerCard.top + 8, triggerCard.right - 10, triggerCard.top + 26},
                  "Trigger volume",
                  RGB(210, 255, 220),
                  bodyFont,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        drawSmall(RECT{triggerCard.left + 10, triggerCard.top + 30, triggerCard.right - 10, triggerCard.top + 48},
                  model.triggerBoundsLine,
                  RGB(216, 222, 230),
                  smallFont,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        drawSmall(RECT{triggerCard.left + 10, triggerCard.top + 52, triggerCard.right - 10, triggerCard.bottom - 8},
                  model.triggerHelpLine,
                  RGB(200, 220, 204),
                  smallFont,
                  DT_LEFT | DT_WORDBREAK);
        infoTop = triggerCard.bottom + 10;
    }

    RECT actionCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 74};
    DrawInspectorCard(dc, actionCard, RGB(55, 60, 70), RGB(156, 162, 170), RGB(22, 24, 30), RGB(108, 116, 132));
    drawSmall(RECT{actionCard.left + 10, actionCard.top + 10, actionCard.right - 10, actionCard.top + 28},
              model.editModeLine,
              RGB(228, 220, 182),
              smallFont,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    drawSmall(RECT{actionCard.left + 10, actionCard.top + 30, actionCard.right - 10, actionCard.top + 48},
              model.groupingLine,
              RGB(214, 208, 168),
              smallFont,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    drawSmall(RECT{actionCard.left + 10, actionCard.top + 50, actionCard.right - 10, actionCard.bottom - 8},
              model.opsLine,
              RGB(214, 208, 168),
              smallFont,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

void RenderBrushInspectorPanel(HDC dc,
                               const RECT& inspectorInner,
                               const BrushInspectorPanelModel& model,
                               HFONT headerFont,
                               HFONT bodyFont,
                               HFONT smallFont,
                               const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton) {
    const BrushPanelLayout brushLayout = ComputeBrushPanelLayout(inspectorInner);
    RECT presetCard{inspectorInner.left + 10, inspectorInner.top + 42, inspectorInner.right - 10, inspectorInner.top + 76};
    DrawInspectorCard(dc, presetCard, RGB(54, 60, 70), RGB(164, 170, 180), RGB(22, 26, 32), RGB(204, 145, 60));
    drawToolbarButton(dc, brushLayout.presetPrevBtn, "<", false);
    drawToolbarButton(dc, brushLayout.presetNextBtn, ">", false);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{presetCard.left + 104, presetCard.top + 8, presetCard.right - 10, presetCard.bottom - 8},
                                 model.presetTitleLine,
                                 RGB(228, 236, 248),
                                 bodyFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    int infoTop = presetCard.bottom + 10;
    auto draw = [&](const std::string& text, COLORREF color, HFONT font, int height, UINT fmt) {
        EditorRenderer::DrawTextLine(dc,
                                     RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + height},
                                     text,
                                     color,
                                     font,
                                     fmt);
        infoTop += (height >= 40 ? 52 : (height >= 36 ? 42 : 20));
    };
    RECT overviewCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 150};
    DrawInspectorCard(dc, overviewCard, RGB(54, 59, 69), RGB(162, 168, 176), RGB(22, 24, 30), RGB(96, 134, 188));
    infoTop = overviewCard.top + 10;
    draw(model.headingLine, RGB(240, 240, 236), headerFont, 20, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    infoTop += 4;
    draw(model.helpLineA, RGB(210, 215, 222), smallFont, 36, DT_LEFT | DT_WORDBREAK);
    draw(model.helpLineB, RGB(200, 206, 214), smallFont, 44, DT_LEFT | DT_WORDBREAK);
    draw(model.selectionLine, RGB(226, 226, 226), smallFont, 20, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    infoTop += 2;
    draw(model.meshAttachedLine, RGB(200, 200, 200), smallFont, 20, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    infoTop = overviewCard.bottom + 10;
    RECT boundsCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 54};
    DrawInspectorCard(dc, boundsCard, RGB(52, 58, 66), RGB(154, 162, 172), RGB(20, 22, 28), RGB(106, 154, 122));
    EditorRenderer::DrawTextLine(dc,
                                 RECT{boundsCard.left + 10, boundsCard.top + 10, boundsCard.right - 10, boundsCard.top + 28},
                                 model.boundsSizeLine,
                                 RGB(208, 228, 208),
                                 smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{boundsCard.left + 10, boundsCard.top + 28, boundsCard.right - 10, boundsCard.bottom - 8},
                                 model.boundsCenterLine,
                                 RGB(208, 228, 208),
                                 smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    if (model.hasStructuralMetadata) {
        infoTop = boundsCard.bottom + 10;
        RECT semanticCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 104};
        DrawInspectorCard(dc,
                          semanticCard,
                          RGB(48, 58, 62),
                          RGB(146, 170, 174),
                          RGB(18, 24, 26),
                          model.structuralMetadataValid ? RGB(92, 170, 126) : RGB(214, 118, 68));
        EditorRenderer::DrawTextLine(dc,
                                     RECT{semanticCard.left + 10, semanticCard.top + 8,
                                          semanticCard.right - 10, semanticCard.top + 26},
                                     "Structural M/P/Q/I",
                                     RGB(216, 238, 232),
                                     bodyFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        EditorRenderer::DrawTextLine(dc,
                                     RECT{semanticCard.left + 10, semanticCard.top + 28,
                                          semanticCard.right - 10, semanticCard.top + 44},
                                     model.semanticIdentityLine,
                                     RGB(212, 224, 230),
                                     smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        EditorRenderer::DrawTextLine(dc,
                                     RECT{semanticCard.left + 10, semanticCard.top + 46,
                                          semanticCard.right - 10, semanticCard.top + 62},
                                     model.semanticPolicyLine,
                                     RGB(204, 218, 224),
                                     smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        EditorRenderer::DrawTextLine(dc,
                                     RECT{semanticCard.left + 10, semanticCard.top + 64,
                                          semanticCard.right - 10, semanticCard.top + 80},
                                     model.semanticChannelsLine,
                                     RGB(196, 214, 220),
                                     smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        EditorRenderer::DrawTextLine(dc,
                                     RECT{semanticCard.left + 10, semanticCard.top + 82,
                                          semanticCard.right - 10, semanticCard.bottom - 6},
                                     model.semanticValidationLine,
                                     model.structuralMetadataValid ? RGB(156, 226, 172) : RGB(255, 174, 118),
                                     smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
}

void RenderPluginStorePanel(HDC dc,
                            const RECT& inspectorInner,
                            PluginStorePanelModel& model,
                            HFONT headerFont,
                            HFONT bodyFont,
                            HFONT smallFont,
                            const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton) {
    model.layout = ComputePluginStoreLayout(
        inspectorInner, static_cast<int>(model.cards.size()), model.scrollTopRow);

    int infoTop = inspectorInner.top + 42;
    EditorRenderer::DrawTextLine(dc,
                                 RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                                 model.headingLine,
                                 RGB(240, 244, 252),
                                 headerFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    infoTop += 22;
    EditorRenderer::DrawTextLine(dc,
                                 RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 18},
                                 model.summaryLine,
                                 RGB(196, 210, 228),
                                 smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    infoTop += 20;
    EditorRenderer::DrawTextLine(dc,
                                 RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 16},
                                 model.modelHelpLine,
                                 RGB(170, 188, 210),
                                 smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    drawToolbarButton(dc, model.layout.refreshBtn, "Refresh", false);
    drawToolbarButton(dc, model.layout.openFolderBtn, "Open Store", false);
    const bool canScrollPrev = model.layout.scrollTopRow > 0;
    const bool canScrollNext =
        model.layout.scrollTopRow + model.layout.visibleCards < model.layout.totalCards;
    drawToolbarButton(dc, model.layout.scrollPrevBtn, "Prev", canScrollPrev);
    drawToolbarButton(dc, model.layout.scrollNextBtn, "Next", canScrollNext);

    const std::size_t visibleCards = std::min(model.layout.cardRects.size(), model.layout.actionBtns.size());
    const std::size_t boundedVisible = std::min(
        visibleCards,
        std::min(model.layout.cardPackageIndices.size(), model.layout.secondaryActionBtns.size()));
    for (std::size_t visibleIndex = 0; visibleIndex < boundedVisible; ++visibleIndex) {
        const int packageIndex = model.layout.cardPackageIndices[visibleIndex];
        if (packageIndex < 0 || packageIndex >= static_cast<int>(model.cards.size())) {
            continue;
        }
        const PluginStoreCardModel& card = model.cards[static_cast<std::size_t>(packageIndex)];
        const RECT cardRect = model.layout.cardRects[visibleIndex];
        const COLORREF accent = card.installed ? RGB(92, 170, 120) : RGB(120, 150, 210);
        DrawInspectorCard(dc, cardRect, RGB(48, 54, 66), RGB(150, 158, 172), RGB(18, 22, 28), accent);

        EditorRenderer::DrawTextLine(dc,
                                     RECT{cardRect.left + 10, cardRect.top + 8, cardRect.right - 100, cardRect.top + 26},
                                     card.titleLine,
                                     RGB(236, 240, 248),
                                     bodyFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        EditorRenderer::DrawTextLine(dc,
                                     RECT{cardRect.left + 10, cardRect.top + 28, cardRect.right - 100, cardRect.top + 44},
                                     card.metaLine,
                                     RGB(180, 196, 214),
                                     smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        EditorRenderer::DrawTextLine(dc,
                                     RECT{cardRect.left + 10, cardRect.top + 46, cardRect.right - 10, cardRect.top + 62},
                                     card.tagLine,
                                     RGB(210, 188, 140),
                                     smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        EditorRenderer::DrawTextLine(dc,
                                     RECT{cardRect.left + 10, cardRect.top + 64, cardRect.right - 10, cardRect.bottom - 34},
                                     card.descriptionLine,
                                     RGB(200, 206, 214),
                                     smallFont,
                                     DT_LEFT | DT_WORDBREAK);
        if (!card.policyLine.empty()) {
            EditorRenderer::DrawTextLine(dc,
                                         RECT{cardRect.left + 10, cardRect.bottom - 46, cardRect.right - 100, cardRect.bottom - 30},
                                         card.policyLine,
                                         card.blocked ? RGB(232, 164, 132) : RGB(156, 196, 214),
                                         smallFont,
                                         DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
        if (!card.statusLine.empty()) {
            EditorRenderer::DrawTextLine(dc,
                                         RECT{cardRect.left + 10, cardRect.bottom - 30, cardRect.right - 100, cardRect.bottom - 10},
                                         card.statusLine,
                                         card.blocked ? RGB(232, 164, 132) : RGB(150, 210, 160),
                                         smallFont,
                                         DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
        const bool actionActive = !card.blocked && card.installed && card.enabled;
        drawToolbarButton(dc, model.layout.actionBtns[visibleIndex], card.actionLabel, actionActive);
        if (card.installed && !card.secondaryActionLabel.empty()) {
            drawToolbarButton(
                dc, model.layout.secondaryActionBtns[visibleIndex], card.secondaryActionLabel, false);
        }
    }

    if (!model.scrollLine.empty()) {
        EditorRenderer::DrawTextLine(dc,
                                     RECT{inspectorInner.left + 10, model.layout.scrollPrevBtn.bottom + 6,
                                          inspectorInner.right - 10, model.layout.scrollPrevBtn.bottom + 22},
                                     model.scrollLine,
                                     RGB(150, 166, 188),
                                     smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }

    const int footerTop = InspectorContentBottom(inspectorInner) - 52;
    EditorRenderer::DrawTextLine(dc,
                                 RECT{inspectorInner.left + 10, footerTop, inspectorInner.right - 10, footerTop + 16},
                                 model.storePathLine,
                                 RGB(150, 158, 170),
                                 smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (!model.statusLine.empty()) {
        EditorRenderer::DrawTextLine(dc,
                                     RECT{inspectorInner.left + 10, footerTop + 18, inspectorInner.right - 10, footerTop + 34},
                                     model.statusLine,
                                     RGB(180, 220, 180),
                                     smallFont,
                                     DT_LEFT | DT_WORDBREAK);
    }
    if (!model.hasMountedGame) {
        EditorRenderer::DrawTextLine(dc,
                                     RECT{inspectorInner.left + 10, footerTop - 20, inspectorInner.right - 10, footerTop - 2},
                                     "Open a game from the workspace strip to install plugins.",
                                     RGB(220, 180, 120),
                                     smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    }
}

void RenderGameplayInspectorPanel(HDC dc,
                                  const RECT& inspectorInner,
                                  const GameplayInspectorPanelModel& model,
                                  HFONT headerFont,
                                  HFONT bodyFont,
                                  HFONT smallFont,
                                  const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton) {
    int infoTop = inspectorInner.top + 42;
    EditorRenderer::DrawTextLine(dc,
                                 RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                                 model.headingLine,
                                 RGB(240, 240, 236),
                                 headerFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    infoTop += 24;
    RECT gameplayCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 88};
    DrawInspectorCard(dc, gameplayCard, RGB(70, 88, 84), RGB(154, 196, 188), RGB(24, 34, 34), RGB(92, 170, 146));
    EditorRenderer::DrawTextLine(dc,
                                 RECT{gameplayCard.left + 10, gameplayCard.top + 8, gameplayCard.right - 10, gameplayCard.top + 26},
                                 "Playtest-ready authoring",
                                 RGB(244, 250, 246),
                                 headerFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{gameplayCard.left + 10, gameplayCard.top + 30, gameplayCard.right - 10, gameplayCard.bottom - 8},
                                 model.summaryLine,
                                 RGB(216, 236, 228),
                                 smallFont,
                                 DT_LEFT | DT_WORDBREAK);
    RECT policyCard{inspectorInner.left + 10, gameplayCard.bottom + 10, inspectorInner.right - 10, gameplayCard.bottom + 102};
    DrawInspectorCard(dc, policyCard, RGB(55, 63, 68), RGB(160, 176, 180), RGB(24, 28, 30), RGB(122, 150, 160));
    EditorRenderer::DrawTextLine(dc,
                                 model.layout.inventoryModeRow,
                                 model.inventoryModeLine,
                                 RGB(226, 226, 226),
                                 bodyFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    EditorRenderer::DrawTextLine(dc,
                                 model.layout.offHandRow,
                                 model.offHandLine,
                                 RGB(200, 220, 200),
                                 smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{inspectorInner.left + 10,
                                      model.layout.offHandRow.bottom + 4,
                                      inspectorInner.right - 10,
                                      model.layout.offHandRow.bottom + 24},
                                 model.gameplayStorageLine,
                                 RGB(200, 220, 200),
                                 smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{inspectorInner.left + 10,
                                      model.layout.offHandRow.bottom + 24,
                                      inspectorInner.right - 10,
                                      model.layout.offHandRow.bottom + 44},
                                 model.hotbarLine,
                                 RGB(200, 200, 200),
                                 smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    RECT actionCard{inspectorInner.left + 10, policyCard.bottom + 10, inspectorInner.right - 10, policyCard.bottom + 82};
    DrawInspectorCard(dc, actionCard, RGB(56, 60, 70), RGB(158, 164, 172), RGB(20, 24, 30), RGB(204, 145, 60));
    drawToolbarButton(dc, model.layout.addTriggerBtn, "+ Trigger", false);
    drawToolbarButton(dc, model.layout.exportBtn, "Export", false);
    drawToolbarButton(dc, model.layout.playtestBtn, "Playtest", false);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{inspectorInner.left + 10,
                                      model.layout.playtestBtn.bottom + 8,
                                      inspectorInner.right - 10,
                                      model.layout.playtestBtn.bottom + 50},
                                 model.controlsLine,
                                 RGB(214, 208, 168),
                                 smallFont,
                                 DT_LEFT | DT_WORDBREAK);
}

void RenderUiWorkbenchPanel(HDC dc,
                            const RECT& inspectorInner,
                            const UiWorkbenchPanelModel& model,
                            HFONT headerFont,
                            HFONT bodyFont,
                            HFONT smallFont,
                            const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton) {
    int infoTop = inspectorInner.top + 48;
    EditorRenderer::DrawTextLine(dc,
                                 RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                                 model.headingLine,
                                 RGB(240, 240, 236),
                                 headerFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    infoTop += 22;
    EditorRenderer::DrawTextLine(dc,
                                 RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 18},
                                 model.sourceLine,
                                 RGB(212, 218, 226),
                                 smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    drawToolbarButton(dc, model.layout.prevScreenBtn, "<", false);
    drawToolbarButton(dc, model.layout.nextScreenBtn, ">", false);
    drawToolbarButton(dc, model.layout.useAutoBtn, "Auto", model.usingAutoSource);
    drawToolbarButton(dc, model.layout.useMenuSampleBtn, "Demo Menu", model.usingMenuSample);
    drawToolbarButton(dc, model.layout.useVnSampleBtn, "Demo VN", model.usingVnSample);
    drawToolbarButton(dc, model.layout.newScreenBtn, "New Screen", false);
    drawToolbarButton(dc, model.layout.newMenuScreenBtn, "New Menu", false);
    drawToolbarButton(dc, model.layout.duplicateScreenBtn, "Duplicate", false);
    drawToolbarButton(dc, model.layout.addButtonBlockBtn, "+ Button", false);
    drawToolbarButton(dc, model.layout.addHeadingBlockBtn, "+ Heading", false);
    drawToolbarButton(dc, model.layout.addParagraphBlockBtn, "+ Paragraph", false);
    drawToolbarButton(dc, model.layout.addSpacerBlockBtn, "+ Spacer", false);
    drawToolbarButton(dc, model.layout.addChoiceBlockBtn, "Add Choices", false);
    drawToolbarButton(dc, model.layout.setStartScreenBtn, "Set Start", false);
    drawToolbarButton(dc, model.layout.addDialogueBlockBtn, "Add Dialogue", false);
    drawToolbarButton(dc, model.layout.addNarrationBlockBtn, "Add Narration", false);
    drawToolbarButton(dc, model.layout.moveBlockUpBtn, "Move Block Up", false);
    drawToolbarButton(dc, model.layout.moveBlockDownBtn, "Move Block Down", false);
    drawToolbarButton(dc, model.layout.deleteBlockBtn, "Delete Selected Block", false);

    const UiWorkbenchInspectorLayout cards = ComputeUiWorkbenchInspectorLayout(inspectorInner);
    const RECT manifestCard = cards.manifestCard;
    const RECT railCard = cards.railCard;
    const RECT previewCard = cards.previewCard;
    const RECT stageRect = cards.stageRect;
    DrawInspectorCard(dc, manifestCard, RGB(55, 60, 70), RGB(164, 170, 178), RGB(22, 24, 30), RGB(204, 145, 60));
    EditorRenderer::DrawTextLine(dc,
                                 RECT{manifestCard.left + 10, manifestCard.top + 10, manifestCard.right - 10, manifestCard.top + 28},
                                 model.statusLine,
                                 model.manifestParsed ? RGB(228, 236, 244) : RGB(255, 198, 142),
                                 bodyFont,
                                 DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{manifestCard.left + 10, manifestCard.top + 32, manifestCard.right - 10, manifestCard.top + 50},
                                 model.hintLine,
                                 RGB(210, 214, 220),
                                 smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (!model.errorLine.empty()) {
        EditorRenderer::DrawTextLine(dc,
                                     RECT{manifestCard.left + 10, manifestCard.top + 50, manifestCard.right - 10, manifestCard.bottom - 8},
                                     model.errorLine,
                                     RGB(230, 170, 132),
                                     smallFont,
                                     DT_LEFT | DT_WORDBREAK);
    }

    DrawInspectorCard(dc, railCard, RGB(54, 58, 67), RGB(158, 164, 172), RGB(20, 22, 28), RGB(96, 134, 188));
    EditorRenderer::DrawTextLine(dc,
                                 RECT{railCard.left + 10, railCard.top + 10, railCard.right - 10, railCard.top + 28},
                                 model.screenHeaderLine,
                                 RGB(236, 240, 245),
                                 headerFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    int rowTop = railCard.top + 34;
    for (const UiWorkbenchScreenSummary& screen : model.screens) {
        RECT rowRect{railCard.left + 10, rowTop, railCard.right - 10, rowTop + 22};
        if (screen.selected) {
            EditorRenderer::FillRectColor(dc, rowRect, RGB(92, 74, 38));
        }
        EditorRenderer::DrawTextLine(dc,
                                     RECT{rowRect.left + 8, rowRect.top, rowRect.right - 140, rowRect.bottom},
                                     screen.titleLine,
                                     screen.selected ? RGB(255, 244, 208) : RGB(228, 232, 238),
                                     smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        EditorRenderer::DrawTextLine(dc,
                                     RECT{rowRect.left + 150, rowRect.top, rowRect.right - 8, rowRect.bottom},
                                     screen.metaLine,
                                     screen.selected ? RGB(232, 220, 178) : RGB(182, 190, 202),
                                     smallFont,
                                     DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        rowTop += 24;
        if (rowTop > railCard.bottom - 28) {
            break;
        }
    }

    DrawInspectorCard(dc, previewCard, RGB(20, 22, 30), RGB(108, 116, 132), RGB(14, 16, 22), RGB(204, 145, 60));
    EditorRenderer::DrawTextLine(dc,
                                 RECT{previewCard.left + 14, previewCard.top + 10, previewCard.right - 14, previewCard.top + 28},
                                 model.previewTitleLine,
                                 RGB(248, 242, 220),
                                 headerFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{previewCard.left + 14, previewCard.top + 32, previewCard.right - 14, previewCard.top + 50},
                                 model.previewMetaLine,
                                 RGB(196, 204, 216),
                                 smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    EditorRenderer::DrawInsetFrame(dc, stageRect, RGB(16, 18, 24), RGB(96, 104, 118), RGB(12, 14, 20));
    EditorRenderer::FillRectColor(dc,
                                  RECT{stageRect.left + 1, stageRect.top + 1, stageRect.right - 1, stageRect.top + 5},
                                  RGB(214, 150, 56));

    int blockTop = stageRect.top + 12;
    for (const UiWorkbenchPreviewBlock& block : model.previewBlocks) {
        const int blockHeight = block.detailLine.empty() ? 28 : 44;
        RECT blockRect{stageRect.left + 12, blockTop, stageRect.right - 12, blockTop + blockHeight};
        COLORREF fill = RGB(38, 42, 52);
        COLORREF accent = RGB(118, 126, 144);
        COLORREF text = RGB(230, 234, 240);
        switch (block.tone) {
            case UiWorkbenchBlockTone::Heading:
                fill = RGB(60, 48, 24);
                accent = RGB(214, 150, 56);
                text = RGB(255, 244, 210);
                break;
            case UiWorkbenchBlockTone::Say:
                fill = RGB(32, 44, 64);
                accent = RGB(102, 162, 218);
                break;
            case UiWorkbenchBlockTone::Narration:
                fill = RGB(44, 44, 52);
                accent = RGB(142, 146, 164);
                break;
            case UiWorkbenchBlockTone::Choices:
                fill = RGB(54, 42, 26);
                accent = RGB(204, 145, 60);
                break;
            case UiWorkbenchBlockTone::Button:
                fill = RGB(34, 58, 44);
                accent = RGB(118, 176, 128);
                break;
            case UiWorkbenchBlockTone::Image:
                fill = RGB(34, 58, 50);
                accent = RGB(102, 176, 148);
                break;
            case UiWorkbenchBlockTone::Note:
                fill = RGB(46, 36, 56);
                accent = RGB(162, 114, 198);
                break;
            case UiWorkbenchBlockTone::Other:
                break;
        }
        if (block.selected) {
            fill = RGB(92, 74, 38);
            accent = RGB(255, 196, 96);
        }
        DrawInspectorCard(dc, blockRect, fill, RGB(110, 118, 130), RGB(16, 18, 24), accent);
        EditorRenderer::DrawTextLine(dc,
                                     RECT{blockRect.left + 10, blockRect.top + 8, blockRect.right - 10, blockRect.top + 24},
                                     block.titleLine,
                                     text,
                                     smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        if (!block.detailLine.empty()) {
            EditorRenderer::DrawTextLine(dc,
                                         RECT{blockRect.left + 10, blockRect.top + 24, blockRect.right - 10, blockRect.bottom - 8},
                                         block.detailLine,
                                         RGB(198, 204, 214),
                                         smallFont,
                                         DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        blockTop += blockHeight + 8;
        if (blockTop > stageRect.bottom - 28) {
            break;
        }
    }

    EditorRenderer::DrawTextLine(dc,
                                 RECT{previewCard.left + 14, previewCard.bottom - 24, previewCard.right - 14, previewCard.bottom - 8},
                                 model.previewFooterLine,
                                 RGB(194, 198, 206),
                                 smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
}
#endif

} // namespace ri::editor
