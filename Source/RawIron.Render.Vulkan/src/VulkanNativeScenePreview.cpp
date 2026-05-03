#include "RawIron/Render/VulkanPreviewPresenter.h"
#include "RawIron/Render/HybridPresentationTargets.h"

#if defined(_WIN32)
#include "RawIron/Core/Log.h"
#include "RawIron/Core/RenderRecorder.h"
#include "RawIron/Core/RenderSubmissionPlan.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Render/VulkanCommandList.h"
#include "RawIron/Math/Vec2.h"
#include "RawIron/Render/PreviewTexture.h"
#include "RawIron/Scene/SceneKit.h"
#include "RawIron/Scene/SceneRenderSubmission.h"
#include "RawIron/Scene/SceneUtils.h"
#define VK_USE_PLATFORM_WIN32_KHR 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#ifndef RAWIRON_VULKAN_NATIVE_PREVIEW_ENABLED
#define RAWIRON_VULKAN_NATIVE_PREVIEW_ENABLED 0
#endif

namespace ri::render::vulkan {

namespace {

namespace fs = std::filesystem;

struct NativeSceneVertex {
    float position[3]{};
    float normal[3]{};
    float uv[2]{};
};

struct NativeSceneDraw {
    std::int32_t meshHandle = -1;
    std::int32_t materialHandle = -1;
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t instanceCount = 1;
    std::array<float, 16> model{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    std::array<float, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
    std::array<float, 3> emissiveColor{0.0f, 0.0f, 0.0f};
    float metallic = 0.0f;
    float roughness = 1.0f;
    std::array<float, 2> textureTiling{1.0f, 1.0f};
    bool useTexture = false;
    bool litShadingModel = false;
    bool alphaCutout = false;
    /// Resolved path under `textureRoot` for this frame (animated sequences); empty falls back to material lookup.
    std::string resolvedAlbedoRelPath{};
    bool nativeWaterUvMotion = false;
    bool additiveBlend = false;
};

struct NativeScenePreviewData {
    const ri::scene::Scene* scene = nullptr;
    fs::path textureRoot{};
    std::array<float, 16> viewProjection{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    std::array<float, 4> clearColor{0.05f, 0.07f, 0.10f, 1.0f};
    std::vector<NativeSceneDraw> draws{};
    float sceneAnimationTimeSeconds = 0.0f;
    std::array<float, 4> cameraWorldPosition{{0.0f, 0.0f, 0.0f, 1.0f}};
    /// x=exposure, y=contrast, z=saturation, w=fog density
    std::array<float, 4> renderTuning{{1.0f, 1.0f, 1.0f, 0.0095f}};
    /// x=noise amount, y=scanline amount, z=barrel distortion, w=chromatic aberration
    std::array<float, 4> postProcessPrimary{{0.0f, 0.0f, 0.0f, 0.0f}};
    /// rgb=tint color, a=tint strength
    std::array<float, 4> postProcessTint{{1.0f, 1.0f, 1.0f, 0.0f}};
    /// x=blur, y=static fade, z=time seconds, w=non-zero: hybrid HDR linear radiance (composite tonemap/post).
    std::array<float, 4> postProcessSecondary{{0.0f, 0.0f, 0.0f, 0.0f}};
    int renderQualityTier = 1;
    std::array<float, 16> lightViewProjection{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    /// xyz=world-space light direction, w=sun intensity
    std::array<float, 4> lightDirectionIntensity{{0.34f, 0.86f, 0.31f, 1.0f}};
    /// xyz=world-space local light position, w=range
    std::array<float, 4> localLightPositionRange{{0.0f, 1.8f, 0.0f, 20.0f}};
    /// rgb=local light color, w=intensity multiplier
    std::array<float, 4> localLightColorIntensity{{1.0f, 0.92f, 0.82f, 2.0f}};
    /// x=width px, y=height px, z=1/width, w=1/height (post radial / vignette).
    std::array<float, 4> viewportMetrics{{1920.0f, 1080.0f, 1.0f / 1920.0f, 1.0f / 1080.0f}};
    /// Column-major `mat4` for `NativeSkybox.vert` (`projection * skyRotation`).
    std::array<float, 16> skyClipFromLocal{};
    /// Column-major `mat4`; upper 3x3 maps eye-space directions to world for equirect sampling.
    std::array<float, 16> skyEyeToWorld{};
    std::int32_t skyUseTextureFile = 0;
    fs::path skyEquirectAbsolute{};
};

struct alignas(16) SkyUniformStd140 {
    std::int32_t hasSkyTexture = 0;
    std::int32_t pad0 = 0;
    std::int32_t pad1 = 0;
    std::int32_t pad2 = 0;
    float clipFromLocal[16]{};
    float eyeToWorldRotation[16]{};
};

static_assert(sizeof(SkyUniformStd140) == 144, "Must match NativeSkybox.{vert,frag} std140 layout.");

struct alignas(16) CameraUniformStd140 {
    float viewProjection[16]{};
    float cameraWorldPosition[4]{};
    float renderTuning[4]{};
    float postProcessPrimary[4]{};
    float postProcessTint[4]{};
    float postProcessSecondary[4]{};
    float lightViewProjection[16]{};
    float lightDirectionIntensity[4]{};
    float localLightPositionRange[4]{};
    float localLightColorIntensity[4]{};
    float viewportMetrics[4]{};
};

static_assert(sizeof(CameraUniformStd140) == 272, "Must match NativeScenePreview shader CameraData std140 layout.");

void StoreMat4ColumnMajorGlsl(const ri::math::Mat4& matrix, float destination[16]) {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            destination[column * 4 + row] = matrix.m[row][column];
        }
    }
}

void StoreMat4ColumnMajorGlsl(const ri::math::Mat4& matrix, std::array<float, 16>& destination) {
    StoreMat4ColumnMajorGlsl(matrix, destination.data());
}

struct WindowState {
    HWND hwnd = nullptr;
    bool running = true;
    void* messageUserData = nullptr;
    VulkanPreviewWindowOptions::Win32MessageHook onWin32Message = nullptr;
};

struct ScopedWindowClass {
    HINSTANCE instance = nullptr;
    const wchar_t* className = L"RawIronVulkanNativeScenePreviewWindow";
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
            throw std::runtime_error("RegisterClassW failed for Vulkan native scene preview window.");
        }
    }

    ~ScopedWindowClass() {
        if (atom != 0) {
            UnregisterClassW(className, instance);
        }
    }
};

struct DeviceSelection {
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    std::uint32_t graphicsQueueFamily = 0;
    std::uint32_t presentQueueFamily = 0;
};

struct BufferResource {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

struct CachedGpuMesh {
    BufferResource vertexBuffer{};
    BufferResource indexBuffer{};
    std::uint32_t indexCount = 0;
};

struct NativeDrawPushConstants {
    float model[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float tiling[2] = {1.0f, 1.0f};
    std::int32_t useTexture = 0;
    std::int32_t nativeWaterUvMotion = 0;
    float nativeWaterTime = 0.0f;
    std::int32_t litShadingModel = 0;
    float metallic = 0.0f;
    float roughness = 1.0f;
    float emissiveColor[3] = {0.0f, 0.0f, 0.0f};
    float qualityTier = 1.0f;
};
static_assert(sizeof(NativeDrawPushConstants) == 128, "Must match NativeScenePreview.{vert,frag} push_constant layout.");
static_assert(offsetof(NativeDrawPushConstants, useTexture) == 88);
static_assert(offsetof(NativeDrawPushConstants, nativeWaterUvMotion) == 92);
static_assert(offsetof(NativeDrawPushConstants, nativeWaterTime) == 96);
static_assert(offsetof(NativeDrawPushConstants, litShadingModel) == 100);
static_assert(offsetof(NativeDrawPushConstants, metallic) == 104);
static_assert(offsetof(NativeDrawPushConstants, roughness) == 108);
static_assert(offsetof(NativeDrawPushConstants, emissiveColor) == 112);
static_assert(offsetof(NativeDrawPushConstants, qualityTier) == 124);
static_assert(sizeof(NativeSceneVertex) == 32);

struct CpuMeshGeometry {
    std::vector<NativeSceneVertex> vertices{};
    std::vector<std::uint32_t> indices{};
};

struct ImageResource {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

BufferResource CreateBuffer(VkPhysicalDevice physicalDevice,
                            VkDevice device,
                            VkDeviceSize size,
                            VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags memoryFlags);

constexpr std::array<ri::math::Vec3, 8> kCubeVertices = {{
    {-0.5f, -0.5f, -0.5f},
    {0.5f, -0.5f, -0.5f},
    {0.5f, 0.5f, -0.5f},
    {-0.5f, 0.5f, -0.5f},
    {-0.5f, -0.5f, 0.5f},
    {0.5f, -0.5f, 0.5f},
    {0.5f, 0.5f, 0.5f},
    {-0.5f, 0.5f, 0.5f},
}};

constexpr std::array<int, 36> kCubeIndices = {{
    4, 5, 6, 4, 6, 7,
    1, 0, 3, 1, 3, 2,
    0, 4, 7, 0, 7, 3,
    5, 1, 2, 5, 2, 6,
    3, 7, 6, 3, 6, 2,
    0, 1, 5, 0, 5, 4,
}};

constexpr std::array<ri::math::Vec3, 4> kPlaneVertices = {{
    {-0.5f, 0.0f, -0.5f},
    {0.5f, 0.0f, -0.5f},
    {-0.5f, 0.0f, 0.5f},
    {0.5f, 0.0f, 0.5f},
}};

constexpr std::array<int, 6> kPlaneIndices = {{
    0, 2, 1,
    1, 2, 3,
}};

constexpr std::array<std::array<int, 4>, 6> kCubeFaces = {{
    {4, 5, 6, 7},
    {1, 0, 3, 2},
    {0, 4, 7, 3},
    {5, 1, 2, 6},
    {3, 7, 6, 2},
    {0, 1, 5, 4},
}};

constexpr std::array<std::array<ri::math::Vec2, 4>, 6> kCubeFaceCornerUv = {{
    {ri::math::Vec2{0.0f, 0.0f},
     ri::math::Vec2{1.0f, 0.0f},
     ri::math::Vec2{1.0f, 1.0f},
     ri::math::Vec2{0.0f, 1.0f}},
    {ri::math::Vec2{0.0f, 0.0f},
     ri::math::Vec2{1.0f, 0.0f},
     ri::math::Vec2{1.0f, 1.0f},
     ri::math::Vec2{0.0f, 1.0f}},
    {ri::math::Vec2{0.0f, 0.0f},
     ri::math::Vec2{1.0f, 0.0f},
     ri::math::Vec2{1.0f, 1.0f},
     ri::math::Vec2{0.0f, 1.0f}},
    {ri::math::Vec2{0.0f, 0.0f},
     ri::math::Vec2{1.0f, 0.0f},
     ri::math::Vec2{1.0f, 1.0f},
     ri::math::Vec2{0.0f, 1.0f}},
    {ri::math::Vec2{0.0f, 0.0f},
     ri::math::Vec2{1.0f, 0.0f},
     ri::math::Vec2{1.0f, 1.0f},
     ri::math::Vec2{0.0f, 1.0f}},
    {ri::math::Vec2{0.0f, 0.0f},
     ri::math::Vec2{1.0f, 0.0f},
     ri::math::Vec2{1.0f, 1.0f},
     ri::math::Vec2{0.0f, 1.0f}},
}};

void ExpectVk(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(operation) + " failed with VkResult=" + std::to_string(static_cast<int>(result)));
    }
}

LRESULT CALLBACK NativePreviewWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
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
        if ((format.format == VK_FORMAT_B8G8R8A8_SRGB || format.format == VK_FORMAT_R8G8B8A8_SRGB) &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    for (const VkSurfaceFormatKHR& format : formats) {
        if ((format.format == VK_FORMAT_B8G8R8A8_UNORM || format.format == VK_FORMAT_R8G8B8A8_UNORM) &&
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

const char* PresentModeName(const VkPresentModeKHR mode) {
    switch (mode) {
    case VK_PRESENT_MODE_MAILBOX_KHR:
        return "mailbox";
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
        return "immediate";
    case VK_PRESENT_MODE_FIFO_KHR:
        return "fifo";
    default:
        return "other";
    }
}

VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes,
                                   const VulkanPresentModePreference preference) {
    const auto hasMode = [&presentModes](const VkPresentModeKHR mode) {
        return std::find(presentModes.begin(), presentModes.end(), mode) != presentModes.end();
    };
    if (preference == VulkanPresentModePreference::Mailbox && hasMode(VK_PRESENT_MODE_MAILBOX_KHR)) {
        return VK_PRESENT_MODE_MAILBOX_KHR;
    }
    if (preference == VulkanPresentModePreference::Immediate && hasMode(VK_PRESENT_MODE_IMMEDIATE_KHR)) {
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
    if (preference == VulkanPresentModePreference::Fifo && hasMode(VK_PRESENT_MODE_FIFO_KHR)) {
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    for (const VkPresentModeKHR mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            // Prefer low-latency uncapped presentation for high-FPS native benchmarking.
            return mode;
        }
    }
    for (const VkPresentModeKHR mode : presentModes) {
        if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            return mode;
        }
    }
    for (const VkPresentModeKHR mode : presentModes) {
        if (mode == VK_PRESENT_MODE_FIFO_KHR) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

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
        if (extensionCount > 0U) {
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

VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice) {
    const std::array<VkFormat, 3> formats = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (const VkFormat format : formats) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
        if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0U) {
            return format;
        }
    }
    throw std::runtime_error("No supported Vulkan depth format was found.");
}

VkFormat FindHdrSceneColorFormat(VkPhysicalDevice physicalDevice) {
    static_cast<void>(HybridPresentationFormats::kSceneHdrColor);
    const std::array<VkFormat, 2> formats = {
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
    };
    for (const VkFormat format : formats) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
        constexpr VkFormatFeatureFlags required =
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if ((properties.optimalTilingFeatures & required) == required) {
            return format;
        }
    }
    throw std::runtime_error("No Vulkan HDR scene color format (RGBA16F or RGBA32F with attachment + sampling).");
}

VkFormat FindShadowDepthFormat(VkPhysicalDevice physicalDevice) {
    const std::array<VkFormat, 3> formats = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (const VkFormat format : formats) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
        const VkFormatFeatureFlags required =
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if ((properties.optimalTilingFeatures & required) == required) {
            return format;
        }
    }
    throw std::runtime_error("No supported Vulkan shadow depth format was found.");
}

ri::math::Mat4 BuildLookAtMatrix(const ri::math::Vec3& eye, const ri::math::Vec3& target, const ri::math::Vec3& upHint) {
    const ri::math::Vec3 forward = ri::math::Normalize(target - eye);
    const ri::math::Vec3 right = ri::math::Normalize(ri::math::Cross(forward, upHint));
    const ri::math::Vec3 up = ri::math::Cross(right, forward);
    ri::math::Mat4 view = ri::math::IdentityMatrix();
    view.m[0][0] = right.x;
    view.m[0][1] = right.y;
    view.m[0][2] = right.z;
    view.m[0][3] = -ri::math::Dot(right, eye);
    view.m[1][0] = up.x;
    view.m[1][1] = up.y;
    view.m[1][2] = up.z;
    view.m[1][3] = -ri::math::Dot(up, eye);
    view.m[2][0] = -forward.x;
    view.m[2][1] = -forward.y;
    view.m[2][2] = -forward.z;
    view.m[2][3] = ri::math::Dot(forward, eye);
    view.m[3][0] = 0.0f;
    view.m[3][1] = 0.0f;
    view.m[3][2] = 0.0f;
    view.m[3][3] = 1.0f;
    return view;
}

ri::math::Mat4 BuildOrthographicMatrix(const float left,
                                       const float right,
                                       const float bottom,
                                       const float top,
                                       const float nearPlane,
                                       const float farPlane) {
    ri::math::Mat4 projection{};
    projection.m[0][0] = 2.0f / std::max(right - left, 0.001f);
    projection.m[1][1] = 2.0f / std::max(top - bottom, 0.001f);
    projection.m[2][2] = -2.0f / std::max(farPlane - nearPlane, 0.001f);
    projection.m[3][3] = 1.0f;
    projection.m[0][3] = -(right + left) / std::max(right - left, 0.001f);
    projection.m[1][3] = -(top + bottom) / std::max(top - bottom, 0.001f);
    projection.m[2][3] = -(farPlane + nearPlane) / std::max(farPlane - nearPlane, 0.001f);
    return projection;
}

std::vector<char> ReadBinaryFile(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw std::runtime_error("Unable to open shader file: " + path.string());
    }

