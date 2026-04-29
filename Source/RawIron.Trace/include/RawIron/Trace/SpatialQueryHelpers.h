#pragma once

#include "RawIron/Trace/TraceScene.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::trace {

struct QuerySweepRequest {
    ri::spatial::Aabb bounds = ri::spatial::MakeEmptyAabb();
    ri::math::Vec3 delta{};
    TraceOptions options{};
};

struct SpatialQueryFilter {
    bool structuralOnly = false;
    std::string idPrefix;
};

[[nodiscard]] std::optional<TraceHit> QueryBlockingRay(const TraceScene& scene,
                                                       const ri::math::Vec3& origin,
                                                       const ri::math::Vec3& direction,
                                                       float distance,
                                                       const TraceOptions& options = {});

[[nodiscard]] std::optional<TraceHit> QueryBlockingBox(const TraceScene& scene,
                                                       const ri::spatial::Aabb& bounds,
                                                       const TraceOptions& options = {});

[[nodiscard]] std::optional<TraceHit> QueryBlockingSweep(const TraceScene& scene,
                                                         const QuerySweepRequest& request);
[[nodiscard]] std::vector<std::string> QueryDeterministicBoxIds(const TraceScene& scene,
                                                                const ri::spatial::Aabb& box,
                                                                const SpatialQueryFilter& filter = {});
[[nodiscard]] std::vector<std::string> QueryDeterministicRayIds(const TraceScene& scene,
                                                                const ri::math::Vec3& origin,
                                                                const ri::math::Vec3& direction,
                                                                float distance,
                                                                const SpatialQueryFilter& filter = {});

} // namespace ri::trace
