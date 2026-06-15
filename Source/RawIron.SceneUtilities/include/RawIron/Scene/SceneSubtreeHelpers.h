#pragma once

#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Scene/Scene.h"

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
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

/// Duplicates a node hierarchy for placement while reusing mesh and material handles from the source.
[[nodiscard]] int CloneSceneSubtree(Scene& scene,
                                    int sourceRoot,
                                    int parent,
                                    const std::string& rootName,
                                    const Transform& rootPlacement);

/// Hidden parent for imported model templates (off-world, not meant for direct rendering).
struct SceneModelTemplateRegistry {
    int templateParent = kInvalidHandle;
    std::unordered_map<std::string, int> templateRootsBySourceKey;
};

/// Imports a model once under \p registry.templateParent and caches the root for reuse.
[[nodiscard]] int EnsureSceneModelTemplate(Scene& scene,
                                             SceneModelTemplateRegistry& registry,
                                             const ImportedModelOptions& templateImportOptions,
                                             std::string* error = nullptr);

/// Clones a cached template into the live scene with placement and optional ground snap.
[[nodiscard]] int InstantiateSceneModelTemplate(Scene& scene,
                                              SceneModelTemplateRegistry& registry,
                                              const std::filesystem::path& sourcePath,
                                              int parent,
                                              const std::string& instanceName,
                                              const Transform& placement,
                                              float snapGroundY,
                                              const ImportedModelOptions& templateImportOptions,
                                              std::string* error = nullptr);

} // namespace ri::scene
