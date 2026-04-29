#pragma once

#include "RawIron/Math/Vec3.h"

#include <cmath>

namespace ri::debug {

/// Finite-safe rounding for JSON / debug snapshots (non-finite or negative `decimalPlaces` → 0).
/// Caps fraction digits so `pow10` stays well-behaved. Proto: `engine/snapshotRoundShim.js`.
[[nodiscard]] inline float RoundSnapshotComponent(float value, int decimalPlaces) noexcept {
    if (!std::isfinite(value) || decimalPlaces < 0) {
        return 0.0f;
    }
    int places = decimalPlaces;
    if (places > 15) {
        places = 15;
    }
    const double scale = std::pow(10.0, static_cast<double>(places));
    return static_cast<float>(std::round(static_cast<double>(value) * scale) / scale);
}

[[nodiscard]] inline double RoundSnapshotScalar(double value, int decimalPlaces) noexcept {
    if (!std::isfinite(value) || decimalPlaces < 0) {
        return 0.0;
    }
    int places = decimalPlaces;
    if (places > 15) {
        places = 15;
    }
    const double scale = std::pow(10.0, static_cast<double>(places));
    return std::round(value * scale) / scale;
}

[[nodiscard]] inline ri::math::Vec3 RoundSnapshotVec3(const ri::math::Vec3& value, int decimalPlaces) noexcept {
    return ri::math::Vec3{
        RoundSnapshotComponent(value.x, decimalPlaces),
        RoundSnapshotComponent(value.y, decimalPlaces),
        RoundSnapshotComponent(value.z, decimalPlaces),
    };
}

} // namespace ri::debug
