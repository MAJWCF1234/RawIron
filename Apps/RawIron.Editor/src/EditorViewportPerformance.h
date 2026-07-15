#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace ri::editor {

struct ViewportRenderPolicy {
    int plotWidth = 0;
    int plotHeight = 0;
    bool cameraMoving = false;
    bool resolutionScalingEnabled = true;
    bool previewDirty = false;
};

struct ViewportRenderSize {
    int width = 0;
    int height = 0;
};

struct ViewportCameraState {
    float targetX = 0.0f;
    float targetY = 0.0f;
    float targetZ = 0.0f;
    float distance = 0.0f;
    float yawDegrees = 0.0f;
    float pitchDegrees = 0.0f;
};

[[nodiscard]] inline int DefaultHierarchyPanelWidth() {
    return 224;
}

[[nodiscard]] inline int DefaultEditorStartupWidth() {
    return 1680;
}

[[nodiscard]] inline int DefaultEditorStartupHeight() {
    return 960;
}

template <typename PadState>
inline void BeginRailPadSpring(PadState& state) {
    state.springStartX = state.offsetX;
    state.springStartY = state.offsetY;
    state.springing = true;
}

template <typename PadState>
inline void AdvanceRailPadSpring(
    PadState& state,
    const double elapsedSeconds,
    const double springSeconds = 0.150) {
    if (!state.springing) {
        return;
    }
    const double t = std::clamp(elapsedSeconds / springSeconds, 0.0, 1.0);
    const float lerp = static_cast<float>(1.0 - t);
    state.offsetX = state.springStartX * lerp;
    state.offsetY = state.springStartY * lerp;
    if (t >= 1.0) {
        state.offsetX = 0.0f;
        state.offsetY = 0.0f;
        state.springing = false;
    }
}

[[nodiscard]] inline ViewportRenderSize ComputeViewportRenderSize(const ViewportRenderPolicy& policy) {
    constexpr int kFullResolutionMaxDimension = 960;

    const int plotWidth = std::max(1, policy.plotWidth);
    const int plotHeight = std::max(1, policy.plotHeight);
    const float motionScale = policy.cameraMoving && policy.resolutionScalingEnabled ? 0.45f : 1.0f;
    const int maxDimension = static_cast<int>(static_cast<float>(kFullResolutionMaxDimension) * motionScale);
    float scale = 1.0f;
    if (plotWidth > maxDimension || plotHeight > maxDimension) {
        scale = std::min(static_cast<float>(maxDimension) / static_cast<float>(plotWidth),
                         static_cast<float>(maxDimension) / static_cast<float>(plotHeight));
    }
    return {
        .width = std::max(64, static_cast<int>(static_cast<float>(plotWidth) * scale)),
        .height = std::max(64, static_cast<int>(static_cast<float>(plotHeight) * scale)),
    };
}

[[nodiscard]] inline bool IsViewportInteractiveMotion(const bool cameraChanged,
                                                      const bool cameraDragging,
                                                      const bool autoOrbitPreview,
                                                      const bool railAnimating) {
    return cameraChanged || cameraDragging || autoOrbitPreview || railAnimating;
}

[[nodiscard]] inline bool ShouldRunLogicLivePreview(const bool creatorLayerVisible,
                                                    const bool gameplayInspectorActive,
                                                    const bool logicCatalogActive) {
    return creatorLayerVisible || gameplayInspectorActive || logicCatalogActive;
}

[[nodiscard]] inline bool ShouldTickLogicPreview(const std::uint32_t frameCounter,
                                                 const bool forceTick) {
    return forceTick || (frameCounter % 2U) == 0U;
}

[[nodiscard]] inline bool ShouldPollGamePreviewScripts(
    const bool hasMountedGame,
    const std::chrono::steady_clock::duration elapsedSinceLastPoll,
    const std::chrono::milliseconds pollInterval = std::chrono::milliseconds(500)) {
    return hasMountedGame && elapsedSinceLastPoll >= pollInterval;
}

[[nodiscard]] inline unsigned int ComputeViewportTimerIntervalMs(
    const bool interactiveMotion,
    const double lastRenderMs) {
    if (interactiveMotion) {
        if (lastRenderMs >= 30.0) {
            return 16U;
        }
        if (lastRenderMs >= 16.0) {
            return 12U;
        }
        return 8U;
    }
    if (lastRenderMs > 60.0) {
        return 100U;
    }
    if (lastRenderMs > 45.0) {
        return 66U;
    }
    if (lastRenderMs > 22.0) {
        return 50U;
    }
    return 50U;
}

[[nodiscard]] inline bool HasCameraStateChanged(
    const ViewportCameraState& previous,
    const ViewportCameraState& current,
    const float epsilon = 0.001f) {
    const auto changed = [epsilon](const float lhs, const float rhs) {
        return std::abs(lhs - rhs) > epsilon;
    };
    return changed(previous.targetX, current.targetX)
        || changed(previous.targetY, current.targetY)
        || changed(previous.targetZ, current.targetZ)
        || changed(previous.distance, current.distance)
        || changed(previous.yawDegrees, current.yawDegrees)
        || changed(previous.pitchDegrees, current.pitchDegrees);
}

[[nodiscard]] inline float AdvanceAutoOrbitYaw(
    const float currentYawDegrees,
    const double deltaSeconds,
    const float degreesPerSecond = 24.0f) {
    const float advanced =
        currentYawDegrees + static_cast<float>(std::max(0.0, deltaSeconds)) * degreesPerSecond;
    return std::fmod(advanced, 360.0f);
}

[[nodiscard]] inline bool ShouldRenderViewportPreview(const ViewportRenderPolicy& policy) {
    return policy.previewDirty;
}

[[nodiscard]] inline bool ShouldRunEditorPreviewAnimation(const bool windowMinimized) {
    return !windowMinimized;
}

[[nodiscard]] inline bool ShouldCompositeCreateModeGhost(
    const bool createToolActive,
    const bool hasPlacementPoint,
    const bool hasDepthBuffer) {
    return createToolActive && hasPlacementPoint && hasDepthBuffer;
}

[[nodiscard]] inline bool NeedsEditorBackBufferResize(
    const int currentWidth,
    const int currentHeight,
    const int requestedWidth,
    const int requestedHeight) {
    return currentWidth != requestedWidth || currentHeight != requestedHeight
        || currentWidth <= 0 || currentHeight <= 0;
}

} // namespace ri::editor
