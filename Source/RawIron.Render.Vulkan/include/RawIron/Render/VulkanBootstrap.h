#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ri::render::vulkan {

struct VulkanDeviceSummary {
    std::string name;
    std::string type;
    std::string apiVersion;
    std::uint32_t vendorId = 0;
    std::uint32_t deviceId = 0;
    std::uint32_t graphicsQueueFamilyCount = 0;
    std::uint32_t presentQueueFamilyCount = 0;
    bool presentSupport = false;
};

struct VulkanBootstrapSummary {
    std::string platformName;
    std::string loaderPath;
    std::string instanceApiVersion;
    std::vector<std::string> instanceExtensions;
    std::vector<std::string> instanceLayers;
    bool validationLayerAvailable = false;
    bool surfaceCreated = false;
    std::string surfaceStatus;
    std::string selectedDeviceName;
    std::vector<VulkanDeviceSummary> devices;
};

struct VulkanBootstrapOptions {
    const char* windowTitle = "RawIron Vulkan Bootstrap";
    bool createSurface = true;
};

VulkanBootstrapSummary RunBootstrap(const VulkanBootstrapOptions& options);
VulkanBootstrapSummary RunBootstrap(const char* windowTitle);

} // namespace ri::render::vulkan
