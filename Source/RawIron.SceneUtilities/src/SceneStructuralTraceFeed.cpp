#include "RawIron/Scene/SceneStructuralTraceFeed.h"

#include "RawIron/Scene/SceneSubtreeColliders.h"
#include "RawIron/Scene/SceneUtils.h"
#include "RawIron/Scene/TraceMeshRefinement.h"

#include <utility>

namespace ri::scene {
namespace {

[[nodiscard]] std::size_t CountStructuralBrushesInSubtree(const Scene& scene,
                                                          const int rootNodeHandle) {
    std::size_t count = 0;
    for (const int nodeHandle : CollectNodeSubtree(scene, rootNodeHandle, true)) {
        if (!scene.GetNode(nodeHandle).structuralBrush.brushId.empty()) {
            ++count;
        }
    }
    return count;
}

void CountStructuralBrushFilterReasons(const Scene& scene,
                                       const int rootNodeHandle,
                                       const SubtreeColliderBuildOptions& options,
                                       StructuralTraceSceneFeedMetrics& metrics) {
    for (const int nodeHandle : CollectNodeSubtree(scene, rootNodeHandle, true)) {
        const StructuralBrushMetadata& metadata = scene.GetNode(nodeHandle).structuralBrush;
        if (metadata.brushId.empty()) {
            continue;
        }

        if (options.respectStructuralBrushCollisionPolicy
            && (metadata.collision == StructuralBrushCollisionPolicy::None
                || metadata.collision == StructuralBrushCollisionPolicy::Query
                || metadata.collision == StructuralBrushCollisionPolicy::Detail)) {
            ++metrics.collisionPolicyFilteredCount;
            continue;
        }

        if (options.requireStructuralBrushQueryMeshChannel
            && !StructuralBrushParticipatesInChannel(metadata, StructuralBrushChannel::QueryMesh)) {
            ++metrics.queryChannelFilteredCount;
            continue;
        }

        if (options.requiredStructuralBrushQueryPurpose.has_value()
            && !StructuralBrushSupportsQueryPurpose(metadata, *options.requiredStructuralBrushQueryPurpose)) {
            ++metrics.queryPurposeFilteredCount;
            continue;
        }
    }
}

[[nodiscard]] StructuralTraceSceneFeedMetrics ComputeStructuralTraceSceneFeedMetrics(
    const std::vector<ri::trace::TraceCollider>& colliders,
    const std::size_t sourceStructuralBrushCount) {
    StructuralTraceSceneFeedMetrics metrics{};
    metrics.sourceStructuralBrushCount = sourceStructuralBrushCount;
    metrics.colliderCount = colliders.size();
    if (metrics.sourceStructuralBrushCount > metrics.colliderCount) {
        metrics.filteredStructuralBrushCount = metrics.sourceStructuralBrushCount - metrics.colliderCount;
    }
    for (const ri::trace::TraceCollider& collider : colliders) {
        if (!collider.dynamic) {
            ++metrics.staticColliderCount;
            if (collider.structural) {
                ++metrics.structuralStaticColliderCount;
            }
        } else {
            ++metrics.dynamicColliderCount;
        }
    }
    return metrics;
}

[[nodiscard]] int FindNodeHandleByColliderName(const Scene& scene, const std::string_view colliderId) {
    if (colliderId.empty()) {
        return -1;
    }
    for (int handle = 0; handle < static_cast<int>(scene.NodeCount()); ++handle) {
        if (scene.GetNode(handle).name == colliderId) {
            return handle;
        }
    }
    return -1;
}

} // namespace

SubtreeColliderBuildOptions MakeDefaultStructuralTraceColliderBuildOptions() {
    SubtreeColliderBuildOptions options{};
    options.structural = true;
    options.dynamic = false;
    options.respectStructuralBrushCollisionPolicy = true;
    options.requireStructuralBrushQueryMeshChannel = true;
    options.requiredStructuralBrushQueryPurpose = StructuralBrushQueryPurpose::Trace;
    options.appendStructuralBrushSemanticTags = true;
    return options;
}

std::vector<ri::trace::TraceCollider> BuildStructuralTraceCollidersForSubtree(
    const Scene& scene,
    const int rootNodeHandle) {
    return BuildStructuralTraceCollidersForSubtree(scene,
                                                  rootNodeHandle,
                                                  MakeDefaultStructuralTraceColliderBuildOptions());
}

std::vector<ri::trace::TraceCollider> BuildStructuralTraceCollidersForSubtree(
    const Scene& scene,
    const int rootNodeHandle,
    const SubtreeColliderBuildOptions& options) {
    std::vector<ri::trace::TraceCollider> colliders;
    const std::size_t added = AppendTraceCollidersForSubtree(scene, rootNodeHandle, options, colliders);
    (void)added;
    return colliders;
}

ri::trace::TraceScene BuildStructuralTraceSceneForSubtree(
    const Scene& scene,
    const int rootNodeHandle,
    ri::spatial::SpatialIndexOptions indexOptions) {
    return BuildStructuralTraceSceneForSubtree(scene,
                                              rootNodeHandle,
                                              MakeDefaultStructuralTraceColliderBuildOptions(),
                                              indexOptions);
}

ri::trace::TraceScene BuildStructuralTraceSceneForSubtree(
    const Scene& scene,
    const int rootNodeHandle,
    const SubtreeColliderBuildOptions& options,
    ri::spatial::SpatialIndexOptions indexOptions) {
    return ri::trace::TraceScene(
        BuildStructuralTraceCollidersForSubtree(scene, rootNodeHandle, options),
        indexOptions);
}

StructuralTraceSceneFeedResult BuildStructuralTraceSceneFeedForSubtree(
    const Scene& scene,
    const int rootNodeHandle,
    ri::spatial::SpatialIndexOptions indexOptions) {
    return BuildStructuralTraceSceneFeedForSubtree(scene,
                                                  rootNodeHandle,
                                                  MakeDefaultStructuralTraceColliderBuildOptions(),
                                                  indexOptions);
}

StructuralTraceSceneFeedResult BuildStructuralTraceSceneFeedForSubtree(
    const Scene& scene,
    const int rootNodeHandle,
    const SubtreeColliderBuildOptions& options,
    ri::spatial::SpatialIndexOptions indexOptions) {
    std::vector<ri::trace::TraceCollider> colliders =
        BuildStructuralTraceCollidersForSubtree(scene, rootNodeHandle, options);
    StructuralTraceSceneFeedResult result{};
    result.metrics = ComputeStructuralTraceSceneFeedMetrics(
        colliders,
        CountStructuralBrushesInSubtree(scene, rootNodeHandle));
    CountStructuralBrushFilterReasons(scene, rootNodeHandle, options, result.metrics);
    result.traceScene = ri::trace::TraceScene(std::move(colliders), indexOptions);
    return result;
}

ri::trace::StructuralTraceRefiner MakeStructuralMeshTraceRefiner(const Scene& scene,
                                                                  const MeshTraceRefinementOptions& meshOptions) {
    return [scenePtr = &scene, meshOptions](const ri::trace::TraceHit& coarse,
                                            const ri::math::Vec3& rayOriginWorld,
                                            const ri::math::Vec3& rayDirectionUnitWorld)
               -> std::optional<ri::trace::TraceHit> {
        const int nodeHandle = FindNodeHandleByColliderName(*scenePtr, coarse.id);
        if (nodeHandle < 0) {
            return std::nullopt;
        }
        const Ray worldRay{
            .origin = rayOriginWorld,
            .direction = rayDirectionUnitWorld,
        };
        return RefineTraceRayHitWithMeshTriangles(coarse, *scenePtr, nodeHandle, worldRay, meshOptions);
    };
}

} // namespace ri::scene
