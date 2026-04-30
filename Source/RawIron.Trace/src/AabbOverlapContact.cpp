#include "RawIron/Trace/AabbOverlapContact.h"

#include <cmath>

namespace ri::trace {
namespace {

ri::spatial::Aabb IntersectionBox(const ri::spatial::Aabb& lhs, const ri::spatial::Aabb& rhs) {
    if (!ri::spatial::Intersects(lhs, rhs)) {
        return ri::spatial::MakeEmptyAabb();
    }
    return ri::spatial::Aabb{
        .min = {
            std::max(lhs.min.x, rhs.min.x),
            std::max(lhs.min.y, rhs.min.y),
            std::max(lhs.min.z, rhs.min.z),
        },
        .max = {
            std::min(lhs.max.x, rhs.max.x),
            std::min(lhs.max.y, rhs.max.y),
            std::min(lhs.max.z, rhs.max.z),
        },
    };
}

float ClampFloat(float value, float minValue, float maxValue) {
    return std::max(minValue, std::min(maxValue, value));
}

} // namespace

std::optional<AabbOverlapContact> ComputeAabbOverlapContact(const ri::spatial::Aabb& queryBox,
                                                            const ri::spatial::Aabb& obstacleBounds,
                                                            std::string obstacleId) {
    const ri::spatial::Aabb intersection = IntersectionBox(queryBox, obstacleBounds);
    if (ri::spatial::IsEmpty(intersection)) {
        return std::nullopt;
    }

    const ri::math::Vec3 size = ri::spatial::Size(intersection);
    if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
        return std::nullopt;
    }

    const ri::math::Vec3 queryCenter = ri::spatial::Center(queryBox);
    const ri::math::Vec3 colliderCenter = ri::spatial::Center(obstacleBounds);
    const ri::math::Vec3 deltaCenter = queryCenter - colliderCenter;

    int axis = 0;
    float penetration = size.x;
    ri::math::Vec3 normal{(deltaCenter.x >= 0.0f ? 1.0f : -1.0f), 0.0f, 0.0f};

    if (size.y < penetration) {
        axis = 1;
        penetration = size.y;
        normal = {0.0f, (deltaCenter.y >= 0.0f ? 1.0f : -1.0f), 0.0f};
    }
    if (size.z < penetration) {
        axis = 2;
        penetration = size.z;
        normal = {0.0f, 0.0f, (deltaCenter.z >= 0.0f ? 1.0f : -1.0f)};
    }

    const bool positive = (axis == 0 ? normal.x : axis == 1 ? normal.y : normal.z) > 0.0f;

    ri::math::Vec3 point = queryCenter;
    if (axis == 0) {
        point.x = normal.x > 0.0f ? obstacleBounds.max.x : obstacleBounds.min.x;
        point.y = ClampFloat(queryCenter.y, obstacleBounds.min.y, obstacleBounds.max.y);
        point.z = ClampFloat(queryCenter.z, obstacleBounds.min.z, obstacleBounds.max.z);
    } else if (axis == 1) {
        point.y = normal.y > 0.0f ? obstacleBounds.max.y : obstacleBounds.min.y;
        point.x = ClampFloat(queryCenter.x, obstacleBounds.min.x, obstacleBounds.max.x);
        point.z = ClampFloat(queryCenter.z, obstacleBounds.min.z, obstacleBounds.max.z);
    } else {
        point.z = normal.z > 0.0f ? obstacleBounds.max.z : obstacleBounds.min.z;
        point.x = ClampFloat(queryCenter.x, obstacleBounds.min.x, obstacleBounds.max.x);
        point.y = ClampFloat(queryCenter.y, obstacleBounds.min.y, obstacleBounds.max.y);
    }

    AabbOverlapContact out{};
    out.obstacleId = std::move(obstacleId);
    out.queryBox = queryBox;
    out.obstacleBounds = obstacleBounds;
    out.penetrationDepth = penetration;
    out.separationAxis = axis;
    out.separationPositive = positive;
    out.separationNormal = normal;
    out.contactPointOnObstacleFace = point;
    return out;
}

TraceHit TraceHitFromAabbOverlapContact(const AabbOverlapContact& contact) {
    return TraceHit{
        .id = contact.obstacleId,
        .bounds = contact.obstacleBounds,
        .point = contact.contactPointOnObstacleFace,
        .normal = contact.separationNormal,
        .penetration = contact.penetrationDepth,
        .time = 0.0f,
        .endBox = contact.queryBox,
    };
}

AabbOverlapContact RefineAabbOverlapContactNormal(const AabbOverlapContact& contact, ri::math::Vec3 meshNormalWorld) {
    AabbOverlapContact refined = contact;
    const float lenSq = ri::math::LengthSquared(meshNormalWorld);
    if (!std::isfinite(lenSq) || lenSq < 1e-20f) {
        return refined;
    }
    ri::math::Vec3 n = ri::math::Normalize(meshNormalWorld);
    if (ri::math::Dot(n, contact.separationNormal) < 0.0f) {
        n = n * -1.0f;
    }
    refined.separationNormal = n;
    return refined;
}

std::optional<TraceHit> ComputeAabbOverlapTraceHit(const ri::spatial::Aabb& queryBox,
                                                   const ri::spatial::Aabb& staticBounds,
                                                   std::string obstacleId) {
    const std::optional<AabbOverlapContact> contact =
        ComputeAabbOverlapContact(queryBox, staticBounds, std::move(obstacleId));
    if (!contact.has_value()) {
        return std::nullopt;
    }
    return TraceHitFromAabbOverlapContact(*contact);
}

} // namespace ri::trace
