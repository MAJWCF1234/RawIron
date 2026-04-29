#include "RawIron/Scene/ModelLoader.h"

#include "RawIron/Scene/FbxLoader.h"
#include "RawIron/Scene/GltfLoader.h"

#include <ufbx.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <string_view>
#include <vector>

namespace ri::scene {

namespace {

using UfbxScenePtr = std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)>;

std::string Lowercase(std::filesystem::path value) {
    std::string text = value.generic_string();
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::optional<ModelImportBackend> ResolveModelImportBackend(const ImportedModelOptions& options, std::string& error) {
    if (options.backend != ModelImportBackend::Auto) {
        error.clear();
        return options.backend;
    }

    const std::string extension = Lowercase(options.sourcePath.extension());
    if (extension == ".obj") {
        error.clear();
        return ModelImportBackend::WavefrontObj;
    }
    if (extension == ".gltf" || extension == ".glb") {
        error.clear();
        return ModelImportBackend::Gltf;
    }
    if (extension == ".fbx") {
        error.clear();
        return ModelImportBackend::Fbx;
    }

    error = "Unsupported model extension '" + options.sourcePath.extension().string() + "'.";
    return std::nullopt;
}

std::string_view BackendToString(ModelImportBackend backend) {
    switch (backend) {
        case ModelImportBackend::WavefrontObj:
            return "obj";
        case ModelImportBackend::Gltf:
            return "gltf";
        case ModelImportBackend::Fbx:
            return "fbx";
        case ModelImportBackend::Auto:
        default:
            return "auto";
    }
}

int AddPlaceholderModelNode(Scene& scene, const ImportedModelOptions& options) {
    const int material = scene.AddMaterial(Material{
        .name = options.nodeName + "_PlaceholderMaterial",
        .shadingModel = ShadingModel::Unlit,
        .baseColor = ri::math::Vec3{1.0f, 0.0f, 1.0f},
        .emissiveColor = ri::math::Vec3{0.06f, 0.0f, 0.06f},
        .roughness = 1.0f,
    });
    const int meshHandle = scene.AddMesh(Mesh{
        .name = options.nodeName + "_PlaceholderMesh",
        .primitive = PrimitiveType::Cube,
    });
    const int node = scene.CreateNode(options.nodeName.empty() ? "MissingModelPlaceholder" : options.nodeName, options.parent);
    scene.GetNode(node).localTransform = options.transform;
    scene.AttachMesh(node, meshHandle, material);
    return node;
}

std::vector<ModelImportBackend> ResolveModelImportCandidates(const ImportedModelOptions& options, std::string& error) {
    std::vector<ModelImportBackend> candidates;
    auto appendUnique = [&candidates](ModelImportBackend backend) {
        if (backend == ModelImportBackend::Auto) {
            return;
        }
        if (std::find(candidates.begin(), candidates.end(), backend) == candidates.end()) {
            candidates.push_back(backend);
        }
    };

    if (options.backend != ModelImportBackend::Auto) {
        appendUnique(options.backend);
        error.clear();
    } else {
        const std::optional<ModelImportBackend> resolvedBackend = ResolveModelImportBackend(options, error);
        if (!resolvedBackend.has_value()) {
            return {};
        }
        appendUnique(*resolvedBackend);
    }

    if (!options.lockToPrimaryBackend) {
        for (const ModelImportBackend backend : options.fallbackBackends) {
            appendUnique(backend);
        }
    }
    return candidates;
}

std::string FormatUfbxError(const std::filesystem::path& path, const ufbx_error& error) {
    std::array<char, 512> message{};
    const std::size_t count = ufbx_format_error(message.data(), message.size(), &error);
    std::ostringstream stream;
    stream << "ufbx_load_file failed for " << path.string();
    if (count > 0U) {
        stream << ": " << std::string(message.data(), count);
    }
    return stream.str();
}

ri::math::Vec3 EulerDegreesFromRotation3x3(const ri::math::Mat4& rotationOnly) {
    const float sy = std::sqrt(rotationOnly.m[0][0] * rotationOnly.m[0][0] +
                               rotationOnly.m[1][0] * rotationOnly.m[1][0]);
    constexpr float kRadToDeg = 180.0f / ri::math::kPi;
    if (sy > 1.0e-6f) {
        const float x = std::atan2(rotationOnly.m[2][1], rotationOnly.m[2][2]);
        const float y = std::atan2(-rotationOnly.m[2][0], sy);
        const float z = std::atan2(rotationOnly.m[1][0], rotationOnly.m[0][0]);
        return ri::math::Vec3{x * kRadToDeg, y * kRadToDeg, z * kRadToDeg};
    }

    const float x = std::atan2(-rotationOnly.m[1][2], rotationOnly.m[1][1]);
    const float y = std::atan2(-rotationOnly.m[2][0], sy);
    return ri::math::Vec3{x * kRadToDeg, y * kRadToDeg, 0.0f};
}

Transform CombineTransforms(const Transform& parent, const Transform& local) {
    const ri::math::Mat4 combinedMatrix = ri::math::Multiply(parent.LocalMatrix(), local.LocalMatrix());
    Transform combined{};
    combined.position = ri::math::ExtractTranslation(combinedMatrix);
    combined.scale = ri::math::ExtractScale(combinedMatrix);

    ri::math::Mat4 rotationOnly = combinedMatrix;
    for (int column = 0; column < 3; ++column) {
        const float scaleValue =
            column == 0 ? combined.scale.x : (column == 1 ? combined.scale.y : combined.scale.z);
        const float inverseScale = std::fabs(scaleValue) > 1.0e-8f ? 1.0f / scaleValue : 0.0f;
        rotationOnly.m[0][column] *= inverseScale;
        rotationOnly.m[1][column] *= inverseScale;
        rotationOnly.m[2][column] *= inverseScale;
    }
    combined.rotationDegrees = EulerDegreesFromRotation3x3(rotationOnly);
    return combined;
}

ri::math::Vec3 ToVec3(ufbx_vec3 value) {
    return ri::math::Vec3{
        static_cast<float>(value.x),
        static_cast<float>(value.y),
        static_cast<float>(value.z),
    };
}

ri::math::Vec2 ToVec2(ufbx_vec2 value) {
    return ri::math::Vec2{
        static_cast<float>(value.x),
        static_cast<float>(value.y),
    };
}

bool AppendObjMesh(const ufbx_node* node,
                   std::vector<ri::math::Vec3>& positions,
                   std::vector<ri::math::Vec2>& texCoords,
                   std::vector<int>& indices,
                   bool& anyTexCoords,
                   std::string& error) {
    if (node == nullptr || node->mesh == nullptr) {
        error.clear();
        return true;
    }

    const ufbx_mesh* sourceMesh = node->mesh;
    if (!sourceMesh->vertex_position.exists) {
        error = "OBJ mesh is missing vertex positions.";
        return false;
    }

    std::vector<uint32_t> triangleIndices(sourceMesh->max_face_triangles * 3U);
    const bool hasTexCoords = sourceMesh->vertex_uv.exists;
    if (hasTexCoords && !anyTexCoords && !positions.empty()) {
        texCoords.resize(positions.size(), ri::math::Vec2{0.0f, 0.0f});
        anyTexCoords = true;
    }

    for (std::size_t faceIndex = 0; faceIndex < sourceMesh->faces.count; ++faceIndex) {
        const ufbx_face& face = sourceMesh->faces[faceIndex];
        const uint32_t triangleCount =
            ufbx_triangulate_face(triangleIndices.data(), triangleIndices.size(), sourceMesh, face);
        for (uint32_t triangleVertex = 0; triangleVertex < triangleCount * 3U; ++triangleVertex) {
            const uint32_t meshIndex = triangleIndices[triangleVertex];
            if (meshIndex >= sourceMesh->vertex_position.indices.count) {
                error = "OBJ mesh produced an out-of-range vertex index while triangulating.";
                return false;
            }

            positions.push_back(ToVec3(ufbx_transform_position(&node->geometry_to_world,
                                                               ufbx_get_vertex_vec3(&sourceMesh->vertex_position,
                                                                                    meshIndex))));
            if (hasTexCoords) {
                texCoords.push_back(ToVec2(ufbx_get_vertex_vec2(&sourceMesh->vertex_uv, meshIndex)));
                anyTexCoords = true;
            } else if (anyTexCoords) {
                texCoords.push_back(ri::math::Vec2{0.0f, 0.0f});
            }
            indices.push_back(static_cast<int>(indices.size()));
        }
    }

    error.clear();
    return true;
}

bool AppendObjGeometryRecursive(const ufbx_node* node,
                                std::vector<ri::math::Vec3>& positions,
                                std::vector<ri::math::Vec2>& texCoords,
                                std::vector<int>& indices,
                                bool& anyTexCoords,
                                std::string& error) {
    if (node == nullptr) {
        error = "Null OBJ node.";
        return false;
    }

    if (!AppendObjMesh(node, positions, texCoords, indices, anyTexCoords, error)) {
        return false;
    }

    for (std::size_t childIndex = 0; childIndex < node->children.count; ++childIndex) {
        if (!AppendObjGeometryRecursive(node->children[childIndex], positions, texCoords, indices, anyTexCoords, error)) {
            return false;
        }
    }

    error.clear();
    return true;
}

} // namespace

std::vector<ModelImportBackend> BuildExternalModelCandidateTypes(const ImportedModelOptions& options, std::string* error) {
    std::string localError;
    std::vector<ModelImportBackend> candidates = ResolveModelImportCandidates(options, localError);
    if (error != nullptr) {
        *error = localError;
    }
    return candidates;
}

std::optional<Mesh> LoadWavefrontObjMesh(const std::filesystem::path& path, std::string& error) {
    ufbx_load_opts loadOptions{};
    loadOptions.file_format = UFBX_FILE_FORMAT_OBJ;
    loadOptions.load_external_files = true;
    loadOptions.ignore_missing_external_files = true;
    loadOptions.generate_missing_normals = true;
    loadOptions.obj_search_mtl_by_filename = true;

    const std::string pathString = path.string();
    ufbx_error loadError{};
    UfbxScenePtr loadedScene(ufbx_load_file(pathString.c_str(), &loadOptions, &loadError), &ufbx_free_scene);
    if (!loadedScene) {
        error = FormatUfbxError(path, loadError);
        return std::nullopt;
    }

    std::vector<ri::math::Vec3> positions;
    std::vector<ri::math::Vec2> texCoords;
    std::vector<int> indices;
    bool anyTexCoords = false;

    if (loadedScene->root_node == nullptr) {
        error = "OBJ file does not contain a scene root: " + path.string();
        return std::nullopt;
    }

    for (std::size_t childIndex = 0; childIndex < loadedScene->root_node->children.count; ++childIndex) {
        if (!AppendObjGeometryRecursive(loadedScene->root_node->children[childIndex],
                                        positions,
                                        texCoords,
                                        indices,
                                        anyTexCoords,
                                        error)) {
            return std::nullopt;
        }
    }

    if (positions.empty() || indices.empty()) {
        error = "OBJ file did not provide any triangle geometry: " + path.string();
        return std::nullopt;
    }

    error.clear();
    return Mesh{
        .name = path.stem().string(),
        .primitive = PrimitiveType::Custom,
        .vertexCount = static_cast<int>(positions.size()),
        .indexCount = static_cast<int>(indices.size()),
        .positions = std::move(positions),
        .texCoords = anyTexCoords ? std::move(texCoords) : std::vector<ri::math::Vec2>{},
        .indices = std::move(indices),
    };
}

int AddModelNode(Scene& scene, const ImportedModelOptions& options, std::string* error) {
    std::string resolveError;
    const std::vector<ModelImportBackend> candidates = BuildExternalModelCandidateTypes(options, &resolveError);
    if (candidates.empty()) {
        if (error != nullptr) {
            *error = resolveError;
        }
        return kInvalidHandle;
    }

    struct AttemptResult {
        ModelImportBackend backend = ModelImportBackend::Auto;
        std::string error;
    };
    std::vector<AttemptResult> failures;

    for (const ModelImportBackend backend : candidates) {
        std::string backendError;
        int result = kInvalidHandle;
        switch (backend) {
            case ModelImportBackend::WavefrontObj:
                result = AddWavefrontObjNode(scene,
                                             ModelNodeOptions{
                                                 .sourcePath = options.sourcePath,
                                                 .nodeName = options.nodeName,
                                                 .parent = options.parent,
                                                 .transform = options.transform,
                                             },
                                             &backendError);
                break;
            case ModelImportBackend::Gltf:
                result = AddGltfModelNode(scene,
                                          GltfModelOptions{
                                              .sourcePath = options.sourcePath,
                                              .wrapperNodeName = options.nodeName,
                                              .parent = options.parent,
                                              .transform = options.transform,
                                              .sceneIndex = options.sceneIndex,
                                              .importCameras = options.importCameras,
                                              .importLights = options.importLights,
                                          },
                                          &backendError);
                break;
            case ModelImportBackend::Fbx:
                result = AddFbxModelNode(scene,
                                         FbxModelOptions{
                                             .sourcePath = options.sourcePath,
                                             .wrapperNodeName = options.nodeName,
                                             .parent = options.parent,
                                             .transform = options.transform,
                                         },
                                         &backendError);
                break;
            case ModelImportBackend::Auto:
                backendError = "auto backend is not importable";
                break;
        }

        if (result != kInvalidHandle) {
            if (error != nullptr) {
                error->clear();
            }
            return result;
        }

        failures.push_back(AttemptResult{
            .backend = backend,
            .error = backendError.empty() ? "import failed" : backendError,
        });
    }

    std::string failureSummary;
    {
        std::ostringstream stream;
        stream << "Model import failed for " << options.sourcePath.string() << " after " << failures.size()
               << " backend attempt(s).";
        for (const AttemptResult& failure : failures) {
            stream << " [" << BackendToString(failure.backend) << ": " << failure.error << "]";
        }
        failureSummary = stream.str();
    }
    if (options.createPlaceholderOnFailure) {
        const int placeholderNode = AddPlaceholderModelNode(scene, options);
        if (error != nullptr) {
            *error = failureSummary + " Placeholder model created.";
        }
        return placeholderNode;
    }

    if (error != nullptr) {
        *error = std::move(failureSummary);
    }
    return kInvalidHandle;
}

int AddWavefrontObjNode(Scene& scene, const ModelNodeOptions& options, std::string* error) {
    std::string localError;
    const std::optional<Mesh> mesh = LoadWavefrontObjMesh(options.sourcePath, localError);
    if (!mesh.has_value()) {
        if (error != nullptr) {
            *error = localError;
        }
        return kInvalidHandle;
    }

    const int material = scene.AddMaterial(Material{
        .name = options.materialName,
        .shadingModel = options.shadingModel,
        .baseColor = options.baseColor,
        .transparent = options.transparent,
    });
    const int meshHandle = scene.AddMesh(*mesh);
    const int node = scene.CreateNode(options.nodeName, options.parent);
    scene.GetNode(node).localTransform = options.transform;
    scene.AttachMesh(node, meshHandle, material);

    if (error != nullptr) {
        error->clear();
    }
    return node;
}

int AddGltfModelNode(Scene& scene, const GltfModelOptions& options, std::string* error) {
    std::string localError;
    const int rootHandle = ImportGltfToScene(scene,
                                             options.sourcePath,
                                             GltfImportOptions{
                                                 .parent = options.parent,
                                                 .wrapperNodeName = options.wrapperNodeName,
                                                 .sceneIndex = options.sceneIndex,
                                                 .importCameras = options.importCameras,
                                                 .importLights = options.importLights,
                                             },
                                             localError);
    if (rootHandle == kInvalidHandle) {
        if (error != nullptr) {
            *error = localError;
        }
        return kInvalidHandle;
    }

    scene.GetNode(rootHandle).localTransform = CombineTransforms(options.transform, scene.GetNode(rootHandle).localTransform);
    if (error != nullptr) {
        error->clear();
    }
    return rootHandle;
}

int AddFbxModelNode(Scene& scene, const FbxModelOptions& options, std::string* error) {
    std::string localError;
    const int rootHandle = ImportFbxToScene(scene,
                                            options.sourcePath,
                                            FbxImportOptions{
                                                .parent = options.parent,
                                                .wrapperNodeName = options.wrapperNodeName,
                                            },
                                            localError);
    if (rootHandle == kInvalidHandle) {
        if (error != nullptr) {
            *error = localError;
        }
        return kInvalidHandle;
    }

    scene.GetNode(rootHandle).localTransform = CombineTransforms(options.transform, scene.GetNode(rootHandle).localTransform);
    if (error != nullptr) {
        error->clear();
    }
    return rootHandle;
}

} // namespace ri::scene
