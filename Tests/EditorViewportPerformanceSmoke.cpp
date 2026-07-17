#include "Apps/RawIron.Editor/src/EditorViewportPerformance.h"
#include "Apps/RawIron.Editor/src/EditorViewportRenderer.h"

#include <chrono>
#include <cstdlib>

int main() {
    using ri::editor::ComputeViewportRenderSize;
    using ri::editor::ComputeViewportTimerIntervalMs;
    using ri::editor::AdvanceAutoOrbitYaw;
    using ri::editor::AdvanceRailPadSpring;
    using ri::editor::BeginRailPadSpring;
    using ri::editor::DefaultEditorStartupHeight;
    using ri::editor::DefaultEditorStartupWidth;
    using ri::editor::DefaultHierarchyPanelWidth;
    using ri::editor::HasCameraStateChanged;
    using ri::editor::NeedsEditorBackBufferResize;
    using ri::editor::ShouldRenderViewportPreview;
    using ri::editor::ShouldRunLogicLivePreview;
    using ri::editor::ShouldCompositeCreateModeGhost;
    using ri::editor::ShouldRunEditorPreviewAnimation;
    using ri::editor::ShouldPollGamePreviewScripts;
    using ri::editor::ShouldPreferNativeViewport;
    using ri::editor::ShouldTickLogicPreview;
    using ri::editor::IsViewportInteractiveMotion;
    using ri::editor::ViewportCameraState;
    using ri::editor::ViewportRenderPolicy;

    const ViewportRenderPolicy settled{
        .plotWidth = 1280,
        .plotHeight = 720,
        .cameraMoving = false,
        .resolutionScalingEnabled = true,
        .previewDirty = true,
    };
    const auto settledSize = ComputeViewportRenderSize(settled);
    if (settledSize.width != 960 || settledSize.height != 540) {
        return EXIT_FAILURE;
    }

    const ViewportRenderPolicy moving{
        .plotWidth = 1280,
        .plotHeight = 720,
        .cameraMoving = true,
        .resolutionScalingEnabled = true,
        .previewDirty = true,
    };
    const auto movingSize = ComputeViewportRenderSize(moving);
    if (movingSize.width != 432 || movingSize.height != 243) {
        return EXIT_FAILURE;
    }

    ViewportRenderPolicy scalingDisabled = moving;
    scalingDisabled.resolutionScalingEnabled = false;
    const auto unscaledMovingSize = ComputeViewportRenderSize(scalingDisabled);
    if (unscaledMovingSize.width != settledSize.width || unscaledMovingSize.height != settledSize.height) {
        return EXIT_FAILURE;
    }
    if (ComputeViewportTimerIntervalMs(true, 22.0) != 12U
        || ComputeViewportTimerIntervalMs(true, 40.0) != 16U
        || ComputeViewportTimerIntervalMs(false, 50.0) != 66U
        || ComputeViewportTimerIntervalMs(false, 10.0) != 50U) {
        return EXIT_FAILURE;
    }

    if (!IsViewportInteractiveMotion(false, true, false, false)
        || IsViewportInteractiveMotion(false, false, false, false)
        || !ShouldRunLogicLivePreview(true, false, false)
        || ShouldRunLogicLivePreview(false, false, false)
        || !ShouldTickLogicPreview(0U, false)
        || ShouldTickLogicPreview(1U, false)
        || !ShouldTickLogicPreview(1U, true)) {
        return EXIT_FAILURE;
    }

    if (ShouldRunEditorPreviewAnimation(false, std::chrono::milliseconds(99))
        || !ShouldRunEditorPreviewAnimation(false, std::chrono::milliseconds(100))
        || ShouldRunEditorPreviewAnimation(true, std::chrono::seconds(1))) {
        return EXIT_FAILURE;
    }

    if (!ShouldPreferNativeViewport(false, false)
        || ShouldPreferNativeViewport(true, false)
        || ShouldPreferNativeViewport(false, true)) {
        return EXIT_FAILURE;
    }

    if (ShouldPollGamePreviewScripts(false, std::chrono::seconds(10))
        || ShouldPollGamePreviewScripts(true, std::chrono::milliseconds(499))
        || !ShouldPollGamePreviewScripts(true, std::chrono::milliseconds(500))) {
        return EXIT_FAILURE;
    }

    if (!ShouldCompositeCreateModeGhost(true, true, true)
        || ShouldCompositeCreateModeGhost(false, true, true)
        || ShouldCompositeCreateModeGhost(true, false, true)) {
        return EXIT_FAILURE;
    }

    if (!ShouldRenderViewportPreview(moving)) {
        return EXIT_FAILURE;
    }

    ViewportRenderPolicy clean = moving;
    clean.previewDirty = false;
    if (ShouldRenderViewportPreview(clean)) {
        return EXIT_FAILURE;
    }

    if (NeedsEditorBackBufferResize(1520, 900, 1520, 900)) {
        return EXIT_FAILURE;
    }
    if (!NeedsEditorBackBufferResize(1520, 900, 1920, 1080)) {
        return EXIT_FAILURE;
    }
    if (!NeedsEditorBackBufferResize(0, 0, 1520, 900)) {
        return EXIT_FAILURE;
    }

    const ViewportCameraState cameraA{
        .targetX = 1.0f,
        .targetY = 2.0f,
        .targetZ = 3.0f,
        .distance = 24.0f,
        .yawDegrees = 90.0f,
        .pitchDegrees = -12.0f,
    };
    ViewportCameraState cameraB = cameraA;
    cameraB.yawDegrees += 0.00001f;
    if (HasCameraStateChanged(cameraA, cameraB)) {
        return EXIT_FAILURE;
    }
    cameraB.yawDegrees += 0.01f;
    if (!HasCameraStateChanged(cameraA, cameraB)) {
        return EXIT_FAILURE;
    }
    const float advancedYaw = AdvanceAutoOrbitYaw(90.0f, 0.5);
    if (advancedYaw <= 90.0f || advancedYaw >= 180.0f) {
        return EXIT_FAILURE;
    }

    struct TestRailPadState {
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float springStartX = 0.0f;
        float springStartY = 0.0f;
        bool springing = false;
    };

    TestRailPadState springState{
        .offsetX = 24.0f,
        .offsetY = -18.0f,
    };
    BeginRailPadSpring(springState);
    if (!springState.springing
        || springState.springStartX != 24.0f
        || springState.springStartY != -18.0f) {
        return EXIT_FAILURE;
    }
    AdvanceRailPadSpring(springState, 0.075);
    if (!springState.springing
        || springState.offsetX >= 24.0f
        || springState.offsetY <= -18.0f
        || springState.offsetX <= 0.0f
        || springState.offsetY >= 0.0f) {
        return EXIT_FAILURE;
    }
    AdvanceRailPadSpring(springState, 0.150);
    if (springState.springing || springState.offsetX != 0.0f || springState.offsetY != 0.0f) {
        return EXIT_FAILURE;
    }

    if (DefaultHierarchyPanelWidth() < 208) {
        return EXIT_FAILURE;
    }
    if (DefaultEditorStartupWidth() < 1640 || DefaultEditorStartupHeight() < 940) {
        return EXIT_FAILURE;
    }

#if defined(_WIN32)
    const RECT defaultToolStrip{10, 58, 1510, 98};
    const auto compactLayout = ri::editor::ComputeEditorToolbarLayout(defaultToolStrip);
    if (!compactLayout.compact || compactLayout.tacticalGroup.right >= compactLayout.foundryGroup.left) {
        return EXIT_FAILURE;
    }
    if (compactLayout.authoring.addCube.left < compactLayout.foundryGroup.left
        || compactLayout.authoring.addPlane.right > compactLayout.foundryGroup.right) {
        return EXIT_FAILURE;
    }
    const auto narrowStatus = ri::editor::ComputeToolStripStatusRect(defaultToolStrip, compactLayout);
    if (narrowStatus.has_value()) {
        return EXIT_FAILURE;
    }

    const RECT wideToolStrip{10, 58, 1910, 98};
    const auto wideLayout = ri::editor::ComputeEditorToolbarLayout(wideToolStrip);
    if (wideLayout.compact || wideLayout.tacticalGroup.right >= wideLayout.foundryGroup.left) {
        return EXIT_FAILURE;
    }
    const auto wideStatus = ri::editor::ComputeToolStripStatusRect(wideToolStrip, wideLayout);
    if (!wideStatus.has_value() || wideStatus->right >= wideLayout.foundryGroup.left) {
        return EXIT_FAILURE;
    }

    const RECT railRect{10, 446, 162, 760};
    const auto railLayout = ri::editor::ComputeCameraRailLayout(railRect);
    if (railLayout.trackballBounds.top <= railRect.top
        || railLayout.panCrossBounds.top <= railLayout.trackballBounds.bottom
        || railLayout.depthCrossBounds.top <= railLayout.panCrossBounds.bottom
        || railLayout.depthCrossBounds.bottom > railRect.bottom) {
        return EXIT_FAILURE;
    }
    if (railLayout.homeButtonBounds.left < railRect.left
        || railLayout.frameSelectionButtonBounds.left < railRect.left
        || railLayout.frameAllButtonBounds.right > railRect.right
        || railLayout.resolutionScaleButtonBounds.right > railRect.right) {
        return EXIT_FAILURE;
    }
    if (ri::editor::HitTestCameraRail(railRect,
                                      POINT{railLayout.trackballCenter.x, railLayout.trackballCenter.y})
            != ri::editor::CameraRailHit::Trackball) {
        return EXIT_FAILURE;
    }
    if (ri::editor::HitTestCameraRail(railRect,
                                      POINT{railLayout.panCenter.x, railLayout.panCenter.y})
            != ri::editor::CameraRailHit::PanCross) {
        return EXIT_FAILURE;
    }
    if (ri::editor::HitTestCameraRail(railRect,
                                      POINT{railLayout.depthCenter.x, railLayout.depthCenter.y})
            != ri::editor::CameraRailHit::DepthCross) {
        return EXIT_FAILURE;
    }
    if (ri::editor::HitTestCameraRail(
            railRect,
            POINT{(railLayout.homeButtonBounds.left + railLayout.homeButtonBounds.right) / 2,
                  (railLayout.homeButtonBounds.top + railLayout.homeButtonBounds.bottom) / 2})
            != ri::editor::CameraRailHit::HomeButton) {
        return EXIT_FAILURE;
    }
    if (ri::editor::HitTestCameraRail(
            railRect,
            POINT{(railLayout.frameSelectionButtonBounds.left + railLayout.frameSelectionButtonBounds.right) / 2,
                  (railLayout.frameSelectionButtonBounds.top + railLayout.frameSelectionButtonBounds.bottom) / 2})
            != ri::editor::CameraRailHit::FrameSelectionButton) {
        return EXIT_FAILURE;
    }
    if (ri::editor::HitTestCameraRail(
            railRect,
            POINT{(railLayout.frameAllButtonBounds.left + railLayout.frameAllButtonBounds.right) / 2,
                  (railLayout.frameAllButtonBounds.top + railLayout.frameAllButtonBounds.bottom) / 2})
            != ri::editor::CameraRailHit::FrameAllButton) {
        return EXIT_FAILURE;
    }
    if (ri::editor::HitTestCameraRail(
            railRect,
            POINT{(railLayout.resolutionScaleButtonBounds.left + railLayout.resolutionScaleButtonBounds.right) / 2,
                  (railLayout.resolutionScaleButtonBounds.top + railLayout.resolutionScaleButtonBounds.bottom) / 2})
            != ri::editor::CameraRailHit::ResolutionScaleButton) {
        return EXIT_FAILURE;
    }
#endif

    return EXIT_SUCCESS;
}
