#include "RawIron/Scene/GltfLoader.h"

#include "RawIron/Math/Mat4.h"
#include "RawIron/Core/Log.h"

#include <cgltf.h>
#include <meshoptimizer.h>

#include <filesystem>
#include <cmath>
#include <cstddef>
#include <limits>
#include <sstream>
#include <string_view>
#include <vector>

namespace ri::scene {

namespace {

bool DecodeMeshoptBufferViews(cgltf_data& data, std::string& error) {
    for (cgltf_size viewIndex = 0; viewIndex < data.buffer_views_count; ++viewIndex) {
        cgltf_buffer_view& view = data.buffer_views[viewIndex];
        if (!view.has_meshopt_compression) {
            continue;
        }

        const cgltf_meshopt_compression& compression = view.meshopt_compression;
        if (compression.buffer == nullptr || compression.buffer->data == nullptr) {
            error = "EXT_meshopt_compression references an unloaded source buffer.";
            return false;
        }
        if (compression.offset > compression.buffer->size
            || compression.size > compression.buffer->size - compression.offset) {
            error = "EXT_meshopt_compression source range exceeds its buffer.";
            return false;
        }
        if (compression.count == 0U || compression.stride == 0U
            || compression.count > std::numeric_limits<std::size_t>::max() / compression.stride) {
            error = "EXT_meshopt_compression declares an invalid decoded size.";
            return false;
        }

        const auto* source = static_cast<const unsigned char*>(compression.buffer->data)
            + compression.offset;
        const std::size_t decodedSize = compression.count * compression.stride;
        auto* decoded = static_cast<unsigned char*>(
            data.memory.alloc_func(data.memory.user_data, decodedSize));
        if (decoded == nullptr) {
            error = "Could not allocate an EXT_meshopt_compression decode buffer.";
            return false;
        }

        int decodeResult = -1;
        switch (compression.mode) {
            case cgltf_meshopt_compression_mode_attributes:
                decodeResult = meshopt_decodeVertexBuffer(decoded,
                                                          compression.count,
                                                          compression.stride,
                                                          source,
                                                          compression.size);
                break;
            case cgltf_meshopt_compression_mode_triangles:
                decodeResult = meshopt_decodeIndexBuffer(decoded,
                                                         compression.count,
                                                         compression.stride,
                                                         source,
                                                         compression.size);
                break;
            case cgltf_meshopt_compression_mode_indices:
                decodeResult = meshopt_decodeIndexSequence(decoded,
                                                           compression.count,
                                                           compression.stride,
                                                           source,
                                                           compression.size);
                break;
            default:
                data.memory.free_func(data.memory.user_data, decoded);
                error = "EXT_meshopt_compression uses an unsupported mode.";
                return false;
        }
        if (decodeResult != 0) {
            data.memory.free_func(data.memory.user_data, decoded);
            error = "meshoptimizer rejected an EXT_meshopt_compression payload in buffer view "
                + std::to_string(viewIndex) + ".";
            return false;
        }

        switch (compression.filter) {
            case cgltf_meshopt_compression_filter_none:
                break;
            case cgltf_meshopt_compression_filter_octahedral:
                meshopt_decodeFilterOct(decoded, compression.count, compression.stride);
                break;
            case cgltf_meshopt_compression_filter_quaternion:
                meshopt_decodeFilterQuat(decoded, compression.count, compression.stride);
                break;
            case cgltf_meshopt_compression_filter_exponential:
                meshopt_decodeFilterExp(decoded, compression.count, compression.stride);
                break;
            case cgltf_meshopt_compression_filter_color:
                meshopt_decodeFilterColor(decoded, compression.count, compression.stride);
                break;
            default:
                data.memory.free_func(data.memory.user_data, decoded);
                error = "EXT_meshopt_compression uses an unsupported decode filter.";
                return false;
        }

        // cgltf_buffer_view_data prefers this decoded pointer over the original buffer range,
        // and cgltf_free releases it through the same allocator used above.
        view.data = decoded;
        view.size = decodedSize;
        view.stride = compression.stride;
    }

    return true;
}

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

std::string ResolveTextureUri(const cgltf_texture_view* textureView,
                              const std::filesystem::path& sourceDirectory) {
    if (textureView == nullptr || textureView->texture == nullptr || textureView->texture->image == nullptr) {
        return {};
    }
    const cgltf_image* image = textureView->texture->image;
    if (image->uri == nullptr || image->uri[0] == '\0') {
        return {};
    }
    const std::string_view uri = image->uri;
    if (uri.starts_with("data:")) {
        return std::string(uri);
    }
    const std::filesystem::path texturePath(image->uri);
    return (texturePath.is_absolute() ? texturePath : sourceDirectory / texturePath)
        .lexically_normal().string();
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

    std::vector<ri::math::Vec3> normals;
    std::vector<ri::math::Vec2> texCoords;
    for (cgltf_size attributeIndex = 0; attributeIndex < primitive.attributes_count; ++attributeIndex) {
        const cgltf_attribute& attribute = primitive.attributes[attributeIndex];
        if (attribute.data == nullptr || attribute.index != 0) {
            continue;
        }
        if (attribute.type == cgltf_attribute_type_normal && attribute.data->type == cgltf_type_vec3) {
            const cgltf_size count = cgltf_accessor_unpack_floats(attribute.data, nullptr, 0);
            if (count == positions.size() * 3U) {
                std::vector<float> unpacked(count);
                cgltf_accessor_unpack_floats(attribute.data, unpacked.data(), count);
                normals.reserve(positions.size());
                for (cgltf_size value = 0; value < count; value += 3) {
                    normals.push_back({unpacked[value], unpacked[value + 1], unpacked[value + 2]});
                }
            }
        } else if (attribute.type == cgltf_attribute_type_texcoord && attribute.data->type == cgltf_type_vec2) {
            const cgltf_size count = cgltf_accessor_unpack_floats(attribute.data, nullptr, 0);
            if (count == positions.size() * 2U) {
                std::vector<float> unpacked(count);
                cgltf_accessor_unpack_floats(attribute.data, unpacked.data(), count);
                texCoords.reserve(positions.size());
                for (cgltf_size value = 0; value < count; value += 2) {
                    texCoords.push_back({unpacked[value], unpacked[value + 1]});
                }
            }
        }
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
        .normals = std::move(normals),
        .texCoords = std::move(texCoords),
        .indices = std::move(indices),
    };
    error.clear();
    return true;
}

Material MaterialFromGltf(const cgltf_material* material,
                          const std::filesystem::path& sourceDirectory) {
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
        ? ResolveTextureUri(&material->pbr_metallic_roughness.base_color_texture, sourceDirectory)
        : std::string{};
    const std::string ormTexture = material->has_pbr_metallic_roughness
        ? ResolveTextureUri(&material->pbr_metallic_roughness.metallic_roughness_texture, sourceDirectory)
        : std::string{};
    const std::string normalTexture = ResolveTextureUri(&material->normal_texture, sourceDirectory);
    const std::string emissiveTexture = ResolveTextureUri(&material->emissive_texture, sourceDirectory);
    const std::string occlusionTexture = ResolveTextureUri(&material->occlusion_texture, sourceDirectory);
    const std::string opacityTexture =
        (!baseColorTexture.empty() && (transparent || alphaMasked || opacity < 0.999f)) ? baseColorTexture : std::string{};
    std::string materialName = "GltfMaterial";
    if (material->name != nullptr && material->name[0] != '\0') {
        materialName = material->name;
    }
    const auto reportUnsupportedSlot = [&](const char* slot, const cgltf_texture_view& view,
                                            const std::string& resolved, const char* colorSpace) {
        if (view.texture == nullptr || !resolved.empty()) return;
        const auto* sampler = view.texture->sampler;
        ri::core::LogInfo("glTF material fallback: material=" + materialName + " slot=" + slot
            + " colorSpace=" + colorSpace + " sourceDirectory=" + sourceDirectory.generic_string()
            + " sampler.min=" + std::to_string(sampler ? sampler->min_filter : 0)
            + " sampler.mag=" + std::to_string(sampler ? sampler->mag_filter : 0)
            + " sampler.wrapS=" + std::to_string(sampler ? sampler->wrap_s : 10497)
            + " sampler.wrapT=" + std::to_string(sampler ? sampler->wrap_t : 10497)
            + " reason=unsupported image source; see glTF texture fallback blob record");
    };
    reportUnsupportedSlot("albedo", material->pbr_metallic_roughness.base_color_texture, baseColorTexture, "sRGB");
    reportUnsupportedSlot("orm", material->pbr_metallic_roughness.metallic_roughness_texture, ormTexture, "linear");
    reportUnsupportedSlot("normal", material->normal_texture, normalTexture, "linear");
    reportUnsupportedSlot("emissive", material->emissive_texture, emissiveTexture, "sRGB");
    reportUnsupportedSlot("occlusion", material->occlusion_texture, occlusionTexture, "linear");
    // Keep geometry inspection available, but never disguise an unsupported authored
    // base-color image (e.g. embedded KTX2/BasisU) as an ordinary white material.
    if (material->has_pbr_metallic_roughness
        && material->pbr_metallic_roughness.base_color_texture.texture != nullptr && baseColorTexture.empty()) {
        baseColor = {1.0f, 0.0f, 1.0f};
        metallic = 0.0f;
        roughness = 1.0f;
        materialName += " [UNSUPPORTED TEXTURE]";
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
                            const std::filesystem::path& sourceDirectory,
                            std::string& error);

bool ImportMeshesForGltfNode(Scene& scene,
                             const cgltf_node* node,
                             int parentHandle,
                             const std::filesystem::path& sourceDirectory,
                             std::string& error) {
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

        const int materialHandle = scene.AddMaterial(MaterialFromGltf(primitive.material, sourceDirectory));
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
                            const std::filesystem::path& sourceDirectory,
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

    if (!ImportMeshesForGltfNode(scene, node, groupHandle, sourceDirectory, error)) {
        return kInvalidHandle;
    }

    for (cgltf_size childIndex = 0; childIndex < node->children_count; ++childIndex) {
        const int childHandle =
            ImportGltfNodeRecursive(
                scene, node->children[childIndex], groupHandle, importOptions, sourceDirectory, error);
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

    if (!DecodeMeshoptBufferViews(*data, error)) {
        cgltf_free(data);
        return kInvalidHandle;
    }

    // Embedded/extension image payloads are not supported by this URI-only importer.
    // Identify every source blob rather than silently dropping material dependencies.
    for (cgltf_size index = 0; index < data->textures_count; ++index) {
        const auto& texture = data->textures[index];
        const auto* image = texture.image ? texture.image : texture.basisu_image;
        if (image == nullptr) image = texture.webp_image;
        if (image == nullptr || (image->uri != nullptr && image->uri[0] != '\0')) continue;
        const auto* view = image->buffer_view;
        ri::core::LogInfo("glTF texture fallback: source=" + path.generic_string()
            + "#image=" + std::to_string(image - data->images)
            + " texture=" + std::to_string(index)
            + " byteOffset=" + std::to_string(view ? view->offset : 0)
            + " byteLength=" + std::to_string(view ? view->size : 0)
            + " format=" + std::string(image->mime_type ? image->mime_type : "unknown")
            + " dimensions=unavailable colorSpace=slot-dependent sampler=not-created"
              " reason=unsupported embedded/extension image; base-color marker=magenta, data slots=scalar defaults");
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

    // All-or-nothing: mid-import failure must not leave wrapper/partial trees attached.
    const SceneAppendWatermark watermark = targetScene.CaptureAppendWatermark();
    const auto failClosed = [&]() -> int {
        targetScene.TruncateToAppendWatermark(watermark);
        cgltf_free(data);
        return kInvalidHandle;
    };

    int importParent = options.parent;
    if (!options.wrapperNodeName.empty()) {
        importParent = targetScene.CreateNode(options.wrapperNodeName, options.parent);
        targetScene.GetNode(importParent).localTransform = Transform{};
    }

    int firstRoot = kInvalidHandle;
    for (cgltf_size nodeIndex = 0; nodeIndex < gltfScene->nodes_count; ++nodeIndex) {
        const int rootHandle =
            ImportGltfNodeRecursive(
                targetScene, gltfScene->nodes[nodeIndex], importParent, options, path.parent_path(), error);
        if (rootHandle == kInvalidHandle) {
            return failClosed();
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
