#include "RawIron/Render/VulkanFrameSubmission.h"

namespace ri::render::vulkan {

namespace {

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

    if (plan.status != VulkanIntentStagingStatus::Ok) {
        flushStats();
        return false;
    }

    for (const VulkanIntentRange& range : plan.ranges) {
        stats.rangesVisited += 1U;

        if (range.passIndex < filter.minPassIndex || range.passIndex > filter.maxPassIndex) {
            continue;
        }
        if (range.firstIntentIndex + range.intentCount > intents.size()) {
            flushStats();
            return false;
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

    if (plan.status != VulkanIntentStagingStatus::Ok) {
        flushStats();
        return false;
    }

    for (const VulkanIntentRange& range : plan.ranges) {
        stats.rangesVisited += 1U;

        if (range.passIndex < filter.minPassIndex || range.passIndex > filter.maxPassIndex) {
            continue;
        }
        if (range.firstIntentIndex + range.intentCount > intents.size()) {
            flushStats();
            return false;
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
