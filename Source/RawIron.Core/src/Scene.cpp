#include "RawIron/Scene/Scene.h"

#include <string>

namespace ri::scene {

namespace {

std::string IndentForDepth(int depth) {
    return std::string(static_cast<std::size_t>(depth) * 2U, ' ');
}

} // namespace

std::string ToString(PrimitiveType primitive) {
    switch (primitive) {
        case PrimitiveType::Custom:
            return "custom";
        case PrimitiveType::Cube:
            return "cube";
        case PrimitiveType::Plane:
            return "plane";
        case PrimitiveType::Sphere:
            return "sphere";
    }
    return "unknown";
}

std::string ToString(ShadingModel shadingModel) {
    switch (shadingModel) {
        case ShadingModel::Unlit:
            return "unlit";
        case ShadingModel::Lit:
            return "lit";
    }
    return "unknown";
}

std::string ToString(ProjectionType projectionType) {
    switch (projectionType) {
        case ProjectionType::Perspective:
            return "perspective";
        case ProjectionType::Orthographic:
            return "orthographic";
    }
    return "unknown";
}

std::string ToString(LightType lightType) {
    switch (lightType) {
        case LightType::Directional:
            return "directional";
        case LightType::Point:
            return "point";
        case LightType::Spot:
            return "spot";
    }
    return "unknown";
}

std::string ToString(CameraConfinementBehavior behavior) {
    switch (behavior) {
        case CameraConfinementBehavior::RegionClamp:
            return "region_clamp";
        case CameraConfinementBehavior::FramingCorridor:
            return "framing_corridor";
        case CameraConfinementBehavior::HoldAttention:
            return "hold_attention";
        case CameraConfinementBehavior::PathGuided:
            return "path_guided";
        case CameraConfinementBehavior::LimitFreeLook:
            return "limit_free_look";
        case CameraConfinementBehavior::AnchorMotion:
            return "anchor_motion";
        case CameraConfinementBehavior::CutsceneSync:
            return "cutscene_sync";
        case CameraConfinementBehavior::MediaPresentation:
            return "media_presentation";
    }
    return "unknown";
}

std::string ToString(CameraConfinementPurpose purpose) {
    switch (purpose) {
        case CameraConfinementPurpose::Unspecified:
            return "unspecified";
        case CameraConfinementPurpose::CornerReveal:
            return "corner_reveal";
        case CameraConfinementPurpose::ForcedObservation:
            return "forced_observation";
        case CameraConfinementPurpose::GuidedPath:
            return "guided_path";
        case CameraConfinementPurpose::ProjectedMedia:
            return "projected_media";
        case CameraConfinementPurpose::CutsceneStaging:
            return "cutscene_staging";
    }
    return "unknown";
}

Scene::Scene()
    : Scene("UntitledScene") {
}

Scene::Scene(std::string name)
    : name_(std::move(name)) {
}

const std::string& Scene::GetName() const noexcept {
    return name_;
}

void Scene::SetName(std::string name) {
    name_ = std::move(name);
}

int Scene::CreateNode(std::string name, int parent) {
    Node node;
    node.name = std::move(name);
    const int handle = static_cast<int>(nodes_.size());
    nodes_.push_back(std::move(node));
    InvalidateTransformCaches();
    InvalidateRenderableNodeCache();

    if (parent != kInvalidHandle) {
        (void)SetParent(handle, parent);
    }

    return handle;
}

bool Scene::SetParent(int child, int parent) {
    if (!IsValidNodeHandle(child)) {
        return false;
    }
    if (parent != kInvalidHandle && !IsValidNodeHandle(parent)) {
        return false;
    }
    if (child == parent || WouldCreateCycle(child, parent)) {
        return false;
    }

    Node& childNode = nodes_.at(static_cast<std::size_t>(child));
    if (childNode.parent != kInvalidHandle) {
        RemoveChildReference(childNode.parent, child);
    }

    childNode.parent = parent;
    if (parent != kInvalidHandle) {
        nodes_.at(static_cast<std::size_t>(parent)).children.push_back(child);
    }
    InvalidateTransformCaches();

    return true;
}

Node& Scene::GetNode(int handle) {
    InvalidateTransformCaches();
    return nodes_.at(static_cast<std::size_t>(handle));
}

const Node& Scene::GetNode(int handle) const {
    return nodes_.at(static_cast<std::size_t>(handle));
}

const std::vector<Node>& Scene::Nodes() const noexcept {
    return nodes_;
}

std::size_t Scene::NodeCount() const noexcept {
    return nodes_.size();
}

int Scene::AddMaterial(Material material) {
    materials_.push_back(std::move(material));
    return static_cast<int>(materials_.size() - 1U);
}

int Scene::AddMesh(Mesh mesh) {
    meshes_.push_back(std::move(mesh));
    return static_cast<int>(meshes_.size() - 1U);
}

int Scene::AddCamera(Camera camera) {
    cameras_.push_back(std::move(camera));
    return static_cast<int>(cameras_.size() - 1U);
}

int Scene::AddLight(Light light) {
    lights_.push_back(std::move(light));
    return static_cast<int>(lights_.size() - 1U);
}

int Scene::AddCameraConfinementVolume(CameraConfinementVolume volume) {
    cameraConfinementVolumes_.push_back(std::move(volume));
    return static_cast<int>(cameraConfinementVolumes_.size() - 1U);
}

int Scene::AddMeshInstanceBatch(MeshInstanceBatch batch) {
    meshInstanceBatches_.push_back(std::move(batch));
    return static_cast<int>(meshInstanceBatches_.size() - 1U);
}

void Scene::AddMeshInstance(int batchHandle, const Transform& transform) {
    meshInstanceBatches_.at(static_cast<std::size_t>(batchHandle)).transforms.push_back(transform);
}

void Scene::AttachMesh(int nodeHandle, int meshHandle, int materialHandle) {
    Node& node = GetNode(nodeHandle);
    node.mesh = meshHandle;
    node.material = materialHandle;
    InvalidateRenderableNodeCache();
}

void Scene::AttachCamera(int nodeHandle, int cameraHandle) {
    GetNode(nodeHandle).camera = cameraHandle;
}

void Scene::AttachLight(int nodeHandle, int lightHandle) {
    GetNode(nodeHandle).light = lightHandle;
}

void Scene::AttachCameraConfinementVolume(int nodeHandle, int volumeHandle) {
    GetNode(nodeHandle).cameraConfinementVolume = volumeHandle;
}

Material& Scene::GetMaterial(int handle) {
    return materials_.at(static_cast<std::size_t>(handle));
}

Mesh& Scene::GetMesh(int handle) {
    return meshes_.at(static_cast<std::size_t>(handle));
}

Camera& Scene::GetCamera(int handle) {
    return cameras_.at(static_cast<std::size_t>(handle));
}

Light& Scene::GetLight(int handle) {
    return lights_.at(static_cast<std::size_t>(handle));
}

CameraConfinementVolume& Scene::GetCameraConfinementVolume(int handle) {
    return cameraConfinementVolumes_.at(static_cast<std::size_t>(handle));
}

MeshInstanceBatch& Scene::GetMeshInstanceBatch(int handle) {
    return meshInstanceBatches_.at(static_cast<std::size_t>(handle));
}

const Material& Scene::GetMaterial(int handle) const {
    return materials_.at(static_cast<std::size_t>(handle));
}

const Mesh& Scene::GetMesh(int handle) const {
    return meshes_.at(static_cast<std::size_t>(handle));
}

const Camera& Scene::GetCamera(int handle) const {
    return cameras_.at(static_cast<std::size_t>(handle));
}

const Light& Scene::GetLight(int handle) const {
    return lights_.at(static_cast<std::size_t>(handle));
}

const CameraConfinementVolume& Scene::GetCameraConfinementVolume(int handle) const {
    return cameraConfinementVolumes_.at(static_cast<std::size_t>(handle));
}

const MeshInstanceBatch& Scene::GetMeshInstanceBatch(int handle) const {
    return meshInstanceBatches_.at(static_cast<std::size_t>(handle));
}

ri::math::Mat4 Scene::ComputeWorldMatrix(int nodeHandle) const {
    return ComputeWorldMatrixCached(nodeHandle);
}

ri::math::Vec3 Scene::ComputeWorldPosition(int nodeHandle) const {
    return ri::math::ExtractTranslation(ComputeWorldMatrix(nodeHandle));
}

const std::vector<int>& Scene::GetRenderableNodeHandles() const {
    if (renderableNodeCacheDirty_) {
        RebuildRenderableNodeCache();
    }
    return renderableNodeCache_;
}

std::string Scene::Describe() const {
    std::string out;
    out += "Scene: " + name_ + '\n';
    out += "Nodes=" + std::to_string(nodes_.size()) +
           " Meshes=" + std::to_string(meshes_.size()) +
           " Materials=" + std::to_string(materials_.size()) +
           " Cameras=" + std::to_string(cameras_.size()) +
           " Lights=" + std::to_string(lights_.size()) +
           " CameraConfinementVolumes=" + std::to_string(cameraConfinementVolumes_.size()) + '\n';

    for (int nodeIndex = 0; nodeIndex < static_cast<int>(nodes_.size()); ++nodeIndex) {
        if (nodes_.at(static_cast<std::size_t>(nodeIndex)).parent == kInvalidHandle) {
            AppendNodeDescription(out, nodeIndex, 0);
        }
    }

    return out;
}

std::size_t Scene::MaterialCount() const noexcept {
    return materials_.size();
}

std::size_t Scene::MeshCount() const noexcept {
    return meshes_.size();
}

std::size_t Scene::CameraCount() const noexcept {
    return cameras_.size();
}

std::size_t Scene::LightCount() const noexcept {
    return lights_.size();
}

std::size_t Scene::CameraConfinementVolumeCount() const noexcept {
    return cameraConfinementVolumes_.size();
}

std::size_t Scene::MeshInstanceBatchCount() const noexcept {
    return meshInstanceBatches_.size();
}

std::size_t Scene::MeshInstanceCount() const noexcept {
    std::size_t count = 0;
    for (const MeshInstanceBatch& batch : meshInstanceBatches_) {
        count += batch.transforms.size();
    }
    return count;
}

bool Scene::IsValidNodeHandle(int handle) const {
    return handle >= 0 && static_cast<std::size_t>(handle) < nodes_.size();
}

void Scene::InvalidateTransformCaches() const {
    transformCacheDirty_ = true;
}

void Scene::InvalidateRenderableNodeCache() const {
    renderableNodeCacheDirty_ = true;
}

void Scene::EnsureWorldMatrixCacheStorage() const {
    if (worldMatrixCache_.size() != nodes_.size()) {
        worldMatrixCache_.resize(nodes_.size());
    }
    if (worldMatrixValid_.size() != nodes_.size()) {
        worldMatrixValid_.assign(nodes_.size(), std::uint8_t{0});
    } else if (transformCacheDirty_) {
        std::fill(worldMatrixValid_.begin(), worldMatrixValid_.end(), std::uint8_t{0});
    }
    transformCacheDirty_ = false;
}

const ri::math::Mat4& Scene::ComputeWorldMatrixCached(int nodeHandle) const {
    EnsureWorldMatrixCacheStorage();
    if (!IsValidNodeHandle(nodeHandle)) {
        static const ri::math::Mat4 kIdentity = ri::math::IdentityMatrix();
        return kIdentity;
    }

    const std::size_t index = static_cast<std::size_t>(nodeHandle);
    if (worldMatrixValid_[index] != 0U) {
        return worldMatrixCache_[index];
    }

    const Node& node = nodes_[index];
    const ri::math::Mat4 local = node.localTransform.LocalMatrix();
    if (node.parent == kInvalidHandle) {
        worldMatrixCache_[index] = local;
    } else {
        worldMatrixCache_[index] = ri::math::Multiply(ComputeWorldMatrixCached(node.parent), local);
    }
    worldMatrixValid_[index] = std::uint8_t{1};
    return worldMatrixCache_[index];
}

void Scene::RebuildRenderableNodeCache() const {
    renderableNodeCache_.clear();
    renderableNodeCache_.reserve(nodes_.size());
    for (int index = 0; index < static_cast<int>(nodes_.size()); ++index) {
        if (nodes_[static_cast<std::size_t>(index)].mesh != kInvalidHandle) {
            renderableNodeCache_.push_back(index);
        }
    }
    renderableNodeCacheDirty_ = false;
}

bool Scene::WouldCreateCycle(int child, int parent) const {
    int current = parent;
    while (current != kInvalidHandle) {
        if (current == child) {
            return true;
        }
        current = nodes_.at(static_cast<std::size_t>(current)).parent;
    }
    return false;
}

void Scene::RemoveChildReference(int parent, int child) {
    if (!IsValidNodeHandle(parent)) {
        return;
    }

    std::vector<int>& children = nodes_.at(static_cast<std::size_t>(parent)).children;
    children.erase(std::remove(children.begin(), children.end(), child), children.end());
}

void Scene::AppendNodeDescription(std::string& out, int nodeHandle, int depth) const {
    const Node& node = GetNode(nodeHandle);
    out += IndentForDepth(depth) + "- " + node.name;
    out += " local=" + ri::math::ToString(node.localTransform.position);
    out += " world=" + ri::math::ToString(ComputeWorldPosition(nodeHandle));

    if (node.mesh != kInvalidHandle) {
        const Mesh& mesh = GetMesh(node.mesh);
        out += " mesh=" + mesh.name + "(" + ToString(mesh.primitive) + ")";
    }
    if (node.material != kInvalidHandle) {
        const Material& material = GetMaterial(node.material);
        out += " material=" + material.name + "(" + ToString(material.shadingModel) + ")";
    }
    if (node.camera != kInvalidHandle) {
        const Camera& camera = GetCamera(node.camera);
        out += " camera=" + camera.name + "(" + ToString(camera.projection) + ")";
    }
    if (node.light != kInvalidHandle) {
        const Light& light = GetLight(node.light);
        out += " light=" + light.name + "(" + ToString(light.type) + ")";
    }
    if (node.cameraConfinementVolume != kInvalidHandle) {
        const CameraConfinementVolume& volume = GetCameraConfinementVolume(node.cameraConfinementVolume);
        out += " camera_confinement=" + volume.name + "(" + ToString(volume.behavior) + ")";
    }
    out += '\n';

    for (int childHandle : node.children) {
        AppendNodeDescription(out, childHandle, depth + 1);
    }
}

} // namespace ri::scene
