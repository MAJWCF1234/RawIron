#include "RawIron/Trace/SpatialQueryHelpers.h"

#include <algorithm>
#include <cmath>

namespace ri::trace {
namespace {
void ApplyDeterministicFilter(std::vector<std::string>& ids, const SpatialQueryFilter& filter) {
    if (!filter.idPrefix.empty()) {
        ids.erase(std::remove_if(ids.begin(),
                                 ids.end(),
                                 [&](const std::string& id) {
                                     return id.rfind(filter.idPrefix, 0) != 0;
                                 }),
                  ids.end());
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
}
} // namespace

std::optional<TraceHit> QueryBlockingRay(const TraceScene& scene,
                                         const ri::math::Vec3& origin,
                                         const ri::math::Vec3& direction,
                                         float distance,
                                         const TraceOptions& options) {
    return scene.TraceRay(origin, direction, distance, options);
}

std::optional<TraceHit> QueryBlockingBox(const TraceScene& scene,
                                         const ri::spatial::Aabb& bounds,
                                         const TraceOptions& options) {
    return scene.TraceBox(bounds, options);
}

std::optional<TraceHit> QueryBlockingSweep(const TraceScene& scene, const QuerySweepRequest& request) {
    return scene.TraceSweptBox(request.bounds, request.delta, request.options);
}

std::vector<std::string> QueryDeterministicBoxIds(const TraceScene& scene,
                                                  const ri::spatial::Aabb& box,
                                                  const SpatialQueryFilter& filter) {
    std::vector<std::string> ids = scene.QueryCollidablesForBox(box, filter.structuralOnly);
    ApplyDeterministicFilter(ids, filter);
    return ids;
}

std::vector<std::string> QueryDeterministicRayIds(const TraceScene& scene,
                                                  const ri::math::Vec3& origin,
                                                  const ri::math::Vec3& direction,
                                                  const float distance,
                                                  const SpatialQueryFilter& filter) {
    std::vector<std::string> ids = scene.QueryCollidablesForRay(origin, direction, distance, filter.structuralOnly);
    ApplyDeterministicFilter(ids, filter);
    return ids;
}

std::optional<TraceHit> QueryCompoundTraceBox(const TraceScene& scene,
                                              std::span<const ri::spatial::Aabb> sampleBoxes,
                                              const TraceOptions& options) {
    for (const ri::spatial::Aabb& box : sampleBoxes) {
        if (const std::optional<TraceHit> hit = scene.TraceBox(box, options); hit.has_value()) {
            return hit;
        }
    }
    return std::nullopt;
}

std::optional<TraceHit> QueryCompoundTraceSweptBox(const TraceScene& scene,
                                                   std::span<const ri::spatial::Aabb> sampleBoxes,
                                                   const ri::math::Vec3& delta,
                                                   const TraceOptions& options) {
    std::optional<TraceHit> bestHit;
    for (const ri::spatial::Aabb& box : sampleBoxes) {
        if (const std::optional<TraceHit> hit = scene.TraceSweptBox(box, delta, options); hit.has_value()) {
            if (!bestHit.has_value() || hit->time < bestHit->time) {
                bestHit = hit;
            }
        }
    }
    return bestHit;
}

SlideMoveResult QueryCompoundSlideMoveBox(const TraceScene& scene,
                                         std::span<const ri::spatial::Aabb> sampleBoxes,
                                         const ri::math::Vec3& delta,
                                         const std::size_t maxBumps,
                                         const float epsilon,
                                         const TraceOptions& options) {
    if (sampleBoxes.empty()) {
        // Match web `slideMoveCompoundBoxes` on an empty set: unimpeded motion, no broadphase work.
        return SlideMoveResult{
            .positionDelta = delta,
            .remainingDelta = {},
            .endBox = ri::spatial::MakeEmptyAabb(),
            .hits = {},
            .blocked = false,
        };
    }
    return scene.SlideMoveBox(sampleBoxes.front(), delta, maxBumps, epsilon, options);
}

std::optional<GroundFeetProbeResult> ProbeGroundAtFeet(const TraceScene& scene,
                                                       const ri::math::Vec3& baseFeet,
                                                       float hullRadius,
                                                       const GroundFeetProbeOptions& options) {
    if (!std::isfinite(baseFeet.x) || !std::isfinite(baseFeet.y) || !std::isfinite(baseFeet.z)) {
        return std::nullopt;
    }

    const float radius = std::max(0.0f, hullRadius) * 0.55f;
    const float probeLift = std::max(0.2f, options.maxDistance + 0.1f);
    const float rayFar = probeLift + 0.1f;
    const float allowCap = std::max(options.allowAbove, GroundProbeAllowAboveCap(options));

    struct Offset {
        float x;
        float z;
    };
    const Offset offsets[5] = {
        {0.0f, 0.0f},
        {radius, 0.0f},
        {-radius, 0.0f},
        {0.0f, radius},
        {0.0f, -radius},
    };

    std::optional<GroundFeetProbeResult> best;
    for (const Offset& offset : offsets) {
        const ri::math::Vec3 probePosition{
            baseFeet.x + offset.x,
            baseFeet.y,
            baseFeet.z + offset.z,
        };
        const ri::math::Vec3 rayOrigin{probePosition.x, probePosition.y + probeLift, probePosition.z};
        GroundTraceOptions groundOptions{};
        groundOptions.maxDistance = rayFar;
        groundOptions.minNormalY = options.minNormalY;

        const std::optional<TraceHit> hit = scene.FindGroundHit(rayOrigin, groundOptions);
        if (!hit.has_value()) {
            continue;
        }

        const float relativeY = hit->point.y - baseFeet.y;
        if (relativeY > allowCap) {
            continue;
        }

        const float clearance = std::max(0.0f, baseFeet.y - hit->point.y);
        GroundFeetProbeResult candidate{
            .hit = *hit,
            .clearance = clearance,
            .supportDelta = relativeY,
        };

        if (!best.has_value()) {
            best = candidate;
            continue;
        }

        const float bestGap = std::fabs(best->supportDelta);
        const float candidateGap = std::fabs(relativeY);
        if (candidateGap < bestGap - 1e-4f
            || (std::fabs(candidateGap - bestGap) <= 1e-4f && hit->point.y > best->hit.point.y)) {
            best = candidate;
        }
    }
    return best;
}

} // namespace ri::trace
