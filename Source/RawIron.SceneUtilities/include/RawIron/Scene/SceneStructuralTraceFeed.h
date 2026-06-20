#pragma once

#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/SceneSubtreeColliders.h"
#include "RawIron/Scene/TraceMeshRefinement.h"
#include "RawIron/Trace/MovementController.h"

#include <vector>

namespace ri::scene {

/// Defaults for movement / ballistics structural traces: structural, static, blocking,
/// query-mesh-backed, trace-purpose only, with semantic tags for later routing/debugging.
[[nodiscard]] SubtreeColliderBuildOptions MakeDefaultStructuralTraceColliderBuildOptions();

[[nodiscard]] std::vector<ri::trace::TraceCollider> BuildStructuralTraceCollidersForSubtree(
    const Scene& scene,
    int rootNodeHandle);
[[nodiscard]] std::vector<ri::trace::TraceCollider> BuildStructuralTraceCollidersForSubtree(
    const Scene& scene,
    int rootNodeHandle,
    const SubtreeColliderBuildOptions& options);

/// Builds a refiner that maps coarse \ref ri::trace::TraceHit::id to a scene node name, then runs
/// \ref RefineTraceRayHitWithMeshTriangles for custom mesh geometry. Returns `nullopt` to keep the coarse hit.
[[nodiscard]] ri::trace::StructuralTraceRefiner MakeStructuralMeshTraceRefiner(
    const Scene& scene,
    const MeshTraceRefinementOptions& meshOptions = {});

} // namespace ri::scene
