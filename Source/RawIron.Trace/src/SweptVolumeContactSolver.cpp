#include "RawIron/Trace/SweptVolumeContactSolver.h"

#include "RawIron/Trace/SweptAabbContact.h"

namespace ri::trace {

std::optional<TraceHit> SweptVolumeContactSolver::ResolveVsStaticAabb(const ri::spatial::Aabb& movingBox,
                                                                      const ri::math::Vec3& delta,
                                                                      const ri::spatial::Aabb& staticBounds,
                                                                      const std::string_view obstacleId) {
    return ComputeSweptAabbTraceHit(movingBox, delta, staticBounds, obstacleId);
}

} // namespace ri::trace
