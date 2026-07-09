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

int CloneSceneSubtree(Scene& scene,
                      const int sourceRoot,
                      const int parent,
                      const std::string& rootName,
                      const Transform& rootPlacement) {
    if (sourceRoot == kInvalidHandle) {
        return kInvalidHandle;
    }

    // CreateNode may reallocate Scene::nodes_, so never hold Node&/children iterators
    // across CreateNode. Copy child handles first, then create the duplicate.
    struct PendingClone {
        int srcHandle = kInvalidHandle;
        int newParent = kInvalidHandle;
    };
    std::vector<PendingClone> stack;
    stack.push_back(PendingClone{.srcHandle = sourceRoot, .newParent = parent});

    int clonedRoot = kInvalidHandle;
    while (!stack.empty()) {
        const PendingClone job = stack.back();
        stack.pop_back();

        const Node& src = scene.GetNode(job.srcHandle);
        const std::string nodeName = job.srcHandle == sourceRoot ? rootName : src.name;
        const Transform localTransform =
            job.srcHandle == sourceRoot ? rootPlacement : src.localTransform;
        const int mesh = src.mesh;
        const int material = src.material;
        const std::vector<int> children = src.children;

        const int dup = scene.CreateNode(nodeName, job.newParent);
        scene.GetNode(dup).localTransform = localTransform;
        if (mesh != kInvalidHandle) {
            scene.AttachMesh(dup, mesh, material);
        }
        if (job.srcHandle == sourceRoot) {
            clonedRoot = dup;
        }

        // Push children in reverse so the first child is processed next (stable DFS order).
        for (std::size_t i = children.size(); i > 0; --i) {
            stack.push_back(PendingClone{.srcHandle = children[i - 1], .newParent = dup});
        }
    }

    return clonedRoot;
}

int EnsureSceneModelTemplate(Scene& scene,
                             SceneModelTemplateRegistry& registry,
                             const ImportedModelOptions& templateImportOptions,
                             std::string* error) {
    if (templateImportOptions.sourcePath.empty()) {
        if (error != nullptr) {
            *error = "Scene model template import requires a source path.";
        }
        return kInvalidHandle;
    }

    const std::string sourceKey = templateImportOptions.sourcePath.lexically_normal().generic_string();
    const auto cached = registry.templateRootsBySourceKey.find(sourceKey);
    if (cached != registry.templateRootsBySourceKey.end()) {
        if (error != nullptr) {
            error->clear();
        }
        return cached->second;
    }

    if (registry.templateParent == kInvalidHandle) {
        registry.templateParent = scene.CreateNode("SceneModelTemplates", kInvalidHandle);
        scene.GetNode(registry.templateParent).localTransform.position = ri::math::Vec3{0.0f, -2000.0f, 0.0f};
    }

    ImportedModelOptions importOptions = templateImportOptions;
    importOptions.parent = registry.templateParent;
    importOptions.snapMeshBaseToGround = false;
    std::string importError;
    const int templateRoot = AddModelNode(scene, importOptions, &importError);
    if (templateRoot == kInvalidHandle) {
        if (error != nullptr) {
            *error = importError;
        }
        return kInvalidHandle;
    }

    registry.templateRootsBySourceKey.emplace(sourceKey, templateRoot);
    if (error != nullptr) {
        error->clear();
    }
    return templateRoot;
}

int InstantiateSceneModelTemplate(Scene& scene,
                                  SceneModelTemplateRegistry& registry,
                                  const std::filesystem::path& sourcePath,
                                  const int parent,
                                  const std::string& instanceName,
                                  const Transform& placement,
                                  const float snapGroundY,
                                  const ImportedModelOptions& templateImportOptions,
                                  std::string* error) {
    ImportedModelOptions importOptions = templateImportOptions;
    importOptions.sourcePath = sourcePath;
    if (importOptions.nodeName.empty()) {
        importOptions.nodeName = "Template_" + sourcePath.stem().string();
    }

    const int templateRoot = EnsureSceneModelTemplate(scene, registry, importOptions, error);
    if (templateRoot == kInvalidHandle) {
        return kInvalidHandle;
    }

    const int instanceRoot = CloneSceneSubtree(scene, templateRoot, parent, instanceName, placement);
    if (instanceRoot == kInvalidHandle) {
        if (error != nullptr) {
            *error = "Failed to clone scene model template for " + sourcePath.string();
        }
        return kInvalidHandle;
    }

    SnapNodeMeshBaseToGround(scene, instanceRoot, snapGroundY);
    if (error != nullptr) {
        error->clear();
    }
    return instanceRoot;
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
