#include "RawIron/Render/VulkanBootstrap.h"

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#endif

#include <vulkan/vulkan.h>

#include <array>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace ri::render::vulkan {

namespace {

std::string FormatVulkanVersion(std::uint32_t version) {
    const std::uint32_t major = VK_API_VERSION_MAJOR(version);
    const std::uint32_t minor = VK_API_VERSION_MINOR(version);
    const std::uint32_t patch = VK_API_VERSION_PATCH(version);
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

std::string GetPlatformName() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

std::string GetDeviceTypeName(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return "discrete";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return "integrated";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return "virtual";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return "cpu";
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
    default:
        return "other";
    }
}

std::string GetLoaderPath() {
#if defined(_WIN32)
    HMODULE module = GetModuleHandleA("vulkan-1.dll");
    if (module == nullptr) {
        return {};
    }

    std::array<char, MAX_PATH> path{};
    const DWORD length = GetModuleFileNameA(module, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0U) {
        return {};
    }
    return std::string(path.data(), path.data() + length);
#elif defined(__linux__)
    void* module = dlopen("libvulkan.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (module == nullptr) {
        return {};
    }

    void* symbol = dlsym(module, "vkCreateInstance");
    Dl_info info{};
    const bool resolved = symbol != nullptr && dladdr(symbol, &info) != 0 && info.dli_fname != nullptr;
    const std::string path = resolved ? std::string(info.dli_fname) : std::string{};
    dlclose(module);
    return path;
#else
    return {};
#endif
}

std::vector<const char*> GetRequiredInstanceExtensions(bool createSurface) {
#if defined(_WIN32)
    if (!createSurface) {
        return {};
    }
    return {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    };
#else
    return {};
#endif
}

void ExpectVk(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(operation) + " failed with VkResult=" + std::to_string(static_cast<int>(result)));
    }
}

std::vector<std::string> EnumerateInstanceExtensions() {
    std::uint32_t count = 0U;
    ExpectVk(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr),
             "vkEnumerateInstanceExtensionProperties(count)");
    std::vector<VkExtensionProperties> properties(count);
    if (count > 0U) {
        ExpectVk(vkEnumerateInstanceExtensionProperties(nullptr, &count, properties.data()),
                 "vkEnumerateInstanceExtensionProperties(list)");
    }

    std::vector<std::string> names;
    names.reserve(count);
    for (const VkExtensionProperties& property : properties) {
        names.push_back(property.extensionName);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> EnumerateInstanceLayers() {
    std::uint32_t count = 0U;
    ExpectVk(vkEnumerateInstanceLayerProperties(&count, nullptr), "vkEnumerateInstanceLayerProperties(count)");
    std::vector<VkLayerProperties> properties(count);
    if (count > 0U) {
        ExpectVk(vkEnumerateInstanceLayerProperties(&count, properties.data()), "vkEnumerateInstanceLayerProperties(list)");
    }

    std::vector<std::string> names;
    names.reserve(count);
    for (const VkLayerProperties& property : properties) {
        names.push_back(property.layerName);
    }
    std::sort(names.begin(), names.end());
    return names;
}

int ScorePhysicalDevice(const VkPhysicalDeviceProperties& properties, bool presentSupport) {
    int score = 0;
    if (presentSupport) {
        score += 1000;
    }
    switch (properties.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        score += 400;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        score += 250;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        score += 150;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        score += 50;
        break;
    default:
        score += 25;
        break;
    }
    score += static_cast<int>(VK_API_VERSION_MAJOR(properties.apiVersion) * 10U);
    score += static_cast<int>(VK_API_VERSION_MINOR(properties.apiVersion));
    return score;
}

#if defined(_WIN32)
class HiddenWindow {
public:
    explicit HiddenWindow(const char* title) {
        instance_ = GetModuleHandleA(nullptr);
        windowClass_.lpfnWndProc = DefWindowProcA;
        windowClass_.hInstance = instance_;
        windowClass_.lpszClassName = "RawIronVulkanBootstrapWindow";
        atom_ = RegisterClassA(&windowClass_);
        if (atom_ == 0) {
            throw std::runtime_error("RegisterClassA failed for Vulkan bootstrap window.");
        }

        hwnd_ = CreateWindowExA(
            0,
            windowClass_.lpszClassName,
            title,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1280,
            720,
            nullptr,
            nullptr,
            instance_,
            nullptr);
        if (hwnd_ == nullptr) {
            UnregisterClassA(windowClass_.lpszClassName, instance_);
            throw std::runtime_error("CreateWindowExA failed for Vulkan bootstrap window.");
        }
    }

    HiddenWindow(const HiddenWindow&) = delete;
    HiddenWindow& operator=(const HiddenWindow&) = delete;

    ~HiddenWindow() {
        if (hwnd_ != nullptr) {
            DestroyWindow(hwnd_);
        }
        if (atom_ != 0) {
            UnregisterClassA(windowClass_.lpszClassName, instance_);
        }
    }

    [[nodiscard]] HINSTANCE GetInstance() const noexcept {
        return instance_;
    }

    [[nodiscard]] HWND GetHandle() const noexcept {
        return hwnd_;
    }

private:
    HINSTANCE instance_ = nullptr;
    WNDCLASSA windowClass_{};
    ATOM atom_ = 0;
    HWND hwnd_ = nullptr;
};
#endif

} // namespace

