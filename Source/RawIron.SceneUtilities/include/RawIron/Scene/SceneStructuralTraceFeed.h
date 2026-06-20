#pragma once

#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/SceneSubtreeColliders.h"
#include "RawIron/Scene/TraceMeshRefinement.h"
#include "RawIron/Trace/MovementController.h"
#include "RawIron/Trace/TraceScene.h"

#include <cstddef>
#include <vector>

namespace ri::scene {

struct StructuralTraceSceneFeedMetrics {
    std::size_t sourceStructuralBrushCount = 0;
    std::size_t filteredStructuralBrushCount = 0;
    std::size_t collisionPolicyFilteredCount = 0;
    std::size_t queryChannelFilteredCount = 0;
    std::size_t queryPurposeFilteredCount = 0;
    std::size_t colliderCount = 0;
    std::size_t staticColliderCount = 0;
    std::size_t structuralStaticColliderCount = 0;
    std::size_t dynamicColliderCount = 0;
};

struct StructuralTraceSceneFeedResult {
    ri::trace::TraceScene traceScene{};
    StructuralTraceSceneFeedMetrics metrics{};
};

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

[[nodiscard]] ri::trace::TraceScene BuildStructuralTraceSceneForSubtree(
    const Scene& scene,
    int rootNodeHandle,
    ri::spatial::SpatialIndexOptions indexOptions = {});
[[nodiscard]] ri::trace::TraceScene BuildStructuralTraceSceneForSubtree(
    const Scene& scene,
    int rootNodeHandle,
    const SubtreeColliderBuildOptions& options,
    ri::spatial::SpatialIndexOptions indexOptions = {});

[[nodiscard]] StructuralTraceSceneFeedResult BuildStructuralTraceSceneFeedForSubtree(
    const Scene& scene,
    int rootNodeHandle,
    ri::spatial::SpatialIndexOptions indexOptions = {});
[[nodiscard]] StructuralTraceSceneFeedResult BuildStructuralTraceSceneFeedForSubtree(
    const Scene& scene,
    int rootNodeHandle,
    const SubtreeColliderBuildOptions& options,
    ri::spatial::SpatialIndexOptions indexOptions = {});

/// Builds a refiner that maps coarse \ref ri::trace::TraceHit::id to a scene node name, then runs
/// \ref RefineTraceRayHitWithMeshTriangles for custom mesh geometry. Returns `nullopt` to keep the coarse hit.
[[nodiscard]] ri::trace::StructuralTraceRefiner MakeStructuralMeshTraceRefiner(
    const Scene& scene,
    const MeshTraceRefinementOptions& meshOptions = {});

} // namespace ri::scene
