#pragma once

#include "RawIron/Math/Vec3.h"
#include "RawIron/Scene/SemanticStructuralPartition.h"

#include <cstddef>

namespace ri::scene {

struct InteractionStructuralGateResult {
    /// True when the interaction may proceed (or the gate was skipped).
    bool eligible = true;
    /// True when a closer QueryMesh+Interaction structural hit blocked the target.
    bool blockedByStructural = false;
    /// False when there was no target or no structural Interaction geometry to evaluate.
    bool evaluated = false;
    float structuralHitDistance = 0.0f;
    /// Look-ray parameter of the target center (`dot(target - origin, normalize(forward))`).
    float targetRayT = 0.0f;
    std::size_t partitionEntryCount = 0U;
};

/// Blocks activation when a QueryMesh + Interaction structural hit lies strictly in front of
/// the target along the look ray. `targetPosition` is the interaction entity's world center;
/// the far clip is that point's projection on `forward`, not euclidean distance, so overlap
/// targets beside the camera are not tested through distant walls. `ignoreNodeHandle` skips
/// that node's mesh so a structural interactable cannot occlude its own center. Does not
/// mutate selection; callers keep legacy ResolveInteractionTarget behavior and only suppress
/// activation when `blockedByStructural` is true. When `partition` is null, a one-off
/// partition is built from `scene`.
[[nodiscard]] InteractionStructuralGateResult EvaluateInteractionStructuralGate(
    const Scene& scene,
    const ri::math::Vec3& origin,
    const ri::math::Vec3& forward,
    const ri::math::Vec3& targetPosition,
    bool hasTarget,
    int ignoreNodeHandle = kInvalidHandle,
    const SemanticStructuralPartition* partition = nullptr);

} // namespace ri::scene
