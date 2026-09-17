#include "ForgePreviewBuilder.h"

#include "RawIron/Content/NativeAnimationDocument.h"
#include "RawIron/Content/PrimitiveModelDocument.h"
#include "RawIron/Content/NativeSculptDocument.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Scene/NativeAnimation.h"
#include "RawIron/Scene/NativeSculpt.h"
#include "RawIron/Scene/PrimitiveModelBake.h"
#include "RawIron/Scene/RigAuthoring.h"
#include "RawIron/Scene/SceneUtils.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace ri::forge {
namespace {

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

struct SkeletonPreview {
    std::vector<int> frameNodes{};
    std::vector<int> boneNodes{};
};

SkeletonPreview InstantiateRigSkeleton(
    ri::scene::Scene& scene,
    const int previewRoot,
    const ri::scene::RigDefinition& rig) {
    const int skeletonRoot = scene.CreateNode("ForgeSkeleton", previewRoot);
    SkeletonPreview preview{};
    preview.boneNodes.assign(rig.bones.size(), ri::scene::kInvalidHandle);
    preview.frameNodes.reserve(rig.bones.size());
    for (std::size_t index = 0; index < rig.bones.size(); ++index) {
        const ri::scene::RigBone& bone = rig.bones[index];
        const std::string boneName = bone.name.empty() ? ("Bone" + std::to_string(index)) : bone.name;
        int parent = skeletonRoot;
        if (bone.parentIndex >= 0 && bone.parentIndex < static_cast<int>(preview.boneNodes.size())
            && preview.boneNodes[static_cast<std::size_t>(bone.parentIndex)] != ri::scene::kInvalidHandle
            && bone.parentIndex != static_cast<int>(index)) {
            parent = preview.boneNodes[static_cast<std::size_t>(bone.parentIndex)];
            const ri::math::Vec3 offset = bone.restLocal.position;
            const float length = ri::math::Length(offset);
            if (length > 0.001F) {
                constexpr float kShaftThickness = 0.028F;
                ri::scene::PrimitiveNodeOptions shaft{};
                shaft.nodeName = boneName + "Shaft";
                shaft.parent = parent;
                shaft.primitive = ri::scene::PrimitiveType::Cube;
                shaft.shadingModel = ri::scene::ShadingModel::Unlit;
                shaft.materialName = boneName + "ShaftMaterial";
                shaft.baseColor = {0.55F, 0.62F, 0.38F};
                shaft.transform.position = offset * 0.5F;
                shaft.transform.scale = {
                    std::max(std::abs(offset.x), kShaftThickness),
                    std::max(std::abs(offset.y), kShaftThickness),
                    std::max(std::abs(offset.z), kShaftThickness),
                };
                (void)ri::scene::AddPrimitiveNode(scene, shaft);
            }
        }
        const int boneNode = scene.CreateNode(boneName, parent);
        scene.GetNode(boneNode).localTransform = bone.restLocal;
        preview.boneNodes[index] = boneNode;

        ri::scene::PrimitiveNodeOptions joint{};
        joint.nodeName = boneName + "Joint";
        joint.parent = boneNode;
        joint.primitive = ri::scene::PrimitiveType::Sphere;
        joint.shadingModel = ri::scene::ShadingModel::Unlit;
        joint.materialName = boneName + "JointMaterial";
        const float jointScale = bone.deform ? 0.055F : 0.034F;
        joint.baseColor = bone.deform ? ri::math::Vec3{0.82F, 0.86F, 0.48F} : ri::math::Vec3{0.42F, 0.48F, 0.52F};
        joint.transform.scale = {jointScale, jointScale, jointScale};
        const int jointNode = ri::scene::AddPrimitiveNode(scene, joint);
        if (jointNode != ri::scene::kInvalidHandle) {
            preview.frameNodes.push_back(jointNode);
        }
    }
    return preview;
}

void AddPreviewStage(
    ForgePreviewBuildResult& result,
    const int previewRoot,
    const std::vector<int>& frameNodes) {
    result.gridNode = ri::scene::AddGridHelper(
        result.scene,
        ri::scene::GridHelperOptions{
            .nodeName = "ForgeGrid",
            .parent = previewRoot,
            .size = 12.0F,
        });
    result.axesNode = ri::scene::AddAxesHelper(
        result.scene,
        ri::scene::AxesHelperOptions{
            .nodeName = "ForgeAxes",
            .parent = previewRoot,
            .axisLength = 1.5F,
        }).root;
    (void)ri::scene::AddLightNode(
        result.scene,
        ri::scene::LightNodeOptions{
            .nodeName = "ForgeKeyLight",
            .parent = previewRoot,
            .transform = ri::scene::Transform{
                .rotationDegrees = {-35.0F, -35.0F, 0.0F},
            },
            .light = ri::scene::Light{
                .name = "ForgeKeyLight",
                .type = ri::scene::LightType::Directional,
                .color = {1.0F, 0.92F, 0.82F},
                .intensity = 2.0F,
            },
        });
    (void)ri::scene::AddLightNode(
        result.scene,
        ri::scene::LightNodeOptions{
            .nodeName = "ForgeFillLight",
            .parent = previewRoot,
            .transform = ri::scene::Transform{
                .rotationDegrees = {25.0F, 145.0F, 0.0F},
            },
            .light = ri::scene::Light{
                .name = "ForgeFillLight",
                .type = ri::scene::LightType::Directional,
                .color = {0.58F, 0.72F, 1.0F},
                .intensity = 0.65F,
            },
        });
    result.camera = ri::scene::AddOrbitCamera(
        result.scene,
        ri::scene::OrbitCameraOptions{
            .rigName = "ForgeOrbit",
            .parent = previewRoot,
            .camera = ri::scene::Camera{
                .name = "ForgeCamera",
                .fieldOfViewDegrees = 55.0F,
                .nearClip = 0.02F,
                .farClip = 1000.0F,
            },
            .orbit = ri::scene::OrbitCameraState{
                .distance = 6.0F,
                .yawDegrees = 145.0F,
                .pitchDegrees = -18.0F,
            },
        });
    if (!frameNodes.empty()) {
        (void)ri::scene::FrameNodesWithOrbitCamera(
            result.scene, result.camera, frameNodes, 1.45F);
    }
}

void OverlayBoundRig(
    ForgePreviewBuildResult& result,
    const int previewRoot,
    std::vector<int>& frameNodes,
    const std::string& rigPath,
    const std::filesystem::path& assetPath) {
    if (rigPath.empty()) {
        return;
    }
    const auto rig = LoadSidecarRig(rigPath, assetPath);
    if (!rig.has_value() || rig->bones.empty()) {
        result.status += " | rig missing";
        return;
    }
    const SkeletonPreview skeleton = InstantiateRigSkeleton(result.scene, previewRoot, *rig);
    result.boneNodes = skeleton.boneNodes;
    result.boneCount = rig->bones.size();
    frameNodes.insert(frameNodes.end(), skeleton.frameNodes.begin(), skeleton.frameNodes.end());
    result.status += " | rig " + (rig->displayName.empty() ? rigPath : rig->displayName);
}

} // namespace

