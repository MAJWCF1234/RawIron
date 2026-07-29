#include "RawIron/Render/VulkanPreviewPresenter.h"

#include <cstdlib>

int main() {
    ri::render::vulkan::VulkanNativeSceneResourceStats captured{};
    bool called = false;
    ri::render::vulkan::VulkanPreviewWindowOptions options{};
    options.onResourceStats = [&](const ri::render::vulkan::VulkanNativeSceneResourceStats& stats) {
        captured = stats;
        called = true;
    };
    const ri::render::vulkan::VulkanNativeSceneResourceStats sample{
        .descriptorPoolCount = 2U,
        .allocatedDescriptorSetCount = 2049U,
        .cachedDescriptorCount = 2048U,
        .uploadedTextureCount = 512U,
        .missingTextureFallbackCount = 3U,
        .descriptorAllocationFailureCount = 1U,
    };
    options.onResourceStats(sample);
    if (!called || captured != sample
        || captured.missingTextureFallbackCount != 3U
        || captured.descriptorAllocationFailureCount != 1U) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
