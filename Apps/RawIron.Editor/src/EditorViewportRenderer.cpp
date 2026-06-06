#include "EditorViewportRenderer.h"

#include "EditorRenderer.h"

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
    const LONG rowTop = toolStrip.top + 8;
    const LONG rowBot = toolStrip.bottom - 8;
    const LONG x0 = toolStrip.left + 720;
    AuthoringToolbarRects rects{};
    rects.addCube = {x0, rowTop, x0 + 88, rowBot};
    rects.addPlane = {x0 + 94, rowTop, x0 + 188, rowBot};
    rects.addTrigger = {x0 + 194, rowTop, x0 + 300, rowBot};
    rects.duplicate = {x0 + 306, rowTop, x0 + 388, rowBot};
    rects.exportCsv = {x0 + 394, rowTop, x0 + 484, rowBot};
    rects.play = {x0 + 490, rowTop, x0 + 568, rowBot};
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
    return rects;
}

#if defined(_WIN32)
void RenderEditorTopChrome(HDC dc,
                           const RECT& client,
                           const RECT& topBar,
                           const EditorViewportChromeModel& model,
                           const EditorViewportTheme& theme) {
    EditorRenderer::DrawPanelFrame(dc, topBar, RGB(92, 97, 104), RGB(192, 198, 206), RGB(30, 34, 40));
    EditorRenderer::FillRectColor(dc, RECT{0, 0, client.right, 8}, RGB(214, 150, 56));
    const RECT projectBand{16, 8, client.right - 16, 50};
    EditorRenderer::DrawInsetFrame(
        dc, projectBand, RGB(112, 118, 126), RGB(212, 216, 222), RGB(36, 42, 48));
    EditorRenderer::FillRectColor(dc,
                                  RECT{projectBand.left + 1, projectBand.top + 1, projectBand.right - 1, projectBand.top + 5},
                                  RGB(226, 162, 68));
    EditorRenderer::DrawTextLine(dc,
                                 RECT{28, 10, 360, 30},
                                 model.title,
                                 RGB(248, 248, 244),
                                 theme.titleFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{28, 28, 620, 46},
                                 model.subtitle,
                                 RGB(222, 226, 230),
                                 theme.smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{360, 10, 880, 30},
                                 model.focusedWorkspaceGameLabel,
                                 RGB(255, 221, 154),
                                 theme.bodyFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{360, 28, client.right - 450, 46},
                                 model.workspaceLabel,
                                 RGB(216, 220, 226),
                                 theme.smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    const TopChromeRects topChrome = ComputeTopChromeRects(topBar);
    EditorRenderer::DrawToolbarButton(dc, topChrome.save, "Save", false, theme.smallFont);
    EditorRenderer::DrawToolbarButton(dc, topChrome.scaffold, "Scaffold", false, theme.smallFont);
    EditorRenderer::DrawToolbarButton(dc, topChrome.exportScene, "Export", false, theme.smallFont);
    EditorRenderer::DrawToolbarButton(dc, topChrome.play, "Playtest", false, theme.smallFont);
    EditorRenderer::DrawToolbarButton(dc, topChrome.files, "Files", model.resourcesModeActive, theme.smallFont);
}

void RenderEditorToolStrip(HDC dc,
                           const RECT& toolStrip,
                           const EditorViewportToolStripModel& model,
                           const EditorViewportTheme& theme) {
    EditorRenderer::DrawPanelFrame(dc, toolStrip, RGB(60, 66, 74), RGB(176, 182, 190), RGB(24, 28, 34));
    EditorRenderer::FillRectColor(dc,
                                  RECT{toolStrip.left + 1, toolStrip.top + 1, toolStrip.right - 1, toolStrip.top + 5},
                                  RGB(74, 84, 98));
    EditorRenderer::DrawTextLine(dc,
                                 RECT{toolStrip.left + 12, toolStrip.top + 8, toolStrip.left + 72, toolStrip.bottom - 8},
                                 "Transform",
                                 RGB(238, 242, 248),
                                 theme.headerFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    EditorRenderer::DrawToolbarButton(
        dc, RECT{toolStrip.left + 78, toolStrip.top + 8, toolStrip.left + 130, toolStrip.bottom - 8}, "Select", true, theme.smallFont);
    EditorRenderer::DrawToolbarButton(
        dc, RECT{toolStrip.left + 134, toolStrip.top + 8, toolStrip.left + 188, toolStrip.bottom - 8}, "Camera", false, theme.smallFont);
    EditorRenderer::DrawToolbarButton(
        dc, RECT{toolStrip.left + 194, toolStrip.top + 8, toolStrip.left + 268, toolStrip.bottom - 8}, "Move", model.editModeLabel == "Move", theme.smallFont);
    EditorRenderer::DrawToolbarButton(
        dc, RECT{toolStrip.left + 274, toolStrip.top + 8, toolStrip.left + 342, toolStrip.bottom - 8}, "Rotate", model.editModeLabel == "Rotate", theme.smallFont);
    EditorRenderer::DrawToolbarButton(
        dc, RECT{toolStrip.left + 348, toolStrip.top + 8, toolStrip.left + 416, toolStrip.bottom - 8}, "Scale", model.editModeLabel == "Scale", theme.smallFont);
    EditorRenderer::DrawToolbarButton(
        dc, RECT{toolStrip.left + 422, toolStrip.top + 8, toolStrip.left + 468, toolStrip.bottom - 8}, "X", model.axisLabel == "X", theme.smallFont);
    EditorRenderer::DrawToolbarButton(
        dc, RECT{toolStrip.left + 474, toolStrip.top + 8, toolStrip.left + 520, toolStrip.bottom - 8}, "Y", model.axisLabel == "Y", theme.smallFont);
    EditorRenderer::DrawToolbarButton(
        dc, RECT{toolStrip.left + 526, toolStrip.top + 8, toolStrip.left + 572, toolStrip.bottom - 8}, "Z", model.axisLabel == "Z", theme.smallFont);
    EditorRenderer::DrawToolbarButton(
        dc,
        RECT{toolStrip.left + 578, toolStrip.top + 8, toolStrip.left + 656, toolStrip.bottom - 8},
        std::string("Snap ") + (model.gridSnapEnabled ? "On" : "Off"),
        model.gridSnapEnabled,
        theme.smallFont);
    EditorRenderer::DrawToolbarButton(
        dc, RECT{toolStrip.left + 662, toolStrip.top + 8, toolStrip.left + 694, toolStrip.bottom - 8}, "-", false, theme.smallFont);
    EditorRenderer::DrawToolbarButton(
        dc, RECT{toolStrip.left + 698, toolStrip.top + 8, toolStrip.left + 730, toolStrip.bottom - 8}, "+", false, theme.smallFont);

    const AuthoringToolbarRects authoringPaint = ComputeAuthoringToolbarRects(toolStrip);
    EditorRenderer::DrawToolbarButton(dc, authoringPaint.addCube, "+ Cube", false, theme.smallFont);
    EditorRenderer::DrawToolbarButton(dc, authoringPaint.addPlane, "+ Plane", false, theme.smallFont);
    EditorRenderer::DrawToolbarButton(dc, authoringPaint.addTrigger, "+ Trigger", false, theme.smallFont);
    EditorRenderer::DrawToolbarButton(dc, authoringPaint.duplicate, "Duplicate", false, theme.smallFont);
    EditorRenderer::DrawToolbarButton(dc, authoringPaint.exportCsv, "Export", false, theme.smallFont);
    EditorRenderer::DrawToolbarButton(dc, authoringPaint.play, "Play", false, theme.smallFont);
    EditorRenderer::DrawTextLine(
        dc,
        RECT{toolStrip.left + 954, toolStrip.top + 8, toolStrip.right - 12, toolStrip.bottom - 8},
        "Step " + model.editStepLabel + "  |  Undo " + std::to_string(model.undoDepth) +
            "  |  Grid " + model.gridSnapLabel + "  |  Authored " + std::to_string(model.authoredCount) +
            "  |  Triggers " + std::to_string(model.triggerCount),
        RGB(220, 226, 234),
        theme.smallFont,
        DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
}

void RenderEditorFramePanels(HDC dc,
                             const RECT& hierarchy,
                             const RECT& viewport,
                             const RECT& inspector,
                             const RECT& hierarchySplitter,
                             const RECT& inspectorSplitter,
                             const RECT& statusBar) {
    EditorRenderer::DrawPanelFrame(dc, hierarchy, RGB(184, 188, 192), RGB(238, 241, 244), RGB(63, 68, 76));
    EditorRenderer::DrawPanelFrame(dc, viewport, RGB(184, 188, 192), RGB(238, 241, 244), RGB(63, 68, 76));
    EditorRenderer::DrawPanelFrame(dc, inspector, RGB(184, 188, 192), RGB(238, 241, 244), RGB(63, 68, 76));
    EditorRenderer::FillRectColor(dc, hierarchySplitter, RGB(132, 136, 144));
    EditorRenderer::FillRectColor(dc, inspectorSplitter, RGB(132, 136, 144));
    EditorRenderer::DrawPanelFrame(dc, statusBar, RGB(168, 170, 174), RGB(252, 252, 252), RGB(96, 98, 102));
}

void RenderEditorStatusBar(HDC dc,
                           const RECT& statusBar,
                           const EditorViewportStatusModel& model,
                           const EditorViewportTheme& theme) {
    EditorRenderer::DrawTextLine(dc,
                                 RECT{statusBar.left + 12, statusBar.top + 8, statusBar.right - 12, statusBar.top + 28},
                                 model.consoleLine,
                                 RGB(228, 234, 240),
                                 theme.smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{statusBar.left + 12, statusBar.top + 30, statusBar.right - 12, statusBar.top + 50},
                                 model.controlsLine,
                                 RGB(212, 217, 223),
                                 theme.smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{statusBar.left + 12, statusBar.top + 50, statusBar.right - 12, statusBar.bottom - 8},
                                 model.stateLine,
                                 RGB(255, 216, 146),
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
    EditorRenderer::FillRectColor(dc, menuBanner, RGB(88, 94, 102));
    EditorRenderer::DrawInsetFrame(dc, menuBanner, RGB(100, 106, 114), RGB(188, 192, 198), RGB(30, 34, 40));
    EditorRenderer::DrawTextLine(dc,
                                 RECT{menuBanner.left + 8, menuBanner.top + 3, menuBanner.right - 8, menuBanner.bottom - 3},
                                 "Scene      Edit      View      Create      Build      Tools      Window      Help",
                                 RGB(242, 245, 248),
                                 theme.smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    constexpr int kMetaStrip = 26;
    const RECT quadArea{
        viewportInner.left + 4, menuBanner.bottom + 4, viewportInner.right - 4, viewportInner.bottom - 4 - kMetaStrip};
    const RECT viewMeta{quadArea.left + 6, quadArea.bottom + 4, quadArea.right - 6, viewportInner.bottom - 4};

    const auto drawCameraAndMeta = [&]() {
        if (model.cameraPlotRect.right > model.cameraPlotRect.left + 8
            && model.cameraPlotRect.bottom > model.cameraPlotRect.top + 8) {
            EditorRenderer::DrawInsetFrame(
                dc, model.cameraPlotRect, RGB(24, 26, 30), RGB(112, 118, 128), RGB(18, 20, 24));
            if (callbacks.drawViewportPreview) {
                callbacks.drawViewportPreview(model.cameraPlotRect);
            }
            if (callbacks.drawRuntimeStatsOverlay) {
                callbacks.drawRuntimeStatsOverlay(model.cameraPlotRect);
            }
        }
        EditorRenderer::DrawTextLine(dc,
                                     viewMeta,
                                     model.cameraSummaryLine,
                                     RGB(166, 172, 182),
                                     theme.smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    };

    if (quadArea.right <= quadArea.left + 32 || quadArea.bottom <= quadArea.top + 32) {
        EditorRenderer::DrawInsetFrame(dc, quadArea, RGB(32, 32, 32), RGB(120, 120, 120), RGB(16, 16, 16));
        if (callbacks.drawViewportPreview) {
            callbacks.drawViewportPreview(quadArea);
        }
        return;
    }

    if (model.full3DViewport) {
        EditorRenderer::DrawInsetFrame(dc, quadArea, RGB(30, 34, 40), RGB(214, 176, 92), RGB(18, 20, 24));
        EditorRenderer::DrawTextLine(dc,
                                     RECT{quadArea.left + 8, quadArea.top + 6, quadArea.right - 8, quadArea.top + 24},
                                     "PERSPECTIVE   |   Tab toggles quad views",
                                     RGB(255, 242, 205),
                                     theme.smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        drawCameraAndMeta();
        return;
    }

    const int midX = (quadArea.left + quadArea.right) / 2;
    const int midY = (quadArea.top + quadArea.bottom) / 2;
    const RECT cellTop{quadArea.left, quadArea.top, midX - 1, midY - 1};
    const RECT cellSide{midX + 1, quadArea.top, quadArea.right, midY - 1};
    const RECT cellFront{quadArea.left, midY + 1, midX - 1, quadArea.bottom};
    const RECT cellCamera{midX + 1, midY + 1, quadArea.right, quadArea.bottom};

    StrokeLine(dc, midX, quadArea.top, midX, quadArea.bottom, RGB(24, 24, 24), 2);
    StrokeLine(dc, quadArea.left, midY, quadArea.right, midY, RGB(24, 24, 24), 2);

    if (callbacks.drawTopView) {
        callbacks.drawTopView(cellTop);
    }
    if (callbacks.drawSideView) {
        callbacks.drawSideView(cellSide);
    }
    if (callbacks.drawFrontView) {
        callbacks.drawFrontView(cellFront);
    }

    EditorRenderer::DrawInsetFrame(dc, cellCamera, RGB(28, 32, 38), RGB(214, 176, 92), RGB(12, 12, 12));
    const RECT cameraInner{cellCamera.left + 2, cellCamera.top + 2, cellCamera.right - 2, cellCamera.bottom - 2};
    EditorRenderer::DrawTextLine(dc,
                                 RECT{cameraInner.left + 6, cameraInner.top + 4, cameraInner.right - 6, cameraInner.top + 22},
                                 "PERSPECTIVE",
                                 RGB(255, 242, 205),
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