ForgePreviewBuildResult BuildForgePreviewScene(
    const std::filesystem::path& assetPath,
    const AssetKind kind,
    const std::uint64_t generation,
    const std::uintmax_t maximumSourceBytes) {
    const auto started = std::chrono::steady_clock::now();
    ForgePreviewBuildResult result{};
    result.generation = generation;
    result.assetPath = assetPath;
    const int previewRoot = result.scene.CreateNode("Forge3DPreview");
    std::vector<int> frameNodes{};

    if (assetPath.empty()) {
        result.status = "Select a model to preview";
    } else if (kind == AssetKind::PrimitiveModel) {
        const auto document = ri::content::LoadPrimitiveModelDocument(assetPath);
        if (!document.has_value()) {
            result.status = "Primitive model preview could not be loaded";
        } else {
            const ri::scene::PrimitiveModelInstantiationResult instantiated =
                ri::scene::InstantiatePrimitiveModel(
                    result.scene,
                    previewRoot,
                    *document,
                    assetPath.parent_path());
            if (instantiated.valid) {
                frameNodes = instantiated.partNodes;
                result.partIds = instantiated.partIds;
                result.groupNodes = instantiated.groupNodes;
                result.groupIds = instantiated.groupIds;
                result.assetLoaded = !frameNodes.empty();
                result.status = "Native grouped primitive preview";
                OverlayBoundRig(result, previewRoot, frameNodes, document->rigPath, assetPath);
            } else {
                result.status = instantiated.errors.empty()
                    ? "Primitive model produced no preview geometry"
                    : instantiated.errors.front();
            }
        }
    } else if (kind == AssetKind::Sculpt) {
        const auto sculpt = ri::content::LoadNativeSculptDocument(assetPath);
        if (!sculpt.has_value()) {
            result.status = "Native sculpt could not be loaded";
        } else {
            result.sculptNode = ri::scene::InstantiateNativeSculpt(result.scene, previewRoot, *sculpt);
            if (result.sculptNode != ri::scene::kInvalidHandle) {
                frameNodes.push_back(result.sculptNode);
                result.assetLoaded = true;
                result.sculptWireframeNode =
                    ri::scene::InstantiateNativeSculptWireframe(result.scene, previewRoot, sculpt->mesh);
                result.sculptNormalsNode =
                    ri::scene::InstantiateNativeSculptNormals(result.scene, previewRoot, sculpt->mesh);
                result.sculptCollisionNode =
                    ri::scene::InstantiateNativeSculptCollisionBounds(result.scene, previewRoot, sculpt->mesh);
                result.sculptBrushCursorNode =
                    ri::scene::InstantiateNativeSculptBrushCursor(result.scene, previewRoot);
                ri::scene::UpdateNativeSculptBrushCursor(
                    result.scene, result.sculptBrushCursorNode, {}, 0.18f, false);
                const auto report = ri::content::ValidateNativeSculptDocument(*sculpt);
                result.status = "Native sculpt (" + sculpt->cage + ", "
                    + std::to_string(report.vertexCount)
                    + " verts)  |  LMB CLAY  |  SHIFT SMOOTH  |  CTRL INFLATE  |  E EXTRUDE  |  9/0 DENSITY";
                OverlayBoundRig(result, previewRoot, frameNodes, sculpt->rigPath, assetPath);
            } else {
                result.status = "Native sculpt produced no preview mesh";
            }
        }
    } else if (kind == AssetKind::Rig) {
        const std::optional<ri::scene::RigDefinition> rig = ri::scene::LoadRigDefinition(assetPath);
        if (!rig.has_value()) {
            result.status = "Rig document could not be loaded";
        } else if (rig->bones.empty()) {
            result.status = "Rig document contains no bones";
        } else {
            const SkeletonPreview skeleton = InstantiateRigSkeleton(result.scene, previewRoot, *rig);
            frameNodes = skeleton.frameNodes;
            result.boneNodes = skeleton.boneNodes;
            result.boneCount = rig->bones.size();
            result.assetLoaded = !frameNodes.empty();
            const ri::scene::RigValidationReport validation = ri::scene::ValidateRigDefinition(*rig);
            if (result.assetLoaded && validation.valid) {
                result.status = "Rig skeleton preview (" + std::to_string(result.boneCount) + " bones)";
            } else if (result.assetLoaded) {
                result.status = validation.errors.empty()
                    ? "Rig skeleton preview with validation warnings"
                    : validation.errors.front();
            } else {
                result.status = "Rig produced no preview joints";
            }
        }
    } else if (kind == AssetKind::Animation) {
        const auto clip = ri::content::LoadNativeAnimationDocument(assetPath);
        if (!clip.has_value()) {
            result.status = "Animation clip could not be loaded";
        } else {
            const auto rig = LoadSidecarRig(clip->rigPath, assetPath);
            if (!rig.has_value() || rig->bones.empty()) {
                result.status = "Animation has no usable rig to preview";
            } else {
                const SkeletonPreview skeleton = InstantiateRigSkeleton(result.scene, previewRoot, *rig);
                frameNodes = skeleton.frameNodes;
                result.boneNodes = skeleton.boneNodes;
                result.boneCount = rig->bones.size();
                const ri::scene::AnimationClip bound =
                    ri::scene::BindNativeAnimationClip(*clip, result.scene, result.boneNodes);
                ri::scene::ApplyAnimationClip(result.scene, bound, 0.0);
                result.assetLoaded = !frameNodes.empty();
                result.status = "Motion clip (" + clip->displayName + ", "
                    + std::to_string(clip->tracks.size()) + " tracks)";
            }
        }
    } else {
        const std::string extension = LowerAscii(assetPath.extension().string());
        if (extension == ".blend") {
            result.status = "Blender source requires export to FBX, glTF, GLB, or OBJ";
        } else {
            std::error_code error{};
            const std::uintmax_t sourceBytes = std::filesystem::file_size(assetPath, error);
            if (!error && sourceBytes > maximumSourceBytes) {
                result.status = "Source exceeds the interactive preview size limit; validate or open it directly";
            } else {
                std::string importError{};
                const int imported = ri::scene::AddModelNode(
                    result.scene,
                    ri::scene::ImportedModelOptions{
                        .sourcePath = assetPath,
                        .nodeName = assetPath.stem().string(),
                        .parent = previewRoot,
                        .snapMeshBaseToGround = true,
                        .createPlaceholderOnFailure = false,
                    },
                    &importError);
                if (imported != ri::scene::kInvalidHandle) {
                    frameNodes = ri::scene::CollectRenderableNodes(result.scene);
                    result.assetLoaded = !frameNodes.empty();
                    result.status = result.assetLoaded
                        ? "Imported source preview"
                        : "Importer returned no renderable geometry";
                } else {
                    result.status = importError.empty() ? "Source preview import failed" : importError;
                }
            }
        }
    }

    result.frameNodes = frameNodes;
    result.renderableNodeCount = frameNodes.size();
    AddPreviewStage(result, previewRoot, frameNodes);
    result.elapsedMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    return result;
}

