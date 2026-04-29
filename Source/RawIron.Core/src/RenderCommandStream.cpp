#include "RawIron/Core/RenderCommandStream.h"

#include <algorithm>
#include <cstring>
#include <numeric>

namespace ri::core {
namespace {

[[nodiscard]] bool SizeAddWithin(std::size_t base, std::size_t add, std::size_t limitExclusive) noexcept {
    if (base > limitExclusive || add > limitExclusive) {
        return false;
    }
    const std::size_t sum = base + add;
    if (sum < base) {
        return false;
    }
    return sum <= limitExclusive;
}

} // namespace

void RenderCommandStream::Clear() noexcept {
    bytes_.clear();
    packetOffsets_.clear();
    commandCount_ = 0;
    nextSequence_ = 0;
}

void RenderCommandStream::Reserve(std::size_t bytes) {
    bytes_.reserve(bytes);
}

std::size_t RenderCommandStream::SizeBytes() const noexcept {
    return bytes_.size();
}

std::size_t RenderCommandStream::CommandCount() const noexcept {
    return commandCount_;
}

std::span<const std::uint8_t> RenderCommandStream::Bytes() const noexcept {
    return std::span<const std::uint8_t>(bytes_.data(), bytes_.size());
}

std::vector<std::size_t> RenderCommandStream::BuildSortedPacketOrder() const {
    struct SortRecord {
        std::size_t packetIndex = 0;
        std::uint64_t sortKey = 0;
        std::uint32_t sequence = 0;
    };

    std::vector<std::size_t> order(packetOffsets_.size());
    std::iota(order.begin(), order.end(), 0U);

    std::vector<SortRecord> sortRecords(order.size());
    for (std::size_t packetIndex = 0; packetIndex < packetOffsets_.size(); ++packetIndex) {
        const std::size_t packetOffset = packetOffsets_[packetIndex];
        RenderCommandHeader header{};
        if (!SizeAddWithin(packetOffset, sizeof(RenderCommandHeader), bytes_.size())) {
            sortRecords[packetIndex] = SortRecord{.packetIndex = packetIndex, .sortKey = 0, .sequence = 0};
            continue;
        }
        std::memcpy(&header, bytes_.data() + packetOffset, sizeof(RenderCommandHeader));
        if (!SizeAddWithin(packetOffset + sizeof(RenderCommandHeader),
                           static_cast<std::size_t>(header.sizeBytes),
                           bytes_.size())) {
            sortRecords[packetIndex] = SortRecord{.packetIndex = packetIndex, .sortKey = 0, .sequence = 0};
            continue;
        }
        sortRecords[packetIndex] = SortRecord{
            .packetIndex = packetIndex,
            .sortKey = header.sortKey,
            .sequence = header.sequence,
        };
    }

    std::sort(sortRecords.begin(), sortRecords.end(), [](const SortRecord& lhs, const SortRecord& rhs) {
        if (lhs.sortKey != rhs.sortKey) {
            return lhs.sortKey < rhs.sortKey;
        }
        return lhs.sequence < rhs.sequence;
    });

    for (std::size_t orderIndex = 0; orderIndex < sortRecords.size(); ++orderIndex) {
        order[orderIndex] = sortRecords[orderIndex].packetIndex;
    }
    return order;
}

bool RenderCommandStream::ReadPacket(std::size_t packetIndex, RenderCommandView& outView) const noexcept {
    if (packetIndex >= packetOffsets_.size()) {
        return false;
    }

    const std::size_t packetOffset = packetOffsets_[packetIndex];
    if (!SizeAddWithin(packetOffset, sizeof(RenderCommandHeader), bytes_.size())) {
        return false;
    }

    RenderCommandHeader header{};
    std::memcpy(&header, bytes_.data() + packetOffset, sizeof(RenderCommandHeader));
    const std::size_t payloadOffset = packetOffset + sizeof(RenderCommandHeader);
    if (!SizeAddWithin(payloadOffset, static_cast<std::size_t>(header.sizeBytes), bytes_.size())) {
        return false;
    }

    outView.header = header;
    outView.payload = bytes_.data() + payloadOffset;
    return true;
}

RenderCommandReader::RenderCommandReader(std::span<const std::uint8_t> bytes) noexcept
    : bytes_(bytes), cursor_(0) {}

bool RenderCommandReader::Next(RenderCommandView& outView) noexcept {
    if (!SizeAddWithin(cursor_, sizeof(RenderCommandHeader), bytes_.size())) {
        return false;
    }

    RenderCommandHeader header{};
    std::memcpy(&header, bytes_.data() + cursor_, sizeof(RenderCommandHeader));

    const std::size_t packetStart = cursor_ + sizeof(RenderCommandHeader);
    if (!SizeAddWithin(packetStart, static_cast<std::size_t>(header.sizeBytes), bytes_.size())) {
        cursor_ = bytes_.size();
        return false;
    }

    outView.header = header;
    outView.payload = bytes_.data() + packetStart;
    cursor_ = packetStart + static_cast<std::size_t>(header.sizeBytes);
    return true;
}

bool RenderCommandReader::Exhausted() const noexcept {
    return cursor_ >= bytes_.size();
}

} // namespace ri::core
