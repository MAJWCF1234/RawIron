#pragma once

#include "RawIron/Math/Vec3.h"
#include "RawIron/Spatial/Aabb.h"
#include "RawIron/Trace/TraceScene.h"

#include <optional>
#include <string>

namespace ri::trace {

/// Full axis-aligned separation contact for two overlapping AABBs (query vs static obstacle).
struct AabbOverlapContact {
    std::string obstacleId;
    ri::spatial::Aabb queryBox{};
    ri::spatial::Aabb obstacleBounds{};
    /// MTD depth along the chosen separation axis (world units).
    float penetrationDepth = 0.0f;
    /// 0 = X, 1 = Y, 2 = Z for the minimum-penetration axis of the intersection box.
    int separationAxis = 0;
    /// True if the free direction (query pushed out) is along the positive world axis.
    bool separationPositive = true;
    /// Unit normal on the obstacle face, pointing from the obstacle toward the free half-space (MTD direction).
    ri::math::Vec3 separationNormal{};
    /// Stable point on the obstacle’s **box** face; use \ref RefineAabbOverlapContactNormal for mesh-driven normals.
    ri::math::Vec3 contactPointOnObstacleFace{};
};

[[nodiscard]] std::optional<AabbOverlapContact> ComputeAabbOverlapContact(const ri::spatial::Aabb& queryBox,
                                                                          const ri::spatial::Aabb& obstacleBounds,
                                                                          std::string obstacleId);

[[nodiscard]] TraceHit TraceHitFromAabbOverlapContact(const AabbOverlapContact& contact);

/// Replace the box normal with a triangle/mesh normal from rendered geometry (still axis-aligned separation depth).
[[nodiscard]] AabbOverlapContact RefineAabbOverlapContactNormal(const AabbOverlapContact& contact,
                                                                ri::math::Vec3 meshNormalWorld);

[[nodiscard]] std::optional<TraceHit> ComputeAabbOverlapTraceHit(const ri::spatial::Aabb& queryBox,
                                                                 const ri::spatial::Aabb& staticBounds,
                                                                 std::string obstacleId);

} // namespace ri::trace
