#pragma once

#include "RawIron/Render/VulkanIntentStaging.h"
#include "RawIron/Render/VulkanPipelineStateCache.h"

#include <cstddef>
#include <cstdint>

namespace ri::render::vulkan {

struct VulkanSubmissionPassFilter {
    std::uint8_t minPassIndex = 0;
    std::uint8_t maxPassIndex = 255;
};

struct VulkanFrameSubmissionStats {
    std::size_t rangesVisited = 0;
    std::size_t rangesSubmitted = 0;
    std::size_t commandsSubmitted = 0;
    std::size_t pipelineResolves = 0;
    std::size_t pipelineResolveFailures = 0;
};

bool ExecuteVulkanFrameSubmission(const std::vector<VulkanCommandIntent>& intents,
                                  const VulkanIntentStagingPlan& plan,
                                  VulkanBackendRecorder& recorder,
                                  const VulkanSubmissionPassFilter& filter = {},
                                  VulkanFrameSubmissionStats* outStats = nullptr);

bool ExecuteVulkanFrameSubmissionWithPipelineCache(
    const std::vector<VulkanCommandIntent>& intents,
    const VulkanIntentStagingPlan& plan,
    VulkanBackendRecorder& recorder,
    VulkanPipelineStateCache& cache,
    const VulkanPipelineStateCache::ResolveFn& resolver,
    const VulkanSubmissionPassFilter& filter = {},
    VulkanFrameSubmissionStats* outStats = nullptr);

} // namespace ri::render::vulkan
