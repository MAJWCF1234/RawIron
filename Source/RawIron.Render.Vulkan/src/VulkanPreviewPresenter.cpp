#include "RawIron/Render/VulkanPreviewPresenter.h"

#if defined(_WIN32)
#include "RawIron/Core/RenderRecorder.h"
#include "RawIron/Core/RenderSubmissionPlan.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Render/VulkanCommandBufferRecorder.h"
#include "RawIron/Render/VulkanIntentStaging.h"
#include "RawIron/Scene/SceneRenderSubmission.h"
#include "RawIron/Scene/SceneKit.h"
#define VK_USE_PLATFORM_WIN32_KHR 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ri::render::vulkan {

namespace {

void ExpectVk(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(operation) + " failed with VkResult=" + std::to_string(static_cast<int>(result)));
    }
}

struct ScopedWindowClass {
    HINSTANCE instance = nullptr;
    const wchar_t* className = L"RawIronVulkanPreviewWindow";
    ATOM atom = 0;

    explicit ScopedWindowClass(WNDPROC windowProc) {
        instance = GetModuleHandleW(nullptr);
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = windowProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = className;
        windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        atom = RegisterClassW(&windowClass);
        if (atom == 0) {
            throw std::runtime_error("RegisterClassW failed for Vulkan preview window.");
        }
    }

    ~ScopedWindowClass() {
        if (atom != 0) {
            UnregisterClassW(className, instance);
        }
    }
};

struct WindowState {
    HWND hwnd = nullptr;
    bool running = true;
    void* messageUserData = nullptr;
    VulkanPreviewWindowOptions::Win32MessageHook onWin32Message = nullptr;
};

LRESULT CALLBACK PreviewWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    WindowState* state = reinterpret_cast<WindowState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<LPCREATESTRUCTW>(lParam);
        state = static_cast<WindowState*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        if (state != nullptr) {
            state->hwnd = hwnd;
        }
    }

    if (message != WM_NCCREATE && state != nullptr && state->onWin32Message != nullptr) {
        state->onWin32Message(state->messageUserData,
                              hwnd,
                              static_cast<unsigned int>(message),
                              static_cast<std::uint64_t>(wParam),
                              static_cast<std::int64_t>(lParam));
    }

    switch (message) {
        case WM_CLOSE:
            if (state != nullptr) {
                state->running = false;
            }
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (state != nullptr) {
                state->running = false;
            }
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

std::wstring Widen(std::string_view text) {
    return std::wstring(text.begin(), text.end());
}

VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const VkSurfaceFormatKHR& format : formats) {
        if ((format.format == VK_FORMAT_B8G8R8A8_SRGB || format.format == VK_FORMAT_B8G8R8A8_UNORM) &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    for (const VkSurfaceFormatKHR& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB || format.format == VK_FORMAT_B8G8R8A8_UNORM ||
            format.format == VK_FORMAT_R8G8B8A8_SRGB || format.format == VK_FORMAT_R8G8B8A8_UNORM) {
            return format;
        }
    }
    return formats.front();
}

VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) {
    for (const VkPresentModeKHR mode : presentModes) {
        if (mode == VK_PRESENT_MODE_FIFO_KHR) {
            // Prefer vsync-safe pacing for preview stability and thermals.
            return mode;
        }
    }
    for (const VkPresentModeKHR mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

struct DeviceSelection {
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    std::uint32_t graphicsQueueFamily = 0;
    std::uint32_t presentQueueFamily = 0;
};

DeviceSelection PickDevice(VkInstance instance, VkSurfaceKHR surface) {
    std::uint32_t deviceCount = 0;
    ExpectVk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices(count)");
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan devices were found.");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    ExpectVk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), "vkEnumeratePhysicalDevices(list)");

    int bestScore = std::numeric_limits<int>::min();
    DeviceSelection best{};
    for (VkPhysicalDevice device : devices) {
        std::uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        std::optional<std::uint32_t> graphicsFamily;
        std::optional<std::uint32_t> presentFamily;
        for (std::uint32_t familyIndex = 0; familyIndex < queueFamilyCount; ++familyIndex) {
            if ((queueFamilies[familyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U && !graphicsFamily.has_value()) {
                graphicsFamily = familyIndex;
            }
            VkBool32 presentSupport = VK_FALSE;
            ExpectVk(vkGetPhysicalDeviceSurfaceSupportKHR(device, familyIndex, surface, &presentSupport),
                     "vkGetPhysicalDeviceSurfaceSupportKHR");
            if (presentSupport == VK_TRUE && !presentFamily.has_value()) {
                presentFamily = familyIndex;
            }
        }

        if (!graphicsFamily.has_value() || !presentFamily.has_value()) {
            continue;
        }

        std::uint32_t extensionCount = 0;
        ExpectVk(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr),
                 "vkEnumerateDeviceExtensionProperties(count)");
        std::vector<VkExtensionProperties> extensions(extensionCount);
        if (extensionCount > 0) {
            ExpectVk(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data()),
                     "vkEnumerateDeviceExtensionProperties(list)");
        }

        bool hasSwapchain = false;
        for (const VkExtensionProperties& extension : extensions) {
            if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchain = true;
                break;
            }
        }
        if (!hasSwapchain) {
            continue;
        }

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);
        int score = 0;
        switch (properties.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: score += 400; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score += 250; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: score += 125; break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU: score += 50; break;
            default: score += 25; break;
        }
        score += static_cast<int>(properties.limits.maxImageDimension2D);
        if (score > bestScore) {
            bestScore = score;
            best = DeviceSelection{
                .physicalDevice = device,
                .graphicsQueueFamily = *graphicsFamily,
                .presentQueueFamily = *presentFamily,
            };
        }
    }

    if (best.physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("No Vulkan device with graphics, present, and swapchain support was found.");
    }
    return best;
}

