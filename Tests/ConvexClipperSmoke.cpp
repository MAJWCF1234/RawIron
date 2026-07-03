#include "RawIron/Structural/ConvexClipper.h"
#include "RawIron/Structural/StructuralCompiler.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define RI_REQUIRE(condition)                                                                        \
    do {                                                                                             \
        if (!(condition)) {                                                                          \
            std::fprintf(stderr, "ConvexClipperSmoke failed: %s (line %d)\n", #condition, __LINE__); \
            return EXIT_FAILURE;                                                                     \
        }                                                                                            \
    } while (false)

namespace {

bool NearlyEqual(const double lhs, const double rhs, const double tolerance = 1e-4) {
    return std::fabs(lhs - rhs) <= tolerance;
}

/// Signed volume of a closed triangle mesh via the divergence theorem; positive for outward-facing windings.
double MeshVolume(const ri::structural::CompiledMesh& mesh) {
    double volume = 0.0;
    for (std::size_t index = 0; index + 2 < mesh.positions.size(); index += 3) {
        const ri::math::Vec3& a = mesh.positions[index];
        const ri::math::Vec3& b = mesh.positions[index + 1];
        const ri::math::Vec3& c = mesh.positions[index + 2];
        volume += static_cast<double>(ri::math::Dot(a, ri::math::Cross(b, c))) / 6.0;
    }
    return volume;
}

double SolidVolume(const ri::structural::ConvexSolid& solid) {
    return std::fabs(MeshVolume(ri::structural::BuildCompiledMeshFromConvexSolid(solid)));
}

double SolidsVolume(const std::vector<ri::structural::ConvexSolid>& solids) {
    double volume = 0.0;
    for (const ri::structural::ConvexSolid& solid : solids) {
        volume += SolidVolume(solid);
    }
    return volume;
}

std::vector<ri::structural::Plane> CollectPlanes(const ri::structural::ConvexSolid& solid) {
    std::vector<ri::structural::Plane> planes;
    planes.reserve(solid.polygons.size());
    for (const ri::structural::ConvexPolygon& polygon : solid.polygons) {
        planes.push_back(polygon.plane);
    }
    return planes;
}

} // namespace

int main() {
    using namespace ri::structural;

    // Unit box solid: 6 polygons, 12 triangles, volume 1.
    const ConvexSolid unitBox = CreateAxisAlignedBoxSolid();
    RI_REQUIRE(unitBox.polygons.size() == 6);
    const CompiledMesh boxMesh = BuildCompiledMeshFromConvexSolid(unitBox);
    RI_REQUIRE(boxMesh.triangleCount == 12);
    RI_REQUIRE(boxMesh.positions.size() == 36);
    RI_REQUIRE(boxMesh.normals.size() == 36);
    RI_REQUIRE(boxMesh.hasBounds);
    RI_REQUIRE(NearlyEqual(boxMesh.boundsMin.x, -0.5) && NearlyEqual(boxMesh.boundsMax.x, 0.5));
    RI_REQUIRE(NearlyEqual(MeshVolume(boxMesh), 1.0));

    // Splitting through the center yields two closed halves of equal volume plus a quad cap.
    const Plane splitPlane{.normal = {1.0f, 0.0f, 0.0f}, .constant = 0.0f};
    const ConvexSolidClipResult split = ClipConvexSolidByPlane(unitBox, splitPlane);
    RI_REQUIRE(split.split);
    RI_REQUIRE(split.front.has_value());
    RI_REQUIRE(split.back.has_value());
    RI_REQUIRE(split.capPoints.size() == 4);
    RI_REQUIRE(NearlyEqual(SolidVolume(*split.front), 0.5));
    RI_REQUIRE(NearlyEqual(SolidVolume(*split.back), 0.5));

    // A plane fully outside the solid must not split it.
    const Plane outsidePlane{.normal = {1.0f, 0.0f, 0.0f}, .constant = -2.0f};
    const ConvexSolidClipResult noSplit = ClipConvexSolidByPlane(unitBox, outsidePlane);
    RI_REQUIRE(!noSplit.split);
    RI_REQUIRE(!noSplit.front.has_value());
    RI_REQUIRE(noSplit.back.has_value());

    // Boolean subtract: carving a centered half-size box removes exactly its volume.
    const ConvexSolid cutter = CreateAxisAlignedBoxSolid({-0.25f, -0.25f, -0.25f}, {0.25f, 0.25f, 0.25f});
    const std::vector<ConvexSolid> carved = SubtractConvexPlanesFromSolid(unitBox, CollectPlanes(cutter));
    RI_REQUIRE(!carved.empty());
    RI_REQUIRE(NearlyEqual(SolidsVolume(carved), 1.0 - 0.125));

    // Boolean intersect: clamping to the cutter's half-spaces keeps exactly the overlap volume.
    const std::vector<ConvexSolid> clamped = IntersectSolidWithConvexPlanes(unitBox, CollectPlanes(cutter));
    RI_REQUIRE(!clamped.empty());
    RI_REQUIRE(NearlyEqual(SolidsVolume(clamped), 0.125));

    // Subtracting a fully-covering volume leaves nothing inside (all pieces are outside the solid).
    const ConvexSolid envelope = CreateAxisAlignedBoxSolid({-2.0f, -2.0f, -2.0f}, {2.0f, 2.0f, 2.0f});
    const std::vector<ConvexSolid> swallowed = SubtractConvexPlanesFromSolid(unitBox, CollectPlanes(envelope));
    RI_REQUIRE(NearlyEqual(SolidsVolume(swallowed), 0.0));

    // Plane dedupe: exact duplicates and negated duplicates collapse; distinct planes survive.
    const Plane planeA{.normal = {1.0f, 0.0f, 0.0f}, .constant = -0.5f};
    const Plane planeANegated = NegatePlane(planeA);
    const Plane planeB{.normal = {0.0f, 1.0f, 0.0f}, .constant = -0.5f};
    const std::vector<Plane> deduped = DedupeConvexPlanes({planeA, planeA, planeANegated, planeB});
    RI_REQUIRE(deduped.size() == 2);

    // Merge: two valid meshes concatenate; malformed inputs (odd counts / mismatched normals) are skipped.
    CompiledMesh malformed = boxMesh;
    malformed.normals.pop_back();
    const CompiledMesh merged = MergeCompiledMeshes({boxMesh, boxMesh, malformed});
    RI_REQUIRE(merged.triangleCount == 24);
    RI_REQUIRE(merged.positions.size() == 72);
    RI_REQUIRE(merged.normals.size() == 72);
    RI_REQUIRE(merged.hasBounds);
    RI_REQUIRE(NearlyEqual(MeshVolume(merged), 2.0));

    // Polygon clip: a face crossing the plane produces both sides plus recorded intersections.
    const ConvexPolygon bottomFace = unitBox.polygons.front();
    const ConvexPolygonClipResult polygonClip = ClipConvexPolygonByPlane(bottomFace, splitPlane);
    RI_REQUIRE(polygonClip.front.has_value());
    RI_REQUIRE(polygonClip.back.has_value());
    RI_REQUIRE(polygonClip.intersections.size() == 2);

    return EXIT_SUCCESS;
}
