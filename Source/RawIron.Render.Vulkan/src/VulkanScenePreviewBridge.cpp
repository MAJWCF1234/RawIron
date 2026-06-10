#include "RawIron/Render/VulkanScenePreviewBridge.h"

#include <algorithm>

namespace ri::render::vulkan {

void ApplyScenePreviewAtmosphereToVulkanFrame(const ri::render::software::ScenePreviewOptions& preview,
                                              VulkanNativeSceneFrame& frame) {
    frame.renderFogStart = preview.fogStartDepth;
    frame.renderFogEnd = preview.fogEndDepth;
    frame.renderFogStrength = preview.fogStrength;
    frame.useEnvironmentClear = true;
    frame.environmentClearTop = preview.clearTop;
    frame.environmentClearBottom = preview.clearBottom;
    frame.nativeFogColorNear = preview.fogColor;
    const bool hasAuthoredFogFar =
        (preview.fogColorFar.x + preview.fogColorFar.y + preview.fogColorFar.z) > 1.0e-4f;
    frame.nativeFogColorFar = hasAuthoredFogFar ? preview.fogColorFar : preview.fogColor;
    frame.nativeAmbientLight = preview.ambientLight;
}

void OverlayScenePreviewPostProcessOnParameters(const ri::render::software::ScenePreviewOptions& preview,
                                                ri::render::PostProcessParameters& postProcess) {
    postProcess.bloomIntensity =
        std::max(postProcess.bloomIntensity, preview.previewBloomStrength);
    postProcess.vignetteStrength =
        std::max(postProcess.vignetteStrength, preview.previewVignetteStrength);
    postProcess.casSharpenAmount =
        std::max(postProcess.casSharpenAmount, preview.previewSharpenAmount);
    if (preview.previewTintStrength > 1.0e-4f) {
        postProcess.tintStrength = std::max(postProcess.tintStrength, preview.previewTintStrength);
        postProcess.tintColor = preview.previewTintColor;
    }
}

} // namespace ri::render::vulkan
