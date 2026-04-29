#pragma once

#include "RawIron/Render/VulkanCommandBufferRecorder.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ri::render::vulkan {

enum class VulkanIntentStagingStatus : std::uint8_t {
    Ok = 0,
    UnexpectedBeginBatch = 1,
    CommandOutsideBatch = 2,
    MismatchedEndBatch = 3,
    UnterminatedBatch = 4,
};

struct VulkanIntentRange {
    std::uint8_t passIndex = 0;
    std::uint16_t pipelineBucket = 0;
    std::size_t firstIntentIndex = 0;
    std::size_t intentCount = 0;
    std::size_t clearCount = 0;
    std::size_t setViewProjectionCount = 0;
    std::size_t drawCount = 0;
};

struct VulkanIntentStagingPlan {
    VulkanIntentStagingStatus status = VulkanIntentStagingStatus::Ok;
    std::size_t totalIntents = 0;
    std::size_t stagedIntents = 0;
    std::vector<VulkanIntentRange> ranges;
};

[[nodiscard]] VulkanIntentStagingPlan BuildVulkanIntentStagingPlan(const std::vector<VulkanCommandIntent>& intents);

} // namespace ri::render::vulkan