AsyncForgePreviewBuilder::AsyncForgePreviewBuilder()
    : worker_([this](const std::stop_token stopToken) { Run(stopToken); }) {}

AsyncForgePreviewBuilder::~AsyncForgePreviewBuilder() {
    worker_.request_stop();
    wake_.notify_all();
}

std::uint64_t AsyncForgePreviewBuilder::Request(
    std::filesystem::path assetPath,
    const AssetKind kind) {
    std::scoped_lock lock(mutex_);
    ++requestedGeneration_;
    requestedAssetPath_ = std::move(assetPath);
    requestedKind_ = kind;
    requestPending_ = true;
    busy_ = true;
    wake_.notify_one();
    return requestedGeneration_;
}

std::optional<ForgePreviewBuildResult> AsyncForgePreviewBuilder::Poll() {
    std::scoped_lock lock(mutex_);
    if (!completed_.has_value()) {
        return std::nullopt;
    }
    std::optional<ForgePreviewBuildResult> result = std::move(completed_);
    completed_.reset();
    return result;
}

bool AsyncForgePreviewBuilder::Busy() const {
    std::scoped_lock lock(mutex_);
    return busy_;
}

void AsyncForgePreviewBuilder::Run(const std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        std::uint64_t generation = 0;
        std::filesystem::path assetPath{};
        AssetKind kind = AssetKind::ModelSource;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, stopToken, [this]() { return requestPending_; });
            if (stopToken.stop_requested()) {
                break;
            }
            generation = requestedGeneration_;
            assetPath = requestedAssetPath_;
            kind = requestedKind_;
            requestPending_ = false;
        }

        ForgePreviewBuildResult result{};
        try {
            result = BuildForgePreviewScene(assetPath, kind, generation);
        } catch (const std::exception& exception) {
            result = BuildForgePreviewScene({}, AssetKind::ModelSource, generation);
            result.assetPath = assetPath;
            result.status = std::string("Preview failed: ") + exception.what();
        } catch (...) {
            result = BuildForgePreviewScene({}, AssetKind::ModelSource, generation);
            result.assetPath = assetPath;
            result.status = "Preview failed with an unknown importer error";
        }
        std::scoped_lock lock(mutex_);
        if (generation == requestedGeneration_) {
            completed_ = std::move(result);
            busy_ = false;
        }
    }
}

bool ShouldReuseForgePreview(
    const std::filesystem::path& loadedPath,
    const bool loadedHasWriteTime,
    const std::filesystem::file_time_type loadedWriteTime,
    const std::filesystem::path& requestedPath,
    const bool requestedHasWriteTime,
    const std::filesystem::file_time_type requestedWriteTime,
    const bool keepLiveDocument) noexcept {
    if (loadedPath != requestedPath) {
        return false;
    }
    if (keepLiveDocument) {
        return true;
    }
    if (loadedHasWriteTime != requestedHasWriteTime) {
        return false;
    }
    return !requestedHasWriteTime || loadedWriteTime == requestedWriteTime;
}

} // namespace ri::forge
