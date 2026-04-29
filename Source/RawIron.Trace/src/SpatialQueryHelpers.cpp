#include "RawIron/Trace/SpatialQueryHelpers.h"

#include <algorithm>

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

} // namespace ri::trace
