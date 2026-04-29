#pragma once

#include <cmath>

namespace ri::math {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

inline constexpr Vec2 operator+(const Vec2& lhs, const Vec2& rhs) {
    return Vec2{lhs.x + rhs.x, lhs.y + rhs.y};
}

inline constexpr Vec2 operator-(const Vec2& lhs, const Vec2& rhs) {
    return Vec2{lhs.x - rhs.x, lhs.y - rhs.y};
}

inline constexpr Vec2 operator*(const Vec2& value, float scalar) {
    return Vec2{value.x * scalar, value.y * scalar};
}

inline constexpr Vec2 operator*(float scalar, const Vec2& value) {
    return value * scalar;
}

inline constexpr Vec2 operator/(const Vec2& value, float scalar) {
    return Vec2{value.x / scalar, value.y / scalar};
}

} // namespace ri::math
