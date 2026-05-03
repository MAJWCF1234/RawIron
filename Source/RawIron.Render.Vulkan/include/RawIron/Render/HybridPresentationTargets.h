#pragma once

#include <cstdint>

namespace ri::render::vulkan {

/// Logical stages for RawIron's hybrid presentation path (forward scene pass + shared targets +
/// screen-space / post). Ordering matches typical GPU scheduling; not every stage is implemented yet.
enum class HybridPresentationStage : std::uint8_t {
    ShadowDepth = 0,
    /// Primary lit scene pass (forward); may render into an HDR surface when hybrid HDR is enabled.
    SceneForward = 1,
    /// Future: SSAO / SSR / decals sampling depth + thin material buffers.
    ScreenSpaceBundle = 2,
    /// Tonemap / bloom / color grading — often a fullscreen pass after HDR scene color exists.
    PostComposite = 3,
    Present = 4,
};

/// Recommended Vulkan formats for optional hybrid targets (feature-check before use).
/// Scene HDR is the minimum stepping stone toward masked bloom, SSAO, and SSR without full deferred lighting.
struct HybridPresentationFormats {
    /// Scene-linear HDR color target feeding composite / future screen-space passes.
    static constexpr auto kSceneHdrColor = "VK_FORMAT_R16G16B16A16_SFLOAT";
    /// Optional future: packed normal + roughness for SSAO / SSR (not allocated by default).
    static constexpr auto kThinGbufferSuggestion = "VK_FORMAT_R8G8B8A8_UNORM or VK_FORMAT_R16G16B16A16_SNORM";
    /// Optional future: per-pixel material / outline id.
    static constexpr auto kMaterialIdSuggestion = "VK_FORMAT_R32_UINT or VK_FORMAT_R8_UINT";
};

} // namespace ri::render::vulkan
