#include "RawIron/Scene/GltfLoader.h"

#include "RawIron/Math/Mat4.h"

#include <cgltf.h>

#include <filesystem>
#include <cmath>
#include <cstddef>
#include <limits>
#include <sstream>
#include <string_view>
#include <vector>

namespace ri::scene {

namespace {

std::string_view CgltfResultMessage(cgltf_result result) {
    switch (result) {
        case cgltf_result_success:
            return "success";
        case cgltf_result_data_too_short:
            return "data too short";
        case cgltf_result_unknown_format:
            return "unknown format";
        case cgltf_result_invalid_json:
            return "invalid json";
        case cgltf_result_invalid_gltf:
            return "invalid gltf";
        case cgltf_result_invalid_options:
            return "invalid options";
        case cgltf_result_file_not_found:
            return "file not found";
        case cgltf_result_io_error:
            return "io error";
        case cgltf_result_out_of_memory:
            return "out of memory";
        case cgltf_result_legacy_gltf:
            return "legacy gltf";
        default:
            return "unknown cgltf error";
    }
}

ri::math::Mat4 Mat4FromCgltfColumnMajor(const cgltf_float* columnMajor) {
    ri::math::Mat4 matrix{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            matrix.m[row][column] = columnMajor[column * 4 + row];
        }
    }
    return matrix;
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
    const float z = 0.0f;
    return ri::math::Vec3{x * kRadToDeg, y * kRadToDeg, z * kRadToDeg};
}

float Clamp01(cgltf_float value) {
    return std::clamp(static_cast<float>(value), 0.0f, 1.0f);
}

std::string ResolveTextureUri(const cgltf_texture_view* textureView) {
    if (textureView == nullptr || textureView->texture == nullptr || textureView->texture->image == nullptr) {
        return {};
    }
    const cgltf_image* image = textureView->texture->image;
    if (image->uri == nullptr || image->uri[0] == '\0') {
        return {};
    }
    return std::filesystem::path(image->uri).lexically_normal().generic_string();
}

Transform LocalTransformFromGltfNode(const cgltf_node* node) {
    cgltf_float columnMajor[16]{};
    cgltf_node_transform_local(node, columnMajor);
    const ri::math::Mat4 local = Mat4FromCgltfColumnMajor(columnMajor);
    Transform transform{};
    transform.position = ri::math::ExtractTranslation(local);
    transform.scale = ri::math::ExtractScale(local);
    ri::math::Mat4 rotation = local;
    for (int column = 0; column < 3; ++column) {
        const float scaleValue =
            column == 0 ? transform.scale.x : (column == 1 ? transform.scale.y : transform.scale.z);
        const float inverseScale = scaleValue > 1.0e-8f ? 1.0f / scaleValue : 0.0f;
        rotation.m[0][column] *= inverseScale;
        rotation.m[1][column] *= inverseScale;
        rotation.m[2][column] *= inverseScale;
    }
    transform.rotationDegrees = EulerDegreesFromRotation3x3(rotation);
    return transform;
}

bool BuildMeshFromPrimitive(const cgltf_primitive& primitive,
                            const std::string& meshName,
                            Mesh& outMesh,
                            std::string& error) {
    if (primitive.has_draco_mesh_compression) {
        error = "Draco-compressed meshes are not supported.";
        return false;
    }
    if (primitive.type != cgltf_primitive_type_triangles) {
        error = "Only triangle primitives are supported.";
        return false;
    }

    const cgltf_accessor* positionAccessor = nullptr;
    for (cgltf_size attributeIndex = 0; attributeIndex < primitive.attributes_count; ++attributeIndex) {
        if (primitive.attributes[attributeIndex].type == cgltf_attribute_type_position) {
            positionAccessor = primitive.attributes[attributeIndex].data;
            break;
        }
    }
    if (positionAccessor == nullptr) {
        error = "Primitive is missing a POSITION accessor.";
        return false;
    }
    if (positionAccessor->type != cgltf_type_vec3) {
        error = "POSITION accessor must be VEC3.";
        return false;
    }
    const cgltf_size floatCount = cgltf_accessor_unpack_floats(positionAccessor, nullptr, 0);
    if (floatCount == 0) {
        error = "POSITION accessor could not be decoded (unsupported format or sparse data).";
        return false;
    }
    if (floatCount % 3 != 0) {
        error = "POSITION accessor float count is not a multiple of three.";
        return false;
    }

    std::vector<float> positionFloats(floatCount);
    cgltf_accessor_unpack_floats(positionAccessor, positionFloats.data(), floatCount);

    std::vector<ri::math::Vec3> positions;
    positions.reserve(floatCount / 3);
    for (cgltf_size vertexIndex = 0; vertexIndex < floatCount; vertexIndex += 3) {
        positions.push_back(ri::math::Vec3{positionFloats[vertexIndex + 0],
                                           positionFloats[vertexIndex + 1],
                                           positionFloats[vertexIndex + 2]});
    }

    std::vector<int> indices;
    if (primitive.indices != nullptr) {
        indices.reserve(primitive.indices->count);
        for (cgltf_size index = 0; index < primitive.indices->count; ++index) {
            const cgltf_size vertexIndex = cgltf_accessor_read_index(primitive.indices, index);
            if (vertexIndex > static_cast<cgltf_size>(std::numeric_limits<int>::max())) {
                error = "Index value overflows int.";
                return false;
            }
            if (vertexIndex >= positions.size()) {
                error = "Index value is out of range for POSITION accessor.";
                return false;
            }
            indices.push_back(static_cast<int>(vertexIndex));
        }
    } else {
        for (cgltf_size index = 0; index + 2 < positionAccessor->count; index += 3) {
            indices.push_back(static_cast<int>(index));
            indices.push_back(static_cast<int>(index + 1));
            indices.push_back(static_cast<int>(index + 2));
        }
    }

    if (positions.empty() || indices.empty()) {
        error = "Primitive did not produce triangle geometry.";
        return false;
    }

    outMesh = Mesh{
        .name = meshName,
        .primitive = PrimitiveType::Custom,
        .vertexCount = static_cast<int>(positions.size()),
        .indexCount = static_cast<int>(indices.size()),
        .positions = std::move(positions),
        .indices = std::move(indices),
    };
    error.clear();
    return true;
}

Material MaterialFromGltf(const cgltf_material* material) {
    if (material == nullptr) {
        return Material{
            .name = "GltfDefaultMaterial",
            .shadingModel = ShadingModel::Unlit,
            .baseColor = ri::math::Vec3{0.82f, 0.82f, 0.86f},
            .emissiveColor = ri::math::Vec3{0.0f, 0.0f, 0.0f},
            .metallic = 0.0f,
            .roughness = 1.0f,
            .opacity = 1.0f,
            .alphaCutoff = 0.5f,
            .doubleSided = false,
            .transparent = false,
        };
    }

    ri::math::Vec3 baseColor{0.82f, 0.82f, 0.86f};
    ri::math::Vec3 emissiveColor{material->emissive_factor[0], material->emissive_factor[1], material->emissive_factor[2]};
    float metallic = 0.0f;
    float roughness = 1.0f;
    float opacity = 1.0f;
    float alphaCutoff = 0.5f;
    if (material->has_pbr_metallic_roughness) {
        const cgltf_float* factor = material->pbr_metallic_roughness.base_color_factor;
        baseColor = ri::math::Vec3{factor[0], factor[1], factor[2]};
        opacity = Clamp01(factor[3]);
        metallic = Clamp01(material->pbr_metallic_roughness.metallic_factor);
        roughness = Clamp01(material->pbr_metallic_roughness.roughness_factor);
    }
    if (material->alpha_cutoff > 0.0f) {
        alphaCutoff = Clamp01(material->alpha_cutoff);
    }

    const bool transparent = material->alpha_mode == cgltf_alpha_mode_blend;
    const bool alphaMasked = material->alpha_mode == cgltf_alpha_mode_mask;
    const std::string baseColorTexture = material->has_pbr_metallic_roughness
        ? ResolveTextureUri(&material->pbr_metallic_roughness.base_color_texture)
        : std::string{};
    const std::string ormTexture = material->has_pbr_metallic_roughness
        ? ResolveTextureUri(&material->pbr_metallic_roughness.metallic_roughness_texture)
        : std::string{};
    const std::string normalTexture = ResolveTextureUri(&material->normal_texture);
    const std::string emissiveTexture = ResolveTextureUri(&material->emissive_texture);
    const std::string occlusionTexture = ResolveTextureUri(&material->occlusion_texture);
    const std::string opacityTexture =
        (!baseColorTexture.empty() && (transparent || alphaMasked || opacity < 0.999f)) ? baseColorTexture : std::string{};
    std::string materialName = "GltfMaterial";
    if (material->name != nullptr && material->name[0] != '\0') {
        materialName = material->name;
    }

    return Material{
        .name = std::move(materialName),
        .shadingModel = material->unlit ? ShadingModel::Unlit : ShadingModel::Lit,
        .baseColor = baseColor,
        .baseColorTexture = baseColorTexture,
        .emissiveColor = emissiveColor,
        .metallic = metallic,
        .roughness = roughness,
        .opacity = opacity,
        .alphaCutoff = alphaCutoff,
        .doubleSided = material->double_sided != 0,
        .transparent = transparent,
        .normalTexture = normalTexture,
        .ormTexture = ormTexture,
        .roughnessTexture = {},
        .metallicTexture = {},
        .emissiveTexture = emissiveTexture,
        .opacityTexture = opacityTexture,
        .occlusionTexture = occlusionTexture,
    };
}

bool BuildCameraFromGltf(const cgltf_camera* camera, Camera& out, std::string& error) {
    if (camera == nullptr) {
        error = "Null glTF camera.";
        return false;
    }
    if (camera->type != cgltf_camera_type_perspective) {
        error = "Only perspective glTF cameras are imported.";
        return false;
    }

    const cgltf_camera_perspective& perspective = camera->data.perspective;
    out.name = camera->name != nullptr && camera->name[0] != '\0' ? camera->name : "GltfCamera";
    out.projection = ProjectionType::Perspective;
    out.fieldOfViewDegrees = perspective.yfov * (180.0f / ri::math::kPi);
    out.nearClip = perspective.znear;
    out.farClip = perspective.has_zfar != 0 ? perspective.zfar : 1000.0f;
    error.clear();
    return true;
}

bool BuildLightFromGltf(const cgltf_light* light, Light& out, std::string& error) {
    if (light == nullptr) {
        error = "Null glTF light.";
        return false;
    }

    out.name = light->name != nullptr && light->name[0] != '\0' ? light->name : "GltfLight";
    out.color = ri::math::Vec3{light->color[0], light->color[1], light->color[2]};
    out.intensity = light->intensity > 0.0f ? light->intensity : 1.0f;

    switch (light->type) {
        case cgltf_light_type_directional:
            out.type = LightType::Directional;
            out.range = 0.0f;
            break;
        case cgltf_light_type_point:
            out.type = LightType::Point;
            out.range = light->range > 0.0f ? light->range : 1.0e4f;
            break;
        case cgltf_light_type_spot:
            out.type = LightType::Spot;
            out.range = light->range > 0.0f ? light->range : 1.0e4f;
            out.spotAngleDegrees = light->spot_outer_cone_angle * (180.0f / ri::math::kPi);
            break;
        default:
            error = "Unsupported glTF light type.";
            return false;
    }

    error.clear();
    return true;
}

int ImportGltfNodeRecursive(Scene& scene,
                            const cgltf_node* node,
                            int parentHandle,
                            const GltfImportOptions& importOptions,
                            std::string& error);

bool ImportMeshesForGltfNode(Scene& scene, const cgltf_node* node, int parentHandle, std::string& error) {
    if (node->mesh == nullptr) {
        error.clear();
        return true;
    }

    for (cgltf_size primitiveIndex = 0; primitiveIndex < node->mesh->primitives_count; ++primitiveIndex) {
        const cgltf_primitive& primitive = node->mesh->primitives[primitiveIndex];
        std::string meshPieceName = node->mesh->name != nullptr ? node->mesh->name : "Mesh";
        meshPieceName += "_Prim";
        meshPieceName += std::to_string(static_cast<int>(primitiveIndex));

        Mesh builtMesh{};
        if (!BuildMeshFromPrimitive(primitive, meshPieceName, builtMesh, error)) {
            return false;
        }

        const int materialHandle = scene.AddMaterial(MaterialFromGltf(primitive.material));
        const int meshHandle = scene.AddMesh(std::move(builtMesh));
        const int primitiveNode = scene.CreateNode(meshPieceName + "_Node", parentHandle);
        scene.GetNode(primitiveNode).localTransform = Transform{};
        scene.AttachMesh(primitiveNode, meshHandle, materialHandle);
    }

    error.clear();
    return true;
}

int ImportGltfNodeRecursive(Scene& scene,
                            const cgltf_node* node,
                            int parentHandle,
                            const GltfImportOptions& importOptions,
                            std::string& error) {
    if (node == nullptr) {
        error = "Null glTF node.";
        return kInvalidHandle;
    }

    std::string nodeName = node->name != nullptr && node->name[0] != '\0' ? node->name : "GltfNode";
    const int groupHandle = scene.CreateNode(std::move(nodeName), parentHandle);
    scene.GetNode(groupHandle).localTransform = LocalTransformFromGltfNode(node);

    if (importOptions.importCameras && node->camera != nullptr) {
        Camera camera{};
        if (!BuildCameraFromGltf(node->camera, camera, error)) {
            return kInvalidHandle;
        }
        const int cameraHandle = scene.AddCamera(std::move(camera));
        scene.AttachCamera(groupHandle, cameraHandle);
    }

    if (importOptions.importLights && node->light != nullptr) {
        Light light{};
        if (!BuildLightFromGltf(node->light, light, error)) {
            return kInvalidHandle;
        }
        const int lightHandle = scene.AddLight(std::move(light));
        scene.AttachLight(groupHandle, lightHandle);
    }

    if (!ImportMeshesForGltfNode(scene, node, groupHandle, error)) {
        return kInvalidHandle;
    }

    for (cgltf_size childIndex = 0; childIndex < node->children_count; ++childIndex) {
        const int childHandle =
            ImportGltfNodeRecursive(scene, node->children[childIndex], groupHandle, importOptions, error);
        if (childHandle == kInvalidHandle) {
            return kInvalidHandle;
        }
    }

    error.clear();
    return groupHandle;
}

} // namespace

