#pragma once

namespace ri::render::vulkan {

/// Device-feature decisions shared by native preview initialization and focused tests.
/// Optional Vulkan features are never requested unless the physical device reports them.
struct VulkanNativeDeviceFeaturePolicy {
    bool samplerAnisotropy = false;
    bool independentBlend = false;
    float maxSamplerAnisotropy = 1.0f;
};

[[nodiscard]] VulkanNativeDeviceFeaturePolicy ResolveVulkanNativeDeviceFeaturePolicy(
    bool samplerAnisotropySupported,
    bool independentBlendSupported,
    float deviceMaxSamplerAnisotropy) noexcept;

} // namespace ri::render::vulkan
