#include "RawIron/Render/VulkanDeviceFeaturePolicy.h"

#include <cmath>
#include <iostream>
#include <limits>

int main() {
    using ri::render::vulkan::ResolveVulkanNativeDeviceFeaturePolicy;

    const auto unsupported = ResolveVulkanNativeDeviceFeaturePolicy(false, false, 16.0f);
    if (unsupported.samplerAnisotropy || unsupported.independentBlend
        || unsupported.maxSamplerAnisotropy != 1.0f) {
        std::cerr << "Unsupported optional Vulkan features were enabled.\n";
        return 1;
    }

    const auto supported = ResolveVulkanNativeDeviceFeaturePolicy(true, true, 32.0f);
    if (!supported.samplerAnisotropy || !supported.independentBlend
        || supported.maxSamplerAnisotropy != 16.0f) {
        std::cerr << "Supported optional Vulkan features were not selected or clamped.\n";
        return 1;
    }

    for (const float invalidLimit : {0.0f, 1.0f, std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::quiet_NaN()}) {
        const auto invalid = ResolveVulkanNativeDeviceFeaturePolicy(true, false, invalidLimit);
        if (invalid.samplerAnisotropy || invalid.maxSamplerAnisotropy != 1.0f) {
            std::cerr << "Invalid/ineffective anisotropy limit did not select the spec-valid fallback.\n";
            return 1;
        }
    }

    const auto independentOnly = ResolveVulkanNativeDeviceFeaturePolicy(false, true, 8.0f);
    if (independentOnly.samplerAnisotropy || !independentOnly.independentBlend) {
        std::cerr << "Independent Vulkan feature selection was coupled incorrectly.\n";
        return 1;
    }

    return 0;
}
