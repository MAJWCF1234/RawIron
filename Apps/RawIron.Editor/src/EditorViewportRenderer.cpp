#include "EditorViewportRenderer.h"

#include "EditorRenderer.h"
#include "EditorUiTheme.h"
#include "RawIron/Render/PreviewTexture.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>

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

void FillEllipse(HDC dc, const RECT& rect, const COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(dc, brush));
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, GetStockObject(NULL_PEN)));
    Ellipse(dc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(brush);
}

bool PointInEllipse(const RECT& rect, const POINT& point) {
    const float rx = static_cast<float>(rect.right - rect.left) * 0.5f;
    const float ry = static_cast<float>(rect.bottom - rect.top) * 0.5f;
    if (rx <= 0.0f || ry <= 0.0f) {
        return false;
    }
    const float cx = static_cast<float>(rect.left) + rx;
    const float cy = static_cast<float>(rect.top) + ry;
    const float dx = (static_cast<float>(point.x) - cx) / rx;
    const float dy = (static_cast<float>(point.y) - cy) / ry;
    return (dx * dx) + (dy * dy) <= 1.0f;
}

RECT MakeRect(const LONG left, const LONG top, const LONG width, const LONG height) {
    return RECT{left, top, left + width, top + height};
}

POINT CenterOf(const RECT& rect) {
    return POINT{(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
}

RECT RectFromCenter(const POINT& center, const LONG width, const LONG height) {
    return RECT{center.x - width / 2, center.y - height / 2, center.x - width / 2 + width, center.y - height / 2 + height};
}

struct CameraRailSpriteSet {
    ri::render::software::RgbaImage orbitPad{};
    ri::render::software::RgbaImage orbitPadGlow{};
    ri::render::software::RgbaImage orbitNub{};
    ri::render::software::RgbaImage orbitNubGlow{};
    ri::render::software::RgbaImage dpadBase{};
    ri::render::software::RgbaImage dpadNub{};
    ri::render::software::RgbaImage dpadGlow{};
    ri::render::software::RgbaImage dpadSmallGlow{};
    ri::render::software::RgbaImage dpadNorthGlow{};
    ri::render::software::RgbaImage dpadSouthGlow{};
    ri::render::software::RgbaImage dpadEastGlow{};
    ri::render::software::RgbaImage dpadWestGlow{};
    ri::render::software::RgbaImage depthPad{};
    ri::render::software::RgbaImage depthPadGlow{};
    ri::render::software::RgbaImage depthNub{};
    ri::render::software::RgbaImage depthNubGlow{};
    ri::render::software::RgbaImage homeButton{};
    ri::render::software::RgbaImage homeButtonGlow{};
    ri::render::software::RgbaImage frameSelectionButton{};
    ri::render::software::RgbaImage frameSelectionButtonGlow{};
    ri::render::software::RgbaImage frameAllButton{};
    ri::render::software::RgbaImage frameAllButtonGlow{};
    ri::render::software::RgbaImage resolutionScaleButton{};
    ri::render::software::RgbaImage resolutionScaleButtonGlow{};
    bool loaded = false;
    std::uint32_t resolvedAssetCount = 0;
};

CameraRailSpriteSet& CameraRailSprites() {
    static CameraRailSpriteSet sprites{};
    if (sprites.loaded) {
        return sprites;
    }
    const std::filesystem::path root = "O:/RawIron/Assets/UI/mobile-controls-1.0/Sprites";
    CameraRailSpriteSet& spriteRef = sprites;
    const auto load = [&spriteRef](ri::render::software::RgbaImage& image, const std::filesystem::path& path) {
        image = ri::render::software::LoadRgbaImageFile(path);
        if (image.Valid()) {
            ++spriteRef.resolvedAssetCount;
        }
    };
    load(sprites.orbitPad, root / "Style A/Default/joystick_circle_pad_a.png");
    load(sprites.orbitNub, root / "Style A/Default/joystick_circle_nub_a.png");
    load(sprites.orbitPadGlow, root / "Highlights A/Default/joystick_circle_pad_highlight.png");
    load(sprites.orbitNubGlow, root / "Highlights A/Default/joystick_circle_nub_highlight.png");
    load(sprites.dpadBase, root / "Style A/Default/dpad.png");
    load(sprites.dpadNub, root / "Style A/Default/joystick_hexagon_nub_b.png");
    load(sprites.dpadGlow, root / "Highlights A/Default/dpad_highlight.png");
    load(sprites.dpadSmallGlow, root / "Highlights A/Default/dpad_small_highlight.png");
    load(sprites.dpadNorthGlow, root / "Highlights A/Default/dpad_element_north_highlight.png");
    load(sprites.dpadSouthGlow, root / "Highlights A/Default/dpad_element_south_highlight.png");
    load(sprites.dpadEastGlow, root / "Highlights A/Default/dpad_element_east_highlight.png");
    load(sprites.dpadWestGlow, root / "Highlights A/Default/dpad_element_west_highlight.png");
    load(sprites.depthPad, root / "Style A/Default/joystick_square_pad_a.png");
    load(sprites.depthPadGlow, root / "Highlights A/Default/joystick_square_pad_highlight.png");
    load(sprites.depthNub, root / "Style A/Default/joystick_square_nub_a.png");
    load(sprites.depthNubGlow, root / "Highlights A/Default/joystick_square_nub_highlight.png");
    load(sprites.homeButton, root / "Style A/Default/button_circle.png");
    load(sprites.homeButtonGlow, root / "Highlights A/Default/button_circle_highlight.png");
    load(sprites.frameSelectionButton, root / "Style A/Default/button_diamond.png");
    load(sprites.frameSelectionButtonGlow, root / "Highlights A/Default/button_diamond_highlight.png");
    load(sprites.frameAllButton, root / "Style A/Default/button_hexagon.png");
    load(sprites.frameAllButtonGlow, root / "Highlights A/Default/button_hexagon_highlight.png");
    load(sprites.resolutionScaleButton, root / "Style A/Default/button_square.png");
    load(sprites.resolutionScaleButtonGlow, root / "Highlights A/Default/button_square_highlight.png");
    sprites.loaded = true;
    return sprites;
}

} // namespace

AuthoringToolbarRects ComputeAuthoringToolbarRects(const RECT& toolStrip) {
    return ComputeEditorToolbarLayout(toolStrip).authoring;
}

EditorToolbarLayout ComputeEditorToolbarLayout(const RECT& toolStrip) {
    const LONG width = toolStrip.right - toolStrip.left;
    const bool compact = width < 1600;
    const LONG rowTop = toolStrip.top + 8;
    const LONG rowBot = toolStrip.bottom - 8;
    EditorToolbarLayout layout{};
    layout.compact = compact;

    if (compact) {
        layout.select = {toolStrip.left + 58, rowTop, toolStrip.left + 96, rowBot};
        layout.create = {toolStrip.left + 100, rowTop, toolStrip.left + 138, rowBot};
        layout.camera = {toolStrip.left + 142, rowTop, toolStrip.left + 188, rowBot};
        layout.tacticalGroup = {toolStrip.left + 196, rowTop - 3, toolStrip.left + 632, rowBot + 3};
        layout.translate = {toolStrip.left + 204, rowTop, toolStrip.left + 240, rowBot};
        layout.rotate = {toolStrip.left + 244, rowTop, toolStrip.left + 280, rowBot};
        layout.scale = {toolStrip.left + 284, rowTop, toolStrip.left + 320, rowBot};
        layout.axisX = {toolStrip.left + 328, rowTop, toolStrip.left + 360, rowBot};
        layout.axisY = {toolStrip.left + 364, rowTop, toolStrip.left + 396, rowBot};
        layout.axisZ = {toolStrip.left + 400, rowTop, toolStrip.left + 432, rowBot};
        layout.snapToggle = {toolStrip.left + 440, rowTop, toolStrip.left + 488, rowBot};
        layout.snapStepDown = {toolStrip.left + 492, rowTop, toolStrip.left + 520, rowBot};
        layout.snapStepUp = {toolStrip.left + 524, rowTop, toolStrip.left + 552, rowBot};
        layout.resolutionScale = {toolStrip.left + 560, rowTop, toolStrip.left + 624, rowBot};

        constexpr LONG kFoundryWidth = 428;
        layout.foundryGroup = {toolStrip.right - 12 - kFoundryWidth, rowTop - 3, toolStrip.right - 12, rowBot + 3};
        const LONG x0 = layout.foundryGroup.left + 8;
        layout.authoring.addCube = {x0, rowTop, x0 + 48, rowBot};
        layout.authoring.addPlane = {x0 + 52, rowTop, x0 + 100, rowBot};
        layout.authoring.addTrigger = {x0 + 104, rowTop, x0 + 158, rowBot};
        layout.authoring.addLight = {x0 + 162, rowTop, x0 + 210, rowBot};
        layout.authoring.duplicate = {x0 + 214, rowTop, x0 + 264, rowBot};
        layout.authoring.exportCsv = {x0 + 268, rowTop, x0 + 324, rowBot};
        layout.authoring.play = {x0 + 328, rowTop, layout.foundryGroup.right - 8, rowBot};
        return layout;
    }

    layout.select = {toolStrip.left + 78, rowTop, toolStrip.left + 130, rowBot};
    layout.create = {toolStrip.left + 134, rowTop, toolStrip.left + 188, rowBot};
    layout.camera = {toolStrip.left + 194, rowTop, toolStrip.left + 248, rowBot};
    layout.tacticalGroup = {toolStrip.left + 254, rowTop - 3, toolStrip.left + 874, rowBot + 3};
    layout.translate = {toolStrip.left + 326, rowTop, toolStrip.left + 396, rowBot};
    layout.rotate = {toolStrip.left + 400, rowTop, toolStrip.left + 468, rowBot};
    layout.scale = {toolStrip.left + 472, rowTop, toolStrip.left + 538, rowBot};
    layout.axisX = {toolStrip.left + 546, rowTop, toolStrip.left + 584, rowBot};
    layout.axisY = {toolStrip.left + 588, rowTop, toolStrip.left + 626, rowBot};
    layout.axisZ = {toolStrip.left + 630, rowTop, toolStrip.left + 668, rowBot};
    layout.snapToggle = {toolStrip.left + 676, rowTop, toolStrip.left + 750, rowBot};
    layout.snapStepDown = {toolStrip.left + 754, rowTop, toolStrip.left + 786, rowBot};
    layout.snapStepUp = {toolStrip.left + 790, rowTop, toolStrip.left + 822, rowBot};
    layout.resolutionScale = {toolStrip.left + 830, rowTop, toolStrip.left + 866, rowBot};

    constexpr LONG kFoundryWidth = 664;
    layout.foundryGroup = {toolStrip.right - 12 - kFoundryWidth, rowTop - 3, toolStrip.right - 12, rowBot + 3};
    const LONG x0 = layout.foundryGroup.left + 78;
    layout.authoring.addCube = {x0, rowTop, x0 + 80, rowBot};
    layout.authoring.addPlane = {x0 + 86, rowTop, x0 + 166, rowBot};
    layout.authoring.addTrigger = {x0 + 172, rowTop, x0 + 268, rowBot};
    layout.authoring.addLight = {x0 + 274, rowTop, x0 + 350, rowBot};
    layout.authoring.duplicate = {x0 + 356, rowTop, x0 + 432, rowBot};
    layout.authoring.exportCsv = {x0 + 438, rowTop, x0 + 518, rowBot};
    layout.authoring.play = {x0 + 524, rowTop, layout.foundryGroup.right - 8, rowBot};
    return layout;
}

CameraRailLayout ComputeCameraRailLayout(const RECT& railRect) {
    CameraRailLayout layout{};
    layout.panel = railRect;
    if (railRect.right <= railRect.left || railRect.bottom <= railRect.top) {
        return layout;
    }

    const LONG width = railRect.right - railRect.left;
    const LONG height = railRect.bottom - railRect.top;
    const LONG centerX = railRect.left + width / 2;
    const LONG topMargin = 12;
    const LONG gap = 14;
    const LONG sideButtonSize = std::max(24L, std::min(width / 4, 32L));
    const LONG sideInset = 6;
    const LONG orbitSize = std::max(56L, std::min(width - 10L, 84L));
    const LONG panSize = std::max(52L, std::min(width - 18L, 74L));
    const LONG depthWidth = std::max(34L, std::min(width - 40L, 48L));
    const LONG depthHeight = std::max(68L, std::min(height - orbitSize - panSize - gap * 2 - topMargin - 8L, 96L));
    layout.trackballBounds = MakeRect(centerX - orbitSize / 2, railRect.top + topMargin, orbitSize, orbitSize);
    layout.panCrossBounds = MakeRect(centerX - panSize / 2, layout.trackballBounds.bottom + gap, panSize, panSize);
    layout.depthCrossBounds = MakeRect(centerX - depthWidth / 2, layout.panCrossBounds.bottom + gap, depthWidth, depthHeight);
    layout.trackballCenter = CenterOf(layout.trackballBounds);
    layout.panCenter = CenterOf(layout.panCrossBounds);
    layout.depthCenter = CenterOf(layout.depthCrossBounds);
    layout.orbitRadius = std::max(12L, orbitSize / 2 - 16L);
    layout.panRadius = std::max(12L, panSize / 2 - 12L);
    layout.depthHalfWidth = std::max(10L, depthWidth / 2 - 6L);
    layout.depthHalfHeight = std::max(14L, depthHeight / 2 - 12L);
    layout.homeButtonBounds = RectFromCenter(POINT{railRect.left + sideInset + sideButtonSize / 2, layout.trackballCenter.y},
                                             sideButtonSize,
                                             sideButtonSize);
    layout.frameSelectionButtonBounds =
        RectFromCenter(POINT{railRect.left + sideInset + sideButtonSize / 2, layout.panCenter.y},
                       sideButtonSize,
                       sideButtonSize);
    layout.frameAllButtonBounds =
        RectFromCenter(POINT{railRect.right - sideInset - sideButtonSize / 2, layout.trackballCenter.y},
                       sideButtonSize,
                       sideButtonSize);
    layout.resolutionScaleButtonBounds =
        RectFromCenter(POINT{railRect.right - sideInset - sideButtonSize / 2, layout.depthCenter.y},
                       sideButtonSize,
                       sideButtonSize);
    layout.trackballNubBounds = RectFromCenter(layout.trackballCenter, orbitSize / 2, orbitSize / 2);
    layout.panNubBounds = RectFromCenter(layout.panCenter, std::max(18L, panSize / 3), std::max(18L, panSize / 3));
    layout.depthNubBounds = RectFromCenter(layout.depthCenter, depthWidth - 6, std::max(18L, depthWidth - 6));
    return layout;
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

std::optional<RECT> ComputeToolStripStatusRect(
    const RECT& toolStrip,
    const EditorToolbarLayout& layout) {
    if (layout.compact) {
        return std::nullopt;
    }
    const LONG left = layout.tacticalGroup.right + 10;
    const LONG right = layout.foundryGroup.left - 10;
    if (right - left < 240) {
        return std::nullopt;
    }
    return RECT{left, toolStrip.top + 8, right, toolStrip.bottom - 8};
}

std::string EditorToolbarTooltipAtPoint(const RECT& toolStrip, const POINT& point) {
    const EditorToolbarLayout layout = ComputeEditorToolbarLayout(toolStrip);
    const auto hit = [&point](const RECT& rect) { return PtInRect(&rect, point) != FALSE; };
    if (hit(layout.select)) return "Select tool";
    if (hit(layout.create)) return "Create / stamp tool";
    if (hit(layout.camera)) return "Camera navigation";
    if (hit(layout.translate)) return "Translate (T)";
    if (hit(layout.rotate)) return "Rotate (R)";
    if (hit(layout.scale)) return "Scale (S)";
    if (hit(layout.axisX)) return "Constrain to X";
    if (hit(layout.axisY)) return "Constrain to Y";
    if (hit(layout.axisZ)) return "Constrain to Z";
    if (hit(layout.snapToggle)) return "Toggle grid snapping";
    if (hit(layout.snapStepDown)) return "Decrease grid step";
    if (hit(layout.snapStepUp)) return "Increase grid step";
    if (hit(layout.resolutionScale)) return "Toggle half-resolution viewport while camera moves";
    if (hit(layout.authoring.addCube)) return "Foundry: create cube";
    if (hit(layout.authoring.addPlane)) return "Foundry: create plane";
    if (hit(layout.authoring.addTrigger)) return "Foundry: create trigger";
    if (hit(layout.authoring.addLight)) return "Foundry: create light";
    if (hit(layout.authoring.duplicate)) return "Duplicate selection";
    if (hit(layout.authoring.exportCsv)) return "Export assembly CSV";
    if (hit(layout.authoring.play)) return "Playtest";
    return {};
}

CameraRailHit HitTestCameraRail(const RECT& railRect, const POINT& point) {
    const CameraRailLayout layout = ComputeCameraRailLayout(railRect);
    if (PtInRect(&layout.homeButtonBounds, point) != FALSE) return CameraRailHit::HomeButton;
    if (PtInRect(&layout.frameSelectionButtonBounds, point) != FALSE) return CameraRailHit::FrameSelectionButton;
    if (PtInRect(&layout.frameAllButtonBounds, point) != FALSE) return CameraRailHit::FrameAllButton;
    if (PtInRect(&layout.resolutionScaleButtonBounds, point) != FALSE) return CameraRailHit::ResolutionScaleButton;
    if (PointInEllipse(layout.trackballBounds, point)) return CameraRailHit::Trackball;
    if (PtInRect(&layout.panCrossBounds, point) != FALSE) return CameraRailHit::PanCross;
    if (PtInRect(&layout.depthCrossBounds, point) != FALSE) return CameraRailHit::DepthCross;
    return CameraRailHit::None;
}

CameraRailSpriteDiagnostics GetCameraRailSpriteDiagnostics() {
    const CameraRailSpriteSet& sprites = CameraRailSprites();
    return CameraRailSpriteDiagnostics{
        .ready = sprites.orbitPad.Valid()
            && sprites.orbitNub.Valid()
            && sprites.orbitPadGlow.Valid()
            && sprites.orbitNubGlow.Valid()
            && sprites.dpadBase.Valid()
            && sprites.dpadNub.Valid()
            && sprites.dpadNorthGlow.Valid()
            && sprites.dpadSouthGlow.Valid()
            && sprites.dpadEastGlow.Valid()
            && sprites.dpadWestGlow.Valid()
            && sprites.depthPad.Valid()
            && sprites.depthNub.Valid()
            && sprites.depthNubGlow.Valid()
            && sprites.homeButton.Valid()
            && sprites.homeButtonGlow.Valid()
            && sprites.frameSelectionButton.Valid()
            && sprites.frameSelectionButtonGlow.Valid()
            && sprites.frameAllButton.Valid()
            && sprites.frameAllButtonGlow.Valid()
            && sprites.resolutionScaleButton.Valid()
            && sprites.resolutionScaleButtonGlow.Valid(),
        .resolvedAssetCount = sprites.resolvedAssetCount,
    };
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
                                 RECT{toolStrip.left + 12, toolStrip.top + 8, toolStrip.left + 68, toolStrip.bottom - 8},
                                 "TOOLS",
                                 EditorUiTheme::kTextOnDark,
                                 theme.headerFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    const EditorToolbarLayout layout = ComputeEditorToolbarLayout(toolStrip);
    EditorRenderer::DrawPanelFrame(
        dc,
        layout.tacticalGroup,
        EditorUiTheme::kToolStripFill,
        EditorUiTheme::kToolStripHi,
        EditorUiTheme::kToolStripShadow);
    EditorRenderer::DrawPanelFrame(
        dc,
        layout.foundryGroup,
        EditorUiTheme::kToolStripFill,
        EditorUiTheme::kCopper,
        EditorUiTheme::kToolStripShadow);
    if (!layout.compact) {
        EditorRenderer::DrawTextLine(
            dc,
            RECT{layout.tacticalGroup.left + 8, toolStrip.top + 8, layout.translate.left - 6, toolStrip.bottom - 8},
            "TACTICAL",
            EditorUiTheme::kTextMuted,
            theme.monoFont != nullptr ? theme.monoFont : theme.smallFont,
            DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        EditorRenderer::DrawTextLine(
            dc,
            RECT{layout.foundryGroup.left + 8, toolStrip.top + 8, layout.authoring.addCube.left - 6, toolStrip.bottom - 8},
            "FOUNDRY",
            EditorUiTheme::kCopper,
            theme.monoFont != nullptr ? theme.monoFont : theme.smallFont,
            DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    }
    const auto toolBtn = [&](const RECT& rect,
                             const std::string& label,
                             const bool active,
                             const EditorToolbarStyle style = EditorToolbarStyle::Dark) {
        EditorRenderer::DrawToolbarButton(dc, rect, label, active, theme.smallFont, style);
    };
    toolBtn(layout.select, layout.compact ? "Q" : "Select", model.toolMode == EditorToolMode::Select);
    toolBtn(layout.create, layout.compact ? "C" : "Create", model.toolMode == EditorToolMode::Create);
    toolBtn(layout.camera, layout.compact ? "CAM" : "Camera", model.toolMode == EditorToolMode::Camera);
    toolBtn(layout.translate, layout.compact ? "T" : "Move", model.editModeLabel == "Move");
    toolBtn(layout.rotate, layout.compact ? "R" : "Rotate", model.editModeLabel == "Rotate");
    toolBtn(layout.scale, layout.compact ? "S" : "Scale", model.editModeLabel == "Scale");
    toolBtn(layout.axisX, "X", model.axisLabel == "X");
    toolBtn(layout.axisY, "Y", model.axisLabel == "Y");
    toolBtn(layout.axisZ, "Z", model.axisLabel == "Z");
    toolBtn(layout.snapToggle,
            layout.compact ? "#" : std::string("Snap ") + (model.gridSnapEnabled ? "On" : "Off"),
            model.gridSnapEnabled);
    toolBtn(layout.snapStepDown, "-", false);
    toolBtn(layout.snapStepUp, "+", false);
    toolBtn(layout.resolutionScale,
            layout.compact ? "1/2" : (model.resolutionScalingEnabled ? "1/2" : "1:1"),
            model.resolutionScalingEnabled);

    const AuthoringToolbarRects& authoringPaint = layout.authoring;
    toolBtn(authoringPaint.addCube, layout.compact ? "CB" : "+ Cube", false, EditorToolbarStyle::Creator);
    toolBtn(authoringPaint.addPlane, layout.compact ? "PL" : "+ Plane", false, EditorToolbarStyle::Creator);
    toolBtn(authoringPaint.addTrigger, layout.compact ? "TRG" : "+ Trigger", false, EditorToolbarStyle::Creator);
    toolBtn(authoringPaint.addLight, layout.compact ? "LGT" : "+ Light", false, EditorToolbarStyle::Creator);
    toolBtn(authoringPaint.duplicate, layout.compact ? "DUP" : "Duplicate", false);
    toolBtn(authoringPaint.exportCsv, layout.compact ? "EXP" : "Export", false);
    toolBtn(authoringPaint.play, layout.compact ? "RUN" : "Play", false);
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
    if (const std::optional<RECT> statusRect = ComputeToolStripStatusRect(toolStrip, layout)) {
        EditorRenderer::DrawTextLine(
            dc,
            *statusRect,
            modeLine + "  |  Step " + model.editStepLabel + "  |  Undo " + std::to_string(model.undoDepth) +
                "  |  Grid " + model.gridSnapLabel + "  |  Authored " + std::to_string(model.authoredCount) +
                "  |  Triggers " + std::to_string(model.triggerCount),
            EditorUiTheme::kTextMuted,
            theme.monoFont != nullptr ? theme.monoFont : theme.smallFont,
            DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
    RestoreDC(dc, savedDc);
}

void RenderEditorToolbarTooltip(
    HDC dc,
    const RECT& toolStrip,
    const POINT& point,
    const EditorViewportTheme& theme) {
    if (!ComputeEditorToolbarLayout(toolStrip).compact) {
        return;
    }
    const std::string tooltip = EditorToolbarTooltipAtPoint(toolStrip, point);
    if (tooltip.empty()) {
        return;
    }
    constexpr LONG kTooltipWidth = 230;
    constexpr LONG kTooltipHeight = 24;
    const LONG left = std::clamp(
        point.x + 12L,
        toolStrip.left + 4L,
        std::max(toolStrip.left + 4L, toolStrip.right - kTooltipWidth - 4L));
    const RECT tooltipRect{left, toolStrip.bottom + 3, left + kTooltipWidth, toolStrip.bottom + 3 + kTooltipHeight};
    EditorRenderer::DrawPanelFrame(
        dc,
        tooltipRect,
        EditorUiTheme::kTooltipFill,
        EditorUiTheme::kCopper,
        EditorUiTheme::kWellShadow);
    EditorRenderer::DrawTextLine(
        dc,
        RECT{tooltipRect.left + 7, tooltipRect.top + 3, tooltipRect.right - 7, tooltipRect.bottom - 3},
        tooltip,
        EditorUiTheme::kTextOnPanel,
        theme.monoFont != nullptr ? theme.monoFont : theme.smallFont,
        DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

void RenderEditorCameraRail(
    HDC dc,
    const RECT& railRect,
    const EditorViewportTheme& theme,
    const CameraRailVisualModel& model) {
    if (railRect.right <= railRect.left || railRect.bottom <= railRect.top) {
        return;
    }
    const CameraRailSpriteSet& sprites = CameraRailSprites();
    const CameraRailLayout layout = ComputeCameraRailLayout(railRect);
    const HFONT microFont = theme.monoFont != nullptr ? theme.monoFont : theme.smallFont;

    if (!GetCameraRailSpriteDiagnostics().ready) {
        EditorRenderer::DrawTextLine(dc,
                                     railRect,
                                     "Nav sprites missing",
                                     EditorUiTheme::kTextStatusGold,
                                     theme.monoFont != nullptr ? theme.monoFont : theme.smallFont,
                                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    const auto drawMicroLabel = [&](const RECT& rect, const std::string& label) {
        EditorRenderer::DrawTextLine(dc,
                                     RECT{rect.left - 8, rect.top - 12, rect.right + 8, rect.top},
                                     label,
                                     EditorUiTheme::kTextMuted,
                                     microFont,
                                     DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    };
    const auto drawSpriteButton = [&](const RECT& rect,
                                      const ri::render::software::RgbaImage& base,
                                      const ri::render::software::RgbaImage& glow,
                                      const std::string& label,
                                      const bool active) {
        EditorRenderer::BlitRgbaImage(dc, rect, base);
        if (active) {
            EditorRenderer::BlitRgbaImage(dc, rect, glow, 235);
        }
        EditorRenderer::DrawTextLine(dc,
                                     RECT{rect.left + 1, rect.top + 1, rect.right - 1, rect.bottom - 1},
                                     label,
                                     EditorUiTheme::kTextOnPanel,
                                     microFont,
                                     DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    };

    drawMicroLabel(layout.trackballBounds, "ORB");
    EditorRenderer::BlitRgbaImage(dc, layout.trackballBounds, sprites.orbitPad);
    if (model.orbitActive) {
        EditorRenderer::BlitRgbaImage(dc, layout.trackballBounds, sprites.orbitPadGlow, 235);
    }
    const POINT orbitNubCenter{
        layout.trackballCenter.x + static_cast<LONG>(std::lround(model.orbitOffsetX)),
        layout.trackballCenter.y + static_cast<LONG>(std::lround(model.orbitOffsetY))};
    const RECT orbitNubRect = RectFromCenter(orbitNubCenter,
                                             layout.trackballNubBounds.right - layout.trackballNubBounds.left,
                                             layout.trackballNubBounds.bottom - layout.trackballNubBounds.top);
    EditorRenderer::BlitRgbaImage(dc, orbitNubRect, sprites.orbitNub);
    if (model.orbitActive) {
        EditorRenderer::BlitRgbaImage(dc, orbitNubRect, sprites.orbitNubGlow, 240);
    }

    drawMicroLabel(layout.panCrossBounds, "PAN");
    EditorRenderer::BlitRgbaImage(dc, layout.panCrossBounds, sprites.dpadBase);
    if (model.panActive) {
        EditorRenderer::BlitRgbaImage(dc, layout.panCrossBounds, sprites.dpadGlow, 180);
        const float absX = std::abs(model.panOffsetX);
        const float absY = std::abs(model.panOffsetY);
        if (absY >= absX && model.panOffsetY < -10.0f) {
            EditorRenderer::BlitRgbaImage(dc, layout.panCrossBounds, sprites.dpadNorthGlow, 255);
        }
        if (absY >= absX && model.panOffsetY > 10.0f) {
            EditorRenderer::BlitRgbaImage(dc, layout.panCrossBounds, sprites.dpadSouthGlow, 255);
        }
        if (absX >= absY && model.panOffsetX > 10.0f) {
            EditorRenderer::BlitRgbaImage(dc, layout.panCrossBounds, sprites.dpadEastGlow, 255);
        }
        if (absX >= absY && model.panOffsetX < -10.0f) {
            EditorRenderer::BlitRgbaImage(dc, layout.panCrossBounds, sprites.dpadWestGlow, 255);
        }
    }
    const POINT panNubCenter{
        layout.panCenter.x + static_cast<LONG>(std::lround(model.panOffsetX)),
        layout.panCenter.y + static_cast<LONG>(std::lround(model.panOffsetY))};
    const RECT panNubRect = RectFromCenter(panNubCenter,
                                           layout.panNubBounds.right - layout.panNubBounds.left,
                                           layout.panNubBounds.bottom - layout.panNubBounds.top);
    EditorRenderer::BlitRgbaImage(dc, panNubRect, sprites.dpadNub);

    drawMicroLabel(layout.depthCrossBounds, "ZOOM");
    EditorRenderer::BlitRgbaImage(dc, layout.depthCrossBounds, sprites.depthPad);
    if (model.depthActive) {
        EditorRenderer::BlitRgbaImage(dc, layout.depthCrossBounds, sprites.depthPadGlow, 210);
    }
    const POINT depthNubCenter{
        layout.depthCenter.x + static_cast<LONG>(std::lround(model.depthOffsetX)),
        layout.depthCenter.y + static_cast<LONG>(std::lround(model.depthOffsetY))};
    const RECT depthNubRect = RectFromCenter(depthNubCenter,
                                             layout.depthNubBounds.right - layout.depthNubBounds.left,
                                             layout.depthNubBounds.bottom - layout.depthNubBounds.top);
    EditorRenderer::BlitRgbaImage(dc, depthNubRect, sprites.depthNub);
    if (model.depthActive) {
        EditorRenderer::BlitRgbaImage(dc, depthNubRect, sprites.depthNubGlow, 240);
    }

    drawSpriteButton(layout.homeButtonBounds, sprites.homeButton, sprites.homeButtonGlow, "HM", false);
    drawSpriteButton(layout.frameSelectionButtonBounds,
                     sprites.frameSelectionButton,
                     sprites.frameSelectionButtonGlow,
                     "SEL",
                     false);
    drawSpriteButton(layout.frameAllButtonBounds, sprites.frameAllButton, sprites.frameAllButtonGlow, "ALL", false);
    drawSpriteButton(layout.resolutionScaleButtonBounds,
                     sprites.resolutionScaleButton,
                     sprites.resolutionScaleButtonGlow,
                     model.resolutionScalingEnabled ? "1/2" : "1:1",
                     model.resolutionScalingEnabled);
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
    HFONT statusFont = theme.monoFont != nullptr ? theme.monoFont : theme.smallFont;
    EditorRenderer::DrawTextLine(dc,
                                 RECT{statusBar.left + 12, statusBar.top + 8, statusBar.right - 12, statusBar.top + 28},
                                 model.consoleLine,
                                 EditorUiTheme::kTextOnPanel,
                                 statusFont,
                                 DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{statusBar.left + 12, statusBar.top + 30, statusBar.right - 12, statusBar.top + 50},
                                 model.controlsLine,
                                 EditorUiTheme::kTextOnPanelMuted,
                                 statusFont,
                                 DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    EditorRenderer::DrawTextLine(dc,
                                 RECT{statusBar.left + 12, statusBar.top + 50, statusBar.right - 12, statusBar.bottom - 8},
                                 model.stateLine,
                                 EditorUiTheme::kTextStatusGold,
                                 statusFont,
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

void PresentEditorFrame(HDC windowDc,
                        HDC backBufferDc,
                        const int width,
                        const int height,
                        const RECT* excludedClientRect,
                        const RECT* blitRect) {
    RECT copyRect{0, 0, width, height};
    if (blitRect != nullptr) {
        copyRect = *blitRect;
        copyRect.left = std::max(0L, copyRect.left);
        copyRect.top = std::max(0L, copyRect.top);
        copyRect.right = std::min(static_cast<LONG>(width), copyRect.right);
        copyRect.bottom = std::min(static_cast<LONG>(height), copyRect.bottom);
    }
    const int copyWidth = std::max(0, static_cast<int>(copyRect.right - copyRect.left));
    const int copyHeight = std::max(0, static_cast<int>(copyRect.bottom - copyRect.top));
    if (copyWidth <= 0 || copyHeight <= 0) {
        return;
    }

    const int savedWindowDc = SaveDC(windowDc);
    if (excludedClientRect != nullptr) {
        ExcludeClipRect(windowDc,
                        excludedClientRect->left,
                        excludedClientRect->top,
                        excludedClientRect->right,
                        excludedClientRect->bottom);
    }
    BitBlt(windowDc,
           copyRect.left,
           copyRect.top,
           copyWidth,
           copyHeight,
           backBufferDc,
           copyRect.left,
           copyRect.top,
           SRCCOPY);
    RestoreDC(windowDc, savedWindowDc);
}
#endif

} // namespace ri::editor
