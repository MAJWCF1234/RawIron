#include "RawIron/Scene/SemanticStructuralPartition.h"
#include "RawIron/Scene/Raycast.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/StructuralBrush.h"

#include <cstdlib>

int main() {
    ri::scene::SemanticStructuralPartition partition;

    ri::scene::StructuralBrushMetadata wall{};
    wall.brushId = "wall_a";
    wall.role = ri::scene::StructuralBrushSemanticRole::Wall;
    wall.region = "atrium";
    wall.operation = ri::scene::StructuralBrushOperation::Solid;
    wall.visibility = ri::scene::StructuralBrushVisibilityPolicy::Occluder;
    wall.rebuildScope = ri::scene::StructuralBrushRebuildScope::Region;
    wall.visualMesh.renderable = true;
    wall.physicsMesh.participatesInSimulation = true;
    wall.queryMesh.raycastable = true;
    wall.queryMesh.traceable = true;
    wall.queryMesh.placeable = false;
    wall.queryMesh.interactable = false;
    wall.informationLayer.reportable = true;

    ri::scene::StructuralBrushMetadata floor{};
    floor.brushId = "floor_a";
    floor.role = ri::scene::StructuralBrushSemanticRole::Floor;
    floor.region = "atrium";
    floor.operation = ri::scene::StructuralBrushOperation::Stamp;
    floor.navigation = ri::scene::StructuralBrushNavigationPolicy::Walkable;
    floor.rebuildScope = ri::scene::StructuralBrushRebuildScope::Manual;
    floor.physicsMesh.participatesInSimulation = false;
    floor.queryMesh.raycastable = true;
    floor.queryMesh.traceable = false;
    floor.queryMesh.placeable = true;
    floor.queryMesh.interactable = true;
    floor.informationLayer.reportable = true;

    ri::scene::StructuralBrushMetadata farWall = wall;
    farWall.brushId = "wall_b";
    farWall.operation = ri::scene::StructuralBrushOperation::Subtract;
    farWall.rebuildScope = ri::scene::StructuralBrushRebuildScope::Global;
    farWall.visualMesh.renderable = false;
    farWall.queryMesh.raycastable = false;
    farWall.queryMesh.traceable = false;
    farWall.queryMesh.placeable = false;
    farWall.queryMesh.interactable = false;

    partition.Rebuild({
        {
            .id = "wall_fragment",
            .bounds = {{-1.0f, 0.0f, -1.0f}, {1.0f, 3.0f, 1.0f}},
            .metadata = wall,
        },
        {
            .id = "floor_fragment",
            .bounds = {{-4.0f, -0.1f, -4.0f}, {4.0f, 0.1f, 4.0f}},
            .metadata = floor,
        },
        {
            .id = "far_wall_fragment",
            .bounds = {{-1.0f, 0.0f, 4.0f}, {1.0f, 3.0f, 5.0f}},
            .metadata = farWall,
        },
    });

    const auto hits = partition.QueryBox({{-2.0f, -0.2f, -2.0f}, {2.0f, 0.2f, 2.0f}});
    if (hits.size() != 2) {
        return EXIT_FAILURE;
    }
    if (hits[0].entry == nullptr
        || hits[0].entry->metadataSignature
               != ri::scene::StructuralBrushMetadataSignature(hits[0].entry->metadata)) {
        return EXIT_FAILURE;
    }

    const auto floorHits = partition.QueryBox(
        {{-2.0f, -0.2f, -2.0f}, {2.0f, 0.2f, 2.0f}},
        {.role = ri::scene::StructuralBrushSemanticRole::Floor});
    if (floorHits.size() != 1
        || floorHits[0].entry == nullptr
        || floorHits[0].entry->metadata.brushId != "floor_a"
        || floorHits[0].entry->metadata.navigation != ri::scene::StructuralBrushNavigationPolicy::Walkable) {
        return EXIT_FAILURE;
    }

    const auto ownedWallHits = partition.QueryBox(
        {{-2.0f, -0.2f, -2.0f}, {2.0f, 3.2f, 5.2f}},
        {.brushId = "wall_b"});
    if (ownedWallHits.size() != 1
        || ownedWallHits[0].entry == nullptr
        || ownedWallHits[0].entry->id != "far_wall_fragment") {
        return EXIT_FAILURE;
    }

    const auto visualChannelHits = partition.QueryBox(
        {{-2.0f, -0.2f, -2.0f}, {2.0f, 3.2f, 5.2f}},
        {.channel = ri::scene::StructuralBrushChannel::VisualMesh});
    if (visualChannelHits.size() != 2
        || visualChannelHits[0].entry == nullptr
        || visualChannelHits[1].entry == nullptr) {
        return EXIT_FAILURE;
    }

    const auto physicsChannelHits = partition.QueryBox(
        {{-2.0f, -0.2f, -2.0f}, {2.0f, 3.2f, 5.2f}},
        {.channel = ri::scene::StructuralBrushChannel::PhysicsMesh});
    if (physicsChannelHits.size() != 2) {
        return EXIT_FAILURE;
    }

    const auto queryChannelHits = partition.QueryRay(
        {0.0f, 1.5f, -3.0f},
        {0.0f, 0.0f, 1.0f},
        10.0f,
        {.channel = ri::scene::StructuralBrushChannel::QueryMesh});
    if (queryChannelHits.size() != 1
        || queryChannelHits[0].entry == nullptr
        || queryChannelHits[0].entry->metadata.brushId != "wall_a") {
        return EXIT_FAILURE;
    }

    const auto tracePurposeHits = partition.QueryBox(
        {{-2.0f, -0.2f, -2.0f}, {2.0f, 0.2f, 2.0f}},
        {.queryPurpose = ri::scene::StructuralBrushQueryPurpose::Trace});
    if (tracePurposeHits.size() != 1
        || tracePurposeHits[0].entry == nullptr
        || tracePurposeHits[0].entry->metadata.brushId != "wall_a") {
        return EXIT_FAILURE;
    }

    const auto placementPurposeHits = partition.QueryBox(
        {{-2.0f, -0.2f, -2.0f}, {2.0f, 0.2f, 2.0f}},
        {.queryPurpose = ri::scene::StructuralBrushQueryPurpose::Placement});
    if (placementPurposeHits.size() != 1
        || placementPurposeHits[0].entry == nullptr
        || placementPurposeHits[0].entry->metadata.brushId != "floor_a") {
        return EXIT_FAILURE;
    }

    const auto informationChannelHits = partition.QueryBox(
        {{-2.0f, -0.2f, -2.0f}, {2.0f, 3.2f, 5.2f}},
        {.channel = ri::scene::StructuralBrushChannel::InformationLayer});
    if (informationChannelHits.size() != 3) {
        return EXIT_FAILURE;
    }

    const auto subtractiveHits = partition.QueryBox(
        {{-2.0f, -0.2f, -2.0f}, {2.0f, 3.2f, 5.2f}},
        {.operation = ri::scene::StructuralBrushOperation::Subtract});
    if (subtractiveHits.size() != 1
        || subtractiveHits[0].entry == nullptr
        || subtractiveHits[0].entry->metadata.brushId != "wall_b") {
        return EXIT_FAILURE;
    }

    const auto globalRebuildHits = partition.QueryRay(
        {0.0f, 1.5f, -3.0f},
        {0.0f, 0.0f, 1.0f},
        10.0f,
        {.rebuildScope = ri::scene::StructuralBrushRebuildScope::Global});
    if (globalRebuildHits.size() != 1
        || globalRebuildHits[0].entry == nullptr
        || globalRebuildHits[0].entry->metadata.brushId != "wall_b") {
        return EXIT_FAILURE;
    }

    const auto wallRayHits = partition.QueryRay(
        {0.0f, 1.5f, -3.0f},
        {0.0f, 0.0f, 1.0f},
        10.0f,
        {.visibility = ri::scene::StructuralBrushVisibilityPolicy::Occluder});
    if (wallRayHits.size() != 2
        || wallRayHits[0].entry == nullptr
        || wallRayHits[1].entry == nullptr
        || wallRayHits[0].entry->metadata.brushId != "wall_a"
        || wallRayHits[1].entry->metadata.brushId != "wall_b"
        || wallRayHits[0].distance > wallRayHits[1].distance) {
        return EXIT_FAILURE;
    }

    const auto nearestWallHit = partition.QueryNearestRay(
        {0.0f, 1.5f, -3.0f},
        {0.0f, 0.0f, 1.0f},
        10.0f,
        {.visibility = ri::scene::StructuralBrushVisibilityPolicy::Occluder});
    if (!nearestWallHit.has_value()
        || nearestWallHit->entry == nullptr
        || nearestWallHit->entry->metadata.brushId != "wall_a") {
        return EXIT_FAILURE;
    }

    const ri::scene::SemanticStructuralPartitionMetrics metrics = partition.Metrics();
    if (metrics.entryCount != 3
        || metrics.regionCount != 1
        || metrics.roleCounts.floor != 1
        || metrics.roleCounts.wall != 2
        || metrics.operationCounts.solid != 1
        || metrics.operationCounts.subtract != 1
        || metrics.operationCounts.stamp != 1
        || metrics.rebuildScopeCounts.region != 1
        || metrics.rebuildScopeCounts.global != 1
        || metrics.rebuildScopeCounts.manual != 1
        || metrics.channelCounts.visualMesh != 2
        || metrics.channelCounts.physicsMesh != 2
        || metrics.channelCounts.queryMesh != 2
        || metrics.channelCounts.informationLayer != 3
        || metrics.queryPurposeCounts.raycast != 2
        || metrics.queryPurposeCounts.trace != 1
        || metrics.queryPurposeCounts.placement != 1
        || metrics.queryPurposeCounts.interaction != 1
        || metrics.boxQueries != 9
        || metrics.rayQueries != 4
        || metrics.boxCandidatesScanned == 0
        || metrics.rayCandidatesScanned == 0) {
        return EXIT_FAILURE;
    }

    partition.ResetMetrics();
    const ri::scene::SemanticStructuralPartitionMetrics resetMetrics = partition.Metrics();
    if (resetMetrics.entryCount != 3
        || resetMetrics.regionCount != 1
        || resetMetrics.boxQueries != 0
        || resetMetrics.rayQueries != 0
        || resetMetrics.boxCandidatesScanned != 0
        || resetMetrics.rayCandidatesScanned != 0) {
        return EXIT_FAILURE;
    }

    ri::scene::Scene scene{"SemanticPartitionSceneFeed"};
    const int root = scene.CreateNode("Root");
    ri::scene::StructuralBrushSpawnOptions brush{};
    brush.nodeName = "SceneWall";
    brush.parent = root;
    brush.structuralType = "box";
    brush.transform.position = {3.0f, 1.5f, 0.0f};
    brush.transform.scale = {2.0f, 3.0f, 1.0f};
    brush.metadata.brushId = "scene_wall";
    brush.metadata.role = ri::scene::StructuralBrushSemanticRole::Wall;
    brush.metadata.region = "scene_region";
    const int sceneWall = ri::scene::AddStructuralBrushNode(scene, brush);
    if (sceneWall == ri::scene::kInvalidHandle) {
        return EXIT_FAILURE;
    }

    const std::vector<ri::scene::SemanticStructuralPartitionEntry> sceneEntries =
        ri::scene::BuildSemanticStructuralPartitionEntries(scene);
    if (sceneEntries.size() != 1
        || sceneEntries[0].id != "SceneWall"
        || sceneEntries[0].nodeHandle != sceneWall
        || sceneEntries[0].metadata.brushId != "scene_wall"
        || sceneEntries[0].metadata.role != ri::scene::StructuralBrushSemanticRole::Wall
        || sceneEntries[0].metadataSignature == 0
        || sceneEntries[0].metadataSignature
               != ri::scene::StructuralBrushMetadataSignature(sceneEntries[0].metadata)) {
        return EXIT_FAILURE;
    }

    ri::scene::SemanticStructuralPartition scenePartition;
    scenePartition.Rebuild(sceneEntries);
    const auto sceneHits = scenePartition.QueryBox(
        {{2.5f, 1.0f, -0.25f}, {3.5f, 2.0f, 0.25f}},
        {.region = "scene_region"});
    if (sceneHits.size() != 1 || sceneHits[0].entry->metadata.brushId != "scene_wall") {
        return EXIT_FAILURE;
    }

    const auto sceneRayHits = scenePartition.QueryRay(
        {3.0f, 1.5f, -3.0f},
        {0.0f, 0.0f, 1.0f},
        6.0f,
        {.region = "scene_region"});
    if (sceneRayHits.size() != 1
        || sceneRayHits[0].entry->metadata.brushId != "scene_wall"
        || sceneRayHits[0].entry->nodeHandle != sceneWall) {
        return EXIT_FAILURE;
    }

    ri::scene::SemanticStructuralPartition builtScenePartition =
        ri::scene::BuildSemanticStructuralPartition(scene);
    const auto builtNearest = builtScenePartition.QueryNearestRay(
        {3.0f, 1.5f, -3.0f},
        {0.0f, 0.0f, 1.0f},
        6.0f,
        {.brushId = "scene_wall"});
    if (!builtNearest.has_value()
        || builtNearest->entry == nullptr
        || builtNearest->entry->nodeHandle != sceneWall) {
        return EXIT_FAILURE;
    }

    const auto semanticPick = ri::scene::PickSemanticStructuralBrush(
        scene,
        {.origin = {3.0f, 1.5f, -3.0f}, .direction = {0.0f, 0.0f, 1.0f}},
        6.0f,
        {.brushId = "scene_wall"});
    if (!semanticPick.has_value()
        || semanticPick->entry.nodeHandle != sceneWall
        || semanticPick->entry.metadata.role != ri::scene::StructuralBrushSemanticRole::Wall) {
        return EXIT_FAILURE;
    }

    ri::scene::SemanticStructuralPartitionCache cache;
    const ri::scene::SemanticStructuralPartition& cachedInitial = cache.GetOrRebuild(scene);
    if (cache.RebuildCount() != 1
        || cache.ReuseCount() != 0
        || cache.IsDirty()
        || cache.NeedsRebuild(scene)
        || cachedInitial.Metrics().entryCount != 1) {
        return EXIT_FAILURE;
    }
    const ri::scene::SemanticStructuralPartition& cachedReused = cache.GetOrRebuild(scene);
    if (cache.RebuildCount() != 1
        || cache.ReuseCount() != 1
        || cachedReused.Metrics().entryCount != 1) {
        return EXIT_FAILURE;
    }
    const int nonStructuralNode = scene.CreateNode("EditorOnlyMarker");
    if (nonStructuralNode == ri::scene::kInvalidHandle) {
        return EXIT_FAILURE;
    }
    if (cache.NeedsRebuild(scene)) {
        return EXIT_FAILURE;
    }
    const ri::scene::SemanticStructuralPartition& nonStructuralReuse = cache.GetOrRebuild(scene);
    if (cache.RebuildCount() != 1
        || cache.ReuseCount() != 2
        || nonStructuralReuse.Metrics().entryCount != 1) {
        return EXIT_FAILURE;
    }

    brush.nodeName = "SceneFloor";
    brush.transform.position = {3.0f, 0.0f, 2.0f};
    brush.transform.scale = {3.0f, 0.2f, 3.0f};
    brush.metadata.brushId = "scene_floor";
    brush.metadata.role = ri::scene::StructuralBrushSemanticRole::Floor;
    brush.metadata.region = "scene_region";
    const int sceneFloor = ri::scene::AddStructuralBrushNode(scene, brush);
    if (sceneFloor == ri::scene::kInvalidHandle) {
        return EXIT_FAILURE;
    }
    if (!cache.NeedsRebuild(scene)) {
        return EXIT_FAILURE;
    }
    const ri::scene::SemanticStructuralPartition& autoRebuiltCache = cache.GetOrRebuild(scene);
    if (cache.RebuildCount() != 2
        || cache.ReuseCount() != 2
        || cache.IsDirty()
        || autoRebuiltCache.Metrics().entryCount != 2
        || autoRebuiltCache.QueryBox({{2.0f, -0.2f, 1.0f}, {4.0f, 0.2f, 3.0f}},
                                     {.role = ri::scene::StructuralBrushSemanticRole::Floor}).size() != 1) {
        return EXIT_FAILURE;
    }

    scene.GetNode(sceneWall).structuralBrush.region = "scene_region_b";
    if (!cache.NeedsRebuild(scene)) {
        return EXIT_FAILURE;
    }
    const ri::scene::SemanticStructuralPartition& retaggedCache = cache.GetOrRebuild(scene);
    if (cache.RebuildCount() != 3
        || cache.ReuseCount() != 2
        || cache.IsDirty()
        || retaggedCache.QueryBox({{2.5f, 1.0f, -0.25f}, {3.5f, 2.0f, 0.25f}},
                                  {.region = "scene_region_b"}).size() != 1) {
        return EXIT_FAILURE;
    }

    cache.Invalidate();
    if (!cache.NeedsRebuild(scene)) {
        return EXIT_FAILURE;
    }
    const ri::scene::SemanticStructuralPartition& rebuiltCache = cache.GetOrRebuild(scene);
    if (cache.RebuildCount() != 4
        || cache.ReuseCount() != 2
        || cache.IsDirty()
        || rebuiltCache.Metrics().entryCount != 2) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
