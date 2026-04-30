#include "RawIron/Scene/SceneUtils.h"

#include "RawIron/Math/Mat4.h"
#include "RawIron/Spatial/Aabb.h"

#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_set>

namespace ri::scene {

namespace {

bool IsValidNodeHandle(const Scene& scene, int nodeHandle) {
    return nodeHandle >= 0 && static_cast<std::size_t>(nodeHandle) < scene.NodeCount();
}

void ExpandBounds(WorldBounds& bounds, bool& hasBounds, const ri::math::Vec3& point) {
    if (!hasBounds) {
        bounds.min = point;
        bounds.max = point;
        hasBounds = true;
        return;
    }

    bounds.min.x = std::min(bounds.min.x, point.x);
    bounds.min.y = std::min(bounds.min.y, point.y);
    bounds.min.z = std::min(bounds.min.z, point.z);
    bounds.max.x = std::max(bounds.max.x, point.x);
    bounds.max.y = std::max(bounds.max.y, point.y);
    bounds.max.z = std::max(bounds.max.z, point.z);
}

std::optional<std::vector<ri::math::Vec3>> GetPrimitiveCorners(const Mesh& mesh) {
    switch (mesh.primitive) {
        case PrimitiveType::Cube:
        case PrimitiveType::Sphere:
            return std::vector<ri::math::Vec3>{
                {-0.5f, -0.5f, -0.5f},
                {0.5f, -0.5f, -0.5f},
                {-0.5f, 0.5f, -0.5f},
                {0.5f, 0.5f, -0.5f},
                {-0.5f, -0.5f, 0.5f},
                {0.5f, -0.5f, 0.5f},
                {-0.5f, 0.5f, 0.5f},
                {0.5f, 0.5f, 0.5f},
            };
        case PrimitiveType::Plane:
            return std::vector<ri::math::Vec3>{
                {-0.5f, 0.0f, -0.5f},
                {0.5f, 0.0f, -0.5f},
                {-0.5f, 0.0f, 0.5f},
                {0.5f, 0.0f, 0.5f},
            };
        case PrimitiveType::Custom:
            if (mesh.positions.empty()) {
                return std::nullopt;
            }
            return mesh.positions;
    }

    return std::nullopt;
}

void AppendNodeWorldBounds(const Scene& scene,
                           int nodeHandle,
                           bool includeChildren,
                           WorldBounds& bounds,
                           bool& hasBounds) {
    if (nodeHandle < 0 || static_cast<std::size_t>(nodeHandle) >= scene.NodeCount()) {
        return;
    }

    const Node& node = scene.GetNode(nodeHandle);
    if (node.mesh != kInvalidHandle) {
        const Mesh& mesh = scene.GetMesh(node.mesh);
        if (const std::optional<std::vector<ri::math::Vec3>> corners = GetPrimitiveCorners(mesh); corners.has_value()) {
            const ri::math::Mat4 world = scene.ComputeWorldMatrix(nodeHandle);
            for (const ri::math::Vec3& corner : *corners) {
                ExpandBounds(bounds, hasBounds, ri::math::TransformPoint(world, corner));
            }
        }
    }

    if (!includeChildren) {
        return;
    }

    for (const int child : node.children) {
        AppendNodeWorldBounds(scene, child, true, bounds, hasBounds);
    }
}

void AppendSubtreeNodes(const Scene& scene, int nodeHandle, std::vector<int>& out) {
    if (!IsValidNodeHandle(scene, nodeHandle)) {
        return;
    }

    out.push_back(nodeHandle);
    const Node& node = scene.GetNode(nodeHandle);
    for (const int child : node.children) {
        AppendSubtreeNodes(scene, child, out);
    }
}

bool TryTransformWorldPointToVolumeLocal(const Scene& scene,
                                         int volumeNodeHandle,
                                         const ri::math::Vec3& worldPoint,
                                         ri::math::Vec3* outLocal) {
    const ri::math::Mat4 worldFromLocal = scene.ComputeWorldMatrix(volumeNodeHandle);
    ri::math::Mat4 localFromWorld{};
    if (!ri::math::TryInvertAffineMat4(worldFromLocal, localFromWorld)) {
        return false;
    }
    *outLocal = ri::math::TransformPoint(localFromWorld, worldPoint);
    return true;
}

bool LocalPointInsideHalfExtents(const ri::math::Vec3& local, const ri::math::Vec3& halfExtents) {
    const float hx = std::max(halfExtents.x, 1.0e-6f);
    const float hy = std::max(halfExtents.y, 1.0e-6f);
    const float hz = std::max(halfExtents.z, 1.0e-6f);
    return std::abs(local.x) <= hx && std::abs(local.y) <= hy && std::abs(local.z) <= hz;
}

} // namespace

std::optional<int> FindNodeByName(const Scene& scene, std::string_view name) {
    const std::vector<Node>& nodes = scene.Nodes();
    for (int index = 0; index < static_cast<int>(nodes.size()); ++index) {
        if (nodes[static_cast<std::size_t>(index)].name == name) {
            return index;
        }
    }

    return std::nullopt;
}

std::optional<int> FindAncestorByName(const Scene& scene, int nodeHandle, std::string_view name) {
    if (!IsValidNodeHandle(scene, nodeHandle)) {
        return std::nullopt;
    }

    int current = scene.GetNode(nodeHandle).parent;
    while (current != kInvalidHandle) {
        const Node& node = scene.GetNode(current);
        if (node.name == name) {
            return current;
        }
        current = node.parent;
    }

    return std::nullopt;
}

std::vector<int> CollectRootNodes(const Scene& scene) {
    std::vector<int> handles;
    const std::vector<Node>& nodes = scene.Nodes();
    for (int index = 0; index < static_cast<int>(nodes.size()); ++index) {
        if (nodes[static_cast<std::size_t>(index)].parent == kInvalidHandle) {
            handles.push_back(index);
        }
    }
    return handles;
}

std::vector<int> CollectNodeSubtree(const Scene& scene, int nodeHandle, bool includeRoot) {
    std::vector<int> handles;
    if (!IsValidNodeHandle(scene, nodeHandle)) {
        return handles;
    }

    if (includeRoot) {
        AppendSubtreeNodes(scene, nodeHandle, handles);
        return handles;
    }

    const Node& node = scene.GetNode(nodeHandle);
    for (const int child : node.children) {
        AppendSubtreeNodes(scene, child, handles);
    }
    return handles;
}

std::vector<int> CollectDescendantNodes(const Scene& scene, int nodeHandle) {
    return CollectNodeSubtree(scene, nodeHandle, false);
}

std::vector<int> CollectMeshHandlesInNodeSubtree(const Scene& scene, const int rootNode, const bool dedupeMeshHandles) {
    std::vector<int> ordered;
    if (!IsValidNodeHandle(scene, rootNode)) {
        return ordered;
    }
    std::unordered_set<int> seen;
    const std::vector<int> subtree = CollectNodeSubtree(scene, rootNode, true);
    ordered.reserve(subtree.size());
    for (const int nodeHandle : subtree) {
        const int meshHandle = scene.GetNode(nodeHandle).mesh;
        if (meshHandle == kInvalidHandle) {
            continue;
        }
        if (dedupeMeshHandles) {
            if (!seen.insert(meshHandle).second) {
                continue;
            }
        }
        ordered.push_back(meshHandle);
    }
    if (dedupeMeshHandles) {
        std::sort(ordered.begin(), ordered.end());
    }
    return ordered;
}

std::vector<int> CollectMeshHandlesInNodeSubtreeAndBatches(const Scene& scene, const int rootNode, const bool dedupeMeshHandles) {
    std::vector<int> ordered;
    if (!IsValidNodeHandle(scene, rootNode)) {
        return ordered;
    }
    std::unordered_set<int> seen;
    const std::vector<int> subtree = CollectNodeSubtree(scene, rootNode, true);
    std::unordered_set<int> subtreeNodes(subtree.begin(), subtree.end());
    ordered.reserve(subtree.size());
    for (const int nodeHandle : subtree) {
        const int meshHandle = scene.GetNode(nodeHandle).mesh;
        if (meshHandle == kInvalidHandle) {
            continue;
        }
        if (dedupeMeshHandles) {
            if (!seen.insert(meshHandle).second) {
                continue;
            }
        }
        ordered.push_back(meshHandle);
    }
    for (std::size_t batchIndex = 0; batchIndex < scene.MeshInstanceBatchCount(); ++batchIndex) {
        const MeshInstanceBatch& batch = scene.GetMeshInstanceBatch(static_cast<int>(batchIndex));
        if (batch.parent == kInvalidHandle || !subtreeNodes.contains(batch.parent) || batch.mesh == kInvalidHandle) {
            continue;
        }
        if (dedupeMeshHandles) {
            if (!seen.insert(batch.mesh).second) {
                continue;
            }
        }
        ordered.push_back(batch.mesh);
    }
    if (dedupeMeshHandles) {
        std::sort(ordered.begin(), ordered.end());
    }
    return ordered;
}

std::vector<int> CollectRenderableNodes(const Scene& scene) {
    return scene.GetRenderableNodeHandles();
}

std::vector<int> CollectCameraNodes(const Scene& scene) {
    std::vector<int> handles;
    const std::vector<Node>& nodes = scene.Nodes();
    for (int index = 0; index < static_cast<int>(nodes.size()); ++index) {
        if (nodes[static_cast<std::size_t>(index)].camera != kInvalidHandle) {
            handles.push_back(index);
        }
    }
    return handles;
}

std::vector<int> CollectLightNodes(const Scene& scene) {
    std::vector<int> handles;
    const std::vector<Node>& nodes = scene.Nodes();
    for (int index = 0; index < static_cast<int>(nodes.size()); ++index) {
        if (nodes[static_cast<std::size_t>(index)].light != kInvalidHandle) {
            handles.push_back(index);
        }
    }
    return handles;
}

std::vector<int> CollectCameraConfinementVolumeNodes(const Scene& scene) {
    std::vector<int> handles;
    const std::vector<Node>& nodes = scene.Nodes();
    for (int index = 0; index < static_cast<int>(nodes.size()); ++index) {
        if (nodes[static_cast<std::size_t>(index)].cameraConfinementVolume != kInvalidHandle) {
            handles.push_back(index);
        }
    }
    return handles;
}

bool IsWorldPointInsideCameraConfinementVolume(const Scene& scene,
                                               const int volumeNodeHandle,
                                               const ri::math::Vec3& worldPoint) {
    if (!IsValidNodeHandle(scene, volumeNodeHandle)) {
        return false;
    }
    const Node& node = scene.GetNode(volumeNodeHandle);
    if (node.cameraConfinementVolume == kInvalidHandle) {
        return false;
    }
    const CameraConfinementVolume& volume = scene.GetCameraConfinementVolume(node.cameraConfinementVolume);
    if (!volume.active) {
        return false;
    }
    ri::math::Vec3 local{};
    if (!TryTransformWorldPointToVolumeLocal(scene, volumeNodeHandle, worldPoint, &local)) {
        return false;
    }
    return LocalPointInsideHalfExtents(local, volume.halfExtents);
}

std::optional<int> ResolveDominantCameraConfinementVolumeNodeAtWorldPoint(const Scene& scene,
                                                                          const ri::math::Vec3& worldPoint) {
    std::optional<int> bestNode;
    int bestPriority = std::numeric_limits<int>::min();
    for (const int nodeHandle : CollectCameraConfinementVolumeNodes(scene)) {
        const CameraConfinementVolume& volume =
            scene.GetCameraConfinementVolume(scene.GetNode(nodeHandle).cameraConfinementVolume);
        if (!volume.active) {
            continue;
        }
        if (!IsWorldPointInsideCameraConfinementVolume(scene, nodeHandle, worldPoint)) {
            continue;
        }
        if (!bestNode.has_value() || volume.priority > bestPriority
            || (volume.priority == bestPriority && nodeHandle < *bestNode)) {
            bestNode = nodeHandle;
            bestPriority = volume.priority;
        }
    }
    return bestNode;
}

std::string DescribeNodePath(const Scene& scene, int nodeHandle) {
    std::vector<std::string> names;
    int current = nodeHandle;
    while (current != kInvalidHandle) {
        names.push_back(scene.GetNode(current).name);
        current = scene.GetNode(current).parent;
    }

    std::string path;
    for (auto it = names.rbegin(); it != names.rend(); ++it) {
        if (!path.empty()) {
            path += '/';
        }
        path += *it;
    }
    return path;
}

std::optional<WorldBounds> ComputeNodeWorldBounds(const Scene& scene, int nodeHandle, bool includeChildren) {
    WorldBounds bounds{};
    bool hasBounds = false;
    AppendNodeWorldBounds(scene, nodeHandle, includeChildren, bounds, hasBounds);
    if (!hasBounds) {
        return std::nullopt;
    }
    return bounds;
}

std::optional<ri::spatial::Aabb> TryComputeMeshNodeWorldAabb(const Scene& scene, const int nodeHandle) {
    if (const std::optional<WorldBounds> bounds = ComputeNodeWorldBounds(scene, nodeHandle, false)) {
        return ri::spatial::Aabb{.min = bounds->min, .max = bounds->max};
    }
    return std::nullopt;
}

std::optional<WorldBounds> ComputeCombinedWorldBounds(const Scene& scene,
                                                      const std::vector<int>& nodeHandles,
                                                      bool includeChildren) {
    WorldBounds bounds{};
    bool hasBounds = false;
    for (const int nodeHandle : nodeHandles) {
        AppendNodeWorldBounds(scene, nodeHandle, includeChildren, bounds, hasBounds);
    }
    if (!hasBounds) {
        return std::nullopt;
    }
    return bounds;
}

ri::math::Vec3 GetBoundsCenter(const WorldBounds& bounds) {
    return (bounds.min + bounds.max) * 0.5f;
}

ri::math::Vec3 GetBoundsSize(const WorldBounds& bounds) {
    return bounds.max - bounds.min;
}

} // namespace ri::scene
