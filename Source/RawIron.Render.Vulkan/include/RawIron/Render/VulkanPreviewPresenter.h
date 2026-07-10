#pragma once

#include "RawIron/Render/PostProcessProfiles.h"
#include "RawIron/Render/ShaderConfig.h"
#include "RawIron/Scene/PhotoModeCamera.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
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
    /// Optional existing Win32 HWND to use as the Vulkan surface target instead of creating one.
    void* clientHwnd = nullptr;
    /// Optional thread-safe notification when the client HWND has been created.
    std::function<void(void*)> onClientHwndCreated{};
    /// Optional Win32 parent HWND. When set, the Vulkan surface is created as an embedded child window.
    void* parentHwnd = nullptr;
    /// When true, native preview renders scene-linear HDR into an offscreen target and runs a fullscreen
    /// composite (tonemap + existing post chain) to the swapchain — foundation for SSAO/SSR/bloom masks.
    bool enableHybridHdrPresentation = true;
    /// Seeds shadow-map resolution and stabilization at Vulkan init (0=1024, 1=2048, 2=4096).
    int initialRenderQualityTier = 1;
    /// Optional `shader.cfg` layer applied after each `VulkanNativeSceneFrameCallback` (see `ShaderConfig.h`).
    ShaderPresentationConfig shaderPresentation{};
};

struct VulkanNativeSceneFrame {
    /// Optional owner that keeps an immutable scene snapshot alive through submission.
    std::shared_ptr<const ri::scene::Scene> sceneOwner{};
    const ri::scene::Scene* scene = nullptr;
    /// Stable identity for GPU mesh/texture caches when `scene` is an immutable per-frame snapshot.
    const void* sceneCacheIdentity = nullptr;
    /// Monotonic snapshot id; unchanged ids mean the editor has not published a new frame payload.
    std::uint64_t frameSequence = 0;
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
    /// Linear distance fog (matches software `ScenePreviewOptions` / `rendering.riscript`).
    float renderFogStart = 2.0f;
    float renderFogEnd = 48.0f;
    float renderFogStrength = 0.90f;
    /// When true, `environmentClearTop` / `environmentClearBottom` replace the default dark clear.
    bool useEnvironmentClear = false;
    ri::math::Vec3 environmentClearTop{0.20f, 0.29f, 0.34f};
    ri::math::Vec3 environmentClearBottom{0.05f, 0.08f, 0.06f};
    /// Outdoor fog / ambient tints for the native lit pass (`NativeScenePreview.frag`).
    ri::math::Vec3 nativeFogColorNear{0.34f, 0.39f, 0.42f};
    ri::math::Vec3 nativeFogColorFar{0.40f, 0.47f, 0.54f};
    ri::math::Vec3 nativeAmbientLight{0.14f, 0.17f, 0.12f};
    /// Optional post-process shaping consumed by the native Vulkan preview shader.
    ri::render::PostProcessParameters postProcess{};
};

struct VulkanNativeSceneResolvedTuning {
    float exposure = 1.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;
    float fogAmount = 0.0095f;
    float fogStart = 2.0f;
    float fogEnd = 48.0f;
    bool linearFog = true;
    ri::math::Vec3 environmentTop{0.20f, 0.29f, 0.34f};
    ri::math::Vec3 environmentBottom{0.05f, 0.08f, 0.06f};
    ri::math::Vec3 fogColorNear{0.34f, 0.39f, 0.42f};
    ri::math::Vec3 fogColorFar{0.40f, 0.47f, 0.54f};
    ri::math::Vec3 ambientLight{0.14f, 0.17f, 0.12f};
};

/// Prevents NaN/Inf authoring or script values from reaching Vulkan uniform buffers.
[[nodiscard]] VulkanNativeSceneResolvedTuning ResolveVulkanNativeSceneTuning(
    const VulkanNativeSceneFrame& frame);

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
