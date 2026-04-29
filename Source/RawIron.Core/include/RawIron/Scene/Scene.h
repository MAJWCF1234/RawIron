#pragma once

#include "RawIron/Math/Mat4.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Scene/Components.h"
#include "RawIron/Scene/Transform.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace ri::scene {

inline constexpr int kInvalidHandle = -1;

struct Node {
    std::string name;
    Transform localTransform{};
    int parent = kInvalidHandle;
    std::vector<int> children;
    int mesh = kInvalidHandle;
    int material = kInvalidHandle;
    int camera = kInvalidHandle;
    int light = kInvalidHandle;
    /// Camera confinement / cinematic staging volume attached to this node (axis-aligned box in local space).
    int cameraConfinementVolume = kInvalidHandle;
};

struct MeshInstanceBatch {
    std::string name;
    int parent = kInvalidHandle;
    int mesh = kInvalidHandle;
    int material = kInvalidHandle;
    std::vector<Transform> transforms;
};

class Scene {
public:
    Scene();
    explicit Scene(std::string name);

    [[nodiscard]] const std::string& GetName() const noexcept;
    void SetName(std::string name);

    [[nodiscard]] int CreateNode(std::string name, int parent = kInvalidHandle);
    [[nodiscard]] bool SetParent(int child, int parent);

    [[nodiscard]] Node& GetNode(int handle);
    [[nodiscard]] const Node& GetNode(int handle) const;
    [[nodiscard]] const std::vector<Node>& Nodes() const noexcept;
    [[nodiscard]] std::size_t NodeCount() const noexcept;

    [[nodiscard]] int AddMaterial(Material material);
    [[nodiscard]] int AddMesh(Mesh mesh);
    [[nodiscard]] int AddCamera(Camera camera);
    [[nodiscard]] int AddLight(Light light);
    [[nodiscard]] int AddCameraConfinementVolume(CameraConfinementVolume volume);
    [[nodiscard]] int AddMeshInstanceBatch(MeshInstanceBatch batch);
    void AddMeshInstance(int batchHandle, const Transform& transform);

    void AttachMesh(int nodeHandle, int meshHandle, int materialHandle = kInvalidHandle);
    void AttachCamera(int nodeHandle, int cameraHandle);
    void AttachLight(int nodeHandle, int lightHandle);
    void AttachCameraConfinementVolume(int nodeHandle, int volumeHandle);

    [[nodiscard]] Material& GetMaterial(int handle);
    [[nodiscard]] Mesh& GetMesh(int handle);
    [[nodiscard]] Camera& GetCamera(int handle);
    [[nodiscard]] Light& GetLight(int handle);
    [[nodiscard]] CameraConfinementVolume& GetCameraConfinementVolume(int handle);
    [[nodiscard]] MeshInstanceBatch& GetMeshInstanceBatch(int handle);
    [[nodiscard]] const Material& GetMaterial(int handle) const;
    [[nodiscard]] const Mesh& GetMesh(int handle) const;
    [[nodiscard]] const Camera& GetCamera(int handle) const;
    [[nodiscard]] const Light& GetLight(int handle) const;
    [[nodiscard]] const CameraConfinementVolume& GetCameraConfinementVolume(int handle) const;
    [[nodiscard]] const MeshInstanceBatch& GetMeshInstanceBatch(int handle) const;

    [[nodiscard]] ri::math::Mat4 ComputeWorldMatrix(int nodeHandle) const;
    [[nodiscard]] ri::math::Vec3 ComputeWorldPosition(int nodeHandle) const;
    [[nodiscard]] const std::vector<int>& GetRenderableNodeHandles() const;
    [[nodiscard]] std::string Describe() const;

    [[nodiscard]] std::size_t MaterialCount() const noexcept;
    [[nodiscard]] std::size_t MeshCount() const noexcept;
    [[nodiscard]] std::size_t CameraCount() const noexcept;
    [[nodiscard]] std::size_t LightCount() const noexcept;
    [[nodiscard]] std::size_t CameraConfinementVolumeCount() const noexcept;
    [[nodiscard]] std::size_t MeshInstanceBatchCount() const noexcept;
    [[nodiscard]] std::size_t MeshInstanceCount() const noexcept;

private:
    [[nodiscard]] bool IsValidNodeHandle(int handle) const;
    [[nodiscard]] bool WouldCreateCycle(int child, int parent) const;
    void RemoveChildReference(int parent, int child);
    void AppendNodeDescription(std::string& out, int nodeHandle, int depth) const;
    void InvalidateTransformCaches() const;
    void InvalidateRenderableNodeCache() const;
    void EnsureWorldMatrixCacheStorage() const;
    [[nodiscard]] const ri::math::Mat4& ComputeWorldMatrixCached(int nodeHandle) const;
    void RebuildRenderableNodeCache() const;

    std::string name_;
    std::vector<Node> nodes_;
    std::vector<Material> materials_;
    std::vector<Mesh> meshes_;
    std::vector<Camera> cameras_;
    std::vector<Light> lights_;
    std::vector<CameraConfinementVolume> cameraConfinementVolumes_;
    std::vector<MeshInstanceBatch> meshInstanceBatches_;
    mutable std::vector<ri::math::Mat4> worldMatrixCache_{};
    mutable std::vector<std::uint8_t> worldMatrixValid_{};
    mutable bool transformCacheDirty_ = true;
    mutable std::vector<int> renderableNodeCache_{};
    mutable bool renderableNodeCacheDirty_ = true;
};

} // namespace ri::scene