    const std::streamsize size = stream.tellg();
    if (size <= 0) {
        throw std::runtime_error("Shader file is empty: " + path.string());
    }

    stream.seekg(0, std::ios::beg);
    std::vector<char> data(static_cast<std::size_t>(size));
    if (!stream.read(data.data(), size)) {
        throw std::runtime_error("Unable to read shader file: " + path.string());
    }
    return data;
}

VkShaderModule CreateShaderModule(VkDevice device, const fs::path& path) {
    const std::vector<char> bytes = ReadBinaryFile(path);
    const VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = bytes.size(),
        .pCode = reinterpret_cast<const std::uint32_t*>(bytes.data()),
    };
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    ExpectVk(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule), "vkCreateShaderModule");
    return shaderModule;
}

ri::math::Vec3 ClampColor(const ri::math::Vec3& color) {
    return ri::math::Vec3{
        std::clamp(color.x, 0.0f, 1.0f),
        std::clamp(color.y, 0.0f, 1.0f),
        std::clamp(color.z, 0.0f, 1.0f),
    };
}

void SetNativeVertex(NativeSceneVertex& vertex,
                     const ri::math::Vec3& position,
                     const ri::math::Vec3& normal,
                     const ri::math::Vec2& uv) {
    vertex.position[0] = position.x;
    vertex.position[1] = position.y;
    vertex.position[2] = position.z;
    vertex.normal[0] = normal.x;
    vertex.normal[1] = normal.y;
    vertex.normal[2] = normal.z;
    vertex.uv[0] = uv.x;
    vertex.uv[1] = uv.y;
}

CpuMeshGeometry BuildCubeMeshGeometryExpanded() {
    CpuMeshGeometry geometry{};
    geometry.vertices.reserve(36U);
    geometry.indices.reserve(36U);
    for (std::size_t face = 0; face < kCubeFaces.size(); ++face) {
        const std::array<int, 4>& faceIdx = kCubeFaces[face];
        const ri::math::Vec3& p0 = kCubeVertices[static_cast<std::size_t>(faceIdx[0])];
        const ri::math::Vec3& p1 = kCubeVertices[static_cast<std::size_t>(faceIdx[1])];
        const ri::math::Vec3& p2 = kCubeVertices[static_cast<std::size_t>(faceIdx[2])];
        const ri::math::Vec3 faceNormal = ri::math::Normalize(ri::math::Cross(p1 - p0, p2 - p0));
        const auto emitCorner = [&](int cornerIndex) {
            const ri::math::Vec3& p = kCubeVertices[static_cast<std::size_t>(faceIdx[static_cast<std::size_t>(cornerIndex)])];
            const ri::math::Vec2& uv = kCubeFaceCornerUv[face][static_cast<std::size_t>(cornerIndex)];
            NativeSceneVertex vertex{};
            SetNativeVertex(vertex, p, faceNormal, uv);
            geometry.vertices.push_back(vertex);
            geometry.indices.push_back(static_cast<std::uint32_t>(geometry.vertices.size() - 1U));
        };
        emitCorner(0);
        emitCorner(1);
        emitCorner(2);
        emitCorner(0);
        emitCorner(2);
        emitCorner(3);
    }
    return geometry;
}

CpuMeshGeometry BuildPlaneMeshGeometryUv() {
    CpuMeshGeometry geometry{};
    geometry.vertices.reserve(4U);
    geometry.indices.reserve(6U);
    std::vector<ri::math::Vec3> normals(4U, ri::math::Vec3{0.0f, 1.0f, 0.0f});
    for (std::size_t i = 0; i < kPlaneVertices.size(); ++i) {
        const ri::math::Vec3& p = kPlaneVertices[i];
        const ri::math::Vec2 uv{p.x + 0.5f, p.z + 0.5f};
        NativeSceneVertex vertex{};
        SetNativeVertex(vertex, p, normals[i], uv);
        geometry.vertices.push_back(vertex);
    }
    for (const int index : kPlaneIndices) {
        geometry.indices.push_back(static_cast<std::uint32_t>(index));
    }
    return geometry;
}

CpuMeshGeometry BuildIndexedMeshGeometryUv(const std::vector<ri::math::Vec3>& positions,
                                           const std::vector<ri::math::Vec2>& texCoords,
                                           const std::vector<std::uint32_t>& indices,
                                           bool hasUv) {
    CpuMeshGeometry geometry{};
    if (positions.empty() || indices.empty() || (indices.size() % 3U) != 0U) {
        return geometry;
    }

    std::vector<ri::math::Vec3> averagedNormals(positions.size(), ri::math::Vec3{0.0f, 0.0f, 0.0f});
    for (std::size_t triangleIndex = 0; triangleIndex + 2U < indices.size(); triangleIndex += 3U) {
        const std::uint32_t ia = indices[triangleIndex + 0U];
        const std::uint32_t ib = indices[triangleIndex + 1U];
        const std::uint32_t ic = indices[triangleIndex + 2U];
        if (ia >= positions.size() || ib >= positions.size() || ic >= positions.size()) {
            continue;
        }
        const ri::math::Vec3 faceNormal = ri::math::Cross(positions[ib] - positions[ia], positions[ic] - positions[ia]);
        averagedNormals[ia] = averagedNormals[ia] + faceNormal;
        averagedNormals[ib] = averagedNormals[ib] + faceNormal;
        averagedNormals[ic] = averagedNormals[ic] + faceNormal;
    }
    for (std::size_t index = 0; index < positions.size(); ++index) {
        ri::math::Vec3 normal = averagedNormals[index];
        if (ri::math::Length(normal) <= 0.0001f) {
            normal = ri::math::Vec3{0.0f, 1.0f, 0.0f};
        } else {
            normal = ri::math::Normalize(normal);
        }
        averagedNormals[index] = normal;
    }

    geometry.vertices.reserve(indices.size());
    geometry.indices.reserve(indices.size());
    for (std::size_t triangleIndex = 0; triangleIndex + 2U < indices.size(); triangleIndex += 3U) {
        const std::uint32_t ia = indices[triangleIndex + 0U];
        const std::uint32_t ib = indices[triangleIndex + 1U];
        const std::uint32_t ic = indices[triangleIndex + 2U];
        if (ia >= positions.size() || ib >= positions.size() || ic >= positions.size()) {
            continue;
        }
        const auto cornerUv = [&](std::uint32_t vertexIndex) -> ri::math::Vec2 {
            if (hasUv && vertexIndex < texCoords.size()) {
                return texCoords[vertexIndex];
            }
            return ri::math::Vec2{0.0f, 0.0f};
        };
        for (const std::uint32_t corner : {ia, ib, ic}) {
            NativeSceneVertex vertex{};
            SetNativeVertex(vertex, positions[corner], averagedNormals[corner], cornerUv(corner));
            geometry.vertices.push_back(vertex);
            geometry.indices.push_back(static_cast<std::uint32_t>(geometry.vertices.size() - 1U));
        }
    }
    return geometry;
}

std::vector<std::uint32_t> BuildSequentialIndices(const std::vector<ri::math::Vec3>& positions) {
    std::vector<std::uint32_t> indices;
    indices.reserve(positions.size());
    for (std::size_t index = 0; index < positions.size(); ++index) {
        indices.push_back(static_cast<std::uint32_t>(index));
    }
    return indices;
}

CpuMeshGeometry BuildNativeMeshGeometry(const ri::scene::Mesh& mesh) {
    switch (mesh.primitive) {
    case ri::scene::PrimitiveType::Cube:
        return BuildCubeMeshGeometryExpanded();
    case ri::scene::PrimitiveType::Plane:
        return BuildPlaneMeshGeometryUv();
    case ri::scene::PrimitiveType::Sphere:
    case ri::scene::PrimitiveType::Custom: {
        if (!mesh.indices.empty()) {
            std::vector<std::uint32_t> indices;
            indices.reserve(mesh.indices.size());
            for (const int index : mesh.indices) {
                if (index >= 0) {
                    indices.push_back(static_cast<std::uint32_t>(index));
                }
            }
            const bool hasUv = mesh.texCoords.size() == mesh.positions.size();
            return BuildIndexedMeshGeometryUv(mesh.positions, mesh.texCoords, indices, hasUv);
        }
        return BuildIndexedMeshGeometryUv(mesh.positions, mesh.texCoords, BuildSequentialIndices(mesh.positions), false);
    }
    }
    return {};
}

[[nodiscard]] std::string MaterialNameLowerAscii(const ri::scene::Material& material) {
    std::string out = material.name;
    for (char& character : out) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return out;
}

[[nodiscard]] bool NativePreviewIsWaterLikeMaterial(const ri::scene::Material& material) {
    const std::string lower = MaterialNameLowerAscii(material);
    if (lower.find("water") != std::string::npos) {
        return true;
    }
    if (material.baseColorTextureFrames.size() >= 8U && material.baseColorTextureFramesPerSecond >= 4.0f) {
        return true;
    }
    return false;
}

[[nodiscard]] std::string ResolveNativePreviewAlbedoRelPath(const ri::scene::Material& material,
                                                            double animationTimeSeconds) {
    if (!material.baseColorTextureFrames.empty()) {
        std::size_t frameIndex = 0;
        if (material.baseColorTextureFramesPerSecond > 0.0f && material.baseColorTextureFrames.size() > 1U) {
            const double frameCursor =
                std::floor(std::max(0.0, animationTimeSeconds)
                            * static_cast<double>(material.baseColorTextureFramesPerSecond));
            frameIndex = static_cast<std::size_t>(
                static_cast<long long>(frameCursor) % static_cast<long long>(material.baseColorTextureFrames.size()));
        }
        const std::string& framePath = material.baseColorTextureFrames[frameIndex];
        if (!framePath.empty()) {
            return framePath;
        }
    }
    return material.baseColorTexture;
}

