#pragma once

#include "RawIron/Render/PostProcessProfiles.h"
#include "RawIron/Scene/PhotoModeCamera.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::scene {
class Scene;
struct SceneKitPreview;
}

namespace ri::render::vulkan {

enum class VulkanPresentModePreference {
    Auto,
    Mailbox,
    Immediate,
    Fifo,
};

struct SceneKitPreviewRenderBridgeStats {
    std::size_t renderCommandCount = 0;
    std::size_t submissionBatchCount = 0;
    std::size_t drawCommandCount = 0;
    std::size_t skippedNodeCount = 0;
    std::size_t vulkanOpCount = 0;
    std::size_t intentCount = 0;
    std::size_t stagedRangeCount = 0;
};

enum class PreviewPixelFormat {
    Bgr8,
    Rgba8,
};

struct PreviewImageData {
    int width = 0;
    int height = 0;
    PreviewPixelFormat format = PreviewPixelFormat::Bgr8;
    std::vector<std::uint8_t> pixels;
};

struct VulkanPreviewWindowOptions {
    std::string windowTitle = "RawIron Vulkan Preview";
    VulkanPresentModePreference presentModePreference = VulkanPresentModePreference::Auto;
    /// Optional FOV overrides for Scene Kit preview (software path + Vulkan bridge); inactive overrides are ignored.
    ri::scene::PhotoModeCameraOverrides scenePhotoMode{};
    /// Root directory for `Material::baseColorTexture` filenames in native Vulkan preview.
    std::filesystem::path textureRoot{};
    /// Optional: receives every Win32 message (after NCCREATE). For mouse look / keyboard sampling.
    void* messageUserData = nullptr;
    using Win32MessageHook = void (*)(void* user, void* hwnd, unsigned int message, std::uint64_t wParam, std::int64_t lParam);
    Win32MessageHook onWin32Message = nullptr;
    /// Written once the client HWND exists (same as swapchain surface target).
    void* outClientHwnd = nullptr;
    /// When true, native preview renders scene-linear HDR into an offscreen target and runs a fullscreen
    /// composite (tonemap + existing post chain) to the swapchain — foundation for SSAO/SSR/bloom masks.
    bool enableHybridHdrPresentation = false;
};

struct VulkanNativeSceneFrame {
    const ri::scene::Scene* scene = nullptr;
    int cameraNode = -1;
    ri::scene::PhotoModeCameraOverrides photoMode{};
    bool photoModeEnabled = false;
    /// When non-empty, native Vulkan draws sample albedo textures from this directory.
    std::filesystem::path textureRoot{};
    /// When non-empty, path is relative to `textureRoot` (e.g. `Skies/sky_equirect.png`) for native skybox sampling.
    std::filesystem::path skyEquirectTextureRelative{};
    /// Drives `Material::baseColorTextureFrames` selection and optional water UV motion in the native Vulkan path.
    double animationTimeSeconds = 0.0;
    /// Global native Vulkan shading controls.
    int renderQualityTier = 1; // 0=competitive, 1=balanced, 2=cinematic
    float renderExposure = 1.0f;
    float renderContrast = 1.0f;
    float renderSaturation = 1.0f;
    float renderFogDensity = 0.0095f;
    /// Optional post-process shaping consumed by the native Vulkan preview shader.
    ri::render::PostProcessParameters postProcess{};
};

using VulkanNativeSceneFrameCallback = std::function<bool(VulkanNativeSceneFrame& frame, std::string* error)>;

bool PresentPreviewImageWindow(const PreviewImageData& image,
                               const VulkanPreviewWindowOptions& options = {},
                               std::string* error = nullptr);

/// Win32 + Vulkan swapchain loop. The window matches `windowWidth`/`windowHeight`; each frame calls `fillFrame`
/// with BGR8 or RGB8 data sized to `softwareRenderWidth`/`softwareRenderHeight`. When that size is smaller than the
/// swapchain image, the presenter upscales with `vkCmdBlitImage` (linear filter). When equal, it copies directly.
bool RunVulkanSoftwarePreviewLoop(int windowWidth,
                                  int windowHeight,
                                  int softwareRenderWidth,
                                  int softwareRenderHeight,
                                  const std::function<void(PreviewImageData& frame)>& fillFrame,
                                  const VulkanPreviewWindowOptions& options = {},
                                  std::string* error = nullptr);

bool BuildSceneKitPreviewVulkanBridge(const ri::scene::SceneKitPreview& preview,
                                      int width,
                                      int height,
                                      const ri::scene::PhotoModeCameraOverrides* photoMode = nullptr,
                                      SceneKitPreviewRenderBridgeStats* outStats = nullptr,
                                      std::string* error = nullptr);

bool RunVulkanNativeSceneLoop(int width,
                              int height,
                              const VulkanNativeSceneFrameCallback& buildFrame,
                              const VulkanPreviewWindowOptions& options = {},
                              std::string* error = nullptr);

bool PresentSceneKitPreviewWindowNative(const ri::scene::SceneKitPreview& preview,
                                        int width,
                                        int height,
                                        const VulkanPreviewWindowOptions& options = {},
                                        std::string* error = nullptr);

bool PresentSceneKitPreviewWindow(const ri::scene::SceneKitPreview& preview,
                                  int width,
                                  int height,
                                  const VulkanPreviewWindowOptions& options = {},
                                  std::string* error = nullptr);

} // namespace ri::render::vulkan
