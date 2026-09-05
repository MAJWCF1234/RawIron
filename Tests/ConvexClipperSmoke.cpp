#include "RawIron/Structural/ConvexClipper.h"
#include "RawIron/Structural/StructuralCompiler.h"
#include "RawIron/Structural/StructuralDeferredOperations.h"

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

bool TestDeferredSurfacePayload() {
    using namespace ri::structural;
    using namespace ri::math;
    bool ok = true;
    const auto check = [&](bool condition, const char* message) {
        if (!condition) { std::fprintf(stderr, "%s\n", message); ok = false; }
    };
    CompiledGeometryNode source;
    source.node.id = "textured-source";
    source.node.scale = {-2,3,.5f};
    const std::vector<Vec3> local{{1,0,0},{2,0,0},{1,1,1}, {4,0,0},{5,0,0},{4,1,1}};
    const Vec3 normal = Normalize({0,-1,1});
    const Vec3 transformedNormal = Normalize({0,-1.f/3,2});
    std::vector<Vec3> world;
    for (auto p : local) world.push_back(TransformPoint(ScaleMatrix(source.node.scale),p));
    source.compiledMesh.positions = world;
    source.compiledMesh.normals = std::vector<Vec3>(6,transformedNormal);
    source.compiledMesh.triangleCount = 2;
    source.compiledMesh.hasBounds = true;
    source.compiledMesh.boundsMin = {-10,0,0}; source.compiledMesh.boundsMax = {-2,3,.5f};
    source.compiledMesh.texCoords = {{0,0},{1,0},{0,1},{.2f,.3f},{.8f,.3f},{.2f,.9f}};
    source.compiledWorldSpace = true;
    StructuralDeferredTargetOperation op;
    op.node.id = "spline-copy"; op.node.count = 2; op.node.points = {{0,0,0},{0,0,10}};
    const auto copies = BuildSplineMeshDeformerCompiledNodes(op,{source});
    check(copies.size()==2,"deferred copy count");
    if (copies.size()==2) {
        check(copies[0].compiledMesh.texCoords.size()==6,"deferred spline loses authored UVs");
        check(Distance(copies[0].compiledMesh.positions[0],world[0])<1.e-5f,"deferred spline loses mirrored scale");
        check(Distance(copies[0].compiledMesh.normals[0],transformedNormal)<1.e-5f,"deferred spline corrupts scaled normals");
    }
    source.compiledMesh.positions = local;
    source.compiledMesh.normals = std::vector<Vec3>(6,normal);
    source.compiledMesh.boundsMin = {1,0,0}; source.compiledMesh.boundsMax = {5,1,1};
    source.compiledMesh.texCoords = {{0,0},{1,0},{0,1},{.2f,.3f},{.8f,.3f},{.2f,.9f}};
    source.compiledWorldSpace = false;
    const auto reflected = TransformCompiledMesh(source.compiledMesh,ScaleMatrix(source.node.scale));
    const auto face=Normalize(Cross(reflected.positions[1]-reflected.positions[0],reflected.positions[2]-reflected.positions[0]));
    check(Distance(face,reflected.normals[0])<1.e-5f,"structural reflected winding and normal disagree");
    check(reflected.texCoords[1].y==1 && reflected.texCoords[2].x==1,"reflected winding must swap UV corners too");
    const auto merged = MergeCompiledMeshes({source.compiledMesh,source.compiledMesh});
    check(merged.texCoords.size()==12,"structural mesh merge loses authored UVs");
    if (merged.texCoords.size()==12) check(merged.texCoords[9].x==.2f && merged.texCoords[11].y==.9f,"merged UV order");
    auto noUvs=source.compiledMesh; noUvs.texCoords.clear();
    check(MergeCompiledMeshes({source.compiledMesh,noUvs}).texCoords.empty(),"mixed UV streams require complete projection fallback");
    const auto localCopies = BuildSplineMeshDeformerCompiledNodes(op,{source});
    check(localCopies.size()==2 && Distance(localCopies[0].compiledMesh.positions[0],world[0])<1.e-5f,
          "deferred spline inversely transforms an already-local mesh");
    StructuralNode cut;
    cut.position = {1.33f,.33f,.33f}; cut.scale = {1,1,1};
    const auto clipped = ApplyTerrainHoleCutoutToMesh(source.compiledMesh,cut);
    check(clipped.positions.size()==3 && clipped.texCoords.size()==3,"terrain cutout loses retained triangle UVs");
    if (clipped.texCoords.size()==3) check(clipped.texCoords[0].x==.2f && clipped.texCoords[2].y==.9f,"terrain cutout mismatches UV corners");
    return ok;
}

int main() {
    using namespace ri::structural;
    RI_REQUIRE(TestDeferredSurfacePayload());

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
