#include "RawIron/Structural/ConvexClipper.h"

#include <algorithm>
#include <cmath>

namespace ri::structural {
namespace {

std::vector<ri::math::Vec3> DedupeSequentialVertices(const std::vector<ri::math::Vec3>& vertices, float epsilon) {
    std::vector<ri::math::Vec3> result;
    const float epsilonSquared = epsilon * epsilon;
    for (const ri::math::Vec3& vertex : vertices) {
        if (!result.empty() && ri::math::DistanceSquared(result.back(), vertex) <= epsilonSquared) {
            continue;
        }
        result.push_back(vertex);
    }
    if (result.size() >= 2 && ri::math::DistanceSquared(result.front(), result.back()) <= epsilonSquared) {
        result.pop_back();
    }
    return result;
}

std::vector<ri::math::Vec3> DedupeVertices(const std::vector<ri::math::Vec3>& vertices, float epsilon) {
    std::vector<ri::math::Vec3> unique;
    const float epsilonSquared = epsilon * epsilon;
    for (const ri::math::Vec3& vertex : vertices) {
        const bool duplicate = std::any_of(unique.begin(), unique.end(), [&](const ri::math::Vec3& candidate) {
            return ri::math::DistanceSquared(candidate, vertex) <= epsilonSquared;
        });
        if (!duplicate) {
            unique.push_back(vertex);
        }
    }
    return unique;
}

struct PlaneBasis {
    ri::math::Vec3 tangent{};
    ri::math::Vec3 bitangent{};
};

PlaneBasis MakePlaneBasis(const ri::math::Vec3& normal) {
    const ri::math::Vec3 reference = std::fabs(normal.y) < 0.99f ? ri::math::Vec3{0.0f, 1.0f, 0.0f}
                                                                 : ri::math::Vec3{1.0f, 0.0f, 0.0f};
    PlaneBasis basis{};
    basis.tangent = ri::math::Normalize(ri::math::Cross(reference, normal));
    basis.bitangent = ri::math::Normalize(ri::math::Cross(normal, basis.tangent));
    return basis;
}

} // namespace

std::string_view ToString(PlaneSide side) {
    switch (side) {
    case PlaneSide::Front:
        return "front";
    case PlaneSide::Back:
        return "back";
    case PlaneSide::Coplanar:
        return "coplanar";
    }
    return "coplanar";
}

float DistanceToPlane(const Plane& plane, const ri::math::Vec3& point) {
    return ri::math::Dot(plane.normal, point) + plane.constant;
}

Plane NegatePlane(const Plane& plane) {
    return Plane{
        .normal = plane.normal * -1.0f,
        .constant = plane.constant * -1.0f,
    };
}

std::optional<Plane> ComputePolygonPlane(const std::vector<ri::math::Vec3>& vertices) {
    if (vertices.size() < 3) {
        return std::nullopt;
    }

    for (std::size_t index = 0; index + 2 < vertices.size(); ++index) {
        const ri::math::Vec3 edgeA = vertices[index + 1] - vertices[index];
        const ri::math::Vec3 edgeB = vertices[index + 2] - vertices[index];
        const ri::math::Vec3 normal = ri::math::Cross(edgeA, edgeB);
        if (ri::math::LengthSquared(normal) <= 1e-12f) {
            continue;
        }
        return CreatePlaneFromPointNormal(vertices[index], normal);
    }

    return std::nullopt;
}

PlaneSide ClassifyPointToPlane(const ri::math::Vec3& point, const Plane& plane, float epsilon) {
    const float distance = DistanceToPlane(plane, point);
    if (distance > epsilon) {
        return PlaneSide::Front;
    }
    if (distance < -epsilon) {
        return PlaneSide::Back;
    }
    return PlaneSide::Coplanar;
}

Plane CreatePlaneFromPointNormal(const ri::math::Vec3& point, const ri::math::Vec3& normal) {
    const ri::math::Vec3 safeNormal = ri::math::Normalize(normal);
    return Plane{
        .normal = safeNormal,
        .constant = -ri::math::Dot(safeNormal, point),
    };
}

ConvexSolid CreateAxisAlignedBoxSolid(const ri::math::Vec3& min, const ri::math::Vec3& max) {
    const ri::math::Vec3 lbf{min.x, min.y, max.z};
    const ri::math::Vec3 rbf{max.x, min.y, max.z};
    const ri::math::Vec3 rbb{max.x, min.y, min.z};
    const ri::math::Vec3 lbb{min.x, min.y, min.z};
    const ri::math::Vec3 ltf{min.x, max.y, max.z};
    const ri::math::Vec3 rtf{max.x, max.y, max.z};
    const ri::math::Vec3 rtb{max.x, max.y, min.z};
    const ri::math::Vec3 ltb{min.x, max.y, min.z};

    ConvexSolid solid{};
    const std::vector<std::vector<ri::math::Vec3>> faces = {
        {lbf, rbf, rtf, ltf},
        {rbb, lbb, ltb, rtb},
        {lbb, lbf, ltf, ltb},
        {rbf, rbb, rtb, rtf},
        {ltf, rtf, rtb, ltb},
        {lbb, rbb, rbf, lbf},
    };

    for (const auto& face : faces) {
        const std::optional<Plane> plane = ComputePolygonPlane(face);
        if (!plane.has_value()) {
            continue;
        }
        solid.polygons.push_back(ConvexPolygon{*plane, face});
    }

    return solid;
}

std::vector<ri::math::Vec3> SortCoplanarPoints(const std::vector<ri::math::Vec3>& points, const Plane& plane, float epsilon) {
    std::vector<ri::math::Vec3> unique = DedupeVertices(points, epsilon);
    if (unique.size() <= 2) {
        return unique;
    }

    ri::math::Vec3 centroid{};
    for (const ri::math::Vec3& point : unique) {
        centroid = centroid + point;
    }
    centroid = centroid / static_cast<float>(unique.size());

    const PlaneBasis basis = MakePlaneBasis(plane.normal);
    std::sort(unique.begin(), unique.end(), [&](const ri::math::Vec3& lhs, const ri::math::Vec3& rhs) {
        const ri::math::Vec3 lhsVector = lhs - centroid;
        const ri::math::Vec3 rhsVector = rhs - centroid;
        const float lhsAngle = std::atan2(ri::math::Dot(lhsVector, basis.bitangent), ri::math::Dot(lhsVector, basis.tangent));
        const float rhsAngle = std::atan2(ri::math::Dot(rhsVector, basis.bitangent), ri::math::Dot(rhsVector, basis.tangent));
        return lhsAngle < rhsAngle;
    });
    return unique;
}

ConvexPolygonClipResult ClipConvexPolygonByPlane(const ConvexPolygon& polygon, const Plane& plane, float epsilon) {
    ConvexPolygonClipResult result{};
    if (polygon.vertices.size() < 3) {
        return result;
    }

    std::vector<ri::math::Vec3> frontVertices;
    std::vector<ri::math::Vec3> backVertices;

    for (std::size_t index = 0; index < polygon.vertices.size(); ++index) {
        const ri::math::Vec3& current = polygon.vertices[index];
        const ri::math::Vec3& next = polygon.vertices[(index + 1) % polygon.vertices.size()];
        const float currentDistance = DistanceToPlane(plane, current);
        const float nextDistance = DistanceToPlane(plane, next);
        const bool currentFront = currentDistance >= -epsilon;
        const bool currentBack = currentDistance <= epsilon;

        if (currentFront) {
            frontVertices.push_back(current);
        }
        if (currentBack) {
            backVertices.push_back(current);
        }

        const bool crosses = (currentDistance > epsilon && nextDistance < -epsilon)
            || (currentDistance < -epsilon && nextDistance > epsilon);
        if (!crosses) {
            continue;
        }

        const float t = currentDistance / (currentDistance - nextDistance);
        const ri::math::Vec3 intersection = ri::math::Lerp(current, next, t);
        frontVertices.push_back(intersection);
        backVertices.push_back(intersection);
        result.intersections.push_back(intersection);
    }

    frontVertices = DedupeSequentialVertices(frontVertices, epsilon);
    backVertices = DedupeSequentialVertices(backVertices, epsilon);
    result.intersections = DedupeVertices(result.intersections, epsilon);

    if (frontVertices.size() >= 3) {
        result.front = ConvexPolygon{polygon.plane, frontVertices};
    }
    if (backVertices.size() >= 3) {
        result.back = ConvexPolygon{polygon.plane, backVertices};
    }

    return result;
}

ConvexSolidClipResult ClipConvexSolidByPlane(const ConvexSolid& solid, const Plane& splitPlane, float epsilon) {
    ConvexSolidClipResult result{};
    ConvexSolid frontSolid{};
    ConvexSolid backSolid{};
    std::vector<ri::math::Vec3> cutPoints;

    for (const ConvexPolygon& polygon : solid.polygons) {
        const ConvexPolygonClipResult clipped = ClipConvexPolygonByPlane(polygon, splitPlane, epsilon);
        if (clipped.front.has_value()) {
            frontSolid.polygons.push_back(*clipped.front);
        }
        if (clipped.back.has_value()) {
            backSolid.polygons.push_back(*clipped.back);
        }
        cutPoints.insert(cutPoints.end(), clipped.intersections.begin(), clipped.intersections.end());
    }

    std::vector<ri::math::Vec3> uniqueCutPoints = DedupeVertices(cutPoints, epsilon);
    if (uniqueCutPoints.size() >= 3) {
        const std::vector<ri::math::Vec3> frontCap = SortCoplanarPoints(uniqueCutPoints, splitPlane, epsilon);
        std::vector<ri::math::Vec3> backCap(frontCap.rbegin(), frontCap.rend());
        frontSolid.polygons.push_back(ConvexPolygon{splitPlane, frontCap});
        backSolid.polygons.push_back(ConvexPolygon{NegatePlane(splitPlane), backCap});
    }

    if (!frontSolid.polygons.empty()) {
        result.front = frontSolid;
    }
    if (!backSolid.polygons.empty()) {
        result.back = backSolid;
    }
    result.split = uniqueCutPoints.size() >= 2;
    result.capPoints = std::move(uniqueCutPoints);
    return result;
}

namespace {

bool SameOrFlippedPlane(const Plane& lhs, const Plane& rhs, float epsilon) {
    const float epsilonSquared = epsilon * epsilon;
    const bool sameNormal = ri::math::DistanceSquared(lhs.normal, rhs.normal) <= epsilonSquared;
    const bool flippedNormal = ri::math::DistanceSquared(lhs.normal, rhs.normal * -1.0f) <= epsilonSquared;
    const bool sameConstant = std::fabs(lhs.constant - rhs.constant) <= epsilon;
    const bool flippedConstant = std::fabs(lhs.constant + rhs.constant) <= epsilon;
    return (sameNormal && sameConstant) || (flippedNormal && flippedConstant);
}

} // namespace

std::vector<Plane> DedupeConvexPlanes(const std::vector<Plane>& planes, const float epsilon) {
    std::vector<Plane> out;
    out.reserve(planes.size());
    for (const Plane& plane : planes) {
        const bool duplicate =
            std::any_of(out.begin(), out.end(), [&](const Plane& kept) { return SameOrFlippedPlane(kept, plane, epsilon); });
        if (!duplicate) {
            out.push_back(plane);
        }
    }
    return out;
}

CompiledMesh BuildCompiledMeshFromConvexSolid(const ConvexSolid& solid) {
    CompiledMesh mesh{};

    for (const ConvexPolygon& polygon : solid.polygons) {
        if (polygon.vertices.size() < 3) {
            continue;
        }

        Plane plane = polygon.plane;
        if (ri::math::LengthSquared(plane.normal) <= 1e-12f) {
            const std::optional<Plane> computed = ComputePolygonPlane(polygon.vertices);
            if (!computed.has_value()) {
                continue;
            }
            plane = *computed;
        }
        const ri::math::Vec3 normal = ri::math::Normalize(plane.normal);

        for (std::size_t index = 1; index + 1 < polygon.vertices.size(); ++index) {
            const ri::math::Vec3 tri[3] = {polygon.vertices[0], polygon.vertices[index], polygon.vertices[index + 1]};
            for (const ri::math::Vec3& vertex : tri) {
                mesh.positions.push_back(vertex);
                mesh.normals.push_back(normal);
                if (!mesh.hasBounds) {
                    mesh.boundsMin = vertex;
                    mesh.boundsMax = vertex;
                    mesh.hasBounds = true;
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
    }

    return mesh;
}

} // namespace ri::structural
