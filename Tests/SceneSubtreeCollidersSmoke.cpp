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

    std::vector<ri::trace::TraceCollider> colliders;
    const std::size_t added = ri::scene::AppendTraceCollidersForSubtree(
        scene,
        root,
        {
            .respectStructuralBrushCollisionPolicy = true,
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

    return EXIT_SUCCESS;
}
