#pragma once

#include "RawIron/Core/RenderCommandStream.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace ri::core {

struct RenderSortKeyFields {
    std::uint8_t passIndex = 0;
    std::uint16_t pipelineBucket = 0;
    std::uint16_t materialBucket = 0;
    std::uint16_t depthBucket = 0;
    std::uint8_t tieBreaker = 0;
};

[[nodiscard]] constexpr RenderSortKeyFields UnpackRenderSortKey(std::uint64_t key) noexcept {
    return RenderSortKeyFields{
        .passIndex = static_cast<std::uint8_t>((key >> 56U) & 0xFFU),
        .pipelineBucket = static_cast<std::uint16_t>((key >> 40U) & 0xFFFFU),
        .materialBucket = static_cast<std::uint16_t>((key >> 24U) & 0xFFFFU),
        .depthBucket = static_cast<std::uint16_t>((key >> 8U) & 0xFFFFU),
        .tieBreaker = static_cast<std::uint8_t>(key & 0xFFU),
    };
}

struct RenderSubmissionBatch {
    std::uint8_t passIndex = 0;
    std::uint16_t pipelineBucket = 0;
    std::size_t firstOrderIndex = 0;
    std::size_t commandCount = 0;
};

struct RenderSubmissionPlan {
    std::vector<std::size_t> packetOrder;
    std::vector<RenderSubmissionBatch> batches;
};

[[nodiscard]] RenderSubmissionPlan BuildRenderSubmissionPlan(const RenderCommandStream& stream);
/// Visits commands in `batchIndex` in submission order. Returns false only when the stream cannot
/// decode a packet. If `visitor` returns false, iteration stops early and this function still returns
/// true (caller distinguishes failure vs early stop when wrapping sinks).
[[nodiscard]] bool ForEachSubmissionBatchCommand(
    const RenderCommandStream& stream,
    const RenderSubmissionPlan& plan,
    std::size_t batchIndex,
    const std::function<bool(const RenderCommandView& view, std::size_t orderIndex, std::size_t packetIndex)>& visitor);

} // namespace ri::core
