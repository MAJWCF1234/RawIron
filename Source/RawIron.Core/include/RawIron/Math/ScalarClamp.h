#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
namespace ri::math {

/// If `raw` is non-finite, returns `fallback`. Otherwise clamps into `[minValue, maxValue]`.
/// When `minValue > maxValue`, bounds are swapped so the interval is always valid.
template<std::floating_point T>
[[nodiscard]] inline T ClampFinite(T raw, T fallback, T minValue, T maxValue) noexcept {
    if (!std::isfinite(raw)) {
        return fallback;
    }
    const T lo = minValue < maxValue ? minValue : maxValue;
    const T hi = minValue < maxValue ? maxValue : minValue;
    return std::clamp(raw, lo, hi);
}

/// `ClampFinite` then `std::llround`, with a final integral clamp (matches proto `clampFiniteInteger`).
[[nodiscard]] inline int ClampFiniteToInt(double raw, int fallback, int minValue, int maxValue) noexcept {
    const double f = static_cast<double>(fallback);
    const double lo = static_cast<double>(minValue);
    const double hi = static_cast<double>(maxValue);
    const double clamped = ClampFinite(raw, f, lo, hi);
    long long rounded = std::llround(clamped);
    const long long loI = static_cast<long long>(minValue < maxValue ? minValue : maxValue);
    const long long hiI = static_cast<long long>(minValue < maxValue ? maxValue : minValue);
    rounded = std::clamp(rounded, loI, hiI);
    return static_cast<int>(rounded);
}

} // namespace ri::math
