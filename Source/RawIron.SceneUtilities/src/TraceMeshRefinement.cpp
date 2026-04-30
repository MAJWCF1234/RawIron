#include "RawIron/Scene/TraceMeshRefinement.h"

#include "RawIron/Math/Mat4.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ri::scene {
namespace {

[[nodiscard]] std::optional<float> IntersectRayTriangleDistanceFacing(const Ray& ray,
                                                                      const ri::math::Vec3& a,
                                                                      const ri::math::Vec3& b,
                                                                      const ri::math::Vec3& c,
                                                                      bool cullBackfaces,
                                                                      bool flipTowardRay,
                                                                      float eps,
                                                                      ri::math::Vec3* outNormal) {
    const ri::math::Vec3 edgeAB = b - a;
    const ri::math::Vec3 edgeAC = c - a;
    ri::math::Vec3 geomNormal = ri::math::Cross(edgeAB, edgeAC);
    const float normalLenSq = ri::math::LengthSquared(geomNormal);
    if (normalLenSq <= eps * eps) {
        return std::nullopt;
    }
    geomNormal = ri::math::Normalize(geomNormal);

    const ri::math::Vec3 p = ri::math::Cross(ray.direction, edgeAC);
    const float determinant = ri::math::Dot(edgeAB, p);
    if (std::fabs(determinant) <= eps) {
        return std::nullopt;
    }

    const float inverseDeterminant = 1.0f / determinant;
    const ri::math::Vec3 t = ray.origin - a;
    const float u = ri::math::Dot(t, p) * inverseDeterminant;
    if (u < 0.0f || u > 1.0f) {
        return std::nullopt;
    }

    const ri::math::Vec3 q = ri::math::Cross(t, edgeAB);
    const float v = ri::math::Dot(ray.direction, q) * inverseDeterminant;
    if (v < 0.0f || (u + v) > 1.0f) {
        return std::nullopt;
    }

    const float distance = ri::math::Dot(edgeAC, q) * inverseDeterminant;
    if (distance < 0.0f) {
        return std::nullopt;
    }

    ri::math::Vec3 shadingNormal = geomNormal;
    const float faceDot = ri::math::Dot(geomNormal, ray.direction);
    if (cullBackfaces && faceDot > eps) {
        return std::nullopt;
    }
    if (flipTowardRay && faceDot > 0.0f) {
        shadingNormal = geomNormal * -1.0f;
    }
    if (outNormal != nullptr) {
        *outNormal = shadingNormal;
    }
    return distance;
}

} // namespace

std::optional<ri::trace::TraceHit> RefineTraceRayHitWithMeshTriangles(const ri::trace::TraceHit& coarse,
                                                                      const Scene& scene,
                                                                      const int nodeHandle,
                                                                      const Ray& worldRay,
                                                                      const MeshTraceRefinementOptions& options) {
    if (nodeHandle < 0 || static_cast<std::size_t>(nodeHandle) >= scene.NodeCount()) {
        return std::nullopt;
    }
    const Node& node = scene.GetNode(nodeHandle);
    if (node.mesh == kInvalidHandle) {
        return std::nullopt;
    }
    const Mesh& mesh = scene.GetMesh(node.mesh);
    if (mesh.primitive != PrimitiveType::Custom) {
        return std::nullopt;
    }
    if (mesh.positions.empty()) {
        return std::nullopt;
    }
    const std::optional<Ray> sanitized = [&]() -> std::optional<Ray> {
        const ri::math::Vec3 direction = ri::math::Normalize(worldRay.direction);
        if (ri::math::LengthSquared(direction) <= options.triangleEpsilon * options.triangleEpsilon) {
            return std::nullopt;
        }
        return Ray{worldRay.origin, direction};
    }();
    if (!sanitized.has_value()) {
        return std::nullopt;
    }

    const ri::math::Mat4 world = scene.ComputeWorldMatrix(nodeHandle);
    const auto resolveVertex = [&](int index) -> ri::math::Vec3 {
        return ri::math::TransformPoint(world, mesh.positions[static_cast<std::size_t>(index)]);
    };

    const bool hasIndices = mesh.indices.size() >= 3;
    const int triangleCount = hasIndices ? static_cast<int>(mesh.indices.size() / 3U)
                                         : static_cast<int>(mesh.positions.size() / 3U);

    float bestDistance = std::numeric_limits<float>::infinity();
    ri::math::Vec3 bestNormal{};
    ri::math::Vec3 bestPoint{};

    const float coarseT = coarse.time;
    for (int triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex) {
        int ia = 0;
        int ib = 0;
        int ic = 0;
        if (hasIndices) {
            ia = mesh.indices[static_cast<std::size_t>(triangleIndex * 3 + 0)];
            ib = mesh.indices[static_cast<std::size_t>(triangleIndex * 3 + 1)];
            ic = mesh.indices[static_cast<std::size_t>(triangleIndex * 3 + 2)];
        } else {
            ia = triangleIndex * 3 + 0;
            ib = triangleIndex * 3 + 1;
            ic = triangleIndex * 3 + 2;
        }

        if (ia < 0 || ib < 0 || ic < 0 || ia >= static_cast<int>(mesh.positions.size())
            || ib >= static_cast<int>(mesh.positions.size()) || ic >= static_cast<int>(mesh.positions.size())) {
            continue;
        }

        const ri::math::Vec3 va = resolveVertex(ia);
        const ri::math::Vec3 vb = resolveVertex(ib);
        const ri::math::Vec3 vc = resolveVertex(ic);

        ri::math::Vec3 triNormal{};
        if (const std::optional<float> hitDistance = IntersectRayTriangleDistanceFacing(
                *sanitized, va, vb, vc, options.cullBackfaces, options.flipTowardRay, options.triangleEpsilon, &triNormal);
            hitDistance.has_value()) {
            if (std::fabs(*hitDistance - coarseT) > options.coarseDistanceEpsilon) {
                continue;
            }
            if (*hitDistance < bestDistance) {
                bestDistance = *hitDistance;
                bestNormal = triNormal;
                bestPoint = sanitized->origin + (sanitized->direction * *hitDistance);
            }
        }
    }

    if (bestDistance == std::numeric_limits<float>::infinity()) {
        return std::nullopt;
    }

    ri::trace::TraceHit refined = coarse;
    refined.point = bestPoint;
    refined.normal = bestNormal;
    refined.time = bestDistance;
    return refined;
}

} // namespace ri::scene
