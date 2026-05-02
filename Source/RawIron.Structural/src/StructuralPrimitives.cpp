#include "RawIron/Structural/StructuralPrimitives.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace ri::structural {
namespace {

constexpr float kHalfExtent = 0.5f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kGoldenRatio = 1.6180339887498948482f;

int ClampInteger(int value, int fallback, int minValue, int maxValue) {
    if (value < minValue || value > maxValue) {
        return fallback;
    }
    return value;
}

int ClampRange(int value, int minValue, int maxValue) {
    return std::max(minValue, std::min(maxValue, value));
}

float SignedPower(float value, float exponent) {
    if (std::fabs(value) <= 1e-6f) {
        return 0.0f;
    }
    return std::copysign(std::pow(std::fabs(value), exponent), value);
}

ConvexSolid BuildSolidFromFaces(const std::vector<std::vector<ri::math::Vec3>>& faces) {
    ConvexSolid solid{};
    for (const auto& face : faces) {
        const std::optional<Plane> plane = ComputePolygonPlane(face);
        if (!plane.has_value()) {
            continue;
        }
        solid.polygons.push_back(ConvexPolygon{*plane, face});
    }
    return solid;
}

CompiledMesh BuildMeshFromTriangles(const std::vector<ri::math::Vec3>& vertices) {
    CompiledMesh mesh{};
    for (std::size_t index = 0; index + 2 < vertices.size(); index += 3) {
        const ri::math::Vec3 a = vertices[index];
        const ri::math::Vec3 b = vertices[index + 1];
        const ri::math::Vec3 c = vertices[index + 2];
        const ri::math::Vec3 normal = ri::math::Normalize(ri::math::Cross(b - a, c - a));
        const ri::math::Vec3 safeNormal = ri::math::LengthSquared(normal) > 1e-12f ? normal : ri::math::Vec3{0.0f, 1.0f, 0.0f};
        const ri::math::Vec3 triangle[3] = {a, b, c};
        for (const ri::math::Vec3& vertex : triangle) {
            mesh.positions.push_back(vertex);
            mesh.normals.push_back(safeNormal);
            if (!mesh.hasBounds) {
                mesh.hasBounds = true;
                mesh.boundsMin = vertex;
                mesh.boundsMax = vertex;
            } else {
                mesh.boundsMin.x = std::min(mesh.boundsMin.x, vertex.x);
                mesh.boundsMin.y = std::min(mesh.boundsMin.y, vertex.y);
                mesh.boundsMin.z = std::min(mesh.boundsMin.z, vertex.z);
                mesh.boundsMax.x = std::max(mesh.boundsMax.x, vertex.x);
                mesh.boundsMax.y = std::max(mesh.boundsMax.y, vertex.y);
                mesh.boundsMax.z = std::max(mesh.boundsMax.z, vertex.z);
            }
        }
        mesh.triangleCount += 1;
    }
    return mesh;
}

void AppendTriangle(std::vector<ri::math::Vec3>& vertices,
                    const ri::math::Vec3& a,
                    const ri::math::Vec3& b,
                    const ri::math::Vec3& c) {
    vertices.push_back(a);
    vertices.push_back(b);
    vertices.push_back(c);
}

void AppendOutwardTriangle(std::vector<ri::math::Vec3>& vertices,
                           const ri::math::Vec3& a,
                           const ri::math::Vec3& b,
                           const ri::math::Vec3& c) {
    const ri::math::Vec3 centroid = (a + b + c) / 3.0f;
    const ri::math::Vec3 normal = ri::math::Cross(b - a, c - a);
    if (ri::math::Dot(normal, centroid) < 0.0f) {
        AppendTriangle(vertices, a, c, b);
        return;
    }
    AppendTriangle(vertices, a, b, c);
}

void AppendOutwardQuad(std::vector<ri::math::Vec3>& vertices,
                       const ri::math::Vec3& a,
                       const ri::math::Vec3& b,
                       const ri::math::Vec3& c,
                       const ri::math::Vec3& d) {
    AppendOutwardTriangle(vertices, a, b, c);
    AppendOutwardTriangle(vertices, a, c, d);
}

void AppendQuadFacing(std::vector<ri::math::Vec3>& vertices,
                      const ri::math::Vec3& a,
                      const ri::math::Vec3& b,
                      const ri::math::Vec3& c,
                      const ri::math::Vec3& d,
                      const ri::math::Vec3& desiredNormal) {
    const ri::math::Vec3 normal = ri::math::Cross(b - a, c - a);
    if (ri::math::Dot(normal, desiredNormal) < 0.0f) {
        AppendTriangle(vertices, a, d, c);
        AppendTriangle(vertices, a, c, b);
        return;
    }
    AppendTriangle(vertices, a, b, c);
    AppendTriangle(vertices, a, c, d);
}

float SeededUnit01(int seed) {
    const float v = std::sin(static_cast<float>(seed) * 127.1f + 311.7f) * 43758.5453123f;
    return v - std::floor(v);
}

void AppendAxisAlignedBox(std::vector<ri::math::Vec3>& triangles,
                          const ri::math::Vec3& minB,
                          const ri::math::Vec3& maxB) {
    const ri::math::Vec3 v000{minB.x, minB.y, minB.z};
    const ri::math::Vec3 v100{maxB.x, minB.y, minB.z};
    const ri::math::Vec3 v110{maxB.x, minB.y, maxB.z};
    const ri::math::Vec3 v010{minB.x, minB.y, maxB.z};
    const ri::math::Vec3 v001{minB.x, maxB.y, minB.z};
    const ri::math::Vec3 v101{maxB.x, maxB.y, minB.z};
    const ri::math::Vec3 v111{maxB.x, maxB.y, maxB.z};
    const ri::math::Vec3 v011{minB.x, maxB.y, maxB.z};
    AppendQuadFacing(triangles, v000, v100, v110, v010, {0.0f, -1.0f, 0.0f});
    AppendQuadFacing(triangles, v001, v011, v111, v101, {0.0f, 1.0f, 0.0f});
    AppendQuadFacing(triangles, v000, v010, v011, v001, {-1.0f, 0.0f, 0.0f});
    AppendQuadFacing(triangles, v100, v101, v111, v110, {1.0f, 0.0f, 0.0f});
    AppendQuadFacing(triangles, v000, v001, v101, v100, {0.0f, 0.0f, -1.0f});
    AppendQuadFacing(triangles, v010, v110, v111, v011, {0.0f, 0.0f, 1.0f});
}

void AppendOpenCylinderBetween(std::vector<ri::math::Vec3>& triangles,
                               const ri::math::Vec3& start,
                               const ri::math::Vec3& end,
                               float radius,
                               int segments) {
    const int segs = ClampRange(segments, 3, 32);
    const ri::math::Vec3 axis = end - start;
    const float len = std::sqrt(ri::math::LengthSquared(axis));
    if (len < 1e-5f || radius < 1e-5f) {
        return;
    }
    const ri::math::Vec3 dir = axis * (1.0f / len);
    ri::math::Vec3 t = (std::fabs(dir.y) < 0.9f) ? ri::math::Cross({0.0f, 1.0f, 0.0f}, dir) : ri::math::Cross({1.0f, 0.0f, 0.0f}, dir);
    if (ri::math::LengthSquared(t) < 1e-10f) {
        t = {1.0f, 0.0f, 0.0f};
    }
    t = ri::math::Normalize(t);
    ri::math::Vec3 b = ri::math::Normalize(ri::math::Cross(dir, t));
    t = ri::math::Normalize(ri::math::Cross(b, dir));

    for (int index = 0; index < segs; ++index) {
        const float a0 = (2.0f * kPi * static_cast<float>(index)) / static_cast<float>(segs);
        const float a1 = (2.0f * kPi * static_cast<float>(index + 1)) / static_cast<float>(segs);
        const ri::math::Vec3 r0 = (t * std::cos(a0) + b * std::sin(a0)) * radius;
        const ri::math::Vec3 r1 = (t * std::cos(a1) + b * std::sin(a1)) * radius;
        const ri::math::Vec3 p0a = start + r0;
        const ri::math::Vec3 p1a = start + r1;
        const ri::math::Vec3 p0b = end + r0;
        const ri::math::Vec3 p1b = end + r1;
        const ri::math::Vec3 n = ri::math::Normalize((r0 + r1) * 0.5f);
        AppendQuadFacing(triangles, p0a, p1a, p1b, p0b, n);
    }
}

void BranchRecursive(std::vector<ri::math::Vec3>& triangles,
                     const ri::math::Vec3& start,
                     const ri::math::Vec3& direction,
                     int depth,
                     int maxDepth,
                     float segmentLength,
                     float branchScale,
                     float branchAngleRad,
                     float radius,
                     int radialSegments,
                     int& branchesEmitted,
                     int branchBudget) {
    if (branchesEmitted >= branchBudget) {
        return;
    }
    const float safeLen = std::max(segmentLength, 0.04f);
    const ri::math::Vec3 end = start + direction * safeLen;
    AppendOpenCylinderBetween(triangles, start, end, radius, radialSegments);
    branchesEmitted += 1;
    if (depth >= maxDepth) {
        return;
    }
    const float nextLen = safeLen * branchScale;
    const float nextRadius = std::max(0.01f, radius * branchScale);
    const float c = std::cos(branchAngleRad);
    const float s = std::sin(branchAngleRad);
    ri::math::Vec3 left{direction.x * c - direction.y * s, direction.x * s + direction.y * c, direction.z};
    if (ri::math::LengthSquared(left) > 1e-10f) {
        left = ri::math::Normalize(left);
    } else {
        left = direction;
    }
    ri::math::Vec3 right{direction.x * c + direction.y * s, -direction.x * s + direction.y * c, direction.z};
    if (ri::math::LengthSquared(right) > 1e-10f) {
        right = ri::math::Normalize(right);
    } else {
        right = direction;
    }
    BranchRecursive(triangles,
                    end,
                    left,
                    depth + 1,
                    maxDepth,
                    nextLen,
                    branchScale,
                    branchAngleRad,
                    nextRadius,
                    radialSegments,
                    branchesEmitted,
                    branchBudget);
    BranchRecursive(triangles,
                    end,
                    right,
                    depth + 1,
                    maxDepth,
                    nextLen,
                    branchScale,
                    branchAngleRad,
                    nextRadius,
                    radialSegments,
                    branchesEmitted,
                    branchBudget);
}

void AppendMesh(CompiledMesh& destination, const CompiledMesh& source) {
    if (!source.hasBounds) {
        return;
    }

    destination.positions.insert(destination.positions.end(), source.positions.begin(), source.positions.end());
    destination.normals.insert(destination.normals.end(), source.normals.begin(), source.normals.end());
    destination.triangleCount += source.triangleCount;
    if (!destination.hasBounds) {
        destination.hasBounds = true;
        destination.boundsMin = source.boundsMin;
        destination.boundsMax = source.boundsMax;
        return;
    }

    destination.boundsMin.x = std::min(destination.boundsMin.x, source.boundsMin.x);
    destination.boundsMin.y = std::min(destination.boundsMin.y, source.boundsMin.y);
    destination.boundsMin.z = std::min(destination.boundsMin.z, source.boundsMin.z);
    destination.boundsMax.x = std::max(destination.boundsMax.x, source.boundsMax.x);
    destination.boundsMax.y = std::max(destination.boundsMax.y, source.boundsMax.y);
    destination.boundsMax.z = std::max(destination.boundsMax.z, source.boundsMax.z);
}

std::optional<ConvexSolid> CreateRampSolid() {
    const ri::math::Vec3 a{-0.5f, -0.5f, 0.5f};
    const ri::math::Vec3 b{-0.5f, 0.5f, 0.5f};
    const ri::math::Vec3 c{0.5f, -0.5f, 0.5f};
    const ri::math::Vec3 d{-0.5f, -0.5f, -0.5f};
    const ri::math::Vec3 e{-0.5f, 0.5f, -0.5f};
    const ri::math::Vec3 f{0.5f, -0.5f, -0.5f};

    return BuildSolidFromFaces({
        {a, b, c},
        {f, e, d},
        {d, e, b, a},
        {d, a, c, f},
        {e, f, c, b},
    });
}

std::optional<ConvexSolid> CreateWedgeSolid() {
    const ri::math::Vec3 a{-0.5f, -0.5f, -0.5f};
    const ri::math::Vec3 b{0.5f, -0.5f, -0.5f};
    const ri::math::Vec3 c{-0.5f, 0.5f, -0.5f};
    const ri::math::Vec3 d{-0.5f, -0.5f, 0.5f};
    const ri::math::Vec3 e{0.5f, -0.5f, 0.5f};
    const ri::math::Vec3 f{-0.5f, 0.5f, 0.5f};

    return BuildSolidFromFaces({
        {a, b, c},
        {f, e, d},
        {d, f, c, a},
        {e, d, a, b},
        {c, f, e, b},
    });
}

std::optional<ConvexSolid> CreateCylinderLikeSolid(int radialSegments) {
    const int segments = ClampInteger(radialSegments, 16, 3, 64);
    std::vector<ri::math::Vec3> top;
    std::vector<ri::math::Vec3> bottom;
    top.reserve(static_cast<std::size_t>(segments));
    bottom.reserve(static_cast<std::size_t>(segments));

    for (int index = 0; index < segments; ++index) {
        const float angle = (2.0f * kPi * static_cast<float>(index)) / static_cast<float>(segments);
        const float x = std::cos(angle) * kHalfExtent;
        const float z = std::sin(angle) * kHalfExtent;
        top.push_back({x, kHalfExtent, z});
        bottom.push_back({x, -kHalfExtent, z});
    }

    std::vector<std::vector<ri::math::Vec3>> faces;
    faces.reserve(static_cast<std::size_t>(segments) + 2U);
    faces.push_back(top);
    faces.push_back(std::vector<ri::math::Vec3>(bottom.rbegin(), bottom.rend()));

    for (int index = 0; index < segments; ++index) {
        const int next = (index + 1) % segments;
        faces.push_back({
            bottom[index],
            bottom[next],
            top[next],
            top[index],
        });
    }

    return BuildSolidFromFaces(faces);
}

std::optional<ConvexSolid> CreateFrustumSolid(int radialSegments, float topRadius, float bottomRadius) {
    const int segments = ClampInteger(radialSegments, 16, 3, 64);
    const float safeTopRadius = std::clamp(std::fabs(topRadius), 0.0f, kHalfExtent);
    const float safeBottomRadius = std::clamp(std::fabs(bottomRadius), 0.0f, kHalfExtent);

    std::vector<ri::math::Vec3> top;
    std::vector<ri::math::Vec3> bottom;
    top.reserve(static_cast<std::size_t>(segments));
    bottom.reserve(static_cast<std::size_t>(segments));

    for (int index = 0; index < segments; ++index) {
        const float angle = (2.0f * kPi * static_cast<float>(index)) / static_cast<float>(segments);
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        top.push_back({cosine * safeTopRadius, kHalfExtent, sine * safeTopRadius});
        bottom.push_back({cosine * safeBottomRadius, -kHalfExtent, sine * safeBottomRadius});
    }

    std::vector<std::vector<ri::math::Vec3>> faces;
    faces.reserve(static_cast<std::size_t>(segments) + 2U);
    faces.push_back(top);
    faces.push_back(std::vector<ri::math::Vec3>(bottom.rbegin(), bottom.rend()));
    for (int index = 0; index < segments; ++index) {
        const int next = (index + 1) % segments;
        faces.push_back({
            bottom[index],
            bottom[next],
            top[next],
            top[index],
        });
    }
    return BuildSolidFromFaces(faces);
}

std::optional<ConvexSolid> CreateConeLikeSolid(int sides) {
    const int segmentCount = ClampInteger(sides, 16, 3, 64);
    std::vector<ri::math::Vec3> base;
    base.reserve(static_cast<std::size_t>(segmentCount));
    for (int index = 0; index < segmentCount; ++index) {
        const float angle = (2.0f * kPi * static_cast<float>(index)) / static_cast<float>(segmentCount);
        base.push_back({std::cos(angle) * kHalfExtent, -kHalfExtent, std::sin(angle) * kHalfExtent});
    }

    const ri::math::Vec3 apex{0.0f, kHalfExtent, 0.0f};
    std::vector<std::vector<ri::math::Vec3>> faces;
    faces.reserve(static_cast<std::size_t>(segmentCount) + 1U);
    faces.push_back(std::vector<ri::math::Vec3>(base.rbegin(), base.rend()));
    for (int index = 0; index < segmentCount; ++index) {
        const int next = (index + 1) % segmentCount;
        faces.push_back({
            base[index],
            base[next],
            apex,
        });
    }
    return BuildSolidFromFaces(faces);
}

std::optional<ConvexSolid> CreateIcosahedronSolid() {
    std::vector<ri::math::Vec3> vertices = {
        {-1.0f, kGoldenRatio, 0.0f},
        {1.0f, kGoldenRatio, 0.0f},
        {-1.0f, -kGoldenRatio, 0.0f},
        {1.0f, -kGoldenRatio, 0.0f},
        {0.0f, -1.0f, kGoldenRatio},
        {0.0f, 1.0f, kGoldenRatio},
        {0.0f, -1.0f, -kGoldenRatio},
        {0.0f, 1.0f, -kGoldenRatio},
        {kGoldenRatio, 0.0f, -1.0f},
        {kGoldenRatio, 0.0f, 1.0f},
        {-kGoldenRatio, 0.0f, -1.0f},
        {-kGoldenRatio, 0.0f, 1.0f},
    };
    for (ri::math::Vec3& vertex : vertices) {
        vertex = ri::math::Normalize(vertex) * kHalfExtent;
    }

    static constexpr int kFaces[20][3] = {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1},
    };

