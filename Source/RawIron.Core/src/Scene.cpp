#include "RawIron/Scene/Scene.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace ri::scene {

namespace {

std::string IndentForDepth(int depth) {
    return std::string(static_cast<std::size_t>(depth) * 2U, ' ');
}

constexpr std::uint64_t kStructuralBrushFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kStructuralBrushFnvPrime = 1099511628211ull;

void HashByte(std::uint64_t& hash, const unsigned char value) {
    hash ^= static_cast<std::uint64_t>(value);
    hash *= kStructuralBrushFnvPrime;
}

void HashString(std::uint64_t& hash, const std::string_view value) {
    for (const unsigned char c : value) {
        HashByte(hash, c);
    }
    HashByte(hash, 0xffU);
}

void HashUint(std::uint64_t& hash, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        HashByte(hash, static_cast<unsigned char>(value & 0xffU));
        value >>= 8U;
    }
}

void HashBool(std::uint64_t& hash, const bool value) {
    HashByte(hash, value ? 1U : 0U);
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

std::string ToString(MaterialStyle materialStyle) {
    switch (materialStyle) {
        case MaterialStyle::Standard:
            return "standard";
        case MaterialStyle::Retro:
            return "retro";
        case MaterialStyle::Layered:
            return "layered";
        case MaterialStyle::MixedMedia:
            return "mixed_media";
        case MaterialStyle::Crystal:
            return "crystal";
    }
    return "unknown";
}

std::string ToString(MaterialWorkflow materialWorkflow) {
    switch (materialWorkflow) {
        case MaterialWorkflow::MetalRough:
            return "metal_rough";
        case MaterialWorkflow::SpecGloss:
            return "spec_gloss";
    }
    return "unknown";
}

std::string ToString(StructuralBrushOperation operation) {
    switch (operation) {
        case StructuralBrushOperation::Unspecified:
            return "unspecified";
        case StructuralBrushOperation::Solid:
            return "solid";
        case StructuralBrushOperation::Subtract:
            return "subtract";
        case StructuralBrushOperation::Intersect:
            return "intersect";
        case StructuralBrushOperation::Stamp:
            return "stamp";
        case StructuralBrushOperation::Merge:
            return "merge";
        case StructuralBrushOperation::Detail:
            return "detail";
    }
    return "unknown";
}

std::string ToString(StructuralBrushSemanticRole role) {
    switch (role) {
        case StructuralBrushSemanticRole::Structure:
            return "structure";
        case StructuralBrushSemanticRole::Wall:
            return "wall";
        case StructuralBrushSemanticRole::Floor:
            return "floor";
        case StructuralBrushSemanticRole::Ceiling:
            return "ceiling";
        case StructuralBrushSemanticRole::Pillar:
            return "pillar";
        case StructuralBrushSemanticRole::Stair:
            return "stair";
        case StructuralBrushSemanticRole::Portal:
            return "portal";
        case StructuralBrushSemanticRole::Trim:
            return "trim";
        case StructuralBrushSemanticRole::Cover:
            return "cover";
        case StructuralBrushSemanticRole::Water:
            return "water";
        case StructuralBrushSemanticRole::Trigger:
            return "trigger";
        case StructuralBrushSemanticRole::Decor:
            return "decor";
        case StructuralBrushSemanticRole::Volume:
            return "volume";
    }
    return "unknown";
}

std::string ToString(StructuralBrushCollisionPolicy policy) {
    switch (policy) {
        case StructuralBrushCollisionPolicy::Solid:
            return "solid";
        case StructuralBrushCollisionPolicy::None:
            return "none";
        case StructuralBrushCollisionPolicy::Query:
            return "query";
        case StructuralBrushCollisionPolicy::Player:
            return "player";
        case StructuralBrushCollisionPolicy::Detail:
            return "detail";
        case StructuralBrushCollisionPolicy::Custom:
            return "custom";
    }
    return "unknown";
}

std::string ToString(StructuralBrushVisibilityPolicy policy) {
    switch (policy) {
        case StructuralBrushVisibilityPolicy::Ignored:
            return "ignored";
        case StructuralBrushVisibilityPolicy::Occluder:
            return "occluder";
        case StructuralBrushVisibilityPolicy::Portal:
            return "portal";
        case StructuralBrushVisibilityPolicy::AntiPortal:
            return "anti_portal";
        case StructuralBrushVisibilityPolicy::Transparent:
            return "transparent";
    }
    return "unknown";
}

std::string ToString(StructuralBrushNavigationPolicy policy) {
    switch (policy) {
        case StructuralBrushNavigationPolicy::Ignored:
            return "ignored";
        case StructuralBrushNavigationPolicy::Walkable:
            return "walkable";
        case StructuralBrushNavigationPolicy::Blocker:
            return "blocker";
        case StructuralBrushNavigationPolicy::Jump:
            return "jump";
        case StructuralBrushNavigationPolicy::Cover:
            return "cover";
        case StructuralBrushNavigationPolicy::Ladder:
            return "ladder";
    }
    return "unknown";
}

std::string ToString(StructuralBrushRebuildScope scope) {
    switch (scope) {
        case StructuralBrushRebuildScope::Local:
            return "local";
        case StructuralBrushRebuildScope::Region:
            return "region";
        case StructuralBrushRebuildScope::Global:
            return "global";
        case StructuralBrushRebuildScope::Manual:
            return "manual";
    }
    return "unknown";
}

std::string ToString(StructuralBrushChannel channel) {
    switch (channel) {
        case StructuralBrushChannel::VisualMesh:
            return "visual_mesh";
        case StructuralBrushChannel::PhysicsMesh:
            return "physics_mesh";
        case StructuralBrushChannel::QueryMesh:
            return "query_mesh";
        case StructuralBrushChannel::InformationLayer:
            return "information_layer";
    }
    return "unknown";
}

std::string ToString(StructuralBrushQueryPurpose purpose) {
    switch (purpose) {
        case StructuralBrushQueryPurpose::Raycast:
            return "raycast";
        case StructuralBrushQueryPurpose::Trace:
            return "trace";
        case StructuralBrushQueryPurpose::Placement:
            return "placement";
        case StructuralBrushQueryPurpose::Interaction:
            return "interaction";
    }
    return "unknown";
}

bool StructuralBrushParticipatesInChannel(const StructuralBrushMetadata& metadata,
                                          const StructuralBrushChannel channel) {
    switch (channel) {
        case StructuralBrushChannel::VisualMesh:
            return metadata.visualMesh.renderable;
        case StructuralBrushChannel::PhysicsMesh:
            return metadata.physicsMesh.participatesInSimulation;
        case StructuralBrushChannel::QueryMesh:
            return metadata.queryMesh.raycastable || metadata.queryMesh.traceable
                || metadata.queryMesh.placeable || metadata.queryMesh.interactable;
        case StructuralBrushChannel::InformationLayer:
            return metadata.informationLayer.reportable;
    }
    return false;
}

bool StructuralBrushSupportsQueryPurpose(const StructuralBrushMetadata& metadata,
                                         const StructuralBrushQueryPurpose purpose) {
    switch (purpose) {
        case StructuralBrushQueryPurpose::Raycast:
            return metadata.queryMesh.raycastable;
        case StructuralBrushQueryPurpose::Trace:
            return metadata.queryMesh.traceable;
        case StructuralBrushQueryPurpose::Placement:
            return metadata.queryMesh.placeable;
        case StructuralBrushQueryPurpose::Interaction:
            return metadata.queryMesh.interactable;
    }
    return false;
}

std::uint64_t StructuralBrushMetadataSignature(const StructuralBrushMetadata& metadata) {
    std::uint64_t hash = kStructuralBrushFnvOffset;

    HashString(hash, metadata.brushId);
    HashString(hash, metadata.region);
    HashUint(hash, static_cast<std::uint64_t>(metadata.operation));
    HashUint(hash, static_cast<std::uint64_t>(metadata.role));
    HashUint(hash, static_cast<std::uint64_t>(metadata.collision));
    HashUint(hash, static_cast<std::uint64_t>(metadata.visibility));
    HashUint(hash, static_cast<std::uint64_t>(metadata.navigation));
    HashUint(hash, static_cast<std::uint64_t>(metadata.rebuildScope));

    HashString(hash, metadata.visualMesh.meshId);
    HashString(hash, metadata.visualMesh.materialSetId);
    HashString(hash, metadata.visualMesh.uvSetId);
    HashBool(hash, metadata.visualMesh.renderable);

    HashString(hash, metadata.physicsMesh.meshId);
    HashString(hash, metadata.physicsMesh.rigidBodyShape);
    HashString(hash, metadata.physicsMesh.simulationShape);
    HashString(hash, metadata.physicsMesh.physicalMaterial);
    HashBool(hash, metadata.physicsMesh.participatesInSimulation);

    HashString(hash, metadata.queryMesh.meshId);
    HashString(hash, metadata.queryMesh.raycastShape);
    HashString(hash, metadata.queryMesh.placementShape);
    HashString(hash, metadata.queryMesh.interactionShape);
    HashBool(hash, metadata.queryMesh.raycastable);
    HashBool(hash, metadata.queryMesh.traceable);
    HashBool(hash, metadata.queryMesh.placeable);
    HashBool(hash, metadata.queryMesh.interactable);

    HashString(hash, metadata.informationLayer.semanticGraphId);
    HashString(hash, metadata.informationLayer.gameplayMeaning);
    HashBool(hash, metadata.informationLayer.reportable);
    HashUint(hash, static_cast<std::uint64_t>(metadata.informationLayer.relations.size()));
    for (const std::string& relation : metadata.informationLayer.relations) {
        HashString(hash, relation);
    }

    return hash;
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
