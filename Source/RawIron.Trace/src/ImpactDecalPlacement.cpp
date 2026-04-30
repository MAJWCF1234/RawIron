#include "RawIron/Trace/ImpactDecalPlacement.h"

#include <cmath>
#include <algorithm>

namespace ri::trace {
namespace {

[[nodiscard]] ri::math::Vec3 OrthonormalTangentU(const ri::math::Vec3& unitNormal) noexcept {
    const ri::math::Vec3 worldUp{0.0f, 1.0f, 0.0f};
    ri::math::Vec3 candidate = ri::math::Cross(worldUp, unitNormal);
    if (ri::math::LengthSquared(candidate) < 1e-12f) {
        candidate = ri::math::Cross({1.0f, 0.0f, 0.0f}, unitNormal);
    }
    return ri::math::Normalize(candidate);
}

} // namespace

std::optional<ImpactDecalPlacement> BuildSplatterDecalPlacement(const ri::math::Vec3& hitPosition,
                                                                const ri::math::Vec3& surfaceNormal,
                                                                const float radius,
                                                                const float surfaceOffset) noexcept {
    if (!std::isfinite(radius) || radius <= 0.0f) {
        return std::nullopt;
    }
    const ri::math::Vec3 n = ri::math::Normalize(surfaceNormal);
    if (ri::math::LengthSquared(n) < 1e-12f) {
        return std::nullopt;
    }
    const ri::math::Vec3 u = OrthonormalTangentU(n);
    const ri::math::Vec3 v = ri::math::Normalize(ri::math::Cross(n, u));
    ImpactDecalPlacement out{};
    out.kind = ImpactDecalKind::Splatter;
    out.origin = hitPosition + (n * surfaceOffset);
    out.axisU = u;
    out.axisV = v;
    out.halfExtentU = radius;
    out.halfExtentV = radius;
    return out;
}

std::optional<ImpactDecalPlacement> BuildDragStreakDecalPlacement(const ri::math::Vec3& hitPosition,
                                                                    const ri::math::Vec3& surfaceNormal,
                                                                    const ri::math::Vec3& velocityWorld,
                                                                    const float streakHalfWidth,
                                                                    const float baseHalfLength,
                                                                    const float referenceSpeed,
                                                                    const float minHalfLength,
                                                                    const float maxHalfLength,
                                                                    const float surfaceOffset) noexcept {
    if (!std::isfinite(streakHalfWidth) || streakHalfWidth <= 0.0f || !std::isfinite(baseHalfLength)
        || baseHalfLength <= 0.0f || !std::isfinite(referenceSpeed) || referenceSpeed <= 1e-5f
        || !std::isfinite(minHalfLength) || !std::isfinite(maxHalfLength)) {
        return std::nullopt;
    }
    const ri::math::Vec3 n = ri::math::Normalize(surfaceNormal);
    if (ri::math::LengthSquared(n) < 1e-12f) {
        return std::nullopt;
    }
    const float vn = ri::math::Dot(velocityWorld, n);
    ri::math::Vec3 tangent = velocityWorld - (n * vn);
    const float speed = ri::math::Length(tangent);
    if (speed < 1e-4f) {
        return std::nullopt;
    }
    tangent = tangent * (1.0f / speed);
    const float scaledHalf = baseHalfLength * (speed / referenceSpeed);
    const float clampedHalf = std::clamp(scaledHalf, minHalfLength, maxHalfLength);
    const ri::math::Vec3 widthAxis = ri::math::Normalize(ri::math::Cross(n, tangent));

    ImpactDecalPlacement out{};
    out.kind = ImpactDecalKind::DragStreak;
    out.origin = hitPosition + (n * surfaceOffset);
    out.axisU = tangent;
    out.axisV = widthAxis;
    out.halfExtentU = clampedHalf;
    out.halfExtentV = streakHalfWidth;
    return out;
}

} // namespace ri::trace
