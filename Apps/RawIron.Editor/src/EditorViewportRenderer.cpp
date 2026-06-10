#include "EditorViewportRenderer.h"

#include "EditorRenderer.h"
#include "EditorUiTheme.h"

#include <algorithm>

namespace ri::editor {

namespace {

void StrokeLine(HDC dc, LONG x1, LONG y1, LONG x2, LONG y2, COLORREF color, int width = 1) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

} // namespace

AuthoringToolbarRects ComputeAuthoringToolbarRects(const RECT& toolStrip) {
    constexpr LONG kAuthoringBlockWidth = 656;
    const LONG rowTop = toolStrip.top + 8;
    const LONG rowBot = toolStrip.bottom - 8;
    const LONG x0 = std::max(toolStrip.left + 806L, toolStrip.right - 12 - kAuthoringBlockWidth);
    AuthoringToolbarRects rects{};
    rects.addCube = {x0, rowTop, x0 + 80, rowBot};
    rects.addPlane = {x0 + 86, rowTop, x0 + 166, rowBot};
    rects.addTrigger = {x0 + 172, rowTop, x0 + 268, rowBot};
    rects.addLight = {x0 + 274, rowTop, x0 + 350, rowBot};
    rects.duplicate = {x0 + 356, rowTop, x0 + 432, rowBot};
    rects.exportCsv = {x0 + 438, rowTop, x0 + 518, rowBot};
    rects.play = {x0 + 524, rowTop, x0 + 656, rowBot};
    return rects;
}

TopChromeRects ComputeTopChromeRects(const RECT& topBar) {
    const LONG rowTop = topBar.top + 10;
    const LONG rowBot = topBar.top + 36;
    const LONG right = topBar.right - 18;
    TopChromeRects rects{};
    rects.files = {right - 94, rowTop, right, rowBot};
    rects.play = {right - 194, rowTop, right - 100, rowBot};
    rects.exportScene = {right - 300, rowTop, right - 200, rowBot};
    rects.scaffold = {right - 406, rowTop, right - 306, rowBot};
    rects.save = {right - 512, rowTop, right - 412, rowBot};
    rects.newGame = {right - 624, rowTop, right - 518, rowBot};
    return rects;
}

bool HitTestViewportCreateMenu(const RECT& viewportInner, const POINT& point) {
    constexpr int kBannerHeight = 24;
    const RECT menuBanner{viewportInner.left + 4,
                          viewportInner.top + 6,
                          viewportInner.right - 4,
                          viewportInner.top + 6 + kBannerHeight};
    if (PtInRect(&menuBanner, point) == FALSE) {
        return false;
    }
    const LONG createLeft = menuBanner.left + 188;
    const LONG createRight = menuBanner.left + 252;
    return point.x >= createLeft && point.x <= createRight;
}

bool HitTestViewportHelpMenu(const RECT& viewportInner, const POINT& point) {
    constexpr int kBannerHeight = 24;
    const RECT menuBanner{viewportInner.left + 4,
                          viewportInner.top + 6,
                          viewportInner.right - 4,
                          viewportInner.top + 6 + kBannerHeight};
    if (PtInRect(&menuBanner, point) == FALSE) {
        return false;
    }
    const LONG helpLeft = menuBanner.right - 56;
    const LONG helpRight = menuBanner.right - 8;
    return point.x >= helpLeft && point.x <= helpRight;
}

EditorViewportWorldBarHit HitTestViewportWorldBar(const RECT& viewportInner,
                                                  const POINT& point,
                                                  const bool showWorldBar,
                                                  const int worldBarHeight) {
    EditorViewportWorldBarHit hit{};
    if (!showWorldBar || worldBarHeight <= 0) {
        return hit;
    }
    constexpr int kBannerHeight = 24;
    const RECT menuBanner{viewportInner.left + 4,
                          viewportInner.top + 6,
                          viewportInner.right - 4,
                          viewportInner.top + 6 + kBannerHeight};
    const RECT worldBar{viewportInner.left + 4,
                        menuBanner.bottom + 4,
                        viewportInner.right - 4,
                        menuBanner.bottom + 4 + worldBarHeight};
    const RECT skyBtn{worldBar.left + 8, worldBar.top + 4, worldBar.left + 232, worldBar.bottom - 4};
    if (PtInRect(&skyBtn, point) != FALSE) {
        hit.hitAtmosphereCycle = true;
    }
    return hit;
}

#if defined(_WIN32)
void RenderEditorTopChrome(HDC dc,
                           const RECT& client,
                           const RECT& topBar,
                           const EditorViewportChromeModel& model,
                           const EditorViewportTheme& theme) {
    EditorRenderer::DrawPanelFrame(dc,
                                   topBar,
                                   EditorUiTheme::kPanelRaisedFill,
                                   EditorUiTheme::kPanelRaisedHi,
                                   EditorUiTheme::kPanelRaisedShadow);
    EditorRenderer::FillRectColor(dc, RECT{0, 0, client.right, 6}, EditorUiTheme::kTopStripe);
    const RECT projectBand{16, 8, client.right - 16, 50};
    EditorRenderer::DrawInsetFrame(dc,
                                   projectBand,
                                   EditorUiTheme::kProjectBandFill,
                                   EditorUiTheme::kProjectBandHi,
                                   EditorUiTheme::kProjectBandShadow);
    EditorRenderer::FillRectColor(
        dc, RECT{projectBand.left + 1, projectBand.top + 1, projectBand.right - 1, projectBand.top + 4},
        EditorUiTheme::kProjectBandAccent);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{28, 10, 360, 30},
                                 model.title,
                                 EditorUiTheme::kTextOnPanel,
                                 theme.titleFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{28, 28, 620, 46},
                                 model.subtitle,
                                 EditorUiTheme::kTextOnPanelMuted,
                                 theme.smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{360, 10, 880, 30},
                                 model.focusedWorkspaceGameLabel,
                                 EditorUiTheme::kTextGold,
                                 theme.bodyFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{360, 28, client.right - 450, 46},
                                 model.workspaceLabel,
                                 EditorUiTheme::kTextOnPanelMuted,
                                 theme.smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    const TopChromeRects topChrome = ComputeTopChromeRects(topBar);
    EditorRenderer::DrawToolbarButton(
        dc, topChrome.newGame, "New Game", false, theme.smallFont, EditorToolbarStyle::Light);
    EditorRenderer::DrawToolbarButton(dc, topChrome.save, "Save", false, theme.smallFont, EditorToolbarStyle::Light);
    EditorRenderer::DrawToolbarButton(
        dc, topChrome.scaffold, "Setup Files", false, theme.smallFont, EditorToolbarStyle::Light);
    EditorRenderer::DrawToolbarButton(
        dc, topChrome.exportScene, "Export", false, theme.smallFont, EditorToolbarStyle::Light);
    EditorRenderer::DrawToolbarButton(
        dc, topChrome.play, "Playtest", false, theme.smallFont, EditorToolbarStyle::Light);
    EditorRenderer::DrawToolbarButton(
        dc, topChrome.files, "Files", model.resourcesModeActive, theme.smallFont, EditorToolbarStyle::Light);
}

void RenderEditorToolStrip(HDC dc,
                           const RECT& toolStrip,
                           const EditorViewportToolStripModel& model,
                           const EditorViewportTheme& theme) {
    EditorRenderer::DrawPanelFrame(dc,
                                   toolStrip,
                                   EditorUiTheme::kToolStripFill,
                                   EditorUiTheme::kToolStripHi,
                                   EditorUiTheme::kToolStripShadow);
    EditorRenderer::FillRectColor(
        dc, RECT{toolStrip.left + 1, toolStrip.top + 1, toolStrip.right - 1, toolStrip.top + 3},
        EditorUiTheme::kToolStripAccent);
    const int savedDc = SaveDC(dc);
    IntersectClipRect(dc, toolStrip.left, toolStrip.top, toolStrip.right, toolStrip.bottom);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{toolStrip.left + 12, toolStrip.top + 8, toolStrip.left + 72, toolStrip.bottom - 8},
                                 "Transform",
                                 EditorUiTheme::kTextOnDark,
                                 theme.headerFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    const auto toolBtn = [&](const RECT& rect, const std::string& label, bool active) {
        EditorRenderer::DrawToolbarButton(dc, rect, label, active, theme.smallFont, EditorToolbarStyle::Dark);
    };
    toolBtn(RECT{toolStrip.left + 78, toolStrip.top + 8, toolStrip.left + 130, toolStrip.bottom - 8},
            "Select",
            model.toolMode == EditorToolMode::Select);
    toolBtn(RECT{toolStrip.left + 134, toolStrip.top + 8, toolStrip.left + 188, toolStrip.bottom - 8},
            "Create",
            model.toolMode == EditorToolMode::Create);
    toolBtn(RECT{toolStrip.left + 194, toolStrip.top + 8, toolStrip.left + 248, toolStrip.bottom - 8},
            "Camera",
            model.toolMode == EditorToolMode::Camera);
    toolBtn(RECT{toolStrip.left + 254, toolStrip.top + 8, toolStrip.left + 328, toolStrip.bottom - 8},
            "Move",
            model.editModeLabel == "Move");
    toolBtn(RECT{toolStrip.left + 334, toolStrip.top + 8, toolStrip.left + 402, toolStrip.bottom - 8},
            "Rotate",
            model.editModeLabel == "Rotate");
    toolBtn(RECT{toolStrip.left + 408, toolStrip.top + 8, toolStrip.left + 476, toolStrip.bottom - 8},
            "Scale",
            model.editModeLabel == "Scale");
    toolBtn(RECT{toolStrip.left + 482, toolStrip.top + 8, toolStrip.left + 528, toolStrip.bottom - 8}, "X", model.axisLabel == "X");
    toolBtn(RECT{toolStrip.left + 534, toolStrip.top + 8, toolStrip.left + 580, toolStrip.bottom - 8}, "Y", model.axisLabel == "Y");
    toolBtn(RECT{toolStrip.left + 586, toolStrip.top + 8, toolStrip.left + 632, toolStrip.bottom - 8}, "Z", model.axisLabel == "Z");
    toolBtn(RECT{toolStrip.left + 638, toolStrip.top + 8, toolStrip.left + 716, toolStrip.bottom - 8},
            std::string("Snap ") + (model.gridSnapEnabled ? "On" : "Off"),
            model.gridSnapEnabled);
    toolBtn(RECT{toolStrip.left + 722, toolStrip.top + 8, toolStrip.left + 754, toolStrip.bottom - 8}, "-", false);
    toolBtn(RECT{toolStrip.left + 758, toolStrip.top + 8, toolStrip.left + 790, toolStrip.bottom - 8}, "+", false);

    const AuthoringToolbarRects authoringPaint = ComputeAuthoringToolbarRects(toolStrip);
    toolBtn(authoringPaint.addCube, "+ Cube", false);
    toolBtn(authoringPaint.addPlane, "+ Plane", false);
    toolBtn(authoringPaint.addTrigger, "+ Trigger", false);
    toolBtn(authoringPaint.addLight, "+ Light", false);
    toolBtn(authoringPaint.duplicate, "Duplicate", false);
    toolBtn(authoringPaint.exportCsv, "Export", false);
    toolBtn(authoringPaint.play, "Play", false);
    const LONG statusLeft =
        std::min(toolStrip.left + 1010L, std::max(authoringPaint.play.right + 12L, toolStrip.left + 806L));
    std::string modeLine = "Mode ";
    switch (model.toolMode) {
        case EditorToolMode::Select:
            modeLine += "Select";
            break;
        case EditorToolMode::Create:
            modeLine += "Create";
            if (!model.armedPresetLabel.empty()) {
                modeLine += " · " + model.armedPresetLabel;
            }
            break;
        case EditorToolMode::Camera:
            modeLine += "Camera";
            break;
    }
    EditorRenderer::DrawTextLine(
        dc,
        RECT{statusLeft, toolStrip.top + 8, toolStrip.right - 12, toolStrip.bottom - 8},
        modeLine + "  |  Step " + model.editStepLabel + "  |  Undo " + std::to_string(model.undoDepth) +
            "  |  Grid " + model.gridSnapLabel + "  |  Authored " + std::to_string(model.authoredCount) +
            "  |  Triggers " + std::to_string(model.triggerCount),
        EditorUiTheme::kTextMuted,
        theme.smallFont,
        DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    RestoreDC(dc, savedDc);
}

void RenderEditorFramePanels(HDC dc,
                             const RECT& hierarchy,
                             const RECT& viewport,
                             const RECT& inspector,
                             const RECT& hierarchySplitter,
                             const RECT& inspectorSplitter,
                             const RECT& statusBar) {
    EditorRenderer::DrawPanelFrame(dc,
                                   hierarchy,
                                   EditorUiTheme::kPanelRaisedFill,
                                   EditorUiTheme::kPanelRaisedHi,
                                   EditorUiTheme::kPanelRaisedShadow);
    EditorRenderer::DrawPanelFrame(dc,
                                   viewport,
                                   EditorUiTheme::kPanelRaisedFill,
                                   EditorUiTheme::kPanelRaisedHi,
                                   EditorUiTheme::kPanelRaisedShadow);
    EditorRenderer::DrawPanelFrame(dc,
                                   inspector,
                                   EditorUiTheme::kPanelRaisedFill,
                                   EditorUiTheme::kPanelRaisedHi,
                                   EditorUiTheme::kPanelRaisedShadow);
    EditorRenderer::FillRectColor(dc, hierarchySplitter, EditorUiTheme::kSplitter);
    EditorRenderer::FillRectColor(dc, inspectorSplitter, EditorUiTheme::kSplitter);
    EditorRenderer::DrawPanelFrame(dc,
                                   statusBar,
                                   EditorUiTheme::kStatusBarFill,
                                   EditorUiTheme::kStatusBarHi,
                                   EditorUiTheme::kStatusBarShadow);
}

void RenderEditorStatusBar(HDC dc,
                           const RECT& statusBar,
                           const EditorViewportStatusModel& model,
                           const EditorViewportTheme& theme) {
    EditorRenderer::DrawTextLine(dc,
                                 RECT{statusBar.left + 12, statusBar.top + 8, statusBar.right - 12, statusBar.top + 28},
                                 model.consoleLine,
                                 EditorUiTheme::kTextOnPanel,
                                 theme.smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{statusBar.left + 12, statusBar.top + 30, statusBar.right - 12, statusBar.top + 50},
                                 model.controlsLine,
                                 EditorUiTheme::kTextOnPanelMuted,
                                 theme.smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{statusBar.left + 12, statusBar.top + 50, statusBar.right - 12, statusBar.bottom - 8},
                                 model.stateLine,
                                 EditorUiTheme::kTextStatusGold,
                                 theme.smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void RenderEditorViewportBlock(HDC dc,
                               const RECT& viewportInner,
                               const EditorViewportBlockModel& model,
                               const EditorViewportTheme& theme,
                               const EditorViewportBlockCallbacks& callbacks) {
    constexpr int kBannerHeight = 24;
    const RECT menuBanner{viewportInner.left + 4,
                          viewportInner.top + 6,
                          viewportInner.right - 4,
                          viewportInner.top + 6 + kBannerHeight};
    EditorRenderer::DrawPanelFrame(dc,
                                   menuBanner,
                                   EditorUiTheme::kMenuBarFill,
                                   EditorUiTheme::kMenuBarHi,
                                   EditorUiTheme::kMenuBarShadow);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{menuBanner.left + 8, menuBanner.top + 3, menuBanner.right - 8, menuBanner.bottom - 3},
                                 model.createMenuActive
                                     ? "Scene   Edit   View   [Create]   Build   Tools   Window   Help (F1)"
                                     : "Scene   Edit   View   Create   Build   Tools   Window   Help (F1)",
                                 EditorUiTheme::kMenuBarText,
                                 theme.smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    const int worldBarHeight = model.showWorldBar ? std::max(0, model.worldBarHeight) : 0;
    if (worldBarHeight > 0) {
        const RECT worldBar{viewportInner.left + 4,
                            menuBanner.bottom + 4,
                            viewportInner.right - 4,
                            menuBanner.bottom + 4 + worldBarHeight};
        EditorRenderer::DrawInsetFrame(dc,
                                       worldBar,
                                       EditorUiTheme::kCreatorCardFill,
                                       EditorUiTheme::kCreatorCardHi,
                                       EditorUiTheme::kCreatorCardShadow);
        const RECT skyBtn{worldBar.left + 8, worldBar.top + 4, worldBar.left + 232, worldBar.bottom - 4};
        EditorRenderer::DrawToolbarButton(
            dc,
            skyBtn,
            "Sky: " + model.atmosphereLabel + "  v",
            true,
            theme.smallFont,
            EditorToolbarStyle::Creator);
        if (!model.createHintLine.empty()) {
            EditorRenderer::DrawTextLine(
                dc,
                RECT{worldBar.left + 244, worldBar.top + 4, worldBar.right - 8, worldBar.bottom - 4},
                model.createHintLine,
                EditorUiTheme::kCreatorBody,
                theme.smallFont,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
    }

    constexpr int kMetaStrip = 26;
    const int bottomReserve = kMetaStrip + std::max(0, model.bottomChromeInset);
    const RECT quadArea{viewportInner.left + 4,
                        menuBanner.bottom + 4 + worldBarHeight,
                        viewportInner.right - 4,
                        viewportInner.bottom - 4 - bottomReserve};
    const RECT viewMeta{
        quadArea.left + 6, quadArea.bottom + 4, quadArea.right - 6, quadArea.bottom + 4 + kMetaStrip};

    const auto drawCameraAndMeta = [&]() {
        if (model.cameraPlotRect.right > model.cameraPlotRect.left + 8
            && model.cameraPlotRect.bottom > model.cameraPlotRect.top + 8) {
            EditorRenderer::DrawInsetFrame(dc,
                                           model.cameraPlotRect,
                                           EditorUiTheme::kViewportWellFill,
                                           EditorUiTheme::kWellHi,
                                           EditorUiTheme::kWellShadow);
            const RECT plotInner = EditorRenderer::InsetRect(model.cameraPlotRect, 2);
            if (callbacks.drawViewportPreview) {
                callbacks.drawViewportPreview(plotInner);
            }
            if (callbacks.drawRuntimeStatsOverlay) {
                callbacks.drawRuntimeStatsOverlay(plotInner);
            }
        }
        EditorRenderer::DrawTextLine(dc,
                                     viewMeta,
                                     model.cameraSummaryLine,
                                     EditorUiTheme::kTextMuted,
                                     theme.smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    };

    if (quadArea.right <= quadArea.left + 32 || quadArea.bottom <= quadArea.top + 32) {
        EditorRenderer::DrawInsetFrame(dc,
                                       quadArea,
                                       EditorUiTheme::kViewportWellFill,
                                       EditorUiTheme::kWellHi,
                                       EditorUiTheme::kWellShadow);
        if (callbacks.drawViewportPreview) {
            callbacks.drawViewportPreview(EditorRenderer::InsetRect(quadArea, 2));
        }
        return;
    }

    if (model.full3DViewport) {
        EditorRenderer::DrawInsetFrame(dc,
                                       quadArea,
                                       EditorUiTheme::kViewportWellFill,
                                       EditorUiTheme::kPerspBorder,
                                       EditorUiTheme::kWellShadow);
        const RECT quadInner = EditorRenderer::InsetRect(quadArea, 2);
        EditorRenderer::DrawTextLine(dc,
                                     RECT{quadInner.left + 6, quadInner.top + 4, quadInner.right - 6, quadInner.top + 22},
                                     "PERSPECTIVE   |   Create: click to stamp   Select: pick objects   Camera: drag view",
                                     EditorUiTheme::kPerspTitle,
                                     theme.smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        if (model.cameraPlotRect.right > model.cameraPlotRect.left + 8
            && model.cameraPlotRect.bottom > model.cameraPlotRect.top + 8) {
            const RECT plotInner = EditorRenderer::InsetRect(model.cameraPlotRect, 2);
            if (callbacks.drawViewportPreview) {
                callbacks.drawViewportPreview(plotInner);
            }
            if (callbacks.drawRuntimeStatsOverlay) {
                callbacks.drawRuntimeStatsOverlay(plotInner);
            }
        }
        EditorRenderer::DrawTextLine(dc,
                                     viewMeta,
                                     model.cameraSummaryLine,
                                     EditorUiTheme::kTextMuted,
                                     theme.smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        return;
    }

    const int midX = (quadArea.left + quadArea.right) / 2;
    const int midY = (quadArea.top + quadArea.bottom) / 2;
    const RECT cellTop{quadArea.left, quadArea.top, midX - 1, midY - 1};
    const RECT cellSide{midX + 1, quadArea.top, quadArea.right, midY - 1};
    const RECT cellFront{quadArea.left, midY + 1, midX - 1, quadArea.bottom};
    const RECT cellCamera{midX + 1, midY + 1, quadArea.right, quadArea.bottom};

    StrokeLine(dc, midX, quadArea.top, midX, quadArea.bottom, EditorUiTheme::kQuadDivider, 2);
    StrokeLine(dc, quadArea.left, midY, quadArea.right, midY, EditorUiTheme::kQuadDivider, 2);

    if (callbacks.drawTopView) {
        callbacks.drawTopView(cellTop);
    }
    if (callbacks.drawSideView) {
        callbacks.drawSideView(cellSide);
    }
    if (callbacks.drawFrontView) {
        callbacks.drawFrontView(cellFront);
    }

    EditorRenderer::DrawInsetFrame(dc,
                                   cellCamera,
                                   EditorUiTheme::kViewportWellFill,
                                   EditorUiTheme::kPerspBorder,
                                   EditorUiTheme::kWellShadow);
    const RECT cameraInner{cellCamera.left + 2, cellCamera.top + 2, cellCamera.right - 2, cellCamera.bottom - 2};
    EditorRenderer::DrawTextLine(dc,
                                 RECT{cameraInner.left + 6, cameraInner.top + 4, cameraInner.right - 6, cameraInner.top + 22},
                                 "PERSPECTIVE   |   Tab layout",
                                 EditorUiTheme::kPerspTitle,
                                 theme.smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    drawCameraAndMeta();
}

void PresentEditorFrame(HDC windowDc, HDC backBufferDc, const int width, const int height, const RECT* excludedClientRect) {
    const int savedWindowDc = SaveDC(windowDc);
    if (excludedClientRect != nullptr) {
        ExcludeClipRect(windowDc,
                        excludedClientRect->left,
                        excludedClientRect->top,
                        excludedClientRect->right,
                        excludedClientRect->bottom);
    }
    BitBlt(windowDc, 0, 0, width, height, backBufferDc, 0, 0, SRCCOPY);
    RestoreDC(windowDc, savedWindowDc);
}
#endif

} // namespace ri::editor
