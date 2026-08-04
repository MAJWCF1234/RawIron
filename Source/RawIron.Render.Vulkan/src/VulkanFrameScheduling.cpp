#include "RawIron/Render/VulkanFrameScheduling.h"

namespace ri::render::vulkan {

bool ShouldRenderVulkanNativeSceneFrame(const std::uint64_t frameSequence,
                                        const bool suppressUnchangedFrames,
                                        const bool hasPresentedFrame,
                                        const std::uint64_t lastPresentedFrameSequence) noexcept {
    return !suppressUnchangedFrames
        || !hasPresentedFrame
        || frameSequence != lastPresentedFrameSequence;
}

} // namespace ri::render::vulkan
