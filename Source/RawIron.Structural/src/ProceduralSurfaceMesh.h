#pragma once

#include "RawIron/Scene/Components.h"
#include "RawIron/Structural/StructuralPrimitives.h"
#include <functional>
#include <span>

namespace ri::structural::detail {
using ri::scene::Mesh;

// Private tessellation helpers for BuildPrimitiveMesh; not a second authoring API.
[[nodiscard]] CompiledMesh BuildSmoothStructuralSurface(std::string_view type, const StructuralPrimitiveOptions& options);

// All generators produce indexed custom meshes with normals and UVs. Invalid,
// non-finite, degenerate or oversized inputs throw std::invalid_argument.
// Each generated mesh is limited to 1,048,576 vertices (including seams/caps).
struct ParametricMeshOptions {
    unsigned segmentsU = 64;
    unsigned segmentsV = 24;
    // Periodic boundaries duplicate positions/normals but retain UV 0/1.
    bool periodicU = false;
    bool periodicV = false;
};
using SurfaceEvaluator = std::function<ri::math::Vec3(float u, float v)>;
// Evaluator domain is [0,1]^2; normals follow dP/du cross dP/dv.
[[nodiscard]] Mesh BuildParametricMesh(const SurfaceEvaluator& evaluate, ParametricMeshOptions options = {});

struct LatheMeshOptions {
    unsigned radialSegments = 64;
    float startRadians = 0.0f;
    float sweepRadians = 6.283185307179586f;
};
// Profile is (positive radius, height), strictly increasing in height. Open ends,
// no axis poles/caps. V measures profile arc length; full sweeps have exact seams.
[[nodiscard]] Mesh BuildLatheMesh(std::span<const ri::math::Vec2> profile, LatheMeshOptions options = {});

// Uniform Catmull-Rom, endpoint extrapolation for open curves. Closed output
// includes the repeated first sample, ready for BuildTubeMesh(closed=true).
[[nodiscard]] std::vector<ri::math::Vec3> SampleCatmullRomPath(
    std::span<const ri::math::Vec3> controlPoints, unsigned segments, bool closed = false);
struct TubeMeshOptions {
    float radius = 0.2f;
    unsigned radialSegments = 16;
    bool closed = false;
    bool capEnds = true;
};
// Parallel-transport frames with distributed closure twist; U measures path arc
// length. Closed paths must repeat their first point. Caps have separate normals.
// Self-intersection/curvature clearance is an authoring responsibility.
[[nodiscard]] Mesh BuildTubeMesh(std::span<const ri::math::Vec3> path, TubeMeshOptions options = {});

[[nodiscard]] Mesh BuildTorusMesh(float majorRadius, float minorRadius, unsigned rings = 64, unsigned sides = 24);
// Non-orientable open ribbon: render with a double-sided material.
[[nodiscard]] Mesh BuildMobiusMesh(float radius, float halfWidth, unsigned rings = 96, unsigned widthSegments = 16);

} // namespace ri::structural::detail