VulkanBootstrapSummary RunBootstrap(const VulkanBootstrapOptions& options) {
    VulkanBootstrapSummary summary{};
    summary.platformName = GetPlatformName();
    summary.loaderPath = GetLoaderPath();
    summary.surfaceStatus = options.createSurface ? "not-integrated" : "skipped";

    std::uint32_t instanceVersion = VK_API_VERSION_1_0;
    ExpectVk(vkEnumerateInstanceVersion(&instanceVersion), "vkEnumerateInstanceVersion");
    summary.instanceApiVersion = FormatVulkanVersion(instanceVersion);
    summary.instanceExtensions = EnumerateInstanceExtensions();
    summary.instanceLayers = EnumerateInstanceLayers();
    summary.validationLayerAvailable =
        std::find(summary.instanceLayers.begin(), summary.instanceLayers.end(), "VK_LAYER_KHRONOS_validation") !=
        summary.instanceLayers.end();

    const std::vector<const char*> extensions = GetRequiredInstanceExtensions(options.createSurface);

    const VkApplicationInfo applicationInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = options.windowTitle,
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "RawIron",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = instanceVersion,
    };

    const VkInstanceCreateInfo instanceInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &applicationInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data(),
    };

    VkInstance instance = VK_NULL_HANDLE;
    ExpectVk(vkCreateInstance(&instanceInfo, nullptr, &instance), "vkCreateInstance");

    VkSurfaceKHR surface = VK_NULL_HANDLE;

    try {
#if defined(_WIN32)
        std::unique_ptr<HiddenWindow> window;
        if (options.createSurface) {
            window = std::make_unique<HiddenWindow>(options.windowTitle);

            const VkWin32SurfaceCreateInfoKHR surfaceInfo{
                .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                .pNext = nullptr,
                .flags = 0,
                .hinstance = window->GetInstance(),
                .hwnd = window->GetHandle(),
            };

            ExpectVk(vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface), "vkCreateWin32SurfaceKHR");
            summary.surfaceCreated = true;
            summary.surfaceStatus = "ready";
        }
#elif defined(__linux__)
        summary.surfaceStatus = options.createSurface ? "platform-layer-pending" : "skipped";
#else
        summary.surfaceStatus = options.createSurface ? "unsupported-platform" : "skipped";
#endif

        std::uint32_t deviceCount = 0U;
        ExpectVk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices(count)");
        if (deviceCount == 0U) {
            throw std::runtime_error("vkEnumeratePhysicalDevices returned zero GPUs.");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        ExpectVk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), "vkEnumeratePhysicalDevices(list)");
        int bestDeviceScore = (std::numeric_limits<int>::min)();
        for (VkPhysicalDevice device : devices) {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(device, &properties);

            bool presentSupport = false;
            std::uint32_t graphicsQueueFamilyCount = 0U;
            std::uint32_t presentQueueFamilyCount = 0U;

            std::uint32_t queueFamilyCount = 0U;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            for (std::uint32_t familyIndex = 0; familyIndex < queueFamilyCount; ++familyIndex) {
                if ((queueFamilies[familyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
                    graphicsQueueFamilyCount += 1U;
                }
            }

            if (surface != VK_NULL_HANDLE) {
                for (std::uint32_t familyIndex = 0; familyIndex < queueFamilyCount; ++familyIndex) {
                    VkBool32 supported = VK_FALSE;
                    ExpectVk(
                        vkGetPhysicalDeviceSurfaceSupportKHR(device, familyIndex, surface, &supported),
                        "vkGetPhysicalDeviceSurfaceSupportKHR");
                    if (supported == VK_TRUE) {
                        presentSupport = true;
                        presentQueueFamilyCount += 1U;
                    }
                }
            }

            summary.devices.push_back(VulkanDeviceSummary{
                .name = properties.deviceName,
                .type = GetDeviceTypeName(properties.deviceType),
                .apiVersion = FormatVulkanVersion(properties.apiVersion),
                .vendorId = properties.vendorID,
                .deviceId = properties.deviceID,
                .graphicsQueueFamilyCount = graphicsQueueFamilyCount,
                .presentQueueFamilyCount = presentQueueFamilyCount,
                .presentSupport = presentSupport,
            });

            const int score = ScorePhysicalDevice(properties, presentSupport);
            if (score > bestDeviceScore) {
                bestDeviceScore = score;
                summary.selectedDeviceName = properties.deviceName;
            }
        }

        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
        }
    } catch (...) {
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
        }
        vkDestroyInstance(instance, nullptr);
        throw;
    }

    vkDestroyInstance(instance, nullptr);
    return summary;
}

VulkanBootstrapSummary RunBootstrap(const char* windowTitle) {
    return RunBootstrap(VulkanBootstrapOptions{
        .windowTitle = windowTitle,
        .createSurface = true,
    });
}

} // namespace ri::render::vulkan
