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
    /// FNV-1a checksum of the fully reconstructed target snapshot.
    std::uint32_t payloadChecksum = 0;
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
    std::uint64_t rejectedSnapshots = 0;
};

/// Maintains per-peer baseline history and produces either full or delta snapshots.
class SnapshotReplicator {
public:
    explicit SnapshotReplicator(std::size_t baselineHistory = 64);

    /// Builds a full or delta packet against the peer's last *remembered* baseline.
    /// Does not advance peer history — call RememberPeerBaseline only after a successful send.
    [[nodiscard]] SnapshotDeltaPacket BuildForPeer(std::size_t peerId,
                                                   const SnapshotBlob& current,
                                                   bool& usedDelta);

    /// Records `current` as the peer's delivered baseline after encode+send succeed.
    void RememberPeerBaseline(std::size_t peerId, const SnapshotBlob& current);

    /// Drops the newest baseline when it matches `tick` (e.g. domain decode rejected after apply).
    void DiscardLatestPeerBaseline(std::size_t peerId, std::uint32_t tick);

    /// Rebuilds a snapshot against remembered baselines. Does not advance history —
    /// call RememberPeerBaseline only after domain validation succeeds.
    [[nodiscard]] std::optional<SnapshotBlob> ApplyFromServer(std::size_t peerId,
                                                              const SnapshotBlob& fallbackBaseline,
                                                              const SnapshotDeltaPacket& packet);

    /// Drops baseline history for a peer that has disconnected.
    void ForgetPeer(std::size_t peerId);

    /// Drops baseline history for every peer not present in `activePeers`. Without this the
    /// baseline map grows without bound across connect/disconnect churn, since peer ids are
    /// never reused.
    void RetainPeers(std::span<const std::size_t> activePeers);

    [[nodiscard]] std::size_t TrackedPeerCount() const noexcept;
    [[nodiscard]] const SnapshotReplicationStats& Stats() const noexcept;

private:
    std::size_t history_ = 64;
    std::unordered_map<std::size_t, std::deque<SnapshotBlob>> peerBaselines_{};
    SnapshotReplicationStats stats_{};
};

} // namespace ri::runtime
