#pragma once

#include "EditorUiTheme.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
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

struct EditorToolbarLayout {
    bool compact = false;
    RECT select{};
    RECT create{};
    RECT camera{};
    RECT tacticalGroup{};
    RECT translate{};
    RECT rotate{};
    RECT scale{};
    RECT axisX{};
    RECT axisY{};
    RECT axisZ{};
    RECT snapToggle{};
    RECT snapStepDown{};
    RECT snapStepUp{};
    RECT resolutionScale{};
    RECT foundryGroup{};
    AuthoringToolbarRects authoring{};
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
    HFONT monoFont = nullptr;
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
    bool resolutionScalingEnabled = true;
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

enum class CameraRailHit {
    None,
    Trackball,
    TrackballCenter,
    PanCross,
    PanCenter,
    DepthCross,
    HomeButton,
    FrameSelectionButton,
    FrameAllButton,
    ResolutionScaleButton,
};

struct CameraRailLayout {
    RECT panel{};
    RECT trackballBounds{};
    RECT panCrossBounds{};
    RECT depthCrossBounds{};
    RECT homeButtonBounds{};
    RECT frameSelectionButtonBounds{};
    RECT frameAllButtonBounds{};
    RECT resolutionScaleButtonBounds{};
    RECT trackballNubBounds{};
    RECT panNubBounds{};
    RECT depthNubBounds{};
    POINT trackballCenter{};
    POINT panCenter{};
    POINT depthCenter{};
    LONG orbitRadius = 0;
    LONG panRadius = 0;
    LONG depthHalfWidth = 0;
    LONG depthHalfHeight = 0;
};

struct CameraRailVisualModel {
    float orbitOffsetX = 0.0f;
    float orbitOffsetY = 0.0f;
    float panOffsetX = 0.0f;
    float panOffsetY = 0.0f;
    float depthOffsetX = 0.0f;
    float depthOffsetY = 0.0f;
    bool orbitActive = false;
    bool panActive = false;
    bool depthActive = false;
    bool resolutionScalingEnabled = true;
};

struct CameraRailSpriteDiagnostics {
    bool ready = false;
    std::uint32_t resolvedAssetCount = 0;
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
[[nodiscard]] EditorToolbarLayout ComputeEditorToolbarLayout(const RECT& toolStrip);
[[nodiscard]] CameraRailLayout ComputeCameraRailLayout(const RECT& railRect);
[[nodiscard]] TopChromeRects ComputeTopChromeRects(const RECT& topBar);
[[nodiscard]] std::optional<RECT> ComputeToolStripStatusRect(
    const RECT& toolStrip,
    const EditorToolbarLayout& layout);
[[nodiscard]] std::string EditorToolbarTooltipAtPoint(const RECT& toolStrip, const POINT& point);
[[nodiscard]] CameraRailHit HitTestCameraRail(const RECT& railRect, const POINT& point);
[[nodiscard]] CameraRailSpriteDiagnostics GetCameraRailSpriteDiagnostics();
struct ViewportMenuBannerHits {
    RECT banner{};
    RECT create{};
    RECT help{};
    bool measured = false;
};

/// Measures Create/Help hit boxes from the real menu label + font (not char-count ratios).
[[nodiscard]] ViewportMenuBannerHits MeasureViewportMenuBannerHits(HDC dc,
                                                                   HFONT font,
                                                                   const RECT& viewportInner,
                                                                   bool createMenuActive);

[[nodiscard]] bool HitTestViewportCreateMenu(const RECT& viewportInner,
                                             const POINT& point,
                                             bool createMenuActive = false,
                                             HFONT font = nullptr);
[[nodiscard]] bool HitTestViewportHelpMenu(const RECT& viewportInner,
                                           const POINT& point,
                                           bool createMenuActive = false,
                                           HFONT font = nullptr);

void RenderEditorTopChrome(HDC dc,
                           const RECT& client,
                           const RECT& topBar,
                           const EditorViewportChromeModel& model,
                           const EditorViewportTheme& theme);
void RenderEditorToolStrip(HDC dc,
                           const RECT& toolStrip,
                           const EditorViewportToolStripModel& model,
                           const EditorViewportTheme& theme);
void RenderEditorToolbarTooltip(HDC dc,
                                const RECT& toolStrip,
                                const POINT& point,
                                const EditorViewportTheme& theme);
void RenderEditorCameraRail(HDC dc,
                            const RECT& railRect,
                            const EditorViewportTheme& theme,
                            const CameraRailVisualModel& model);
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
void PresentEditorFrame(HDC windowDc,
                        HDC backBufferDc,
                        int width,
                        int height,
                        const RECT* excludedClientRect,
                        const RECT* blitRect = nullptr);
#endif

} // namespace ri::editor
