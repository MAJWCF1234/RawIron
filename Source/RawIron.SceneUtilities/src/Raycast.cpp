#include "RawIron/Scene/Raycast.h"

#include "RawIron/Math/Mat4.h"

#include <algorithm>
#include <array>
#include <limits>

namespace ri::scene {

namespace {

constexpr float kRayEpsilon = 0.0001f;

struct PrimitiveBounds {
    ri::math::Vec3 center{0.0f, 0.0f, 0.0f};
    ri::math::Vec3 scale{1.0f, 1.0f, 1.0f};
};

[[nodiscard]] std::optional<Ray> SanitizeRay(const Ray& ray) {
    const ri::math::Vec3 direction = ri::math::Normalize(ray.direction);
    if (ri::math::LengthSquared(direction) <= kRayEpsilon * kRayEpsilon) {
        return std::nullopt;
    }
    return Ray{ray.origin, direction};
}

[[nodiscard]] PrimitiveBounds ComputePrimitiveBounds(const Scene& scene, int nodeHandle) {
    const ri::math::Mat4 world = scene.ComputeWorldMatrix(nodeHandle);
    return PrimitiveBounds{
        .center = ri::math::ExtractTranslation(world),
        .scale = ri::math::Abs(ri::math::ExtractScale(world)),
    };
}

[[nodiscard]] std::optional<float> IntersectTriangleDistance(const Ray& ray,
                                                             const ri::math::Vec3& a,
                                                             const ri::math::Vec3& b,
                                                             const ri::math::Vec3& c) {
    const ri::math::Vec3 edgeAB = b - a;
    const ri::math::Vec3 edgeAC = c - a;
    const ri::math::Vec3 p = ri::math::Cross(ray.direction, edgeAC);
    const float determinant = ri::math::Dot(edgeAB, p);
    if (std::fabs(determinant) <= kRayEpsilon) {
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

    return distance;
}

[[nodiscard]] std::optional<RaycastHit> IntersectSphere(int nodeHandle, int meshHandle, const PrimitiveBounds& bounds, const Ray& ray) {
    const float radius = std::max({bounds.scale.x, bounds.scale.y, bounds.scale.z}) * 0.5f;
    const ri::math::Vec3 offset = ray.origin - bounds.center;
    const float projection = ri::math::Dot(offset, ray.direction);
    const float discriminant = projection * projection - (ri::math::Dot(offset, offset) - radius * radius);
    if (discriminant < 0.0f) {
        return std::nullopt;
    }

    const float root = std::sqrt(discriminant);
    float distance = -projection - root;
    if (distance < 0.0f) {
        distance = -projection + root;
    }
    if (distance < 0.0f) {
        return std::nullopt;
    }

    const ri::math::Vec3 position = ray.origin + ray.direction * distance;
    return RaycastHit{
        .node = nodeHandle,
        .mesh = meshHandle,
        .distance = distance,
        .position = position,
        .normal = ri::math::Normalize(position - bounds.center),
    };
}

[[nodiscard]] std::optional<RaycastHit> IntersectPlane(int nodeHandle, int meshHandle, const PrimitiveBounds& bounds, const Ray& ray) {
    if (std::fabs(ray.direction.y) <= kRayEpsilon) {
        return std::nullopt;
    }

    const float distance = (bounds.center.y - ray.origin.y) / ray.direction.y;
    if (distance < 0.0f) {
        return std::nullopt;
    }

    const ri::math::Vec3 position = ray.origin + ray.direction * distance;
    const float halfWidth = bounds.scale.x * 0.5f;
    const float halfDepth = bounds.scale.z * 0.5f;
    if (std::fabs(position.x - bounds.center.x) > halfWidth || std::fabs(position.z - bounds.center.z) > halfDepth) {
        return std::nullopt;
    }

    return RaycastHit{
        .node = nodeHandle,
        .mesh = meshHandle,
        .distance = distance,
        .position = position,
        .normal = ri::math::Vec3{0.0f, ray.direction.y > 0.0f ? -1.0f : 1.0f, 0.0f},
    };
}

[[nodiscard]] std::optional<RaycastHit> IntersectAxisAlignedBox(int nodeHandle, int meshHandle, const PrimitiveBounds& bounds, const Ray& ray) {
    const ri::math::Vec3 halfExtents = bounds.scale * 0.5f;
    const ri::math::Vec3 minimum = bounds.center - halfExtents;
    const ri::math::Vec3 maximum = bounds.center + halfExtents;

    float nearDistance = 0.0f;
    float farDistance = std::numeric_limits<float>::max();
    ri::math::Vec3 nearNormal{0.0f, 0.0f, 0.0f};

    const std::array<float, 3> origin = {ray.origin.x, ray.origin.y, ray.origin.z};
    const std::array<float, 3> direction = {ray.direction.x, ray.direction.y, ray.direction.z};
    const std::array<float, 3> mins = {minimum.x, minimum.y, minimum.z};
    const std::array<float, 3> maxs = {maximum.x, maximum.y, maximum.z};
    const std::array<ri::math::Vec3, 3> normals = {
        ri::math::Vec3{1.0f, 0.0f, 0.0f},
        ri::math::Vec3{0.0f, 1.0f, 0.0f},
        ri::math::Vec3{0.0f, 0.0f, 1.0f},
    };

    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (std::fabs(direction[axis]) <= kRayEpsilon) {
            if (origin[axis] < mins[axis] || origin[axis] > maxs[axis]) {
                return std::nullopt;
            }
            continue;
        }

        float first = (mins[axis] - origin[axis]) / direction[axis];
        float second = (maxs[axis] - origin[axis]) / direction[axis];
        ri::math::Vec3 firstNormal = normals[axis] * -1.0f;
        if (direction[axis] < 0.0f) {
            std::swap(first, second);
            firstNormal = normals[axis];
        }

        if (first > nearDistance) {
            nearDistance = first;
            nearNormal = firstNormal;
        }
        farDistance = std::min(farDistance, second);
        if (nearDistance > farDistance) {
            return std::nullopt;
        }
    }

    const float distance = nearDistance >= 0.0f ? nearDistance : farDistance;
    if (distance < 0.0f || distance == std::numeric_limits<float>::max()) {
        return std::nullopt;
    }

    return RaycastHit{
        .node = nodeHandle,
        .mesh = meshHandle,
        .distance = distance,
        .position = ray.origin + ray.direction * distance,
        .normal = nearNormal,
    };
}

[[nodiscard]] std::optional<RaycastHit> IntersectCustomMesh(const Scene& scene, int nodeHandle, int meshHandle, const Mesh& mesh, const Ray& ray) {
    if (mesh.positions.empty()) {
        return std::nullopt;
    }

    const ri::math::Mat4 world = scene.ComputeWorldMatrix(nodeHandle);
    const auto resolveVertex = [&](int index) -> ri::math::Vec3 {
        return ri::math::TransformPoint(world, mesh.positions[static_cast<std::size_t>(index)]);
    };

    std::optional<RaycastHit> nearestHit;
    const bool hasIndices = mesh.indices.size() >= 3;
    const int triangleCount = hasIndices
        ? static_cast<int>(mesh.indices.size() / 3U)
        : static_cast<int>(mesh.positions.size() / 3U);

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

        if (ia < 0 || ib < 0 || ic < 0 ||
            ia >= static_cast<int>(mesh.positions.size()) ||
            ib >= static_cast<int>(mesh.positions.size()) ||
            ic >= static_cast<int>(mesh.positions.size())) {
            continue;
        }

        const ri::math::Vec3 a = resolveVertex(ia);
        const ri::math::Vec3 b = resolveVertex(ib);
        const ri::math::Vec3 c = resolveVertex(ic);
        const std::optional<float> distance = IntersectTriangleDistance(ray, a, b, c);
        if (!distance.has_value()) {
            continue;
        }

        if (nearestHit.has_value() && *distance >= nearestHit->distance) {
            continue;
        }

        const ri::math::Vec3 normal = ri::math::Normalize(ri::math::Cross(b - a, c - a));
        nearestHit = RaycastHit{
            .node = nodeHandle,
            .mesh = meshHandle,
            .distance = *distance,
            .position = ray.origin + ray.direction * *distance,
            .normal = normal,
        };
    }

