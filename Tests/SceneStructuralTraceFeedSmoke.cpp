#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/SceneStructuralTraceFeed.h"
#include "RawIron/Scene/StructuralBrush.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace {

[[nodiscard]] bool ContainsTag(const ri::trace::TraceCollider& collider, const std::string& tag) {
    for (const std::string& candidate : collider.simulationTags) {
        if (candidate == tag) {
            return true;
        }
    }
    return false;
}

} // namespace

int main() {
    ri::scene::Scene scene{"SceneStructuralTraceFeedSmoke"};
    const int root = scene.CreateNode("Root");

    auto addBrush = [&](const char* name,
                        const ri::math::Vec3 position,
                        const ri::scene::StructuralBrushCollisionPolicy collision) {
        ri::scene::StructuralBrushSpawnOptions brush{};
        brush.nodeName = name;
        brush.parent = root;
        brush.structuralType = "box";
        brush.transform.position = position;
        brush.transform.scale = {1.0f, 1.0f, 1.0f};
        brush.metadata.brushId = name;
        brush.metadata.collision = collision;
        return ri::scene::AddStructuralBrushNode(scene, brush);
    };

    const int traceable = addBrush("TraceableWall",
                                   {0.0f, 0.0f, 0.0f},
                                   ri::scene::StructuralBrushCollisionPolicy::Solid);
    const int placementOnly = addBrush("PlacementOnlyWall",
                                       {2.0f, 0.0f, 0.0f},
                                       ri::scene::StructuralBrushCollisionPolicy::Solid);
    const int queryCollision = addBrush("QueryCollisionWall",
                                        {4.0f, 0.0f, 0.0f},
                                        ri::scene::StructuralBrushCollisionPolicy::Query);
    if (traceable == ri::scene::kInvalidHandle
        || placementOnly == ri::scene::kInvalidHandle
        || queryCollision == ri::scene::kInvalidHandle) {
        return EXIT_FAILURE;
    }

    ri::scene::StructuralBrushMetadata& placementMetadata = scene.GetNode(placementOnly).structuralBrush;
    placementMetadata.queryMesh.raycastable = false;
    placementMetadata.queryMesh.traceable = false;
    placementMetadata.queryMesh.placeable = true;
    placementMetadata.queryMesh.interactable = false;

    const std::vector<ri::trace::TraceCollider> colliders =
        ri::scene::BuildStructuralTraceCollidersForSubtree(scene, root);
    if (colliders.size() != 1
        || colliders[0].id != "TraceableWall"
        || !colliders[0].structural
        || colliders[0].dynamic
        || !ContainsTag(colliders[0], "structural.brush:TraceableWall")
        || !ContainsTag(colliders[0], "structural.query_purpose:trace")) {
        return EXIT_FAILURE;
    }

    ri::trace::TraceScene traceScene = ri::scene::BuildStructuralTraceSceneForSubtree(scene, root);
    const ri::trace::TraceSceneMetrics metrics = traceScene.Metrics();
    if (metrics.colliderCount != 1
        || metrics.staticColliderCount != 1
        || metrics.structuralStaticColliderCount != 1
        || metrics.dynamicColliderCount != 0) {
        return EXIT_FAILURE;
    }

    ri::trace::TraceOptions traceOptions{};
    traceOptions.structuralOnly = true;
    const std::optional<ri::trace::TraceHit> hit = traceScene.TraceRay(
        {0.0f, 0.0f, -3.0f},
        {0.0f, 0.0f, 1.0f},
        10.0f,
        traceOptions);
    if (!hit.has_value() || hit->id != "TraceableWall") {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
