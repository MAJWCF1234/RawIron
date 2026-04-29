#pragma once

#include "RawIron/Math/Vec3.h"

#include <optional>
#include <string_view>
#include <vector>

namespace ri::structural {

enum class PlaneSide {
    Front,
    Back,
    Coplanar,
};

struct Plane {
    ri::math::Vec3 normal{};
    float constant = 0.0f;
};

struct ConvexPolygon {
    Plane plane{};
    std::vector<ri::math::Vec3> vertices;
};

struct ConvexSolid {
    std::vector<ConvexPolygon> polygons;
};

struct CompiledMesh {
    std::vector<ri::math::Vec3> positions;
    std::vector<ri::math::Vec3> normals;
    std::size_t triangleCount = 0;
    bool hasBounds = false;
    ri::math::Vec3 boundsMin{};
    ri::math::Vec3 boundsMax{};
};

struct ConvexPolygonClipResult {
    std::optional<ConvexPolygon> front;
    std::optional<ConvexPolygon> back;
    std::vector<ri::math::Vec3> intersections;
};

struct ConvexSolidClipResult {
    std::optional<ConvexSolid> front;
    std::optional<ConvexSolid> back;
    bool split = false;
    std::vector<ri::math::Vec3> capPoints;
};

[[nodiscard]] std::string_view ToString(PlaneSide side);
[[nodiscard]] float DistanceToPlane(const Plane& plane, const ri::math::Vec3& point);
[[nodiscard]] Plane NegatePlane(const Plane& plane);
[[nodiscard]] std::optional<Plane> ComputePolygonPlane(const std::vector<ri::math::Vec3>& vertices);
[[nodiscard]] PlaneSide ClassifyPointToPlane(const ri::math::Vec3& point, const Plane& plane, float epsilon = 1e-5f);
[[nodiscard]] Plane CreatePlaneFromPointNormal(const ri::math::Vec3& point, const ri::math::Vec3& normal);
[[nodiscard]] ConvexSolid CreateAxisAlignedBoxSolid(const ri::math::Vec3& min = {-0.5f, -0.5f, -0.5f},
                                                    const ri::math::Vec3& max = {0.5f, 0.5f, 0.5f});
[[nodiscard]] std::vector<ri::math::Vec3> SortCoplanarPoints(const std::vector<ri::math::Vec3>& points,
                                                             const Plane& plane,
                                                             float epsilon = 1e-5f);
[[nodiscard]] ConvexPolygonClipResult ClipConvexPolygonByPlane(const ConvexPolygon& polygon,
                                                               const Plane& plane,
                                                               float epsilon = 1e-5f);
[[nodiscard]] ConvexSolidClipResult ClipConvexSolidByPlane(const ConvexSolid& solid,
                                                           const Plane& splitPlane,
                                                           float epsilon = 1e-5f);
[[nodiscard]] CompiledMesh BuildCompiledMeshFromConvexSolid(const ConvexSolid& solid);
/// Stabilizes mesh-derived plane soups before CSG (duplicate / opposite-facing planes within epsilon).
[[nodiscard]] std::vector<Plane> DedupeConvexPlanes(const std::vector<Plane>& planes, float epsilon = 1e-4f);

} // namespace ri::structural
