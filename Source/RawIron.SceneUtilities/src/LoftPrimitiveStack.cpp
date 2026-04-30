#include "RawIron/Scene/LoftPrimitiveStack.h"

#include "RawIron/Math/ScalarClamp.h"

#include <algorithm>
#include <cmath>

namespace ri::scene {
namespace {

float Wrap01(float t) {
    if (!std::isfinite(t)) {
        return 0.0f;
    }
    const float floorT = std::floor(t);
    float out = t - floorT;
    if (out < 0.0f) {
        out += 1.0f;
    }
    return out;
}

ri::math::Vec3 SampleLoopAt(const std::vector<ri::math::Vec3>& loop, float t) {
    if (loop.empty()) {
        return {};
    }
    if (loop.size() == 1U) {
        return loop.front();
    }
    const float wrapped = Wrap01(t);
    const float x = wrapped * static_cast<float>(loop.size());
    const std::size_t i0 = static_cast<std::size_t>(std::floor(x)) % loop.size();
    const std::size_t i1 = (i0 + 1U) % loop.size();
    const float localT = x - std::floor(x);
    return ri::math::Lerp(loop[i0], loop[i1], localT);
}

void PushTriangle(Mesh& mesh, int a, int b, int c) {
    mesh.indices.push_back(a);
    mesh.indices.push_back(b);
    mesh.indices.push_back(c);
}

float DistanceSquared2d(const ri::math::Vec2& lhs, const ri::math::Vec2& rhs) {
    const float dx = lhs.x - rhs.x;
    const float dy = lhs.y - rhs.y;
    return (dx * dx) + (dy * dy);
}

float SignedArea2d(const std::vector<ri::math::Vec2>& loop) {
    float area = 0.0f;
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const ri::math::Vec2& a = loop[i];
        const ri::math::Vec2& b = loop[(i + 1U) % loop.size()];
        area += (a.x * b.y) - (b.x * a.y);
    }
    return area * 0.5f;
}

} // namespace

std::vector<ri::math::Vec2> GetPrimitiveProfile2dPreset(const std::string_view presetName) {
    if (presetName == "diamond" || presetName == "beam") {
        return {{0.0f, -0.5f}, {0.5f, 0.0f}, {0.0f, 0.5f}, {-0.5f, 0.0f}};
    }
    if (presetName == "hex" || presetName == "hexagon") {
        return {
            {0.5f, 0.0f},
            {0.25f, 0.4330127f},
            {-0.25f, 0.4330127f},
            {-0.5f, 0.0f},
            {-0.25f, -0.4330127f},
            {0.25f, -0.4330127f},
        };
    }
    return {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
}

std::vector<ri::math::Vec2> SanitizePrimitiveProfile2dLoop(const std::vector<ri::math::Vec2>& authoredPoints,
                                                           const std::string_view fallbackPreset,
                                                           const std::size_t maxPoints,
                                                           const float maxAbsCoordinate) {
    const std::size_t safeMaxPoints = std::clamp<std::size_t>(maxPoints, 3U, 256U);
    const float extent = ri::math::ClampFinite(std::fabs(maxAbsCoordinate), 1.0f, 0.001f, 1000.0f);
    std::vector<ri::math::Vec2> loop;
    loop.reserve(std::min(authoredPoints.size(), safeMaxPoints));

    for (const ri::math::Vec2& point : authoredPoints) {
        if (loop.size() >= safeMaxPoints) {
            break;
        }
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            continue;
        }
        const ri::math::Vec2 clamped{
            std::clamp(point.x, -extent, extent),
            std::clamp(point.y, -extent, extent),
        };
        if (!loop.empty() && DistanceSquared2d(loop.back(), clamped) <= 1e-8f) {
            continue;
        }
        loop.push_back(clamped);
    }

    if (loop.size() > 1U && DistanceSquared2d(loop.front(), loop.back()) <= 1e-8f) {
        loop.pop_back();
    }
    if (loop.size() < 3U || std::fabs(SignedArea2d(loop)) <= 1e-6f) {
        loop = GetPrimitiveProfile2dPreset(fallbackPreset);
    }
    if (SignedArea2d(loop) < 0.0f) {
        std::reverse(loop.begin(), loop.end());
    }
    return loop;
}

