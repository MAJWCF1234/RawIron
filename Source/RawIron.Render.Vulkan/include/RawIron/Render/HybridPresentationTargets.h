#pragma once

#include <cstdint>

namespace ri::render::vulkan {

/// Logical stages for RawIron's hybrid presentation path: **forward** shading first, then a
/// **deferred-style screen-space bundle** that reads depth + HDR (no full G-buffer yet), then
/// **post composite** (tonemap / grading). This blends classic forward rendering with deferred
/// screen-space techniques without maintaining two complete lighting paths.
enum class HybridPresentationStage : std::uint8_t {
    ShadowDepth = 0,
    /// Primary lit scene pass (forward PBR); renders into an HDR color target + shared depth.
    SceneForward = 1,
    /// Screen-space pass(es): sample forward HDR + stored depth (contact AO today; SSAO/SSR hooks).
    ScreenSpaceBundle = 2,
    /// Tonemap / bloom / color grading into swapchain — consumes processed HDR from the bundle stage.
    PostComposite = 3,
    Present = 4,
};

/// Recommended Vulkan formats for optional hybrid targets (feature-check before use).
struct HybridPresentationFormats {
    /// Scene-linear HDR color target from the forward pass; input to the screen-space bundle.
    static constexpr auto kSceneHdrColor = "VK_FORMAT_R16G16B16A16_SFLOAT";
    /// Output of the screen-space bundle (still linear HDR) — fed to composite / tonemap.
    static constexpr auto kBundleHdrColor = "VK_FORMAT_R16G16B16A16_SFLOAT";
    /// Depth buffer must remain store-compatible and sampleable for deferred-style screen passes.
    static constexpr auto kSceneDepthSampled = "VK_FORMAT_D32_SFLOAT or equivalent with SAMPLE usage";
    /// Future: packed normal + roughness for higher-quality SSAO / SSR (optional second MRT).
    static constexpr auto kThinGbufferSuggestion = "VK_FORMAT_R8G8B8A8_UNORM or VK_FORMAT_R16G16B16A16_SNORM";
    /// Optional future: per-pixel material / outline id.
    static constexpr auto kMaterialIdSuggestion = "VK_FORMAT_R32_UINT or VK_FORMAT_R8_UINT";
};

} // namespace ri::render::vulkan
