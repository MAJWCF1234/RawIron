#include "RawIron/Trace/SweptAabbContact.h"

#include <cmath>
#include <limits>

namespace ri::trace {
namespace {

ri::spatial::Aabb TranslateBox(const ri::spatial::Aabb& box, const ri::math::Vec3& delta) {
    if (ri::spatial::IsEmpty(box)) {
        return box;
    }
    return ri::spatial::Aabb{
        .min = box.min + delta,
        .max = box.max + delta,
    };
}

float ClampFloat(float value, float minValue, float maxValue) {
    return std::max(minValue, std::min(maxValue, value));
}

} // namespace

std::optional<TraceHit> ComputeSweptAabbTraceHit(const ri::spatial::Aabb& queryBox,
                                               const ri::math::Vec3& delta,
                                               const ri::spatial::Aabb& staticBounds,
                                               std::string obstacleId) {
    if (ri::spatial::Intersects(queryBox, staticBounds)) {
        std::optional<TraceHit> overlap = ComputeAabbOverlapTraceHit(queryBox, staticBounds, obstacleId);
        if (!overlap.has_value()) {
            return std::nullopt;
        }
        overlap->time = 0.0f;
        overlap->endBox = queryBox;
        return overlap;
    }

    float entryTime = -std::numeric_limits<float>::infinity();
    float exitTime = std::numeric_limits<float>::infinity();
    char hitAxis = 'x';
    float hitSign = 0.0f;

    auto updateAxis = [&](char axis,
                          float boxMin,
                          float boxMax,
                          float obstacleMin,
                          float obstacleMax,
                          float velocity) {
        if (std::fabs(velocity) < 1e-8f) {
            return !(boxMax <= obstacleMin || boxMin >= obstacleMax);
        }

        const float invVelocity = 1.0f / velocity;
        float axisEntry = velocity > 0.0f ? (obstacleMin - boxMax) * invVelocity : (obstacleMax - boxMin) * invVelocity;
        float axisExit = velocity > 0.0f ? (obstacleMax - boxMin) * invVelocity : (obstacleMin - boxMax) * invVelocity;
        if (axisEntry > axisExit) {
            std::swap(axisEntry, axisExit);
        }

        if (axisEntry > entryTime) {
            entryTime = axisEntry;
            hitAxis = axis;
            hitSign = velocity > 0.0f ? -1.0f : 1.0f;
        }
        exitTime = std::min(exitTime, axisExit);
        return entryTime <= exitTime;
    };

    if (!updateAxis('x', queryBox.min.x, queryBox.max.x, staticBounds.min.x, staticBounds.max.x, delta.x)
        || !updateAxis('y', queryBox.min.y, queryBox.max.y, staticBounds.min.y, staticBounds.max.y, delta.y)
        || !updateAxis('z', queryBox.min.z, queryBox.max.z, staticBounds.min.z, staticBounds.max.z, delta.z)) {
        return std::nullopt;
    }

    if (entryTime < 0.0f || entryTime > 1.0f || exitTime < 0.0f) {
        return std::nullopt;
    }

    const ri::spatial::Aabb endBox = TranslateBox(queryBox, delta * entryTime);
    const ri::math::Vec3 movedCenter = ri::spatial::Center(endBox);
    ri::math::Vec3 normal{};
    ri::math::Vec3 point = movedCenter;
    if (hitAxis == 'x') {
        normal = {hitSign != 0.0f ? hitSign : 1.0f, 0.0f, 0.0f};
        point.x = hitSign > 0.0f ? staticBounds.max.x : staticBounds.min.x;
        point.y = ClampFloat(movedCenter.y, staticBounds.min.y, staticBounds.max.y);
        point.z = ClampFloat(movedCenter.z, staticBounds.min.z, staticBounds.max.z);
    } else if (hitAxis == 'y') {
        normal = {0.0f, hitSign != 0.0f ? hitSign : 1.0f, 0.0f};
        point.y = hitSign > 0.0f ? staticBounds.max.y : staticBounds.min.y;
        point.x = ClampFloat(movedCenter.x, staticBounds.min.x, staticBounds.max.x);
        point.z = ClampFloat(movedCenter.z, staticBounds.min.z, staticBounds.max.z);
    } else {
        normal = {0.0f, 0.0f, hitSign != 0.0f ? hitSign : 1.0f};
        point.z = hitSign > 0.0f ? staticBounds.max.z : staticBounds.min.z;
        point.x = ClampFloat(movedCenter.x, staticBounds.min.x, staticBounds.max.x);
        point.y = ClampFloat(movedCenter.y, staticBounds.min.y, staticBounds.max.y);
    }

    return TraceHit{
        .id = std::move(obstacleId),
        .bounds = staticBounds,
        .point = point,
        .normal = normal,
        .penetration = 0.0f,
        .time = entryTime,
        .endBox = endBox,
    };
}

std::optional<TraceHit> ComputeSweptAabbTraceHit(const ri::spatial::Aabb& queryBox,
                                               const ri::math::Vec3& delta,
                                               const ri::spatial::Aabb& staticBounds,
                                               std::string_view obstacleId) {
    return ComputeSweptAabbTraceHit(queryBox, delta, staticBounds, std::string(obstacleId));
}

} // namespace ri::trace
