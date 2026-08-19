#include "RawIron/Scene/InteractionStructuralGate.h"

#include "RawIron/Math/Vec3.h"
#include "RawIron/Scene/Raycast.h"

#include <algorithm>
#include <cmath>

namespace ri::scene {
namespace {

constexpr float kGateDistanceEpsilon = 0.001f;

[[nodiscard]] ri::math::Vec3 NormalizeForward(const ri::math::Vec3& forward) {
    if (ri::math::LengthSquared(forward) <= 0.00001f) {
        return ri::math::Vec3{0.0f, 0.0f, 1.0f};
    }
    return ri::math::Normalize(forward);
}

} // namespace

InteractionStructuralGateResult EvaluateInteractionStructuralGate(
    const Scene& scene,
    const ri::math::Vec3& origin,
    const ri::math::Vec3& forward,
    const ri::math::Vec3& targetPosition,
    const bool hasTarget,
    const int ignoreNodeHandle,
    const SemanticStructuralPartition* partition) {
    InteractionStructuralGateResult result{};
    if (!hasTarget) {
        return result;
    }

    const ri::math::Vec3 direction = NormalizeForward(forward);
    const float targetRayT = ri::math::Dot(targetPosition - origin, direction);
    result.targetRayT = targetRayT;
    if (!std::isfinite(targetRayT) || targetRayT <= kGateDistanceEpsilon) {
        return result;
    }

    SemanticStructuralPartition ownedPartition;
    const SemanticStructuralPartition* active = partition;
    if (active == nullptr) {
        ownedPartition = BuildSemanticStructuralPartition(scene);
        active = &ownedPartition;
    }
    result.partitionEntryCount = active->Metrics().entryCount;
    if (result.partitionEntryCount == 0U) {
        return result;
    }

    const float farDistance = std::max(targetRayT, kGateDistanceEpsilon);
    const Ray ray{
        .origin = origin,
        .direction = direction,
    };
    const SemanticStructuralPartitionQuery query{
        .channel = StructuralBrushChannel::QueryMesh,
        .queryPurpose = StructuralBrushQueryPurpose::Interaction,
        .ignoreNodeHandle = ignoreNodeHandle,
    };
    const std::optional<SemanticStructuralRaycastHit> hit =
        RaycastSemanticStructuralPartition(scene, *active, ray, farDistance, query);
    result.evaluated = true;
    if (!hit.has_value()) {
        return result;
    }

    result.structuralHitDistance = hit->hit.distance;
    if (hit->hit.distance + kGateDistanceEpsilon < targetRayT) {
        result.blockedByStructural = true;
        result.eligible = false;
    }
    return result;
}

} // namespace ri::scene