void ConvertPreviewImageInto(PreviewImageData& converted,
                             const PreviewImageData& image,
                             VkFormat targetFormat) {
    converted.width = image.width;
    converted.height = image.height;
    converted.format = PreviewPixelFormat::Rgba8;
    converted.pixels.resize(static_cast<std::size_t>(image.width * image.height * 4), 255);

    const bool targetIsBgra =
        targetFormat == VK_FORMAT_B8G8R8A8_UNORM || targetFormat == VK_FORMAT_B8G8R8A8_SRGB;
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const std::size_t srcOffset = static_cast<std::size_t>((y * image.width + x) * 3);
            const std::size_t dstOffset = static_cast<std::size_t>((y * image.width + x) * 4);

            std::uint8_t r = 0;
            std::uint8_t g = 0;
            std::uint8_t b = 0;
            if (image.format == PreviewPixelFormat::Bgr8) {
                b = image.pixels[srcOffset + 0];
                g = image.pixels[srcOffset + 1];
                r = image.pixels[srcOffset + 2];
            } else {
                r = image.pixels[srcOffset + 0];
                g = image.pixels[srcOffset + 1];
                b = image.pixels[srcOffset + 2];
            }

            if (targetIsBgra) {
                converted.pixels[dstOffset + 0] = b;
                converted.pixels[dstOffset + 1] = g;
                converted.pixels[dstOffset + 2] = r;
                converted.pixels[dstOffset + 3] = 255;
            } else {
                converted.pixels[dstOffset + 0] = r;
                converted.pixels[dstOffset + 1] = g;
                converted.pixels[dstOffset + 2] = b;
                converted.pixels[dstOffset + 3] = 255;
            }
        }
    }
}

PreviewImageData ConvertPreviewImage(const PreviewImageData& image, VkFormat targetFormat) {
    PreviewImageData converted{};
    ConvertPreviewImageInto(converted, image, targetFormat);
    return converted;
}

PreviewImageData BuildVulkanPreviewImage(const ri::render::software::SoftwareImage& image) {
    PreviewImageData preview{};
    preview.width = image.width;
    preview.height = image.height;
    preview.format = PreviewPixelFormat::Bgr8;
    preview.pixels = image.pixels;
    return preview;
}

std::uint32_t FindMemoryType(VkPhysicalDevice physicalDevice,
                             std::uint32_t typeBits,
                             VkMemoryPropertyFlags requiredFlags) {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    for (std::uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index) {
        const bool supported = (typeBits & (1U << index)) != 0U;
        const bool hasFlags = (memoryProperties.memoryTypes[index].propertyFlags & requiredFlags) == requiredFlags;
        if (supported && hasFlags) {
            return index;
        }
    }
    throw std::runtime_error("No suitable Vulkan memory type was found.");
}

} // namespace

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

bool BuildSceneKitPreviewVulkanBridge(const ri::scene::SceneKitPreview& preview,
                                      const int width,
                                      const int height,
                                      const ri::scene::PhotoModeCameraOverrides* photoMode,
                                      SceneKitPreviewRenderBridgeStats* outStats,
                                      std::string* error) {
    try {
        ri::scene::SceneRenderSubmissionOptions submissionOptions{};
        submissionOptions.viewportWidth = std::max(width, 1);
        submissionOptions.viewportHeight = std::max(height, 1);
        submissionOptions.enableFarHorizon = true;
        submissionOptions.farHorizonStartDistance = 80.0f;
        submissionOptions.farHorizonEndDistance = 220.0f;
        submissionOptions.farHorizonMaxDistance = 520.0f;
        submissionOptions.farHorizonMaxNodeStride = 2U;
        submissionOptions.farHorizonMaxInstanceStride = 3U;
        submissionOptions.farHorizonCullTransparent = true;
        submissionOptions.enableCoarseOcclusion = true;
        submissionOptions.coarseOcclusionGridWidth = 64;
        submissionOptions.coarseOcclusionGridHeight = 36;
        submissionOptions.coarseOcclusionDepthBias = 0.8f;
        if (photoMode != nullptr && ri::scene::PhotoModeFieldOfViewActive(*photoMode)) {
            submissionOptions.photoMode = *photoMode;
        }

        const ri::scene::SceneRenderSubmission submission = ri::scene::BuildSceneRenderSubmission(
            preview.scene,
            preview.orbitCamera.cameraNode,
            submissionOptions);
        if (submission.stats.cameraNodeHandle == ri::scene::kInvalidHandle) {
            throw std::runtime_error("Scene preview does not expose a valid camera for Vulkan submission.");
        }

        const ri::core::RenderSubmissionPlan plan = ri::core::BuildRenderSubmissionPlan(submission.commands);

        VulkanCommandListSink commandListSink{};
        ri::core::RenderRecorderStats recorderStats{};
        if (!ri::core::ExecuteRenderSubmissionPlan(submission.commands, plan, commandListSink, &recorderStats)) {
            throw std::runtime_error("Failed to execute render submission plan for Scene Kit preview.");
        }

        VulkanCommandBufferRecorder commandBufferRecorder{};
        if (!RecordVulkanCommandList(commandListSink.Operations(), commandBufferRecorder)) {
            throw std::runtime_error("Failed to translate Vulkan command list into backend intents.");
        }

        const VulkanIntentStagingPlan stagingPlan = BuildVulkanIntentStagingPlan(commandBufferRecorder.Intents());
        if (stagingPlan.status != VulkanIntentStagingStatus::Ok) {
            throw std::runtime_error("Vulkan intent staging rejected the Scene Kit preview command stream.");
        }

        if (outStats != nullptr) {
            *outStats = SceneKitPreviewRenderBridgeStats{
                .renderCommandCount = submission.commands.CommandCount(),
                .submissionBatchCount = plan.batches.size(),
                .drawCommandCount = submission.stats.drawCommandCount,
                .skippedNodeCount = submission.stats.skippedNodes,
                .vulkanOpCount = commandListSink.Operations().size(),
                .intentCount = commandBufferRecorder.Intents().size(),
                .stagedRangeCount = stagingPlan.ranges.size(),
            };
        }
        return true;
    } catch (const std::exception& exception) {
        if (error != nullptr) {
            *error = exception.what();
        }
        return false;
    }
}

