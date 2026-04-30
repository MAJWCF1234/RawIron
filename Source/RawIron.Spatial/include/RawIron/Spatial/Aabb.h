#pragma once

#include "RawIron/Math/Vec3.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ri::spatial {

struct Aabb {
    ri::math::Vec3 min{
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity(),
    };
    ri::math::Vec3 max{
        -std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
    };
};

struct Ray {
    ri::math::Vec3 origin{};
    ri::math::Vec3 direction{};
};

inline bool IsFinite(const Aabb& box) {
    return std::isfinite(box.min.x) && std::isfinite(box.min.y) && std::isfinite(box.min.z)
        && std::isfinite(box.max.x) && std::isfinite(box.max.y) && std::isfinite(box.max.z);
}

inline bool IsEmpty(const Aabb& box) {
    return !IsFinite(box) || box.min.x > box.max.x || box.min.y > box.max.y || box.min.z > box.max.z;
}

inline Aabb MakeEmptyAabb() {
    return Aabb{};
}

inline ri::math::Vec3 Center(const Aabb& box) {
    return (box.min + box.max) * 0.5f;
}

inline ri::math::Vec3 Size(const Aabb& box) {
    return IsEmpty(box) ? ri::math::Vec3{} : (box.max - box.min);
}

inline Aabb ExpandByPoint(const Aabb& box, const ri::math::Vec3& point) {
    if (IsEmpty(box)) {
        return Aabb{.min = point, .max = point};
    }
    return Aabb{
        .min = {
            (std::min)(box.min.x, point.x),
            (std::min)(box.min.y, point.y),
            (std::min)(box.min.z, point.z),
        },
        .max = {
            (std::max)(box.max.x, point.x),
            (std::max)(box.max.y, point.y),
            (std::max)(box.max.z, point.z),
        },
    };
}

inline Aabb Union(const Aabb& lhs, const Aabb& rhs) {
    if (IsEmpty(lhs)) {
        return rhs;
    }
    if (IsEmpty(rhs)) {
        return lhs;
    }
    return Aabb{
        .min = {
            (std::min)(lhs.min.x, rhs.min.x),
            (std::min)(lhs.min.y, rhs.min.y),
            (std::min)(lhs.min.z, rhs.min.z),
        },
        .max = {
            (std::max)(lhs.max.x, rhs.max.x),
            (std::max)(lhs.max.y, rhs.max.y),
            (std::max)(lhs.max.z, rhs.max.z),
        },
    };
}

inline bool Intersects(const Aabb& lhs, const Aabb& rhs) {
    if (IsEmpty(lhs) || IsEmpty(rhs)) {
        return false;
    }
    return lhs.min.x <= rhs.max.x && lhs.max.x >= rhs.min.x
        && lhs.min.y <= rhs.max.y && lhs.max.y >= rhs.min.y
        && lhs.min.z <= rhs.max.z && lhs.max.z >= rhs.min.z;
}

inline bool IsFinite(const Ray& ray) {
    return std::isfinite(ray.origin.x) && std::isfinite(ray.origin.y) && std::isfinite(ray.origin.z)
        && std::isfinite(ray.direction.x) && std::isfinite(ray.direction.y) && std::isfinite(ray.direction.z);
}

inline bool IntersectRayAabb(const Ray& ray, const Aabb& box, float farDistance, float* outDistance = nullptr) {
    if (!IsFinite(ray) || IsEmpty(box) || !std::isfinite(farDistance) || farDistance <= 0.0f
        || ri::math::LengthSquared(ray.direction) < 1e-20f) {
        return false;
    }

    const ri::math::Vec3 direction = ri::math::Normalize(ray.direction);
    float tMin = 0.0f;
    float tMax = farDistance;

    auto updateAxis = [&](float origin, float dir, float minValue, float maxValue) {
        if (std::fabs(dir) <= 1e-8f) {
            return origin >= minValue && origin <= maxValue;
        }
        const float invDir = 1.0f / dir;
        float t0 = (minValue - origin) * invDir;
        float t1 = (maxValue - origin) * invDir;
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        tMin = (std::max)(tMin, t0);
        tMax = (std::min)(tMax, t1);
        return tMax >= tMin;
    };

    if (!updateAxis(ray.origin.x, direction.x, box.min.x, box.max.x)
        || !updateAxis(ray.origin.y, direction.y, box.min.y, box.max.y)
        || !updateAxis(ray.origin.z, direction.z, box.min.z, box.max.z)) {
        return false;
    }

    if (outDistance != nullptr) {
        *outDistance = tMin;
    }
    return tMin <= farDistance;
}

inline Aabb BuildSegmentBounds(const ri::math::Vec3& start, const ri::math::Vec3& end) {
    Aabb box = MakeEmptyAabb();
    box = ExpandByPoint(box, start);
    box = ExpandByPoint(box, end);
    return box;
}

} // namespace ri::spatial