std::vector<ri::math::Vec3> GetPrimitiveProfilePreset(const std::string_view presetName) {
    if (presetName == "duct" || presetName == "box") {
        return {
            {-0.5f, -0.5f, 0.0f},
            {0.5f, -0.5f, 0.0f},
            {0.5f, 0.5f, 0.0f},
            {-0.5f, 0.5f, 0.0f},
        };
    }
    if (presetName == "beam" || presetName == "diamond") {
        return {
            {0.0f, -0.7f, 0.0f},
            {0.6f, 0.0f, 0.0f},
            {0.0f, 0.7f, 0.0f},
            {-0.6f, 0.0f, 0.0f},
        };
    }
    return {
        {0.7f, 0.0f, 0.0f},
        {0.35f, 0.6f, 0.0f},
        {-0.35f, 0.6f, 0.0f},
        {-0.7f, 0.0f, 0.0f},
        {-0.35f, -0.6f, 0.0f},
        {0.35f, -0.6f, 0.0f},
    };
}

std::vector<ri::math::Vec3> ResamplePrimitiveProfileLoop(const std::vector<ri::math::Vec3>& profileLoop,
                                                         const std::size_t sampleCount) {
    const std::vector<ri::math::Vec3> source = profileLoop.empty() ? GetPrimitiveProfilePreset("hex") : profileLoop;
    const std::size_t clampedSamples = std::max<std::size_t>(3U, sampleCount);
    std::vector<ri::math::Vec3> out{};
    out.reserve(clampedSamples);
    for (std::size_t i = 0; i < clampedSamples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(clampedSamples);
        out.push_back(SampleLoopAt(source, t));
    }
    return out;
}

float GetLoftInterpolationValue(float t) {
    const float x = ri::math::ClampFinite(t, 0.0f, 0.0f, 1.0f);
    return x * x * (3.0f - (2.0f * x)); // smoothstep
}

Mesh BuildLoftPrimitiveGeometryFromProfiles(const std::vector<ri::math::Vec3>& startProfileLoop,
                                            const std::vector<ri::math::Vec3>& endProfileLoop,
                                            const std::size_t depthSegments,
                                            const bool capEnds) {
    Mesh mesh{};
    mesh.name = "LoftPrimitive";
    mesh.primitive = PrimitiveType::Custom;

    const std::size_t ringSamples = std::max<std::size_t>(
        3U,
        std::max(startProfileLoop.size(), endProfileLoop.size()));
    const std::vector<ri::math::Vec3> start = ResamplePrimitiveProfileLoop(startProfileLoop, ringSamples);
    const std::vector<ri::math::Vec3> end = ResamplePrimitiveProfileLoop(endProfileLoop, ringSamples);
    const std::size_t rings = std::max<std::size_t>(2U, depthSegments + 1U);
    mesh.positions.reserve(rings * ringSamples + (capEnds ? 2U : 0U));

    for (std::size_t ring = 0; ring < rings; ++ring) {
        const float tRaw = static_cast<float>(ring) / static_cast<float>(rings - 1U);
        const float t = GetLoftInterpolationValue(tRaw);
        for (std::size_t i = 0; i < ringSamples; ++i) {
            const ri::math::Vec3 p = ri::math::Lerp(start[i], end[i], t);
            mesh.positions.push_back({p.x, p.y, tRaw});
        }
    }

    for (std::size_t ring = 0; ring + 1U < rings; ++ring) {
        for (std::size_t i = 0; i < ringSamples; ++i) {
            const std::size_t n = (i + 1U) % ringSamples;
            const int a = static_cast<int>((ring * ringSamples) + i);
            const int b = static_cast<int>(((ring + 1U) * ringSamples) + i);
            const int c = static_cast<int>(((ring + 1U) * ringSamples) + n);
            const int d = static_cast<int>((ring * ringSamples) + n);
            PushTriangle(mesh, a, b, c);
            PushTriangle(mesh, a, c, d);
        }
    }

    if (capEnds) {
        const int startCenter = static_cast<int>(mesh.positions.size());
        const int endCenter = startCenter + 1;
        mesh.positions.push_back({0.0f, 0.0f, 0.0f});
        mesh.positions.push_back({0.0f, 0.0f, 1.0f});
        for (std::size_t i = 0; i < ringSamples; ++i) {
            const std::size_t n = (i + 1U) % ringSamples;
            PushTriangle(mesh, startCenter, static_cast<int>(n), static_cast<int>(i));
            const int base = static_cast<int>((rings - 1U) * ringSamples);
            PushTriangle(mesh, endCenter, base + static_cast<int>(i), base + static_cast<int>(n));
        }
    }

    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    mesh.indexCount = static_cast<int>(mesh.indices.size());
    return mesh;
}

} // namespace ri::scene
