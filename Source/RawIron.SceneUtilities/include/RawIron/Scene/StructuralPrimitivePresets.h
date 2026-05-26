#pragma once

#include "RawIron/Structural/StructuralPrimitives.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace ri::scene {

struct StructuralPrimitivePreset {
    const char* label = "box";
    const char* structuralType = "box";
    int radialSegments = 16;
    int sides = 16;
    int steps = 8;
    int hemisphereSegments = 6;
    float thickness = 0.16f;
    float topRadius = 0.18f;
    float bottomRadius = 0.5f;
    float length = 0.5f;
    float spanDegrees = 180.0f;
    float ridgeRatio = 0.34f;
    const char* archStyle = "round";
    float bevelRadius = 0.08f;
    int bevelSegments = 3;
};

/// Curated structural brush presets (editor + game assembly).
inline constexpr std::array<StructuralPrimitivePreset, 30> kStructuralPrimitivePresets{{
    {.label = "box", .structuralType = "box"},
    {.label = "plane", .structuralType = "plane"},
    {.label = "arch_round", .structuralType = "arch", .thickness = 0.16f, .spanDegrees = 180.0f, .archStyle = "round"},
    {.label = "arch_gothic", .structuralType = "arch", .thickness = 0.18f, .archStyle = "gothic"},
    {.label = "hollow_box", .structuralType = "hollow_box"},
    {.label = "ramp", .structuralType = "ramp"},
    {.label = "wedge", .structuralType = "wedge"},
    {.label = "cylinder", .structuralType = "cylinder", .radialSegments = 16},
    {.label = "cylinder_hi", .structuralType = "cylinder", .radialSegments = 24},
    {.label = "cone", .structuralType = "cone", .sides = 16},
    {.label = "pyramid", .structuralType = "pyramid", .sides = 4},
    {.label = "capsule", .structuralType = "capsule", .radialSegments = 16, .hemisphereSegments = 6, .length = 0.5f},
    {.label = "capsule_tall", .structuralType = "capsule", .radialSegments = 16, .hemisphereSegments = 8, .length = 1.2f},
    {.label = "frustum", .structuralType = "frustum", .radialSegments = 16, .topRadius = 0.18f, .bottomRadius = 0.5f},
    {.label = "frustum_wide", .structuralType = "frustum", .radialSegments = 16, .topRadius = 0.30f, .bottomRadius = 0.8f},
    {.label = "geodesic_sphere", .structuralType = "geodesic_sphere"},
    {.label = "hexahedron", .structuralType = "hexahedron"},
    {.label = "convex_hull", .structuralType = "convex_hull"},
    {.label = "roof_gable", .structuralType = "roof_gable"},
    {.label = "hipped_roof", .structuralType = "hipped_roof"},
    {.label = "roof_gable_sharp", .structuralType = "roof_gable", .ridgeRatio = 0.24f},
    {.label = "hipped_roof_flat", .structuralType = "hipped_roof", .ridgeRatio = 0.48f},
    {.label = "arch_round_wide", .structuralType = "arch", .thickness = 0.12f, .spanDegrees = 240.0f, .archStyle = "round"},
    {.label = "arch_round_thick", .structuralType = "arch", .thickness = 0.30f, .spanDegrees = 180.0f, .archStyle = "round"},
    {.label = "torus", .structuralType = "torus", .radialSegments = 20, .sides = 12},
    {.label = "stairs", .structuralType = "stairs", .steps = 8},
    {.label = "spiral_stairs", .structuralType = "spiral_stairs", .radialSegments = 16, .steps = 10},
    {.label = "dome_vault", .structuralType = "dome_vault", .radialSegments = 20},
    {.label = "half_pipe", .structuralType = "half_pipe", .radialSegments = 20, .spanDegrees = 180.0f},
    {.label = "rounded_box", .structuralType = "rounded_box", .bevelRadius = 0.08f, .bevelSegments = 3},
}};

[[nodiscard]] ri::structural::StructuralPrimitiveOptions ShapeFromStructuralPreset(
    const StructuralPrimitivePreset& preset);

[[nodiscard]] std::optional<StructuralPrimitivePreset> FindStructuralPreset(std::string_view labelOrType);

} // namespace ri::scene
