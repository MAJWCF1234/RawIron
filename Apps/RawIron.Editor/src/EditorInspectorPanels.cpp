#include "EditorInspectorPanels.h"

#include "EditorRenderer.h"

namespace ri::editor {

#if defined(_WIN32)
GameplayPanelLayout ComputeGameplayPanelLayout(const RECT& inspectorInner) {
    GameplayPanelLayout layout{};
    int infoTop = inspectorInner.top + 42;
    infoTop += 24;
    infoTop += 88;
    layout.inventoryModeRow =
        RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20};
    infoTop += 22;
    layout.offHandRow =
        RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20};
    infoTop += 20;
    infoTop += 20;
    infoTop += 20;
    infoTop += 30;
    layout.addTriggerBtn =
        RECT{inspectorInner.left + 10, infoTop, inspectorInner.left + 118, infoTop + 26};
    layout.exportBtn =
        RECT{inspectorInner.left + 124, infoTop, inspectorInner.left + 228, infoTop + 26};
    layout.playtestBtn =
        RECT{inspectorInner.left + 234, infoTop, inspectorInner.right - 10, infoTop + 26};
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
    const auto drawSmall = [&](const std::string& text, COLORREF color, int height = 20, UINT fmt = DT_LEFT | DT_SINGLELINE | DT_VCENTER) {
        EditorRenderer::DrawTextLine(dc,
                                     RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + height},
                                     text,
                                     color,
                                     smallFont,
                                     fmt);
        infoTop += (height == 20 ? 20 : 22);
    };
    drawSmall(model.pathLine, RGB(210, 210, 210), 20, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    infoTop += 2;
    drawSmall(model.kindLine, RGB(228, 216, 170));
    if (model.meshPrimitiveLine.has_value()) {
        drawSmall(*model.meshPrimitiveLine, RGB(200, 210, 220));
    }
    drawSmall(model.localPosLine, RGB(200, 200, 200));
    drawSmall(model.localRotLine, RGB(200, 200, 200));
    drawSmall(model.localScaleLine, RGB(200, 200, 200));
    if (model.editableAuthored) {
        EditorRenderer::DrawTextLine(dc,
                                     RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 18},
                                     "Transform nudge (uses grid snap on position):",
                                     RGB(214, 208, 168),
                                     smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        infoTop += 20;
        drawNudgeRow(dc, infoTop, inspectorInner, "Pos", 0);
        drawNudgeRow(dc, infoTop, inspectorInner, "Rot", 1);
        drawNudgeRow(dc, infoTop, inspectorInner, "Scl", 2);
    } else {
        EditorRenderer::DrawTextLine(dc,
                                     RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 18},
                                     "Protected node - use viewport keys (T/R/U) on authored meshes only.",
                                     RGB(214, 180, 140),
                                     smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        infoTop += 20;
    }
    drawSmall(model.editModeLine, RGB(214, 208, 168));
    drawSmall(model.groupingLine, RGB(214, 208, 168), 20, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    drawSmall(model.opsLine, RGB(214, 208, 168), 20, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    drawSmall(model.worldPosLine, RGB(200, 220, 200));
}

void RenderBrushInspectorPanel(HDC dc,
                               const RECT& inspectorInner,
                               const BrushInspectorPanelModel& model,
                               HFONT headerFont,
                               HFONT bodyFont,
                               HFONT smallFont,
                               const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton) {
    drawToolbarButton(dc,
                      RECT{inspectorInner.left + 10, inspectorInner.top + 42, inspectorInner.left + 52, inspectorInner.top + 66},
                      "<",
                      false);
    drawToolbarButton(dc,
                      RECT{inspectorInner.left + 56, inspectorInner.top + 42, inspectorInner.left + 98, inspectorInner.top + 66},
                      ">",
                      false);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{inspectorInner.left + 104, inspectorInner.top + 44, inspectorInner.right - 10, inspectorInner.top + 64},
                                 model.presetTitleLine,
                                 RGB(228, 236, 248),
                                 bodyFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    int infoTop = inspectorInner.top + 74;
    auto draw = [&](const std::string& text, COLORREF color, HFONT font, int height, UINT fmt) {
        EditorRenderer::DrawTextLine(dc,
                                     RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + height},
                                     text,
                                     color,
                                     font,
                                     fmt);
        infoTop += (height >= 40 ? 52 : (height >= 36 ? 42 : 20));
    };
    draw(model.headingLine, RGB(240, 240, 236), headerFont, 20, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    infoTop += 4;
    draw(model.helpLineA, RGB(210, 215, 222), smallFont, 36, DT_LEFT | DT_WORDBREAK);
    draw(model.helpLineB, RGB(200, 206, 214), smallFont, 44, DT_LEFT | DT_WORDBREAK);
    draw(model.selectionLine, RGB(226, 226, 226), smallFont, 20, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    infoTop += 2;
    draw(model.meshAttachedLine, RGB(200, 200, 200), smallFont, 20, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    draw(model.boundsSizeLine, RGB(200, 220, 200), smallFont, 20, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    draw(model.boundsCenterLine, RGB(200, 220, 200), smallFont, 20, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
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
    RECT gameplayCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 78};
    EditorRenderer::DrawInsetFrame(dc, gameplayCard, RGB(70, 88, 84), RGB(154, 196, 188), RGB(24, 34, 34));
    EditorRenderer::DrawTextLine(dc,
                                 RECT{gameplayCard.left + 10, gameplayCard.top + 8, gameplayCard.right - 10, gameplayCard.top + 26},
                                 "Game-ready authoring",
                                 RGB(244, 250, 246),
                                 headerFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{gameplayCard.left + 10, gameplayCard.top + 30, gameplayCard.right - 10, gameplayCard.bottom - 8},
                                 model.summaryLine,
                                 RGB(216, 236, 228),
                                 smallFont,
                                 DT_LEFT | DT_WORDBREAK);
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
#endif

} // namespace ri::editor
