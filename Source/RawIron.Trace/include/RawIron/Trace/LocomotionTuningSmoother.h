#pragma once

#include "RawIron/Trace/LocomotionTuning.h"

#include <cmath>

namespace ri::trace {

/// First-order exponential smoothing toward live tuning targets (avoids pops when cvars/replication change mid-play).
/// Uses a single time constant τ — standard practice for human-perceived continuity of mechanics tuning.
struct LocomotionTuningSmoother {
    LocomotionTuning value{};
    float tauSeconds = 0.18f;
};

[[nodiscard]] inline LocomotionTuning SmoothLocomotionTuningToward(const LocomotionTuning& current,
                                                                   const LocomotionTuning& target,
                                                                   float deltaSeconds,
                                                                   float tauSeconds) noexcept {
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f || !std::isfinite(tauSeconds)) {
        return current;
    }
    const float tau = std::max(tauSeconds, 1e-4f);
    const float a = 1.0f - std::exp(-deltaSeconds / tau);
    LocomotionTuning out = current;
    auto blend = [&](float& dst, float src) {
        dst = dst + ((src - dst) * a);
    };
    blend(out.walkSpeed, target.walkSpeed);
    blend(out.sprintSpeed, target.sprintSpeed);
    blend(out.crouchSpeed, target.crouchSpeed);
    blend(out.proneSpeed, target.proneSpeed);
    blend(out.gravity, target.gravity);
    blend(out.jumpForce, target.jumpForce);
    blend(out.fallGravityMultiplier, target.fallGravityMultiplier);
    blend(out.lowJumpGravityMultiplier, target.lowJumpGravityMultiplier);
    blend(out.maxFallSpeed, target.maxFallSpeed);
    blend(out.maxStepHeight, target.maxStepHeight);
    return out;
}

[[nodiscard]] inline LocomotionTuning AdvanceLocomotionTuningSmoother(LocomotionTuningSmoother& smoother,
                                                                      const LocomotionTuning& target,
                                                                      float deltaSeconds) noexcept {
    smoother.value =
        SmoothLocomotionTuningToward(smoother.value, target, deltaSeconds, smoother.tauSeconds);
    return smoother.value;
}

[[nodiscard]] inline LocomotionTuning ResetLocomotionTuningSmoother(LocomotionTuningSmoother& smoother,
                                                                   const LocomotionTuning& snapshot) noexcept {
    smoother.value = snapshot;
    return smoother.value;
}

} // namespace ri::trace
