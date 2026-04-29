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
    ModelImportBackend backend = ModelImportBackend::Auto;
    /// Optional backend attempts after the primary backend fails. Duplicate entries are ignored.
    std::vector<ModelImportBackend> fallbackBackends{};
    /// Creates a deterministic placeholder primitive when all import attempts fail.
    bool createPlaceholderOnFailure = false;
    int sceneIndex = -1;
    bool importCameras = false;
    bool importLights = false;
};

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
};

/// Loads `.fbx` under a named wrapper; applies `transform` to the wrapper node.
int AddFbxModelNode(Scene& scene, const FbxModelOptions& options, std::string* error = nullptr);

} // namespace ri::scene
