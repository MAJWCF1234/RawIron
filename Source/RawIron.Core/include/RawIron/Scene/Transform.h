#pragma once

#include "RawIron/Math/Mat4.h"
#include "RawIron/Math/Vec3.h"

namespace ri::scene {

struct Transform {
    ri::math::Vec3 position{0.0f, 0.0f, 0.0f};
    ri::math::Vec3 rotationDegrees{0.0f, 0.0f, 0.0f};
    ri::math::Vec3 scale{1.0f, 1.0f, 1.0f};

    [[nodiscard]] ri::math::Mat4 LocalMatrix() const {
        return ri::math::TRS(position, rotationDegrees, scale);
    }
};

} // namespace ri::scene
