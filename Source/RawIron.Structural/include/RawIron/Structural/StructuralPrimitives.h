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
    /// Edge fillet radius for `rounded_box` (meters, in unit-cube space before scale). 0 uses a sensible default.
    float bevelRadius = 0.0f;
    int bevelSegments = 0;
    bool centerColumn = true;
    std::string archStyle = "round";
    std::string latticeStyle = "x_brace";
    std::vector<ri::math::Vec3> points;
    std::vector<ri::math::Vec3> vertices;
    /// Row-major height samples on the unit XZ patch \([-0.5,0.5]^2\) for `heightmap_patch` / `displacement` / `terrain_quad`.
    /// When non-empty, length must equal \((cellsX+1)(cellsZ+1)\) with `cellsX`,`cellsZ` > 0 (else ignored).
    std::vector<float> heightfieldSamples{};
};

/// True when `type` is implemented by `BuildPrimitiveMesh` / collision solids in this module.
/// The `protoengine` browser preview only implements a small dispatch set; `displacement` / `voronoi_fracture` / `metaball_cluster` / `lsystem_branch` / full heightfield samples are **native** here.
[[nodiscard]] bool IsNativeStructuralPrimitive(std::string_view type);
/// True when the native structural type has a **polyhedral convex solid** suitable for boolean CSG / convex clipping
/// (`ConvexSolid` pipeline). Mesh-forward primitives (`stairs`, `rounded_box`, …) return false; `box` and `hollow_box`
/// return true because the compiler builds solids for them explicitly.
[[nodiscard]] bool StructuralPrimitiveHasConvexSolidSupport(std::string_view type);
/// ASCII lowercase + trim outer whitespace; stable map keys across authoring variants (` Box ` → `box`).
[[nodiscard]] std::string NormalizeStructuralPrimitiveTypeKey(std::string_view type);
[[nodiscard]] std::optional<ConvexSolid> CreateConvexPrimitiveSolid(std::string_view type,
                                                                    const StructuralPrimitiveOptions& options = {});
[[nodiscard]] CompiledMesh BuildPrimitiveMesh(std::string_view type,
                                             const StructuralPrimitiveOptions& options = {});

/// Single-call hipped-roof builder. `ridgeRatio` is clamped (0.08..0.85, halved for the ridge half-extent;
/// see `CreateHippedRoofSolid`). The JS proto-engine renders a 16-triangle non-indexed stand-in with
/// the same vertex topology; native callers should prefer this for collision-quality geometry.
[[nodiscard]] CompiledMesh BuildHippedRoofCompiledMesh(float ridgeRatio = 0.34f);
/// Single-call convex-hull builder from arbitrary points. Uses `CreateConvexHullSolidFromPoints` plus
/// `BuildCompiledMeshFromConvexSolid`; falls back to a unit AABB stand-in when fewer than 4 unique
/// points are supplied (matching what the JS proto-engine renders for the `convex_hull` primitive).
[[nodiscard]] CompiledMesh BuildConvexHullCompiledMeshFromPoints(const std::vector<ri::math::Vec3>& points);

} // namespace ri::structural