bool BuildNativeScenePreviewData(const ri::scene::Scene& scene,
                                 int cameraNode,
                                 int width,
                                 int height,
                                 const ri::scene::PhotoModeCameraOverrides* photoMode,
                                 const fs::path& textureRoot,
                                 const fs::path& skyEquirectRelativeToTextureRoot,
                                 double animationTimeSeconds,
                                 NativeScenePreviewData* outData,
                                 std::string* error) {
    try {
        if (outData == nullptr) {
            throw std::runtime_error("Native scene preview output was null.");
        }
        auto absolutePathExistsCached = [](const fs::path& path) {
            static std::unordered_map<std::string, bool> cache;
            const std::string key = path.lexically_normal().string();
            const auto found = cache.find(key);
            if (found != cache.end()) {
                return found->second;
            }
            const bool exists = fs::is_regular_file(path);
            cache.emplace(key, exists);
            return exists;
        };

        ri::scene::SceneRenderSubmissionOptions submissionOptions{};
        submissionOptions.viewportWidth = std::max(width, 1);
        submissionOptions.viewportHeight = std::max(height, 1);
        // Native Vulkan draws the full authored scene each frame; distance/occlusion LOD from the software
        // preview path causes visible popping on large void levels, so keep those toggles off here.
        submissionOptions.enableFarHorizon = false;
        submissionOptions.enableCoarseOcclusion = false;
        submissionOptions.enableFrustumCulling = false;
        if (photoMode != nullptr && ri::scene::PhotoModeFieldOfViewActive(*photoMode)) {
            submissionOptions.photoMode = *photoMode;
        }

        const ri::scene::SceneRenderSubmission submission = ri::scene::BuildSceneRenderSubmission(
            scene,
            cameraNode,
            submissionOptions);
        if (submission.stats.cameraNodeHandle == ri::scene::kInvalidHandle) {
            throw std::runtime_error("Scene preview does not expose a valid camera for native Vulkan rendering.");
        }

        const ri::core::RenderSubmissionPlan plan = ri::core::BuildRenderSubmissionPlan(submission.commands);
        VulkanCommandListSink sink{};
        ri::core::RenderRecorderStats recorderStats{};
        if (!ri::core::ExecuteRenderSubmissionPlan(submission.commands, plan, sink, &recorderStats)) {
            throw std::runtime_error("Failed to execute render submission plan for native Vulkan rendering.");
        }

        NativeScenePreviewData& data = *outData;
        data.scene = &scene;
        data.textureRoot = textureRoot;
        data.viewProjection = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
        data.clearColor = {0.05f, 0.07f, 0.10f, 1.0f};
        data.sceneAnimationTimeSeconds = static_cast<float>(animationTimeSeconds);
        data.cameraWorldPosition = {{0.0f, 0.0f, 0.0f, 1.0f}};
        data.skyClipFromLocal.fill(0.0f);
        data.skyEyeToWorld.fill(0.0f);
        data.skyUseTextureFile = 0;
        data.skyEquirectAbsolute.clear();
        data.draws.clear();
        const int submissionCamera = submission.stats.cameraNodeHandle;
        if (submissionCamera != ri::scene::kInvalidHandle) {
            const ri::math::Vec3 cameraWorld = scene.ComputeWorldPosition(submissionCamera);
            data.cameraWorldPosition = {{cameraWorld.x, cameraWorld.y, cameraWorld.z, 1.0f}};
            const float aspectRatio = static_cast<float>(std::max(width, 1))
                / static_cast<float>(std::max(height, 1));
            const ri::math::Mat4 projection = ri::scene::BuildCameraProjectionMatrix(
                scene,
                submissionCamera,
                aspectRatio,
                photoMode);
            const ri::math::Mat4 skyRotation = ri::scene::BuildCameraSkyRotationMatrix(scene, submissionCamera);
            const ri::math::Mat4 clipSky = ri::math::Multiply(projection, skyRotation);
            ri::math::Mat4 eyeToWorld = ri::math::IdentityMatrix();
            for (int row = 0; row < 3; ++row) {
                for (int column = 0; column < 3; ++column) {
                    eyeToWorld.m[row][column] = skyRotation.m[column][row];
                }
            }
            StoreMat4ColumnMajorGlsl(clipSky, data.skyClipFromLocal);
            StoreMat4ColumnMajorGlsl(eyeToWorld, data.skyEyeToWorld);
            if (!skyEquirectRelativeToTextureRoot.empty()) {
                fs::path absoluteSky = skyEquirectRelativeToTextureRoot.lexically_normal();
                if (!skyEquirectRelativeToTextureRoot.is_absolute() && !textureRoot.empty()) {
                    absoluteSky = (textureRoot / skyEquirectRelativeToTextureRoot).lexically_normal();
                }
                if (absolutePathExistsCached(absoluteSky)) {
                    data.skyUseTextureFile = 1;
                    data.skyEquirectAbsolute = std::move(absoluteSky);
                }
            }
        }
        for (const VulkanRenderOp& operation : sink.Operations()) {
            switch (operation.type) {
            case VulkanRenderOpType::ClearColor:
                data.clearColor = {
                    operation.clearColor[0],
                    operation.clearColor[1],
                    operation.clearColor[2],
                    operation.clearColor[3],
                };
                break;
            case VulkanRenderOpType::SetViewProjection:
                std::copy(std::begin(operation.viewProjection), std::end(operation.viewProjection), data.viewProjection.begin());
                break;
            case VulkanRenderOpType::DrawMesh: {
                if (operation.meshHandle < 0) {
                    break;
                }
                const ri::scene::Material material = operation.materialHandle >= 0
                    ? scene.GetMaterial(operation.materialHandle)
                    : ri::scene::Material{};
                const ri::math::Vec3 color = ClampColor(material.baseColor);
                NativeSceneDraw draw{};
                draw.meshHandle = operation.meshHandle;
                draw.materialHandle = operation.materialHandle;
                draw.firstIndex = operation.firstIndex;
                draw.indexCount = operation.indexCount;
                draw.instanceCount = std::max(operation.instanceCount, 1U);
                std::copy(std::begin(operation.model), std::end(operation.model), draw.model.begin());
                draw.color = {
                    color.x,
                    color.y,
                    color.z,
                    std::clamp(material.opacity, 0.0f, 1.0f),
                };
                if (material.alphaCutoff > 0.01f && material.alphaCutoff < 0.99f && !material.transparent) {
                    draw.alphaCutout = true;
                    draw.color[3] = std::clamp(material.alphaCutoff, 0.01f, 0.99f);
                }
                draw.emissiveColor = {
                    std::clamp(material.emissiveColor.x, 0.0f, 16.0f),
                    std::clamp(material.emissiveColor.y, 0.0f, 16.0f),
                    std::clamp(material.emissiveColor.z, 0.0f, 16.0f),
                };
                draw.metallic = std::clamp(material.metallic, 0.0f, 1.0f);
                draw.roughness = std::clamp(material.roughness, 0.04f, 1.0f);
                draw.textureTiling = {material.textureTiling.x, material.textureTiling.y};
                {
                    const bool hasNamedTexture = !material.baseColorTexture.empty();
                    const bool hasFrameTexture =
                        !material.baseColorTextureFrames.empty() && !material.baseColorTextureFrames.front().empty();
                    draw.useTexture = hasNamedTexture || hasFrameTexture;
                }
                if (draw.useTexture) {
                    draw.resolvedAlbedoRelPath = ResolveNativePreviewAlbedoRelPath(material, animationTimeSeconds);
                    draw.nativeWaterUvMotion = NativePreviewIsWaterLikeMaterial(material);
                }
                draw.litShadingModel = material.shadingModel == ri::scene::ShadingModel::Lit;
                draw.additiveBlend = material.additiveBlend;
                data.draws.push_back(draw);
                break;
            }
            default:
                break;
            }
        }

        {
            const ri::math::Vec3 cameraPos{
                data.cameraWorldPosition[0],
                data.cameraWorldPosition[1],
                data.cameraWorldPosition[2],
            };
            const ri::math::Vec3 lightDir = ri::math::Normalize(ri::math::Vec3{0.34f, 0.86f, 0.31f});
            const float orthoRadius = 85.0f;
            const ri::math::Vec3 shadowCenter = cameraPos + ri::math::Vec3{0.0f, 3.0f, 0.0f};
            const ri::math::Vec3 lightEye = shadowCenter - (lightDir * 120.0f);
            const ri::math::Mat4 lightView = BuildLookAtMatrix(lightEye, shadowCenter, ri::math::Vec3{0.0f, 1.0f, 0.0f});
            const ri::math::Mat4 lightProjection = BuildOrthographicMatrix(
                -orthoRadius, orthoRadius, -orthoRadius, orthoRadius, 8.0f, 260.0f);
            const ri::math::Mat4 lightViewProjection = ri::math::Multiply(lightProjection, lightView);
            StoreMat4ColumnMajorGlsl(lightViewProjection, data.lightViewProjection);
            data.lightDirectionIntensity = {lightDir.x, lightDir.y, lightDir.z, 1.0f};
            data.localLightPositionRange = {cameraPos.x, cameraPos.y + 0.15f, cameraPos.z, 24.0f};
            data.localLightColorIntensity = {1.0f, 0.93f, 0.84f, 1.9f};
        }

        return true;
    } catch (const std::exception& exception) {
        if (error != nullptr) {
            *error = exception.what();
        }
        return false;
    }
}

bool BuildNativeScenePreviewData(const VulkanNativeSceneFrame& frame,
                                 int width,
                                 int height,
                                 NativeScenePreviewData* outData,
                                 std::string* error) {
    if (frame.scene == nullptr) {
        if (error != nullptr) {
            *error = "Native Vulkan scene callback did not provide a scene.";
        }
        return false;
    }
    if (frame.cameraNode == ri::scene::kInvalidHandle) {
        if (error != nullptr) {
            *error = "Native Vulkan scene callback did not provide a valid camera node.";
        }
        return false;
    }
    const ri::scene::PhotoModeCameraOverrides* const photoMode =
        frame.photoModeEnabled && ri::scene::PhotoModeFieldOfViewActive(frame.photoMode) ? &frame.photoMode : nullptr;
    const fs::path textureRoot = !frame.textureRoot.empty() ? frame.textureRoot : fs::path{};
    const bool ok = BuildNativeScenePreviewData(*frame.scene,
                                                frame.cameraNode,
                                                width,
                                                height,
                                                photoMode,
                                                textureRoot,
                                                frame.skyEquirectTextureRelative,
                                                frame.animationTimeSeconds,
                                                outData,
                                                error);
    if (!ok) {
        return false;
    }
    outData->renderTuning = {
        std::clamp(frame.renderExposure, 0.5f, 2.5f),
        std::clamp(frame.renderContrast, 0.7f, 1.6f),
        std::clamp(frame.renderSaturation, 0.0f, 1.8f),
        std::clamp(frame.renderFogDensity, 0.0f, 0.05f),
    };
    const ri::render::PostProcessParameters sanitizedPost = ri::render::SanitizePostProcessParameters(frame.postProcess);
    outData->postProcessPrimary = {
        sanitizedPost.noiseAmount,
        sanitizedPost.scanlineAmount,
        sanitizedPost.barrelDistortion,
        sanitizedPost.chromaticAberration,
    };
    outData->postProcessTint = {
        sanitizedPost.tintColor.x,
        sanitizedPost.tintColor.y,
        sanitizedPost.tintColor.z,
        sanitizedPost.tintStrength,
    };
    outData->postProcessSecondary = {
        sanitizedPost.blurAmount,
        sanitizedPost.staticFadeAmount,
        sanitizedPost.timeSeconds,
        0.0f,
    };
    outData->renderQualityTier = std::clamp(frame.renderQualityTier, 0, 2);
    const float vw = static_cast<float>(std::max(width, 1));
    const float vh = static_cast<float>(std::max(height, 1));
    outData->viewportMetrics = {vw, vh, 1.0f / vw, 1.0f / vh};
    return true;
}

void DestroyBuffer(VkDevice device, BufferResource& resource) {
    if (resource.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, resource.buffer, nullptr);
        resource.buffer = VK_NULL_HANDLE;
    }
    if (resource.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, resource.memory, nullptr);
        resource.memory = VK_NULL_HANDLE;
    }
}