int ImportGltfToScene(Scene& targetScene,
                      const std::filesystem::path& path,
                      const GltfImportOptions& options,
                      std::string& error) {
    cgltf_options parseOptions{};
    cgltf_data* data = nullptr;

    const std::string pathString = path.string();
    cgltf_result parseResult = cgltf_parse_file(&parseOptions, pathString.c_str(), &data);
    if (parseResult != cgltf_result_success || data == nullptr) {
        std::ostringstream stream;
        stream << "cgltf_parse_file failed (" << CgltfResultMessage(parseResult) << "): " << pathString;
        error = stream.str();
        if (data != nullptr) {
            cgltf_free(data);
        }
        return kInvalidHandle;
    }

    parseResult = cgltf_load_buffers(&parseOptions, data, pathString.c_str());
    if (parseResult != cgltf_result_success) {
        cgltf_free(data);
        std::ostringstream stream;
        stream << "cgltf_load_buffers failed (" << CgltfResultMessage(parseResult) << ").";
        error = stream.str();
        return kInvalidHandle;
    }

    const cgltf_scene* gltfScene = nullptr;
    if (options.sceneIndex >= 0) {
        if (data->scenes_count == 0U ||
            static_cast<cgltf_size>(options.sceneIndex) >= data->scenes_count) {
            cgltf_free(data);
            error = "glTF scene index is out of range.";
            return kInvalidHandle;
        }
        gltfScene = &data->scenes[options.sceneIndex];
    } else if (data->scene != nullptr) {
        gltfScene = data->scene;
    } else if (data->scenes_count > 0) {
        gltfScene = &data->scenes[0];
    }

    if (gltfScene == nullptr || gltfScene->nodes_count == 0) {
        cgltf_free(data);
        error = "glTF file does not contain a scene with root nodes.";
        return kInvalidHandle;
    }

    int importParent = options.parent;
    if (!options.wrapperNodeName.empty()) {
        importParent = targetScene.CreateNode(options.wrapperNodeName, options.parent);
        targetScene.GetNode(importParent).localTransform = Transform{};
    }

    int firstRoot = kInvalidHandle;
    for (cgltf_size nodeIndex = 0; nodeIndex < gltfScene->nodes_count; ++nodeIndex) {
        const int rootHandle =
            ImportGltfNodeRecursive(targetScene, gltfScene->nodes[nodeIndex], importParent, options, error);
        if (rootHandle == kInvalidHandle) {
            cgltf_free(data);
            return kInvalidHandle;
        }
        if (firstRoot == kInvalidHandle) {
            firstRoot = rootHandle;
        }
    }

    cgltf_free(data);

    error.clear();
    if (!options.wrapperNodeName.empty()) {
        return importParent;
    }
    return firstRoot;
}

} // namespace ri::scene
