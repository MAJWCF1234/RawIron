#pragma once

#include "RawIron/Math/Mat4.h"
#include <cmath>
#include <cstdint>

namespace ri::render::vulkan {

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