void EnsureMappedBufferCapacity(VkPhysicalDevice physicalDevice,
                                VkDevice device,
                                VkBufferUsageFlags usage,
                                VkDeviceSize requiredSize,
                                BufferResource& resource,
                                VkDeviceSize& capacity,
                                void*& mappedMemory,
                                const char* label) {
    const VkDeviceSize targetSize = std::max<VkDeviceSize>(requiredSize, 1U);
    if (resource.buffer != VK_NULL_HANDLE && capacity >= targetSize && mappedMemory != nullptr) {
        return;
    }

    if (mappedMemory != nullptr && resource.memory != VK_NULL_HANDLE) {
        vkUnmapMemory(device, resource.memory);
        mappedMemory = nullptr;
    }
    DestroyBuffer(device, resource);

    resource = CreateBuffer(physicalDevice,
                            device,
                            targetSize,
                            usage,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    capacity = targetSize;
    ExpectVk(vkMapMemory(device, resource.memory, 0, VK_WHOLE_SIZE, 0, &mappedMemory), label);
}

void DestroyCachedGpuMesh(VkDevice device, CachedGpuMesh& mesh) {
    DestroyBuffer(device, mesh.vertexBuffer);
    DestroyBuffer(device, mesh.indexBuffer);
    mesh.indexCount = 0;
}

void ClearGpuMeshCache(VkDevice device, std::unordered_map<int, CachedGpuMesh>& cache) {
    for (auto& [meshHandle, mesh] : cache) {
        (void)meshHandle;
        DestroyCachedGpuMesh(device, mesh);
    }
    cache.clear();
}

void EnsureGpuMeshCached(VkPhysicalDevice physicalDevice,
                         VkDevice device,
                         const ri::scene::Scene& scene,
                         int meshHandle,
                         std::unordered_map<int, CachedGpuMesh>& cache) {
    if (cache.contains(meshHandle)) {
        return;
    }

    const CpuMeshGeometry geometry = BuildNativeMeshGeometry(scene.GetMesh(meshHandle));
    if (geometry.vertices.empty() || geometry.indices.empty()) {
        throw std::runtime_error("Unable to build Vulkan mesh geometry for mesh handle " + std::to_string(meshHandle) + ".");
    }

    CachedGpuMesh gpuMesh{};
    const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(geometry.vertices.size() * sizeof(NativeSceneVertex));
    gpuMesh.vertexBuffer = CreateBuffer(physicalDevice,
                                        device,
                                        vertexBytes,
                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* mappedVertexMemory = nullptr;
    ExpectVk(vkMapMemory(device, gpuMesh.vertexBuffer.memory, 0, VK_WHOLE_SIZE, 0, &mappedVertexMemory), "vkMapMemory(cached-vertex)");
    std::memcpy(mappedVertexMemory, geometry.vertices.data(), static_cast<std::size_t>(vertexBytes));
    vkUnmapMemory(device, gpuMesh.vertexBuffer.memory);

    const VkDeviceSize indexBytes = static_cast<VkDeviceSize>(geometry.indices.size() * sizeof(std::uint32_t));
    gpuMesh.indexBuffer = CreateBuffer(physicalDevice,
                                       device,
                                       indexBytes,
                                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* mappedIndexMemory = nullptr;
    ExpectVk(vkMapMemory(device, gpuMesh.indexBuffer.memory, 0, VK_WHOLE_SIZE, 0, &mappedIndexMemory), "vkMapMemory(cached-index)");
    std::memcpy(mappedIndexMemory, geometry.indices.data(), static_cast<std::size_t>(indexBytes));
    vkUnmapMemory(device, gpuMesh.indexBuffer.memory);

    gpuMesh.indexCount = static_cast<std::uint32_t>(geometry.indices.size());
    cache.emplace(meshHandle, std::move(gpuMesh));
}

CachedGpuMesh CreateStaticUnitCubeGpuMesh(VkPhysicalDevice physicalDevice, VkDevice device) {
    const CpuMeshGeometry geometry = BuildCubeMeshGeometryExpanded();
    if (geometry.vertices.empty() || geometry.indices.empty()) {
        throw std::runtime_error("Unable to build static unit cube mesh for Vulkan skybox.");
    }

    CachedGpuMesh gpuMesh{};
    const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(geometry.vertices.size() * sizeof(NativeSceneVertex));
    gpuMesh.vertexBuffer = CreateBuffer(physicalDevice,
                                        device,
                                        vertexBytes,
                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* mappedVertexMemory = nullptr;
    ExpectVk(vkMapMemory(device, gpuMesh.vertexBuffer.memory, 0, VK_WHOLE_SIZE, 0, &mappedVertexMemory),
             "vkMapMemory(skybox-vertex)");
    std::memcpy(mappedVertexMemory, geometry.vertices.data(), static_cast<std::size_t>(vertexBytes));
    vkUnmapMemory(device, gpuMesh.vertexBuffer.memory);

    const VkDeviceSize indexBytes = static_cast<VkDeviceSize>(geometry.indices.size() * sizeof(std::uint32_t));
    gpuMesh.indexBuffer = CreateBuffer(physicalDevice,
                                       device,
                                       indexBytes,
                                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* mappedIndexMemory = nullptr;
    ExpectVk(vkMapMemory(device, gpuMesh.indexBuffer.memory, 0, VK_WHOLE_SIZE, 0, &mappedIndexMemory),
             "vkMapMemory(skybox-index)");
    std::memcpy(mappedIndexMemory, geometry.indices.data(), static_cast<std::size_t>(indexBytes));
    vkUnmapMemory(device, gpuMesh.indexBuffer.memory);

    gpuMesh.indexCount = static_cast<std::uint32_t>(geometry.indices.size());
    return gpuMesh;
}

BufferResource CreateBuffer(VkPhysicalDevice physicalDevice,
                            VkDevice device,
                            VkDeviceSize size,
                            VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags memoryFlags) {
    BufferResource resource{};
    const VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = std::max<VkDeviceSize>(size, 1U),
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    ExpectVk(vkCreateBuffer(device, &bufferInfo, nullptr, &resource.buffer), "vkCreateBuffer");

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, resource.buffer, &requirements);
    const VkMemoryAllocateInfo allocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = FindMemoryType(physicalDevice, requirements.memoryTypeBits, memoryFlags),
    };
    ExpectVk(vkAllocateMemory(device, &allocateInfo, nullptr, &resource.memory), "vkAllocateMemory(buffer)");
    ExpectVk(vkBindBufferMemory(device, resource.buffer, resource.memory, 0), "vkBindBufferMemory");
    return resource;
}

ImageResource CreateDepthImage(VkPhysicalDevice physicalDevice,
                               VkDevice device,
                               VkFormat format,
                               std::uint32_t width,
                               std::uint32_t height,
                               VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    ImageResource resource{};
    const VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ExpectVk(vkCreateImage(device, &imageInfo, nullptr, &resource.image), "vkCreateImage(depth)");

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, resource.image, &requirements);
    const VkMemoryAllocateInfo allocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = FindMemoryType(physicalDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    ExpectVk(vkAllocateMemory(device, &allocateInfo, nullptr, &resource.memory), "vkAllocateMemory(depth)");
    ExpectVk(vkBindImageMemory(device, resource.image, resource.memory, 0), "vkBindImageMemory(depth)");

    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = resource.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    ExpectVk(vkCreateImageView(device, &viewInfo, nullptr, &resource.view), "vkCreateImageView(depth)");
    return resource;
}

ImageResource CreateHdrSceneColorImage(VkPhysicalDevice physicalDevice,
                                       VkDevice device,
                                       VkFormat format,
                                       std::uint32_t width,
                                       std::uint32_t height) {
    ImageResource resource{};
    const VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ExpectVk(vkCreateImage(device, &imageInfo, nullptr, &resource.image), "vkCreateImage(hdr-scene)");

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, resource.image, &requirements);
    const VkMemoryAllocateInfo allocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = FindMemoryType(physicalDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    ExpectVk(vkAllocateMemory(device, &allocateInfo, nullptr, &resource.memory), "vkAllocateMemory(hdr-scene)");
    ExpectVk(vkBindImageMemory(device, resource.image, resource.memory, 0), "vkBindImageMemory(hdr-scene)");

    const VkImageViewCreateInfo hdrViewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = resource.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    ExpectVk(vkCreateImageView(device, &hdrViewInfo, nullptr, &resource.view), "vkCreateImageView(hdr-scene)");
    return resource;
}

struct GpuAlbedoImage {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

void DestroyGpuAlbedoImage(VkDevice device, GpuAlbedoImage& image) {
    if (image.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, image.view, nullptr);
        image.view = VK_NULL_HANDLE;
    }
    if (image.image != VK_NULL_HANDLE) {
        vkDestroyImage(device, image.image, nullptr);
        image.image = VK_NULL_HANDLE;
    }
    if (image.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, image.memory, nullptr);
        image.memory = VK_NULL_HANDLE;
    }
}

void SubmitOneTimeCommands(VkDevice device,
                           VkCommandPool commandPool,
                           VkQueue queue,
                           const std::function<void(VkCommandBuffer)>& recorder) {
    const VkCommandBufferAllocateInfo allocateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    ExpectVk(vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer), "vkAllocateCommandBuffers(upload)");

    const VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    ExpectVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer(upload)");
    recorder(commandBuffer);
    ExpectVk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer(upload)");

    const VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
    };
    ExpectVk(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit(upload)");
    ExpectVk(vkQueueWaitIdle(queue), "vkQueueWaitIdle(upload)");
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

GpuAlbedoImage CreateGpuAlbedoImageRgba8(VkPhysicalDevice physicalDevice,
                                         VkDevice device,
                                         VkCommandPool commandPool,
                                         VkQueue queue,
                                         const int width,
                                         const int height,
                                         const std::uint8_t* rgbaPixels) {
    if (width <= 0 || height <= 0 || rgbaPixels == nullptr) {
        return {};
    }

    const VkDeviceSize pixelBytes = static_cast<VkDeviceSize>(width * height * 4);
    BufferResource staging = CreateBuffer(physicalDevice,
                                            device,
                                            pixelBytes,
                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* mapped = nullptr;
    ExpectVk(vkMapMemory(device, staging.memory, 0, VK_WHOLE_SIZE, 0, &mapped), "vkMapMemory(staging-tex)");
    std::memcpy(mapped, rgbaPixels, static_cast<std::size_t>(pixelBytes));
    vkUnmapMemory(device, staging.memory);

    GpuAlbedoImage out{};
    const VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ExpectVk(vkCreateImage(device, &imageInfo, nullptr, &out.image), "vkCreateImage(albedo)");

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, out.image, &requirements);
    const VkMemoryAllocateInfo allocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = FindMemoryType(physicalDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    ExpectVk(vkAllocateMemory(device, &allocateInfo, nullptr, &out.memory), "vkAllocateMemory(albedo)");
    ExpectVk(vkBindImageMemory(device, out.image, out.memory, 0), "vkBindImageMemory(albedo)");

    SubmitOneTimeCommands(device, commandPool, queue, [&](VkCommandBuffer cmd) {
        const VkImageMemoryBarrier undefToDst{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = out.image,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &undefToDst);

        const VkBufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .imageExtent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1},
        };
        vkCmdCopyBufferToImage(cmd, staging.buffer, out.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        const VkImageMemoryBarrier dstToShader{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = out.image,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &dstToShader);
    });

    DestroyBuffer(device, staging);

    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = out.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    ExpectVk(vkCreateImageView(device, &viewInfo, nullptr, &out.view), "vkCreateImageView(albedo)");
    return out;
}

struct NativeAlbedoTextureCache {
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkDescriptorPool textureDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout textureSetLayout = VK_NULL_HANDLE;
    VkSampler linearSampler = VK_NULL_HANDLE;
    GpuAlbedoImage whiteImage{};
    VkDescriptorSet whiteDescriptorSet = VK_NULL_HANDLE;
    std::unordered_map<std::string, GpuAlbedoImage> imagesByKey{};
    std::unordered_map<std::string, VkDescriptorSet> descriptorByKey{};

    void destroy() {
        if (device == VK_NULL_HANDLE) {
            return;
        }
        descriptorByKey.clear();
        whiteDescriptorSet = VK_NULL_HANDLE;
        if (textureDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, textureDescriptorPool, nullptr);
            textureDescriptorPool = VK_NULL_HANDLE;
        }
        for (auto& entry : imagesByKey) {
            DestroyGpuAlbedoImage(device, entry.second);
        }
        imagesByKey.clear();
        DestroyGpuAlbedoImage(device, whiteImage);
        if (linearSampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, linearSampler, nullptr);
            linearSampler = VK_NULL_HANDLE;
        }
        textureSetLayout = VK_NULL_HANDLE;
        device = VK_NULL_HANDLE;
        physicalDevice = VK_NULL_HANDLE;
        commandPool = VK_NULL_HANDLE;
        graphicsQueue = VK_NULL_HANDLE;
    }

    [[nodiscard]] std::string ResolveMaterialTextureRelPath(const ri::scene::Material& material) const {
        if (!material.baseColorTexture.empty()) {
            return material.baseColorTexture;
        }
        if (!material.baseColorTextureFrames.empty() && !material.baseColorTextureFrames.front().empty()) {
            return material.baseColorTextureFrames.front();
        }
        return {};
    }

    [[nodiscard]] VkDescriptorSet descriptorFor(const ri::scene::Scene& scene,
                                               const NativeSceneDraw& draw,
                                               const fs::path& textureRoot) {
        if (whiteDescriptorSet == VK_NULL_HANDLE) {
            return VK_NULL_HANDLE;
        }
        if (!draw.useTexture || draw.materialHandle < 0) {
            return whiteDescriptorSet;
        }
        const ri::scene::Material& material = scene.GetMaterial(draw.materialHandle);
        const std::string rel =
            !draw.resolvedAlbedoRelPath.empty() ? draw.resolvedAlbedoRelPath : ResolveMaterialTextureRelPath(material);
        if (rel.empty()) {
            return whiteDescriptorSet;
        }
        const fs::path requestedPath = fs::path(rel).lexically_normal();
        const fs::path fullPath =
            requestedPath.is_absolute() ? requestedPath
                                        : (!textureRoot.empty() ? (textureRoot / requestedPath).lexically_normal() : fs::path{});
        if (fullPath.empty()) {
            return whiteDescriptorSet;
        }
        const std::string key = fullPath.generic_string();
        if (const auto it = descriptorByKey.find(key); it != descriptorByKey.end()) {
            return it->second;
        }

        const ri::render::software::RgbaImage rgba = ri::render::software::LoadRgbaImageFile(fullPath);
        if (!rgba.Valid()) {
            descriptorByKey.emplace(key, whiteDescriptorSet);
            return whiteDescriptorSet;
        }

        GpuAlbedoImage gpuImage = CreateGpuAlbedoImageRgba8(
            physicalDevice, device, commandPool, graphicsQueue, rgba.width, rgba.height, rgba.rgba.data());
        if (gpuImage.view == VK_NULL_HANDLE) {
            descriptorByKey.emplace(key, whiteDescriptorSet);
            return whiteDescriptorSet;
        }

        VkDescriptorSet set = VK_NULL_HANDLE;
        const VkDescriptorSetAllocateInfo allocateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = textureDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &textureSetLayout,
        };
        if (vkAllocateDescriptorSets(device, &allocateInfo, &set) != VK_SUCCESS) {
            DestroyGpuAlbedoImage(device, gpuImage);
            return whiteDescriptorSet;
        }

        const VkDescriptorImageInfo imageInfo{
            .sampler = linearSampler,
            .imageView = gpuImage.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        const VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfo,
        };
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

        imagesByKey.emplace(key, std::move(gpuImage));
        descriptorByKey.emplace(key, set);
        return set;
    }

    [[nodiscard]] VkDescriptorSet descriptorForAbsolutePath(const fs::path& absolutePath) {
        if (whiteDescriptorSet == VK_NULL_HANDLE) {
            return VK_NULL_HANDLE;
        }
        if (absolutePath.empty()) {
            return whiteDescriptorSet;
        }

        const std::string key = std::string("__abs__|") + absolutePath.generic_string();
        if (const auto it = descriptorByKey.find(key); it != descriptorByKey.end()) {
            return it->second;
        }

        const ri::render::software::RgbaImage rgba = ri::render::software::LoadRgbaImageFile(absolutePath);
        if (!rgba.Valid()) {
            descriptorByKey.emplace(key, whiteDescriptorSet);
            return whiteDescriptorSet;
        }

        GpuAlbedoImage gpuImage = CreateGpuAlbedoImageRgba8(
            physicalDevice, device, commandPool, graphicsQueue, rgba.width, rgba.height, rgba.rgba.data());
        if (gpuImage.view == VK_NULL_HANDLE) {
            descriptorByKey.emplace(key, whiteDescriptorSet);
            return whiteDescriptorSet;
        }

        VkDescriptorSet set = VK_NULL_HANDLE;
        const VkDescriptorSetAllocateInfo allocateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = textureDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &textureSetLayout,
        };
        if (vkAllocateDescriptorSets(device, &allocateInfo, &set) != VK_SUCCESS) {
            DestroyGpuAlbedoImage(device, gpuImage);
            return whiteDescriptorSet;
        }

        const VkDescriptorImageInfo imageInfo{
            .sampler = linearSampler,
            .imageView = gpuImage.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        const VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfo,
        };
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

        imagesByKey.emplace(key, std::move(gpuImage));
        descriptorByKey.emplace(key, set);
        return set;
    }

    void initialize(VkPhysicalDevice physDevice,
                    VkDevice dev,
                    VkCommandPool pool,
                    VkQueue queue,
                    VkDescriptorSetLayout textureLayout,
                    VkDescriptorPool texturePool,
                    VkSampler sampler) {
        physicalDevice = physDevice;
        device = dev;
        commandPool = pool;
        graphicsQueue = queue;
        textureSetLayout = textureLayout;
        textureDescriptorPool = texturePool;
        linearSampler = sampler;

        constexpr std::uint8_t whitePixel[4] = {255, 255, 255, 255};
        whiteImage = CreateGpuAlbedoImageRgba8(physicalDevice, device, commandPool, graphicsQueue, 1, 1, whitePixel);
        if (whiteImage.view == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to create fallback white albedo texture.");
        }

        const VkDescriptorSetAllocateInfo allocateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = textureDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &textureSetLayout,
        };
        ExpectVk(vkAllocateDescriptorSets(device, &allocateInfo, &whiteDescriptorSet), "vkAllocateDescriptorSets(white-albedo)");

        const VkDescriptorImageInfo imageInfo{
            .sampler = linearSampler,
            .imageView = whiteImage.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        const VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = whiteDescriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfo,
        };
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }
};

void RecordHybridCompositeInCommandBuffer(VkCommandBuffer commandBuffer,
                                          VkRenderPass compositeRenderPass,
                                          VkFramebuffer compositeFramebuffer,
                                          VkExtent2D extent,
                                          VkPipeline compositePipeline,
                                          VkPipelineLayout compositePipelineLayout,
                                          VkDescriptorSet cameraDescriptorSet,
                                          VkDescriptorSet hdrTextureDescriptorSet);

