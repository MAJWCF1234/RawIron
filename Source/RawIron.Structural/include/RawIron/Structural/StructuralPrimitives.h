#pragma once

#include "RawIron/Structural/ConvexClipper.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::structural {

struct StructuralPrimitiveOptions {
    int radialSegments = 16;
    int sides = 16;
    int detail = 0;
    int steps = 0;
    int cellsX = 0;
    int cellsY = 0;
    int cellsZ = 0;
    int hemisphereSegments = 6;
    float thickness = 0.16f;
    float depth = 0.5f;
    float strutRadius = 0.035f;
    float topRadius = 0.18f;
    float bottomRadius = 0.5f;
    float length = 0.5f;
    float exponentX = 1.0f;
    float exponentY = 1.0f;
    float exponentZ = 1.0f;
    float spanDegrees = 180.0f;
    float sweepDegrees = 360.0f;
    float startDegrees = 0.0f;
    float ridgeRatio = 0.34f;
    bool centerColumn = true;
    std::string archStyle = "round";
    std::string latticeStyle = "x_brace";
    std::vector<ri::math::Vec3> points;
    std::vector<ri::math::Vec3> vertices;
};

[[nodiscard]] bool IsNativeStructuralPrimitive(std::string_view type);
[[nodiscard]] std::optional<ConvexSolid> CreateConvexPrimitiveSolid(std::string_view type,
                                                                    const StructuralPrimitiveOptions& options = {});
[[nodiscard]] CompiledMesh BuildPrimitiveMesh(std::string_view type,
                                             const StructuralPrimitiveOptions& options = {});

} // namespace ri::structural