    std::vector<std::vector<ri::math::Vec3>> faces;
    faces.reserve(20U);
    for (const auto& face : kFaces) {
        std::vector<ri::math::Vec3> triangle = {
            vertices[face[0]],
            vertices[face[1]],
            vertices[face[2]],
        };
        const std::optional<Plane> plane = ComputePolygonPlane(triangle);
        if (plane.has_value()) {
            const ri::math::Vec3 centroid = (triangle[0] + triangle[1] + triangle[2]) / 3.0f;
            if (ri::math::Dot(plane->normal, centroid) < 0.0f) {
                std::swap(triangle[1], triangle[2]);
            }
        }
        faces.push_back(std::move(triangle));
    }
    return BuildSolidFromFaces(faces);
}

std::vector<ri::math::Vec3> DeduplicatePoints(const std::vector<ri::math::Vec3>& points, float epsilon = 1e-4f) {
    std::vector<ri::math::Vec3> unique;
    const float epsilonSquared = epsilon * epsilon;
    for (const ri::math::Vec3& point : points) {
        const bool duplicate = std::any_of(unique.begin(), unique.end(), [&](const ri::math::Vec3& candidate) {
            return ri::math::DistanceSquared(candidate, point) <= epsilonSquared;
        });
        if (!duplicate) {
            unique.push_back(point);
        }
    }
    return unique;
}

bool NearlySamePlane(const Plane& lhs, const Plane& rhs, float epsilon = 1e-4f) {
    return ri::math::DistanceSquared(lhs.normal, rhs.normal) <= (epsilon * epsilon)
        && std::fabs(lhs.constant - rhs.constant) <= epsilon;
}

std::optional<ConvexSolid> CreateConvexHullSolidFromPoints(const std::vector<ri::math::Vec3>& sourcePoints) {
    const std::vector<ri::math::Vec3> points = DeduplicatePoints(sourcePoints);
    if (points.size() < 4U) {
        return std::nullopt;
    }

    struct PlaneGroup {
        Plane plane{};
        std::vector<ri::math::Vec3> points;
    };

    std::vector<PlaneGroup> groups;
    for (std::size_t a = 0; a < points.size(); ++a) {
        for (std::size_t b = a + 1; b < points.size(); ++b) {
            for (std::size_t c = b + 1; c < points.size(); ++c) {
                const std::optional<Plane> maybePlane = ComputePolygonPlane({points[a], points[b], points[c]});
                if (!maybePlane.has_value()) {
                    continue;
                }

                Plane plane = *maybePlane;
                bool hasFront = false;
                bool hasBack = false;
                for (const ri::math::Vec3& point : points) {
                    const float distance = DistanceToPlane(plane, point);
                    hasFront = hasFront || distance > 1e-4f;
                    hasBack = hasBack || distance < -1e-4f;
                    if (hasFront && hasBack) {
                        break;
                    }
                }
                if (hasFront && hasBack) {
                    continue;
                }
                if (!hasBack && hasFront) {
                    plane = NegatePlane(plane);
                }

                auto existing = std::find_if(groups.begin(), groups.end(), [&](const PlaneGroup& group) {
                    return NearlySamePlane(group.plane, plane);
                });
                if (existing == groups.end()) {
                    groups.push_back(PlaneGroup{.plane = plane, .points = {}});
                    existing = std::prev(groups.end());
                }
                for (const ri::math::Vec3& point : points) {
                    if (std::fabs(DistanceToPlane(plane, point)) <= 1e-4f) {
                        existing->points.push_back(point);
                    }
                }
            }
        }
    }

    std::vector<std::vector<ri::math::Vec3>> faces;
    for (PlaneGroup& group : groups) {
        group.points = DeduplicatePoints(group.points);
        if (group.points.size() < 3U) {
            continue;
        }
        std::vector<ri::math::Vec3> ordered = SortCoplanarPoints(group.points, group.plane);
        const std::optional<Plane> orderedPlane = ComputePolygonPlane(ordered);
        if (orderedPlane.has_value() && ri::math::Dot(orderedPlane->normal, group.plane.normal) < 0.0f) {
            std::reverse(ordered.begin(), ordered.end());
        }
        faces.push_back(std::move(ordered));
    }

    if (faces.size() < 4U) {
        return std::nullopt;
    }
    return BuildSolidFromFaces(faces);
}

std::optional<ConvexSolid> CreateHexahedronSolid(const std::vector<ri::math::Vec3>& sourceVertices) {
    if (sourceVertices.size() < 8U) {
        return std::nullopt;
    }
    const std::vector<ri::math::Vec3> vertices(sourceVertices.begin(), sourceVertices.begin() + 8U);
    const std::vector<std::vector<int>> faceIndices = {
        {0, 1, 2, 3},
        {4, 7, 6, 5},
        {0, 4, 5, 1},
        {1, 5, 6, 2},
        {2, 6, 7, 3},
        {3, 7, 4, 0},
    };

    ri::math::Vec3 solidCentroid{};
    for (const ri::math::Vec3& vertex : vertices) {
        solidCentroid = solidCentroid + vertex;
    }
    solidCentroid = solidCentroid / static_cast<float>(vertices.size());

    std::vector<std::vector<ri::math::Vec3>> faces;
    faces.reserve(faceIndices.size());
    for (const auto& face : faceIndices) {
        std::vector<ri::math::Vec3> polygon;
        polygon.reserve(face.size());
        ri::math::Vec3 faceCentroid{};
        for (int index : face) {
            polygon.push_back(vertices[static_cast<std::size_t>(index)]);
            faceCentroid = faceCentroid + vertices[static_cast<std::size_t>(index)];
        }
        faceCentroid = faceCentroid / static_cast<float>(polygon.size());
        const std::optional<Plane> plane = ComputePolygonPlane(polygon);
        if (plane.has_value() && ri::math::Dot(plane->normal, faceCentroid - solidCentroid) < 0.0f) {
            std::reverse(polygon.begin(), polygon.end());
        }
        faces.push_back(std::move(polygon));
    }
    return BuildSolidFromFaces(faces);
}

