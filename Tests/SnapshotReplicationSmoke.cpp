#include "RawIron/Runtime/SnapshotReplication.h"

#include <cstdlib>
#include <vector>

int main() {
    ri::runtime::SnapshotBlob baseline{
        .tick = 10U,
        .bytes = std::vector<std::uint8_t>(1024U, 0U),
    };
    ri::runtime::SnapshotBlob target = baseline;
    target.tick = 11U;
    for (std::size_t index = 100U; index < 120U; ++index) {
        target.bytes[index] = static_cast<std::uint8_t>(index);
    }
    target.bytes[900U] = 0xA5U;

    const std::optional<ri::runtime::SnapshotDeltaPacket> delta =
        ri::runtime::BuildSnapshotDelta(baseline, target);
    if (!delta.has_value() || delta->encodedOps.empty()) {
        return EXIT_FAILURE;
    }
    const std::optional<ri::runtime::SnapshotBlob> rebuilt =
        ri::runtime::ApplySnapshotDelta(baseline, *delta);
    if (!rebuilt.has_value() || rebuilt->tick != target.tick || rebuilt->bytes != target.bytes) {
        return EXIT_FAILURE;
    }

    ri::runtime::SnapshotDeltaPacket corruptedDelta = *delta;
    corruptedDelta.encodedOps.back() ^= 0x01U;
    if (ri::runtime::ApplySnapshotDelta(baseline, corruptedDelta).has_value()) {
        return EXIT_FAILURE;
    }
    ri::runtime::SnapshotDeltaPacket malformed = *delta;
    malformed.encodedOps = {0x80U, 0x80U, 0x80U, 0x80U, 0x80U};
    if (ri::runtime::ApplySnapshotDelta(baseline, malformed).has_value()) {
        return EXIT_FAILURE;
    }
    ri::runtime::SnapshotDeltaPacket outOfBounds = *delta;
    // start=2048, length=1, payload=0xff.
    outOfBounds.encodedOps = {0x80U, 0x10U, 0x01U, 0xFFU};
    if (ri::runtime::ApplySnapshotDelta(baseline, outOfBounds).has_value()) {
        return EXIT_FAILURE;
    }
    ri::runtime::SnapshotDeltaPacket trailingGarbage = *delta;
    trailingGarbage.encodedOps.push_back(0x00U);
    if (ri::runtime::ApplySnapshotDelta(baseline, trailingGarbage).has_value()) {
        return EXIT_FAILURE;
    }
    ri::runtime::SnapshotDeltaPacket zeroLengthRun = *delta;
    // start=0, length=0 — reject empty patch runs even if checksum matches baseline.
    zeroLengthRun.encodedOps = {0x00U, 0x00U};
    zeroLengthRun.targetTick = baseline.tick + 1U;
    zeroLengthRun.payloadChecksum = delta->payloadChecksum; // may not match; reject must precede checksum
    // Force checksum to match an unchanged baseline so rejection is for len==0, not hash.
    {
        std::uint32_t hash = 2166136261U;
        for (const std::uint8_t byte : baseline.bytes) {
            hash ^= byte;
            hash *= 16777619U;
        }
        zeroLengthRun.payloadChecksum = hash;
        zeroLengthRun.baseTick = baseline.tick;
    }
    if (ri::runtime::ApplySnapshotDelta(baseline, zeroLengthRun).has_value()) {
        return EXIT_FAILURE;
    }

    ri::runtime::SnapshotReplicator sender{};
    bool usedDelta = true;
    ri::runtime::SnapshotDeltaPacket full = sender.BuildForPeer(1U, baseline, usedDelta);
    if (usedDelta || full.encodedOps != baseline.bytes) {
        return EXIT_FAILURE;
    }
    // Baseline advances only after a successful delivery path.
    sender.RememberPeerBaseline(1U, baseline);

    bool usedDeltaAfterRemember = false;
    const ri::runtime::SnapshotDeltaPacket second =
        sender.BuildForPeer(1U, target, usedDeltaAfterRemember);
    if (!usedDeltaAfterRemember || second.baseTick != baseline.tick || second.targetTick != target.tick) {
        return EXIT_FAILURE;
    }
    // Encode/send failure must not advance history — rebuild against the same baseline.
    bool usedDeltaAfterFailedSend = false;
    const ri::runtime::SnapshotDeltaPacket retry =
        sender.BuildForPeer(1U, target, usedDeltaAfterFailedSend);
    if (!usedDeltaAfterFailedSend || retry.baseTick != baseline.tick) {
        return EXIT_FAILURE;
    }
    sender.RememberPeerBaseline(1U, target);

    ri::runtime::SnapshotReplicator receiver{};
    const ri::runtime::SnapshotBlob noFallback{};
    const std::optional<ri::runtime::SnapshotBlob> accepted =
        receiver.ApplyFromServer(1U, noFallback, full);
    if (!accepted.has_value() || accepted->bytes != baseline.bytes) {
        return EXIT_FAILURE;
    }
    // Apply does not commit; domain-valid callers must Remember explicitly.
    if (receiver.TrackedPeerCount() != 0U) {
        return EXIT_FAILURE;
    }
    receiver.RememberPeerBaseline(1U, *accepted);
    if (receiver.TrackedPeerCount() != 1U) {
        return EXIT_FAILURE;
    }

    // Domain-invalid payload may decode at the replicator layer; callers must not Remember.
    const ri::runtime::SnapshotBlob shortBlob{
        .tick = 99U,
        .bytes = {0x01U, 0x02U, 0x03U},
    };
    bool unusedDelta = false;
    ri::runtime::SnapshotReplicator packetMaker{};
    const ri::runtime::SnapshotDeltaPacket shortFull =
        packetMaker.BuildForPeer(0U, shortBlob, unusedDelta);
    const std::optional<ri::runtime::SnapshotBlob> shortAccepted =
        receiver.ApplyFromServer(3U, noFallback, shortFull);
    if (!shortAccepted.has_value() || receiver.TrackedPeerCount() != 1U) {
        return EXIT_FAILURE;
    }
    receiver.DiscardLatestPeerBaseline(3U, shortAccepted->tick); // no-op if never remembered
    if (receiver.TrackedPeerCount() != 1U) {
        return EXIT_FAILURE;
    }

    full.encodedOps.front() ^= 0x80U;
    if (receiver.ApplyFromServer(2U, noFallback, full).has_value()
        || receiver.Stats().rejectedSnapshots != 1U) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
