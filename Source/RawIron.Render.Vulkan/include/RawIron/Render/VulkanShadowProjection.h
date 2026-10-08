#pragma once

#include "RawIron/Math/Mat4.h"
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace ri::render::vulkan {

// Half-width in metres; smaller coverage improves precision at the cost of range.
inline float ResolveShadowCoverageRadius(float requested) {
    return std::isfinite(requested) ? std::clamp(requested, 8.0f, 256.0f) : 90.0f;
}

// Affine orthographic light VP matrix: the translation column projects world origin.
// Snap that fixed anchor, not the moving camera/follow center (which always projects
// to zero and therefore cannot stabilize the world-space shadow texel grid).
inline ri::math::Mat4 StabilizeOrthographicShadowMatrix(ri::math::Mat4 lightViewProjection,
    std::uint32_t resolution) {
    if (resolution == 0) return lightViewProjection;
    const double halfResolution = static_cast<double>(resolution) * 0.5;
    for (int axis = 0; axis < 2; ++axis) {
        const double offset = lightViewProjection.m[axis][3];
        if (std::isfinite(offset))
            lightViewProjection.m[axis][3] = static_cast<float>(std::round(offset * halfResolution) / halfResolution);
    }
    return lightViewProjection;
}
} // namespace ri::render::vulkan
