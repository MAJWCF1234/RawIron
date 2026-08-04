#include "RawIron/Render/VulkanDeviceFeaturePolicy.h"

#include <algorithm>
#include <cmath>

namespace ri::render::vulkan {

VulkanNativeDeviceFeaturePolicy ResolveVulkanNativeDeviceFeaturePolicy(
    const bool samplerAnisotropySupported,
    const bool independentBlendSupported,
    const float deviceMaxSamplerAnisotropy) noexcept {
    VulkanNativeDeviceFeaturePolicy result{};
    result.independentBlend = independentBlendSupported;
    result.samplerAnisotropy = samplerAnisotropySupported
        && std::isfinite(deviceMaxSamplerAnisotropy)
        && deviceMaxSamplerAnisotropy > 1.0f;
    result.maxSamplerAnisotropy = result.samplerAnisotropy
        ? std::clamp(deviceMaxSamplerAnisotropy, 1.0f, 16.0f)
        : 1.0f;
    return result;
}

} // namespace ri::render::vulkan
