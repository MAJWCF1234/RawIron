#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/SceneSubtreeColliders.h"
#include "RawIron/Scene/StructuralBrush.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace {

[[nodiscard]] bool ContainsColliderId(const std::vector<ri::trace::TraceCollider>& colliders,
                                      const std::string& id) {
    for (const ri::trace::TraceCollider& collider : colliders) {
        if (collider.id == id) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] const ri::trace::TraceCollider* FindCollider(const std::vector<ri::trace::TraceCollider>& colliders,
                                                           const std::string& id) {
    for (const ri::trace::TraceCollider& collider : colliders) {
        if (collider.id == id) {
            return &collider;
        }
    }
    return nullptr;
}

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
    ri::scene::Scene scene{"SceneSubtreeCollidersSmoke"};
    const int root = scene.CreateNode("Root");

    auto addBrush = [&](const char* name, const ri::math::Vec3 position,
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

    const int solid = addBrush("SolidBrush", {0.0f, 0.0f, 0.0f}, ri::scene::StructuralBrushCollisionPolicy::Solid);
    const int player = addBrush("PlayerBrush", {2.0f, 0.0f, 0.0f}, ri::scene::StructuralBrushCollisionPolicy::Player);
    const int query = addBrush("QueryOnlyBrush", {4.0f, 0.0f, 0.0f}, ri::scene::StructuralBrushCollisionPolicy::Query);
    const int none = addBrush("NoCollisionBrush", {6.0f, 0.0f, 0.0f}, ri::scene::StructuralBrushCollisionPolicy::None);
    const int detail = addBrush("DetailBrush", {8.0f, 0.0f, 0.0f}, ri::scene::StructuralBrushCollisionPolicy::Detail);
    if (solid == ri::scene::kInvalidHandle
        || player == ri::scene::kInvalidHandle
        || query == ri::scene::kInvalidHandle
        || none == ri::scene::kInvalidHandle
        || detail == ri::scene::kInvalidHandle) {
        return EXIT_FAILURE;
    }
    ri::scene::StructuralBrushMetadata& playerMetadata = scene.GetNode(player).structuralBrush;
    playerMetadata.region = "test_region";
    playerMetadata.operation = ri::scene::StructuralBrushOperation::Solid;
    playerMetadata.role = ri::scene::StructuralBrushSemanticRole::Wall;
    playerMetadata.visibility = ri::scene::StructuralBrushVisibilityPolicy::Occluder;
    playerMetadata.navigation = ri::scene::StructuralBrushNavigationPolicy::Blocker;
    playerMetadata.rebuildScope = ri::scene::StructuralBrushRebuildScope::Region;

    std::vector<ri::trace::TraceCollider> colliders;
    const std::size_t added = ri::scene::AppendTraceCollidersForSubtree(
        scene,
        root,
        {
            .respectStructuralBrushCollisionPolicy = true,
            .appendStructuralBrushSemanticTags = true,
        },
        colliders);

    if (added != 2
        || colliders.size() != 2
        || !ContainsColliderId(colliders, "SolidBrush")
        || !ContainsColliderId(colliders, "PlayerBrush")
        || ContainsColliderId(colliders, "QueryOnlyBrush")
        || ContainsColliderId(colliders, "NoCollisionBrush")
        || ContainsColliderId(colliders, "DetailBrush")) {
        return EXIT_FAILURE;
    }

    const ri::trace::TraceCollider* playerCollider = FindCollider(colliders, "PlayerBrush");
    if (playerCollider == nullptr
        || !ContainsTag(*playerCollider, "structural.brush:PlayerBrush")
        || !ContainsTag(*playerCollider, "structural.region:test_region")
        || !ContainsTag(*playerCollider, "structural.operation:solid")
        || !ContainsTag(*playerCollider, "structural.role:wall")
        || !ContainsTag(*playerCollider, "structural.collision:player")
        || !ContainsTag(*playerCollider, "structural.visibility:occluder")
        || !ContainsTag(*playerCollider, "structural.navigation:blocker")
        || !ContainsTag(*playerCollider, "structural.rebuild:region")) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
