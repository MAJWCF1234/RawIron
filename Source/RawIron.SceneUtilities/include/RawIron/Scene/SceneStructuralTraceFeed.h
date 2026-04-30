#pragma once

#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/TraceMeshRefinement.h"
#include "RawIron/Trace/MovementController.h"

namespace ri::scene {

/// Builds a refiner that maps coarse \ref ri::trace::TraceHit::id to a scene node name, then runs
/// \ref RefineTraceRayHitWithMeshTriangles for custom mesh geometry. Returns `nullopt` to keep the coarse hit.
[[nodiscard]] ri::trace::StructuralTraceRefiner MakeStructuralMeshTraceRefiner(
    const Scene& scene,
    const MeshTraceRefinementOptions& meshOptions = {});

} // namespace ri::scene