void RecordSceneCommandBuffer(VkCommandBuffer commandBuffer,
                              VkRenderPass shadowRenderPass,
                              VkFramebuffer shadowFramebuffer,
                              VkExtent2D shadowExtent,
                              VkPipeline shadowPipeline,
                              VkPipelineLayout shadowPipelineLayout,
                              VkFramebuffer framebuffer,
                              VkRenderPass renderPass,
                              VkExtent2D extent,
                              VkPipeline skyPipeline,
                              VkPipelineLayout skyPipelineLayout,
                              VkDescriptorSet skyDescriptorSet,
                              VkDescriptorSet skyTextureDescriptorSet,
                              const CachedGpuMesh& skyMesh,
                              VkPipeline scenePipeline,
                              VkPipeline scenePipelineAdditive,
                              VkPipelineLayout pipelineLayout,
                              VkDescriptorSet cameraDescriptorSet,
                              VkDescriptorSet shadowDescriptorSet,
                              NativeAlbedoTextureCache& textureCache,
                              const ri::scene::Scene& scene,
                              const std::unordered_map<int, CachedGpuMesh>& meshCache,
                              const NativeScenePreviewData& sceneData,
                              VkFramebuffer hybridCompositeFramebuffer,
                              VkRenderPass hybridCompositeRenderPass,
                              VkPipeline hybridCompositePipeline,
                              VkPipelineLayout hybridCompositePipelineLayout,
                              VkDescriptorSet hybridCompositeHdrDescriptorSet) {
    const VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    ExpectVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer");

    const std::array<VkClearValue, 2> clearValues = {{
        VkClearValue{.color = {
            sceneData.clearColor[0],
            sceneData.clearColor[1],
            sceneData.clearColor[2],
            sceneData.clearColor[3],
        }},
        VkClearValue{.depthStencil = {1.0f, 0}},
    }};
    const VkRenderPassBeginInfo renderPassBeginInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = {0, 0},
            .extent = extent,
        },
        .clearValueCount = static_cast<std::uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data(),
    };
    const VkViewport viewport{
        .x = 0.0f,
        .y = static_cast<float>(extent.height),
        .width = static_cast<float>(extent.width),
        .height = -static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    const VkRect2D scissor{
        .offset = {0, 0},
        .extent = extent,
    };

    if (shadowRenderPass != VK_NULL_HANDLE && shadowFramebuffer != VK_NULL_HANDLE
        && shadowPipeline != VK_NULL_HANDLE && shadowPipelineLayout != VK_NULL_HANDLE) {
        const VkClearValue shadowClearValue{
            .depthStencil = {1.0f, 0},
        };
        const VkRenderPassBeginInfo shadowBeginInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = shadowRenderPass,
            .framebuffer = shadowFramebuffer,
            .renderArea = {
                .offset = {0, 0},
                .extent = shadowExtent,
            },
            .clearValueCount = 1,
            .pClearValues = &shadowClearValue,
        };
        const VkViewport shadowViewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(shadowExtent.width),
            .height = static_cast<float>(shadowExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        const VkRect2D shadowScissor{
            .offset = {0, 0},
            .extent = shadowExtent,
        };
        vkCmdBeginRenderPass(commandBuffer, &shadowBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);
        vkCmdSetViewport(commandBuffer, 0, 1, &shadowViewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &shadowScissor);
        vkCmdSetDepthBias(commandBuffer, 1.25f, 0.0f, 1.75f);
        vkCmdBindDescriptorSets(commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                shadowPipelineLayout,
                                0,
                                1,
                                &cameraDescriptorSet,
                                0,
                                nullptr);
        for (const NativeSceneDraw& draw : sceneData.draws) {
            // Alpha-cutout foliage is expensive in depth-only pass without alpha sampling;
            // skip it here to keep realtime shadows stable and performant.
            if (draw.alphaCutout) {
                continue;
            }
            if (draw.additiveBlend) {
                continue;
            }
            const auto meshIt = meshCache.find(draw.meshHandle);
            if (meshIt == meshCache.end()) {
                continue;
            }
            const CachedGpuMesh& mesh = meshIt->second;
            const VkDeviceSize vertexOffset = 0;
            const std::uint32_t firstIndex = std::min(draw.firstIndex, mesh.indexCount);
            const std::uint32_t availableIndexCount = mesh.indexCount - firstIndex;
            const std::uint32_t indexCount =
                draw.indexCount == 0 ? availableIndexCount : std::min(draw.indexCount, availableIndexCount);
            if (indexCount == 0) {
                continue;
            }
            NativeDrawPushConstants pushConstants{};
            std::copy(draw.model.begin(), draw.model.end(), std::begin(pushConstants.model));
            vkCmdPushConstants(commandBuffer,
                               shadowPipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0,
                               sizeof(NativeDrawPushConstants),
                               &pushConstants);
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mesh.vertexBuffer.buffer, &vertexOffset);
            vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, indexCount, std::max(draw.instanceCount, 1U), firstIndex, 0, 0);
        }
        vkCmdEndRenderPass(commandBuffer);
    }

    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    if (skyPipeline != VK_NULL_HANDLE && skyPipelineLayout != VK_NULL_HANDLE && skyDescriptorSet != VK_NULL_HANDLE
        && skyTextureDescriptorSet != VK_NULL_HANDLE && skyMesh.indexCount > 0U) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        const std::array<VkDescriptorSet, 2> skySets = {skyDescriptorSet, skyTextureDescriptorSet};
        vkCmdBindDescriptorSets(commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                skyPipelineLayout,
                                0,
                                static_cast<std::uint32_t>(skySets.size()),
                                skySets.data(),
                                0,
                                nullptr);
        const VkDeviceSize skyVertexOffset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &skyMesh.vertexBuffer.buffer, &skyVertexOffset);
        vkCmdBindIndexBuffer(commandBuffer, skyMesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, skyMesh.indexCount, 1, 0, 0, 0);
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipeline);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    bool sceneAdditivePipelineBound = false;
    for (const NativeSceneDraw& draw : sceneData.draws) {
        const auto meshIt = meshCache.find(draw.meshHandle);
        if (meshIt == meshCache.end()) {
            continue;
        }
        const CachedGpuMesh& mesh = meshIt->second;
        const VkDeviceSize vertexOffset = 0;
        const std::uint32_t firstIndex = std::min(draw.firstIndex, mesh.indexCount);
        const std::uint32_t availableIndexCount = mesh.indexCount - firstIndex;
        const std::uint32_t indexCount = draw.indexCount == 0 ? availableIndexCount : std::min(draw.indexCount, availableIndexCount);
        if (indexCount == 0) {
            continue;
        }

        if (scenePipelineAdditive != VK_NULL_HANDLE) {
            if (draw.additiveBlend != sceneAdditivePipelineBound) {
                vkCmdBindPipeline(commandBuffer,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  draw.additiveBlend ? scenePipelineAdditive : scenePipeline);
                sceneAdditivePipelineBound = draw.additiveBlend;
            }
        }

        VkDescriptorSet textureSet = textureCache.descriptorFor(scene, draw, sceneData.textureRoot);
        if (textureSet == VK_NULL_HANDLE) {
            textureSet = textureCache.whiteDescriptorSet;
        }
        const std::array<VkDescriptorSet, 3> bindSets = {cameraDescriptorSet, textureSet, shadowDescriptorSet};
        vkCmdBindDescriptorSets(commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout,
                                0,
                                static_cast<std::uint32_t>(bindSets.size()),
                                bindSets.data(),
                                0,
                                nullptr);

        NativeDrawPushConstants pushConstants{};
        std::copy(draw.model.begin(), draw.model.end(), std::begin(pushConstants.model));
        std::copy(draw.color.begin(), draw.color.end(), std::begin(pushConstants.color));
        pushConstants.tiling[0] = draw.textureTiling[0];
        pushConstants.tiling[1] = draw.textureTiling[1];
        pushConstants.useTexture = draw.useTexture ? 1 : 0;
        pushConstants.nativeWaterUvMotion = draw.nativeWaterUvMotion ? 1 : 0;
        pushConstants.nativeWaterTime = sceneData.sceneAnimationTimeSeconds;
        pushConstants.litShadingModel = (draw.litShadingModel ? 1 : 0) | (draw.alphaCutout ? 2 : 0);
        pushConstants.metallic = draw.metallic;
        pushConstants.roughness = draw.roughness;
        pushConstants.emissiveColor[0] = draw.emissiveColor[0];
        pushConstants.emissiveColor[1] = draw.emissiveColor[1];
        pushConstants.emissiveColor[2] = draw.emissiveColor[2];
        pushConstants.qualityTier = static_cast<float>(sceneData.renderQualityTier);
        vkCmdPushConstants(commandBuffer,
                           pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(NativeDrawPushConstants),
                           &pushConstants);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mesh.vertexBuffer.buffer, &vertexOffset);
        vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, indexCount, std::max(draw.instanceCount, 1U), firstIndex, 0, 0);
    }
    vkCmdEndRenderPass(commandBuffer);
    RecordHybridCompositeInCommandBuffer(commandBuffer,
                                         hybridCompositeRenderPass,
                                         hybridCompositeFramebuffer,
                                         extent,
                                         hybridCompositePipeline,
                                         hybridCompositePipelineLayout,
                                         cameraDescriptorSet,
                                         hybridCompositeHdrDescriptorSet);
    ExpectVk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
}

void RecordHybridCompositeInCommandBuffer(VkCommandBuffer commandBuffer,
                                          VkRenderPass compositeRenderPass,
                                          VkFramebuffer compositeFramebuffer,
                                          VkExtent2D extent,
                                          VkPipeline compositePipeline,
                                          VkPipelineLayout compositePipelineLayout,
                                          VkDescriptorSet cameraDescriptorSet,
                                          VkDescriptorSet hdrTextureDescriptorSet) {
    if (compositeFramebuffer == VK_NULL_HANDLE || compositeRenderPass == VK_NULL_HANDLE
        || compositePipeline == VK_NULL_HANDLE || compositePipelineLayout == VK_NULL_HANDLE
        || hdrTextureDescriptorSet == VK_NULL_HANDLE) {
        return;
    }
    const VkClearValue compositeClear{};
    const VkRenderPassBeginInfo compositeBegin{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = compositeRenderPass,
        .framebuffer = compositeFramebuffer,
        .renderArea = {
            .offset = {0, 0},
            .extent = extent,
        },
        .clearValueCount = 1,
        .pClearValues = &compositeClear,
    };
    const VkViewport compositeViewport{
        .x = 0.0f,
        .y = static_cast<float>(extent.height),
        .width = static_cast<float>(extent.width),
        .height = -static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    const VkRect2D compositeScissor{
        .offset = {0, 0},
        .extent = extent,
    };
    vkCmdBeginRenderPass(commandBuffer, &compositeBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline);
    vkCmdSetViewport(commandBuffer, 0, 1, &compositeViewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &compositeScissor);
    const std::array<VkDescriptorSet, 2> compositeSets = {cameraDescriptorSet, hdrTextureDescriptorSet};
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            compositePipelineLayout,
                            0,
                            static_cast<std::uint32_t>(compositeSets.size()),
                            compositeSets.data(),
                            0,
                            nullptr);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
}

} // namespace

