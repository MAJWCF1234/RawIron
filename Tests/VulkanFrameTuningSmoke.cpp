#include "RawIron/Render/VulkanPreviewPresenter.h"
#include "RawIron/Render/VulkanShadowProjection.h"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <iostream>

int main() {
    // Camera movement below half a shadow texel must not slide a fixed world point
    // through the shadow texture. Crossing a cell boundary advances exactly one texel.
    for (const std::uint32_t resolution : {1024U,2048U,4096U}) {
        auto base = ri::math::IdentityMatrix();
        base.m[0][0] = base.m[1][1] = 1.f/90.f;
        base.m[0][3] = -.25f;
        base.m[1][3] = .5f;
        base.m[2][3] = .37f;
        const auto snapped = ri::render::vulkan::StabilizeOrthographicShadowMatrix(base,resolution);
        for (float fraction : {-.49f,-.25f,.25f,.49f,.51f,1.25f}) {
            auto moved = base;
            moved.m[0][3] += fraction*2.f/resolution;
            moved.m[1][3] -= fraction*2.f/resolution;
            moved = ri::render::vulkan::StabilizeOrthographicShadowMatrix(moved,resolution);
            const float dx = (moved.m[0][3]-snapped.m[0][3])*resolution*.5f;
            const float dy = (moved.m[1][3]-snapped.m[1][3])*resolution*.5f;
            if (std::abs(dx-std::round(fraction))>1e-4f || std::abs(dy+std::round(fraction))>1e-4f
                || moved.m[0][0]!=base.m[0][0] || moved.m[2][3]!=base.m[2][3]) {
                std::cerr << "Shadow grid crawls during camera movement\n"; return EXIT_FAILURE;
            }
        }
    }
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
