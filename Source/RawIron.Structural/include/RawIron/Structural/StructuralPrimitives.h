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
    int hemisphereSegments = 6;
    float thickness = 0.16f;
    float topRadius = 0.18f;
    float bottomRadius = 0.5f;
    float length = 0.5f;
    float spanDegrees = 180.0f;
    float ridgeRatio = 0.34f;
    std::string archStyle = "round";
    std::vector<ri::math::Vec3> points;
    std::vector<ri::math::Vec3> vertices;
};

[[nodiscard]] bool IsNativeStructuralPrimitive(std::string_view type);
[[nodiscard]] std::optional<ConvexSolid> CreateConvexPrimitiveSolid(std::string_view type,
                                                                    const StructuralPrimitiveOptions& options = {});
[[nodiscard]] CompiledMesh BuildPrimitiveMesh(std::string_view type,
                                             const StructuralPrimitiveOptions& options = {});

} // namespace ri::structural
