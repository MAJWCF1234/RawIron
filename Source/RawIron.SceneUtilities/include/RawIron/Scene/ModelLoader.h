#pragma once

#include "RawIron/Scene/Helpers.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ri::scene {

struct ModelNodeOptions {
    std::filesystem::path sourcePath;
    std::string nodeName = "Model";
    int parent = kInvalidHandle;
    Transform transform{};
    std::string materialName = "ModelMaterial";
    /// When not invalid, the imported mesh reuses this material instead of creating a new one.
    int existingMaterial = kInvalidHandle;
    /// After import, shift the wrapper so mesh bounds sit on `transform.position.y`.
    bool snapMeshBaseToGround = false;
    ShadingModel shadingModel = ShadingModel::Lit;
    ri::math::Vec3 baseColor{0.72f, 0.76f, 0.84f};
    bool transparent = false;
};

std::optional<Mesh> LoadWavefrontObjMesh(const std::filesystem::path& path, std::string& error);
int AddWavefrontObjNode(Scene& scene, const ModelNodeOptions& options, std::string* error = nullptr);

enum class ModelImportBackend {
    Auto,
    WavefrontObj,
    Gltf,
    Fbx,
};

struct ImportedModelOptions {
    std::filesystem::path sourcePath;
    std::string nodeName = "ImportedModel";
    int parent = kInvalidHandle;
    Transform transform{};
    /// When non-empty, OBJ imports use this material name instead of the default `ModelMaterial`.
    std::string materialName{};
    /// When not invalid, OBJ imports attach this existing material instead of creating a new one.
    int existingMaterial = kInvalidHandle;
    /// Shift imported hierarchy so the lowest mesh point rests on `transform.position.y`.
    bool snapMeshBaseToGround = false;
    ModelImportBackend backend = ModelImportBackend::Auto;
    /// Optional backend attempts after the primary backend fails. Duplicate entries are ignored.
    std::vector<ModelImportBackend> fallbackBackends{};
    /// When true, disables fallback probing and only attempts the resolved/explicit primary backend.
    bool lockToPrimaryBackend = false;
    /// Creates a deterministic placeholder primitive when all import attempts fail.
    bool createPlaceholderOnFailure = false;
    int sceneIndex = -1;
    bool importCameras = false;
    bool importLights = false;
};

/// Returns normalized backend attempt order for model imports (extension/backend resolution + optional fallbacks).
[[nodiscard]] std::vector<ModelImportBackend> BuildExternalModelCandidateTypes(const ImportedModelOptions& options,
                                                                                std::string* error = nullptr);

/// Loads a supported model file under a single entry point, dispatching by extension unless `backend` is explicit.
int AddModelNode(Scene& scene, const ImportedModelOptions& options, std::string* error = nullptr);

struct GltfModelOptions {
    std::filesystem::path sourcePath;
    std::string wrapperNodeName = "GltfModel";
    int parent = kInvalidHandle;
    Transform transform{};
    int sceneIndex = -1;
    bool importCameras = false;
    bool importLights = false;
};

/// Loads `.gltf` / `.glb` under a named wrapper; applies `transform` to the wrapper node.
int AddGltfModelNode(Scene& scene, const GltfModelOptions& options, std::string* error = nullptr);

struct FbxModelOptions {
    std::filesystem::path sourcePath;
    std::string wrapperNodeName = "FbxModel";
    int parent = kInvalidHandle;
    Transform transform{};
    bool snapMeshBaseToGround = false;
};

/// Loads `.fbx` under a named wrapper; applies `transform` to the wrapper node.
int AddFbxModelNode(Scene& scene, const FbxModelOptions& options, std::string* error = nullptr);

} // namespace ri::scene
