#pragma once

#include "RawIron/Math/Vec3.h"
#include "RawIron/Scene/Scene.h"

#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace ri::scene {

/// Controls \ref ToggleSceneSubtreeVisibilityAndCollision : preview hidden list + optional trace collider suppression by node name.
struct SceneSubtreeToggleOptions {
    bool updateHiddenRenderNodes = true;
    /// When true, adds/removes \ref Node::name entries from \p suppressedColliderIds (common convention: collider id == node name).
    bool suppressCollisionByNodeName = true;
    /// Invoked after collision suppression set mutates (caller rebuilds \ref ri::trace::TraceScene here).
    std::function<void()> onCollisionRegistryChanged{};
};

/// Single entry point: show or hide an entire subtree for software preview (\p hiddenNodeHandles) and optionally suppress
/// matching static collider ids so the next \ref ri::trace::TraceScene::SetColliders rebuild drops them from broadphase.
void ToggleSceneSubtreeVisibilityAndCollision(std::vector<int>& hiddenNodeHandles,
                                              std::unordered_set<std::string>& suppressedColliderIds,
                                              const Scene& scene,
                                              int rootNode,
                                              bool hideSubtree,
                                              const SceneSubtreeToggleOptions& options = {});

/// Scale emissive color on every material referenced by mesh instances in the subtree (nodes + mesh instance batches).
void ApplySubtreeEmissiveIntensity(Scene& scene,
                                   int rootNode,
                                   float emissiveScale,
                                   const ri::math::Vec3& emissiveTint = ri::math::Vec3{1.0f, 1.0f, 1.0f});

} // namespace ri::scene
