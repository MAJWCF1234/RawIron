#pragma once

#include "RawIron/Scene/Scene.h"
#include "RawIron/Spatial/Aabb.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::scene {

struct WorldBounds {
    ri::math::Vec3 min{0.0f, 0.0f, 0.0f};
    ri::math::Vec3 max{0.0f, 0.0f, 0.0f};
};

std::optional<int> FindNodeByName(const Scene& scene, std::string_view name);
std::optional<int> FindAncestorByName(const Scene& scene, int nodeHandle, std::string_view name);
std::vector<int> CollectRootNodes(const Scene& scene);
std::vector<int> CollectNodeSubtree(const Scene& scene, int nodeHandle, bool includeRoot = true);
std::vector<int> CollectDescendantNodes(const Scene& scene, int nodeHandle);
/// All \ref Node::mesh handles in the subtree of \p rootNode (optionally with duplicate mesh indices removed, then sorted).
[[nodiscard]] std::vector<int> CollectMeshHandlesInNodeSubtree(const Scene& scene, int rootNode, bool dedupeMeshHandles = true);
/// Node-attached meshes plus \ref MeshInstanceBatch entries parented under the subtree, deduped and sorted.
[[nodiscard]] std::vector<int> CollectMeshHandlesInNodeSubtreeAndBatches(const Scene& scene, int rootNode, bool dedupeMeshHandles = true);
std::vector<int> CollectRenderableNodes(const Scene& scene);
std::vector<int> CollectCameraNodes(const Scene& scene);
std::vector<int> CollectLightNodes(const Scene& scene);
std::vector<int> CollectCameraConfinementVolumeNodes(const Scene& scene);

/// `volumeNodeHandle` is the scene graph node carrying the volume (see `Node::cameraConfinementVolume`).
[[nodiscard]] bool IsWorldPointInsideCameraConfinementVolume(const Scene& scene,
                                                           int volumeNodeHandle,
                                                           const ri::math::Vec3& worldPoint);

/// Highest `CameraConfinementVolume::priority` among active volumes that contain `worldPoint`; ties break toward
/// lower node handle for determinism.
[[nodiscard]] std::optional<int> ResolveDominantCameraConfinementVolumeNodeAtWorldPoint(
    const Scene& scene,
    const ri::math::Vec3& worldPoint);
std::string DescribeNodePath(const Scene& scene, int nodeHandle);
std::optional<WorldBounds> ComputeNodeWorldBounds(const Scene& scene, int nodeHandle, bool includeChildren = true);
/// Tight world AABB for this node's attached mesh only (ignores child nodes). Empty if the node has no mesh / no bounds.
[[nodiscard]] std::optional<ri::spatial::Aabb> TryComputeMeshNodeWorldAabb(const Scene& scene, int nodeHandle);
std::optional<WorldBounds> ComputeCombinedWorldBounds(const Scene& scene,
                                                      const std::vector<int>& nodeHandles,
                                                      bool includeChildren = true);
ri::math::Vec3 GetBoundsCenter(const WorldBounds& bounds);
ri::math::Vec3 GetBoundsSize(const WorldBounds& bounds);

} // namespace ri::scene
