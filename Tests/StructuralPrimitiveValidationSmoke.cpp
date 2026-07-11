#include "RawIron/Structural/StructuralPrimitives.h"

#include <cmath>
#include <cstdlib>
#include <limits>

int main() {
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
