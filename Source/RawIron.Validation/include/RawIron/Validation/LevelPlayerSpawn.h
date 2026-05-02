#pragma once

#include "RawIron/Math/Vec3.h"
#include "RawIron/Structural/StructuralGraph.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ri::validation {

struct PlayerStartCandidate {
    std::string id;
    std::string name;
    ri::math::Vec3 feetPosition{};
    /// Degrees, same convention as structural node `rotation` (yaw / pitch / roll as authored).
    ri::math::Vec3 rotationEulerDegrees{};
};

[[nodiscard]] std::size_t CountPlayerStartSpawners(std::span<const ri::structural::StructuralNode> spawners);

[[nodiscard]] std::vector<PlayerStartCandidate> CollectPlayerStartSpawners(
    std::span<const ri::structural::StructuralNode> spawners);

/// First `player_start` in span order (legacy web: `spawners.find`); deterministic for tooling.
[[nodiscard]] std::optional<PlayerStartCandidate> TryGetPrimaryPlayerStart(
    std::span<const ri::structural::StructuralNode> spawners);

/// Optional CI gate: many pipelines assume a single authored spawn. Returns an error message when
/// more than one `player_start` exists (case-insensitive type match).
[[nodiscard]] std::optional<std::string> ValidateAtMostOnePlayerStart(
    std::span<const ri::structural::StructuralNode> spawners,
    std::string_view contextLabel = "spawners");

/// Deterministic search ladder used by `placePlayerAtSpawn` recovery (mirrors web shell parity).
/// Index 0 is the authored position; subsequent entries radiate horizontally and lift vertically.
[[nodiscard]] std::span<const ri::math::Vec3> PlayerSpawnRecoveryOffsets() noexcept;

} // namespace ri::validation
