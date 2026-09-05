#include "RawIron/Structural/StructuralPrimitives.h"

#include <cmath>
#include <cstdlib>
#include <limits>

int main() {
    // Enhanced surfaces use the same canonical primitive API and diagnostics.
    for (const auto type : {"spline_sweep","torus","mobius","parametric_patch"}) {
        const auto report=ri::structural::ValidateStructuralPrimitive(type);
        const auto mesh=ri::structural::BuildPrimitiveMesh(type);
        if (!report.valid || report.convexSolidSupported || mesh.positions.empty()
            || mesh.texCoords.size()!=mesh.positions.size() || !mesh.hasBounds) return EXIT_FAILURE;
        for (std::size_t i=0;i<mesh.positions.size();i+=3) {
            const auto face=ri::math::Cross(mesh.positions[i+1]-mesh.positions[i],mesh.positions[i+2]-mesh.positions[i]);
            if (ri::math::Dot(face,mesh.normals[i]+mesh.normals[i+1]+mesh.normals[i+2])<=0) return EXIT_FAILURE;
        }
    }
    ri::structural::StructuralPrimitiveOptions patch;
    patch.cellsX=2; patch.cellsY=2;
    for (int x=0;x<3;++x) for (int y=0;y<3;++y) patch.vertices.push_back({float(x),0,-float(y)});
    const auto lattice=ri::structural::BuildPrimitiveMesh("parametric_patch",patch);
    if (lattice.triangleCount!=8 || lattice.boundsMax.x!=2 || lattice.boundsMin.z!=-2) return EXIT_FAILURE;
    patch.vertices.pop_back();
    if (ri::structural::ValidateStructuralPrimitive("parametric_patch",patch).valid) return EXIT_FAILURE;
    ri::structural::StructuralPrimitiveOptions openProfile;
    openProfile.closedProfile=false; openProfile.points={{.5f,0,0},{.6f,1,0}};
    if (!ri::structural::ValidateStructuralPrimitive("revolve",openProfile).valid) return EXIT_FAILURE;
    openProfile.points[1].y=0;
    if (ri::structural::ValidateStructuralPrimitive("revolve",openProfile).valid) return EXIT_FAILURE;
    ri::structural::StructuralPrimitiveOptions badSweep;
    badSweep.points={{0,0,0},{0,0,0}};
    if (ri::structural::ValidateStructuralPrimitive("spline_sweep",badSweep).valid) return EXIT_FAILURE;
    if (!ri::structural::IsNativeStructuralPrimitive(" Rounded_Box ")) {
        return EXIT_FAILURE;
    }

    ri::structural::StructuralPrimitiveOptions poisoned{};
    poisoned.thickness = std::numeric_limits<float>::quiet_NaN();
    poisoned.points.push_back({std::numeric_limits<float>::infinity(), 0.0f, 0.0f});
    const ri::structural::StructuralPrimitiveValidationReport invalid =
        ri::structural::ValidateStructuralPrimitive(" rounded_box ", poisoned);
    if (invalid.valid || invalid.errors.size() < 2U || invalid.normalizedType != "rounded_box") {
        return EXIT_FAILURE;
    }

    const ri::structural::CompiledMesh sanitizedMesh =
        ri::structural::BuildPrimitiveMesh(" Rounded_Box ", poisoned);
    if (sanitizedMesh.positions.empty()) {
        return EXIT_FAILURE;
    }
    for (const ri::math::Vec3& point : sanitizedMesh.positions) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
            return EXIT_FAILURE;
        }
    }

    ri::structural::StructuralPrimitiveOptions heightfield{};
    heightfield.cellsX = 2;
    heightfield.cellsZ = 2;
    heightfield.heightfieldSamples = {0.0f, 1.0f};
    if (ri::structural::ValidateStructuralPrimitive("heightmap_patch", heightfield).valid) {
        return EXIT_FAILURE;
    }
    if (ri::structural::ValidateStructuralPrimitive("not_a_primitive", {}).valid) {
        return EXIT_FAILURE;
    }
    if (!ri::structural::ValidateStructuralPrimitive("box", {}).valid) {
        return EXIT_FAILURE;
    }

    ri::structural::StructuralPrimitiveOptions extremeSuperellipsoid{};
    extremeSuperellipsoid.exponentX = -1000.0f;
    extremeSuperellipsoid.exponentY = std::numeric_limits<float>::max();
    const ri::structural::StructuralPrimitiveValidationReport extremeReport =
        ri::structural::ValidateStructuralPrimitive("superellipsoid", extremeSuperellipsoid);
    if (extremeReport.valid || extremeReport.errors.size() < 2U) {
        return EXIT_FAILURE;
    }
    const ri::structural::CompiledMesh safeExtremeMesh =
        ri::structural::BuildPrimitiveMesh("superellipsoid", extremeSuperellipsoid);
    for (const ri::math::Vec3& point : safeExtremeMesh.positions) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
