#pragma once

#include <cstdint>

namespace ri::render::vulkan {

/// Returns whether a callback-produced native scene frame should proceed to rendering.
/// Continuous producers disable suppression and always render. Suppressed producers render their
/// first frame and then skip only an exact repeat of the last successfully presented sequence.
/// Numeric ordering is deliberately ignored: decreases and wrap render when the value differs, but
/// reusing the exact last-presented id cannot signal a reset and requires a new id or continuous mode.
[[nodiscard]] bool ShouldRenderVulkanNativeSceneFrame(
    std::uint64_t frameSequence,
    bool suppressUnchangedFrames,
    bool hasPresentedFrame,
    std::uint64_t lastPresentedFrameSequence) noexcept;

} // namespace ri::render::vulkan