bool RunVulkanNativeSceneLoop(const int width,
                              const int height,
                              const VulkanNativeSceneFrameCallback& buildFrame,
                              const VulkanPreviewWindowOptions& options,
                              std::string* error) {
#if RAWIRON_VULKAN_NATIVE_PREVIEW_ENABLED
    try {
        if (!buildFrame) {
            throw std::runtime_error("RunVulkanNativeSceneLoop requires a frame callback.");
        }

        ScopedWindowClass windowClass(&NativePreviewWindowProc);
        WindowState windowState{};
        windowState.messageUserData = options.messageUserData;
        windowState.onWin32Message = options.onWin32Message;

        RECT rect{0, 0, std::max(width, 1), std::max(height, 1)};
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
            throw std::runtime_error("CreateWindowExW failed for Vulkan native scene preview window.");
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
            .pApplicationName = "RawIron Vulkan Native Preview",
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
        VkPhysicalDeviceFeatures enabledFeatures{};
        enabledFeatures.samplerAnisotropy = VK_TRUE;
        const VkDeviceCreateInfo deviceInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = static_cast<std::uint32_t>(queueInfos.size()),
            .pQueueCreateInfos = queueInfos.data(),
            .enabledExtensionCount = 1,
            .ppEnabledExtensionNames = deviceExtensions,
            .pEnabledFeatures = &enabledFeatures,
        };

        VkDevice device = VK_NULL_HANDLE;
        ExpectVk(vkCreateDevice(selection.physicalDevice, &deviceInfo, nullptr, &device), "vkCreateDevice");

        VkPhysicalDeviceProperties physicalDeviceProperties{};
        vkGetPhysicalDeviceProperties(selection.physicalDevice, &physicalDeviceProperties);
        const float maxSamplerAnisotropy =
            std::min(16.0f, std::max(1.0f, physicalDeviceProperties.limits.maxSamplerAnisotropy));

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
        if (presentModeCount > 0U) {
            ExpectVk(vkGetPhysicalDeviceSurfacePresentModesKHR(selection.physicalDevice, surface, &presentModeCount, presentModes.data()),
                     "vkGetPhysicalDeviceSurfacePresentModesKHR(list)");
        }
        const VkPresentModeKHR presentMode = ChoosePresentMode(presentModes, options.presentModePreference);
        ri::core::LogInfo(std::string("Vulkan present mode: ") + PresentModeName(presentMode));

        VkExtent2D extent = capabilities.currentExtent;
        if (extent.width == std::numeric_limits<std::uint32_t>::max()) {
            extent.width = static_cast<std::uint32_t>(std::max(width, 1));
            extent.height = static_cast<std::uint32_t>(std::max(height, 1));
        }

        std::uint32_t imageCount = capabilities.minImageCount + 1U;
        if (capabilities.maxImageCount > 0U) {
            imageCount = std::min(imageCount, capabilities.maxImageCount);
        }

        if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0U) {
            throw std::runtime_error("Swapchain images do not support color attachments on this device.");
        }

        VkSwapchainCreateInfoKHR swapchainInfo{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface,
            .minImageCount = imageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
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

        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        ExpectVk(vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain), "vkCreateSwapchainKHR");

        std::uint32_t swapchainImageCount = 0;
        ExpectVk(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr), "vkGetSwapchainImagesKHR(count)");
        std::vector<VkImage> swapchainImages(swapchainImageCount);
        ExpectVk(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data()), "vkGetSwapchainImagesKHR(list)");

        std::vector<VkImageView> swapchainImageViews;
        swapchainImageViews.reserve(swapchainImages.size());
        for (VkImage image : swapchainImages) {
            const VkImageViewCreateInfo imageViewInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = surfaceFormat.format,
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            VkImageView view = VK_NULL_HANDLE;
            ExpectVk(vkCreateImageView(device, &imageViewInfo, nullptr, &view), "vkCreateImageView(color)");
            swapchainImageViews.push_back(view);
        }

        const VkFormat depthFormat = FindDepthFormat(selection.physicalDevice);
        const ImageResource depthImage = CreateDepthImage(selection.physicalDevice, device, depthFormat, extent.width, extent.height);
        const VkFormat shadowDepthFormat = FindShadowDepthFormat(selection.physicalDevice);
        constexpr std::uint32_t kShadowMapResolution = 2048U;
        ImageResource shadowDepthImage = CreateDepthImage(
            selection.physicalDevice,
            device,
            shadowDepthFormat,
            kShadowMapResolution,
            kShadowMapResolution,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        const VkAttachmentDescription colorAttachment{
            .format = surfaceFormat.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        };
        const VkAttachmentDescription depthAttachment{
            .format = depthFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        const VkAttachmentReference colorReference{
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        const VkAttachmentReference depthReference{
            .attachment = 1,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        const VkSubpassDescription subpass{
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorReference,
            .pDepthStencilAttachment = &depthReference,
        };
        const VkSubpassDependency dependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        };
        const std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        const VkRenderPassCreateInfo renderPassInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = static_cast<std::uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency,
        };
        VkRenderPass renderPass = VK_NULL_HANDLE;
        ExpectVk(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass), "vkCreateRenderPass");

        const VkAttachmentDescription shadowDepthAttachment{
            .format = shadowDepthFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        const VkAttachmentReference shadowDepthReference{
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        const VkSubpassDescription shadowSubpass{
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .pDepthStencilAttachment = &shadowDepthReference,
        };
        const std::array<VkSubpassDependency, 2> shadowDependencies = {{
            VkSubpassDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            },
            VkSubpassDependency{
                .srcSubpass = 0,
                .dstSubpass = VK_SUBPASS_EXTERNAL,
                .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            },
        }};
        const VkRenderPassCreateInfo shadowRenderPassInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &shadowDepthAttachment,
            .subpassCount = 1,
            .pSubpasses = &shadowSubpass,
            .dependencyCount = static_cast<std::uint32_t>(shadowDependencies.size()),
            .pDependencies = shadowDependencies.data(),
        };
        VkRenderPass shadowRenderPass = VK_NULL_HANDLE;
        ExpectVk(vkCreateRenderPass(device, &shadowRenderPassInfo, nullptr, &shadowRenderPass),
                 "vkCreateRenderPass(shadow)");
        const VkFramebufferCreateInfo shadowFramebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = shadowRenderPass,
            .attachmentCount = 1,
            .pAttachments = &shadowDepthImage.view,
            .width = kShadowMapResolution,
            .height = kShadowMapResolution,
            .layers = 1,
        };
        VkFramebuffer shadowFramebuffer = VK_NULL_HANDLE;
        ExpectVk(vkCreateFramebuffer(device, &shadowFramebufferInfo, nullptr, &shadowFramebuffer),
                 "vkCreateFramebuffer(shadow)");

        std::vector<VkFramebuffer> framebuffers;
        framebuffers.reserve(swapchainImageViews.size());
        for (VkImageView imageView : swapchainImageViews) {
            const std::array<VkImageView, 2> attachmentsForFramebuffer = {imageView, depthImage.view};
            const VkFramebufferCreateInfo framebufferInfo{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = renderPass,
                .attachmentCount = static_cast<std::uint32_t>(attachmentsForFramebuffer.size()),
                .pAttachments = attachmentsForFramebuffer.data(),
                .width = extent.width,
                .height = extent.height,
                .layers = 1,
            };
            VkFramebuffer framebuffer = VK_NULL_HANDLE;
            ExpectVk(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer), "vkCreateFramebuffer");
            framebuffers.push_back(framebuffer);
        }

        const VkDescriptorSetLayoutBinding cameraBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        };
        const VkDescriptorSetLayoutCreateInfo cameraSetLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &cameraBinding,
        };
        VkDescriptorSetLayout cameraSetLayout = VK_NULL_HANDLE;
        ExpectVk(vkCreateDescriptorSetLayout(device, &cameraSetLayoutInfo, nullptr, &cameraSetLayout),
                 "vkCreateDescriptorSetLayout(camera)");

        const VkDescriptorSetLayoutBinding textureBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        };
        const VkDescriptorSetLayoutCreateInfo textureSetLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &textureBinding,
        };
        VkDescriptorSetLayout textureSetLayout = VK_NULL_HANDLE;
        ExpectVk(vkCreateDescriptorSetLayout(device, &textureSetLayoutInfo, nullptr, &textureSetLayout),
                 "vkCreateDescriptorSetLayout(texture)");

        const VkDescriptorSetLayoutBinding shadowBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        };
        const VkDescriptorSetLayoutCreateInfo shadowSetLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &shadowBinding,
        };
        VkDescriptorSetLayout shadowSetLayout = VK_NULL_HANDLE;
        ExpectVk(vkCreateDescriptorSetLayout(device, &shadowSetLayoutInfo, nullptr, &shadowSetLayout),
                 "vkCreateDescriptorSetLayout(shadow)");

        const VkDescriptorSetLayoutBinding skyUniformBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        };
        const VkDescriptorSetLayoutCreateInfo skyCameraSetLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &skyUniformBinding,
        };
        VkDescriptorSetLayout skyCameraSetLayout = VK_NULL_HANDLE;
        ExpectVk(vkCreateDescriptorSetLayout(device, &skyCameraSetLayoutInfo, nullptr, &skyCameraSetLayout),
                 "vkCreateDescriptorSetLayout(sky-camera)");

        const bool enableHybridHdr = options.enableHybridHdrPresentation;
        VkFormat hdrSceneFormat = VK_FORMAT_UNDEFINED;
        ImageResource hdrSceneColorImage{};
        VkRenderPass hdrSceneRenderPass = VK_NULL_HANDLE;
        VkFramebuffer hdrSceneFramebuffer = VK_NULL_HANDLE;
        VkRenderPass compositeRenderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> compositeFramebuffers;
        VkDescriptorSetLayout compositeHdrTextureSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout compositePipelineLayout = VK_NULL_HANDLE;
        VkShaderModule compositeVertShader = VK_NULL_HANDLE;
        VkShaderModule compositeFragShader = VK_NULL_HANDLE;
        VkPipeline pipelineHdrScene = VK_NULL_HANDLE;
        VkPipeline pipelineHdrSceneAdditive = VK_NULL_HANDLE;
        VkPipeline skyPipelineHdr = VK_NULL_HANDLE;
        VkPipeline compositePipeline = VK_NULL_HANDLE;
        VkDescriptorPool compositeDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet compositeHdrDescriptorSet = VK_NULL_HANDLE;

        if (enableHybridHdr) {
            hdrSceneFormat = FindHdrSceneColorFormat(selection.physicalDevice);
            hdrSceneColorImage =
                CreateHdrSceneColorImage(selection.physicalDevice, device, hdrSceneFormat, extent.width, extent.height);

            const VkAttachmentDescription hdrSceneColorAttachmentDesc{
                .format = hdrSceneFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            const std::array<VkAttachmentDescription, 2> hdrSceneAttachmentDescs = {hdrSceneColorAttachmentDesc,
                                                                                     depthAttachment};
            const VkSubpassDependency hdrSceneToSampleDependency{
                .srcSubpass = 0,
                .dstSubpass = VK_SUBPASS_EXTERNAL,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            };
            const std::array<VkSubpassDependency, 2> hdrSceneDependencies = {dependency, hdrSceneToSampleDependency};
            const VkRenderPassCreateInfo hdrSceneRenderPassInfo{
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .attachmentCount = static_cast<std::uint32_t>(hdrSceneAttachmentDescs.size()),
                .pAttachments = hdrSceneAttachmentDescs.data(),
                .subpassCount = 1,
                .pSubpasses = &subpass,
                .dependencyCount = static_cast<std::uint32_t>(hdrSceneDependencies.size()),
                .pDependencies = hdrSceneDependencies.data(),
            };
            ExpectVk(vkCreateRenderPass(device, &hdrSceneRenderPassInfo, nullptr, &hdrSceneRenderPass),
                     "vkCreateRenderPass(hdr-scene)");

            const std::array<VkImageView, 2> hdrSceneFbAttachments = {hdrSceneColorImage.view, depthImage.view};
            const VkFramebufferCreateInfo hdrSceneFramebufferInfo{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = hdrSceneRenderPass,
                .attachmentCount = static_cast<std::uint32_t>(hdrSceneFbAttachments.size()),
                .pAttachments = hdrSceneFbAttachments.data(),
                .width = extent.width,
                .height = extent.height,
                .layers = 1,
            };
            ExpectVk(vkCreateFramebuffer(device, &hdrSceneFramebufferInfo, nullptr, &hdrSceneFramebuffer),
                     "vkCreateFramebuffer(hdr-scene)");

            const VkAttachmentDescription compositeSwapAttachmentDesc{
                .format = surfaceFormat.format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            };
            const VkAttachmentReference compositeSwapColorReference{
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };
            const VkSubpassDescription compositeSubpassDesc{
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = 1,
                .pColorAttachments = &compositeSwapColorReference,
            };
            const VkSubpassDependency compositePassDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            };
            const VkRenderPassCreateInfo compositeRenderPassInfo{
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments = &compositeSwapAttachmentDesc,
                .subpassCount = 1,
                .pSubpasses = &compositeSubpassDesc,
                .dependencyCount = 1,
                .pDependencies = &compositePassDependency,
            };
            ExpectVk(vkCreateRenderPass(device, &compositeRenderPassInfo, nullptr, &compositeRenderPass),
                     "vkCreateRenderPass(composite)");
            compositeFramebuffers.reserve(swapchainImageViews.size());
            for (VkImageView swapColorView : swapchainImageViews) {
                const VkFramebufferCreateInfo compositeFbInfo{
                    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                    .renderPass = compositeRenderPass,
                    .attachmentCount = 1,
                    .pAttachments = &swapColorView,
                    .width = extent.width,
                    .height = extent.height,
                    .layers = 1,
                };
                VkFramebuffer compositeFb = VK_NULL_HANDLE;
                ExpectVk(vkCreateFramebuffer(device, &compositeFbInfo, nullptr, &compositeFb), "vkCreateFramebuffer(composite)");
                compositeFramebuffers.push_back(compositeFb);
            }

            const VkDescriptorSetLayoutBinding compositeHdrSamplerBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            };
            const VkDescriptorSetLayoutCreateInfo compositeHdrSamplerLayoutInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = 1,
                .pBindings = &compositeHdrSamplerBinding,
            };
            ExpectVk(vkCreateDescriptorSetLayout(device, &compositeHdrSamplerLayoutInfo, nullptr, &compositeHdrTextureSetLayout),
                     "vkCreateDescriptorSetLayout(composite-hdr)");

            const std::array<VkDescriptorSetLayout, 2> compositePipelineSetLayouts = {cameraSetLayout, compositeHdrTextureSetLayout};
            const VkPipelineLayoutCreateInfo compositePipelineLayoutInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = static_cast<std::uint32_t>(compositePipelineSetLayouts.size()),
                .pSetLayouts = compositePipelineSetLayouts.data(),
                .pushConstantRangeCount = 0,
                .pPushConstantRanges = nullptr,
            };
            ExpectVk(vkCreatePipelineLayout(device, &compositePipelineLayoutInfo, nullptr, &compositePipelineLayout),
                     "vkCreatePipelineLayout(composite)");
        }

        const std::array<VkDescriptorSetLayout, 3> pipelineSetLayouts = {cameraSetLayout, textureSetLayout, shadowSetLayout};

        const VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(NativeDrawPushConstants),
        };
        const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = static_cast<std::uint32_t>(pipelineSetLayouts.size()),
            .pSetLayouts = pipelineSetLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange,
        };
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        ExpectVk(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout");

        const std::array<VkDescriptorSetLayout, 2> skyPipelineSetLayouts = {skyCameraSetLayout, textureSetLayout};
        const VkPipelineLayoutCreateInfo skyPipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = static_cast<std::uint32_t>(skyPipelineSetLayouts.size()),
            .pSetLayouts = skyPipelineSetLayouts.data(),
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
        };
        VkPipelineLayout skyPipelineLayout = VK_NULL_HANDLE;
        ExpectVk(vkCreatePipelineLayout(device, &skyPipelineLayoutInfo, nullptr, &skyPipelineLayout),
                 "vkCreatePipelineLayout(sky)");

        const std::array<VkDescriptorSetLayout, 1> shadowPipelineSetLayouts = {cameraSetLayout};
        const VkPushConstantRange shadowPushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(NativeDrawPushConstants),
        };
        const VkPipelineLayoutCreateInfo shadowPipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = static_cast<std::uint32_t>(shadowPipelineSetLayouts.size()),
            .pSetLayouts = shadowPipelineSetLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &shadowPushConstantRange,
        };
        VkPipelineLayout shadowPipelineLayout = VK_NULL_HANDLE;
        ExpectVk(vkCreatePipelineLayout(device, &shadowPipelineLayoutInfo, nullptr, &shadowPipelineLayout),
                 "vkCreatePipelineLayout(shadow)");

        const fs::path shaderDir = fs::path(RAWIRON_VULKAN_SHADER_DIR);
        const VkShaderModule vertShader = CreateShaderModule(device, shaderDir / "NativeScenePreview.vert.spv");
        const VkShaderModule fragShader = CreateShaderModule(device, shaderDir / "NativeScenePreview.frag.spv");
        const VkShaderModule skyVertShader = CreateShaderModule(device, shaderDir / "NativeSkybox.vert.spv");
        const VkShaderModule skyFragShader = CreateShaderModule(device, shaderDir / "NativeSkybox.frag.spv");
        const VkShaderModule shadowVertShader = CreateShaderModule(device, shaderDir / "NativeShadowDepth.vert.spv");
        if (enableHybridHdr) {
            compositeVertShader = CreateShaderModule(device, shaderDir / "NativeComposite.vert.spv");
            compositeFragShader = CreateShaderModule(device, shaderDir / "NativeComposite.frag.spv");
        }

        const VkPipelineShaderStageCreateInfo vertStage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShader,
            .pName = "main",
        };
        const VkPipelineShaderStageCreateInfo fragStage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShader,
            .pName = "main",
        };
        const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {vertStage, fragStage};

        const VkVertexInputBindingDescription vertexBinding{
            .binding = 0,
            .stride = static_cast<std::uint32_t>(sizeof(NativeSceneVertex)),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
        const std::array<VkVertexInputAttributeDescription, 3> vertexAttributes = {{
            VkVertexInputAttributeDescription{
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = static_cast<std::uint32_t>(offsetof(NativeSceneVertex, position)),
            },
            VkVertexInputAttributeDescription{
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = static_cast<std::uint32_t>(offsetof(NativeSceneVertex, normal)),
            },
            VkVertexInputAttributeDescription{
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = static_cast<std::uint32_t>(offsetof(NativeSceneVertex, uv)),
            },
        }};
        const VkPipelineVertexInputStateCreateInfo vertexInputInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &vertexBinding,
            .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(vertexAttributes.size()),
            .pVertexAttributeDescriptions = vertexAttributes.data(),
        };
        const VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        };
        const VkPipelineViewportStateCreateInfo viewportStateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };
        const VkPipelineRasterizationStateCreateInfo rasterizationInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.0f,
        };
        const VkPipelineMultisampleStateCreateInfo multisampleInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };
        const VkPipelineDepthStencilStateCreateInfo depthStencilInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        };
        const VkPipelineColorBlendAttachmentState blendAttachment{
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
        const VkPipelineColorBlendStateCreateInfo colorBlendInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &blendAttachment,
        };
        const std::array<VkDynamicState, 2> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const VkPipelineDynamicStateCreateInfo dynamicStateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data(),
        };
        const VkGraphicsPipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = static_cast<std::uint32_t>(shaderStages.size()),
            .pStages = shaderStages.data(),
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssemblyInfo,
            .pViewportState = &viewportStateInfo,
            .pRasterizationState = &rasterizationInfo,
            .pMultisampleState = &multisampleInfo,
            .pDepthStencilState = &depthStencilInfo,
            .pColorBlendState = &colorBlendInfo,
            .pDynamicState = &dynamicStateInfo,
            .layout = pipelineLayout,
            .renderPass = renderPass,
            .subpass = 0,
        };
        VkPipeline pipeline = VK_NULL_HANDLE;
        ExpectVk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline),
                 "vkCreateGraphicsPipelines");

        VkPipelineDepthStencilStateCreateInfo depthStencilAdditiveInfo = depthStencilInfo;
        depthStencilAdditiveInfo.depthWriteEnable = VK_FALSE;
        const VkPipelineColorBlendAttachmentState blendAttachmentAdditive{
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
        const VkPipelineColorBlendStateCreateInfo colorBlendAdditiveInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &blendAttachmentAdditive,
        };
        VkGraphicsPipelineCreateInfo pipelineAdditiveInfo = pipelineInfo;
        pipelineAdditiveInfo.pDepthStencilState = &depthStencilAdditiveInfo;
        pipelineAdditiveInfo.pColorBlendState = &colorBlendAdditiveInfo;
        VkPipeline pipelineAdditive = VK_NULL_HANDLE;
        ExpectVk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineAdditiveInfo, nullptr, &pipelineAdditive),
                 "vkCreateGraphicsPipelines(scene-additive)");

        const VkPipelineShaderStageCreateInfo skyVertStage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = skyVertShader,
            .pName = "main",
        };
        const VkPipelineShaderStageCreateInfo skyFragStage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = skyFragShader,
            .pName = "main",
        };
        const std::array<VkPipelineShaderStageCreateInfo, 2> skyShaderStages = {skyVertStage, skyFragStage};
        const VkPipelineRasterizationStateCreateInfo skyRasterizationInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_FRONT_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.0f,
        };
        const VkPipelineDepthStencilStateCreateInfo skyDepthStencilInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_FALSE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        };
        const VkPipelineColorBlendAttachmentState skyBlendAttachment{
            .blendEnable = VK_FALSE,
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
        const VkPipelineColorBlendStateCreateInfo skyColorBlendInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &skyBlendAttachment,
        };
        const VkGraphicsPipelineCreateInfo skyPipelineInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = static_cast<std::uint32_t>(skyShaderStages.size()),
            .pStages = skyShaderStages.data(),
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssemblyInfo,
            .pViewportState = &viewportStateInfo,
            .pRasterizationState = &skyRasterizationInfo,
            .pMultisampleState = &multisampleInfo,
            .pDepthStencilState = &skyDepthStencilInfo,
            .pColorBlendState = &skyColorBlendInfo,
            .pDynamicState = &dynamicStateInfo,
            .layout = skyPipelineLayout,
            .renderPass = renderPass,
            .subpass = 0,
        };
        VkPipeline skyPipeline = VK_NULL_HANDLE;
        ExpectVk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &skyPipelineInfo, nullptr, &skyPipeline),
                 "vkCreateGraphicsPipelines(sky)");

        if (enableHybridHdr) {
            VkGraphicsPipelineCreateInfo hdrPipelineInfo = pipelineInfo;
            hdrPipelineInfo.renderPass = hdrSceneRenderPass;
            ExpectVk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &hdrPipelineInfo, nullptr, &pipelineHdrScene),
                     "vkCreateGraphicsPipelines(hdr-scene)");

            VkGraphicsPipelineCreateInfo hdrAdditiveInfo = pipelineAdditiveInfo;
            hdrAdditiveInfo.renderPass = hdrSceneRenderPass;
            ExpectVk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &hdrAdditiveInfo, nullptr, &pipelineHdrSceneAdditive),
                     "vkCreateGraphicsPipelines(hdr-scene-additive)");

            VkGraphicsPipelineCreateInfo skyHdrPipelineInfo = skyPipelineInfo;
            skyHdrPipelineInfo.renderPass = hdrSceneRenderPass;
            ExpectVk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &skyHdrPipelineInfo, nullptr, &skyPipelineHdr),
                     "vkCreateGraphicsPipelines(sky-hdr)");

            const VkPipelineShaderStageCreateInfo compositeVertStage{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = compositeVertShader,
                .pName = "main",
            };
            const VkPipelineShaderStageCreateInfo compositeFragStage{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = compositeFragShader,
                .pName = "main",
            };
            const std::array<VkPipelineShaderStageCreateInfo, 2> compositeStages = {compositeVertStage, compositeFragStage};
            const VkPipelineVertexInputStateCreateInfo compositeVertexInputInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 0,
                .pVertexBindingDescriptions = nullptr,
                .vertexAttributeDescriptionCount = 0,
                .pVertexAttributeDescriptions = nullptr,
            };
            const VkPipelineRasterizationStateCreateInfo compositeRasterInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .lineWidth = 1.0f,
            };
            const VkPipelineDepthStencilStateCreateInfo compositeDepthStencilInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                .depthTestEnable = VK_FALSE,
                .depthWriteEnable = VK_FALSE,
            };
            const VkPipelineColorBlendAttachmentState compositeBlendAttachment{
                .blendEnable = VK_FALSE,
                .colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            };
            const VkPipelineColorBlendStateCreateInfo compositeColorBlendInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments = &compositeBlendAttachment,
            };
            const VkGraphicsPipelineCreateInfo compositePipelineInfo{
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .stageCount = static_cast<std::uint32_t>(compositeStages.size()),
                .pStages = compositeStages.data(),
                .pVertexInputState = &compositeVertexInputInfo,
                .pInputAssemblyState = &inputAssemblyInfo,
                .pViewportState = &viewportStateInfo,
                .pRasterizationState = &compositeRasterInfo,
                .pMultisampleState = &multisampleInfo,
                .pDepthStencilState = &compositeDepthStencilInfo,
                .pColorBlendState = &compositeColorBlendInfo,
                .pDynamicState = &dynamicStateInfo,
                .layout = compositePipelineLayout,
                .renderPass = compositeRenderPass,
                .subpass = 0,
            };
            ExpectVk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &compositePipelineInfo, nullptr, &compositePipeline),
                     "vkCreateGraphicsPipelines(composite)");
        }

        const VkPipelineShaderStageCreateInfo shadowVertStage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = shadowVertShader,
            .pName = "main",
        };
        const VkPipelineRasterizationStateCreateInfo shadowRasterizationInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_TRUE,
            .lineWidth = 1.0f,
        };
        const VkPipelineColorBlendStateCreateInfo shadowColorBlendInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 0,
            .pAttachments = nullptr,
        };
        const VkPipelineDepthStencilStateCreateInfo shadowDepthStencilInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        };
        const std::array<VkDynamicState, 3> shadowDynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_DEPTH_BIAS,
        };
        const VkPipelineDynamicStateCreateInfo shadowDynamicStateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = static_cast<std::uint32_t>(shadowDynamicStates.size()),
            .pDynamicStates = shadowDynamicStates.data(),
        };
        const VkGraphicsPipelineCreateInfo shadowPipelineInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 1,
            .pStages = &shadowVertStage,
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssemblyInfo,
            .pViewportState = &viewportStateInfo,
            .pRasterizationState = &shadowRasterizationInfo,
            .pMultisampleState = &multisampleInfo,
            .pDepthStencilState = &shadowDepthStencilInfo,
            .pColorBlendState = &shadowColorBlendInfo,
            .pDynamicState = &shadowDynamicStateInfo,
            .layout = shadowPipelineLayout,
            .renderPass = shadowRenderPass,
            .subpass = 0,
        };
        VkPipeline shadowPipeline = VK_NULL_HANDLE;
        ExpectVk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &shadowPipelineInfo, nullptr, &shadowPipeline),
                 "vkCreateGraphicsPipelines(shadow)");

        BufferResource uniformBuffer{};
        VkDeviceSize uniformBufferCapacity = 0;
        void* mappedUniformMemory = nullptr;
        constexpr VkDeviceSize kSceneCameraUniformBytes = sizeof(CameraUniformStd140);
        EnsureMappedBufferCapacity(selection.physicalDevice,
                                   device,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   kSceneCameraUniformBytes,
                                   uniformBuffer,
                                   uniformBufferCapacity,
                                   mappedUniformMemory,
                                   "vkMapMemory(uniform)");

        std::unordered_map<int, CachedGpuMesh> meshCache{};
        const ri::scene::Scene* cachedScene = nullptr;

        const VkDescriptorPoolSize cameraPoolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
        };
        const VkDescriptorPoolCreateInfo cameraPoolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &cameraPoolSize,
        };
        VkDescriptorPool cameraDescriptorPool = VK_NULL_HANDLE;
        ExpectVk(vkCreateDescriptorPool(device, &cameraPoolInfo, nullptr, &cameraDescriptorPool), "vkCreateDescriptorPool(camera)");

        const VkDescriptorSetAllocateInfo cameraSetAllocateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = cameraDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &cameraSetLayout,
        };
        VkDescriptorSet cameraDescriptorSet = VK_NULL_HANDLE;
        ExpectVk(vkAllocateDescriptorSets(device, &cameraSetAllocateInfo, &cameraDescriptorSet), "vkAllocateDescriptorSets(camera)");

        const VkDescriptorBufferInfo descriptorBufferInfo{
            .buffer = uniformBuffer.buffer,
            .offset = 0,
            .range = kSceneCameraUniformBytes,
        };
        const VkWriteDescriptorSet writeCamera{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = cameraDescriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &descriptorBufferInfo,
        };
        vkUpdateDescriptorSets(device, 1, &writeCamera, 0, nullptr);

        BufferResource skyUniformBuffer{};
        VkDeviceSize skyUniformBufferCapacity = 0;
        void* mappedSkyUniformMemory = nullptr;
        EnsureMappedBufferCapacity(selection.physicalDevice,
                                   device,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   sizeof(SkyUniformStd140),
                                   skyUniformBuffer,
                                   skyUniformBufferCapacity,
                                   mappedSkyUniformMemory,
                                   "vkMapMemory(sky-uniform)");

        const VkDescriptorPoolSize skyCameraPoolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
        };
        const VkDescriptorPoolCreateInfo skyCameraPoolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &skyCameraPoolSize,
        };
        VkDescriptorPool skyDescriptorPool = VK_NULL_HANDLE;
        ExpectVk(vkCreateDescriptorPool(device, &skyCameraPoolInfo, nullptr, &skyDescriptorPool),
                 "vkCreateDescriptorPool(sky-camera)");

        const VkDescriptorSetAllocateInfo skySetAllocateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = skyDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &skyCameraSetLayout,
        };
        VkDescriptorSet skyDescriptorSet = VK_NULL_HANDLE;
        ExpectVk(vkAllocateDescriptorSets(device, &skySetAllocateInfo, &skyDescriptorSet), "vkAllocateDescriptorSets(sky-camera)");

        const VkDescriptorBufferInfo skyDescriptorBufferInfo{
            .buffer = skyUniformBuffer.buffer,
            .offset = 0,
            .range = sizeof(SkyUniformStd140),
        };
        const VkWriteDescriptorSet writeSkyCamera{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = skyDescriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &skyDescriptorBufferInfo,
        };
        vkUpdateDescriptorSets(device, 1, &writeSkyCamera, 0, nullptr);

        const VkCommandPoolCreateInfo commandPoolInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = selection.graphicsQueueFamily,
        };
        VkCommandPool commandPool = VK_NULL_HANDLE;
        ExpectVk(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool), "vkCreateCommandPool");

        const VkSamplerCreateInfo samplerInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .anisotropyEnable = maxSamplerAnisotropy > 1.0f ? VK_TRUE : VK_FALSE,
            .maxAnisotropy = maxSamplerAnisotropy,
        };
        VkSampler linearSampler = VK_NULL_HANDLE;
        ExpectVk(vkCreateSampler(device, &samplerInfo, nullptr, &linearSampler), "vkCreateSampler(albedo)");

        if (enableHybridHdr) {
            const VkDescriptorPoolSize compositePoolSize{
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
            };
            const VkDescriptorPoolCreateInfo compositePoolInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .maxSets = 1,
                .poolSizeCount = 1,
                .pPoolSizes = &compositePoolSize,
            };
            ExpectVk(vkCreateDescriptorPool(device, &compositePoolInfo, nullptr, &compositeDescriptorPool),
                     "vkCreateDescriptorPool(composite-hdr)");
            const VkDescriptorSetAllocateInfo compositeAllocateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = compositeDescriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &compositeHdrTextureSetLayout,
            };
            ExpectVk(vkAllocateDescriptorSets(device, &compositeAllocateInfo, &compositeHdrDescriptorSet),
                     "vkAllocateDescriptorSets(composite-hdr)");
            const VkDescriptorImageInfo hdrSceneImageInfo{
                .sampler = linearSampler,
                .imageView = hdrSceneColorImage.view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            const VkWriteDescriptorSet writeHdrScene{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = compositeHdrDescriptorSet,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &hdrSceneImageInfo,
            };
            vkUpdateDescriptorSets(device, 1, &writeHdrScene, 0, nullptr);
        }

        VkSamplerCreateInfo shadowSamplerInfo{};
        shadowSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        shadowSamplerInfo.magFilter = VK_FILTER_LINEAR;
        shadowSamplerInfo.minFilter = VK_FILTER_LINEAR;
        shadowSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        shadowSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        shadowSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        shadowSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        shadowSamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        shadowSamplerInfo.compareEnable = VK_FALSE;
        shadowSamplerInfo.maxLod = 0.0f;
        VkSampler shadowSampler = VK_NULL_HANDLE;
        ExpectVk(vkCreateSampler(device, &shadowSamplerInfo, nullptr, &shadowSampler), "vkCreateSampler(shadow)");

        const VkDescriptorPoolSize shadowPoolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
        };
        const VkDescriptorPoolCreateInfo shadowPoolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &shadowPoolSize,
        };
        VkDescriptorPool shadowDescriptorPool = VK_NULL_HANDLE;
        ExpectVk(vkCreateDescriptorPool(device, &shadowPoolInfo, nullptr, &shadowDescriptorPool),
                 "vkCreateDescriptorPool(shadow)");
        const VkDescriptorSetAllocateInfo shadowSetAllocateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = shadowDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &shadowSetLayout,
        };
        VkDescriptorSet shadowDescriptorSet = VK_NULL_HANDLE;
        ExpectVk(vkAllocateDescriptorSets(device, &shadowSetAllocateInfo, &shadowDescriptorSet),
                 "vkAllocateDescriptorSets(shadow)");
        const VkDescriptorImageInfo shadowDescriptorImageInfo{
            .sampler = shadowSampler,
            .imageView = shadowDepthImage.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        const VkWriteDescriptorSet writeShadow{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = shadowDescriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &shadowDescriptorImageInfo,
        };
        vkUpdateDescriptorSets(device, 1, &writeShadow, 0, nullptr);

        constexpr std::uint32_t kMaxTextureDescriptors = 512U;
        const VkDescriptorPoolSize texturePoolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = kMaxTextureDescriptors,
        };
        const VkDescriptorPoolCreateInfo texturePoolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = kMaxTextureDescriptors,
            .poolSizeCount = 1,
            .pPoolSizes = &texturePoolSize,
        };
        VkDescriptorPool textureDescriptorPool = VK_NULL_HANDLE;
        ExpectVk(vkCreateDescriptorPool(device, &texturePoolInfo, nullptr, &textureDescriptorPool),
                 "vkCreateDescriptorPool(texture)");

        NativeAlbedoTextureCache textureCache{};
        textureCache.initialize(
            selection.physicalDevice, device, commandPool, graphicsQueue, textureSetLayout, textureDescriptorPool, linearSampler);

        CachedGpuMesh skyMesh = CreateStaticUnitCubeGpuMesh(selection.physicalDevice, device);

        std::vector<VkCommandBuffer> commandBuffers(framebuffers.size(), VK_NULL_HANDLE);
        const VkCommandBufferAllocateInfo commandBufferInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = static_cast<std::uint32_t>(commandBuffers.size()),
        };
        ExpectVk(vkAllocateCommandBuffers(device, &commandBufferInfo, commandBuffers.data()), "vkAllocateCommandBuffers");

        constexpr std::uint32_t kFramesInFlight = 2U;
        const VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        const VkFenceCreateInfo fenceInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        std::array<VkSemaphore, kFramesInFlight> imageAvailable{};
        std::array<VkSemaphore, kFramesInFlight> renderFinished{};
        std::array<VkFence, kFramesInFlight> inFlightFences{};
        for (std::uint32_t frameIndex = 0; frameIndex < kFramesInFlight; ++frameIndex) {
            ExpectVk(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailable[frameIndex]),
                     "vkCreateSemaphore(imageAvailable)");
            ExpectVk(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinished[frameIndex]),
                     "vkCreateSemaphore(renderFinished)");
            ExpectVk(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[frameIndex]), "vkCreateFence");
        }
        std::vector<VkFence> imageInFlight(commandBuffers.size(), VK_NULL_HANDLE);
        std::uint32_t currentFrame = 0U;

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

            VulkanNativeSceneFrame frame{};
            std::string frameError;
            if (!buildFrame(frame, &frameError)) {
                throw std::runtime_error(frameError.empty() ? "Native Vulkan frame callback failed." : frameError);
            }
            if (frame.textureRoot.empty() && !options.textureRoot.empty()) {
                frame.textureRoot = options.textureRoot;
            }

            NativeScenePreviewData sceneData{};
            if (!BuildNativeScenePreviewData(frame, width, height, &sceneData, &frameError)) {
                throw std::runtime_error(frameError.empty() ? "Failed to build native Vulkan scene data." : frameError);
            }
            if (options.enableHybridHdrPresentation) {
                sceneData.postProcessSecondary[3] = 1.0f;
            }
            if (sceneData.scene == nullptr) {
                throw std::runtime_error("Native Vulkan scene data did not include a scene pointer.");
            }
            if (cachedScene != sceneData.scene) {
                ClearGpuMeshCache(device, meshCache);
                cachedScene = sceneData.scene;
            }
            for (const NativeSceneDraw& draw : sceneData.draws) {
                if (draw.meshHandle >= 0) {
                    EnsureGpuMeshCached(selection.physicalDevice, device, *sceneData.scene, draw.meshHandle, meshCache);
                }
            }

            CameraUniformStd140 cameraUniform{};
            std::memcpy(cameraUniform.viewProjection, sceneData.viewProjection.data(), sizeof(cameraUniform.viewProjection));
            std::memcpy(cameraUniform.cameraWorldPosition,
                        sceneData.cameraWorldPosition.data(),
                        sizeof(cameraUniform.cameraWorldPosition));
            std::memcpy(cameraUniform.renderTuning, sceneData.renderTuning.data(), sizeof(cameraUniform.renderTuning));
            std::memcpy(cameraUniform.postProcessPrimary,
                        sceneData.postProcessPrimary.data(),
                        sizeof(cameraUniform.postProcessPrimary));
            std::memcpy(cameraUniform.postProcessTint, sceneData.postProcessTint.data(), sizeof(cameraUniform.postProcessTint));
            std::memcpy(cameraUniform.postProcessSecondary,
                        sceneData.postProcessSecondary.data(),
                        sizeof(cameraUniform.postProcessSecondary));
            std::memcpy(cameraUniform.lightViewProjection,
                        sceneData.lightViewProjection.data(),
                        sizeof(cameraUniform.lightViewProjection));
            std::memcpy(cameraUniform.lightDirectionIntensity,
                        sceneData.lightDirectionIntensity.data(),
                        sizeof(cameraUniform.lightDirectionIntensity));
            std::memcpy(cameraUniform.localLightPositionRange,
                        sceneData.localLightPositionRange.data(),
                        sizeof(cameraUniform.localLightPositionRange));
            std::memcpy(cameraUniform.localLightColorIntensity,
                        sceneData.localLightColorIntensity.data(),
                        sizeof(cameraUniform.localLightColorIntensity));
            std::memcpy(cameraUniform.viewportMetrics, sceneData.viewportMetrics.data(), sizeof(cameraUniform.viewportMetrics));
            std::memcpy(mappedUniformMemory, &cameraUniform, sizeof(CameraUniformStd140));
            SkyUniformStd140 skyUniform{};
            skyUniform.hasSkyTexture = sceneData.skyUseTextureFile;
            std::memcpy(skyUniform.clipFromLocal, sceneData.skyClipFromLocal.data(), sizeof(skyUniform.clipFromLocal));
            std::memcpy(skyUniform.eyeToWorldRotation, sceneData.skyEyeToWorld.data(), sizeof(skyUniform.eyeToWorldRotation));
            std::memcpy(mappedSkyUniformMemory, &skyUniform, sizeof(SkyUniformStd140));

            VkDescriptorSet skyTextureSet = textureCache.whiteDescriptorSet;
            if (sceneData.skyUseTextureFile != 0) {
                const VkDescriptorSet loaded = textureCache.descriptorForAbsolutePath(sceneData.skyEquirectAbsolute);
                if (loaded != VK_NULL_HANDLE) {
                    skyTextureSet = loaded;
                }
            }

            VkFence& frameFence = inFlightFences[currentFrame];
            ExpectVk(vkWaitForFences(device, 1, &frameFence, VK_TRUE, UINT64_MAX), "vkWaitForFences");

            std::uint32_t imageIndex = 0;
            VkResult acquireResult = vkAcquireNextImageKHR(
                device,
                swapchain,
                UINT64_MAX,
                imageAvailable[currentFrame],
                VK_NULL_HANDLE,
                &imageIndex);
            if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
                continue;
            }
            ExpectVk(acquireResult, "vkAcquireNextImageKHR");
            if (imageInFlight[imageIndex] != VK_NULL_HANDLE) {
                ExpectVk(vkWaitForFences(device, 1, &imageInFlight[imageIndex], VK_TRUE, UINT64_MAX),
                         "vkWaitForFences(image)");
            }
            imageInFlight[imageIndex] = frameFence;
            ExpectVk(vkResetFences(device, 1, &frameFence), "vkResetFences");
            ExpectVk(vkResetCommandBuffer(commandBuffers[imageIndex], 0), "vkResetCommandBuffer");
            RecordSceneCommandBuffer(commandBuffers[imageIndex],
                                     shadowRenderPass,
                                     shadowFramebuffer,
                                     VkExtent2D{kShadowMapResolution, kShadowMapResolution},
                                     shadowPipeline,
                                     shadowPipelineLayout,
                                     enableHybridHdr ? hdrSceneFramebuffer : framebuffers[imageIndex],
                                     enableHybridHdr ? hdrSceneRenderPass : renderPass,
                                     extent,
                                     enableHybridHdr ? skyPipelineHdr : skyPipeline,
                                     skyPipelineLayout,
                                     skyDescriptorSet,
                                     skyTextureSet,
                                     skyMesh,
                                     enableHybridHdr ? pipelineHdrScene : pipeline,
                                     enableHybridHdr ? pipelineHdrSceneAdditive : pipelineAdditive,
                                     pipelineLayout,
                                     cameraDescriptorSet,
                                     shadowDescriptorSet,
                                     textureCache,
                                     *sceneData.scene,
                                     meshCache,
                                     sceneData,
                                     enableHybridHdr ? compositeFramebuffers[imageIndex] : VK_NULL_HANDLE,
                                     enableHybridHdr ? compositeRenderPass : VK_NULL_HANDLE,
                                     enableHybridHdr ? compositePipeline : VK_NULL_HANDLE,
                                     enableHybridHdr ? compositePipelineLayout : VK_NULL_HANDLE,
                                     enableHybridHdr ? compositeHdrDescriptorSet : VK_NULL_HANDLE);

            const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            const VkSubmitInfo submitInfo{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &imageAvailable[currentFrame],
                .pWaitDstStageMask = &waitStage,
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffers[imageIndex],
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &renderFinished[currentFrame],
            };
            ExpectVk(vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameFence), "vkQueueSubmit");

            const VkPresentInfoKHR presentInfo{
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &renderFinished[currentFrame],
                .swapchainCount = 1,
                .pSwapchains = &swapchain,
                .pImageIndices = &imageIndex,
            };
            const VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
            if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR) {
                ExpectVk(presentResult, "vkQueuePresentKHR");
            }
            currentFrame = (currentFrame + 1U) % kFramesInFlight;
        }

        vkDeviceWaitIdle(device);
        textureCache.destroy();
        for (std::uint32_t frameIndex = 0; frameIndex < kFramesInFlight; ++frameIndex) {
            vkDestroyFence(device, inFlightFences[frameIndex], nullptr);
            vkDestroySemaphore(device, renderFinished[frameIndex], nullptr);
            vkDestroySemaphore(device, imageAvailable[frameIndex], nullptr);
        }
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyDescriptorPool(device, cameraDescriptorPool, nullptr);
        vkDestroyDescriptorPool(device, shadowDescriptorPool, nullptr);
        vkDestroyDescriptorPool(device, skyDescriptorPool, nullptr);
        if (mappedUniformMemory != nullptr) {
            vkUnmapMemory(device, uniformBuffer.memory);
        }
        if (mappedSkyUniformMemory != nullptr) {
            vkUnmapMemory(device, skyUniformBuffer.memory);
        }
        vkDestroySampler(device, shadowSampler, nullptr);
        ClearGpuMeshCache(device, meshCache);
        DestroyBuffer(device, skyMesh.vertexBuffer);
        DestroyBuffer(device, skyMesh.indexBuffer);
        DestroyBuffer(device, uniformBuffer);
        DestroyBuffer(device, skyUniformBuffer);
        vkDestroyPipeline(device, shadowPipeline, nullptr);
        vkDestroyShaderModule(device, shadowVertShader, nullptr);
        vkDestroyPipelineLayout(device, shadowPipelineLayout, nullptr);
        vkDestroyFramebuffer(device, shadowFramebuffer, nullptr);
        vkDestroyRenderPass(device, shadowRenderPass, nullptr);
        if (enableHybridHdr) {
            vkDestroyPipeline(device, compositePipeline, nullptr);
            vkDestroyPipeline(device, skyPipelineHdr, nullptr);
            vkDestroyPipeline(device, pipelineHdrSceneAdditive, nullptr);
            vkDestroyPipeline(device, pipelineHdrScene, nullptr);
            vkDestroyShaderModule(device, compositeFragShader, nullptr);
            vkDestroyShaderModule(device, compositeVertShader, nullptr);
            vkDestroyDescriptorPool(device, compositeDescriptorPool, nullptr);
            for (VkFramebuffer compositeFb : compositeFramebuffers) {
                if (compositeFb != VK_NULL_HANDLE) {
                    vkDestroyFramebuffer(device, compositeFb, nullptr);
                }
            }
            vkDestroyRenderPass(device, compositeRenderPass, nullptr);
            vkDestroyFramebuffer(device, hdrSceneFramebuffer, nullptr);
            vkDestroyRenderPass(device, hdrSceneRenderPass, nullptr);
            vkDestroyImageView(device, hdrSceneColorImage.view, nullptr);
            vkDestroyImage(device, hdrSceneColorImage.image, nullptr);
            vkFreeMemory(device, hdrSceneColorImage.memory, nullptr);
            vkDestroyPipelineLayout(device, compositePipelineLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, compositeHdrTextureSetLayout, nullptr);
        }
        vkDestroyPipeline(device, skyPipeline, nullptr);
        vkDestroyShaderModule(device, skyFragShader, nullptr);
        vkDestroyShaderModule(device, skyVertShader, nullptr);
        vkDestroyPipelineLayout(device, skyPipelineLayout, nullptr);
        vkDestroyPipeline(device, pipelineAdditive, nullptr);
        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyShaderModule(device, fragShader, nullptr);
        vkDestroyShaderModule(device, vertShader, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, shadowSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, textureSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, skyCameraSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, cameraSetLayout, nullptr);
        for (VkFramebuffer framebuffer : framebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        vkDestroyRenderPass(device, renderPass, nullptr);
        vkDestroyImageView(device, depthImage.view, nullptr);
        vkDestroyImage(device, depthImage.image, nullptr);
        vkFreeMemory(device, depthImage.memory, nullptr);
        vkDestroyImageView(device, shadowDepthImage.view, nullptr);
        vkDestroyImage(device, shadowDepthImage.image, nullptr);
        vkFreeMemory(device, shadowDepthImage.memory, nullptr);
        for (VkImageView imageView : swapchainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
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
#else
    if (error != nullptr) {
        *error = "Native Vulkan scene preview is disabled because glslangValidator was not available at build time.";
    }
    (void)width;
    (void)height;
    (void)buildFrame;
    (void)options;
    return false;
#endif
}

bool PresentSceneKitPreviewWindowNative(const ri::scene::SceneKitPreview& preview,
                                        const int width,
                                        const int height,
                                        const VulkanPreviewWindowOptions& options,
                                        std::string* error) {
    const VulkanNativeSceneFrameCallback buildFrame =
        [&preview, &options](VulkanNativeSceneFrame& frame, std::string*) {
            frame.scene = &preview.scene;
            frame.cameraNode = preview.orbitCamera.cameraNode;
            frame.photoMode = options.scenePhotoMode;
            frame.photoModeEnabled = ri::scene::PhotoModeFieldOfViewActive(options.scenePhotoMode);
            frame.textureRoot = options.textureRoot;
            frame.animationTimeSeconds = static_cast<double>(GetTickCount64()) * 0.001;
            frame.postProcess.timeSeconds = static_cast<float>(frame.animationTimeSeconds);
            return true;
        };
    return RunVulkanNativeSceneLoop(width, height, buildFrame, options, error);
}

} // namespace ri::render::vulkan

#else

namespace ri::render::vulkan {

bool RunVulkanNativeSceneLoop(int,
                              int,
                              const VulkanNativeSceneFrameCallback&,
                              const VulkanPreviewWindowOptions&,
                              std::string* error) {
    if (error != nullptr) {
        *error = "Native Vulkan scene preview is currently only implemented on Windows.";
    }
    return false;
}

bool PresentSceneKitPreviewWindowNative(const ri::scene::SceneKitPreview&,
                                        int,
                                        int,
                                        const VulkanPreviewWindowOptions&,
                                        std::string* error) {
    if (error != nullptr) {
        *error = "Native Vulkan scene preview is currently only implemented on Windows.";
    }
    return false;
}

} // namespace ri::render::vulkan

#endif