std::optional<ConvexSolid> CreateGableRoofSolid() {
    const std::vector<ri::math::Vec3> front = {
        {-0.5f, -0.5f, 0.5f},
        {-0.5f, 0.0f, 0.5f},
        {0.0f, 0.5f, 0.5f},
        {0.5f, 0.0f, 0.5f},
        {0.5f, -0.5f, 0.5f},
    };
    const std::vector<ri::math::Vec3> back = {
        {-0.5f, -0.5f, -0.5f},
        {-0.5f, 0.0f, -0.5f},
        {0.0f, 0.5f, -0.5f},
        {0.5f, 0.0f, -0.5f},
        {0.5f, -0.5f, -0.5f},
    };
    return BuildSolidFromFaces({
        front,
        std::vector<ri::math::Vec3>(back.rbegin(), back.rend()),
        {back[0], back[1], front[1], front[0]},
        {back[1], back[2], front[2], front[1]},
        {back[2], back[3], front[3], front[2]},
        {back[3], back[4], front[4], front[3]},
        {back[4], back[0], front[0], front[4]},
    });
}

std::optional<ConvexSolid> CreateHippedRoofSolid(float ridgeRatio) {
    const float ridgeHalf = std::clamp(std::fabs(ridgeRatio) * 0.5f, 0.04f, 0.42f);
    return CreateConvexHullSolidFromPoints({
        {-0.5f, -0.5f, -0.5f},
        {0.5f, -0.5f, -0.5f},
        {0.5f, -0.5f, 0.5f},
        {-0.5f, -0.5f, 0.5f},
        {-0.5f, 0.0f, -0.5f},
        {0.5f, 0.0f, -0.5f},
        {0.5f, 0.0f, 0.5f},
        {-0.5f, 0.0f, 0.5f},
        {0.0f, 0.5f, -ridgeHalf},
        {0.0f, 0.5f, ridgeHalf},
    });
}

CompiledMesh CreatePlaneMesh() {
    return BuildMeshFromTriangles({
        {-0.5f, -0.5f, 0.0f},
        {0.5f, -0.5f, 0.0f},
        {0.5f, 0.5f, 0.0f},
        {-0.5f, -0.5f, 0.0f},
        {0.5f, 0.5f, 0.0f},
        {-0.5f, 0.5f, 0.0f},
    });
}

CompiledMesh CreateHollowBoxMesh(float thickness) {
    const float safeThickness = std::clamp(std::fabs(thickness), 0.02f, 0.48f);
    const float inner = kHalfExtent - safeThickness;

    const std::vector<ConvexSolid> wallSolids = {
        CreateAxisAlignedBoxSolid({-kHalfExtent, -kHalfExtent, -kHalfExtent}, {-inner, kHalfExtent, kHalfExtent}),
        CreateAxisAlignedBoxSolid({inner, -kHalfExtent, -kHalfExtent}, {kHalfExtent, kHalfExtent, kHalfExtent}),
        CreateAxisAlignedBoxSolid({-inner, -kHalfExtent, -kHalfExtent}, {inner, -inner, kHalfExtent}),
        CreateAxisAlignedBoxSolid({-inner, inner, -kHalfExtent}, {inner, kHalfExtent, kHalfExtent}),
        CreateAxisAlignedBoxSolid({-inner, -inner, -kHalfExtent}, {inner, inner, -inner}),
        CreateAxisAlignedBoxSolid({-inner, -inner, inner}, {inner, inner, kHalfExtent}),
    };

    CompiledMesh mesh{};
    for (const ConvexSolid& wall : wallSolids) {
        AppendMesh(mesh, BuildCompiledMeshFromConvexSolid(wall));
    }
    return mesh;
}

