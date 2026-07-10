#include "RawIron/Render/VulkanPreviewPresenter.h"

#include <cmath>
#include <cstdlib>
#include <limits>

int main() {
    ri::render::vulkan::VulkanNativeSceneFrame frame{};
    frame.renderExposure = std::numeric_limits<float>::quiet_NaN();
    frame.renderContrast = std::numeric_limits<float>::infinity();
    frame.renderSaturation = -std::numeric_limits<float>::infinity();
    frame.renderFogStart = std::numeric_limits<float>::quiet_NaN();
    frame.renderFogEnd = std::numeric_limits<float>::quiet_NaN();
    frame.renderFogStrength = std::numeric_limits<float>::quiet_NaN();
    frame.environmentClearTop.x = std::numeric_limits<float>::quiet_NaN();
    frame.nativeAmbientLight.z = std::numeric_limits<float>::infinity();

    const ri::render::vulkan::VulkanNativeSceneResolvedTuning tuning =
        ri::render::vulkan::ResolveVulkanNativeSceneTuning(frame);
    const float scalars[] = {
        tuning.exposure,
        tuning.contrast,
        tuning.saturation,
        tuning.fogAmount,
        tuning.fogStart,
        tuning.fogEnd,
        tuning.environmentTop.x,
        tuning.ambientLight.z,
    };
    for (const float value : scalars) {
        if (!std::isfinite(value)) {
            return EXIT_FAILURE;
        }
    }
    if (tuning.exposure != 1.0f || tuning.contrast != 1.0f || tuning.saturation != 1.0f
        || tuning.fogEnd <= tuning.fogStart) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
