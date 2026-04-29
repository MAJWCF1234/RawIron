#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ri::math {

/// World-yaw helpers for +Y-up scenes that use `atan2(dir.x, dir.z)` (Three.js style).

/// How to reduce a yaw *difference* onto the shortest signed arc between two headings.
enum class YawShortestDeltaMode : std::uint8_t {
    /// O(1); uses `std::remainderf` (IEEE). Preferred for shipped gameplay and matches `protoengine`
    /// `yawMathShim` / typical native math libraries.
    IeeeRemainder = 0,
    /// Legacy iterative wrap into `(-π, π]`. Useful for strict parity with older loop-based sandboxes
    /// or deterministic regression against archived reference captures.
    LegacyIterativePi = 1,
};

[[nodiscard]] inline constexpr float Pi() noexcept {
    return 3.141592653589793238462643383279502884f;
}

[[nodiscard]] inline constexpr float TwoPi() noexcept {
    return Pi() * 2.0f;
}

/// Wraps a yaw or yaw-difference to the IEEE remainder range of `(-π, π]` vs `2π` (O(1); avoids
/// unbounded while-loops used in the legacy web prototype).
[[nodiscard]] inline float NormalizeYawRadians(float radians) noexcept {
    if (!std::isfinite(radians)) {
        return 0.0f;
    }
    return std::remainderf(radians, TwoPi());
}

[[nodiscard]] inline float NormalizeYawRadiansLegacyIterative(float radians) noexcept {
    if (!std::isfinite(radians)) {
        return 0.0f;
    }
    float delta = radians;
    const float pi = Pi();
    const float twoPi = TwoPi();
    while (delta > pi) {
        delta -= twoPi;
    }
    while (delta < -pi) {
        delta += twoPi;
    }
    return delta;
}

/// Shortest signed yaw delta from `fromRadians` to `toRadians` (wraps the difference, not the endpoints).
[[nodiscard]] inline float ShortestYawDeltaRadians(float fromRadians,
                                                   float toRadians,
                                                   YawShortestDeltaMode mode = YawShortestDeltaMode::IeeeRemainder) noexcept {
    const float raw = toRadians - fromRadians;
    if (mode == YawShortestDeltaMode::LegacyIterativePi) {
        return NormalizeYawRadiansLegacyIterative(raw);
    }
    return NormalizeYawRadians(raw);
}

struct YawStepResult {
    float newYaw = 0.0f;
    /// `cos(remaining)` after stepping, clamped to [-1, 1] (matches prototype turn alignment).
    float alignment = 1.0f;
};

/// Steps `currentYaw` toward `desiredYaw` by at most `maxRadiansPerStep` (already multiplied by dt).
[[nodiscard]] inline YawStepResult StepYawToward(float currentYaw,
                                                 float desiredYaw,
                                                 float maxRadiansPerStep,
                                                 YawShortestDeltaMode mode = YawShortestDeltaMode::IeeeRemainder) noexcept {
    YawStepResult out{};
    if (!std::isfinite(currentYaw)) {
        currentYaw = 0.0f;
    }
    if (!std::isfinite(desiredYaw)) {
        out.newYaw = currentYaw;
        out.alignment = 1.0f;
        return out;
    }

    float cap = std::isfinite(maxRadiansPerStep) ? maxRadiansPerStep : 0.001f;
    cap = std::max(0.001f, cap);

    const float delta = ShortestYawDeltaRadians(currentYaw, desiredYaw, mode);
    const float step = std::clamp(delta, -cap, cap);
    out.newYaw = currentYaw + step;
    const float remaining = ShortestYawDeltaRadians(out.newYaw, desiredYaw, mode);
    out.alignment = std::cos(remaining);
    if (!std::isfinite(out.alignment)) {
        out.alignment = 1.0f;
    }
    out.alignment = std::clamp(out.alignment, -1.0f, 1.0f);
    return out;
}

} // namespace ri::math
