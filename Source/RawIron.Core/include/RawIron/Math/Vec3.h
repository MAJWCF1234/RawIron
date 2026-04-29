#pragma once

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace ri::math {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline constexpr Vec3 operator+(const Vec3& lhs, const Vec3& rhs) {
    return Vec3{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

inline constexpr Vec3 operator-(const Vec3& lhs, const Vec3& rhs) {
    return Vec3{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

inline constexpr Vec3 operator*(const Vec3& value, float scalar) {
    return Vec3{value.x * scalar, value.y * scalar, value.z * scalar};
}

inline constexpr Vec3 operator*(float scalar, const Vec3& value) {
    return value * scalar;
}

inline constexpr Vec3 operator/(const Vec3& value, float scalar) {
    return Vec3{value.x / scalar, value.y / scalar, value.z / scalar};
}

inline constexpr float Dot(const Vec3& lhs, const Vec3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

inline constexpr Vec3 Cross(const Vec3& lhs, const Vec3& rhs) {
    return Vec3{
        (lhs.y * rhs.z) - (lhs.z * rhs.y),
        (lhs.z * rhs.x) - (lhs.x * rhs.z),
        (lhs.x * rhs.y) - (lhs.y * rhs.x),
    };
}

inline constexpr float LengthSquared(const Vec3& value) {
    return Dot(value, value);
}

inline float Length(const Vec3& value) {
    return std::sqrt(LengthSquared(value));
}

inline constexpr float DistanceSquared(const Vec3& lhs, const Vec3& rhs) {
    return LengthSquared(lhs - rhs);
}

inline float Distance(const Vec3& lhs, const Vec3& rhs) {
    return Length(lhs - rhs);
}

inline Vec3 Normalize(const Vec3& value) {
    const float length = Length(value);
    if (length <= 0.000001f) {
        return Vec3{};
    }
    return value * (1.0f / length);
}

inline Vec3 Abs(const Vec3& value) {
    return Vec3{std::fabs(value.x), std::fabs(value.y), std::fabs(value.z)};
}

inline Vec3 Lerp(const Vec3& lhs, const Vec3& rhs, float t) {
    return lhs + ((rhs - lhs) * t);
}

inline constexpr float RadiansToDegrees(float radians) {
    return radians * 57.29577951308232f;
}

inline constexpr Vec3 RadiansToDegrees(const Vec3& radians) {
    return Vec3{
        RadiansToDegrees(radians.x),
        RadiansToDegrees(radians.y),
        RadiansToDegrees(radians.z),
    };
}

inline std::string ToString(const Vec3& value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2)
           << '(' << value.x << ", " << value.y << ", " << value.z << ')';
    return stream.str();
}

} // namespace ri::math
