#pragma once
#include "RawIron/Scene/Components.h"
#include <span>

namespace ri::scene {
struct MeshUvTransform {
    ri::math::Vec2 offset{};
    ri::math::Vec2 repeat{1,1};
    ri::math::Vec2 center{.5f,.5f};
    float rotationRadians=0;
};
struct MeshMorphTarget {
    std::vector<ri::math::Vec3> positions;
    // Optional absolute normals, supplied for every target and on the base mesh.
    std::vector<ri::math::Vec3> normals;
};
struct MeshClipPlane { ri::math::Vec3 normal{0,1,0}; float constant=0; };
/// Owned results preserve the source. Invalid streams/parameters throw invalid_argument.
[[nodiscard]] Mesh TransformMeshUvs(const Mesh& source,const MeshUvTransform& transform);
/// Absolute targets: base + sum(weight * (target - base)); weights may extrapolate.
/// Blends authored normals when all targets supply them; otherwise recomputes
/// area-weighted normals. Keeps topology/UV/color streams and hard-edge topology.
[[nodiscard]] Mesh BlendMeshMorphTargets(const Mesh& source,std::span<const MeshMorphTarget> targets,
    std::span<const float> weights);
/// Intersect mesh-local half spaces n.p + constant >= 0. Interpolates attributes;
/// open section boundaries are intentional. No synthetic caps or shadow-only geometry.
[[nodiscard]] Mesh ClipMeshPlanes(const Mesh& source,std::span<const MeshClipPlane> planes);
/// RGB channels follow local XYZ within the authored range; output is scene-linear.
[[nodiscard]] Mesh ColorMeshByPosition(const Mesh& source,ri::math::Vec3 minimum,ri::math::Vec3 maximum);
} // namespace ri::scene
