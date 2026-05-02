#pragma once

#include "RawIron/Trace/TraceScene.h"

#include <algorithm>
#include <optional>
#include <span>
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

/// Rounded-hull / compound collider queries (legacy web: `traceCompoundBoxes` / `traceCompoundBoxesSweep`).
/// `QueryCompoundTraceBox` returns the first blocking hit in `sampleBoxes` order (short-circuit), matching
/// the browser prototype. `QueryCompoundTraceSweptBox` returns the sweep with the smallest hit time across
/// samples. `QueryCompoundSlideMoveBox` matches `slideMoveCompoundBoxes`: slide using only the primary
/// (first) sample AABB, as in the web player.
[[nodiscard]] std::optional<TraceHit> QueryCompoundTraceBox(const TraceScene& scene,
                                                            std::span<const ri::spatial::Aabb> sampleBoxes,
                                                            const TraceOptions& options = {});

[[nodiscard]] std::optional<TraceHit> QueryCompoundTraceSweptBox(const TraceScene& scene,
                                                                 std::span<const ri::spatial::Aabb> sampleBoxes,
                                                                 const ri::math::Vec3& delta,
                                                                 const TraceOptions& options = {});

[[nodiscard]] SlideMoveResult QueryCompoundSlideMoveBox(const TraceScene& scene,
                                                        std::span<const ri::spatial::Aabb> sampleBoxes,
                                                        const ri::math::Vec3& delta,
                                                        std::size_t maxBumps = 4,
                                                        float epsilon = 0.001f,
                                                        const TraceOptions& options = {});

/// Multi-offset downward probe under the feet (legacy web: `Player.probeGroundAtFeet`).
/// Uses five XZ offsets at `0.55 * hullRadius`, lifts each probe, casts down with `FindGroundHit`,
/// then picks the candidate with the smallest `|supportDelta|` (tie-break: higher hit point).
struct GroundFeetProbeOptions {
    float maxDistance = 1.15f;
    float allowAbove = 0.06f;
    float groundContactOffset = 0.03f;
    float minNormalY = 0.45f;
};

struct GroundFeetProbeResult {
    TraceHit hit{};
    float clearance = 0.0f;
    float supportDelta = 0.0f;
};

[[nodiscard]] inline float GroundProbeAllowAboveCap(const GroundFeetProbeOptions& options) noexcept {
    return std::max(options.groundContactOffset + 0.16f, 0.2f);
}

[[nodiscard]] std::optional<GroundFeetProbeResult> ProbeGroundAtFeet(const TraceScene& scene,
                                                                     const ri::math::Vec3& baseFeet,
                                                                     float hullRadius,
                                                                     const GroundFeetProbeOptions& options = {});

} // namespace ri::trace
