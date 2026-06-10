#pragma once

#include "EditorUiTheme.h"

#include <cstddef>
#include <functional>
#include <string>

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

#if defined(_WIN32)
struct AuthoringToolbarRects {
    RECT addCube{};
    RECT addPlane{};
    RECT addTrigger{};
    RECT addLight{};
    RECT duplicate{};
    RECT exportCsv{};
    RECT play{};
};

struct TopChromeRects {
    RECT newGame{};
    RECT save{};
    RECT scaffold{};
    RECT exportScene{};
    RECT play{};
    RECT files{};
};

struct EditorViewportTheme {
    HFONT titleFont = nullptr;
    HFONT headerFont = nullptr;
    HFONT bodyFont = nullptr;
    HFONT smallFont = nullptr;
};

struct EditorViewportChromeModel {
    std::string title;
    std::string subtitle;
    std::string focusedWorkspaceGameLabel;
    std::string workspaceLabel;
    bool resourcesModeActive = false;
};

struct EditorViewportToolStripModel {
    EditorToolMode toolMode = EditorToolMode::Select;
    std::string armedPresetLabel;
    std::string editModeLabel;
    std::string axisLabel;
    bool gridSnapEnabled = false;
    std::string editStepLabel;
    std::string gridSnapLabel;
    std::size_t undoDepth = 0;
    std::size_t authoredCount = 0;
    std::size_t triggerCount = 0;
};

struct EditorViewportStatusModel {
    std::string consoleLine;
    std::string controlsLine;
    std::string stateLine;
};

struct EditorViewportBlockModel {
    bool full3DViewport = false;
    bool createMenuActive = false;
    bool showWorldBar = false;
    std::string atmosphereLabel;
    std::string createHintLine;
    int bottomChromeInset = 0;
    int worldBarHeight = 0;
    RECT cameraPlotRect{};
    std::string cameraSummaryLine;
};

struct EditorViewportWorldBarHit {
    bool hitAtmosphereCycle = false;
};

[[nodiscard]] EditorViewportWorldBarHit HitTestViewportWorldBar(const RECT& viewportInner,
                                                                  const POINT& point,
                                                                  bool showWorldBar,
                                                                  int worldBarHeight);

struct EditorViewportBlockCallbacks {
    std::function<void(const RECT&)> drawViewportPreview;
    std::function<void(const RECT&)> drawRuntimeStatsOverlay;
    std::function<void(const RECT&)> drawTopView;
    std::function<void(const RECT&)> drawSideView;
    std::function<void(const RECT&)> drawFrontView;
};

[[nodiscard]] AuthoringToolbarRects ComputeAuthoringToolbarRects(const RECT& toolStrip);
[[nodiscard]] TopChromeRects ComputeTopChromeRects(const RECT& topBar);
[[nodiscard]] bool HitTestViewportCreateMenu(const RECT& viewportInner, const POINT& point);
[[nodiscard]] bool HitTestViewportHelpMenu(const RECT& viewportInner, const POINT& point);

void RenderEditorTopChrome(HDC dc,
                           const RECT& client,
                           const RECT& topBar,
                           const EditorViewportChromeModel& model,
                           const EditorViewportTheme& theme);
void RenderEditorToolStrip(HDC dc,
                           const RECT& toolStrip,
                           const EditorViewportToolStripModel& model,
                           const EditorViewportTheme& theme);
void RenderEditorFramePanels(HDC dc,
                             const RECT& hierarchy,
                             const RECT& viewport,
                             const RECT& inspector,
                             const RECT& hierarchySplitter,
                             const RECT& inspectorSplitter,
                             const RECT& statusBar);
void RenderEditorStatusBar(HDC dc,
                           const RECT& statusBar,
                           const EditorViewportStatusModel& model,
                           const EditorViewportTheme& theme);
void RenderEditorViewportBlock(HDC dc,
                               const RECT& viewportInner,
                               const EditorViewportBlockModel& model,
                               const EditorViewportTheme& theme,
                               const EditorViewportBlockCallbacks& callbacks);
void PresentEditorFrame(HDC windowDc, HDC backBufferDc, int width, int height, const RECT* excludedClientRect);
#endif

} // namespace ri::editor
