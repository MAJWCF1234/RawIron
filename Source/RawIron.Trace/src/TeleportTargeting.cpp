#include "RawIron/Trace/TeleportTargeting.h"

#include "RawIron/Spatial/Aabb.h"

#include <algorithm>
#include <cmath>

namespace ri::trace {
namespace {

bool IsFinite(const ri::math::Vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

} // namespace

TeleportTargetingResult ResolveTeleportTarget(
    const TraceScene& traceScene,
    const ri::math::Vec3& origin,
    const ri::math::Vec3& direction,
    const TeleportTargetingOptions& options) {
    TeleportTargetingResult result{};
    if (!IsFinite(origin) || !IsFinite(direction) || !IsFinite(options.gravity)
        || !IsFinite(options.playerHalfExtents) || !std::isfinite(options.launchSpeed)
        || !std::isfinite(options.maximumDurationSeconds)
        || !std::isfinite(options.minimumLandingNormalY)
        || !std::isfinite(options.landingSurfaceInset)
        || options.launchSpeed <= 0.0f || ri::math::LengthSquared(direction) <= 1.0e-10f) {
        return result;
    }
    const float duration = std::clamp(options.maximumDurationSeconds, 0.1f, 5.0f);
    const std::uint32_t segmentCount = std::clamp(options.segments, 4U, 256U);
    const float stepSeconds = duration / static_cast<float>(segmentCount);
    const ri::math::Vec3 launchVelocity = ri::math::Normalize(direction) * options.launchSpeed;
    result.arcPoints.reserve(static_cast<std::size_t>(segmentCount) + 1U);
    result.arcPoints.push_back(origin);
    ri::math::Vec3 previous = origin;
    for (std::uint32_t segment = 1U; segment <= segmentCount; ++segment) {
        const float time = stepSeconds * static_cast<float>(segment);
        const ri::math::Vec3 next = origin + launchVelocity * time
            + options.gravity * (0.5f * time * time);
        const ri::math::Vec3 delta = next - previous;
        const float distance = ri::math::Length(delta);
        if (distance <= 1.0e-6f) continue;
        const std::optional<TraceHit> hit = traceScene.TraceRay(
            previous, delta / distance, distance, {.structuralOnly = true});
        if (!hit.has_value()) {
            result.arcPoints.push_back(next);
            previous = next;
            continue;
        }
        result.hit = true;
        result.hitPoint = hit->point;
        result.hitNormal = hit->normal;
        result.colliderId = hit->id;
        result.arcPoints.push_back(hit->point);
        const float inset = std::clamp(options.landingSurfaceInset, 0.001f, 0.20f);
        result.destinationFeet = hit->point + ri::math::Vec3{0.0f, inset, 0.0f};
        const ri::math::Vec3 halfExtents{
            std::clamp(std::fabs(options.playerHalfExtents.x), 0.05f, 2.0f),
            std::clamp(std::fabs(options.playerHalfExtents.y), 0.25f, 3.0f),
            std::clamp(std::fabs(options.playerHalfExtents.z), 0.05f, 2.0f)};
        const ri::math::Vec3 center = result.destinationFeet
            + ri::math::Vec3{0.0f, halfExtents.y, 0.0f};
        const ri::spatial::Aabb standingBounds{
            .min = center - halfExtents,
            .max = center + halfExtents};
        result.validLanding = hit->normal.y >= std::clamp(
            options.minimumLandingNormalY, 0.0f, 1.0f)
            && !traceScene.TraceBox(standingBounds, {
                .structuralOnly = true,
                .ignoreId = hit->id}).has_value();
        return result;
    }
    return result;
}

} // namespace ri::trace
