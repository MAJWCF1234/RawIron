#pragma once

#include "RawIron/Runtime/SnapshotReplication.h"
#include "RawIron/World/InteractivePropField.h"

#include <span>
#include <string>

namespace ri::world {

/// Encodes only authoritative dynamic state. Pool identity, shape, mass, and materials remain
/// part of the prechecked game/package contract and are matched by deterministic pool index.
[[nodiscard]] ri::runtime::SnapshotBlob BuildInteractivePropSnapshot(
    std::span<const InteractivePropState> props,
    std::uint32_t tick);

/// Atomically validates and applies a decoded authoritative snapshot. A malformed or mismatched
/// pool never partially mutates local state.
[[nodiscard]] bool ApplyInteractivePropSnapshot(
    const ri::runtime::SnapshotBlob& snapshot,
    std::span<InteractivePropState> props,
    std::string* error = nullptr);

} // namespace ri::world
