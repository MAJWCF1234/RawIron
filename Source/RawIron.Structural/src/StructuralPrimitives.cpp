#include "RawIron/Structural/StructuralPrimitives.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
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

CompiledMesh CreateGeodesicSphereMesh() {
    const std::optional<ConvexSolid> solid = CreateIcosahedronSolid();
    return BuildCompiledMeshFromConvexSolid(solid.value_or(CreateAxisAlignedBoxSolid()));
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
        || type == "capsule"
        || type == "frustum"
        || type == "geodesic_sphere"
        || type == "hexahedron"
        || type == "convex_hull"
        || type == "roof_gable"
        || type == "hipped_roof";
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
        return CreateGeodesicSphereMesh();
    }

    const std::optional<ConvexSolid> solid = CreateConvexPrimitiveSolid(type, options);
    if (solid.has_value()) {
        return BuildCompiledMeshFromConvexSolid(*solid);
    }

    return BuildCompiledMeshFromConvexSolid(CreateAxisAlignedBoxSolid());
}

} // namespace ri::structural
