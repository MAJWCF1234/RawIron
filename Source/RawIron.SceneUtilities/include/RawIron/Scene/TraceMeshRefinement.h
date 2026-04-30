#pragma once

#include "RawIron/Math/Vec3.h"
#include "RawIron/Scene/Raycast.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Trace/TraceScene.h"

#include <optional>

namespace ri::scene {

/// Options for refining a coarse \ref ri::trace::TraceRay hit against triangle soup (custom meshes).
struct MeshTraceRefinementOptions {
    /// Reject hits farther than \ref ri::trace::TraceHit::time + epsilon from coarse ray cast.
    float coarseDistanceEpsilon = 0.08f;
    /// Grazing / degenerate triangle epsilon (matches ray cast internals scale).
    float triangleEpsilon = 1e-4f;
    /// When true, ignores intersections whose geometric normals face the ray (backface hits).
    bool cullBackfaces = true;
    /// When retaining a backface ( \p cullBackfaces false ), flip shading normal toward incident ray.
    bool flipTowardRay = true;
};

/// After an AABB broad-phase ray hit (\p coarse), recomputes contact position and **world** shading normal from mesh
/// triangles on \p nodeHandle. Returns nullopt if no triangle agrees within distance epsilon or mesh has no geometry.
[[nodiscard]] std::optional<ri::trace::TraceHit> RefineTraceRayHitWithMeshTriangles(const ri::trace::TraceHit& coarse,
                                                                                  const Scene& scene,
                                                                                  int nodeHandle,
                                                                                  const Ray& worldRay,
                                                                                  const MeshTraceRefinementOptions& options = {});

} // namespace ri::scene
