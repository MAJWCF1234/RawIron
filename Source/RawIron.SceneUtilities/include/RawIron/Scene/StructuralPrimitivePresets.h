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
    int detail = 0;
    int steps = 8;
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
    float spanDegrees = 180.0f;
    float sweepDegrees = 360.0f;
    float startDegrees = 0.0f;
    float ridgeRatio = 0.34f;
    float exponentX = 1.0f;
    float exponentY = 1.0f;
    float exponentZ = 1.0f;
    const char* archStyle = "round";
    const char* latticeStyle = "x_brace";
    float bevelRadius = 0.08f;
    int bevelSegments = 3;
    bool centerColumn = true;
};

/// Curated structural primitive presets (editor + game assembly).
inline constexpr auto kStructuralPrimitivePresets = std::to_array<StructuralPrimitivePreset>({
    {.label = "box", .structuralType = "box"},
    {.label = "plane", .structuralType = "plane"},
    {.label = "arch_round", .structuralType = "arch", .thickness = 0.16f, .spanDegrees = 180.0f, .archStyle = "round"},
    {.label = "arch_flat_top", .structuralType = "arch", .thickness = 0.16f, .spanDegrees = 180.0f, .archStyle = "flat_top"},
    {.label = "arch_gothic", .structuralType = "arch", .thickness = 0.18f, .archStyle = "gothic"},
    {.label = "hollow_box", .structuralType = "hollow_box"},
    {.label = "hollow_box_wide", .structuralType = "hollow_box", .thickness = 0.10f},
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
    {.label = "uv_sphere", .structuralType = "uv_sphere", .radialSegments = 20, .sides = 28},
    {.label = "sphere_smooth", .structuralType = "uv_sphere", .radialSegments = 28, .sides = 36},
    {.label = "hexahedron", .structuralType = "hexahedron"},
    {.label = "convex_hull", .structuralType = "convex_hull"},
    {.label = "roof_gable", .structuralType = "roof_gable"},
    {.label = "hipped_roof", .structuralType = "hipped_roof"},
    {.label = "roof_gable_sharp", .structuralType = "roof_gable", .ridgeRatio = 0.24f},
    {.label = "hipped_roof_flat", .structuralType = "hipped_roof", .ridgeRatio = 0.48f},
    {.label = "arch_round_wide", .structuralType = "arch", .thickness = 0.12f, .spanDegrees = 240.0f, .archStyle = "round"},
    {.label = "arch_round_thick", .structuralType = "arch", .thickness = 0.30f, .spanDegrees = 180.0f, .archStyle = "round"},
    {.label = "torus", .structuralType = "torus", .radialSegments = 20, .sides = 12},
    {.label = "tube", .structuralType = "tube", .radialSegments = 24, .thickness = 0.18f, .topRadius = 0.32f},
    {.label = "corner", .structuralType = "corner", .radialSegments = 18, .thickness = 0.18f},
    {.label = "corner_rounded", .structuralType = "corner", .radialSegments = 20, .thickness = 0.16f, .latticeStyle = "rounded"},
    {.label = "stairs", .structuralType = "stairs", .steps = 8},
    {.label = "stairs_wide", .structuralType = "stairs", .steps = 14},
    {.label = "spiral_stairs", .structuralType = "spiral_stairs", .radialSegments = 16, .steps = 10},
    {.label = "dome_vault", .structuralType = "dome_vault", .radialSegments = 20},
    {.label = "half_pipe", .structuralType = "half_pipe", .radialSegments = 20, .spanDegrees = 180.0f},
    {.label = "quarter_pipe", .structuralType = "quarter_pipe", .radialSegments = 18, .spanDegrees = 90.0f},
    {.label = "pipe_elbow", .structuralType = "pipe_elbow", .radialSegments = 22, .sides = 10, .thickness = 0.08f, .length = 0.34f},
    {.label = "torus_slice", .structuralType = "torus_slice", .radialSegments = 24, .sides = 10, .thickness = 0.08f, .spanDegrees = 135.0f},
    {.label = "rounded_box", .structuralType = "rounded_box", .bevelRadius = 0.08f, .bevelSegments = 3},
    {.label = "rounded_box_soft", .structuralType = "rounded_box", .bevelRadius = 0.18f, .bevelSegments = 5},
    {.label = "superellipsoid", .structuralType = "superellipsoid", .radialSegments = 20, .sides = 28, .exponentX = 2.6f, .exponentY = 2.6f, .exponentZ = 2.6f},
    {.label = "superellipsoid_blocky", .structuralType = "superellipsoid", .radialSegments = 18, .sides = 24, .exponentX = 5.0f, .exponentY = 5.0f, .exponentZ = 5.0f},
    {.label = "lattice_volume", .structuralType = "lattice_volume", .radialSegments = 3, .sides = 3, .detail = 3, .cellsX = 3, .cellsY = 3, .cellsZ = 3, .strutRadius = 0.025f, .latticeStyle = "x_brace"},
    {.label = "octet_lattice", .structuralType = "lattice_volume", .radialSegments = 3, .sides = 3, .detail = 3, .cellsX = 3, .cellsY = 3, .cellsZ = 3, .strutRadius = 0.022f, .latticeStyle = "octet_truss"},
    {.label = "k_brace_lattice", .structuralType = "lattice_volume", .radialSegments = 3, .sides = 3, .detail = 3, .cellsX = 3, .cellsY = 3, .cellsZ = 3, .strutRadius = 0.024f, .latticeStyle = "k_brace"},
    {.label = "spline_sweep", .structuralType = "spline_sweep", .thickness = 0.045f},
    {.label = "revolve", .structuralType = "revolve", .radialSegments = 24, .sweepDegrees = 360.0f},
    {.label = "loft_primitive", .structuralType = "loft_primitive"},
    {.label = "spline_ribbon", .structuralType = "spline_ribbon", .thickness = 0.08f},
    {.label = "catenary", .structuralType = "catenary_primitive", .radialSegments = 20, .thickness = 0.025f, .depth = 0.28f},
    {.label = "cable", .structuralType = "cable_primitive", .radialSegments = 20, .thickness = 0.025f, .depth = 0.18f},
    {.label = "thick_polygon", .structuralType = "thick_polygon_primitive", .depth = 0.4f},
    {.label = "trim_sheet_sweep", .structuralType = "trim_sheet_sweep", .thickness = 0.04f},
    {.label = "water_surface", .structuralType = "water_surface_primitive", .radialSegments = 16, .thickness = 0.04f},
    {.label = "terrain_quad", .structuralType = "terrain_quad", .radialSegments = 16, .sides = 16, .cellsX = 16, .cellsZ = 16, .depth = 0.18f},
    {.label = "heightmap_patch", .structuralType = "heightmap_patch", .radialSegments = 16, .sides = 16, .cellsX = 16, .cellsZ = 16, .depth = 0.16f},
    {.label = "displacement", .structuralType = "displacement", .radialSegments = 16, .sides = 16, .cellsX = 16, .cellsZ = 16, .depth = 0.14f},
    {.label = "voronoi_fracture", .structuralType = "voronoi_fracture", .detail = 8},
    {.label = "metaball_cluster", .structuralType = "metaball_cluster", .radialSegments = 16, .sides = 20},
    {.label = "lsystem_branch", .structuralType = "lsystem_branch", .radialSegments = 8, .detail = 3, .thickness = 0.035f, .length = 0.34f},
    {.label = "buttress", .structuralType = "buttress", .depth = 0.45f},
    {.label = "recessed_wall_panel", .structuralType = "recessed_wall_panel", .thickness = 0.12f, .depth = 0.12f},
    {.label = "perforated_wall", .structuralType = "perforated_wall", .cellsX = 5, .cellsY = 4, .thickness = 0.08f},
    {.label = "ribbed_slab", .structuralType = "ribbed_slab", .cellsX = 7, .thickness = 0.06f},
    {.label = "beam_frame", .structuralType = "beam_frame", .thickness = 0.10f},
    {.label = "column_cluster", .structuralType = "column_cluster", .radialSegments = 12, .sides = 6, .topRadius = 0.055f, .bottomRadius = 0.32f},
    {.label = "catwalk_segment", .structuralType = "catwalk_segment", .cellsX = 5, .thickness = 0.06f},
    {.label = "skylight_oculus", .structuralType = "skylight_oculus", .radialSegments = 28, .sides = 12, .thickness = 0.07f},
    {.label = "barrel_vault", .structuralType = "barrel_vault", .radialSegments = 18, .cellsZ = 10, .thickness = 0.12f},
    {.label = "barrel_vault_long", .structuralType = "barrel_vault", .radialSegments = 20, .cellsZ = 16, .thickness = 0.10f},
    {.label = "corridor_shell", .structuralType = "corridor_shell", .thickness = 0.10f, .centerColumn = true},
    {.label = "corridor_shell_open", .structuralType = "corridor_shell", .thickness = 0.10f, .centerColumn = false},
    {.label = "lintel_beam", .structuralType = "lintel_beam", .thickness = 0.12f, .depth = 0.10f},
    {.label = "pilaster", .structuralType = "pilaster", .thickness = 0.14f, .depth = 0.18f},
    {.label = "parapet_wall", .structuralType = "parapet_wall", .thickness = 0.20f, .depth = 0.08f},
    {.label = "stair_landing", .structuralType = "stair_landing", .thickness = 0.10f, .depth = 0.08f},
    {.label = "shaft_void", .structuralType = "shaft_void", .thickness = 0.12f},
    {.label = "brise_soleil", .structuralType = "brise_soleil", .cellsY = 6, .thickness = 0.04f, .depth = 0.22f},
    {.label = "cantilever_slab", .structuralType = "cantilever_slab", .thickness = 0.10f, .depth = 0.28f},
    {.label = "retaining_batter", .structuralType = "retaining_batter", .thickness = 0.14f, .depth = 0.28f},
    {.label = "colonnade_span", .structuralType = "colonnade_span", .radialSegments = 12, .sides = 5, .thickness = 0.10f, .topRadius = 0.05f, .bottomRadius = 0.28f},
    {.label = "handrail_segment", .structuralType = "handrail_segment", .cellsX = 5, .thickness = 0.035f},
    {.label = "extrude_profile", .structuralType = "extrude_along_normal_primitive", .depth = 0.40f},
});

[[nodiscard]] ri::structural::StructuralPrimitiveOptions ShapeFromStructuralPreset(
    const StructuralPrimitivePreset& preset);

[[nodiscard]] std::optional<StructuralPrimitivePreset> FindStructuralPreset(std::string_view labelOrType);

} // namespace ri::scene
