#include "RawIron/Runtime/SnapshotReplication.h"

#include <algorithm>
#include <limits>

namespace ri::runtime {
namespace {

[[nodiscard]] std::uint32_t SnapshotChecksum(const std::vector<std::uint8_t>& bytes) {
    std::uint32_t hash = 2166136261U;
    for (const std::uint8_t byte : bytes) {
        hash ^= byte;
        hash *= 16777619U;
    }
    return hash;
}

void WriteVarint(std::vector<std::uint8_t>& out, std::uint32_t value) {
    while (value >= 0x80U) {
        out.push_back(static_cast<std::uint8_t>((value & 0x7FU) | 0x80U));
        value >>= 7U;
    }
    out.push_back(static_cast<std::uint8_t>(value & 0x7FU));
}

bool ReadVarint(const std::vector<std::uint8_t>& in, std::size_t& at, std::uint32_t& value) {
    value = 0;
    std::uint32_t shift = 0;
    while (at < in.size()) {
        const std::uint8_t byte = in[at++];
        value |= (static_cast<std::uint32_t>(byte & 0x7FU) << shift);
        if ((byte & 0x80U) == 0U) {
            return true;
        }
        shift += 7U;
        if (shift > 28U) {
            return false;
        }
    }
    return false;
}

} // namespace

std::optional<SnapshotDeltaPacket> BuildSnapshotDelta(const SnapshotBlob& baseline, const SnapshotBlob& target) {
    if (baseline.tick >= target.tick) {
        return std::nullopt;
    }
    if (baseline.bytes.size() != target.bytes.size()) {
        return std::nullopt;
    }
    if (target.bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }

    SnapshotDeltaPacket packet{};
    packet.baseTick = baseline.tick;
    packet.targetTick = target.tick;
    packet.payloadChecksum = SnapshotChecksum(target.bytes);

    const std::size_t size = baseline.bytes.size();
    std::size_t i = 0;
    while (i < size) {
        if (baseline.bytes[i] == target.bytes[i]) {
            ++i;
            continue;
        }
        const std::size_t start = i;
        while (i < size && baseline.bytes[i] != target.bytes[i]) {
            ++i;
        }
        const std::size_t len = i - start;
        WriteVarint(packet.encodedOps, static_cast<std::uint32_t>(start));
        WriteVarint(packet.encodedOps, static_cast<std::uint32_t>(len));
        packet.encodedOps.insert(packet.encodedOps.end(), target.bytes.begin() + static_cast<std::ptrdiff_t>(start),
                                 target.bytes.begin() + static_cast<std::ptrdiff_t>(i));
    }
    return packet;
}

std::optional<SnapshotBlob> ApplySnapshotDelta(const SnapshotBlob& baseline, const SnapshotDeltaPacket& delta) {
    if (baseline.tick != delta.baseTick || delta.targetTick <= delta.baseTick) {
        return std::nullopt;
    }

    SnapshotBlob rebuilt{};
    rebuilt.tick = delta.targetTick;
    rebuilt.bytes = baseline.bytes;

    std::size_t at = 0;
    while (at < delta.encodedOps.size()) {
        std::uint32_t start = 0;
        std::uint32_t len = 0;
        if (!ReadVarint(delta.encodedOps, at, start) || !ReadVarint(delta.encodedOps, at, len)) {
            return std::nullopt;
        }
        const std::size_t startIndex = static_cast<std::size_t>(start);
        const std::size_t runLength = static_cast<std::size_t>(len);
        if (startIndex > rebuilt.bytes.size() || runLength > rebuilt.bytes.size() - startIndex) {
            return std::nullopt;
        }
        if (at > delta.encodedOps.size() || runLength > delta.encodedOps.size() - at) {
            return std::nullopt;
        }
        std::copy(delta.encodedOps.begin() + static_cast<std::ptrdiff_t>(at),
                  delta.encodedOps.begin() + static_cast<std::ptrdiff_t>(at + runLength),
                  rebuilt.bytes.begin() + static_cast<std::ptrdiff_t>(startIndex));
        at += runLength;
    }
    if (SnapshotChecksum(rebuilt.bytes) != delta.payloadChecksum) {
        return std::nullopt;
    }
    return rebuilt;
}

SnapshotReplicator::SnapshotReplicator(const std::size_t baselineHistory)
    : history_(std::max<std::size_t>(1U, baselineHistory)) {}

SnapshotDeltaPacket SnapshotReplicator::BuildForPeer(const std::size_t peerId,
                                                     const SnapshotBlob& current,
                                                     bool& usedDelta) {
    usedDelta = false;
    SnapshotDeltaPacket packet{};
    packet.targetTick = current.tick;

    auto& history = peerBaselines_[peerId];
    const SnapshotBlob* baseline = history.empty() ? nullptr : &history.back();
    if (baseline != nullptr && baseline->bytes.size() == current.bytes.size() && baseline->tick < current.tick) {
        const auto maybeDelta = BuildSnapshotDelta(*baseline, current);
        if (maybeDelta.has_value()) {
            const std::size_t fullBytes = current.bytes.size();
            const std::size_t deltaBytes = maybeDelta->encodedOps.size();
            if (deltaBytes > 0 && deltaBytes < fullBytes) {
                usedDelta = true;
                packet = *maybeDelta;
                stats_.deltaSnapshots += 1;
                stats_.bytesDelta += deltaBytes;
            }
        }
    }

    if (!usedDelta) {
        packet.baseTick = current.tick;
        packet.targetTick = current.tick;
        packet.payloadChecksum = SnapshotChecksum(current.bytes);
        packet.encodedOps = current.bytes;
        stats_.fullSnapshots += 1;
        stats_.bytesFull += current.bytes.size();
    }

    history.push_back(current);
    while (history.size() > history_) {
        history.pop_front();
    }
    return packet;
}

std::optional<SnapshotBlob> SnapshotReplicator::ApplyFromServer(const std::size_t peerId,
                                                                const SnapshotBlob& fallbackBaseline,
                                                                const SnapshotDeltaPacket& packet) {
    auto& history = peerBaselines_[peerId];
    const SnapshotBlob* baseline = nullptr;
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        if (it->tick == packet.baseTick) {
            baseline = &(*it);
            break;
        }
    }
    if (baseline == nullptr && fallbackBaseline.tick == packet.baseTick) {
        baseline = &fallbackBaseline;
    }

    SnapshotBlob rebuilt{};
    if (packet.baseTick == packet.targetTick) {
        rebuilt.tick = packet.targetTick;
        rebuilt.bytes = packet.encodedOps;
        if (SnapshotChecksum(rebuilt.bytes) != packet.payloadChecksum) {
            stats_.rejectedSnapshots += 1U;
            return std::nullopt;
        }
    } else {
        if (baseline == nullptr) {
            stats_.rejectedSnapshots += 1U;
            return std::nullopt;
        }
        const auto applied = ApplySnapshotDelta(*baseline, packet);
        if (!applied.has_value()) {
            stats_.rejectedSnapshots += 1U;
            return std::nullopt;
        }
        rebuilt = *applied;
    }

    history.push_back(rebuilt);
    while (history.size() > history_) {
        history.pop_front();
    }
    return rebuilt;
}

const SnapshotReplicationStats& SnapshotReplicator::Stats() const noexcept {
    return stats_;
}

} // namespace ri::runtime
