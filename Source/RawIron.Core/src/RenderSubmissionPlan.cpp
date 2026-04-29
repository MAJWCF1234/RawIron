#include "RawIron/Core/RenderSubmissionPlan.h"

namespace ri::core {

RenderSubmissionPlan BuildRenderSubmissionPlan(const RenderCommandStream& stream) {
    RenderSubmissionPlan plan{};
    const std::vector<std::size_t> sortedPacketOrder = stream.BuildSortedPacketOrder();
    plan.packetOrder.reserve(sortedPacketOrder.size());

    RenderCommandView view{};
    for (std::size_t orderIndex = 0; orderIndex < sortedPacketOrder.size(); ++orderIndex) {
        const std::size_t packetIndex = sortedPacketOrder[orderIndex];
        if (!stream.ReadPacket(packetIndex, view)) {
            continue;
        }
        plan.packetOrder.push_back(packetIndex);
        const std::size_t validOrderIndex = plan.packetOrder.size() - 1U;

        const RenderSortKeyFields key = UnpackRenderSortKey(view.header.sortKey);
        const bool startNewBatch = plan.batches.empty()
            || plan.batches.back().passIndex != key.passIndex
            || plan.batches.back().pipelineBucket != key.pipelineBucket;

        if (startNewBatch) {
            plan.batches.push_back(RenderSubmissionBatch{
                .passIndex = key.passIndex,
                .pipelineBucket = key.pipelineBucket,
                .firstOrderIndex = validOrderIndex,
                .commandCount = 1,
            });
        } else {
            plan.batches.back().commandCount += 1;
        }
    }

    return plan;
}

bool ForEachSubmissionBatchCommand(
    const RenderCommandStream& stream,
    const RenderSubmissionPlan& plan,
    std::size_t batchIndex,
    const std::function<bool(const RenderCommandView& view, std::size_t orderIndex, std::size_t packetIndex)>& visitor) {
    if (batchIndex >= plan.batches.size()) {
        return false;
    }
    if (!visitor) {
        return false;
    }

    const RenderSubmissionBatch& batch = plan.batches[batchIndex];
    const std::size_t first = batch.firstOrderIndex;
    if (first > plan.packetOrder.size()) {
        return false;
    }
    if (batch.commandCount > (plan.packetOrder.size() - first)) {
        return false;
    }
    const std::size_t lastExclusive = first + batch.commandCount;

    RenderCommandView view{};
    for (std::size_t orderIndex = first; orderIndex < lastExclusive; ++orderIndex) {
        const std::size_t packetIndex = plan.packetOrder[orderIndex];
        if (!stream.ReadPacket(packetIndex, view)) {
            return false;
        }
        if (!visitor(view, orderIndex, packetIndex)) {
            break;
        }
    }

    return true;
}

} // namespace ri::core
