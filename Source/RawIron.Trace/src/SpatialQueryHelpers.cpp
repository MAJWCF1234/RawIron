#include "RawIron/Trace/SpatialQueryHelpers.h"

namespace ri::trace {

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

} // namespace ri::trace
