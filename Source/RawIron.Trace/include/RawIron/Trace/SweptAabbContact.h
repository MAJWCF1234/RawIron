#pragma once

#include "RawIron/Trace/AabbOverlapContact.h"
#include "RawIron/Trace/TraceScene.h"

#include <optional>
#include <string>
#include <string_view>

namespace ri::trace {

/// Swept AABB vs static AABB (CCD-style interval along \p delta). Overlap at the start returns \p time == 0 with
/// overlap resolution from \ref ComputeAabbOverlapTraceHit.
[[nodiscard]] std::optional<TraceHit> ComputeSweptAabbTraceHit(const ri::spatial::Aabb& queryBox,
                                                               const ri::math::Vec3& delta,
                                                               const ri::spatial::Aabb& staticBounds,
                                                               std::string obstacleId);

/// Narrow-phase helper for a single static obstacle (same math as \ref TraceScene::TraceSweptBox for one collider).
[[nodiscard]] std::optional<TraceHit> ComputeSweptAabbTraceHit(const ri::spatial::Aabb& queryBox,
                                                               const ri::math::Vec3& delta,
                                                               const ri::spatial::Aabb& staticBounds,
                                                               std::string_view obstacleId);

} // namespace ri::trace