bool PresentPreviewImageWindow(const PreviewImageData& inputImage,
                               const VulkanPreviewWindowOptions& options,
                               std::string* error) {
    try {
        if (inputImage.width <= 0 || inputImage.height <= 0 || inputImage.pixels.empty()) {
            throw std::runtime_error("Preview image is empty.");
        }

        ScopedWindowClass windowClass(&PreviewWindowProc);
        WindowState windowState{};
        windowState.messageUserData = options.messageUserData;
        windowState.onWin32Message = options.onWin32Message;

        RECT rect{0, 0, inputImage.width, inputImage.height};
        AdjustWindowRect(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
        HWND hwnd = CreateWindowExW(
            0,
            windowClass.className,
            Widen(options.windowTitle).c_str(),
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,
            nullptr,
            windowClass.instance,
            &windowState);
        if (hwnd == nullptr) {
            throw std::runtime_error("CreateWindowExW failed for Vulkan preview window.");
        }
        if (options.outClientHwnd != nullptr) {
            *static_cast<HWND*>(options.outClientHwnd) = hwnd;
        }

        const std::array<const char*, 2> extensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        };
        const VkApplicationInfo applicationInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "RawIron Vulkan Preview",
            .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
            .pEngineName = "RawIron",
            .engineVersion = VK_MAKE_VERSION(0, 1, 0),
            .apiVersion = VK_API_VERSION_1_0,
        };
        const VkInstanceCreateInfo instanceInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &applicationInfo,
            .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
        };

        VkInstance instance = VK_NULL_HANDLE;
        ExpectVk(vkCreateInstance(&instanceInfo, nullptr, &instance), "vkCreateInstance");

        const VkWin32SurfaceCreateInfoKHR surfaceInfo{
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hinstance = windowClass.instance,
            .hwnd = hwnd,
        };
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        ExpectVk(vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface), "vkCreateWin32SurfaceKHR");

        const DeviceSelection selection = PickDevice(instance, surface);

        const float queuePriority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        std::vector<std::uint32_t> families = {selection.graphicsQueueFamily, selection.presentQueueFamily};
        std::sort(families.begin(), families.end());
        families.erase(std::unique(families.begin(), families.end()), families.end());
        for (const std::uint32_t family : families) {
            queueInfos.push_back(VkDeviceQueueCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = family,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority,
            });
        }

        const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        const VkDeviceCreateInfo deviceInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = static_cast<std::uint32_t>(queueInfos.size()),
            .pQueueCreateInfos = queueInfos.data(),
            .enabledExtensionCount = 1,
            .ppEnabledExtensionNames = deviceExtensions,
        };

        VkDevice device = VK_NULL_HANDLE;
        ExpectVk(vkCreateDevice(selection.physicalDevice, &deviceInfo, nullptr, &device), "vkCreateDevice");

        VkQueue graphicsQueue = VK_NULL_HANDLE;
        VkQueue presentQueue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, selection.graphicsQueueFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, selection.presentQueueFamily, 0, &presentQueue);

        VkSurfaceCapabilitiesKHR capabilities{};
        ExpectVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(selection.physicalDevice, surface, &capabilities),
                 "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

        std::uint32_t formatCount = 0;
        ExpectVk(vkGetPhysicalDeviceSurfaceFormatsKHR(selection.physicalDevice, surface, &formatCount, nullptr),
                 "vkGetPhysicalDeviceSurfaceFormatsKHR(count)");
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        ExpectVk(vkGetPhysicalDeviceSurfaceFormatsKHR(selection.physicalDevice, surface, &formatCount, formats.data()),
                 "vkGetPhysicalDeviceSurfaceFormatsKHR(list)");
        if (formats.empty()) {
            throw std::runtime_error("No Vulkan surface formats were available.");
        }
        const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(formats);

        std::uint32_t presentModeCount = 0;
        ExpectVk(vkGetPhysicalDeviceSurfacePresentModesKHR(selection.physicalDevice, surface, &presentModeCount, nullptr),
                 "vkGetPhysicalDeviceSurfacePresentModesKHR(count)");
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        if (presentModeCount > 0) {
            ExpectVk(vkGetPhysicalDeviceSurfacePresentModesKHR(selection.physicalDevice, surface, &presentModeCount, presentModes.data()),
                     "vkGetPhysicalDeviceSurfacePresentModesKHR(list)");
        }
        const VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);

        VkExtent2D extent = capabilities.currentExtent;
        if (extent.width == std::numeric_limits<std::uint32_t>::max()) {
            extent.width = static_cast<std::uint32_t>(inputImage.width);
            extent.height = static_cast<std::uint32_t>(inputImage.height);
        }

        std::uint32_t imageCount = capabilities.minImageCount + 1U;
        if (capabilities.maxImageCount > 0U) {
            imageCount = std::min(imageCount, capabilities.maxImageCount);
        }

        VkSwapchainCreateInfoKHR swapchainInfo{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface,
            .minImageCount = imageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .imageSharingMode = selection.graphicsQueueFamily == selection.presentQueueFamily
                ? VK_SHARING_MODE_EXCLUSIVE
                : VK_SHARING_MODE_CONCURRENT,
            .queueFamilyIndexCount = selection.graphicsQueueFamily == selection.presentQueueFamily ? 0U : 2U,
            .pQueueFamilyIndices = selection.graphicsQueueFamily == selection.presentQueueFamily ? nullptr : families.data(),
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = presentMode,
            .clipped = VK_TRUE,
        };
        if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0U) {
            throw std::runtime_error("Swapchain images do not support transfer destinations on this device.");
        }

        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        ExpectVk(vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain), "vkCreateSwapchainKHR");

        std::uint32_t swapchainImageCount = 0;
        ExpectVk(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr), "vkGetSwapchainImagesKHR(count)");
        std::vector<VkImage> swapchainImages(swapchainImageCount);
        ExpectVk(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data()), "vkGetSwapchainImagesKHR(list)");

        const PreviewImageData convertedImage = ConvertPreviewImage(inputImage, surfaceFormat.format);
        const VkDeviceSize uploadSize = static_cast<VkDeviceSize>(convertedImage.pixels.size());

        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        const VkBufferCreateInfo bufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = uploadSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        ExpectVk(vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer), "vkCreateBuffer");

        VkMemoryRequirements bufferRequirements{};
        vkGetBufferMemoryRequirements(device, stagingBuffer, &bufferRequirements);
        const VkMemoryAllocateInfo bufferMemoryInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = bufferRequirements.size,
            .memoryTypeIndex = FindMemoryType(selection.physicalDevice,
                                              bufferRequirements.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
        };
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        ExpectVk(vkAllocateMemory(device, &bufferMemoryInfo, nullptr, &stagingMemory), "vkAllocateMemory(buffer)");
        ExpectVk(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0), "vkBindBufferMemory");

        void* mappedMemory = nullptr;
        ExpectVk(vkMapMemory(device, stagingMemory, 0, uploadSize, 0, &mappedMemory), "vkMapMemory");
        std::memcpy(mappedMemory, convertedImage.pixels.data(), convertedImage.pixels.size());
        vkUnmapMemory(device, stagingMemory);

        const VkCommandPoolCreateInfo commandPoolInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = selection.graphicsQueueFamily,
        };
        VkCommandPool commandPool = VK_NULL_HANDLE;
        ExpectVk(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool), "vkCreateCommandPool");

        std::vector<VkCommandBuffer> commandBuffers(swapchainImages.size(), VK_NULL_HANDLE);
        const VkCommandBufferAllocateInfo commandBufferInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = static_cast<std::uint32_t>(commandBuffers.size()),
        };
        ExpectVk(vkAllocateCommandBuffers(device, &commandBufferInfo, commandBuffers.data()), "vkAllocateCommandBuffers");

        const VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkSemaphore renderFinished = VK_NULL_HANDLE;
        ExpectVk(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailable), "vkCreateSemaphore(imageAvailable)");
        ExpectVk(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinished), "vkCreateSemaphore(renderFinished)");

        const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
        VkFence renderFence = VK_NULL_HANDLE;
        ExpectVk(vkCreateFence(device, &fenceInfo, nullptr, &renderFence), "vkCreateFence");

        std::vector<bool> imageInitialized(swapchainImages.size(), false);

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        while (windowState.running) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (!windowState.running) {
                break;
            }

            ExpectVk(vkWaitForFences(device, 1, &renderFence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
            ExpectVk(vkResetFences(device, 1, &renderFence), "vkResetFences");

            std::uint32_t imageIndex = 0;
            VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &imageIndex);
            if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
                continue;
            }
            ExpectVk(acquireResult, "vkAcquireNextImageKHR");

            const VkCommandBuffer commandBuffer = commandBuffers[imageIndex];
            ExpectVk(vkResetCommandBuffer(commandBuffer, 0), "vkResetCommandBuffer");
            const VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            ExpectVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer");

            VkImageMemoryBarrier toTransfer{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .oldLayout = imageInitialized[imageIndex] ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = swapchainImages[imageIndex],
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            vkCmdPipelineBarrier(commandBuffer,
                                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0, nullptr,
                                 0, nullptr,
                                 1, &toTransfer);

            const VkBufferImageCopy copyRegion{
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
                .imageOffset = {0, 0, 0},
                .imageExtent = {static_cast<std::uint32_t>(convertedImage.width), static_cast<std::uint32_t>(convertedImage.height), 1},
            };
            vkCmdCopyBufferToImage(commandBuffer,
                                   stagingBuffer,
                                   swapchainImages[imageIndex],
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   1,
                                   &copyRegion);

            VkImageMemoryBarrier toPresent{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = 0,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = swapchainImages[imageIndex],
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            vkCmdPipelineBarrier(commandBuffer,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 0,
                                 0, nullptr,
                                 0, nullptr,
                                 1, &toPresent);

            ExpectVk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");

            const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            const VkSubmitInfo submitInfo{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &imageAvailable,
                .pWaitDstStageMask = &waitStage,
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &renderFinished,
            };
            ExpectVk(vkQueueSubmit(graphicsQueue, 1, &submitInfo, renderFence), "vkQueueSubmit");

            const VkPresentInfoKHR presentInfo{
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &renderFinished,
                .swapchainCount = 1,
                .pSwapchains = &swapchain,
                .pImageIndices = &imageIndex,
            };
            const VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
            if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR) {
                ExpectVk(presentResult, "vkQueuePresentKHR");
            }
            imageInitialized[imageIndex] = true;

            WaitMessage();
        }

        vkDeviceWaitIdle(device);
        vkDestroyFence(device, renderFence, nullptr);
        vkDestroySemaphore(device, renderFinished, nullptr);
        vkDestroySemaphore(device, imageAvailable, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        return true;
    } catch (const std::exception& exception) {
        if (error != nullptr) {
            *error = exception.what();
        }
        return false;
    }
}

