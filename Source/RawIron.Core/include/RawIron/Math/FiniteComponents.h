#pragma once

#include "RawIron/Math/Vec3.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace ri::math {

/// Matches proto `finiteVec3Components`: if `arr` has fewer than three floats, reads from `fallback` as the base tuple,
/// then replaces any non-finite component with the corresponding `fallback` axis.
[[nodiscard]] inline Vec3 FiniteVec3FromSpan(std::span<const float> arr, const Vec3& fallback) noexcept {
    float bx = fallback.x;
    float by = fallback.y;
    float bz = fallback.z;
    if (arr.size() >= 3) {
        bx = arr[0];
        by = arr[1];
        bz = arr[2];
    }
    return Vec3{
        std::isfinite(bx) ? bx : fallback.x,
        std::isfinite(by) ? by : fallback.y,
        std::isfinite(bz) ? bz : fallback.z,
    };
}

/// Same contract as `finiteVec2Components` with fallback `[fx, fy]`.
[[nodiscard]] inline std::array<float, 2> FiniteVec2FromSpan(std::span<const float> arr, float fx,
                                                             float fy) noexcept {
    float bx = fx;
    float by = fy;
    if (arr.size() >= 2) {
        bx = arr[0];
        by = arr[1];
    }
    return {std::isfinite(bx) ? bx : fx, std::isfinite(by) ? by : fy};
}

[[nodiscard]] inline std::array<float, 2> FiniteVec2FromSpan(std::span<const float> arr,
                                                             const std::array<float, 2>& fallback) noexcept {
    return FiniteVec2FromSpan(arr, fallback[0], fallback[1]);
}

/// Unit quaternion [x,y,z,w]; degenerate input → identity (proto `finiteQuatComponents`).
[[nodiscard]] inline std::array<float, 4> FiniteQuatComponents(std::span<const float> arr) noexcept {
    float qx = 0.0f;
    float qy = 0.0f;
    float qz = 0.0f;
    float qw = 1.0f;
    if (arr.size() >= 4) {
        qx = arr[0];
        qy = arr[1];
        qz = arr[2];
        qw = arr[3];
    }
    if (!std::isfinite(qx)) {
        qx = 0.0f;
    }
    if (!std::isfinite(qy)) {
        qy = 0.0f;
    }
    if (!std::isfinite(qz)) {
        qz = 0.0f;
    }
    if (!std::isfinite(qw)) {
        qw = 1.0f;
    }
    const float lenSq = (qx * qx) + (qy * qy) + (qz * qz) + (qw * qw);
    const float len = std::sqrt(lenSq);
    constexpr float kEps = 1e-8f;
    if (!std::isfinite(len) || len < kEps) {
        return {0.0f, 0.0f, 0.0f, 1.0f};
    }
    const float inv = 1.0f / len;
    return {qx * inv, qy * inv, qz * inv, qw * inv};
}

/// Transform scale: sanitize vec3 then clamp tiny magnitudes to 1 and cap |component| (proto `finiteScaleComponents`).
[[nodiscard]] inline Vec3 FiniteScaleComponents(std::span<const float> arr, const Vec3& fallback) noexcept {
    const Vec3 sc = FiniteVec3FromSpan(arr, fallback);
    constexpr float kMinAbs = 1e-4f;
    constexpr float kMaxAbs = 512.0f;
    auto clampAxis = [](float v) -> float {
        if (!std::isfinite(v) || std::fabs(v) < kMinAbs) {
            return 1.0f;
        }
        if (v > kMaxAbs) {
            return kMaxAbs;
        }
        if (v < -kMaxAbs) {
            return -kMaxAbs;
        }
        return v;
    };
    return Vec3{clampAxis(sc.x), clampAxis(sc.y), clampAxis(sc.z)};
}

} // namespace ri::math
