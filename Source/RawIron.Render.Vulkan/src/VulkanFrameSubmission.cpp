#include "RawIron/Render/VulkanFrameSubmission.h"

namespace ri::render::vulkan {

namespace {

bool ValidateSubmissionPlan(const std::vector<VulkanCommandIntent>& intents,
                            const VulkanIntentStagingPlan& plan) {
    if (plan.status != VulkanIntentStagingStatus::Ok || plan.totalIntents != intents.size()) {
        return false;
    }

    bool inBatch = false;
    std::size_t rangeIndex = 0U;
    std::size_t stagedIntents = 0U;
    std::size_t activeIntentCount = 0U;
    std::size_t activeClearCount = 0U;
    std::size_t activeViewProjectionCount = 0U;
    std::size_t activeDrawCount = 0U;

    for (std::size_t intentIndex = 0; intentIndex < intents.size(); ++intentIndex) {
        const VulkanCommandIntent& intent = intents[intentIndex];
        switch (intent.type) {
        case VulkanCommandIntentType::BeginBatch: {
            if (inBatch || rangeIndex >= plan.ranges.size()) {
                return false;
            }
            const VulkanIntentRange& range = plan.ranges[rangeIndex];
            if (range.passIndex != intent.passIndex
                || range.pipelineBucket != intent.pipelineBucket
                || range.firstIntentIndex != intentIndex + 1U) {
                return false;
            }
            inBatch = true;
            activeIntentCount = 0U;
            activeClearCount = 0U;
            activeViewProjectionCount = 0U;
            activeDrawCount = 0U;
            break;
        }
        case VulkanCommandIntentType::EndBatch: {
            if (!inBatch || rangeIndex >= plan.ranges.size()) {
                return false;
            }
            const VulkanIntentRange& range = plan.ranges[rangeIndex];
            if (range.passIndex != intent.passIndex
                || range.pipelineBucket != intent.pipelineBucket
                || range.intentCount != activeIntentCount
                || range.clearCount != activeClearCount
                || range.setViewProjectionCount != activeViewProjectionCount
                || range.drawCount != activeDrawCount) {
                return false;
            }
            inBatch = false;
            ++rangeIndex;
            break;
        }
        case VulkanCommandIntentType::ClearColor:
        case VulkanCommandIntentType::SetViewProjection:
        case VulkanCommandIntentType::DrawMesh: {
            if (!inBatch || rangeIndex >= plan.ranges.size()) {
                return false;
            }
            const VulkanIntentRange& range = plan.ranges[rangeIndex];
            if (intent.passIndex != range.passIndex || intent.pipelineBucket != range.pipelineBucket) {
                return false;
            }
            ++activeIntentCount;
            ++stagedIntents;
            if (intent.type == VulkanCommandIntentType::ClearColor) {
                ++activeClearCount;
            } else if (intent.type == VulkanCommandIntentType::SetViewProjection) {
                ++activeViewProjectionCount;
            } else {
                ++activeDrawCount;
            }
            break;
        }
        default:
            return false;
        }
    }

    return !inBatch && rangeIndex == plan.ranges.size() && stagedIntents == plan.stagedIntents;
}

bool ReplayIntent(const VulkanCommandIntent& intent, VulkanBackendRecorder& recorder) {
    switch (intent.type) {
    case VulkanCommandIntentType::ClearColor:
        return recorder.ClearColor(intent.clearColor);
    case VulkanCommandIntentType::SetViewProjection:
        return recorder.SetViewProjection(intent.viewProjection);
    case VulkanCommandIntentType::DrawMesh:
        return recorder.DrawMesh(intent.meshHandle,
                                 intent.materialHandle,
                                 intent.firstIndex,
                                 intent.indexCount,
                                 intent.instanceCount,
                                 intent.model);
    case VulkanCommandIntentType::BeginBatch:
    case VulkanCommandIntentType::EndBatch:
    default:
        return false;
    }
}

std::uint16_t ToMaterialBucket(std::int32_t materialHandle) {
    if (materialHandle < 0) {
        return 0U;
    }
    return static_cast<std::uint16_t>(static_cast<std::uint32_t>(materialHandle) & 0xFFFFU);
}

} // namespace

bool ExecuteVulkanFrameSubmission(const std::vector<VulkanCommandIntent>& intents,
                                  const VulkanIntentStagingPlan& plan,
                                  VulkanBackendRecorder& recorder,
                                  const VulkanSubmissionPassFilter& filter,
                                  VulkanFrameSubmissionStats* outStats) {
    VulkanFrameSubmissionStats stats{};
    const auto flushStats = [&]() {
        if (outStats != nullptr) {
            *outStats = stats;
        }
    };

    if (!ValidateSubmissionPlan(intents, plan)) {
        flushStats();
        return false;
    }

    for (const VulkanIntentRange& range : plan.ranges) {
        stats.rangesVisited += 1U;

        if (range.passIndex < filter.minPassIndex || range.passIndex > filter.maxPassIndex) {
            continue;
        }
        if (!recorder.BeginBatch(range.passIndex, range.pipelineBucket)) {
            flushStats();
            return false;
        }
        stats.rangesSubmitted += 1U;

        const std::size_t end = range.firstIntentIndex + range.intentCount;
        for (std::size_t index = range.firstIntentIndex; index < end; ++index) {
            if (!ReplayIntent(intents[index], recorder)) {
                flushStats();
                return false;
            }
            stats.commandsSubmitted += 1U;
        }

        if (!recorder.EndBatch(range.passIndex, range.pipelineBucket)) {
            flushStats();
            return false;
        }
    }

    flushStats();
    return true;
}

bool ExecuteVulkanFrameSubmissionWithPipelineCache(
    const std::vector<VulkanCommandIntent>& intents,
    const VulkanIntentStagingPlan& plan,
    VulkanBackendRecorder& recorder,
    VulkanPipelineStateCache& cache,
    const VulkanPipelineStateCache::ResolveFn& resolver,
    const VulkanSubmissionPassFilter& filter,
    VulkanFrameSubmissionStats* outStats) {
    VulkanFrameSubmissionStats stats{};
    const auto flushStats = [&]() {
        if (outStats != nullptr) {
            *outStats = stats;
        }
    };

    if (!ValidateSubmissionPlan(intents, plan)) {
        flushStats();
        return false;
    }

    for (const VulkanIntentRange& range : plan.ranges) {
        stats.rangesVisited += 1U;

        if (range.passIndex < filter.minPassIndex || range.passIndex > filter.maxPassIndex) {
            continue;
        }
        if (!recorder.BeginBatch(range.passIndex, range.pipelineBucket)) {
            flushStats();
            return false;
        }
        stats.rangesSubmitted += 1U;

        const std::size_t end = range.firstIntentIndex + range.intentCount;
        for (std::size_t index = range.firstIntentIndex; index < end; ++index) {
            const VulkanCommandIntent& intent = intents[index];
            if (intent.type == VulkanCommandIntentType::DrawMesh) {
                const VulkanPipelineStateKey key{
                    .passIndex = range.passIndex,
                    .pipelineBucket = range.pipelineBucket,
                    .materialBucket = ToMaterialBucket(intent.materialHandle),
                };
                stats.pipelineResolves += 1U;
                const std::optional<VulkanPipelineStateRecord> resolved = cache.Resolve(key, resolver);
                if (!resolved.has_value()) {
                    stats.pipelineResolveFailures += 1U;
                    flushStats();
                    return false;
                }
                // Validation/warming gate only: every draw must have a resolvable pipeline
                // record before replay, but binding itself happens inside the recorder's
                // DrawMesh (which owns the concrete VkPipeline for this material/pass). The
                // recorder interface intentionally exposes no apply-state hook, so the
                // resolved record is not bound here.
                (void)resolved;
            }

            if (!ReplayIntent(intent, recorder)) {
                flushStats();
                return false;
            }
            stats.commandsSubmitted += 1U;
        }

        if (!recorder.EndBatch(range.passIndex, range.pipelineBucket)) {
            flushStats();
            return false;
        }
    }

    flushStats();
    return true;
}

} // namespace ri::render::vulkan
