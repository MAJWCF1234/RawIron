#include "RawIron/Scene/UfbxMeshImport.h"

#include <ufbx.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::scene {

namespace {

using UfbxScenePtr = std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)>;

struct UfbxMaterialInternTable {
    std::unordered_map<std::string, int> bySignature;
};

[[nodiscard]] std::string BuildMaterialSignature(const Material& material) {
    return material.name + "|bc=" + material.baseColorTexture + "|n=" + material.normalTexture + "|orm="
        + material.ormTexture + "|smoothA=" + (material.albedoAlphaIsSmoothness ? "1" : "0");
}

[[nodiscard]] int InternUfbxMaterial(Scene& scene, Material material, UfbxMaterialInternTable& table) {
    const std::string signature = BuildMaterialSignature(material);
    const auto existing = table.bySignature.find(signature);
    if (existing != table.bySignature.end()) {
        return existing->second;
    }
    const int handle = scene.AddMaterial(std::move(material));
    table.bySignature.emplace(signature, handle);
    return handle;
}

std::string ToString(ufbx_string value, std::string_view fallback) {
    if (value.data != nullptr && value.length > 0U) {
        return std::string(value.data, value.length);
    }
    return std::string(fallback);
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

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool IsImageExtension(const std::filesystem::path& path) {
    const std::string ext = ToLowerAscii(path.extension().string());
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr"
        || ext == ".tif" || ext == ".tiff";
}

bool ContainsAny(std::string_view text, const std::vector<std::string>& needles) {
    for (const std::string& needle : needles) {
        if (!needle.empty() && text.find(needle) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

bool IsLikelyBaseColorTexture(std::string_view lowerName) {
    const std::vector<std::string> reject = {
        "normal", "_n", "_nr", "_nor", "rough", "metal", "spec", "gloss", "ao", "height", "disp", "mask", "emit",
        "emiss", "opacity", "alpha", "trans",
    };
    if (ContainsAny(lowerName, reject)) {
        return false;
    }
    const std::vector<std::string> prefer = {
        "albedo", "diff", "diffuse", "basecolor", "base_color", "_d", "_col", "color", "tx_",
    };
    return ContainsAny(lowerName, prefer);
}

void AppendTextureCandidatesFromDirectory(const std::filesystem::path& directory,
                                          std::vector<std::filesystem::path>& out) {
    std::error_code ec{};
    if (!std::filesystem::is_directory(directory, ec) || ec) {
        return;
    }
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(directory, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        if (!IsImageExtension(entry.path())) {
            continue;
        }
        const std::filesystem::path normalized = entry.path().lexically_normal();
        if (std::find(out.begin(), out.end(), normalized) == out.end()) {
            out.push_back(normalized);
        }
    }
}

void AppendTextureCandidatesFromTree(const std::filesystem::path& root,
                                     std::vector<std::filesystem::path>& out,
                                     const int maxDepth,
                                     const int depth = 0) {
    if (maxDepth < 0 || depth > maxDepth) {
        return;
    }
    AppendTextureCandidatesFromDirectory(root, out);
    if (depth == maxDepth) {
        return;
    }
    std::error_code ec{};
    if (!std::filesystem::is_directory(root, ec) || ec) {
        return;
    }
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(root, ec)) {
        if (ec || !entry.is_directory()) {
            continue;
        }
        AppendTextureCandidatesFromTree(entry.path(), out, maxDepth, depth + 1);
    }
}

std::vector<std::filesystem::path> CollectTextureCandidates(const std::filesystem::path& modelPath) {
    std::vector<std::filesystem::path> out;
    const std::filesystem::path directory = modelPath.parent_path();
    AppendTextureCandidatesFromDirectory(directory, out);
    AppendTextureCandidatesFromDirectory(directory / "Textures", out);
    AppendTextureCandidatesFromDirectory(directory.parent_path() / "Textures", out);
    AppendTextureCandidatesFromDirectory(directory.parent_path().parent_path() / "Textures", out);
    AppendTextureCandidatesFromTree(directory.parent_path(), out, 2);
    return out;
}

[[nodiscard]] std::string ExistingAbsolutePath(const std::filesystem::path& path) {
    std::error_code ec{};
    if (path.empty() || !std::filesystem::exists(path, ec) || ec) {
        return {};
    }
    return std::filesystem::absolute(path, ec).lexically_normal().generic_string();
}

std::string AbsolutizeTexturePath(const std::filesystem::path& path, const std::filesystem::path& modelPath) {
    if (path.empty()) {
        return {};
    }
    std::filesystem::path resolved = path;
    if (!resolved.is_absolute()) {
        resolved = modelPath.parent_path() / resolved;
    }
    return ExistingAbsolutePath(resolved.lexically_normal());
}

/// Remap baked author-machine absolute paths / bare filenames onto pack-local textures
/// (e.g. Models/*.fbx + sibling ../Textures/<basename>).
[[nodiscard]] std::string RemapTextureByBasename(const std::filesystem::path& authoredPath,
                                                 const std::filesystem::path& modelPath,
                                                 const std::vector<std::filesystem::path>& textureCandidates) {
    if (authoredPath.empty()) {
        return {};
    }
    const std::filesystem::path fileName = authoredPath.filename();
    if (fileName.empty()) {
        return {};
    }
    const std::string fileNameLower = ToLowerAscii(fileName.string());
    const std::string stemLower = ToLowerAscii(authoredPath.stem().string());
    const std::filesystem::path modelDir = modelPath.parent_path();
    const std::filesystem::path packDir = modelDir.parent_path();

    const std::array<std::filesystem::path, 5> probes{{
        modelDir / fileName,
        modelDir / "Textures" / fileName,
        packDir / "Textures" / fileName,
        packDir / fileName,
        modelDir.parent_path().parent_path() / "Textures" / fileName,
    }};
    for (const std::filesystem::path& probe : probes) {
        if (const std::string absolute = ExistingAbsolutePath(probe); !absolute.empty()) {
            return absolute;
        }
    }

    const std::filesystem::path* stemMatch = nullptr;
    for (const std::filesystem::path& candidate : textureCandidates) {
        const std::string candidateNameLower = ToLowerAscii(candidate.filename().string());
        if (candidateNameLower == fileNameLower) {
            if (const std::string absolute = ExistingAbsolutePath(candidate); !absolute.empty()) {
                return absolute;
            }
        }
        if (stemMatch == nullptr && !stemLower.empty()) {
            const std::string candidateStemLower = ToLowerAscii(candidate.stem().string());
            if (candidateStemLower == stemLower) {
                stemMatch = &candidate;
            }
        }
    }
    if (stemMatch != nullptr) {
        return ExistingAbsolutePath(*stemMatch);
    }
    return {};
}

std::string ResolveUfbxTextureFile(const ufbx_texture* texture,
                                   const std::filesystem::path& modelPath,
                                   const std::vector<std::filesystem::path>& textureCandidates,
                                   const int depth = 0) {
    if (texture == nullptr || depth > 8) {
        return {};
    }
    for (std::size_t fileIndex = 0; fileIndex < texture->file_textures.count; ++fileIndex) {
        const ufbx_texture* nestedTexture = texture->file_textures[fileIndex];
        const std::string resolved =
            ResolveUfbxTextureFile(nestedTexture, modelPath, textureCandidates, depth + 1);
        if (!resolved.empty()) {
            return resolved;
        }
    }

    const std::array<std::string, 3> authoredPaths{{
        ToString(texture->filename, ""),
        ToString(texture->relative_filename, ""),
        ToString(texture->absolute_filename, ""),
    }};
    for (const std::string& authored : authoredPaths) {
        if (authored.empty()) {
            continue;
        }
        if (const std::string resolved = AbsolutizeTexturePath(authored, modelPath); !resolved.empty()) {
            return resolved;
        }
    }
    for (const std::string& authored : authoredPaths) {
        if (authored.empty()) {
            continue;
        }
        if (const std::string remapped = RemapTextureByBasename(authored, modelPath, textureCandidates);
            !remapped.empty()) {
            return remapped;
        }
    }
    return {};
}

std::string ResolveUfbxMaterialTextureProp(const ufbx_material* material,
                                           const std::filesystem::path& modelPath,
                                           const std::vector<std::filesystem::path>& textureCandidates,
                                           const std::vector<const char*>& propNames) {
    if (material == nullptr) {
        return {};
    }
    for (const char* propName : propNames) {
        if (propName == nullptr) {
            break;
        }
        const ufbx_texture* texture = ufbx_find_prop_texture(material, propName);
        const std::string resolved = ResolveUfbxTextureFile(texture, modelPath, textureCandidates);
        if (!resolved.empty()) {
            return resolved;
        }
    }
    return {};
}

std::string ResolveFbxTextureByRole(const std::vector<std::filesystem::path>& textureCandidates,
                                    const std::vector<std::string>& includeTokens,
                                    const std::vector<std::string>& rejectTokens) {
    int bestScore = std::numeric_limits<int>::min();
    const std::filesystem::path* bestPath = nullptr;
    for (const std::filesystem::path& candidate : textureCandidates) {
        const std::string fileNameLower = ToLowerAscii(candidate.filename().string());
        if (!ContainsAny(fileNameLower, includeTokens)) {
            continue;
        }
        int score = 0;
        for (const std::string& token : includeTokens) {
            if (!token.empty() && fileNameLower.find(token) != std::string::npos) {
                score += 12;
            }
        }
        for (const std::string& token : rejectTokens) {
            if (!token.empty() && fileNameLower.find(token) != std::string::npos) {
                score -= 16;
            }
        }
        if (score > bestScore) {
            bestScore = score;
            bestPath = &candidate;
        }
    }
    if (bestPath == nullptr) {
        return {};
    }
    std::error_code ec{};
    const std::filesystem::path absolute = std::filesystem::absolute(*bestPath, ec);
    return absolute.lexically_normal().generic_string();
}

int ScoreTextureCandidate(const std::filesystem::path& candidate,
                          std::string_view materialNameLower,
                          std::string_view modelStemLower) {
    const std::string fileNameLower = ToLowerAscii(candidate.filename().string());
    int score = 0;
    if (fileNameLower.find("tx_") != std::string::npos) {
        score += 24;
    }
    if (!modelStemLower.empty() && fileNameLower.find(std::string(modelStemLower)) != std::string::npos) {
        score += 18;
    }
    if (!materialNameLower.empty() && fileNameLower.find(std::string(materialNameLower)) != std::string::npos) {
        score += 14;
    }
    if (materialNameLower.find("trunk") != std::string::npos && fileNameLower.find("trunk") != std::string::npos) {
        score += 22;
    }
    if (materialNameLower.find("branch") != std::string::npos && fileNameLower.find("branch") != std::string::npos) {
        score += 22;
    }
    if (materialNameLower.find("trunk") != std::string::npos && fileNameLower.find("branch") != std::string::npos) {
        score -= 18;
    }
    if (materialNameLower.find("branch") != std::string::npos && fileNameLower.find("trunk") != std::string::npos) {
        score -= 18;
    }
    if (IsLikelyBaseColorTexture(fileNameLower)) {
        score += 18;
    }
    const std::vector<std::string> reject = {
        "normal", "_n", "_nr", "_nor", "rough", "metal", "spec", "gloss", "ao", "height", "disp", "mask", "emit",
        "emiss", "opacity", "alpha", "trans",
    };
    if (ContainsAny(fileNameLower, reject)) {
        score -= 24;
    }
    return score;
}

std::string ResolveHeuristicBaseColorTexture(const ufbx_material* material,
                                             const std::filesystem::path& modelPath,
                                             const std::vector<std::filesystem::path>& textureCandidates) {
    if (textureCandidates.empty()) {
        return {};
    }
    const std::string materialNameLower =
        material != nullptr ? ToLowerAscii(ToString(material->name, "")) : std::string{};
    std::string modelStemLower = ToLowerAscii(modelPath.stem().string());
    if (modelStemLower.rfind("ms_", 0) == 0) {
        modelStemLower = modelStemLower.substr(3);
    }

    int bestScore = std::numeric_limits<int>::min();
    const std::filesystem::path* bestPath = nullptr;
    for (const std::filesystem::path& candidate : textureCandidates) {
        const int score = ScoreTextureCandidate(candidate, materialNameLower, modelStemLower);
        if (score > bestScore) {
            bestScore = score;
            bestPath = &candidate;
        }
    }
    if (bestPath == nullptr || bestScore <= 0) {
        return {};
    }
    std::error_code ec{};
    const std::filesystem::path absolute = std::filesystem::absolute(*bestPath, ec);
    return absolute.lexically_normal().generic_string();
}

std::string ResolveUfbxBaseColorTexture(const ufbx_material* material,
                                        const std::filesystem::path& modelPath,
                                        const std::vector<std::filesystem::path>& textureCandidates) {
    static const std::vector<const char*> kAlbedoPropNames{
        "DiffuseColor",
        "diffuse",
        "map_Kd",
        "base_color",
        "BaseColor",
        "albedo",
    };
    const std::string embedded =
        ResolveUfbxMaterialTextureProp(material, modelPath, textureCandidates, kAlbedoPropNames);
    if (!embedded.empty()) {
        return embedded;
    }
    return ResolveHeuristicBaseColorTexture(material, modelPath, textureCandidates);
}

std::string ResolveUfbxNormalTexture(const ufbx_material* material,
                                     const std::filesystem::path& modelPath,
                                     const std::vector<std::filesystem::path>& textureCandidates) {
    static const std::vector<const char*> kNormalPropNames{
        "NormalMap",
        "normal",
        "map_Bump",
        "bump",
        "norm",
    };
    const std::string embedded =
        ResolveUfbxMaterialTextureProp(material, modelPath, textureCandidates, kNormalPropNames);
    if (!embedded.empty()) {
        return embedded;
    }
    return ResolveFbxTextureByRole(
        textureCandidates,
        {"normal", "_n", "_nor", "_nrm"},
        {"rough", "metal", "orm", "ao", "emit", "opacity", "alpha"});
}

bool IsFoliageLikeName(std::string_view lower) {
    const std::vector<std::string> foliage = {"leaf", "leaves", "branch", "bush", "grass", "tree", "needle", "frond"};
    return ContainsAny(lower, foliage);
}

float Clamp01(double value) {
    return std::clamp(static_cast<float>(value), 0.0f, 1.0f);
}

Transform TransformFromUfbx(const ufbx_transform& value) {
    const ufbx_vec3 euler = ufbx_quat_to_euler(value.rotation, UFBX_ROTATION_ORDER_XYZ);
    return Transform{
        .position = ToVec3(value.translation),
        .rotationDegrees = ToVec3(euler),
        .scale = ToVec3(value.scale),
    };
}

Material MaterialFromUfbx(const ufbx_material* material,
                        const std::filesystem::path& modelPath,
                        const std::vector<std::filesystem::path>& textureCandidates) {
    if (material == nullptr) {
        return Material{
            .name = "FbxDefaultMaterial",
            .shadingModel = ShadingModel::Lit,
            .baseColor = ri::math::Vec3{0.82f, 0.82f, 0.86f},
            .roughness = 1.0f,
            .opacity = 1.0f,
        };
    }

    ri::math::Vec3 baseColor{0.82f, 0.82f, 0.86f};
    ri::math::Vec3 emissiveColor{0.0f, 0.0f, 0.0f};
    float metallic = 0.0f;
    float roughness = 1.0f;
    float opacity = 1.0f;

    if (material->pbr.base_color.has_value) {
        baseColor = ToVec3(material->pbr.base_color.value_vec3);
    } else if (material->fbx.diffuse_color.has_value) {
        baseColor = ToVec3(material->fbx.diffuse_color.value_vec3);
    }

    if (material->pbr.emission_color.has_value) {
        emissiveColor = ToVec3(material->pbr.emission_color.value_vec3);
    } else if (material->fbx.emission_color.has_value) {
        emissiveColor = ToVec3(material->fbx.emission_color.value_vec3);
    }

    if (material->pbr.metalness.has_value) {
        metallic = Clamp01(material->pbr.metalness.value_real);
    }

    if (material->pbr.roughness.has_value) {
        roughness = Clamp01(material->pbr.roughness.value_real);
    } else if (material->pbr.glossiness.has_value) {
        roughness = Clamp01(1.0 - material->pbr.glossiness.value_real);
    }

    if (material->pbr.opacity.has_value) {
        opacity = Clamp01(material->pbr.opacity.value_real);
    } else if (material->fbx.transparency_factor.has_value) {
        opacity = Clamp01(1.0 - material->fbx.transparency_factor.value_real);
    }

    const std::string baseColorTexture = ResolveUfbxBaseColorTexture(material, modelPath, textureCandidates);
    const std::string normalTexture = ResolveUfbxNormalTexture(material, modelPath, textureCandidates);
    const std::string ormTexture = ResolveFbxTextureByRole(
        textureCandidates,
        {"orm", "occlusionroughnessmetallic", "metalrough", "mra"},
        {"normal", "emit", "opacity", "alpha"});
    const std::string roughnessTexture = ormTexture.empty()
        ? ResolveFbxTextureByRole(
              textureCandidates,
              {"rough", "roughness", "_rgh"},
              {"normal", "metal", "orm", "emit", "opacity", "alpha"})
        : std::string{};
    const std::string metallicTexture = ormTexture.empty()
        ? ResolveFbxTextureByRole(
              textureCandidates,
              {"metal", "metallic", "_met"},
              {"normal", "rough", "orm", "emit", "opacity", "alpha"})
        : std::string{};
    const std::string emissiveTexture = ResolveFbxTextureByRole(
        textureCandidates,
        {"emiss", "emit", "_e"},
        {"normal", "rough", "metal", "orm", "opacity", "alpha"});
    const std::string opacityTexture = ResolveFbxTextureByRole(
        textureCandidates,
        {"opacity", "alpha", "trans", "mask"},
        {"normal", "rough", "metal", "orm", "emit"});
    const std::string occlusionTexture = ormTexture.empty()
        ? ResolveFbxTextureByRole(
              textureCandidates,
              {"ao", "occlusion", "ambientocclusion"},
              {"normal", "rough", "metal", "orm", "emit", "opacity", "alpha"})
        : std::string{};
    const std::string materialName = ToString(material->name, "FbxMaterial");
    const std::string materialNameLower = ToLowerAscii(materialName);
    const std::string textureNameLower = ToLowerAscii(std::filesystem::path(baseColorTexture).filename().string());
    const bool foliageLike = IsFoliageLikeName(materialNameLower) || IsFoliageLikeName(textureNameLower);
    const bool packedSmoothnessAlbedo =
        textureNameLower.find("smoothness") != std::string::npos
        || (textureNameLower.find("[albedo]") != std::string::npos
            && textureNameLower.find("smoothness") != std::string::npos);

    Material built{
        .name = materialName,
        .shadingModel = material->features.unlit.enabled ? ShadingModel::Unlit : ShadingModel::Lit,
        .baseColor = baseColor,
        .baseColorTexture = baseColorTexture,
        .emissiveColor = emissiveColor,
        .metallic = metallic,
        .roughness = roughness,
        .opacity = opacity,
        .alphaCutoff = foliageLike && !packedSmoothnessAlbedo ? 0.45f : 1.0f,
        .doubleSided = material->features.double_sided.enabled || foliageLike,
        .transparent = opacity < 0.999f && !packedSmoothnessAlbedo,
        .normalTexture = normalTexture,
        .ormTexture = ormTexture,
        .roughnessTexture = roughnessTexture,
        .metallicTexture = metallicTexture,
        .emissiveTexture = emissiveTexture,
        .opacityTexture = opacityTexture,
        .occlusionTexture = occlusionTexture,
    };
    built.albedoAlphaIsSmoothness = packedSmoothnessAlbedo;
    if (!built.transparent) {
        built.opacity = 1.0f;
    }
    if ((built.materialStyle == MaterialStyle::Layered || built.materialStyle == MaterialStyle::MixedMedia)
        && !built.baseColorTexture.empty()) {
        built.detailTexture = built.baseColorTexture;
    }
    return built;
}

std::string FormatLoadError(const std::filesystem::path& path, const ufbx_error& error) {
    std::array<char, 512> message{};
    const std::size_t count = ufbx_format_error(message.data(), message.size(), &error);
    std::ostringstream stream;
    stream << "ufbx_load_file failed for " << path.string();
    if (count > 0U) {
        stream << ": " << std::string(message.data(), count);
    }
    return stream.str();
}

bool BuildMeshFromFaces(const ufbx_mesh* sourceMesh,
                        const ufbx_uint32_list* faceIndices,
                        std::string_view meshName,
                        Mesh& outMesh,
                        std::string& error) {
    if (sourceMesh == nullptr) {
        error = "Null FBX mesh.";
        return false;
    }
    if (!sourceMesh->vertex_position.exists) {
        error = "FBX mesh is missing vertex positions.";
        return false;
    }

    std::vector<ri::math::Vec3> positions;
    std::vector<ri::math::Vec3> normals;
    std::vector<ri::math::Vec2> texCoords;
    std::vector<int> indices;
    std::vector<uint32_t> triangleIndices(sourceMesh->max_face_triangles * 3U);
    const bool hasTexCoords = sourceMesh->vertex_uv.exists;
    const bool hasNormals = sourceMesh->vertex_normal.exists;

    const auto appendFace = [&](const ufbx_face& face) -> bool {
        const uint32_t triangleCount =
            ufbx_triangulate_face(triangleIndices.data(), triangleIndices.size(), sourceMesh, face);
        for (uint32_t triangleVertex = 0; triangleVertex < triangleCount * 3U; ++triangleVertex) {
            const uint32_t meshIndex = triangleIndices[triangleVertex];
            if (meshIndex >= sourceMesh->vertex_position.indices.count) {
                error = "FBX mesh produced an out-of-range vertex index while triangulating.";
                return false;
            }

            positions.push_back(ToVec3(ufbx_get_vertex_vec3(&sourceMesh->vertex_position, meshIndex)));
            if (hasNormals) {
                normals.push_back(ToVec3(ufbx_get_vertex_vec3(&sourceMesh->vertex_normal, meshIndex)));
            }
            if (hasTexCoords) {
                texCoords.push_back(ToVec2(ufbx_get_vertex_vec2(&sourceMesh->vertex_uv, meshIndex)));
            }
            indices.push_back(static_cast<int>(indices.size()));
        }
        return true;
    };

    if (faceIndices != nullptr && faceIndices->count > 0U) {
        for (std::size_t faceIndex = 0; faceIndex < faceIndices->count; ++faceIndex) {
            const uint32_t sourceFaceIndex = faceIndices->data[faceIndex];
            if (sourceFaceIndex >= sourceMesh->faces.count) {
                error = "FBX mesh part referenced an invalid face.";
                return false;
            }
            if (!appendFace(sourceMesh->faces[sourceFaceIndex])) {
                return false;
            }
        }
    } else {
        for (std::size_t faceIndex = 0; faceIndex < sourceMesh->faces.count; ++faceIndex) {
            if (!appendFace(sourceMesh->faces[faceIndex])) {
                return false;
            }
        }
    }

    outMesh = Mesh{
        .name = std::string(meshName),
        .primitive = PrimitiveType::Custom,
        .vertexCount = static_cast<int>(positions.size()),
        .indexCount = static_cast<int>(indices.size()),
        .positions = std::move(positions),
        .normals = hasNormals ? std::move(normals) : std::vector<ri::math::Vec3>{},
        .texCoords = hasTexCoords ? std::move(texCoords) : std::vector<ri::math::Vec2>{},
        .indices = std::move(indices),
    };
    error.clear();
    return true;
}

bool AttachUfbxMesh(Scene& scene,
                   const ufbx_node* sourceNode,
                   int nodeHandle,
                   const std::filesystem::path& modelPath,
                   const std::vector<std::filesystem::path>& textureCandidates,
                   UfbxMaterialInternTable& materialTable,
                   std::string& error) {
    if (sourceNode->mesh == nullptr) {
        error.clear();
        return true;
    }

    const ufbx_mesh* sourceMesh = sourceNode->mesh;
    const auto resolveMaterial = [&](uint32_t index) -> const ufbx_material* {
        if (index < sourceNode->materials.count && sourceNode->materials[index] != nullptr) {
            return sourceNode->materials[index];
        }
        if (index < sourceMesh->materials.count && sourceMesh->materials[index] != nullptr) {
            return sourceMesh->materials[index];
        }
        return nullptr;
    };

    std::size_t emittedParts = 0U;
    for (std::size_t partIndex = 0; partIndex < sourceMesh->material_parts.count; ++partIndex) {
        const ufbx_mesh_part& part = sourceMesh->material_parts[partIndex];
        if (part.num_faces == 0U) {
            continue;
        }

        const std::string partName = ToString(sourceMesh->name, "FbxMesh") + "_Part" + std::to_string(part.index);
        Mesh builtMesh{};
        if (!BuildMeshFromFaces(sourceMesh, &part.face_indices, partName, builtMesh, error)) {
            return false;
        }
        if (builtMesh.indexCount == 0) {
            continue;
        }

        const int materialHandle = InternUfbxMaterial(
            scene,
            MaterialFromUfbx(resolveMaterial(part.index), modelPath, textureCandidates),
            materialTable);
        const int meshHandle = scene.AddMesh(std::move(builtMesh));
        const int meshNode = scene.CreateNode(partName + "_Node", nodeHandle);
        scene.GetNode(meshNode).localTransform = Transform{};
        scene.AttachMesh(meshNode, meshHandle, materialHandle);
        ++emittedParts;
    }

    if (emittedParts > 0U) {
        error.clear();
        return true;
    }

    Mesh builtMesh{};
    if (!BuildMeshFromFaces(sourceMesh, nullptr, ToString(sourceMesh->name, "FbxMesh"), builtMesh, error)) {
        return false;
    }
    if (builtMesh.indexCount == 0) {
        error.clear();
        return true;
    }

    const int materialHandle = InternUfbxMaterial(
        scene,
        MaterialFromUfbx(resolveMaterial(0U), modelPath, textureCandidates),
        materialTable);
    const int meshHandle = scene.AddMesh(std::move(builtMesh));
    const int meshNode = scene.CreateNode(ToString(sourceMesh->name, "FbxMesh") + "_Node", nodeHandle);
    scene.GetNode(meshNode).localTransform = Transform{};
    scene.AttachMesh(meshNode, meshHandle, materialHandle);
    error.clear();
    return true;
}

int ImportUfbxNodeRecursive(Scene& scene,
                           const ufbx_node* sourceNode,
                           int parentHandle,
                           const std::filesystem::path& modelPath,
                           const std::vector<std::filesystem::path>& textureCandidates,
                           UfbxMaterialInternTable& materialTable,
                           std::string& error) {
    if (sourceNode == nullptr) {
        error = "Null FBX node.";
        return kInvalidHandle;
    }

    std::string fallbackName = "FbxNode";
    if (sourceNode->is_geometry_transform_helper) {
        fallbackName = "FbxGeometryHelper";
    } else if (sourceNode->is_scale_helper) {
        fallbackName = "FbxScaleHelper";
    }

    const int nodeHandle = scene.CreateNode(ToString(sourceNode->name, fallbackName), parentHandle);
    scene.GetNode(nodeHandle).localTransform = TransformFromUfbx(sourceNode->local_transform);

    if (!AttachUfbxMesh(scene, sourceNode, nodeHandle, modelPath, textureCandidates, materialTable, error)) {
        return kInvalidHandle;
    }

    for (std::size_t childIndex = 0; childIndex < sourceNode->children.count; ++childIndex) {
        if (ImportUfbxNodeRecursive(
                scene,
                sourceNode->children[childIndex],
                nodeHandle,
                modelPath,
                textureCandidates,
                materialTable,
                error)
            == kInvalidHandle) {
            return kInvalidHandle;
        }
    }

    error.clear();
    return nodeHandle;
}

[[nodiscard]] bool IsObjPath(const std::filesystem::path& path) {
    const std::string ext = ToLowerAscii(path.extension().string());
    return ext == ".obj";
}

} // namespace

int ImportUfbxSceneFile(Scene& targetScene,
                        const std::filesystem::path& path,
                        const UfbxSceneImportOptions& options,
                        std::string& error) {
    const std::vector<std::filesystem::path> textureCandidates = CollectTextureCandidates(path);
    ufbx_load_opts loadOptions{};
    const UfbxSceneImportOptions::FileFormat resolvedFormat =
        options.fileFormat == UfbxSceneImportOptions::FileFormat::Auto && IsObjPath(path)
            ? UfbxSceneImportOptions::FileFormat::Obj
            : options.fileFormat;
    loadOptions.ignore_missing_external_files = true;
    loadOptions.generate_missing_normals = true;
    if (resolvedFormat == UfbxSceneImportOptions::FileFormat::Obj) {
        loadOptions.file_format = UFBX_FILE_FORMAT_OBJ;
        loadOptions.load_external_files = true;
        loadOptions.obj_search_mtl_by_filename = true;
    } else {
        loadOptions.target_axes = ufbx_axes_right_handed_y_up;
        loadOptions.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES;
        loadOptions.inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_HELPER_NODES;
    }

    const std::string pathString = path.string();
    ufbx_error loadError{};
    UfbxScenePtr loadedScene(ufbx_load_file(pathString.c_str(), &loadOptions, &loadError), &ufbx_free_scene);
    if (!loadedScene) {
        error = FormatLoadError(path, loadError);
        return kInvalidHandle;
    }

    if (loadedScene->root_node == nullptr) {
        error = "Model file does not contain a scene root: " + pathString;
        return kInvalidHandle;
    }

    // All-or-nothing: any mid-import failure must not leave wrapper/partial trees attached.
    const SceneAppendWatermark watermark = targetScene.CaptureAppendWatermark();
    const auto failClosed = [&]() -> int {
        targetScene.TruncateToAppendWatermark(watermark);
        return kInvalidHandle;
    };

    int importParent = options.parent;
    if (!options.wrapperNodeName.empty()) {
        importParent = targetScene.CreateNode(options.wrapperNodeName, options.parent);
        if (resolvedFormat != UfbxSceneImportOptions::FileFormat::Obj) {
            targetScene.GetNode(importParent).localTransform =
                TransformFromUfbx(loadedScene->root_node->local_transform);
        }
    }

    UfbxMaterialInternTable materialTable;
    int firstRoot = kInvalidHandle;
    const auto importChild = [&](const ufbx_node* childNode) {
        const int importedRoot =
            ImportUfbxNodeRecursive(targetScene, childNode, importParent, path, textureCandidates, materialTable, error);
        if (importedRoot == kInvalidHandle) {
            return false;
        }
        if (firstRoot == kInvalidHandle) {
            firstRoot = importedRoot;
        }
        return true;
    };

    if (loadedScene->root_node->children.count > 0U) {
        for (std::size_t childIndex = 0; childIndex < loadedScene->root_node->children.count; ++childIndex) {
            if (!importChild(loadedScene->root_node->children[childIndex])) {
                return failClosed();
            }
        }
    } else if (loadedScene->root_node->mesh != nullptr) {
        const int importedRoot =
            ImportUfbxNodeRecursive(
                targetScene, loadedScene->root_node, importParent, path, textureCandidates, materialTable, error);
        if (importedRoot == kInvalidHandle) {
            return failClosed();
        }
        firstRoot = importedRoot;
    } else {
        error = "Model file does not contain any importable geometry: " + pathString;
        return failClosed();
    }

    error.clear();
    if (!options.wrapperNodeName.empty()) {
        return importParent;
    }
    return firstRoot;
}

} // namespace ri::scene