    return nearestHit;
}

} // namespace

std::optional<Ray> BuildPerspectiveCameraRay(const Scene& scene,
                                             int cameraNodeHandle,
                                             float viewportX,
                                             float viewportY,
                                             float aspectRatio) {
    if (cameraNodeHandle < 0 || static_cast<std::size_t>(cameraNodeHandle) >= scene.NodeCount()) {
        return std::nullopt;
    }

    const Node& node = scene.GetNode(cameraNodeHandle);
    if (node.camera == kInvalidHandle) {
        return std::nullopt;
    }

    const Camera& camera = scene.GetCamera(node.camera);
    if (camera.projection != ProjectionType::Perspective) {
        return std::nullopt;
    }

    const ri::math::Mat4 world = scene.ComputeWorldMatrix(cameraNodeHandle);
    const ri::math::Vec3 position = ri::math::ExtractTranslation(world);
    const ri::math::Vec3 right = ri::math::ExtractRight(world);
    const ri::math::Vec3 up = ri::math::ExtractUp(world);
    const ri::math::Vec3 forward = ri::math::ExtractForward(world);

    const float safeAspect = std::max(aspectRatio, 0.001f);
    const float ndcX = (std::clamp(viewportX, 0.0f, 1.0f) * 2.0f) - 1.0f;
    const float ndcY = 1.0f - (std::clamp(viewportY, 0.0f, 1.0f) * 2.0f);
    const float tanHalfFov = std::tan(ri::math::DegreesToRadians(camera.fieldOfViewDegrees * 0.5f));

    const ri::math::Vec3 direction = ri::math::Normalize(
        forward +
        (right * (ndcX * tanHalfFov * safeAspect)) +
        (up * (ndcY * tanHalfFov)));
    if (ri::math::LengthSquared(direction) <= kRayEpsilon * kRayEpsilon) {
        return std::nullopt;
    }

    return Ray{
        .origin = position,
        .direction = direction,
    };
}

