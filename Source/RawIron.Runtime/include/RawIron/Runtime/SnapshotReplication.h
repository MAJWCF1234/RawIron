#pragma once

#include <cstdint>
#include <cstddef>
#include <deque>
#include <unordered_map>
#include <optional>
#include <span>
#include <vector>

namespace ri::runtime {

struct SnapshotBlob {
    std::uint32_t tick = 0;
    std::vector<std::uint8_t> bytes;
};

struct SnapshotDeltaPacket {
    std::uint32_t baseTick = 0;
    std::uint32_t targetTick = 0;
    std::vector<std::uint8_t> encodedOps;
};

/// Bitpacking/varint delta codec for authoritative state snapshots.
/// Operations are run-based byte patch commands against a baseline snapshot.
[[nodiscard]] std::optional<SnapshotDeltaPacket> BuildSnapshotDelta(const SnapshotBlob& baseline,
                                                                    const SnapshotBlob& target);

[[nodiscard]] std::optional<SnapshotBlob> ApplySnapshotDelta(const SnapshotBlob& baseline,
                                                             const SnapshotDeltaPacket& delta);

struct SnapshotReplicationStats {
    std::uint64_t fullSnapshots = 0;
    std::uint64_t deltaSnapshots = 0;
    std::uint64_t bytesFull = 0;
    std::uint64_t bytesDelta = 0;
};

/// Maintains per-peer baseline history and produces either full or delta snapshots.
class SnapshotReplicator {
public:
    explicit SnapshotReplicator(std::size_t baselineHistory = 64);

    [[nodiscard]] SnapshotDeltaPacket BuildForPeer(std::size_t peerId,
                                                   const SnapshotBlob& current,
                                                   bool& usedDelta);
    [[nodiscard]] std::optional<SnapshotBlob> ApplyFromServer(std::size_t peerId,
                                                              const SnapshotBlob& fallbackBaseline,
                                                              const SnapshotDeltaPacket& packet);
    [[nodiscard]] const SnapshotReplicationStats& Stats() const noexcept;

private:
    std::size_t history_ = 64;
    std::unordered_map<std::size_t, std::deque<SnapshotBlob>> peerBaselines_{};
    SnapshotReplicationStats stats_{};
};

} // namespace ri::runtime