bool RunVulkanSoftwarePreviewLoop(const int windowWidth,
                                  const int windowHeight,
                                  const int softwareRenderWidth,
                                  const int softwareRenderHeight,
                                  const std::function<void(PreviewImageData& frame)>& fillFrame,
                                  const VulkanPreviewWindowOptions& options,
                                  std::string* error) {
    try {
        if (windowWidth <= 0 || windowHeight <= 0) {
            throw std::runtime_error("RunVulkanSoftwarePreviewLoop: invalid window dimensions.");
        }
        if (softwareRenderWidth <= 0 || softwareRenderHeight <= 0) {
            throw std::runtime_error("RunVulkanSoftwarePreviewLoop: invalid software render dimensions.");
        }
        if (softwareRenderWidth > windowWidth || softwareRenderHeight > windowHeight) {
            throw std::runtime_error(
                "RunVulkanSoftwarePreviewLoop: software render size must not exceed the window client size.");
        }
        if (!fillFrame) {
            throw std::runtime_error("RunVulkanSoftwarePreviewLoop: empty fill callback.");
        }

        ScopedWindowClass windowClass(&PreviewWindowProc);
        WindowState windowState{};
        windowState.messageUserData = options.messageUserData;
        windowState.onWin32Message = options.onWin32Message;

        RECT rect{0, 0, windowWidth, windowHeight};
        AdjustWindowRect(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
        HWND hwnd = CreateWindowExW(
            0,
            windowClass.className,
            Widen(options.windowTitle).c_str(),
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,
            nullptr,
            windowClass.instance,
            &windowState);
        if (hwnd == nullptr) {
            throw std::runtime_error("CreateWindowExW failed for Vulkan software preview loop.");
        }
        if (options.outClientHwnd != nullptr) {
            *static_cast<HWND*>(options.outClientHwnd) = hwnd;
        }

        const std::array<const char*, 2> extensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        };
        const VkApplicationInfo applicationInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "RawIron Vulkan Software Loop",
            .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
            .pEngineName = "RawIron",
            .engineVersion = VK_MAKE_VERSION(0, 1, 0),
            .apiVersion = VK_API_VERSION_1_0,
        };
        const VkInstanceCreateInfo instanceInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &applicationInfo,
            .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
        };

        VkInstance instance = VK_NULL_HANDLE;
        ExpectVk(vkCreateInstance(&instanceInfo, nullptr, &instance), "vkCreateInstance");

        const VkWin32SurfaceCreateInfoKHR surfaceInfo{
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hinstance = windowClass.instance,
            .hwnd = hwnd,
        };
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        ExpectVk(vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface), "vkCreateWin32SurfaceKHR");

        const DeviceSelection selection = PickDevice(instance, surface);

        const float queuePriority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        std::vector<std::uint32_t> families = {selection.graphicsQueueFamily, selection.presentQueueFamily};
        std::sort(families.begin(), families.end());
        families.erase(std::unique(families.begin(), families.end()), families.end());
        for (const std::uint32_t family : families) {
            queueInfos.push_back(VkDeviceQueueCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = family,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority,
            });
        }

        const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        const VkDeviceCreateInfo deviceInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = static_cast<std::uint32_t>(queueInfos.size()),
            .pQueueCreateInfos = queueInfos.data(),
            .enabledExtensionCount = 1,
            .ppEnabledExtensionNames = deviceExtensions,
        };

        VkDevice device = VK_NULL_HANDLE;
        ExpectVk(vkCreateDevice(selection.physicalDevice, &deviceInfo, nullptr, &device), "vkCreateDevice");

        VkQueue graphicsQueue = VK_NULL_HANDLE;
        VkQueue presentQueue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, selection.graphicsQueueFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, selection.presentQueueFamily, 0, &presentQueue);

        VkSurfaceCapabilitiesKHR capabilities{};
        ExpectVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(selection.physicalDevice, surface, &capabilities),
                 "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

        std::uint32_t formatCount = 0;
        ExpectVk(vkGetPhysicalDeviceSurfaceFormatsKHR(selection.physicalDevice, surface, &formatCount, nullptr),
                 "vkGetPhysicalDeviceSurfaceFormatsKHR(count)");
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        ExpectVk(vkGetPhysicalDeviceSurfaceFormatsKHR(selection.physicalDevice, surface, &formatCount, formats.data()),
                 "vkGetPhysicalDeviceSurfaceFormatsKHR(list)");
        if (formats.empty()) {
            throw std::runtime_error("No Vulkan surface formats were available.");
        }
        const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(formats);

        std::uint32_t presentModeCount = 0;
        ExpectVk(vkGetPhysicalDeviceSurfacePresentModesKHR(selection.physicalDevice, surface, &presentModeCount, nullptr),
                 "vkGetPhysicalDeviceSurfacePresentModesKHR(count)");
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        if (presentModeCount > 0) {
            ExpectVk(vkGetPhysicalDeviceSurfacePresentModesKHR(selection.physicalDevice, surface, &presentModeCount, presentModes.data()),
                     "vkGetPhysicalDeviceSurfacePresentModesKHR(list)");
        }
        const VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);

        VkExtent2D extent = capabilities.currentExtent;
        if (extent.width == std::numeric_limits<std::uint32_t>::max()) {
            extent.width = static_cast<std::uint32_t>(windowWidth);
            extent.height = static_cast<std::uint32_t>(windowHeight);
        }

        std::uint32_t imageCount = capabilities.minImageCount + 1U;
        if (capabilities.maxImageCount > 0U) {
            imageCount = std::min(imageCount, capabilities.maxImageCount);
        }

        VkSwapchainCreateInfoKHR swapchainInfo{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface,
            .minImageCount = imageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .imageSharingMode = selection.graphicsQueueFamily == selection.presentQueueFamily
                ? VK_SHARING_MODE_EXCLUSIVE
                : VK_SHARING_MODE_CONCURRENT,
            .queueFamilyIndexCount = selection.graphicsQueueFamily == selection.presentQueueFamily ? 0U : 2U,
            .pQueueFamilyIndices = selection.graphicsQueueFamily == selection.presentQueueFamily ? nullptr : families.data(),
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = presentMode,
            .clipped = VK_TRUE,
        };
        if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0U) {
            throw std::runtime_error("Swapchain images do not support transfer destinations on this device.");
        }

        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        ExpectVk(vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain), "vkCreateSwapchainKHR");

        std::uint32_t swapchainImageCount = 0;
        ExpectVk(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr), "vkGetSwapchainImagesKHR(count)");
        std::vector<VkImage> swapchainImages(swapchainImageCount);
        ExpectVk(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data()), "vkGetSwapchainImagesKHR(list)");

        const std::uint32_t swapPixelW = extent.width;
        const std::uint32_t swapPixelH = extent.height;
        const bool directBufferToSwapchain =
            static_cast<std::uint32_t>(softwareRenderWidth) == swapPixelW &&
            static_cast<std::uint32_t>(softwareRenderHeight) == swapPixelH;

        VkFormatProperties surfaceFormatProps{};
        vkGetPhysicalDeviceFormatProperties(selection.physicalDevice, surfaceFormat.format, &surfaceFormatProps);
        const bool formatSupportsBlit =
            (surfaceFormatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0U &&
            (surfaceFormatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0U;

        VkImage softwareRgbImage = VK_NULL_HANDLE;
        VkDeviceMemory softwareRgbMemory = VK_NULL_HANDLE;

        if (!directBufferToSwapchain) {
            if (static_cast<std::uint32_t>(softwareRenderWidth) > swapPixelW ||
                static_cast<std::uint32_t>(softwareRenderHeight) > swapPixelH) {
                throw std::runtime_error(
                    "RunVulkanSoftwarePreviewLoop: internal render resolution exceeds the swapchain; use a larger window "
                    "or --render-scale=1.");
            }
            if (!formatSupportsBlit) {
                throw std::runtime_error(
                    "RunVulkanSoftwarePreviewLoop: GPU blit is required for sub-native rendering but this surface "
                    "format does not support blits. Try --render-scale=1.");
            }

            const VkImageCreateInfo softwareImageInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = surfaceFormat.format,
                .extent = {static_cast<std::uint32_t>(softwareRenderWidth),
                           static_cast<std::uint32_t>(softwareRenderHeight),
                           1},
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            };
            ExpectVk(vkCreateImage(device, &softwareImageInfo, nullptr, &softwareRgbImage), "vkCreateImage(softwareRgb)");

            VkMemoryRequirements softwareReq{};
            vkGetImageMemoryRequirements(device, softwareRgbImage, &softwareReq);
            const VkMemoryAllocateInfo softwareAlloc{
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = softwareReq.size,
                .memoryTypeIndex =
                    FindMemoryType(selection.physicalDevice, softwareReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
            };
            ExpectVk(vkAllocateMemory(device, &softwareAlloc, nullptr, &softwareRgbMemory), "vkAllocateMemory(softwareRgb)");
            ExpectVk(vkBindImageMemory(device, softwareRgbImage, softwareRgbMemory, 0), "vkBindImageMemory(softwareRgb)");
        }

        PreviewImageData convertedScratch{};
        PreviewImageData scratch{};
        scratch.width = softwareRenderWidth;
        scratch.height = softwareRenderHeight;
        scratch.format = PreviewPixelFormat::Rgba8;
        scratch.pixels.resize(static_cast<std::size_t>(softwareRenderWidth * softwareRenderHeight * 3), 0);

        const VkDeviceSize uploadSize =
            static_cast<VkDeviceSize>(static_cast<std::size_t>(softwareRenderWidth * softwareRenderHeight) * 4ULL);

        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        const VkBufferCreateInfo bufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = uploadSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        ExpectVk(vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer), "vkCreateBuffer");

        VkMemoryRequirements bufferRequirements{};
        vkGetBufferMemoryRequirements(device, stagingBuffer, &bufferRequirements);
        const VkMemoryAllocateInfo bufferMemoryInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = bufferRequirements.size,
            .memoryTypeIndex = FindMemoryType(selection.physicalDevice,
                                              bufferRequirements.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
        };
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        ExpectVk(vkAllocateMemory(device, &bufferMemoryInfo, nullptr, &stagingMemory), "vkAllocateMemory(buffer)");
        ExpectVk(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0), "vkBindBufferMemory");

        void* mappedMemory = nullptr;
        ExpectVk(vkMapMemory(device, stagingMemory, 0, uploadSize, 0, &mappedMemory), "vkMapMemory");

        const VkCommandPoolCreateInfo commandPoolInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = selection.graphicsQueueFamily,
        };
        VkCommandPool commandPool = VK_NULL_HANDLE;
        ExpectVk(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool), "vkCreateCommandPool");

        std::vector<VkCommandBuffer> commandBuffers(swapchainImages.size(), VK_NULL_HANDLE);
        const VkCommandBufferAllocateInfo commandBufferInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = static_cast<std::uint32_t>(commandBuffers.size()),
        };
        ExpectVk(vkAllocateCommandBuffers(device, &commandBufferInfo, commandBuffers.data()), "vkAllocateCommandBuffers");

        const VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkSemaphore renderFinished = VK_NULL_HANDLE;
        ExpectVk(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailable), "vkCreateSemaphore(imageAvailable)");
        ExpectVk(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinished), "vkCreateSemaphore(renderFinished)");

        const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
        VkFence renderFence = VK_NULL_HANDLE;
        ExpectVk(vkCreateFence(device, &fenceInfo, nullptr, &renderFence), "vkCreateFence");

        std::vector<bool> imageInitialized(swapchainImages.size(), false);
        bool softwareRgbLayoutIsSrc = false;

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        while (windowState.running) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (!windowState.running) {
                break;
            }

            fillFrame(scratch);
            ConvertPreviewImageInto(convertedScratch, scratch, surfaceFormat.format);
            if (convertedScratch.pixels.size() != static_cast<std::size_t>(uploadSize)) {
                throw std::runtime_error("RunVulkanSoftwarePreviewLoop: converted frame size mismatch.");
            }
            std::memcpy(mappedMemory, convertedScratch.pixels.data(), convertedScratch.pixels.size());

            ExpectVk(vkWaitForFences(device, 1, &renderFence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
            ExpectVk(vkResetFences(device, 1, &renderFence), "vkResetFences");

            std::uint32_t imageIndex = 0;
            VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &imageIndex);
            if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
                continue;
            }
            ExpectVk(acquireResult, "vkAcquireNextImageKHR");

            const VkCommandBuffer commandBuffer = commandBuffers[imageIndex];
            ExpectVk(vkResetCommandBuffer(commandBuffer, 0), "vkResetCommandBuffer");
            const VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            ExpectVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer");

            VkImageMemoryBarrier toTransfer{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .oldLayout = imageInitialized[imageIndex] ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = swapchainImages[imageIndex],
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            vkCmdPipelineBarrier(commandBuffer,
                                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0, nullptr,
                                 0, nullptr,
                                 1, &toTransfer);

            if (!directBufferToSwapchain) {
                VkImageMemoryBarrier softwareToWrite{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .srcAccessMask =
                        softwareRgbLayoutIsSrc ? VkAccessFlags{VK_ACCESS_TRANSFER_READ_BIT} : VkAccessFlags{0},
                    .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .oldLayout =
                        softwareRgbLayoutIsSrc ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = softwareRgbImage,
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                };
                vkCmdPipelineBarrier(commandBuffer,
                                       softwareRgbLayoutIsSrc ? VK_PIPELINE_STAGE_TRANSFER_BIT
                                                                : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                                       0,
                                       0, nullptr,
                                       0, nullptr,
                                       1, &softwareToWrite);
            }

            const VkExtent3D copyExtent{
                static_cast<std::uint32_t>(softwareRenderWidth),
                static_cast<std::uint32_t>(softwareRenderHeight),
                1,
            };
            const VkBufferImageCopy copyRegion{
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
                .imageOffset = {0, 0, 0},
                .imageExtent = copyExtent,
            };
            vkCmdCopyBufferToImage(commandBuffer,
                                   stagingBuffer,
                                   directBufferToSwapchain ? swapchainImages[imageIndex] : softwareRgbImage,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   1,
                                   &copyRegion);

            if (!directBufferToSwapchain) {
                VkImageMemoryBarrier softwareToSrc{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = softwareRgbImage,
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                };
                vkCmdPipelineBarrier(commandBuffer,
                                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                                       0,
                                       0, nullptr,
                                       0, nullptr,
                                       1, &softwareToSrc);
                softwareRgbLayoutIsSrc = true;

                VkImageBlit blitRegion{};
                blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blitRegion.srcSubresource.mipLevel = 0;
                blitRegion.srcSubresource.baseArrayLayer = 0;
                blitRegion.srcSubresource.layerCount = 1;
                blitRegion.srcOffsets[0] = {0, 0, 0};
                blitRegion.srcOffsets[1] = {softwareRenderWidth, softwareRenderHeight, 1};
                blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blitRegion.dstSubresource.mipLevel = 0;
                blitRegion.dstSubresource.baseArrayLayer = 0;
                blitRegion.dstSubresource.layerCount = 1;
                blitRegion.dstOffsets[0] = {0, 0, 0};
                blitRegion.dstOffsets[1] = {static_cast<int32_t>(swapPixelW), static_cast<int32_t>(swapPixelH), 1};

                vkCmdBlitImage(commandBuffer,
                               softwareRgbImage,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               swapchainImages[imageIndex],
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1,
                               &blitRegion,
                               VK_FILTER_LINEAR);
            }

            VkImageMemoryBarrier toPresent{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = 0,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = swapchainImages[imageIndex],
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            vkCmdPipelineBarrier(commandBuffer,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 0,
                                 0, nullptr,
                                 0, nullptr,
                                 1, &toPresent);

            ExpectVk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");

            const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            const VkSubmitInfo submitInfo{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &imageAvailable,
                .pWaitDstStageMask = &waitStage,
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &renderFinished,
            };
            ExpectVk(vkQueueSubmit(graphicsQueue, 1, &submitInfo, renderFence), "vkQueueSubmit");

            const VkPresentInfoKHR presentInfo{
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &renderFinished,
                .swapchainCount = 1,
                .pSwapchains = &swapchain,
                .pImageIndices = &imageIndex,
            };
            const VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
            if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR) {
                ExpectVk(presentResult, "vkQueuePresentKHR");
            }
            imageInitialized[imageIndex] = true;
        }

        vkDeviceWaitIdle(device);
        vkUnmapMemory(device, stagingMemory);
        if (softwareRgbImage != VK_NULL_HANDLE) {
            vkDestroyImage(device, softwareRgbImage, nullptr);
        }
        if (softwareRgbMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, softwareRgbMemory, nullptr);
        }
        vkDestroyFence(device, renderFence, nullptr);
        vkDestroySemaphore(device, renderFinished, nullptr);
        vkDestroySemaphore(device, imageAvailable, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        return true;
    } catch (const std::exception& exception) {
        if (error != nullptr) {
            *error = exception.what();
        }
        return false;
    }
}

