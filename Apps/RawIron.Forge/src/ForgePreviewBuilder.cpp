#include "ForgePreviewBuilder.h"

#include "RawIron/Content/NativeAnimationDocument.h"
#include "RawIron/Content/BlockCharacterDocument.h"
#include "RawIron/Content/PrimitiveModelDocument.h"
#include "RawIron/Content/NativeSculptDocument.h"
#include "RawIron/Math/Mat4.h"
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
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ri::forge {
namespace {

namespace fs = std::filesystem;

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

struct SkeletonPreviewOptions {
    bool drawJoints = true;
    bool drawShafts = true;
};

SkeletonPreview InstantiateRigSkeleton(
    ri::scene::Scene& scene,
    const int previewRoot,
    const ri::scene::RigDefinition& rig,
    const SkeletonPreviewOptions options = {}) {
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
            // Thin bone stick along the dominant axis only. The old AABB-style
            // max(|ox|,|oy|,|oz|) scale turned diagonal bones into opaque slabs
            // that filled the hearth.
            if (options.drawShafts && length > 0.001F) {
                constexpr float kShaftThickness = 0.022F;
                ri::scene::PrimitiveNodeOptions shaft{};
                shaft.nodeName = boneName + "Shaft";
                shaft.parent = parent;
                shaft.primitive = ri::scene::PrimitiveType::Cube;
                shaft.shadingModel = ri::scene::ShadingModel::Unlit;
                shaft.materialName = boneName + "ShaftMaterial";
                shaft.baseColor = {0.55F, 0.62F, 0.38F};
                shaft.transform.position = offset * 0.5F;
                ri::math::Vec3 scale{kShaftThickness, kShaftThickness, kShaftThickness};
                const float ax = std::abs(offset.x);
                const float ay = std::abs(offset.y);
                const float az = std::abs(offset.z);
                if (ax >= ay && ax >= az) {
                    scale.x = length;
                } else if (ay >= az) {
                    scale.y = length;
                } else {
                    scale.z = length;
                }
                shaft.transform.scale = scale;
                (void)ri::scene::AddPrimitiveNode(scene, shaft);
            }
        }
        const int boneNode = scene.CreateNode(boneName, parent);
        scene.GetNode(boneNode).localTransform = bone.restLocal;
        preview.boneNodes[index] = boneNode;

        if (options.drawJoints) {
            ri::scene::PrimitiveNodeOptions joint{};
            joint.nodeName = boneName + "Joint";
            joint.parent = boneNode;
            joint.primitive = ri::scene::PrimitiveType::Sphere;
            joint.shadingModel = ri::scene::ShadingModel::Unlit;
            joint.materialName = boneName + "JointMaterial";
            const float jointScale = bone.deform ? 0.045F : 0.028F;
            joint.baseColor =
                bone.deform ? ri::math::Vec3{0.82F, 0.86F, 0.48F} : ri::math::Vec3{0.42F, 0.48F, 0.52F};
            joint.transform.scale = {jointScale, jointScale, jointScale};
            const int jointNode = ri::scene::AddPrimitiveNode(scene, joint);
            if (jointNode != ri::scene::kInvalidHandle) {
                preview.frameNodes.push_back(jointNode);
            }
        }
    }
    return preview;
}

