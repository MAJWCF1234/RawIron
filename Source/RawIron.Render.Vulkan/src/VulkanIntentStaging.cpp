#include "RawIron/Render/VulkanIntentStaging.h"

namespace ri::render::vulkan {

VulkanIntentStagingPlan BuildVulkanIntentStagingPlan(const std::vector<VulkanCommandIntent>& intents) {
    VulkanIntentStagingPlan plan{};
    plan.totalIntents = intents.size();

    bool inBatch = false;
    std::uint8_t activePass = 0;
    std::uint16_t activePipeline = 0;
    std::size_t activeRangeIndex = 0;

    for (std::size_t index = 0; index < intents.size(); ++index) {
        const VulkanCommandIntent& intent = intents[index];
        switch (intent.type) {
        case VulkanCommandIntentType::BeginBatch:
            if (inBatch) {
                plan.status = VulkanIntentStagingStatus::UnexpectedBeginBatch;
                return plan;
            }
            inBatch = true;
            activePass = intent.passIndex;
            activePipeline = intent.pipelineBucket;
            plan.ranges.push_back(VulkanIntentRange{
                .passIndex = activePass,
                .pipelineBucket = activePipeline,
                .firstIntentIndex = index + 1U,
                .intentCount = 0,
                .clearCount = 0,
                .setViewProjectionCount = 0,
                .drawCount = 0,
            });
            activeRangeIndex = plan.ranges.size() - 1U;
            break;

        case VulkanCommandIntentType::EndBatch:
            if (!inBatch) {
                plan.status = VulkanIntentStagingStatus::CommandOutsideBatch;
                return plan;
            }
            if (intent.passIndex != activePass || intent.pipelineBucket != activePipeline) {
                plan.status = VulkanIntentStagingStatus::MismatchedEndBatch;
                return plan;
            }
            inBatch = false;
            break;

        case VulkanCommandIntentType::ClearColor:
        case VulkanCommandIntentType::SetViewProjection:
        case VulkanCommandIntentType::DrawMesh:
            if (!inBatch) {
                plan.status = VulkanIntentStagingStatus::CommandOutsideBatch;
                return plan;
            }
            if (intent.passIndex != activePass || intent.pipelineBucket != activePipeline) {
                plan.status = VulkanIntentStagingStatus::MismatchedEndBatch;
                return plan;
            }
            plan.ranges[activeRangeIndex].intentCount += 1U;
            plan.stagedIntents += 1U;
            if (intent.type == VulkanCommandIntentType::ClearColor) {
                plan.ranges[activeRangeIndex].clearCount += 1U;
            } else if (intent.type == VulkanCommandIntentType::SetViewProjection) {
                plan.ranges[activeRangeIndex].setViewProjectionCount += 1U;
            } else if (intent.type == VulkanCommandIntentType::DrawMesh) {
                plan.ranges[activeRangeIndex].drawCount += 1U;
            }
            break;
        }
    }

    if (inBatch) {
        plan.status = VulkanIntentStagingStatus::UnterminatedBatch;
        return plan;
    }

    plan.status = VulkanIntentStagingStatus::Ok;
    return plan;
}

} // namespace ri::render::vulkan