bool PresentSceneKitPreviewWindow(const ri::scene::SceneKitPreview& preview,
                                  const int width,
                                  const int height,
                                  const VulkanPreviewWindowOptions& options,
                                  std::string* error) {
    std::string bridgeError;
    const ri::scene::PhotoModeCameraOverrides* const photoForBridge =
        ri::scene::PhotoModeFieldOfViewActive(options.scenePhotoMode) ? &options.scenePhotoMode : nullptr;
    if (!BuildSceneKitPreviewVulkanBridge(preview, width, height, photoForBridge, nullptr, &bridgeError)) {
        if (error != nullptr) {
            *error = bridgeError;
        }
        return false;
    }

    const ri::render::software::ScenePreviewOptions previewOptions{
        .width = std::max(width, 1),
        .height = std::max(height, 1),
        .photoMode = options.scenePhotoMode,
    };
    const ri::render::software::SoftwareImage image = ri::render::software::RenderScenePreview(
        preview.scene,
        preview.orbitCamera.cameraNode,
        previewOptions);
    return PresentPreviewImageWindow(
        BuildVulkanPreviewImage(image),
        options,
        error);
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

} // namespace ri::render::vulkan

#else

namespace ri::render::vulkan {

bool PresentPreviewImageWindow(const PreviewImageData&, const VulkanPreviewWindowOptions&, std::string* error) {
    if (error != nullptr) {
        *error = "Vulkan preview presentation is currently only implemented on Windows.";
    }
    return false;
}

bool RunVulkanSoftwarePreviewLoop(int,
                                  int,
                                  int,
                                  int,
                                  const std::function<void(PreviewImageData&)>&,
                                  const VulkanPreviewWindowOptions&,
                                  std::string* error) {
    if (error != nullptr) {
        *error = "Vulkan software preview loop is currently only implemented on Windows.";
    }
    return false;
}

bool BuildSceneKitPreviewVulkanBridge(const ri::scene::SceneKitPreview&,
                                      int,
                                      int,
                                      const ri::scene::PhotoModeCameraOverrides*,
                                      SceneKitPreviewRenderBridgeStats*,
                                      std::string* error) {
    if (error != nullptr) {
        *error = "Vulkan scene preview validation is currently only implemented on Windows.";
    }
    return false;
}

bool PresentSceneKitPreviewWindow(const ri::scene::SceneKitPreview&,
                                  int,
                                  int,
                                  const VulkanPreviewWindowOptions&,
                                  std::string* error) {
    if (error != nullptr) {
        *error = "Vulkan Scene Kit preview presentation is currently only implemented on Windows.";
    }
    return false;
}

} // namespace ri::render::vulkan

#endif
