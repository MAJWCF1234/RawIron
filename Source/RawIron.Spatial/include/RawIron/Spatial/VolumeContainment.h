#pragma once

#include "RawIron/Math/Vec3.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ri::spatial {

/// Axis-aligned trigger volumes as used by level authoring and the web prototype shell.
/// Box and Y-up cylinder match `AnomalousEchoGame.isPointInsideVolume` semantics; sphere uses
/// squared distance (equivalent to Three.js `distanceTo` for containment tests).
struct AuthoringVolumeDesc {
    enum class Shape : std::uint8_t {
        Box = 0,
        CylinderY,
        Sphere,
    };

    Shape shape = Shape::Box;
    ri::math::Vec3 center{};
    /// Full extents (not half-extents); box test uses half = `boxSize * 0.5f`.
    ri::math::Vec3 boxSize{1.0f, 1.0f, 1.0f};
    float cylinderRadius = 0.5f;
    float cylinderHeight = 2.0f;
    float sphereRadius = 0.5f;
};

namespace detail {

inline bool IsFinite3(const ri::math::Vec3& v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline float AuthoringCylinderRadius(float rawRadius) noexcept {
    const float r = std::isfinite(rawRadius) ? rawRadius : 0.5f;
    return std::max(0.25f, r);
}

inline float AuthoringCylinderHeight(float rawHeight, float sizeYFallback) noexcept {
    const float primary = std::isfinite(rawHeight) ? rawHeight : sizeYFallback;
    const float h = std::isfinite(primary) ? primary : 2.0f;
    return std::max(0.25f, h);
}

} // namespace detail

/// False when the point or volume parameters are non-finite or degenerate where relevant.
[[nodiscard]] inline bool PointInsideAuthoringVolume(const ri::math::Vec3& point,
                                                     const AuthoringVolumeDesc& volume) noexcept {
    using namespace detail;
    if (!IsFinite3(point) || !IsFinite3(volume.center)) {
        return false;
    }

    switch (volume.shape) {
    case AuthoringVolumeDesc::Shape::Box: {
        if (!IsFinite3(volume.boxSize)) {
            return false;
        }
        const ri::math::Vec3 half{
            std::abs(volume.boxSize.x) * 0.5f,
            std::abs(volume.boxSize.y) * 0.5f,
            std::abs(volume.boxSize.z) * 0.5f,
        };
        const float dx = std::abs(point.x - volume.center.x);
        const float dy = std::abs(point.y - volume.center.y);
        const float dz = std::abs(point.z - volume.center.z);
        return dx <= half.x && dy <= half.y && dz <= half.z;
    }
    case AuthoringVolumeDesc::Shape::CylinderY: {
        const float radius = AuthoringCylinderRadius(volume.cylinderRadius);
        const float height = AuthoringCylinderHeight(volume.cylinderHeight, volume.boxSize.y);
        const float dx = point.x - volume.center.x;
        const float dz = point.z - volume.center.z;
        const float distSq = dx * dx + dz * dz;
        if (distSq > radius * radius) {
            return false;
        }
        const float halfH = height * 0.5f;
        return std::abs(point.y - volume.center.y) <= halfH;
    }
    case AuthoringVolumeDesc::Shape::Sphere: {
        if (!std::isfinite(volume.sphereRadius)) {
            return false;
        }
        const ri::math::Vec3 d = point - volume.center;
        const float distSq = ri::math::LengthSquared(d);
        const float r = volume.sphereRadius;
        return distSq <= r * r;
    }
    }
    return false;
}

} // namespace ri::spatial
