#include "RawIron/Render/VulkanPreviewPresenter.h"

#include <algorithm>
#include <cmath>

namespace ri::render::vulkan {
namespace {

float FiniteOr(const float value, const float fallback) {
    return std::isfinite(value) ? value : fallback;
}

float FiniteClamp(const float value, const float fallback, const float minimum, const float maximum) {
    return std::clamp(FiniteOr(value, fallback), minimum, maximum);
}

ri::math::Vec3 FiniteColor(const ri::math::Vec3& value, const ri::math::Vec3& fallback) {
    return {
        FiniteClamp(value.x, fallback.x, 0.0f, 1.0f),
        FiniteClamp(value.y, fallback.y, 0.0f, 1.0f),
        FiniteClamp(value.z, fallback.z, 0.0f, 1.0f),
    };
}

} // namespace

VulkanNativeSceneResolvedTuning ResolveVulkanNativeSceneTuning(const VulkanNativeSceneFrame& frame) {
    const VulkanNativeSceneFrame defaults{};
    VulkanNativeSceneResolvedTuning tuning{};
    tuning.exposure = FiniteClamp(frame.renderExposure, defaults.renderExposure, 0.5f, 2.5f);
    tuning.contrast = FiniteClamp(frame.renderContrast, defaults.renderContrast, 0.7f, 1.6f);
    tuning.saturation = FiniteClamp(frame.renderSaturation, defaults.renderSaturation, 0.0f, 1.8f);
    tuning.fogStart = std::max(0.0f, FiniteOr(frame.renderFogStart, defaults.renderFogStart));
    tuning.fogEnd = std::max(
        tuning.fogStart + 0.001f,
        FiniteOr(frame.renderFogEnd, defaults.renderFogEnd));
    tuning.linearFog = FiniteOr(frame.renderFogEnd, defaults.renderFogEnd)
        > FiniteOr(frame.renderFogStart, defaults.renderFogStart) + 0.001f;
    tuning.fogAmount = tuning.linearFog
        ? FiniteClamp(frame.renderFogStrength, defaults.renderFogStrength, 0.0f, 1.0f)
        : FiniteClamp(frame.renderFogDensity, defaults.renderFogDensity, 0.0f, 0.05f);
    tuning.environmentTop = FiniteColor(frame.environmentClearTop, defaults.environmentClearTop);
    tuning.environmentBottom = FiniteColor(frame.environmentClearBottom, defaults.environmentClearBottom);
    tuning.fogColorNear = FiniteColor(frame.nativeFogColorNear, defaults.nativeFogColorNear);
    tuning.fogColorFar = FiniteColor(frame.nativeFogColorFar, defaults.nativeFogColorFar);
    tuning.ambientLight = FiniteColor(frame.nativeAmbientLight, defaults.nativeAmbientLight);
    return tuning;
}

} // namespace ri::render::vulkan