CompiledMesh CreateCapsuleMesh(const StructuralPrimitiveOptions& options) {
    const int radialSegments = ClampInteger(options.radialSegments, 12, 6, 64);
    const int hemisphereSegments = ClampInteger(options.hemisphereSegments, 6, 2, 24);
    const float radius = 0.25f;
    const float cylinderHalfHeight = std::clamp(std::fabs(options.length) * 0.5f, 0.0f, kHalfExtent - radius);

    struct Ring {
        float y = 0.0f;
        float radius = 0.0f;
    };

    std::vector<Ring> rings;
    rings.reserve(static_cast<std::size_t>((hemisphereSegments * 2) + 1));
    for (int index = 1; index <= hemisphereSegments; ++index) {
        const float angle = (static_cast<float>(index) / static_cast<float>(hemisphereSegments)) * (kPi * 0.5f);
        rings.push_back({
            .y = cylinderHalfHeight + (std::cos(angle) * radius),
            .radius = std::sin(angle) * radius,
        });
    }
    if (cylinderHalfHeight > 0.0f) {
        rings.push_back({.y = -cylinderHalfHeight, .radius = radius});
    }
    for (int index = 1; index < hemisphereSegments; ++index) {
        const float angle = (static_cast<float>(index) / static_cast<float>(hemisphereSegments)) * (kPi * 0.5f);
        rings.push_back({
            .y = -cylinderHalfHeight - (std::sin(angle) * radius),
            .radius = std::cos(angle) * radius,
        });
    }

    std::vector<std::vector<ri::math::Vec3>> ringVertices;
    ringVertices.reserve(rings.size());
    for (const Ring& ring : rings) {
        std::vector<ri::math::Vec3> loop;
        loop.reserve(static_cast<std::size_t>(radialSegments));
        for (int segment = 0; segment < radialSegments; ++segment) {
            const float angle = (2.0f * kPi * static_cast<float>(segment)) / static_cast<float>(radialSegments);
            loop.push_back({
                std::cos(angle) * ring.radius,
                ring.y,
                std::sin(angle) * ring.radius,
            });
        }
        ringVertices.push_back(std::move(loop));
    }

    const ri::math::Vec3 topTip{0.0f, cylinderHalfHeight + radius, 0.0f};
    const ri::math::Vec3 bottomTip{0.0f, -cylinderHalfHeight - radius, 0.0f};
    std::vector<ri::math::Vec3> triangles;
    triangles.reserve(static_cast<std::size_t>(radialSegments * ((rings.size() + 1U) * 6U)));

    for (int segment = 0; segment < radialSegments; ++segment) {
        const int next = (segment + 1) % radialSegments;
        AppendOutwardTriangle(triangles, topTip, ringVertices.front()[segment], ringVertices.front()[next]);
    }
    for (std::size_t ringIndex = 0; ringIndex + 1U < ringVertices.size(); ++ringIndex) {
        const auto& current = ringVertices[ringIndex];
        const auto& next = ringVertices[ringIndex + 1U];
        for (int segment = 0; segment < radialSegments; ++segment) {
            const int nextSegment = (segment + 1) % radialSegments;
            AppendOutwardQuad(
                triangles,
                current[segment],
                current[nextSegment],
                next[nextSegment],
                next[segment]);
        }
    }
    for (int segment = 0; segment < radialSegments; ++segment) {
        const int next = (segment + 1) % radialSegments;
        AppendOutwardTriangle(triangles, bottomTip, ringVertices.back()[next], ringVertices.back()[segment]);
    }

    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateFrustumMesh(const StructuralPrimitiveOptions& options) {
    const std::optional<ConvexSolid> solid =
        CreateFrustumSolid(options.radialSegments, options.topRadius, options.bottomRadius);
    return BuildCompiledMeshFromConvexSolid(solid.value_or(CreateAxisAlignedBoxSolid()));
}

void SubdivideSphereTriangles(std::vector<ri::math::Vec3>& triangles) {
    std::vector<ri::math::Vec3> next;
    next.reserve(triangles.size() * 4U);
    for (std::size_t i = 0; i + 2U < triangles.size(); i += 3U) {
        const ri::math::Vec3 a = ri::math::Normalize(triangles[i]) * kHalfExtent;
        const ri::math::Vec3 b = ri::math::Normalize(triangles[i + 1U]) * kHalfExtent;
        const ri::math::Vec3 c = ri::math::Normalize(triangles[i + 2U]) * kHalfExtent;
        const ri::math::Vec3 ab = ri::math::Normalize((a + b) * 0.5f) * kHalfExtent;
        const ri::math::Vec3 bc = ri::math::Normalize((b + c) * 0.5f) * kHalfExtent;
        const ri::math::Vec3 ca = ri::math::Normalize((c + a) * 0.5f) * kHalfExtent;
        AppendOutwardTriangle(next, a, ab, ca);
        AppendOutwardTriangle(next, b, bc, ab);
        AppendOutwardTriangle(next, c, ca, bc);
        AppendOutwardTriangle(next, ab, bc, ca);
    }
    triangles = std::move(next);
}

CompiledMesh CreateGeodesicSphereMesh(const StructuralPrimitiveOptions& options) {
    const std::optional<ConvexSolid> solid = CreateIcosahedronSolid();
    if (!solid.has_value()) {
        return BuildCompiledMeshFromConvexSolid(CreateAxisAlignedBoxSolid());
    }
    std::vector<ri::math::Vec3> triangles;
    triangles.reserve(60U);
    for (const ConvexPolygon& polygon : solid->polygons) {
        if (polygon.vertices.size() == 3U) {
            AppendOutwardTriangle(triangles, polygon.vertices[0], polygon.vertices[1], polygon.vertices[2]);
        }
    }
    const int detail = ClampRange(options.detail, 0, 4);
    for (int i = 0; i < detail; ++i) {
        SubdivideSphereTriangles(triangles);
    }
    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateSuperellipsoidMesh(const StructuralPrimitiveOptions& options) {
    const int latSegments = ClampInteger(options.radialSegments, 16, 6, 96);
    const int lonSegments = ClampInteger(options.sides, 24, 8, 128);
    const float exponentX = std::clamp(options.exponentX, 0.05f, 8.0f);
    const float exponentY = std::clamp(options.exponentY, 0.05f, 8.0f);
    const float exponentZ = std::clamp(options.exponentZ, 0.05f, 8.0f);

    const auto sample = [&](float theta, float phi) {
        const float cosPhi = std::cos(phi);
        return ri::math::Vec3{
            SignedPower(cosPhi * std::cos(theta), exponentX) * kHalfExtent,
            SignedPower(std::sin(phi), exponentY) * kHalfExtent,
            SignedPower(cosPhi * std::sin(theta), exponentZ) * kHalfExtent,
        };
    };

    std::vector<ri::math::Vec3> triangles;
    triangles.reserve(static_cast<std::size_t>(latSegments * lonSegments * 6));
    for (int lat = 0; lat < latSegments; ++lat) {
        const float v0 = -0.5f + (static_cast<float>(lat) / static_cast<float>(latSegments));
        const float v1 = -0.5f + (static_cast<float>(lat + 1) / static_cast<float>(latSegments));
        const float phi0 = v0 * kPi;
        const float phi1 = v1 * kPi;
        for (int lon = 0; lon < lonSegments; ++lon) {
            const float theta0 = (2.0f * kPi * static_cast<float>(lon)) / static_cast<float>(lonSegments);
            const float theta1 = (2.0f * kPi * static_cast<float>(lon + 1)) / static_cast<float>(lonSegments);
            const ri::math::Vec3 a = sample(theta0, phi0);
            const ri::math::Vec3 b = sample(theta1, phi0);
            const ri::math::Vec3 c = sample(theta1, phi1);
            const ri::math::Vec3 d = sample(theta0, phi1);
            AppendOutwardTriangle(triangles, a, b, c);
            AppendOutwardTriangle(triangles, a, c, d);
        }
    }
    return BuildMeshFromTriangles(triangles);
}

std::vector<ri::math::Vec3> BuildRoundArchOuterLoop(int segments, float spanDegrees) {
    const int safeSegments = ClampInteger(segments, 18, 4, 64);
    const float safeSpan = std::clamp(spanDegrees, 40.0f, 360.0f);
    const float spanRadians = safeSpan * (kPi / 180.0f);
    const float startAngle = (kPi * 0.5f) + (spanRadians * 0.5f);
    const float endAngle = (kPi * 0.5f) - (spanRadians * 0.5f);

    std::vector<ri::math::Vec3> loop;
    loop.reserve(static_cast<std::size_t>(safeSegments) + 3U);
    loop.push_back({-0.5f, -0.5f, 0.0f});
    for (int index = 0; index <= safeSegments; ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(safeSegments);
        const float angle = startAngle + ((endAngle - startAngle) * t);
        loop.push_back({std::cos(angle) * 0.5f, std::sin(angle) * 0.5f, 0.0f});
    }
    loop.push_back({0.5f, -0.5f, 0.0f});
    return loop;
}

std::vector<ri::math::Vec3> BuildRoundArchInnerLoop(int segments, float spanDegrees, float thickness) {
    const int safeSegments = ClampInteger(segments, 18, 4, 64);
    const float safeSpan = std::clamp(spanDegrees, 40.0f, 360.0f);
    const float spanRadians = safeSpan * (kPi / 180.0f);
    const float startAngle = (kPi * 0.5f) + (spanRadians * 0.5f);
    const float endAngle = (kPi * 0.5f) - (spanRadians * 0.5f);
    const float innerRadius = std::clamp(0.5f - thickness, 0.03f, 0.48f);

    std::vector<ri::math::Vec3> loop;
    loop.reserve(static_cast<std::size_t>(safeSegments) + 3U);
    loop.push_back({-0.5f + thickness, -0.5f, 0.0f});
    for (int index = 0; index <= safeSegments; ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(safeSegments);
        const float angle = startAngle + ((endAngle - startAngle) * t);
        loop.push_back({std::cos(angle) * innerRadius, std::sin(angle) * innerRadius, 0.0f});
    }
    loop.push_back({0.5f - thickness, -0.5f, 0.0f});
    return loop;
}

std::vector<ri::math::Vec3> BuildGothicArchOuterLoop() {
    return {
        {-0.5f, -0.5f, 0.0f},
        {-0.5f, -0.06f, 0.0f},
        {0.0f, 0.5f, 0.0f},
        {0.5f, -0.06f, 0.0f},
        {0.5f, -0.5f, 0.0f},
    };
}

std::vector<ri::math::Vec3> BuildGothicArchInnerLoop(float thickness) {
    const float safeThickness = std::clamp(std::fabs(thickness), 0.04f, 0.38f);
    return {
        {-0.5f + safeThickness, -0.5f, 0.0f},
        {-0.5f + safeThickness, -0.06f + (safeThickness * 0.65f), 0.0f},
        {0.0f, 0.5f - (safeThickness * 0.95f), 0.0f},
        {0.5f - safeThickness, -0.06f + (safeThickness * 0.65f), 0.0f},
        {0.5f - safeThickness, -0.5f, 0.0f},
    };
}

CompiledMesh CreateExtrudedBandMesh(const std::vector<ri::math::Vec3>& outerLoop,
                                    const std::vector<ri::math::Vec3>& innerLoop) {
    if (outerLoop.size() < 3U || innerLoop.size() < 3U || outerLoop.size() != innerLoop.size()) {
        return BuildCompiledMeshFromConvexSolid(CreateAxisAlignedBoxSolid());
    }

    constexpr float kHalfDepth = 0.5f;
    std::vector<ri::math::Vec3> triangles;
    triangles.reserve(outerLoop.size() * 24U);
    const std::size_t count = outerLoop.size();
    for (std::size_t index = 0; index < count; ++index) {
        const std::size_t next = (index + 1U) % count;
        const ri::math::Vec3 outerFrontA{outerLoop[index].x, outerLoop[index].y, kHalfDepth};
        const ri::math::Vec3 outerFrontB{outerLoop[next].x, outerLoop[next].y, kHalfDepth};
        const ri::math::Vec3 innerFrontA{innerLoop[index].x, innerLoop[index].y, kHalfDepth};
        const ri::math::Vec3 innerFrontB{innerLoop[next].x, innerLoop[next].y, kHalfDepth};
        const ri::math::Vec3 outerBackA{outerLoop[index].x, outerLoop[index].y, -kHalfDepth};
        const ri::math::Vec3 outerBackB{outerLoop[next].x, outerLoop[next].y, -kHalfDepth};
        const ri::math::Vec3 innerBackA{innerLoop[index].x, innerLoop[index].y, -kHalfDepth};
        const ri::math::Vec3 innerBackB{innerLoop[next].x, innerLoop[next].y, -kHalfDepth};

        AppendQuadFacing(triangles, outerFrontA, outerFrontB, innerFrontB, innerFrontA, {0.0f, 0.0f, 1.0f});
        AppendQuadFacing(triangles, outerBackA, innerBackA, innerBackB, outerBackB, {0.0f, 0.0f, -1.0f});

        const ri::math::Vec3 outerMid = ri::math::Normalize((outerLoop[index] + outerLoop[next]) * 0.5f);
        AppendQuadFacing(triangles, outerFrontB, outerFrontA, outerBackA, outerBackB, {outerMid.x, outerMid.y, 0.0f});

        const ri::math::Vec3 innerMid = ri::math::Normalize((innerLoop[index] + innerLoop[next]) * -0.5f);
        AppendQuadFacing(triangles, innerFrontA, innerFrontB, innerBackB, innerBackA, {innerMid.x, innerMid.y, 0.0f});
    }
    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateArchMesh(const StructuralPrimitiveOptions& options) {
    const float thickness = std::clamp(std::fabs(options.thickness), 0.04f, 0.45f);
    const bool gothic = options.archStyle == "gothic";
    const std::vector<ri::math::Vec3> outerLoop = gothic
        ? BuildGothicArchOuterLoop()
        : BuildRoundArchOuterLoop(options.radialSegments, options.spanDegrees);
    const std::vector<ri::math::Vec3> innerLoop = gothic
        ? BuildGothicArchInnerLoop(thickness)
        : BuildRoundArchInnerLoop(options.radialSegments, options.spanDegrees, thickness);
    return CreateExtrudedBandMesh(outerLoop, innerLoop);
}

std::vector<ri::math::Vec3> SanitizeProfileLoop3d(const std::vector<ri::math::Vec3>& points) {
    std::vector<ri::math::Vec3> loop;
    loop.reserve(std::min<std::size_t>(points.size(), 128U));
    for (const ri::math::Vec3& point : points) {
        if (loop.size() >= 128U) {
            break;
        }
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            continue;
        }
        const ri::math::Vec3 clamped{
            std::clamp(point.x, -1.0f, 1.0f),
            std::clamp(point.y, -1.0f, 1.0f),
            0.0f,
        };
        if (!loop.empty() && ri::math::DistanceSquared(loop.back(), clamped) <= 1e-8f) {
            continue;
        }
        loop.push_back(clamped);
    }
    if (loop.size() > 1U && ri::math::DistanceSquared(loop.front(), loop.back()) <= 1e-8f) {
        loop.pop_back();
    }
    if (loop.size() < 3U) {
        return {
            {-0.5f, -0.5f, 0.0f},
            {0.5f, -0.5f, 0.0f},
            {0.5f, 0.5f, 0.0f},
            {-0.5f, 0.5f, 0.0f},
        };
    }
    return loop;
}

CompiledMesh CreateExtrudeAlongNormalMesh(const StructuralPrimitiveOptions& options) {
    const std::vector<ri::math::Vec3> loop = SanitizeProfileLoop3d(options.points);
    const float halfDepth = std::clamp(std::fabs(options.depth), 0.01f, 2.0f) * 0.5f;
    std::vector<ri::math::Vec3> triangles;
    triangles.reserve(loop.size() * 12U);

    const ri::math::Vec3 frontCenter{0.0f, 0.0f, halfDepth};
    const ri::math::Vec3 backCenter{0.0f, 0.0f, -halfDepth};
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const std::size_t next = (i + 1U) % loop.size();
        const ri::math::Vec3 frontA{loop[i].x, loop[i].y, halfDepth};
        const ri::math::Vec3 frontB{loop[next].x, loop[next].y, halfDepth};
        const ri::math::Vec3 backA{loop[i].x, loop[i].y, -halfDepth};
        const ri::math::Vec3 backB{loop[next].x, loop[next].y, -halfDepth};
        AppendQuadFacing(triangles, frontA, backA, backB, frontB, ri::math::Normalize(frontA + frontB));
        AppendTriangle(triangles, frontCenter, frontA, frontB);
        AppendTriangle(triangles, backCenter, backB, backA);
    }
    return BuildMeshFromTriangles(triangles);
}

std::tuple<int, int, int> QuantizedPointKey(const ri::math::Vec3& point) {
    return {
        static_cast<int>(std::lround(point.x * 10000.0f)),
        static_cast<int>(std::lround(point.y * 10000.0f)),
        static_cast<int>(std::lround(point.z * 10000.0f)),
    };
}

void AppendBeamMesh(std::vector<ri::math::Vec3>& triangles,
                    const ri::math::Vec3& a,
                    const ri::math::Vec3& b,
                    float radius) {
    const ri::math::Vec3 axis = b - a;
    if (ri::math::LengthSquared(axis) <= 1e-8f) {
        return;
    }
    const ri::math::Vec3 forward = ri::math::Normalize(axis);
    const ri::math::Vec3 upHint = std::fabs(forward.y) < 0.9f ? ri::math::Vec3{0.0f, 1.0f, 0.0f} : ri::math::Vec3{1.0f, 0.0f, 0.0f};
    const ri::math::Vec3 right = ri::math::Normalize(ri::math::Cross(upHint, forward)) * radius;
    const ri::math::Vec3 up = ri::math::Normalize(ri::math::Cross(forward, right)) * radius;

    const ri::math::Vec3 a0 = a - right - up;
    const ri::math::Vec3 a1 = a + right - up;
    const ri::math::Vec3 a2 = a + right + up;
    const ri::math::Vec3 a3 = a - right + up;
    const ri::math::Vec3 b0 = b - right - up;
    const ri::math::Vec3 b1 = b + right - up;
    const ri::math::Vec3 b2 = b + right + up;
    const ri::math::Vec3 b3 = b - right + up;

    AppendQuadFacing(triangles, a0, a1, b1, b0, up * -1.0f);
    AppendQuadFacing(triangles, a1, a2, b2, b1, right);
    AppendQuadFacing(triangles, a2, a3, b3, b2, up);
    AppendQuadFacing(triangles, a3, a0, b0, b3, right * -1.0f);
    AppendQuadFacing(triangles, a3, a2, a1, a0, forward * -1.0f);
    AppendQuadFacing(triangles, b0, b1, b2, b3, forward);
}

CompiledMesh CreateLatticeVolumeMesh(const StructuralPrimitiveOptions& options) {
    const int cellsX = ClampRange(options.cellsX > 0 ? options.cellsX : options.radialSegments, 1, 12);
    const int cellsY = ClampRange(options.cellsY > 0 ? options.cellsY : options.detail, 1, 12);
    const int cellsZ = ClampRange(options.cellsZ > 0 ? options.cellsZ : options.sides, 1, 12);
    const float radius = std::clamp(std::fabs(options.strutRadius), 0.004f, 0.12f);
    const std::string style = options.latticeStyle.empty() ? "x_brace" : options.latticeStyle;

    std::set<std::pair<std::tuple<int, int, int>, std::tuple<int, int, int>>> emitted;
    std::vector<ri::math::Vec3> triangles;
    const auto pointAt = [&](int x, int y, int z) {
        return ri::math::Vec3{
            -0.5f + (static_cast<float>(x) / static_cast<float>(cellsX)),
            -0.5f + (static_cast<float>(y) / static_cast<float>(cellsY)),
            -0.5f + (static_cast<float>(z) / static_cast<float>(cellsZ)),
        };
    };
    const auto add = [&](const ri::math::Vec3& a, const ri::math::Vec3& b) {
        auto ka = QuantizedPointKey(a);
        auto kb = QuantizedPointKey(b);
        if (kb < ka) {
            std::swap(ka, kb);
        }
        if (!emitted.insert({ka, kb}).second) {
            return;
        }
        AppendBeamMesh(triangles, a, b, radius);
    };

    for (int x = 0; x < cellsX; ++x) {
        for (int y = 0; y < cellsY; ++y) {
            for (int z = 0; z < cellsZ; ++z) {
                const ri::math::Vec3 p000 = pointAt(x, y, z);
                const ri::math::Vec3 p100 = pointAt(x + 1, y, z);
                const ri::math::Vec3 p010 = pointAt(x, y + 1, z);
                const ri::math::Vec3 p001 = pointAt(x, y, z + 1);
                const ri::math::Vec3 p110 = pointAt(x + 1, y + 1, z);
                const ri::math::Vec3 p101 = pointAt(x + 1, y, z + 1);
                const ri::math::Vec3 p011 = pointAt(x, y + 1, z + 1);
                const ri::math::Vec3 p111 = pointAt(x + 1, y + 1, z + 1);
                add(p000, p100);
                add(p000, p010);
                add(p000, p001);
                add(p100, p110);
                add(p100, p101);
                add(p010, p110);
                add(p010, p011);
                add(p001, p101);
                add(p001, p011);
                add(p111, p110);
                add(p111, p101);
                add(p111, p011);
                if (style == "octet_truss") {
                    const ri::math::Vec3 center = (p000 + p111) * 0.5f;
                    add(center, p000); add(center, p100); add(center, p010); add(center, p001);
                    add(center, p110); add(center, p101); add(center, p011); add(center, p111);
                } else if (style == "k_brace") {
                    const ri::math::Vec3 midBottom = (p000 + p100) * 0.5f;
                    const ri::math::Vec3 midTop = (p011 + p111) * 0.5f;
                    add(midBottom, p010); add(midBottom, p110);
                    add(midTop, p001); add(midTop, p101);
                } else {
                    add(p000, p111);
                    add(p100, p011);
                    add(p010, p101);
                    add(p001, p110);
                }
            }
        }
    }

    return BuildMeshFromTriangles(triangles);
}

float ClampDegrees(float value, float fallback, float minValue = 1.0f, float maxValue = 360.0f) {
    if (!std::isfinite(value) || value <= 0.0f) {
        value = fallback;
    }
    return std::clamp(value, minValue, maxValue);
}

CompiledMesh CreateArcChannelMesh(float spanDegrees, const StructuralPrimitiveOptions& options) {
    const int segments = ClampRange(options.radialSegments, 6, 96);
    const float outerRadius = 0.5f;
    const float wall = std::clamp(std::fabs(options.thickness), 0.025f, 0.22f);
    const float innerRadius = std::max(0.05f, outerRadius - wall);
    const float halfSpan = ClampDegrees(spanDegrees, spanDegrees, 10.0f, 270.0f) * 0.5f * (kPi / 180.0f);

    std::vector<ri::math::Vec3> outerLoop;
    std::vector<ri::math::Vec3> innerLoop;
    outerLoop.reserve(static_cast<std::size_t>(segments + 1));
    innerLoop.reserve(static_cast<std::size_t>(segments + 1));
    for (int index = 0; index <= segments; ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(segments);
        const float angle = -halfSpan + (2.0f * halfSpan * t);
        outerLoop.push_back({std::sin(angle) * outerRadius, std::cos(angle) * outerRadius, 0.0f});
        innerLoop.push_back({std::sin(angle) * innerRadius, std::cos(angle) * innerRadius, 0.0f});
    }
    return CreateExtrudedBandMesh(outerLoop, innerLoop);
}

CompiledMesh CreateTorusArcMesh(const StructuralPrimitiveOptions& options, float fallbackDegrees) {
    const int arcSegments = ClampRange(options.radialSegments, 6, 128);
    const int tubeSegments = ClampRange(options.sides, 6, 48);
    const float sweep = ClampDegrees(options.sweepDegrees > 0.0f ? options.sweepDegrees : options.spanDegrees,
                                     fallbackDegrees,
                                     5.0f,
                                     360.0f) * (kPi / 180.0f);
    const float majorRadius = std::clamp(std::fabs(options.length), 0.08f, 0.45f);
    const float tubeRadius = std::clamp(std::fabs(options.thickness), 0.015f, 0.18f);
    std::vector<ri::math::Vec3> triangles;
    triangles.reserve(static_cast<std::size_t>(arcSegments * tubeSegments * 6));

    const auto pointAt = [&](int arcIndex, int tubeIndex) {
        const float u = (static_cast<float>(arcIndex) / static_cast<float>(arcSegments)) * sweep;
        const float v = (static_cast<float>(tubeIndex) / static_cast<float>(tubeSegments)) * 2.0f * kPi;
        const float ringRadius = majorRadius + std::cos(v) * tubeRadius;
        return ri::math::Vec3{
            std::cos(u) * ringRadius,
            std::sin(v) * tubeRadius,
            std::sin(u) * ringRadius,
        };
    };

    for (int arc = 0; arc < arcSegments; ++arc) {
        for (int tube = 0; tube < tubeSegments; ++tube) {
            const int nextTube = (tube + 1) % tubeSegments;
            AppendOutwardQuad(triangles,
                              pointAt(arc, tube),
                              pointAt(arc + 1, tube),
                              pointAt(arc + 1, nextTube),
                              pointAt(arc, nextTube));
        }
    }
    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateSplineSweepMesh(const StructuralPrimitiveOptions& options) {
    std::vector<ri::math::Vec3> path;
    path.reserve(options.points.size());
    for (const ri::math::Vec3& point : options.points) {
        if (std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z)) {
            path.push_back({
                std::clamp(point.x, -0.5f, 0.5f),
                std::clamp(point.y, -0.5f, 0.5f),
                std::clamp(point.z, -0.5f, 0.5f),
            });
        }
    }
    if (path.size() < 2U) {
        path = {{-0.5f, 0.0f, -0.5f}, {0.5f, 0.0f, 0.5f}};
    }

    const float radius = std::clamp(std::fabs(options.thickness), 0.01f, 0.12f);
    std::vector<ri::math::Vec3> triangles;
    for (std::size_t index = 1; index < path.size(); ++index) {
        AppendBeamMesh(triangles, path[index - 1U], path[index], radius);
    }
    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateRevolveMesh(const StructuralPrimitiveOptions& options) {
    const int segments = ClampRange(options.radialSegments, 6, 128);
    const float sweep = ClampDegrees(options.sweepDegrees > 0.0f ? options.sweepDegrees : options.spanDegrees,
                                     360.0f,
                                     5.0f,
                                     360.0f) * (kPi / 180.0f);
    std::vector<ri::math::Vec3> profile = SanitizeProfileLoop3d(options.points);
    for (ri::math::Vec3& point : profile) {
        point.x = std::clamp(std::fabs(point.x), 0.01f, 0.5f);
        point.y = std::clamp(point.y, -0.5f, 0.5f);
        point.z = 0.0f;
    }

    std::vector<ri::math::Vec3> triangles;
    triangles.reserve(profile.size() * static_cast<std::size_t>(segments) * 6U);
    const auto pointAt = [&](int segment, std::size_t profileIndex) {
        const float u = (static_cast<float>(segment) / static_cast<float>(segments)) * sweep;
        const ri::math::Vec3 p = profile[profileIndex % profile.size()];
        return ri::math::Vec3{std::cos(u) * p.x, p.y, std::sin(u) * p.x};
    };
    for (int segment = 0; segment < segments; ++segment) {
        for (std::size_t profileIndex = 0; profileIndex < profile.size(); ++profileIndex) {
            const std::size_t nextProfile = (profileIndex + 1U) % profile.size();
            AppendOutwardQuad(triangles,
                              pointAt(segment, profileIndex),
                              pointAt(segment + 1, profileIndex),
                              pointAt(segment + 1, nextProfile),
                              pointAt(segment, nextProfile));
        }
    }
    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateDomeVaultMesh(const StructuralPrimitiveOptions& options) {
    const int radial = ClampRange(options.radialSegments, 8, 128);
    const int rings = ClampRange(options.sides > 0 ? options.sides : options.detail, 4, 64);
    const float heightRatio = std::clamp(options.ridgeRatio > 0.0f ? options.ridgeRatio : 0.5f, 0.15f, 1.0f);
    std::vector<ri::math::Vec3> triangles;
    triangles.reserve(static_cast<std::size_t>(radial * rings * 6));
    const auto pointAt = [&](int ring, int segment) {
        const float theta = (static_cast<float>(ring) / static_cast<float>(rings)) * (kPi * 0.5f);
        const float phi = (static_cast<float>(segment) / static_cast<float>(radial)) * 2.0f * kPi;
        return ri::math::Vec3{
            std::sin(theta) * std::cos(phi) * 0.5f,
            std::cos(theta) * 0.5f * heightRatio,
            std::sin(theta) * std::sin(phi) * 0.5f,
        };
    };
    for (int ring = 0; ring < rings; ++ring) {
        for (int segment = 0; segment < radial; ++segment) {
            const int nextSegment = (segment + 1) % radial;
            AppendOutwardQuad(triangles,
                              pointAt(ring, segment),
                              pointAt(ring + 1, segment),
                              pointAt(ring + 1, nextSegment),
                              pointAt(ring, nextSegment));
        }
    }
    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateLoftMesh(const StructuralPrimitiveOptions& options) {
    const std::vector<ri::math::Vec3> profile = SanitizeProfileLoop3d(options.points);
    std::vector<ri::math::Vec3> path;
    path.reserve(options.vertices.size());
    for (const ri::math::Vec3& point : options.vertices) {
        if (std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z)) {
            path.push_back({
                std::clamp(point.x, -0.5f, 0.5f),
                std::clamp(point.y, -0.5f, 0.5f),
                std::clamp(point.z, -0.5f, 0.5f),
            });
        }
    }
    if (path.size() < 2U) {
        path = {{0.0f, 0.0f, -0.5f}, {0.0f, 0.0f, 0.5f}};
    }

    std::vector<ri::math::Vec3> triangles;
    triangles.reserve((path.size() - 1U) * profile.size() * 6U);
    const auto pointAt = [&](std::size_t pathIndex, std::size_t profileIndex) {
        const ri::math::Vec3 p = profile[profileIndex % profile.size()] * 0.5f;
        return path[pathIndex % path.size()] + ri::math::Vec3{p.x, p.y, 0.0f};
    };
    for (std::size_t pathIndex = 0; pathIndex + 1U < path.size(); ++pathIndex) {
        for (std::size_t profileIndex = 0; profileIndex < profile.size(); ++profileIndex) {
            const std::size_t nextProfile = (profileIndex + 1U) % profile.size();
            AppendOutwardQuad(triangles,
                              pointAt(pathIndex, profileIndex),
                              pointAt(pathIndex + 1U, profileIndex),
                              pointAt(pathIndex + 1U, nextProfile),
                              pointAt(pathIndex, nextProfile));
        }
    }
    return BuildMeshFromTriangles(triangles);
}

std::vector<ri::math::Vec3> SanitizePath3d(const std::vector<ri::math::Vec3>& points,
                                           const std::vector<ri::math::Vec3>& fallback) {
    std::vector<ri::math::Vec3> path;
    path.reserve(std::min<std::size_t>(points.size(), 128U));
    for (const ri::math::Vec3& point : points) {
        if (path.size() >= 128U) {
            break;
        }
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
            continue;
        }
        const ri::math::Vec3 clamped{
            std::clamp(point.x, -0.5f, 0.5f),
            std::clamp(point.y, -0.5f, 0.5f),
            std::clamp(point.z, -0.5f, 0.5f),
        };
        if (!path.empty() && ri::math::DistanceSquared(path.back(), clamped) <= 1e-8f) {
            continue;
        }
        path.push_back(clamped);
    }
    return path.size() >= 2U ? path : fallback;
}

CompiledMesh CreateRibbonMesh(const StructuralPrimitiveOptions& options) {
    const std::vector<ri::math::Vec3> path = SanitizePath3d(options.points, {
        {-0.5f, 0.0f, -0.25f},
        {0.0f, 0.05f, 0.0f},
        {0.5f, 0.0f, 0.25f},
    });
    const float halfWidth = std::clamp(std::fabs(options.thickness), 0.01f, 0.25f);
    std::vector<ri::math::Vec3> triangles;
    triangles.reserve((path.size() - 1U) * 6U);

    for (std::size_t index = 1; index < path.size(); ++index) {
        const ri::math::Vec3 a = path[index - 1U];
        const ri::math::Vec3 b = path[index];
        const ri::math::Vec3 forward = ri::math::Normalize(b - a);
        const ri::math::Vec3 side = ri::math::Normalize(ri::math::Cross({0.0f, 1.0f, 0.0f}, forward)) * halfWidth;
        const ri::math::Vec3 safeSide = ri::math::LengthSquared(side) > 1e-8f ? side : ri::math::Vec3{halfWidth, 0.0f, 0.0f};
        AppendQuadFacing(triangles, a - safeSide, b - safeSide, b + safeSide, a + safeSide, {0.0f, 1.0f, 0.0f});
    }
    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateCableLikeMesh(const StructuralPrimitiveOptions& options, bool catenary) {
    const int segments = ClampRange(options.radialSegments, 4, 96);
    const float sag = std::clamp(std::fabs(options.depth), 0.0f, 0.45f);
    const float radius = std::clamp(std::fabs(options.thickness), 0.006f, 0.08f);
    std::vector<ri::math::Vec3> path;
    path.reserve(static_cast<std::size_t>(segments + 1));
    for (int index = 0; index <= segments; ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(segments);
        const float x = -0.5f + t;
        const float arc = catenary ? (4.0f * (t - 0.5f) * (t - 0.5f)) : std::sin(t * kPi);
        const float y = catenary ? -sag * (1.0f - arc) : -sag * arc;
        path.push_back({x, y, 0.0f});
    }

    std::vector<ri::math::Vec3> triangles;
    for (std::size_t index = 1; index < path.size(); ++index) {
        AppendBeamMesh(triangles, path[index - 1U], path[index], radius);
    }
    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateThickPolygonMesh(const StructuralPrimitiveOptions& options) {
    const std::vector<ri::math::Vec3> loop = SanitizeProfileLoop3d(options.points);
    const float halfHeight = std::clamp(std::fabs(options.depth), 0.02f, 1.0f) * 0.5f;
    std::vector<ri::math::Vec3> triangles;
    triangles.reserve(loop.size() * 12U);
    const ri::math::Vec3 topCenter{0.0f, halfHeight, 0.0f};
    const ri::math::Vec3 bottomCenter{0.0f, -halfHeight, 0.0f};
    for (std::size_t index = 0; index < loop.size(); ++index) {
        const std::size_t next = (index + 1U) % loop.size();
        const ri::math::Vec3 topA{loop[index].x, halfHeight, loop[index].y};
        const ri::math::Vec3 topB{loop[next].x, halfHeight, loop[next].y};
        const ri::math::Vec3 bottomA{loop[index].x, -halfHeight, loop[index].y};
        const ri::math::Vec3 bottomB{loop[next].x, -halfHeight, loop[next].y};
        AppendQuadFacing(triangles, bottomA, bottomB, topB, topA, ri::math::Normalize(topA + topB));
        AppendTriangle(triangles, topCenter, topA, topB);
        AppendTriangle(triangles, bottomCenter, bottomB, bottomA);
    }
    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateTrimSheetSweepMesh(const StructuralPrimitiveOptions& options) {
    StructuralPrimitiveOptions sweep = options;
    sweep.thickness = std::clamp(std::fabs(options.thickness), 0.012f, 0.1f);
    return CreateSplineSweepMesh(sweep);
}

CompiledMesh CreateWaterSurfaceMesh(const StructuralPrimitiveOptions& options) {
    const int segments = ClampRange(options.radialSegments, 2, 64);
    const float amplitude = std::clamp(std::fabs(options.thickness), 0.0f, 0.08f);
    std::vector<ri::math::Vec3> triangles;
    triangles.reserve(static_cast<std::size_t>(segments * segments * 6));
    const auto pointAt = [&](int x, int z) {
        const float fx = -0.5f + (static_cast<float>(x) / static_cast<float>(segments));
        const float fz = -0.5f + (static_cast<float>(z) / static_cast<float>(segments));
        const float wave = std::sin((fx + fz) * kPi * 2.0f) * amplitude;
        return ri::math::Vec3{fx, wave, fz};
    };
    for (int x = 0; x < segments; ++x) {
        for (int z = 0; z < segments; ++z) {
            AppendQuadFacing(triangles, pointAt(x, z), pointAt(x + 1, z), pointAt(x + 1, z + 1), pointAt(x, z + 1), {0.0f, 1.0f, 0.0f});
        }
    }
    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateHeightfieldMesh(const StructuralPrimitiveOptions& options, bool terrain) {
    const int xSegments = ClampRange(options.cellsX > 0 ? options.cellsX : options.radialSegments, 2, 64);
    const int zSegments = ClampRange(options.cellsZ > 0 ? options.cellsZ : options.sides, 2, 64);
    const std::size_t expectedSamples =
        static_cast<std::size_t>((xSegments + 1) * (zSegments + 1));
    const bool useSamples =
        !options.heightfieldSamples.empty() && options.heightfieldSamples.size() == expectedSamples;
    const float amplitude = std::clamp(std::fabs(options.depth), 0.02f, 0.5f);
    std::vector<ri::math::Vec3> triangles;
    triangles.reserve(static_cast<std::size_t>(xSegments * zSegments * 6));
    const auto pointAt = [&](int x, int z) {
        const float fx = -0.5f + (static_cast<float>(x) / static_cast<float>(xSegments));
        const float fz = -0.5f + (static_cast<float>(z) / static_cast<float>(zSegments));
        float ridge = 0.0f;
        if (useSamples) {
            const std::size_t index = static_cast<std::size_t>(z * (xSegments + 1) + x);
            ridge = std::clamp(options.heightfieldSamples[index], -1.0f, 1.0f);
        } else {
            ridge = terrain ? std::sin(fx * kPi * 2.0f) * std::cos(fz * kPi * 2.0f)
                            : std::sin((fx * 2.7f + fz * 1.3f) * kPi);
        }
        return ri::math::Vec3{fx, ridge * amplitude, fz};
    };
    for (int x = 0; x < xSegments; ++x) {
        for (int z = 0; z < zSegments; ++z) {
            AppendQuadFacing(triangles, pointAt(x, z), pointAt(x, z + 1), pointAt(x + 1, z + 1), pointAt(x + 1, z), {0.0f, 1.0f, 0.0f});
        }
    }
    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateVoronoiFractureMesh(const StructuralPrimitiveOptions& options) {
    const int seedCount = ClampInteger(options.detail > 0 ? options.detail : 14, 14, 1, 128);
    const float gapThickness =
        options.thickness > 1e-6f ? std::clamp(std::fabs(options.thickness), 0.0f, 0.2f) : 0.04f;
    const float jitter =
        options.strutRadius > 1e-6f ? std::clamp(std::fabs(options.strutRadius), 0.0f, 0.45f) : 0.18f;
    const int cellsPerAxis = std::max(1, static_cast<int>(std::ceil(std::cbrt(static_cast<double>(seedCount)))));
    const float cellSize = 1.0f / static_cast<float>(cellsPerAxis);
    const float shardSize = std::max(0.04f, cellSize - gapThickness);
    std::vector<ri::math::Vec3> triangles;
    int seedIndex = 0;
    for (int ix = 0; ix < cellsPerAxis && seedIndex < seedCount; ++ix) {
        for (int iy = 0; iy < cellsPerAxis && seedIndex < seedCount; ++iy) {
            for (int iz = 0; iz < cellsPerAxis && seedIndex < seedCount; ++iz) {
                const ri::math::Vec3 center{
                    -0.5f + ((static_cast<float>(ix) + 0.5f) * cellSize),
                    -0.5f + ((static_cast<float>(iy) + 0.5f) * cellSize),
                    -0.5f + ((static_cast<float>(iz) + 0.5f) * cellSize)};
                const float jx = (SeededUnit01(seedIndex + 1) - 0.5f) * cellSize * jitter;
                const float jy = (SeededUnit01(seedIndex + 11) - 0.5f) * cellSize * jitter;
                const float jz = (SeededUnit01(seedIndex + 21) - 0.5f) * cellSize * jitter;
                const float sx = shardSize * (0.74f + (SeededUnit01(seedIndex + 31) * 0.22f));
                const float sy = shardSize * (0.74f + (SeededUnit01(seedIndex + 41) * 0.22f));
                const float sz = shardSize * (0.74f + (SeededUnit01(seedIndex + 51) * 0.22f));
                const ri::math::Vec3 c2 = center + ri::math::Vec3{jx, jy, jz};
                const ri::math::Vec3 minB{c2.x - sx * 0.5f, c2.y - sy * 0.5f, c2.z - sz * 0.5f};
                const ri::math::Vec3 maxB{c2.x + sx * 0.5f, c2.y + sy * 0.5f, c2.z + sz * 0.5f};
                AppendAxisAlignedBox(triangles, minB, maxB);
                seedIndex += 1;
            }
        }
    }
    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateMetaballClusterMesh(const StructuralPrimitiveOptions& options) {
    const int lat = ClampInteger(options.radialSegments, 16, 6, 64);
    const int lon = lat;
    const float baseR =
        std::clamp(options.length > 1e-5f ? std::fabs(options.length) * 0.76f : 0.38f, 0.08f, 1.5f);
    const float blendR = std::max(
        baseR * 1.6f,
        std::clamp(options.depth > 1e-5f ? std::fabs(options.depth) : baseR * 1.2f, 0.05f, 4.0f));
    const float blendR2 = blendR * blendR;

    struct Influence {
        ri::math::Vec3 center{};
        float weight = 0.0f;
    };
    std::vector<Influence> influences;
    if (!options.points.empty()) {
        for (std::size_t index = 0; index < options.points.size(); ++index) {
            const float w = 0.18f + SeededUnit01(static_cast<int>(index) + 1) * 0.18f;
            influences.push_back({options.points[index], w});
        }
    } else {
        influences.push_back({{-0.24f, 0.05f, -0.12f}, 0.26f});
        influences.push_back({{0.18f, 0.12f, 0.08f}, 0.22f});
        influences.push_back({{0.02f, -0.14f, 0.2f}, 0.18f});
    }

    std::vector<ri::math::Vec3> basePos;
    basePos.reserve(static_cast<std::size_t>((lat + 1) * (lon + 1)));
    for (int iy = 0; iy <= lat; ++iy) {
        const float t = static_cast<float>(iy) / static_cast<float>(lat);
        const float phi = t * kPi;
        const float sinPhi = std::sin(phi);
        const float cosPhi = std::cos(phi);
        for (int ix = 0; ix <= lon; ++ix) {
            const float u = static_cast<float>(ix) / static_cast<float>(lon);
            const float theta = u * 2.0f * kPi;
            const float x = sinPhi * std::cos(theta) * baseR;
            const float y = cosPhi * baseR;
            const float z = sinPhi * std::sin(theta) * baseR;
            basePos.push_back({x, y, z});
        }
    }

    for (ri::math::Vec3& p : basePos) {
        ri::math::Vec3 n = p;
        if (ri::math::LengthSquared(n) > 1e-10f) {
            n = ri::math::Normalize(n);
        } else {
            n = {0.0f, 1.0f, 0.0f};
        }
        float displacement = 0.0f;
        for (const Influence& inf : influences) {
            const float distSq = ri::math::LengthSquared(p - inf.center);
            displacement += std::exp(-(distSq / std::max(1e-4f, blendR2))) * inf.weight;
        }
        p = p + n * displacement;
    }

    std::vector<ri::math::Vec3> triangles;
    for (int iy = 0; iy < lat; ++iy) {
        for (int ix = 0; ix < lon; ++ix) {
            const int row = iy * (lon + 1);
            const int a = row + ix;
            const int b = a + 1;
            const int c = a + (lon + 1) + 1;
            const int d = a + (lon + 1);
            AppendQuadFacing(triangles, basePos[a], basePos[b], basePos[c], basePos[d], {0.0f, 1.0f, 0.0f});
        }
    }
    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateLSystemBranchMesh(const StructuralPrimitiveOptions& options) {
    const int iterations = ClampInteger(options.detail > 0 ? options.detail : 3, 3, 1, 6);
    const float branchLength =
        std::clamp(options.length > 1e-5f ? std::fabs(options.length) : 0.55f, 0.08f, 4.0f);
    const float branchScale =
        std::clamp(options.topRadius > 1e-5f ? std::fabs(options.topRadius) : 0.72f, 0.2f, 0.92f);
    const float branchAngleDeg =
        std::clamp(options.spanDegrees > 1e-3f ? options.spanDegrees : 28.0f, 1.0f, 85.0f);
    const float branchAngleRad = branchAngleDeg * (kPi / 180.0f);
    const float radius =
        std::clamp(options.strutRadius > 1e-5f ? std::fabs(options.strutRadius) : 0.09f, 0.01f, 1.0f);
    const int radialSegments = std::max(5, ClampInteger(options.radialSegments, 6, 3, 18));

    std::vector<ri::math::Vec3> triangles;
    int branchesEmitted = 0;
    constexpr int kBranchBudget = 256;
    BranchRecursive(triangles,
                    {0.0f, -0.5f, 0.0f},
                    {0.0f, 1.0f, 0.0f},
                    1,
                    iterations,
                    branchLength,
                    branchScale,
                    branchAngleRad,
                    radius,
                    radialSegments,
                    branchesEmitted,
                    kBranchBudget);
    if (triangles.empty()) {
        AppendAxisAlignedBox(triangles, {-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f});
    }
    return BuildMeshFromTriangles(triangles);
}

void AppendExtrudedProfile(std::vector<ri::math::Vec3>& triangles,
                           const std::vector<ri::math::Vec3>& profile,
                           float halfHeight) {
    if (profile.size() < 3U) {
        return;
    }

    const auto appendTriangleFacing = [&](const ri::math::Vec3& a,
                                          const ri::math::Vec3& b,
                                          const ri::math::Vec3& c,
                                          const ri::math::Vec3& desiredNormal) {
        const ri::math::Vec3 normal = ri::math::Cross(b - a, c - a);
        if (ri::math::Dot(normal, desiredNormal) < 0.0f) {
            AppendTriangle(triangles, a, c, b);
            return;
        }
        AppendTriangle(triangles, a, b, c);
    };

    for (std::size_t index = 1; index + 1 < profile.size(); ++index) {
        appendTriangleFacing({profile[0].x, halfHeight, profile[0].z},
                             {profile[index].x, halfHeight, profile[index].z},
                             {profile[index + 1].x, halfHeight, profile[index + 1].z},
                             {0.0f, 1.0f, 0.0f});
        appendTriangleFacing({profile[0].x, -halfHeight, profile[0].z},
                             {profile[index + 1].x, -halfHeight, profile[index + 1].z},
                             {profile[index].x, -halfHeight, profile[index].z},
                             {0.0f, -1.0f, 0.0f});
    }

    for (std::size_t index = 0; index < profile.size(); ++index) {
        const std::size_t next = (index + 1U) % profile.size();
        const ri::math::Vec3 a{profile[index].x, -halfHeight, profile[index].z};
        const ri::math::Vec3 b{profile[next].x, -halfHeight, profile[next].z};
        const ri::math::Vec3 c{profile[next].x, halfHeight, profile[next].z};
        const ri::math::Vec3 d{profile[index].x, halfHeight, profile[index].z};
        const ri::math::Vec3 outward = ri::math::Normalize(ri::math::Cross(c - b, a - b));
        AppendQuadFacing(triangles, a, b, c, d, outward);
    }
}

CompiledMesh CreateTubeMesh(const StructuralPrimitiveOptions& options) {
    const int segments = ClampRange(options.radialSegments, 6, 128);
    const float outerRadius = 0.5f;
    const float innerRadius = std::clamp(options.topRadius > 0.0f ? options.topRadius : outerRadius - options.thickness,
                                        0.04f,
                                        0.46f);
    std::vector<ri::math::Vec3> triangles;
    triangles.reserve(static_cast<std::size_t>(segments) * 24U);
    for (int index = 0; index < segments; ++index) {
        const float a0 = (2.0f * kPi * static_cast<float>(index)) / static_cast<float>(segments);
        const float a1 = (2.0f * kPi * static_cast<float>(index + 1)) / static_cast<float>(segments);
        const ri::math::Vec3 o0{std::cos(a0) * outerRadius, -0.5f, std::sin(a0) * outerRadius};
        const ri::math::Vec3 o1{std::cos(a1) * outerRadius, -0.5f, std::sin(a1) * outerRadius};
        const ri::math::Vec3 o2{std::cos(a1) * outerRadius, 0.5f, std::sin(a1) * outerRadius};
        const ri::math::Vec3 o3{std::cos(a0) * outerRadius, 0.5f, std::sin(a0) * outerRadius};
        const ri::math::Vec3 i0{std::cos(a0) * innerRadius, -0.5f, std::sin(a0) * innerRadius};
        const ri::math::Vec3 i1{std::cos(a1) * innerRadius, -0.5f, std::sin(a1) * innerRadius};
        const ri::math::Vec3 i2{std::cos(a1) * innerRadius, 0.5f, std::sin(a1) * innerRadius};
        const ri::math::Vec3 i3{std::cos(a0) * innerRadius, 0.5f, std::sin(a0) * innerRadius};
        AppendQuadFacing(triangles, o0, o1, o2, o3, {o0.x, 0.0f, o0.z});
        AppendQuadFacing(triangles, i1, i0, i3, i2, {-i0.x, 0.0f, -i0.z});
        AppendQuadFacing(triangles, o3, o2, i2, i3, {0.0f, 1.0f, 0.0f});
        AppendQuadFacing(triangles, o1, o0, i0, i1, {0.0f, -1.0f, 0.0f});
    }
    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateTorusMesh(const StructuralPrimitiveOptions& options) {
    const int ringSegments = ClampRange(options.radialSegments, 8, 192);
    const int tubeSegments = ClampRange(options.sides, 6, 64);
    const float sweep = std::clamp(std::fabs(options.sweepDegrees), 30.0f, 360.0f) * (kPi / 180.0f);
    const float start = options.startDegrees * (kPi / 180.0f);
    const bool closed = std::fabs(sweep - (2.0f * kPi)) < 1e-3f;
    const int ringLimit = closed ? ringSegments : ringSegments + 1;
    const float tubeRadius = std::clamp(options.thickness > 0.0f ? options.thickness : 0.14f, 0.03f, 0.22f);
    const float majorRadius = std::max(0.08f, 0.5f - tubeRadius);
    auto pointAt = [&](int ring, int tube) {
        const float u = start + (static_cast<float>(ring) / static_cast<float>(ringSegments)) * sweep;
        const float v = (2.0f * kPi * static_cast<float>(tube)) / static_cast<float>(tubeSegments);
        const float radius = majorRadius + (std::cos(v) * tubeRadius);
        return ri::math::Vec3{std::cos(u) * radius, std::sin(v) * tubeRadius, std::sin(u) * radius};
    };

    std::vector<ri::math::Vec3> triangles;
    triangles.reserve(static_cast<std::size_t>(ringSegments * tubeSegments) * 6U);
    for (int ring = 0; ring < ringSegments; ++ring) {
        const int nextRing = closed ? (ring + 1) % ringSegments : ring + 1;
        for (int tube = 0; tube < tubeSegments; ++tube) {
            const int nextTube = (tube + 1) % tubeSegments;
            AppendQuadFacing(triangles,
                             pointAt(ring, tube),
                             pointAt(nextRing, tube),
                             pointAt(nextRing, nextTube),
                             pointAt(ring, nextTube),
                             pointAt(ring, tube));
        }
    }
    if (!closed) {
        for (int endRing : {0, ringLimit - 1}) {
            const ri::math::Vec3 center = pointAt(endRing, 0) - ri::math::Normalize(pointAt(endRing, 0)) * tubeRadius;
            for (int tube = 0; tube < tubeSegments; ++tube) {
                const int nextTube = (tube + 1) % tubeSegments;
                AppendTriangle(triangles, center, pointAt(endRing, tube), pointAt(endRing, nextTube));
            }
        }
    }
    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateCornerMesh(const StructuralPrimitiveOptions& options) {
    const bool rounded = options.archStyle == "rounded" || options.latticeStyle == "rounded";
    const float inner = std::clamp(0.5f - options.thickness, 0.08f, 0.44f);
    std::vector<ri::math::Vec3> profile = {{-0.5f, 0.0f, -0.5f}, {0.5f, 0.0f, -0.5f}, {0.5f, 0.0f, -inner}};
    if (rounded) {
        const int segments = ClampRange(options.radialSegments, 3, 32);
        for (int index = 0; index <= segments; ++index) {
            const float t = static_cast<float>(index) / static_cast<float>(segments);
            const float angle = (-90.0f + (90.0f * t)) * (kPi / 180.0f);
            profile.push_back({std::cos(angle) * inner, 0.0f, std::sin(angle) * inner});
        }
    } else {
        profile.push_back({inner, 0.0f, inner});
    }
    profile.push_back({-0.5f, 0.0f, 0.5f});

    std::vector<ri::math::Vec3> triangles;
    AppendExtrudedProfile(triangles, profile, 0.5f);
    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateStairsMesh(const StructuralPrimitiveOptions& options) {
    const int steps = ClampRange(options.steps > 0 ? options.steps : options.detail, 2, 64);
    std::vector<ri::math::Vec3> triangles;
    triangles.reserve(static_cast<std::size_t>(steps) * 36U);
    for (int step = 0; step < steps; ++step) {
        const float z0 = -0.5f + (static_cast<float>(step) / static_cast<float>(steps));
        const float z1 = -0.5f + (static_cast<float>(step + 1) / static_cast<float>(steps));
        const float y0 = -0.5f;
        const float y1 = -0.5f + (static_cast<float>(step + 1) / static_cast<float>(steps));
        const ri::math::Vec3 a{-0.5f, y0, z0};
        const ri::math::Vec3 b{0.5f, y0, z0};
        const ri::math::Vec3 c{0.5f, y0, z1};
        const ri::math::Vec3 d{-0.5f, y0, z1};
        const ri::math::Vec3 e{-0.5f, y1, z0};
        const ri::math::Vec3 f{0.5f, y1, z0};
        const ri::math::Vec3 g{0.5f, y1, z1};
        const ri::math::Vec3 h{-0.5f, y1, z1};
        AppendQuadFacing(triangles, e, f, g, h, {0.0f, 1.0f, 0.0f});
        AppendQuadFacing(triangles, a, d, c, b, {0.0f, -1.0f, 0.0f});
        AppendQuadFacing(triangles, d, h, g, c, {0.0f, 0.0f, 1.0f});
        AppendQuadFacing(triangles, a, b, f, e, {0.0f, 0.0f, -1.0f});
        AppendQuadFacing(triangles, a, e, h, d, {-1.0f, 0.0f, 0.0f});
        AppendQuadFacing(triangles, b, c, g, f, {1.0f, 0.0f, 0.0f});
    }
    return BuildMeshFromTriangles(triangles);
}

CompiledMesh CreateSpiralStairsMesh(const StructuralPrimitiveOptions& options) {
    const int steps = ClampRange(options.steps > 0 ? options.steps : options.detail, 3, 96);
    const float sweep = std::clamp(std::fabs(options.sweepDegrees), 60.0f, 720.0f) * (kPi / 180.0f);
    const float start = options.startDegrees * (kPi / 180.0f);
    const float innerRadius = std::clamp(options.topRadius > 0.0f ? options.topRadius : 0.16f, 0.02f, 0.42f);
    const float outerRadius = std::clamp(options.bottomRadius > 0.0f ? options.bottomRadius : 0.5f, innerRadius + 0.04f, 0.5f);
    const float stepThickness = std::clamp(options.thickness * 0.45f, 0.025f, 0.12f);
    auto p = [&](float angle, float radius, float y) {
        return ri::math::Vec3{std::cos(angle) * radius, y, std::sin(angle) * radius};
    };

    std::vector<ri::math::Vec3> triangles;
    triangles.reserve(static_cast<std::size_t>(steps) * 36U);
    for (int step = 0; step < steps; ++step) {
        const float a0 = start + (static_cast<float>(step) / static_cast<float>(steps)) * sweep;
        const float a1 = start + (static_cast<float>(step + 1) / static_cast<float>(steps)) * sweep;
        const float yTop = -0.5f + (static_cast<float>(step + 1) / static_cast<float>(steps));
        const float yBottom = std::max(-0.5f, yTop - stepThickness);
        const ri::math::Vec3 io0 = p(a0, innerRadius, yTop);
        const ri::math::Vec3 oo0 = p(a0, outerRadius, yTop);
        const ri::math::Vec3 oo1 = p(a1, outerRadius, yTop);
        const ri::math::Vec3 io1 = p(a1, innerRadius, yTop);
        const ri::math::Vec3 bi0 = p(a0, innerRadius, yBottom);
        const ri::math::Vec3 bo0 = p(a0, outerRadius, yBottom);
        const ri::math::Vec3 bo1 = p(a1, outerRadius, yBottom);
        const ri::math::Vec3 bi1 = p(a1, innerRadius, yBottom);
        AppendQuadFacing(triangles, io0, oo0, oo1, io1, {0.0f, 1.0f, 0.0f});
        AppendQuadFacing(triangles, bi0, bi1, bo1, bo0, {0.0f, -1.0f, 0.0f});
        AppendQuadFacing(triangles, oo0, bo0, bo1, oo1, oo0);
        AppendQuadFacing(triangles, bi0, io0, io1, bi1, {-io0.x, 0.0f, -io0.z});
        AppendQuadFacing(triangles, io0, bi0, bo0, oo0, p(a0, 1.0f, 0.0f));
        AppendQuadFacing(triangles, bi1, io1, oo1, bo1, p(a1, 1.0f, 0.0f));
    }

    if (options.centerColumn) {
        const int columnSegments = ClampRange(options.radialSegments, 8, 48);
        const float radius = std::clamp(innerRadius * 0.4f, 0.02f, 0.08f);
        for (int index = 0; index < columnSegments; ++index) {
            const float a0 = (2.0f * kPi * static_cast<float>(index)) / static_cast<float>(columnSegments);
            const float a1 = (2.0f * kPi * static_cast<float>(index + 1)) / static_cast<float>(columnSegments);
            const ri::math::Vec3 b0{std::cos(a0) * radius, -0.5f, std::sin(a0) * radius};
            const ri::math::Vec3 b1{std::cos(a1) * radius, -0.5f, std::sin(a1) * radius};
            const ri::math::Vec3 t0{b0.x, 0.5f, b0.z};
            const ri::math::Vec3 t1{b1.x, 0.5f, b1.z};
            AppendQuadFacing(triangles, b0, b1, t1, t0, {b0.x, 0.0f, b0.z});
            AppendTriangle(triangles, {0.0f, 0.5f, 0.0f}, t0, t1);
            AppendTriangle(triangles, {0.0f, -0.5f, 0.0f}, b1, b0);
        }
    }
    return BuildMeshFromTriangles(triangles);
}

/// Filleted “rounded box” visual: superellipsoid with exponents derived from `bevelRadius` (meters, unit cube).
/// This is a **high-quality mesh approximation** to Three.js `RoundedBoxGeometry` without CSG: smooth silhouettes
/// and stable bounds near ±0.5, suitable for author-facing structural previews and collision convex fallback (`box`).
CompiledMesh CreateRoundedBoxMesh(const StructuralPrimitiveOptions& options) {
    const float r =
        std::clamp(options.bevelRadius > 1e-6f ? options.bevelRadius : 0.08f, 0.01f, 0.49f);
    StructuralPrimitiveOptions se = options;
    const float t = r / 0.5f;
    const float exp = std::clamp(2.0f + 5.5f * t, 2.08f, 7.0f);
    se.exponentX = exp;
    se.exponentY = exp;
    se.exponentZ = exp;
    const int segHint = options.bevelSegments > 0 ? options.bevelSegments : 4;
    se.radialSegments = ClampInteger(segHint * 4, 16, 6, 96);
    se.sides = ClampInteger(segHint * 5, 24, 8, 128);
    return CreateSuperellipsoidMesh(se);
}

} // namespace

bool IsNativeStructuralPrimitive(std::string_view type) {
    return type == "box"
        || type == "plane"
        || type == "arch"
        || type == "hollow_box"
        || type == "ramp"
        || type == "wedge"
        || type == "cylinder"
        || type == "cone"
        || type == "pyramid"
        || type == "stairs"
        || type == "spiral_stairs"
        || type == "tube"
        || type == "torus"
        || type == "corner"
        || type == "capsule"
        || type == "frustum"
        || type == "geodesic_sphere"
        || type == "superellipsoid"
        || type == "extrude_along_normal_primitive"
        || type == "lattice_volume"
        || type == "half_pipe"
        || type == "quarter_pipe"
        || type == "pipe_elbow"
        || type == "torus_slice"
        || type == "spline_sweep"
        || type == "revolve"
        || type == "dome_vault"
        || type == "loft_primitive"
        || type == "spline_ribbon"
        || type == "catenary_primitive"
        || type == "cable_primitive"
        || type == "thick_polygon_primitive"
        || type == "trim_sheet_sweep"
        || type == "water_surface_primitive"
        || type == "terrain_quad"
        || type == "heightmap_patch"
        || type == "hexahedron"
        || type == "convex_hull"
        || type == "roof_gable"
        || type == "hipped_roof"
        || type == "rounded_box"
        || type == "displacement"
        || type == "displacement_map"
        || type == "manifold_sweep"
        || type == "spline_extrusion"
        || type == "voronoi_fracture"
        || type == "cellular_shatter"
        || type == "metaball"
        || type == "metaball_cluster"
        || type == "lsystem_branch"
        || type == "lsystem";
}

std::optional<ConvexSolid> CreateConvexPrimitiveSolid(std::string_view type,
                                                      const StructuralPrimitiveOptions& options) {
    if (type == "box") {
        return CreateAxisAlignedBoxSolid();
    }
    if (type == "ramp") {
        return CreateRampSolid();
    }
    if (type == "wedge") {
        return CreateWedgeSolid();
    }
    if (type == "cylinder") {
        return CreateCylinderLikeSolid(options.radialSegments);
    }
    if (type == "frustum") {
        return CreateFrustumSolid(options.radialSegments, options.topRadius, options.bottomRadius);
    }
    if (type == "cone") {
        return CreateConeLikeSolid(options.sides);
    }
    if (type == "pyramid") {
        StructuralPrimitiveOptions pyramidOptions = options;
        pyramidOptions.sides = 4;
        return CreateConeLikeSolid(pyramidOptions.sides);
    }
    if (type == "geodesic_sphere") {
        return CreateIcosahedronSolid();
    }
    if (type == "hexahedron") {
        const std::vector<ri::math::Vec3> defaultVertices = {
            {-0.5f, -0.5f, -0.5f},
            {0.5f, -0.5f, -0.5f},
            {0.5f, -0.5f, 0.5f},
            {-0.5f, -0.5f, 0.5f},
            {-0.5f, 0.5f, -0.5f},
            {0.5f, 0.5f, -0.5f},
            {0.5f, 0.5f, 0.5f},
            {-0.5f, 0.5f, 0.5f},
        };
        return CreateHexahedronSolid(options.vertices.empty() ? defaultVertices : options.vertices);
    }
    if (type == "convex_hull") {
        const std::vector<ri::math::Vec3> defaultPoints = {
            {-0.5f, -0.5f, -0.5f},
            {0.5f, -0.5f, -0.5f},
            {0.5f, -0.5f, 0.5f},
            {-0.5f, -0.5f, 0.5f},
            {0.0f, 0.5f, 0.0f},
        };
        return CreateConvexHullSolidFromPoints(options.points.empty() ? defaultPoints : options.points);
    }
    if (type == "roof_gable") {
        return CreateGableRoofSolid();
    }
    if (type == "hipped_roof") {
        return CreateHippedRoofSolid(options.ridgeRatio);
    }
    return std::nullopt;
}

CompiledMesh BuildPrimitiveMesh(std::string_view type, const StructuralPrimitiveOptions& options) {
    if (type == "rounded_box") {
        return CreateRoundedBoxMesh(options);
    }
    if (type == "displacement" || type == "displacement_map") {
        return CreateHeightfieldMesh(options, false);
    }
    if (type == "manifold_sweep" || type == "spline_extrusion") {
        return CreateSplineSweepMesh(options);
    }
    if (type == "voronoi_fracture" || type == "cellular_shatter") {
        return CreateVoronoiFractureMesh(options);
    }
    if (type == "metaball" || type == "metaball_cluster") {
        return CreateMetaballClusterMesh(options);
    }
    if (type == "lsystem_branch" || type == "lsystem") {
        return CreateLSystemBranchMesh(options);
    }
    if (type == "plane") {
        return CreatePlaneMesh();
    }
    if (type == "arch") {
        return CreateArchMesh(options);
    }
    if (type == "hollow_box") {
        return CreateHollowBoxMesh(options.thickness);
    }
    if (type == "capsule") {
        return CreateCapsuleMesh(options);
    }
    if (type == "frustum") {
        return CreateFrustumMesh(options);
    }
    if (type == "geodesic_sphere") {
        return CreateGeodesicSphereMesh(options);
    }
    if (type == "superellipsoid") {
        return CreateSuperellipsoidMesh(options);
    }
    if (type == "extrude_along_normal_primitive") {
        return CreateExtrudeAlongNormalMesh(options);
    }
    if (type == "lattice_volume") {
        return CreateLatticeVolumeMesh(options);
    }
    if (type == "stairs") {
        return CreateStairsMesh(options);
    }
    if (type == "spiral_stairs") {
        return CreateSpiralStairsMesh(options);
    }
    if (type == "tube") {
        return CreateTubeMesh(options);
    }
    if (type == "torus") {
        return CreateTorusMesh(options);
    }
    if (type == "corner") {
        return CreateCornerMesh(options);
    }
    if (type == "half_pipe") {
        return CreateArcChannelMesh(180.0f, options);
    }
    if (type == "quarter_pipe") {
        return CreateArcChannelMesh(90.0f, options);
    }
    if (type == "pipe_elbow") {
        return CreateTorusArcMesh(options, 90.0f);
    }
    if (type == "torus_slice") {
        return CreateTorusArcMesh(options, 135.0f);
    }
    if (type == "spline_sweep") {
        return CreateSplineSweepMesh(options);
    }
    if (type == "revolve") {
        return CreateRevolveMesh(options);
    }
    if (type == "dome_vault") {
        return CreateDomeVaultMesh(options);
    }
    if (type == "loft_primitive") {
        return CreateLoftMesh(options);
    }
    if (type == "spline_ribbon") {
        return CreateRibbonMesh(options);
    }
    if (type == "catenary_primitive") {
        return CreateCableLikeMesh(options, true);
    }
    if (type == "cable_primitive") {
        return CreateCableLikeMesh(options, false);
    }
    if (type == "thick_polygon_primitive") {
        return CreateThickPolygonMesh(options);
    }
    if (type == "trim_sheet_sweep") {
        return CreateTrimSheetSweepMesh(options);
    }
    if (type == "water_surface_primitive") {
        return CreateWaterSurfaceMesh(options);
    }
    if (type == "terrain_quad") {
        return CreateHeightfieldMesh(options, true);
    }
    if (type == "heightmap_patch") {
        return CreateHeightfieldMesh(options, false);
    }

    const std::optional<ConvexSolid> solid = CreateConvexPrimitiveSolid(type, options);
    if (solid.has_value()) {
        return BuildCompiledMeshFromConvexSolid(*solid);
    }

    return BuildCompiledMeshFromConvexSolid(CreateAxisAlignedBoxSolid());
}

CompiledMesh BuildHippedRoofCompiledMesh(float ridgeRatio) {
    const std::optional<ConvexSolid> solid = CreateHippedRoofSolid(ridgeRatio);
    return BuildCompiledMeshFromConvexSolid(solid.value_or(CreateAxisAlignedBoxSolid()));
}

CompiledMesh BuildConvexHullCompiledMeshFromPoints(const std::vector<ri::math::Vec3>& points) {
    const std::optional<ConvexSolid> solid = CreateConvexHullSolidFromPoints(points);
    return BuildCompiledMeshFromConvexSolid(solid.value_or(CreateAxisAlignedBoxSolid()));
}

} // namespace ri::structural