std::optional<RaycastHit> RaycastNode(const Scene& scene, int nodeHandle, const Ray& ray) {
    const std::optional<Ray> sanitizedRay = SanitizeRay(ray);
    if (!sanitizedRay.has_value()) {
        return std::nullopt;
    }

    if (nodeHandle < 0 || static_cast<std::size_t>(nodeHandle) >= scene.NodeCount()) {
        return std::nullopt;
    }

    const Node& node = scene.GetNode(nodeHandle);
    if (node.mesh == kInvalidHandle) {
        return std::nullopt;
    }

    const Mesh& mesh = scene.GetMesh(node.mesh);
    const PrimitiveBounds bounds = ComputePrimitiveBounds(scene, nodeHandle);
    switch (mesh.primitive) {
        case PrimitiveType::Sphere:
            return IntersectSphere(nodeHandle, node.mesh, bounds, *sanitizedRay);
        case PrimitiveType::Plane:
            return IntersectPlane(nodeHandle, node.mesh, bounds, *sanitizedRay);
        case PrimitiveType::Cube:
            return IntersectAxisAlignedBox(nodeHandle, node.mesh, bounds, *sanitizedRay);
        case PrimitiveType::Custom:
            return IntersectCustomMesh(scene, nodeHandle, node.mesh, mesh, *sanitizedRay);
    }

    return std::nullopt;
}

std::optional<RaycastHit> RaycastSceneNearest(const Scene& scene, const Ray& ray) {
    const std::vector<RaycastHit> hits = RaycastSceneAll(scene, ray);
    if (hits.empty()) {
        return std::nullopt;
    }
    return hits.front();
}

std::vector<RaycastHit> RaycastSceneAll(const Scene& scene, const Ray& ray) {
    std::vector<RaycastHit> hits;
    const std::vector<Node>& nodes = scene.Nodes();
    for (int nodeIndex = 0; nodeIndex < static_cast<int>(nodes.size()); ++nodeIndex) {
        if (const std::optional<RaycastHit> hit = RaycastNode(scene, nodeIndex, ray); hit.has_value()) {
            hits.push_back(*hit);
        }
    }

    std::sort(hits.begin(), hits.end(), [](const RaycastHit& lhs, const RaycastHit& rhs) {
        return lhs.distance < rhs.distance;
    });
    return hits;
}

} // namespace ri::scene
