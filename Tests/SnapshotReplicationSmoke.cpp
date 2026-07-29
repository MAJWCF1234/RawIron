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

    ri::runtime::SnapshotReplicator sender{};
    bool usedDelta = true;
    ri::runtime::SnapshotDeltaPacket full = sender.BuildForPeer(1U, baseline, usedDelta);
    if (usedDelta || full.encodedOps != baseline.bytes) {
        return EXIT_FAILURE;
    }
    ri::runtime::SnapshotReplicator receiver{};
    const ri::runtime::SnapshotBlob noFallback{};
    const std::optional<ri::runtime::SnapshotBlob> accepted =
        receiver.ApplyFromServer(1U, noFallback, full);
    if (!accepted.has_value() || accepted->bytes != baseline.bytes) {
        return EXIT_FAILURE;
    }

    full.encodedOps.front() ^= 0x80U;
    if (receiver.ApplyFromServer(2U, noFallback, full).has_value()
        || receiver.Stats().rejectedSnapshots != 1U) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
