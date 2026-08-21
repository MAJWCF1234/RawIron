#pragma once

#include "RawIron/Math/Vec3.h"
#include "RawIron/Trace/TraceScene.h"

#include <string>
#include <vector>

namespace ri::trace {

struct TeleportTargetingOptions {
    float launchSpeed = 8.0f;
    ri::math::Vec3 gravity{0.0f, -9.81f, 0.0f};
    float maximumDurationSeconds = 2.0f;
    std::uint32_t segments = 40U;
    float minimumLandingNormalY = 0.70f;
    ri::math::Vec3 playerHalfExtents{0.25f, 0.90f, 0.25f};
    float landingSurfaceInset = 0.02f;
};

struct TeleportTargetingResult {
    bool hit = false;
    bool validLanding = false;
    ri::math::Vec3 hitPoint{};
    ri::math::Vec3 hitNormal{};
    ri::math::Vec3 destinationFeet{};
    std::string colliderId{};
    std::vector<ri::math::Vec3> arcPoints{};
};

/// Samples a bounded ballistic arc against TraceScene and validates a standing player volume at
/// the first hit. Rendering and input remain host concerns; every host receives the same result.
[[nodiscard]] TeleportTargetingResult ResolveTeleportTarget(
    const TraceScene& traceScene,
    const ri::math::Vec3& origin,
    const ri::math::Vec3& direction,
    const TeleportTargetingOptions& options = {});

} // namespace ri::trace