void AddPreviewStage(
    ForgePreviewBuildResult& result,
    const int previewRoot,
    const std::vector<int>& frameNodes) {
    // Lit ground so directional shadows from the character actually show.
    {
        ri::scene::PrimitiveNodeOptions floor{};
        floor.nodeName = "ForgeShadowFloor";
        floor.parent = previewRoot;
        floor.primitive = ri::scene::PrimitiveType::Cube;
        floor.shadingModel = ri::scene::ShadingModel::Lit;
        floor.materialStyle = ri::scene::MaterialStyle::Standard;
        floor.materialName = "ForgeShadowFloorMaterial";
        floor.baseColor = {0.35F, 0.36F, 0.38F};
        floor.roughness = 1.0F;
        floor.transform.position = {0.0F, -0.08F, 0.0F};
        floor.transform.scale = {16.0F, 0.16F, 16.0F};
        (void)ri::scene::AddPrimitiveNode(result.scene, floor);
    }
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
                // Directional emits along +Z; positive pitch aims down onto the floor.
                .rotationDegrees = {42.0F, -32.0F, 0.0F},
            },
            .light = ri::scene::Light{
                .name = "ForgeKeyLight",
                .type = ri::scene::LightType::Directional,
                .color = {1.0F, 0.94F, 0.84F},
                .intensity = 3.1F,
            },
        });
    (void)ri::scene::AddLightNode(
        result.scene,
        ri::scene::LightNodeOptions{
            .nodeName = "ForgeFillLight",
            .parent = previewRoot,
            .transform = ri::scene::Transform{
                .rotationDegrees = {18.0F, 140.0F, 0.0F},
            },
            .light = ri::scene::Light{
                .name = "ForgeFillLight",
                .type = ri::scene::LightType::Directional,
                .color = {0.55F, 0.68F, 0.95F},
                .intensity = 0.45F,
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

[[nodiscard]] std::string NormalizeRelativeRigKey(std::string value) {
    for (char& character : value) {
        if (character == '\\') {
            character = '/';
        } else {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
    }
    while (!value.empty() && value.front() == '/') {
        value.erase(value.begin());
    }
    constexpr std::string_view kPrefix = "assets/source/";
    if (value.size() > kPrefix.size() && value.compare(0, kPrefix.size(), kPrefix) == 0) {
        value.erase(0, kPrefix.size());
    }
    return value;
}

[[nodiscard]] std::filesystem::path FindCompanionSculptForRig(
    const std::filesystem::path& assetPath,
    const std::string& relativeRigPath,
    const std::string& preferredStem = {}) {
    if (relativeRigPath.empty() || assetPath.empty()) {
        return {};
    }
    const std::string wantedRig = NormalizeRelativeRigKey(relativeRigPath);
    const fs::path sourceRoot = assetPath.parent_path().parent_path();
    const fs::path sculptFolder = sourceRoot / "sculpts";
    std::error_code folderError{};
    if (!fs::is_directory(sculptFolder, folderError)) {
        return {};
    }

    fs::path preferredMatch{};
    fs::path anyMatch{};
    for (const fs::directory_entry& entry : fs::directory_iterator(sculptFolder, folderError)) {
        if (folderError || !entry.is_regular_file()) {
            continue;
        }
        const fs::path path = entry.path();
        if (path.filename().string().find(".ri_sculpt.json") == std::string::npos) {
            continue;
        }
        const auto sculpt = ri::content::LoadNativeSculptDocument(path);
        if (!sculpt.has_value() || sculpt->rigPath.empty()) {
            continue;
        }
        if (NormalizeRelativeRigKey(sculpt->rigPath) != wantedRig) {
            continue;
        }
        if (anyMatch.empty()) {
            anyMatch = path;
        }
        const std::string fileStem = path.stem().string(); // e.g. psx_scout.ri_sculpt
        const std::string baseStem = fs::path(fileStem).stem().string(); // e.g. psx_scout
        if (!preferredStem.empty()
            && (baseStem == preferredStem || fileStem == preferredStem
                || fileStem == preferredStem + ".ri_sculpt")) {
            preferredMatch = path;
            break;
        }
    }
    return preferredMatch.empty() ? anyMatch : preferredMatch;
}

[[nodiscard]] std::string CharacterStemFromClipPath(const std::filesystem::path& animPath) {
    std::string stem = animPath.stem().string();
    // strip .ri_anim if present (path may be name.ri_anim.json)
    constexpr std::string_view kAnimSuffix = ".ri_anim";
    if (stem.size() > kAnimSuffix.size()
        && stem.compare(stem.size() - kAnimSuffix.size(), kAnimSuffix.size(), kAnimSuffix) == 0) {
        stem.resize(stem.size() - kAnimSuffix.size());
    }
    for (const std::string_view suffix : {"_walk", "_idle", "_run", "_pose"}) {
        if (stem.size() > suffix.size()
            && stem.compare(stem.size() - suffix.size(), suffix.size(), suffix) == 0) {
            stem.resize(stem.size() - suffix.size());
            break;
        }
    }
    return stem;
}

[[nodiscard]] int FindBoneNodeByName(
    const ri::scene::Scene& scene,
    const std::vector<int>& boneNodes,
    const std::string_view name) {
    for (const int node : boneNodes) {
        if (node == ri::scene::kInvalidHandle) {
            continue;
        }
        if (scene.GetNode(node).name == name) {
            return node;
        }
    }
    return ri::scene::kInvalidHandle;
}

[[nodiscard]] int AttachPsxPartMesh(
    ri::scene::Scene& scene,
    const int parent,
    const ri::scene::PsxBlockPartDesc& part,
    const ri::math::Vec3& localCenter,
    const std::size_t partIndex,
    const std::filesystem::path& textureRoot = {}) {
    // Lit Custom with hard face normals (taper + angle baked into the mesh).
    ri::scene::Mesh mesh = ri::scene::MakePsxBlockPartMesh(part);
    if (mesh.positions.empty()) {
        return ri::scene::kInvalidHandle;
    }
    std::string albedoTexture = part.albedoTexture;
    if (!albedoTexture.empty() && !textureRoot.empty() && !fs::path(albedoTexture).is_absolute()) {
        const fs::path resolved = textureRoot / albedoTexture;
        std::error_code ec{};
        if (fs::is_regular_file(resolved, ec)) {
            albedoTexture = resolved.lexically_normal().string();
        }
    }
    const int material = scene.AddMaterial(ri::scene::Material{
        .name = part.boneName + "PsxMat" + std::to_string(partIndex),
        .shadingModel = ri::scene::ShadingModel::Lit,
        .materialStyle = ri::scene::MaterialStyle::Standard,
        .baseColor = part.baseColor,
        .baseColorTexture = std::move(albedoTexture),
        .metallic = part.metallic,
        .roughness = part.roughness,
    });
    const int meshHandle = scene.AddMesh(std::move(mesh));
    const int node = scene.CreateNode(
        part.boneName + "PsxPart" + std::to_string(partIndex), parent);
    if (node == ri::scene::kInvalidHandle) {
        return ri::scene::kInvalidHandle;
    }
    scene.GetNode(node).localTransform.position = localCenter;
    scene.AttachMesh(node, meshHandle, material);
    return node;
}

[[nodiscard]] std::optional<ri::content::BlockCharacterDocument> LoadCompanionBlockCharacter(
    const std::filesystem::path& sculptPath,
    const ri::content::NativeSculptDocument& sculpt) {
    std::vector<fs::path> candidates{};
    if (!sculpt.blockCharPath.empty()) {
        candidates.push_back(sculptPath.parent_path() / sculpt.blockCharPath);
        // Common: blockCharPath is workspace-relative from Assets/Source.
        candidates.push_back(sculptPath.parent_path().parent_path() / sculpt.blockCharPath);
    }
    // `psx_scout.ri_sculpt.json` → stem `psx_scout.ri_sculpt` → base `psx_scout`.
    const std::string fileStem = sculptPath.stem().string();
    const std::string baseStem = fs::path(fileStem).stem().string();
    candidates.push_back(
        sculptPath.parent_path().parent_path() / "blockchars" / (baseStem + ".ri_blockchar.json"));
    candidates.push_back(sculptPath.parent_path() / (baseStem + ".ri_blockchar.json"));
    std::error_code ec{};
    for (const fs::path& candidate : candidates) {
        if (!candidate.empty() && fs::is_regular_file(candidate, ec)) {
            if (auto loaded = ri::content::LoadBlockCharacterDocument(candidate)) {
                return loaded;
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::vector<ri::scene::PsxBlockPartDesc> LoadPsxPartsForSculpt(
    const std::filesystem::path& sculptPath,
    const ri::content::NativeSculptDocument& sculpt) {
    const auto blockChar = LoadCompanionBlockCharacter(sculptPath, sculpt);
    if (!blockChar.has_value()) {
        return {};
    }
    return ri::scene::BlockCharacterPartsToPsxDescs(*blockChar);
}

/// PSX preview: parent Lit tapered meshes to bones from authored .ri_blockchar.json.
bool AttachPsxBlockPartsToBones(
    ForgePreviewBuildResult& result,
    std::vector<int>& frameNodes,
    const std::filesystem::path& animPath,
    const std::string& relativeRigPath,
    const std::unordered_map<std::string, ri::math::Mat4>& restBoneWorld) {
    const std::string preferred = CharacterStemFromClipPath(animPath);
    const fs::path sculptPath = FindCompanionSculptForRig(animPath, relativeRigPath, preferred);
    if (sculptPath.empty()) {
        return false;
    }
    const auto sculpt = ri::content::LoadNativeSculptDocument(sculptPath);
    if (!sculpt.has_value() || sculpt->cage != "psx") {
        return false;
    }
    result.companionSculptPath = sculptPath;
    const std::vector<ri::scene::PsxBlockPartDesc> parts = LoadPsxPartsForSculpt(sculptPath, *sculpt);
    if (parts.empty()) {
        result.status = "PSX sculpt is missing its .ri_blockchar.json (blockCharPath).";
        return false;
    }

    std::size_t attached = 0;
    std::size_t partIndex = 0;
    for (const ri::scene::PsxBlockPartDesc& part : parts) {
        const int boneNode = FindBoneNodeByName(result.scene, result.boneNodes, part.boneName);
        if (boneNode == ri::scene::kInvalidHandle) {
            ++partIndex;
            continue;
        }
        const auto restIt = restBoneWorld.find(part.boneName);
        if (restIt == restBoneWorld.end()) {
            ++partIndex;
            continue;
        }
        ri::math::Mat4 inverseRest{};
        if (!ri::math::TryInvertMat4(restIt->second, inverseRest)) {
            ++partIndex;
            continue;
        }
        const ri::math::Vec3 localCenter = ri::math::TransformPoint(inverseRest, part.center);
        const fs::path textureRoot =
            sculptPath.parent_path().parent_path().parent_path() / "Textures";
        const int node = AttachPsxPartMesh(
            result.scene, boneNode, part, localCenter, partIndex, textureRoot);
        ++partIndex;
        if (node == ri::scene::kInvalidHandle) {
            continue;
        }
        frameNodes.push_back(node);
        if (result.sculptNode == ri::scene::kInvalidHandle) {
            result.sculptNode = node;
        }
        ++attached;
    }
    result.companionIsRigidBlocks = attached > 0U;
    return attached > 0U;
}

bool AttachCompanionSculpt(
    ForgePreviewBuildResult& result,
    const int previewRoot,
    std::vector<int>& frameNodes,
    const std::filesystem::path& animPath,
    const std::string& relativeRigPath) {
    const std::string preferred = CharacterStemFromClipPath(animPath);
    const fs::path sculptPath = FindCompanionSculptForRig(animPath, relativeRigPath, preferred);
    if (sculptPath.empty()) {
        return false;
    }
    const auto sculpt = ri::content::LoadNativeSculptDocument(sculptPath);
    if (!sculpt.has_value()) {
        return false;
    }
    // Prefer bone-parented cubes for PSX — see AttachPsxBlockPartsToBones.
    if (sculpt->cage == "psx") {
        return false;
    }
    result.sculptNode = ri::scene::InstantiateNativeSculpt(result.scene, previewRoot, *sculpt);
    if (result.sculptNode == ri::scene::kInvalidHandle) {
        return false;
    }
    result.companionSculptPath = sculptPath;
    frameNodes.push_back(result.sculptNode);
    return true;
}

[[nodiscard]] std::unordered_map<std::string, ri::math::Mat4> CollectBoneWorldMatrices(
    const ri::scene::Scene& scene,
    const std::vector<int>& boneNodes) {
    std::unordered_map<std::string, ri::math::Mat4> worlds{};
    worlds.reserve(boneNodes.size());
    for (const int node : boneNodes) {
        if (node == ri::scene::kInvalidHandle) {
            continue;
        }
        const std::string& name = scene.GetNode(node).name;
        if (!name.empty()) {
            worlds[name] = scene.ComputeWorldMatrix(node);
        }
    }
    return worlds;
}

void SkinCompanionSculptToPose(
    ForgePreviewBuildResult& result,
    const std::unordered_map<std::string, ri::math::Mat4>& restWorld,
    const std::unordered_map<std::string, ri::math::Mat4>& posedWorld) {
    if (result.sculptNode == ri::scene::kInvalidHandle || result.companionSculptPath.empty()) {
        return;
    }
    const auto sculpt = ri::content::LoadNativeSculptDocument(result.companionSculptPath);
    if (!sculpt.has_value() || sculpt->vertexBoneNames.empty()) {
        return;
    }
    ri::scene::Mesh displayed = sculpt->mesh;
    (void)ri::scene::SkinRigidMesh(
        displayed,
        sculpt->vertexBoneNames,
        sculpt->mesh.positions,
        sculpt->mesh.normals,
        restWorld,
        posedWorld,
        sculpt->vertexInfluences);
    // Guard against a bad bind posing verts into giant screen-filling slabs.
    float peak = 0.0F;
    for (const ri::math::Vec3& position : displayed.positions) {
        peak = std::max(peak, std::max(std::abs(position.x), std::max(std::abs(position.y), std::abs(position.z))));
    }
    if (peak > 40.0F) {
        displayed = sculpt->mesh;
        result.status += " | skin unbound (rest)";
    }
    ri::scene::WriteNativeSculptMesh(result.scene, result.sculptNode, displayed);
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
    } else if (kind == AssetKind::BlockCharacter) {
        const auto blockChar = ri::content::LoadBlockCharacterDocument(assetPath);
        if (!blockChar.has_value()) {
            result.status = "Block character could not be loaded";
        } else {
            const std::vector<ri::scene::PsxBlockPartDesc> parts =
                ri::scene::BlockCharacterPartsToPsxDescs(*blockChar);
            // Assets/Source/blockchars/x → Assets/Textures
            const fs::path textureRoot =
                assetPath.parent_path().parent_path().parent_path() / "Textures";
            std::size_t partIndex = 0;
            for (const ri::scene::PsxBlockPartDesc& part : parts) {
                const int node = AttachPsxPartMesh(
                    result.scene, previewRoot, part, part.center, partIndex, textureRoot);
                ++partIndex;
                if (node != ri::scene::kInvalidHandle) {
                    frameNodes.push_back(node);
                    if (result.sculptNode == ri::scene::kInvalidHandle) {
                        result.sculptNode = node;
                    }
                }
            }
            result.companionIsRigidBlocks = !frameNodes.empty();
            result.assetLoaded = !frameNodes.empty();
            result.status = "Authored block character (" + std::to_string(frameNodes.size())
                + " parts)";
            if (!blockChar->rigPath.empty()) {
                OverlayBoundRig(result, previewRoot, frameNodes, blockChar->rigPath, assetPath);
            }
        }
    } else if (kind == AssetKind::Sculpt) {
        const auto sculpt = ri::content::LoadNativeSculptDocument(assetPath);
        if (!sculpt.has_value()) {
            result.status = "Native sculpt could not be loaded";
        } else if (sculpt->cage == "psx") {
            // Static PSX preview: world-space Lit parts from authored .ri_blockchar.json.
            const std::vector<ri::scene::PsxBlockPartDesc> parts =
                LoadPsxPartsForSculpt(assetPath, *sculpt);
            const fs::path textureRoot =
                assetPath.parent_path().parent_path().parent_path() / "Textures";
            std::size_t partIndex = 0;
            for (const ri::scene::PsxBlockPartDesc& part : parts) {
                const int node = AttachPsxPartMesh(
                    result.scene, previewRoot, part, part.center, partIndex, textureRoot);
                ++partIndex;
                if (node != ri::scene::kInvalidHandle) {
                    frameNodes.push_back(node);
                    if (result.sculptNode == ri::scene::kInvalidHandle) {
                        result.sculptNode = node;
                    }
                }
            }
            result.companionIsRigidBlocks = !frameNodes.empty();
            result.companionSculptPath = assetPath;
            result.assetLoaded = !frameNodes.empty();
            result.status = parts.empty()
                ? "PSX sculpt missing .ri_blockchar.json (set blockCharPath)"
                : ("PSX block character (" + std::to_string(frameNodes.size()) + " parts)");
            // No skeleton chrome — the parts are the character.
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
                const std::string preferredStem = CharacterStemFromClipPath(assetPath);
                const fs::path companionPath =
                    FindCompanionSculptForRig(assetPath, clip->rigPath, preferredStem);
                const auto companionSculpt = companionPath.empty()
                    ? std::optional<ri::content::NativeSculptDocument>{}
                    : ri::content::LoadNativeSculptDocument(companionPath);
                const bool psxClay =
                    companionSculpt.has_value() && companionSculpt->cage == "psx";
                const bool willAttachClay = companionSculpt.has_value();
                // PSX uses bone-parented cubes; hide joint/shaft chrome. Soft clay keeps joints off too.
                const SkeletonPreview skeleton = InstantiateRigSkeleton(
                    result.scene,
                    previewRoot,
                    *rig,
                    SkeletonPreviewOptions{
                        .drawJoints = !willAttachClay,
                        .drawShafts = !willAttachClay,
                    });
                frameNodes = skeleton.frameNodes;
                result.boneNodes = skeleton.boneNodes;
                result.boneCount = rig->bones.size();
                result.restBoneWorld = CollectBoneWorldMatrices(result.scene, result.boneNodes);
                for (const int node : result.boneNodes) {
                    if (node == ri::scene::kInvalidHandle) {
                        continue;
                    }
                    const std::string& name = result.scene.GetNode(node).name;
                    if (!name.empty()) {
                        result.restBoneLocal[name] = result.scene.GetNode(node).localTransform;
                    }
                }
                bool attachedClay = false;
                if (psxClay) {
                    attachedClay = AttachPsxBlockPartsToBones(
                        result, frameNodes, assetPath, clip->rigPath, result.restBoneWorld);
                } else if (willAttachClay) {
                    attachedClay = AttachCompanionSculpt(
                        result, previewRoot, frameNodes, assetPath, clip->rigPath);
                }
                const ri::scene::AnimationClip bound =
                    ri::scene::BindNativeAnimationClip(*clip, result.scene, result.boneNodes);
                ri::scene::ApplyAnimationClip(result.scene, bound, 0.0);
                if (attachedClay && !psxClay) {
                    SkinCompanionSculptToPose(
                        result,
                        result.restBoneWorld,
                        CollectBoneWorldMatrices(result.scene, result.boneNodes));
                }
                result.assetLoaded =
                    !frameNodes.empty() || result.sculptNode != ri::scene::kInvalidHandle;
                result.status = "Motion clip (" + clip->displayName + ", "
                    + std::to_string(clip->tracks.size()) + " tracks)";
                if (attachedClay) {
                    result.status += psxClay ? " | psx scout " : " | clay ";
                    result.status += result.companionSculptPath.filename().string();
                }
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
    // Smaller stage chrome when a character mesh is the focus.
    if (result.sculptNode != ri::scene::kInvalidHandle) {
        // Temporarily shrink default helpers via a tighter frame padding.
        AddPreviewStage(result, previewRoot, frameNodes);
        if (result.axesNode != ri::scene::kInvalidHandle
            && result.axesNode >= 0
            && static_cast<std::size_t>(result.axesNode) < result.scene.NodeCount()) {
            result.scene.GetNode(result.axesNode).localTransform.scale = {0.35F, 0.35F, 0.35F};
        }
    } else {
        AddPreviewStage(result, previewRoot, frameNodes);
    }
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
