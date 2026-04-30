#include "RawIron/Scene/SceneSubtreeHelpers.h"

#include "RawIron/Scene/SceneUtils.h"

#include <algorithm>
#include <unordered_set>

namespace ri::scene {
namespace {

void RemoveAll(std::vector<int>& values, int removeHandle) {
    values.erase(std::remove(values.begin(), values.end(), removeHandle), values.end());
}

void MergeHiddenNodes(std::vector<int>& hiddenNodeHandles, const std::vector<int>& subtree, bool hideSubtree) {
    if (hideSubtree) {
        std::unordered_set<int> seen(hiddenNodeHandles.begin(), hiddenNodeHandles.end());
        for (const int handle : subtree) {
            if (seen.insert(handle).second) {
                hiddenNodeHandles.push_back(handle);
            }
        }
        return;
    }
    for (const int handle : subtree) {
        RemoveAll(hiddenNodeHandles, handle);
    }
}

void MergeColliderSuppression(std::unordered_set<std::string>& suppressedColliderIds,
                              const Scene& scene,
                              const std::vector<int>& subtree,
                              const bool suppress) {
    for (const int handle : subtree) {
        const std::string& name = scene.GetNode(handle).name;
        if (name.empty()) {
            continue;
        }
        if (suppress) {
            suppressedColliderIds.insert(name);
        } else {
            suppressedColliderIds.erase(name);
        }
    }
}

void ApplyEmissiveToMaterial(Material& material,
                             const float emissiveScale,
                             const ri::math::Vec3& emissiveTint) {
    material.emissiveColor.x = std::max(0.0f, material.emissiveColor.x * emissiveScale * emissiveTint.x);
    material.emissiveColor.y = std::max(0.0f, material.emissiveColor.y * emissiveScale * emissiveTint.y);
    material.emissiveColor.z = std::max(0.0f, material.emissiveColor.z * emissiveScale * emissiveTint.z);
}

} // namespace

void ToggleSceneSubtreeVisibilityAndCollision(std::vector<int>& hiddenNodeHandles,
                                            std::unordered_set<std::string>& suppressedColliderIds,
                                            const Scene& scene,
                                            const int rootNode,
                                            const bool hideSubtree,
                                            const SceneSubtreeToggleOptions& options) {
    const std::vector<int> subtree = CollectNodeSubtree(scene, rootNode, true);
    if (options.updateHiddenRenderNodes) {
        MergeHiddenNodes(hiddenNodeHandles, subtree, hideSubtree);
    }
    if (options.suppressCollisionByNodeName) {
        const std::size_t before = suppressedColliderIds.size();
        MergeColliderSuppression(suppressedColliderIds, scene, subtree, hideSubtree);
        if (options.onCollisionRegistryChanged && suppressedColliderIds.size() != before) {
            options.onCollisionRegistryChanged();
        }
    }
}

void ApplySubtreeEmissiveIntensity(Scene& scene,
                                   const int rootNode,
                                   const float emissiveScale,
                                   const ri::math::Vec3& emissiveTint) {
    if (rootNode < 0 || static_cast<std::size_t>(rootNode) >= scene.NodeCount()) {
        return;
    }
    const std::vector<int> subtree = CollectNodeSubtree(scene, rootNode, true);
    std::unordered_set<int> touchedMaterials;

    for (const int nodeHandle : subtree) {
        Node& node = scene.GetNode(nodeHandle);
        if (node.material != kInvalidHandle
            && touchedMaterials.insert(node.material).second) {
            ApplyEmissiveToMaterial(scene.GetMaterial(node.material), emissiveScale, emissiveTint);
        }
    }

    for (std::size_t batchIndex = 0; batchIndex < scene.MeshInstanceBatchCount(); ++batchIndex) {
        MeshInstanceBatch& batch = scene.GetMeshInstanceBatch(static_cast<int>(batchIndex));
        if (batch.parent == kInvalidHandle) {
            continue;
        }
        const auto inSubtree = std::find(subtree.begin(), subtree.end(), batch.parent);
        if (inSubtree == subtree.end()) {
            continue;
        }
        if (batch.material != kInvalidHandle && touchedMaterials.insert(batch.material).second) {
            ApplyEmissiveToMaterial(scene.GetMaterial(batch.material), emissiveScale, emissiveTint);
        }
    }
}

} // namespace ri::scene
