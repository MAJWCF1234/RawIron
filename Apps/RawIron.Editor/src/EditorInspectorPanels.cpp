#include "EditorInspectorPanels.h"

#include "EditorRenderer.h"

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
    int infoTop = inspectorInner.top + 42;
    infoTop += 24;
    infoTop += 96;
    infoTop += 12;
    infoTop += 92;
    infoTop += 12;
    layout.inventoryModeRow =
        RECT{inspectorInner.left + 18, infoTop, inspectorInner.right - 18, infoTop + 20};
    infoTop += 24;
    layout.offHandRow =
        RECT{inspectorInner.left + 18, infoTop, inspectorInner.right - 18, infoTop + 20};
    infoTop += 24;
    infoTop += 22;
    infoTop += 22;
    infoTop += 18;
    layout.addTriggerBtn =
        RECT{inspectorInner.left + 18, infoTop, inspectorInner.left + 132, infoTop + 28};
    layout.exportBtn =
        RECT{inspectorInner.left + 138, infoTop, inspectorInner.left + 248, infoTop + 28};
    layout.playtestBtn =
        RECT{inspectorInner.left + 254, infoTop, inspectorInner.right - 18, infoTop + 28};
    return layout;
}

UiWorkbenchLayout ComputeUiWorkbenchLayout(const RECT& inspectorInner) {
    UiWorkbenchLayout layout{};
    const int top = inspectorInner.top + 74;
    layout.prevScreenBtn = RECT{inspectorInner.left + 10, top, inspectorInner.left + 52, top + 24};
    layout.nextScreenBtn = RECT{inspectorInner.left + 56, top, inspectorInner.left + 98, top + 24};
    layout.useAutoBtn = RECT{inspectorInner.left + 112, top, inspectorInner.left + 188, top + 24};
    layout.useMenuSampleBtn = RECT{inspectorInner.left + 194, top, inspectorInner.left + 300, top + 24};
    layout.useVnSampleBtn = RECT{inspectorInner.left + 306, top, inspectorInner.right - 10, top + 24};
    const int actionTop = top + 34;
    const int left = inspectorInner.left + 10;
    const int right = inspectorInner.right - 10;
    const int gap = 6;
    const int width = (right - left - gap) / 2;
    layout.newScreenBtn = RECT{left, actionTop, left + width, actionTop + 24};
    layout.duplicateScreenBtn = RECT{left + width + gap, actionTop, right, actionTop + 24};
    layout.addChoiceBlockBtn = RECT{left, actionTop + 30, left + width, actionTop + 54};
    layout.setStartScreenBtn = RECT{left + width + gap, actionTop + 30, right, actionTop + 54};
    const int blockTop = actionTop + 64;
    layout.addDialogueBlockBtn = RECT{left, blockTop, left + width, blockTop + 24};
    layout.addNarrationBlockBtn = RECT{left + width + gap, blockTop, right, blockTop + 24};
    layout.moveBlockUpBtn = RECT{left, blockTop + 30, left + width, blockTop + 54};
    layout.moveBlockDownBtn = RECT{left + width + gap, blockTop + 30, right, blockTop + 54};
    layout.deleteBlockBtn = RECT{left, blockTop + 60, right, blockTop + 84};
    return layout;
}

void RenderNodeInspectorPanel(HDC dc,
                              const RECT& inspectorInner,
                              const NodeInspectorPanelModel& model,
                              HFONT /*headerFont*/,
                              HFONT bodyFont,
                              HFONT smallFont,
                              const std::function<void(HDC, int&, const RECT&, const char*, int)>& drawNudgeRow) {
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
    RECT presetCard{inspectorInner.left + 10, inspectorInner.top + 42, inspectorInner.right - 10, inspectorInner.top + 76};
    DrawInspectorCard(dc, presetCard, RGB(54, 60, 70), RGB(164, 170, 180), RGB(22, 26, 32), RGB(204, 145, 60));
    drawToolbarButton(dc,
                      RECT{presetCard.left + 8, presetCard.top + 8, presetCard.left + 50, presetCard.top + 32},
                      "<",
                      false);
    drawToolbarButton(dc,
                      RECT{presetCard.left + 54, presetCard.top + 8, presetCard.left + 96, presetCard.top + 32},
                      ">",
                      false);
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
    drawToolbarButton(dc, model.layout.duplicateScreenBtn, "Duplicate", false);
    drawToolbarButton(dc, model.layout.addChoiceBlockBtn, "Add Choices", false);
    drawToolbarButton(dc, model.layout.setStartScreenBtn, "Set Start", false);
    drawToolbarButton(dc, model.layout.addDialogueBlockBtn, "Add Dialogue", false);
    drawToolbarButton(dc, model.layout.addNarrationBlockBtn, "Add Narration", false);
    drawToolbarButton(dc, model.layout.moveBlockUpBtn, "Move Block Up", false);
    drawToolbarButton(dc, model.layout.moveBlockDownBtn, "Move Block Down", false);
    drawToolbarButton(dc, model.layout.deleteBlockBtn, "Delete Selected Block", false);

    RECT manifestCard{inspectorInner.left + 10, inspectorInner.top + 258, inspectorInner.right - 10, inspectorInner.top + 334};
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

    RECT railCard{inspectorInner.left + 10, manifestCard.bottom + 10, inspectorInner.right - 10, manifestCard.bottom + 120};
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

    RECT previewCard{inspectorInner.left + 10, railCard.bottom + 10, inspectorInner.right - 10, inspectorInner.bottom - 72};
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

    RECT stageRect{previewCard.left + 14, previewCard.top + 58, previewCard.right - 14, previewCard.bottom - 32};
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
