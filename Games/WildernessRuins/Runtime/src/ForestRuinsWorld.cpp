#include "RawIron/Games/ForestRuins/ForestRuinsRuntime.h"

#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/ScriptScalars.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Scene/SceneSubtreeHelpers.h"
#include "RawIron/Scene/SceneUtils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::games::forestruins {

namespace {

namespace fs = std::filesystem;
using namespace ri::scene;

std::string ToLowerAscii(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

bool PathContainsAscii(const fs::path& path, std::string_view tokenLower) {
    const std::string asLower = ToLowerAscii(path.generic_string());
    return asLower.find(std::string(tokenLower)) != std::string::npos;
}

struct DeterministicRng {
    std::uint64_t state = 0x9E3779B97F4A7C15ULL;

    [[nodiscard]] std::uint64_t NextU64() {
        state += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = state;
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31U);
    }

    [[nodiscard]] float NextUnit() {
        return static_cast<float>((NextU64() >> 40U) * (1.0 / static_cast<double>(1ULL << 24U)));
    }

    [[nodiscard]] float NextRange(const float minValue, const float maxValue) {
        return minValue + ((maxValue - minValue) * NextUnit());
    }

    [[nodiscard]] int NextIndex(const int count) {
        if (count <= 1) {
            return 0;
        }
        return static_cast<int>(NextU64() % static_cast<std::uint64_t>(count));
    }
};

struct Clearing {
    ri::math::Vec3 center{};
    float radius = 0.0f;
};

[[nodiscard]] bool PointInsideAnyClearing(const ri::math::Vec3& point, const std::vector<Clearing>& clearings) {
    for (const Clearing& clearing : clearings) {
        const float dx = point.x - clearing.center.x;
        const float dz = point.z - clearing.center.z;
        if (((dx * dx) + (dz * dz)) <= (clearing.radius * clearing.radius)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] ri::math::Vec3 PickScatterPoint(DeterministicRng& rng,
                                              const float extent,
                                              const bool requireClearing,
                                              const std::vector<Clearing>& clearings) {
    for (int attempt = 0; attempt < 64; ++attempt) {
        const ri::math::Vec3 point{
            rng.NextRange(-extent, extent),
            0.0f,
            rng.NextRange(-extent, extent),
        };
        const bool inClearing = PointInsideAnyClearing(point, clearings);
        if ((requireClearing && inClearing) || (!requireClearing && !inClearing)) {
            return point;
        }
    }
    return ri::math::Vec3{
        rng.NextRange(-extent, extent),
        0.0f,
        rng.NextRange(-extent, extent),
    };
}

[[nodiscard]] float RuinPathCenterX(const float z) {
    return (std::sin(z * 0.065f) * 3.6f) + (std::sin((z + 18.0f) * 0.023f) * 2.2f);
}

[[nodiscard]] float RuinPathHalfWidth(const float z) {
    return 4.8f + (std::sin((z + 44.0f) * 0.11f) * 0.7f);
}

[[nodiscard]] Mesh MakeVerticalBillboardMesh(const float uMin, const float uMax) {
    Mesh mesh{};
    mesh.name = "vertical-conifer-billboard";
    mesh.primitive = PrimitiveType::Custom;
    mesh.positions = {
        ri::math::Vec3{-0.5f, 0.0f, 0.0f},
        ri::math::Vec3{0.5f, 0.0f, 0.0f},
        ri::math::Vec3{0.5f, 1.0f, 0.0f},
        ri::math::Vec3{-0.5f, 1.0f, 0.0f},
    };
    mesh.texCoords = {
        ri::math::Vec2{uMin, 1.0f},
        ri::math::Vec2{uMax, 1.0f},
        ri::math::Vec2{uMax, 0.0f},
        ri::math::Vec2{uMin, 0.0f},
    };
    mesh.indices = {0, 1, 2, 0, 2, 3};
    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    mesh.indexCount = static_cast<int>(mesh.indices.size());
    return mesh;
}

[[nodiscard]] std::string ToAbsoluteAssetPath(const fs::path& path) {
    std::error_code ec{};
    const fs::path absolute = fs::absolute(path, ec);
    return absolute.lexically_normal().generic_string();
}

[[nodiscard]] bool IsMeshExtension(const fs::path& path) {
    const std::string ext = ToLowerAscii(path.extension().string());
    return ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb";
}

struct ScatterAsset {
    std::string namePrefix{};
    fs::path sourcePath{};
    float minScale = 1.0f;
    float maxScale = 1.0f;
    float colliderRadius = 0.0f;
    float colliderHeight = 0.0f;
};

[[nodiscard]] bool ShouldSkipPostApocalypseScatter(const fs::path& meshPath) {
    const std::string lower = ToLowerAscii(meshPath.generic_string());
    if (lower.find("/decals/") != std::string::npos || lower.find("\\decals\\") != std::string::npos) {
        return true;
    }
    const std::string stem = ToLowerAscii(meshPath.stem().string());
    if (stem == "ms_decal" || stem == "ms_candle") {
        return true;
    }
    return false;
}

void ApplyPostApocalypseScatterHeuristics(ScatterAsset& asset) {
    const std::string lower = ToLowerAscii(asset.sourcePath.generic_string());
    asset.minScale = 1.7f;
    asset.maxScale = 3.1f;
    asset.colliderRadius = 1.4f;
    asset.colliderHeight = 2.0f;
    if (lower.find("bus_stop") != std::string::npos || lower.find("billboard") != std::string::npos
        || lower.find("fireplace_tower") != std::string::npos || lower.find("tent_civilian") != std::string::npos) {
        asset.minScale = 2.4f;
        asset.maxScale = 4.2f;
        asset.colliderRadius = 2.8f;
        asset.colliderHeight = 4.8f;
    } else if (lower.find("pole") != std::string::npos) {
        asset.minScale = 2.2f;
        asset.maxScale = 3.6f;
        asset.colliderRadius = 1.2f;
        asset.colliderHeight = 5.0f;
    } else if (lower.find("fence") != std::string::npos || lower.find("barrier") != std::string::npos) {
        asset.minScale = 2.0f;
        asset.maxScale = 3.6f;
        asset.colliderRadius = 2.2f;
        asset.colliderHeight = 2.4f;
    } else if (lower.find("plank") != std::string::npos || lower.find("brick") != std::string::npos
               || lower.find("crate") != std::string::npos || lower.find("pallet") != std::string::npos) {
        asset.minScale = 1.6f;
        asset.maxScale = 2.8f;
        asset.colliderRadius = 1.2f;
        asset.colliderHeight = 1.4f;
    } else if (lower.find("sign") != std::string::npos || lower.find("mailbox") != std::string::npos) {
        asset.minScale = 2.0f;
        asset.maxScale = 3.4f;
        asset.colliderRadius = 1.0f;
        asset.colliderHeight = 2.6f;
    }
}

[[nodiscard]] std::vector<ScatterAsset> DiscoverPostApocalypseScatterAssets(const fs::path& postApocRoot) {
    std::vector<ScatterAsset> assets;
    std::error_code ec{};
    if (!fs::is_directory(postApocRoot, ec) || ec) {
        return assets;
    }
    for (const fs::directory_entry& entry :
         fs::recursive_directory_iterator(postApocRoot, fs::directory_options::skip_permission_denied, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        const fs::path meshPath = entry.path();
        if (!IsMeshExtension(meshPath)) {
            continue;
        }
        const std::string stem = meshPath.stem().string();
        if (stem.rfind("MS_", 0) != 0 && stem.rfind("ms_", 0) != 0) {
            continue;
        }
        if (ShouldSkipPostApocalypseScatter(meshPath)) {
            continue;
        }
        ScatterAsset asset{
            .namePrefix = meshPath.parent_path().filename().string(),
            .sourcePath = meshPath,
        };
        ApplyPostApocalypseScatterHeuristics(asset);
        assets.push_back(std::move(asset));
    }
    std::sort(assets.begin(), assets.end(), [](const ScatterAsset& a, const ScatterAsset& b) {
        return a.sourcePath.generic_string() < b.sourcePath.generic_string();
    });
    assets.erase(std::unique(assets.begin(),
                             assets.end(),
                             [](const ScatterAsset& a, const ScatterAsset& b) {
                                 return a.sourcePath == b.sourcePath;
                             }),
                assets.end());
    return assets;
}

[[nodiscard]] std::vector<ScatterAsset> DiscoverRockScatterAssets(const fs::path& rocksRoot) {
    std::vector<ScatterAsset> assets;
    std::error_code ec{};
    if (!fs::is_directory(rocksRoot, ec) || ec) {
        return assets;
    }
    for (const fs::directory_entry& entry : fs::directory_iterator(rocksRoot, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        const fs::path meshPath = entry.path();
        if (!IsMeshExtension(meshPath)) {
            continue;
        }
        const std::string stemLower = ToLowerAscii(meshPath.stem().string());
        if (stemLower == "box") {
            continue;
        }
        if (stemLower.rfind("rock", 0) != 0) {
            continue;
        }
        assets.push_back(ScatterAsset{
            .namePrefix = std::string{"Rock"},
            .sourcePath = meshPath,
            .minScale = 2.2f,
            .maxScale = 5.6f,
            .colliderRadius = 1.8f,
            .colliderHeight = 2.6f,
        });
    }
    std::sort(assets.begin(), assets.end(), [](const ScatterAsset& a, const ScatterAsset& b) {
        return a.sourcePath.generic_string() < b.sourcePath.generic_string();
    });
    return assets;
}

[[nodiscard]] std::vector<ScatterAsset> MergeScatterAssets(std::vector<ScatterAsset> preferred,
                                                           const std::vector<ScatterAsset>& fallback) {
    std::vector<ScatterAsset> merged = std::move(preferred);
    for (const ScatterAsset& asset : fallback) {
        if (!fs::exists(asset.sourcePath)) {
            continue;
        }
        const bool alreadyListed = std::any_of(merged.begin(), merged.end(), [&](const ScatterAsset& existing) {
            return existing.sourcePath == asset.sourcePath;
        });
        if (!alreadyListed) {
            merged.push_back(asset);
        }
    }
    return merged;
}

struct ForestSceneLayout {
    fs::path sceneRoot{};
    fs::path assetsRoot{};
    fs::path packRoot{};
    fs::path astra113Root{};
    fs::path texturesRoot{};
    fs::path botdRoot{};
    fs::path botdBillboardsRoot{};
    fs::path botdMeshesRoot{};
    fs::path botdSharedTexturesRoot{};
    fs::path botdTrunkDiffuse{};
    fs::path exportedMeshesRoot{};
    fs::path rocksRoot{};
    fs::path bushesMeshesRoot{};
    fs::path groundDiffuse{};
    fs::path groundNormal{};
    fs::path barkDiffuse{};
    bool usesSortedAssetPack = false;
};

[[nodiscard]] fs::path PreferExistingPath(const fs::path& preferred, const fs::path& fallback) {
    std::error_code ec{};
    if (fs::exists(preferred, ec) && !ec) {
        return preferred;
    }
    return fallback;
}

[[nodiscard]] ForestSceneLayout MakeForestSceneLayout(const fs::path& workspaceRoot, const fs::path& gameRoot) {
    ForestSceneLayout layout{};
    const fs::path legacySceneRoot = workspaceRoot / "Assets" / "Source" / "Forest Scene";
    const fs::path sortedPackRoot = workspaceRoot / "Assets" / "Source" / "Forest Scene Assets";
    const fs::path legacyAssetsRoot = legacySceneRoot / "Assets";
    const fs::path legacyPackRoot = legacyAssetsRoot / "Assets";
    const fs::path legacyAstraRoot = legacyAssetsRoot / "Astra113 Assets";

    const fs::path sortedModelsRoot = sortedPackRoot / "Models";
    const fs::path sortedTexturesRoot = sortedPackRoot / "Textures";
    const bool sortedPackReady = fs::is_directory(sortedModelsRoot) && fs::is_directory(sortedTexturesRoot);

    layout.usesSortedAssetPack = sortedPackReady;
    layout.sceneRoot = sortedPackReady ? sortedPackRoot : legacySceneRoot;
    layout.assetsRoot = sortedPackReady ? sortedPackRoot : legacyAssetsRoot;
    layout.packRoot = sortedPackReady ? (sortedModelsRoot / "Assets") : legacyPackRoot;
    layout.astra113Root =
        sortedPackReady ? (sortedModelsRoot / "Astra113 Assets") : legacyAstraRoot;
    layout.texturesRoot =
        sortedPackReady ? (sortedTexturesRoot / "Astra113 Assets" / "Textures")
                        : (legacyAstraRoot / "Textures");

    const fs::path legacyBotdRoot = legacyPackRoot / "Conifers [BOTD]";
    const fs::path sortedBotdRoot = sortedTexturesRoot / "Assets" / "Conifers [BOTD]";
    layout.botdRoot = PreferExistingPath(sortedBotdRoot, legacyBotdRoot);
    layout.botdBillboardsRoot = PreferExistingPath(
        sortedPackRoot / "UnityOnly" / "Assets" / "Conifers [BOTD]" / "Sources" / "Billboards",
        layout.botdRoot / "Sources" / "Billboards");
    layout.botdMeshesRoot = PreferExistingPath(
        sortedPackRoot / "UnityOnly" / "Assets" / "Conifers [BOTD]" / "Sources" / "Meshes",
        layout.botdRoot / "Sources" / "Meshes");
    layout.botdSharedTexturesRoot = PreferExistingPath(
        sortedPackRoot / "UnityOnly" / "Assets" / "Conifers [BOTD]" / "Sources" / "Shared Textures",
        layout.botdRoot / "Sources" / "Shared Textures");
    layout.botdTrunkDiffuse = layout.botdSharedTexturesRoot / "BODT Conifer Trunk [Albedo] [Smoothness].tif";
    layout.exportedMeshesRoot = gameRoot / "Assets" / "Generated" / "ForestScene" / "Meshes";

    layout.rocksRoot = PreferExistingPath(
        sortedModelsRoot / "Assets" / "Rocks and Boulders 2" / "Rocks" / "Source" / "Models",
        legacyPackRoot / "Rocks and Boulders 2" / "Rocks" / "Source" / "Models");
    layout.bushesMeshesRoot = PreferExistingPath(
        sortedModelsRoot / "Astra113 Assets" / "YughuesFreeBushes2018" / "Meshes",
        legacyAstraRoot / "YughuesFreeBushes2018" / "Meshes");

    layout.groundDiffuse =
        PreferExistingPath(layout.texturesRoot / "Forest_Ground_diffuseOriginal.png",
                           legacyAstraRoot / "Textures" / "Forest_Ground_diffuseOriginal.png");
    layout.groundNormal =
        PreferExistingPath(layout.texturesRoot / "Forest_Ground_normal.png",
                           legacyAstraRoot / "Textures" / "Forest_Ground_normal.png");
    layout.barkDiffuse =
        PreferExistingPath(layout.texturesRoot / "Bark.tif", legacyAstraRoot / "Textures" / "Bark.tif");
    return layout;
}

struct BotdBillboardVariant {
    std::string label{};
    fs::path albedoPath{};
    fs::path normalPath{};
    float width = 3.2f;
    float height = 8.8f;
};

[[nodiscard]] std::optional<BotdBillboardVariant> TryResolveBotdBillboardFolder(const fs::path& folderPath,
                                                                                const std::string& folderName) {
    std::error_code ec{};
    if (!fs::is_directory(folderPath, ec) || ec) {
        return std::nullopt;
    }
    for (const fs::directory_entry& entry : fs::directory_iterator(folderPath, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        const std::string fileName = entry.path().filename().string();
        const std::string lower = ToLowerAscii(fileName);
        if (!lower.ends_with(".png")) {
            continue;
        }
        if (lower.find("albedo") == std::string::npos) {
            continue;
        }
        BotdBillboardVariant variant{
            .label = folderName,
            .albedoPath = entry.path(),
        };
        std::error_code siblingEc{};
        for (const fs::directory_entry& sibling : fs::directory_iterator(folderPath, siblingEc)) {
            if (siblingEc || !sibling.is_regular_file()) {
                continue;
            }
            const std::string siblingLower = ToLowerAscii(sibling.path().filename().string());
            if (!siblingLower.ends_with(".png")) {
                continue;
            }
            if (siblingLower.find("normal") != std::string::npos && siblingLower.find("albedo") == std::string::npos) {
                variant.normalPath = sibling.path();
                break;
            }
        }
        const std::string folderLower = ToLowerAscii(folderName);
        if (folderLower.find("bare") != std::string::npos) {
            variant.width = 2.9f;
            variant.height = 8.2f;
        } else if (folderLower.find("small") != std::string::npos) {
            variant.width = 3.0f;
            variant.height = 7.6f;
        } else if (folderLower.find("medium") != std::string::npos) {
            variant.width = 3.4f;
            variant.height = 9.2f;
        } else if (folderLower.find("tall") != std::string::npos) {
            variant.width = 4.0f;
            variant.height = 12.4f;
        }
        return variant;
    }
    return std::nullopt;
}

[[nodiscard]] std::vector<BotdBillboardVariant> DiscoverBotdBillboardVariants(const fs::path& billboardsRoot) {
    static const std::array<const char*, 4> kPreferredOrder{
        "Billboard Bare",
        "Billboard Small",
        "Billboard Medium",
        "Billboard Tall",
    };
    std::vector<BotdBillboardVariant> variants;
    variants.reserve(kPreferredOrder.size());
    for (const char* folderName : kPreferredOrder) {
        const fs::path folderPath = billboardsRoot / folderName;
        if (const std::optional<BotdBillboardVariant> variant = TryResolveBotdBillboardFolder(folderPath, folderName);
            variant.has_value()) {
            variants.push_back(*variant);
        }
    }
    if (!variants.empty()) {
        return variants;
    }
    std::error_code ec{};
    if (!fs::is_directory(billboardsRoot, ec) || ec) {
        return variants;
    }
    for (const fs::directory_entry& entry : fs::directory_iterator(billboardsRoot, ec)) {
        if (ec || !entry.is_directory()) {
            continue;
        }
        if (const std::optional<BotdBillboardVariant> variant =
                TryResolveBotdBillboardFolder(entry.path(), entry.path().filename().string());
            variant.has_value()) {
            variants.push_back(*variant);
        }
    }
    return variants;
}

[[nodiscard]] std::vector<ScatterAsset> DiscoverBushScatterAssets(const fs::path& bushesRoot) {
    std::vector<ScatterAsset> assets;
    std::error_code ec{};
    if (!fs::is_directory(bushesRoot, ec) || ec) {
        return assets;
    }
    for (const fs::directory_entry& entry : fs::directory_iterator(bushesRoot, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        if (!IsMeshExtension(entry.path())) {
            continue;
        }
        const std::string stemLower = ToLowerAscii(entry.path().stem().string());
        if (stemLower.rfind("bush", 0) != 0) {
            continue;
        }
        assets.push_back(ScatterAsset{
            .namePrefix = "Bush",
            .sourcePath = entry.path(),
            .minScale = 1.8f,
            .maxScale = 3.7f,
            .colliderRadius = 1.4f,
            .colliderHeight = 1.6f,
        });
    }
    std::sort(assets.begin(), assets.end(), [](const ScatterAsset& a, const ScatterAsset& b) {
        return a.sourcePath.generic_string() < b.sourcePath.generic_string();
    });
    return assets;
}

[[nodiscard]] std::vector<fs::path> DiscoverExportedConiferMeshes(const fs::path& exportedMeshesRoot) {
    std::vector<fs::path> meshes;
    std::error_code ec{};
    if (!fs::is_directory(exportedMeshesRoot, ec) || ec) {
        return meshes;
    }
    for (const fs::directory_entry& entry : fs::directory_iterator(exportedMeshesRoot, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        if (!IsMeshExtension(entry.path())) {
            continue;
        }
        const std::string stemLower = ToLowerAscii(entry.path().stem().string());
        if (stemLower.find("conifer") == std::string::npos) {
            continue;
        }
        if (stemLower.find("lod1") != std::string::npos) {
            continue;
        }
        meshes.push_back(entry.path());
    }
    std::sort(meshes.begin(), meshes.end());
    return meshes;
}

[[nodiscard]] bool SourceForestPackReady(const ForestSceneLayout& forest) {
    return fs::exists(forest.groundDiffuse) && fs::exists(forest.rocksRoot / "Rock1A.fbx")
        && !DiscoverBotdBillboardVariants(forest.botdBillboardsRoot).empty();
}

[[nodiscard]] bool ContainsAny(std::string_view text, std::initializer_list<std::string_view> needles) {
    for (const std::string_view needle : needles) {
        if (!needle.empty() && text.find(needle) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

void AbsolutizeMaterialTexturePaths(ri::scene::Scene& scene) {
    const auto absolutize = [](std::string& texturePath) {
        if (texturePath.empty()) {
            return;
        }
        std::error_code ec{};
        fs::path resolved(texturePath);
        if (!resolved.is_absolute()) {
            resolved = fs::absolute(resolved, ec);
        }
        resolved = resolved.lexically_normal();
        if (fs::exists(resolved, ec) && !ec) {
            texturePath = resolved.generic_string();
        }
    };

    for (std::size_t materialIndex = 0; materialIndex < scene.MaterialCount(); ++materialIndex) {
        ri::scene::Material& material = scene.GetMaterial(static_cast<int>(materialIndex));
        absolutize(material.baseColorTexture);
        absolutize(material.normalTexture);
        absolutize(material.ormTexture);
        absolutize(material.roughnessTexture);
        absolutize(material.metallicTexture);
        absolutize(material.emissiveTexture);
        absolutize(material.opacityTexture);
        absolutize(material.occlusionTexture);
        absolutize(material.detailTexture);
        for (std::string& framePath : material.baseColorTextureFrames) {
            absolutize(framePath);
        }
    }
}

void ApplyForestRuinsShowcaseMaterials(ri::scene::Scene& scene, const fs::path& engineTexturesRoot) {
    const fs::path lrtPackageRoot =
        fs::weakly_canonical(engineTexturesRoot / ".." / "Packages" / "LRT - Texture Pack - RT28.8 - 128x");
    const auto packagePath = [&lrtPackageRoot](std::string_view tail) {
        return (lrtPackageRoot / fs::path(tail)).lexically_normal().generic_string();
    };
    const auto packageExists = [&lrtPackageRoot](std::string_view tail) {
        return fs::exists((lrtPackageRoot / fs::path(tail)).lexically_normal());
    };
    auto forcePackageTriplet = [&](ri::scene::Material& material,
                                   std::string_view albedoTail,
                                   std::string_view normalTail,
                                   std::string_view specTail) {
        material.baseColorTexture.clear();
        material.baseColorTextureFrames.clear();
        material.baseColorTextureFramesPerSecond = 0.0f;
        material.normalTexture.clear();
        material.ormTexture.clear();
        if (packageExists(albedoTail)) {
            material.baseColorTexture = packagePath(albedoTail);
        }
        if (packageExists(normalTail)) {
            material.normalTexture = packagePath(normalTail);
        }
        if (packageExists(specTail)) {
            material.ormTexture = packagePath(specTail);
        }
    };
    auto setLayeredStone = [&](ri::scene::Material& material,
                               const ri::math::Vec3& tint,
                               const float roughness,
                               std::string_view albedoTail,
                               std::string_view normalTail,
                               std::string_view specTail,
                               const ri::math::Vec2 tiling) {
        material.shadingModel = ri::scene::ShadingModel::Lit;
        material.materialStyle = ri::scene::MaterialStyle::Layered;
        material.materialWorkflow = ri::scene::MaterialWorkflow::MetalRough;
        material.baseColor = tint;
        material.emissiveColor = ri::math::Vec3{0.0f, 0.0f, 0.0f};
        material.metallic = 0.0f;
        material.roughness = roughness;
        material.transparent = false;
        material.additiveBlend = false;
        material.textureTiling = tiling;
        forcePackageTriplet(material, albedoTail, normalTail, specTail);
        material.detailTexture = material.baseColorTexture;
    };

    for (std::size_t materialIndex = 0; materialIndex < scene.MaterialCount(); ++materialIndex) {
        ri::scene::Material& material = scene.GetMaterial(static_cast<int>(materialIndex));
        const std::string key = ToLowerAscii(material.name);

        if (ContainsAny(key, {"old-road-moss-dirt", "leaf-litter-shadow"})) {
            setLayeredStone(material,
                            material.baseColor,
                            0.97f,
                            "tile/RT_coarse_dirt.png",
                            "tile/RT_coarse_dirt_n.png",
                            "tile/RT_coarse_dirt_s.png",
                            ri::math::Vec2{3.2f, 3.2f});
            continue;
        }
        if (ContainsAny(key, {"road-crack-moss"})) {
            setLayeredStone(material,
                            material.baseColor,
                            0.94f,
                            "tile/RT_mossy_cobblestone.png",
                            "tile/RT_mossy_cobblestone_n.png",
                            "tile/RT_mossy_cobblestone_s.png",
                            ri::math::Vec2{2.4f, 2.4f});
            continue;
        }
        if (ContainsAny(key, {"ruin-road-stone"})) {
            setLayeredStone(material,
                            material.baseColor,
                            0.92f,
                            "tile/RT_cobblestone.png",
                            "tile/RT_cobblestone_n.png",
                            "tile/RT_cobblestone_s.png",
                            ri::math::Vec2{2.0f, 2.0f});
            continue;
        }
        if (ContainsAny(key, {"hero-ruin-stone"})) {
            setLayeredStone(material,
                            material.baseColor,
                            0.96f,
                            "tile/RT_mossy_stone_bricks.png",
                            "tile/RT_mossy_stone_bricks_n.png",
                            "tile/RT_mossy_stone_bricks_s.png",
                            ri::math::Vec2{1.8f, 1.8f});
            continue;
        }
        if (ContainsAny(key, {"hero-ruin-dark-stone"})) {
            setLayeredStone(material,
                            material.baseColor,
                            0.98f,
                            "tile/RT_cobbled_deepslate.png",
                            "tile/RT_cobbled_deepslate_n.png",
                            "tile/RT_cobbled_deepslate_s.png",
                            ri::math::Vec2{1.6f, 1.6f});
            continue;
        }
        if (ContainsAny(key, {"hero-ruin-rubble"})) {
            setLayeredStone(material,
                            material.baseColor,
                            0.95f,
                            "tile/RT_mossy_cobblestone.png",
                            "tile/RT_mossy_cobblestone_n.png",
                            "tile/RT_mossy_cobblestone_s.png",
                            ri::math::Vec2{2.2f, 2.2f});
            continue;
        }
        if (ContainsAny(key, {"material-showcase-pad", "material-showcase-label"})) {
            setLayeredStone(material,
                            material.baseColor,
                            0.88f,
                            "tile/RT_smooth_stone.png",
                            "tile/RT_smooth_stone_n.png",
                            "tile/RT_smooth_stone_s.png",
                            ri::math::Vec2{2.0f, 2.0f});
            continue;
        }
        if (ContainsAny(key, {"material-showcase-oak-planks"})) {
            setLayeredStone(material,
                            material.baseColor,
                            0.74f,
                            "tile/rt2_oak_planks.png",
                            "tile/rt2_oak_planks_n.png",
                            "tile/rt2_oak_planks_s.png",
                            ri::math::Vec2{1.4f, 1.4f});
            continue;
        }
        if (ContainsAny(key, {"material-showcase-moss-stone"})) {
            setLayeredStone(material,
                            material.baseColor,
                            0.82f,
                            "tile/RT_mossy_stone_bricks.png",
                            "tile/RT_mossy_stone_bricks_n.png",
                            "tile/RT_mossy_stone_bricks_s.png",
                            ri::math::Vec2{1.6f, 1.6f});
            continue;
        }
        if (ContainsAny(key, {"material-showcase-gold-block"})) {
            setLayeredStone(material,
                            material.baseColor,
                            0.22f,
                            "tile/rt2_gold_block.png",
                            "tile/rt2_gold_block_n.png",
                            "tile/rt2_gold_block_s.png",
                            ri::math::Vec2{1.0f, 1.0f});
            material.materialWorkflow = ri::scene::MaterialWorkflow::SpecGloss;
            material.metallic = 0.92f;
            continue;
        }
        if (ContainsAny(key, {"material-showcase-copper-block"})) {
            setLayeredStone(material,
                            material.baseColor,
                            0.28f,
                            "tile/rt2_copper_block.png",
                            "tile/rt2_copper_block_n.png",
                            "tile/rt2_copper_block_s.png",
                            ri::math::Vec2{1.0f, 1.0f});
            material.materialWorkflow = ri::scene::MaterialWorkflow::SpecGloss;
            material.metallic = 0.88f;
            continue;
        }
        if (ContainsAny(key, {"material-showcase-prismarine"})) {
            setLayeredStone(material,
                            material.baseColor,
                            0.52f,
                            "tile/rt2_prismarine_bricks.png",
                            "tile/rt2_prismarine_bricks_n.png",
                            "tile/rt2_prismarine_bricks_s.png",
                            ri::math::Vec2{1.4f, 1.4f});
            continue;
        }
        if (ContainsAny(key, {"material-showcase-deepslate"})) {
            setLayeredStone(material,
                            material.baseColor,
                            0.66f,
                            "tile/rt2_deepslate_tiles.png",
                            "tile/rt2_deepslate_tiles_n.png",
                            "tile/rt2_deepslate_tiles_s.png",
                            ri::math::Vec2{1.5f, 1.5f});
            continue;
        }
        if (ContainsAny(key, {"moss-cushion"})) {
            setLayeredStone(material,
                            material.baseColor,
                            0.90f,
                            "tile/RT_moss_block.png",
                            "tile/RT_moss_block_n.png",
                            "tile/RT_moss_block_s.png",
                            ri::math::Vec2{1.4f, 1.4f});
        }
    }
}

} // namespace

bool IsForestRuinsGameRoot(const fs::path& gameRoot) {
    const std::optional<ri::content::GameManifest> manifest = ri::content::LoadGameManifest(gameRoot / "manifest.json");
    return manifest.has_value() && manifest->id == "wilderness-ruins";
}

World BuildForestRuinsWorld(std::string_view sceneName, const fs::path& gameRoot) {
    World world{};
    world.scene = Scene(std::string(sceneName));
    Scene& scene = world.scene;
    const fs::path workspaceRoot = ri::content::DetectWorkspaceRoot(gameRoot);
    const ForestSceneLayout forest = MakeForestSceneLayout(workspaceRoot, gameRoot);
    const std::vector<BotdBillboardVariant> botdTreeVariants = DiscoverBotdBillboardVariants(forest.botdBillboardsRoot);
    const std::vector<fs::path> exportedConiferMeshes = DiscoverExportedConiferMeshes(forest.exportedMeshesRoot);
    const bool useBotdForestTrees = !botdTreeVariants.empty();
    const bool useExportedConiferMeshes = !exportedConiferMeshes.empty();
    const int botdVerticalBillboardMesh =
        useBotdForestTrees ? scene.AddMesh(MakeVerticalBillboardMesh(0.0f, 1.0f)) : ri::scene::kInvalidHandle;
    const ri::content::ScriptScalarMap gameplay = ri::content::LoadScriptScalars(gameRoot / "scripts" / "gameplay.riscript");

    world.handles.root = scene.CreateNode("WildernessRuinsLayer");

    LightNodeOptions sun{};
    sun.nodeName = "SunLight";
    sun.parent = world.handles.root;
    sun.transform.rotationDegrees = ri::math::Vec3{-42.0f, 34.0f, 0.0f};
    sun.light = Light{
        .name = "SunLight",
        .type = LightType::Directional,
        .color = ri::math::Vec3{1.00f, 0.94f, 0.82f},
        .intensity = 2.45f,
    };
    world.handles.sun = AddLightNode(scene, sun);

    LightNodeOptions bounce{};
    bounce.nodeName = "BounceFill";
    bounce.parent = world.handles.root;
    bounce.transform.position = ri::math::Vec3{0.0f, 4.0f, 0.0f};
    bounce.light = Light{
        .name = "BounceFill",
        .type = LightType::Point,
        .color = ri::math::Vec3{0.42f, 0.56f, 0.48f},
        .intensity = 2.85f,
        .range = 120.0f,
    };
    (void)AddLightNode(scene, bounce);

    OrbitCameraOptions orbitCamera{};
    orbitCamera.parent = world.handles.root;
    orbitCamera.camera = Camera{
        .name = "EditorOrbitCamera",
        .projection = ProjectionType::Perspective,
        .fieldOfViewDegrees = 80.0f,
        .nearClip = 0.05f,
        .farClip = 2000.0f,
    };
    orbitCamera.orbit = OrbitCameraState{
        .target = ri::math::Vec3{0.0f, 4.0f, 78.0f},
        .distance = 34.0f,
        .yawDegrees = 180.0f,
        .pitchDegrees = -28.0f,
    };
    world.handles.orbitCamera = AddOrbitCamera(scene, orbitCamera);

    GridHelperOptions grid{};
    grid.parent = world.handles.root;
    grid.nodeName = "ForestAuthoringGrid";
    grid.size = 260.0f;
    grid.color = ri::math::Vec3{0.16f, 0.22f, 0.16f};
    grid.transform.position = ri::math::Vec3{0.0f, 0.01f, 0.0f};
    world.handles.grid = AddGridHelper(scene, grid);

    AxesHelperOptions axes{};
    axes.parent = world.handles.root;
    axes.axisLength = 2.2f;
    axes.axisThickness = 0.08f;
    axes.transform.position = ri::math::Vec3{0.0f, 0.02f, 0.0f};
    world.handles.axes = AddAxesHelper(scene, axes);

    auto addCollider = [&](std::string id, const ri::math::Vec3& min, const ri::math::Vec3& max) {
        world.colliders.push_back(ri::trace::TraceCollider{
            .id = std::move(id),
            .bounds = ri::spatial::Aabb{.min = min, .max = max},
            .structural = true,
        });
    };

    ProceduralTerrainOptions terrain{};
    terrain.nodeName = "ForestTerrain";
    terrain.parent = world.handles.root;
    terrain.materialName = "wilderness-ground";
    terrain.baseColor = ri::math::Vec3{0.30f, 0.37f, 0.24f};
    terrain.baseColorTexture = ToAbsoluteAssetPath(forest.groundDiffuse);
    if (fs::exists(forest.groundNormal)) {
        terrain.normalTexture = ToAbsoluteAssetPath(forest.groundNormal);
    }
    terrain.textureTiling = ri::math::Vec2{36.0f, 36.0f};
    terrain.resolutionX = 96;
    terrain.resolutionZ = 96;
    terrain.sizeX = 520.0f;
    terrain.sizeZ = 520.0f;
    terrain.heightAmplitude = 1.15f;
    terrain.heightFrequency = 0.018f;
    terrain.detailAmplitude = 0.32f;
    terrain.detailFrequency = 0.092f;
    (void)AddProceduralTerrainNode(scene, terrain);

    auto sampleTerrainHeight = [&terrain](const float worldX, const float worldZ) -> float {
        const float ridge =
            std::sin(worldX * terrain.heightFrequency) * std::cos(worldZ * terrain.heightFrequency * 0.78f);
        const float swell = std::sin((worldX + worldZ) * terrain.heightFrequency * 0.45f);
        const float detail =
            std::sin(worldX * terrain.detailFrequency) * std::sin(worldZ * terrain.detailFrequency * 1.31f);
        return (ridge * terrain.heightAmplitude) + (swell * terrain.heightAmplitude * 0.55f)
            + (detail * terrain.detailAmplitude);
    };

    auto resolveImportedModelUniformScale = [](const fs::path& sourcePath) -> float {
        if (PathContainsAscii(sourcePath, "forest scene")) {
            return 0.055f;
        }
        if (PathContainsAscii(sourcePath, "generated/forestscene")) {
            return 1.0f;
        }
        if (PathContainsAscii(sourcePath, "post apocalypse")) {
            return 0.018f;
        }
        return 0.025f;
    };

    SceneModelTemplateRegistry scatterModelTemplates{};

    auto addImported = [&](const std::string& nodeName,
                           const fs::path& sourcePath,
                           const ri::math::Vec3& position,
                           const ri::math::Vec3& rotation,
                           const ri::math::Vec3& scale) {
        if (!fs::exists(sourcePath)) {
            return;
        }
        const float importScale = resolveImportedModelUniformScale(sourcePath);
        const ri::math::Vec3 calibratedScale = scale * importScale;
        std::string importError;
        (void)InstantiateSceneModelTemplate(
            scene,
            scatterModelTemplates,
            sourcePath,
            world.handles.root,
            nodeName,
            Transform{
                .position = position,
                .rotationDegrees = rotation,
                .scale = calibratedScale,
            },
            position.y,
            ImportedModelOptions{
                .sourcePath = sourcePath,
                .nodeName = "Template_" + sourcePath.stem().string(),
                .createPlaceholderOnFailure = true,
            },
            &importError);
    };

    auto groundPoint = [&](const float x, const float z) {
        return ri::math::Vec3{x, sampleTerrainHeight(x, z), z};
    };

    auto addPrimitive = [&](const std::string& nodeName,
                            const PrimitiveType primitive,
                            const ri::math::Vec3& position,
                            const ri::math::Vec3& rotation,
                            const ri::math::Vec3& scale,
                            const ri::math::Vec3& baseColor,
                            const std::string& materialName,
                            const ShadingModel shadingModel = ShadingModel::Lit) {
        PrimitiveNodeOptions primitiveOptions{};
        primitiveOptions.nodeName = nodeName;
        primitiveOptions.parent = world.handles.root;
        primitiveOptions.primitive = primitive;
        primitiveOptions.materialName = materialName;
        primitiveOptions.shadingModel = shadingModel;
        primitiveOptions.baseColor = baseColor;
        primitiveOptions.alphaCutoff = 1.0f;
        primitiveOptions.roughness = 0.96f;
        primitiveOptions.transform = Transform{
            .position = position,
            .rotationDegrees = rotation,
            .scale = scale,
        };
        return AddPrimitiveNode(scene, primitiveOptions);
    };

    auto addBoxOnGround = [&](const std::string& nodeName,
                              const float x,
                              const float z,
                              const ri::math::Vec3& size,
                              const ri::math::Vec3& rotation,
                              const ri::math::Vec3& color,
                              const std::string& materialName) {
        const float y = sampleTerrainHeight(x, z) + (size.y * 0.5f);
        return addPrimitive(nodeName,
                            PrimitiveType::Cube,
                            ri::math::Vec3{x, y, z},
                            rotation,
                            size,
                            color,
                            materialName);
    };

    const fs::path preferredBarkDiffuse =
        fs::exists(forest.botdTrunkDiffuse) ? forest.botdTrunkDiffuse : forest.barkDiffuse;
    const std::string barkTexturePath =
        fs::exists(preferredBarkDiffuse) ? ToAbsoluteAssetPath(preferredBarkDiffuse) : std::string{};
    int coniferTrunkBatch = ri::scene::kInvalidHandle;
    std::array<int, 4> coniferBillboardBatches{
        ri::scene::kInvalidHandle,
        ri::scene::kInvalidHandle,
        ri::scene::kInvalidHandle,
        ri::scene::kInvalidHandle,
    };
    if (!useBotdForestTrees) {
        const int coniferTrunkMesh = scene.AddMesh(Mesh{
            .name = "instanced-conifer-trunk",
            .primitive = PrimitiveType::Cube,
            .positions = {},
            .texCoords = {},
            .indices = {},
        });
        Material trunkMaterial{
            .name = "wet-conifer-bark",
            .shadingModel = ShadingModel::Lit,
            .baseColor = ri::math::Vec3{0.20f, 0.13f, 0.075f},
            .roughness = 0.98f,
        };
        if (!barkTexturePath.empty()) {
            trunkMaterial.baseColorTexture = barkTexturePath;
        }
        const int coniferTrunkMaterial = scene.AddMaterial(trunkMaterial);
        coniferTrunkBatch = scene.AddMeshInstanceBatch(MeshInstanceBatch{
            .name = "ConiferTrunkInstances",
            .parent = world.handles.root,
            .mesh = coniferTrunkMesh,
            .material = coniferTrunkMaterial,
            .transforms = {},
        });
        const fs::path atlasPath = gameRoot / "Assets" / "Generated" / "conifer_desktop_atlas_billboards.png";
        const int coniferBillboardMaterial = scene.AddMaterial(Material{
            .name = "speedtree-conifer-atlas-fallback",
            .shadingModel = ShadingModel::Lit,
            .baseColor = ri::math::Vec3{0.34f, 0.48f, 0.30f},
            .baseColorTexture = fs::exists(atlasPath) ? ToAbsoluteAssetPath(atlasPath) : std::string{},
            .roughness = 0.96f,
            .alphaCutoff = 0.42f,
            .doubleSided = true,
        });
        const std::array<int, 4> coniferBillboardMeshes{
            scene.AddMesh(MakeVerticalBillboardMesh(0.00f, 0.25f)),
            scene.AddMesh(MakeVerticalBillboardMesh(0.25f, 0.50f)),
            scene.AddMesh(MakeVerticalBillboardMesh(0.50f, 0.75f)),
            scene.AddMesh(MakeVerticalBillboardMesh(0.75f, 1.00f)),
        };
        const std::array<const char*, 4> batchNames{
            "ConiferAtlasBillboardA",
            "ConiferAtlasBillboardB",
            "ConiferAtlasBillboardC",
            "ConiferAtlasBillboardD",
        };
        for (std::size_t slot = 0; slot < coniferBillboardBatches.size(); ++slot) {
            coniferBillboardBatches[slot] = scene.AddMeshInstanceBatch(MeshInstanceBatch{
                .name = batchNames[slot],
                .parent = world.handles.root,
                .mesh = coniferBillboardMeshes[slot],
                .material = coniferBillboardMaterial,
                .transforms = {},
            });
        }
    }

    auto addInstancedConiferTree = [&](const ri::math::Vec3& root,
                                       const float height,
                                       const float crownRadius,
                                       const float yawDegrees,
                                       const int colorVariant) {
        if (coniferTrunkBatch == ri::scene::kInvalidHandle) {
            return;
        }
        const float safeHeight = std::max(height, 1.0f);
        const float safeRadius = std::max(crownRadius, 0.35f);
        const float trunkHeight = safeHeight * 0.43f;
        scene.AddMeshInstance(coniferTrunkBatch,
                              Transform{
                                  .position = ri::math::Vec3{root.x, root.y + (trunkHeight * 0.5f), root.z},
                                  .rotationDegrees = ri::math::Vec3{0.0f, yawDegrees, 0.0f},
                                  .scale = ri::math::Vec3{safeRadius * 0.16f, trunkHeight, safeRadius * 0.16f},
                              });
        const std::size_t variantIndex = static_cast<std::size_t>(std::abs(colorVariant) % 4);
        const float billboardWidth = std::max(safeRadius * 1.75f, safeHeight * 0.38f);
        if (coniferBillboardBatches[variantIndex] == ri::scene::kInvalidHandle) {
            return;
        }
        scene.AddMeshInstance(coniferBillboardBatches[variantIndex],
                              Transform{
                                  .position = root,
                                  .rotationDegrees = ri::math::Vec3{0.0f, yawDegrees, 0.0f},
                                  .scale = ri::math::Vec3{billboardWidth, safeHeight, 1.0f},
                              });
        scene.AddMeshInstance(coniferBillboardBatches[variantIndex],
                              Transform{
                                  .position = ri::math::Vec3{root.x, root.y + 0.04f, root.z},
                                  .rotationDegrees = ri::math::Vec3{0.0f, yawDegrees + 93.0f, 0.0f},
                                  .scale = ri::math::Vec3{billboardWidth * 0.92f, safeHeight * 0.98f, 1.0f},
                              });
    };

    const fs::path postApocRoot = workspaceRoot / "Assets" / "Source" / "Post apocalypse";
    const fs::path forestRocksRoot = forest.rocksRoot;
    const fs::path bushesRoot = forest.bushesMeshesRoot;
    const bool sourcePackReady = SourceForestPackReady(forest);

    const std::vector<ScatterAsset> fallbackRuinAssets{
        ScatterAsset{
            .namePrefix = "RuinBusStop",
            .sourcePath = postApocRoot / "Bus_Stop_Rural" / "MS_Bus_Stop_Rural.fbx",
            .minScale = 2.8f,
            .maxScale = 4.2f,
            .colliderRadius = 3.2f,
            .colliderHeight = 4.2f,
        },
        ScatterAsset{
            .namePrefix = "RuinBarrier",
            .sourcePath = postApocRoot / "Barrier_Road" / "MS_Barrier_Road.fbx",
            .minScale = 2.1f,
            .maxScale = 3.4f,
            .colliderRadius = 1.8f,
            .colliderHeight = 2.3f,
        },
        ScatterAsset{
            .namePrefix = "RuinFence",
            .sourcePath = postApocRoot / "Fence_Wood" / "MS_Fence_Wood.fbx",
            .minScale = 3.4f,
            .maxScale = 5.0f,
            .colliderRadius = 2.5f,
            .colliderHeight = 3.4f,
        },
        ScatterAsset{
            .namePrefix = "RuinPole",
            .sourcePath = postApocRoot / "Pole_Light_Rural" / "MS_Pole_Light_Rural.fbx",
            .minScale = 2.6f,
            .maxScale = 3.6f,
            .colliderRadius = 1.5f,
            .colliderHeight = 5.0f,
        },
        ScatterAsset{
            .namePrefix = "RuinBrickPile",
            .sourcePath = postApocRoot / "Brick_Pile" / "MS_Brick_Pile.fbx",
            .minScale = 1.8f,
            .maxScale = 3.0f,
            .colliderRadius = 1.6f,
            .colliderHeight = 1.8f,
        },
        ScatterAsset{
            .namePrefix = "RuinCableReel",
            .sourcePath = postApocRoot / "Cable_Reel" / "MS_Cable_Reel.fbx",
            .minScale = 1.8f,
            .maxScale = 2.8f,
            .colliderRadius = 1.3f,
            .colliderHeight = 1.8f,
        },
        ScatterAsset{
            .namePrefix = "RuinTower",
            .sourcePath = postApocRoot / "Fireplace_Tower" / "MS_Fireplace_Tower.fbx",
            .minScale = 2.4f,
            .maxScale = 3.7f,
            .colliderRadius = 2.7f,
            .colliderHeight = 5.2f,
        },
        ScatterAsset{
            .namePrefix = "RuinTent",
            .sourcePath = postApocRoot / "Tent_Civilian" / "MS_Tent_Civilian.fbx",
            .minScale = 2.4f,
            .maxScale = 3.8f,
            .colliderRadius = 2.4f,
            .colliderHeight = 2.5f,
        },
        ScatterAsset{
            .namePrefix = "RuinBillboard",
            .sourcePath = postApocRoot / "Sign_Billboard" / "MS_Sign_Billboard.fbx",
            .minScale = 2.7f,
            .maxScale = 4.2f,
            .colliderRadius = 3.3f,
            .colliderHeight = 4.8f,
        },
        ScatterAsset{
            .namePrefix = "RuinTransformer",
            .sourcePath = postApocRoot / "Transformer_Box" / "MS_Transformer_Box.fbx",
            .minScale = 1.8f,
            .maxScale = 3.1f,
            .colliderRadius = 1.8f,
            .colliderHeight = 2.5f,
        },
        ScatterAsset{
            .namePrefix = "RuinCrate",
            .sourcePath = postApocRoot / "Crate" / "MS_Crate.fbx",
            .minScale = 1.7f,
            .maxScale = 2.8f,
            .colliderRadius = 1.1f,
            .colliderHeight = 1.6f,
        },
    };
    const std::vector<ScatterAsset> fallbackRockAssets{
        ScatterAsset{
            .namePrefix = "Rock",
            .sourcePath = forestRocksRoot / "Rock1A.fbx",
            .minScale = 2.4f,
            .maxScale = 5.0f,
            .colliderRadius = 1.9f,
            .colliderHeight = 2.4f,
        },
        ScatterAsset{
            .namePrefix = "Rock",
            .sourcePath = forestRocksRoot / "Rock1B.fbx",
            .minScale = 2.2f,
            .maxScale = 4.8f,
            .colliderRadius = 1.8f,
            .colliderHeight = 2.3f,
        },
        ScatterAsset{
            .namePrefix = "Rock",
            .sourcePath = forestRocksRoot / "Rock2.fbx",
            .minScale = 2.4f,
            .maxScale = 5.1f,
            .colliderRadius = 2.0f,
            .colliderHeight = 2.6f,
        },
        ScatterAsset{
            .namePrefix = "Rock",
            .sourcePath = forestRocksRoot / "Rock3.fbx",
            .minScale = 2.6f,
            .maxScale = 5.6f,
            .colliderRadius = 2.2f,
            .colliderHeight = 2.8f,
        },
        ScatterAsset{
            .namePrefix = "Rock",
            .sourcePath = forestRocksRoot / "Rock4A.fbx",
            .minScale = 2.3f,
            .maxScale = 4.9f,
            .colliderRadius = 1.9f,
            .colliderHeight = 2.4f,
        },
        ScatterAsset{
            .namePrefix = "Rock",
            .sourcePath = forestRocksRoot / "Rock5A.fbx",
            .minScale = 2.4f,
            .maxScale = 5.0f,
            .colliderRadius = 2.0f,
            .colliderHeight = 2.5f,
        },
        ScatterAsset{
            .namePrefix = "Rock",
            .sourcePath = forestRocksRoot / "Rock6C.fbx",
            .minScale = 2.8f,
            .maxScale = 5.8f,
            .colliderRadius = 2.3f,
            .colliderHeight = 3.0f,
        },
    };
    std::vector<ScatterAsset> ruinAssets =
        MergeScatterAssets(DiscoverPostApocalypseScatterAssets(postApocRoot), fallbackRuinAssets);
    std::vector<ScatterAsset> rockAssets =
        MergeScatterAssets(DiscoverRockScatterAssets(forestRocksRoot), fallbackRockAssets);
    const std::vector<ScatterAsset> fallbackBushAssets{
        ScatterAsset{.namePrefix = "Bush", .sourcePath = bushesRoot / "Bush01.FBX", .minScale = 1.8f, .maxScale = 3.4f, .colliderRadius = 1.4f, .colliderHeight = 1.5f},
        ScatterAsset{.namePrefix = "Bush", .sourcePath = bushesRoot / "Bush02.FBX", .minScale = 1.8f, .maxScale = 3.5f, .colliderRadius = 1.4f, .colliderHeight = 1.5f},
        ScatterAsset{.namePrefix = "Bush", .sourcePath = bushesRoot / "Bush03.FBX", .minScale = 1.8f, .maxScale = 3.6f, .colliderRadius = 1.4f, .colliderHeight = 1.6f},
        ScatterAsset{.namePrefix = "Bush", .sourcePath = bushesRoot / "Bush04.FBX", .minScale = 1.8f, .maxScale = 3.6f, .colliderRadius = 1.4f, .colliderHeight = 1.6f},
        ScatterAsset{.namePrefix = "Bush", .sourcePath = bushesRoot / "Bush05.FBX", .minScale = 1.8f, .maxScale = 3.7f, .colliderRadius = 1.4f, .colliderHeight = 1.7f},
    };
    std::vector<ScatterAsset> bushAssets = MergeScatterAssets(DiscoverBushScatterAssets(bushesRoot), fallbackBushAssets);

    auto addConiferBillboardCluster = [&](const std::string& nodeName,
                                          const ri::math::Vec3& root,
                                          const BotdBillboardVariant& variant,
                                          const float scale,
                                          const float yawBase,
                                          const float alphaCutoff,
                                          const bool addTrunk) {
        if (!fs::exists(variant.albedoPath)) {
            return;
        }
        const float treeWidth = variant.width * scale;
        const float treeHeight = variant.height * scale;
        const bool placeTrunk = addTrunk && !useExportedConiferMeshes;
        if (placeTrunk) {
            PrimitiveNodeOptions trunk{};
            trunk.nodeName = nodeName + "_Trunk";
            trunk.parent = world.handles.root;
            trunk.primitive = PrimitiveType::Cube;
            trunk.materialName = "forest-bark-trunk";
            trunk.shadingModel = ShadingModel::Lit;
            trunk.baseColor = ri::math::Vec3{0.55f, 0.48f, 0.40f};
            if (!barkTexturePath.empty()) {
                trunk.baseColorTexture = barkTexturePath;
            }
            trunk.roughness = 0.98f;
            trunk.transform.position = {root.x, root.y + (treeHeight * 0.28f), root.z};
            trunk.transform.rotationDegrees = {0.0f, yawBase, 0.0f};
            trunk.transform.scale = {treeWidth * 0.07f, treeHeight * 0.48f, treeWidth * 0.07f};
            (void)AddPrimitiveNode(scene, trunk);
        }
        if (botdVerticalBillboardMesh == ri::scene::kInvalidHandle) {
            return;
        }
        Material billboardMaterial{
            .name = "conifer-billboard-" + variant.label,
            .shadingModel = ShadingModel::Lit,
            .baseColor = ri::math::Vec3{1.0f, 1.0f, 1.0f},
            .baseColorTexture = ToAbsoluteAssetPath(variant.albedoPath),
            .roughness = 0.96f,
            .alphaCutoff = alphaCutoff,
            .doubleSided = true,
        };
        if (!variant.normalPath.empty() && fs::exists(variant.normalPath)) {
            billboardMaterial.normalTexture = ToAbsoluteAssetPath(variant.normalPath);
        }
        const int billboardMaterialHandle = scene.AddMaterial(billboardMaterial);
        const auto addCrossedBillboard = [&](const std::string& planeName,
                                             const float yawOffset,
                                             const float widthScale) {
            const int node = scene.CreateNode(planeName, world.handles.root);
            scene.GetNode(node).localTransform = Transform{
                .position = root,
                .rotationDegrees = ri::math::Vec3{0.0f, yawBase + yawOffset, 0.0f},
                .scale = ri::math::Vec3{treeWidth * widthScale, treeHeight, 1.0f},
            };
            scene.AttachMesh(node, botdVerticalBillboardMesh, billboardMaterialHandle);
        };
        addCrossedBillboard(nodeName + "_Billboard_1", 0.0f, 1.0f);
        addCrossedBillboard(nodeName + "_Billboard_2", 90.0f, 0.92f);
    };

    std::unordered_map<std::string, int> exportedConiferTemplates;
    const int coniferTemplateParent = scene.CreateNode("ConiferMeshTemplates", world.handles.root);
    scene.GetNode(coniferTemplateParent).localTransform.position = ri::math::Vec3{0.0f, -2000.0f, 0.0f};

    auto addForestConiferTree = [&](const ri::math::Vec3& root,
                                    const float targetHeight,
                                    const float yawDegrees,
                                    const int variantIndex,
                                    const std::string& nodePrefix) {
        if (useExportedConiferMeshes) {
            const fs::path& meshPath =
                exportedConiferMeshes[static_cast<std::size_t>(std::abs(variantIndex))
                                      % exportedConiferMeshes.size()];
            const std::string meshKey = meshPath.lexically_normal().generic_string();
            int templateRoot = kInvalidHandle;
            const auto cachedTemplate = exportedConiferTemplates.find(meshKey);
            if (cachedTemplate != exportedConiferTemplates.end()) {
                templateRoot = cachedTemplate->second;
            } else {
                std::string importError;
                templateRoot = AddModelNode(scene,
                                            ImportedModelOptions{
                                                .sourcePath = meshPath,
                                                .nodeName = "ConiferTemplate_" + meshPath.stem().string(),
                                                .parent = coniferTemplateParent,
                                                .transform = Transform{},
                                                .snapMeshBaseToGround = false,
                                            },
                                            &importError);
                if (templateRoot != kInvalidHandle) {
                    exportedConiferTemplates.emplace(meshKey, templateRoot);
                }
            }
            if (templateRoot == kInvalidHandle) {
                return;
            }
            const float meshScale = std::clamp(targetHeight / 24.0f, 0.35f, 2.4f);
            const Transform placement{
                .position = root,
                .rotationDegrees = ri::math::Vec3{0.0f, yawDegrees, 0.0f},
                .scale = ri::math::Vec3{meshScale, meshScale, meshScale},
            };
            const int instanceRoot = CloneSceneSubtree(
                scene, templateRoot, world.handles.root, nodePrefix + "_Mesh", placement);
            if (instanceRoot != kInvalidHandle) {
                SnapNodeMeshBaseToGround(scene, instanceRoot, root.y);
            }
            return;
        }
        if (useBotdForestTrees) {
            const BotdBillboardVariant& variant =
                botdTreeVariants[static_cast<std::size_t>(std::abs(variantIndex))
                                 % botdTreeVariants.size()];
            const float heightScale = std::clamp(targetHeight / std::max(variant.height, 1.0f), 0.35f, 2.4f);
            addConiferBillboardCluster(nodePrefix,
                                     root,
                                     variant,
                                     heightScale,
                                     yawDegrees,
                                     0.28f,
                                     false);
            return;
        }
        addInstancedConiferTree(root, targetHeight, targetHeight * 0.22f, yawDegrees, variantIndex);
    };

    const int scatterSeed = ri::content::ScriptScalarOrInt(gameplay, "scatter_seed", 1337);
    const float scatterExtent = ri::content::ScriptScalarOrClamped(gameplay, "scatter_extent", 170.0f, 60.0f, 300.0f);
    const int clearingCount = ri::content::ScriptScalarOrIntClamped(gameplay, "scatter_clearings", 7, 1, 20);
    const float clearingRadiusMin =
        ri::content::ScriptScalarOrClamped(gameplay, "scatter_clearing_radius_min", 12.0f, 4.0f, 40.0f);
    const float clearingRadiusMax = std::max(
        clearingRadiusMin,
        ri::content::ScriptScalarOrClamped(gameplay, "scatter_clearing_radius_max", 26.0f, clearingRadiusMin, 60.0f));
    const int ruinCount = ri::content::ScriptScalarOrIntClamped(gameplay, "scatter_ruin_count", 14, 4, 180);
    const int rockCount = ri::content::ScriptScalarOrIntClamped(gameplay, "scatter_rock_count", 28, 8, 300);
    const int bushCount = ri::content::ScriptScalarOrIntClamped(gameplay, "scatter_bush_count", 40, 8, 500);
    const int treeCount = ri::content::ScriptScalarOrIntClamped(gameplay, "scatter_tree_count", 90, 0, 1200);
    const bool generateTrees = useExportedConiferMeshes
        || ri::content::ScriptScalarOrBool(gameplay, "scatter_tree_proxies", true)
        || ri::content::ScriptScalarOrBool(gameplay, "scatter_tree_billboards", true);

    DeterministicRng rng{};
    rng.state ^= static_cast<std::uint64_t>(scatterSeed) * 0x9E3779B97F4A7C15ULL;
    std::vector<Clearing> clearings;
    clearings.reserve(static_cast<std::size_t>(clearingCount));
    for (int i = 0; i < clearingCount; ++i) {
        clearings.push_back(Clearing{
            .center = ri::math::Vec3{
                rng.NextRange(-scatterExtent * 0.85f, scatterExtent * 0.85f),
                0.0f,
                rng.NextRange(-scatterExtent * 0.85f, scatterExtent * 0.85f),
            },
            .radius = rng.NextRange(clearingRadiusMin, clearingRadiusMax),
        });
    }

    const ri::math::Vec3 guaranteedSpawn{0.0f, 0.0f, 76.0f};
    const float spawnReserveRadius = 11.5f;
    clearings.push_back(Clearing{.center = guaranteedSpawn, .radius = spawnReserveRadius});
    for (float z = 82.0f; z >= -34.0f; z -= 12.0f) {
        clearings.push_back(Clearing{
            .center = ri::math::Vec3{RuinPathCenterX(z), 0.0f, z},
            .radius = RuinPathHalfWidth(z) + 2.7f,
        });
    }
    clearings.push_back(Clearing{.center = ri::math::Vec3{0.0f, 0.0f, 34.0f}, .radius = 20.0f});
    clearings.push_back(Clearing{.center = ri::math::Vec3{-8.0f, 0.0f, 10.0f}, .radius = 15.0f});
    clearings.push_back(Clearing{.center = ri::math::Vec3{12.0f, 0.0f, -18.0f}, .radius = 13.5f});
    const int terrainColliderGrid = 30;
    const float terrainMinX = -terrain.sizeX * 0.5f;
    const float terrainMinZ = -terrain.sizeZ * 0.5f;
    const float terrainTileSizeX = terrain.sizeX / static_cast<float>(terrainColliderGrid);
    const float terrainTileSizeZ = terrain.sizeZ / static_cast<float>(terrainColliderGrid);
    for (int z = 0; z < terrainColliderGrid; ++z) {
        for (int x = 0; x < terrainColliderGrid; ++x) {
            const float x0 = terrainMinX + (static_cast<float>(x) * terrainTileSizeX);
            const float x1 = x0 + terrainTileSizeX;
            const float z0 = terrainMinZ + (static_cast<float>(z) * terrainTileSizeZ);
            const float z1 = z0 + terrainTileSizeZ;
            const float h00 = sampleTerrainHeight(x0, z0);
            const float h10 = sampleTerrainHeight(x1, z0);
            const float h01 = sampleTerrainHeight(x0, z1);
            const float h11 = sampleTerrainHeight(x1, z1);
            const float maxHeight = std::max(std::max(h00, h10), std::max(h01, h11));
            const float minHeight = std::min(std::min(h00, h10), std::min(h01, h11));
            addCollider("terrain-cell-" + std::to_string(z) + "-" + std::to_string(x),
                        ri::math::Vec3{x0 - 0.08f, minHeight - 6.0f, z0 - 0.08f},
                        ri::math::Vec3{x1 + 0.08f, maxHeight + 0.35f, z1 + 0.08f});
        }
    }

    auto spawnScatter = [&](const std::vector<ScatterAsset>& assets, const int count, const bool useClearings) {
        if (assets.empty() || count <= 0) {
            return;
        }
        for (int i = 0; i < count; ++i) {
            const ScatterAsset& asset = assets[static_cast<std::size_t>(rng.NextIndex(static_cast<int>(assets.size())))];
            if (!fs::exists(asset.sourcePath)) {
                continue;
            }
            float scaleUniform = rng.NextRange(asset.minScale, asset.maxScale);
            ri::math::Vec3 position = PickScatterPoint(rng, scatterExtent, useClearings, clearings);
            position.y = sampleTerrainHeight(position.x, position.z);
            const ri::math::Vec3 spawnDelta = position - guaranteedSpawn;
            if ((spawnDelta.x * spawnDelta.x) + (spawnDelta.z * spawnDelta.z) < (spawnReserveRadius * spawnReserveRadius)) {
                position = PickScatterPoint(rng, scatterExtent, useClearings, clearings);
                scaleUniform *= 0.98f;
            }
            const ri::math::Vec3 rotation{0.0f, rng.NextRange(-180.0f, 180.0f), 0.0f};
            const ri::math::Vec3 scale{scaleUniform, scaleUniform, scaleUniform};
            const std::string nodeName = asset.namePrefix + "_" + std::to_string(i + 1) + "_"
                + std::to_string(scatterSeed);
            addImported(nodeName, asset.sourcePath, position, rotation, scale);
            if (asset.colliderRadius > 0.0f && asset.colliderHeight > 0.0f) {
                addCollider("scatter-" + nodeName,
                            ri::math::Vec3{
                                position.x - (asset.colliderRadius * scaleUniform),
                                position.y,
                                position.z - (asset.colliderRadius * scaleUniform),
                            },
                            ri::math::Vec3{
                                position.x + (asset.colliderRadius * scaleUniform),
                                position.y + (asset.colliderHeight * scaleUniform),
                                position.z + (asset.colliderRadius * scaleUniform),
                            });
            }
        }
    };


    const int roadSegmentCount = sourcePackReady ? 10 : 27;
    const int roadCrackCount = sourcePackReady ? 6 : 42;
    const int floorShadowPatchCount = (sourcePackReady || useExportedConiferMeshes) ? 0 : 76;
    const int roadEdgeStoneCount = sourcePackReady ? 8 : 34;
    const int heroRubbleCount = sourcePackReady ? 5 : 18;

    for (int i = 0; i < roadSegmentCount; ++i) {
        const float z = 77.0f - (static_cast<float>(i) * 4.8f);
        const float x = RuinPathCenterX(z);
        const float width = (RuinPathHalfWidth(z) * 1.12f) + ((i % 3 == 0) ? 0.75f : 0.0f);
        addBoxOnGround("OldForestRoad_" + std::to_string(i + 1),
                       x,
                       z,
                       ri::math::Vec3{width, 0.07f, 5.25f},
                       ri::math::Vec3{0.0f, (i % 2 == 0) ? 1.8f : -2.3f, 0.0f},
                       (i % 2 == 0) ? ri::math::Vec3{0.095f, 0.098f, 0.087f}
                                    : ri::math::Vec3{0.070f, 0.078f, 0.068f},
                       "old-road-moss-dirt");
    }
    for (int i = 0; i < roadCrackCount; ++i) {
        const float z = 75.0f - (static_cast<float>(i) * 3.1f);
        const float x = RuinPathCenterX(z) + (std::sin(static_cast<float>(i) * 1.9f) * 1.5f);
        addBoxOnGround("RoadMossCrack_" + std::to_string(i + 1),
                       x,
                       z,
                       ri::math::Vec3{1.0f + static_cast<float>((i * 7) % 5) * 0.38f, 0.085f, 0.23f},
                       ri::math::Vec3{0.0f, static_cast<float>((i * 41) % 100) - 50.0f, 0.0f},
                       (i % 3 == 0) ? ri::math::Vec3{0.045f, 0.120f, 0.040f}
                                    : ri::math::Vec3{0.025f, 0.035f, 0.030f},
                       "road-crack-moss");
    }
    for (int i = 0; i < floorShadowPatchCount; ++i) {
        const float z = 77.0f - (static_cast<float>(i) * 2.25f);
        const float side = (i % 2 == 0) ? -1.0f : 1.0f;
        const float x = RuinPathCenterX(z)
            + side * (RuinPathHalfWidth(z) + 3.2f + static_cast<float>((i * 13) % 7) * 0.8f);
        addBoxOnGround("ForestFloorShadowPatch_" + std::to_string(i + 1),
                       x,
                       z,
                       ri::math::Vec3{
                           2.6f + static_cast<float>((i * 5) % 6) * 0.55f,
                           0.035f,
                           1.6f + static_cast<float>((i * 11) % 5) * 0.45f,
                       },
                       ri::math::Vec3{0.0f, static_cast<float>((i * 37) % 180), 0.0f},
                       (i % 4 == 0) ? ri::math::Vec3{0.045f, 0.090f, 0.038f}
                                    : ri::math::Vec3{0.050f, 0.045f, 0.032f},
                       "leaf-litter-shadow");
    }
    for (int i = 0; i < roadEdgeStoneCount; ++i) {
        const float z = 72.0f - (static_cast<float>(i) * 3.2f);
        const float side = (i % 2 == 0) ? -1.0f : 1.0f;
        const float x = RuinPathCenterX(z) + (side * (RuinPathHalfWidth(z) + 1.8f + (static_cast<float>(i % 4) * 0.35f)));
        const ri::math::Vec3 rockSize{
            0.65f + (static_cast<float>((i * 7) % 5) * 0.12f),
            0.18f + (static_cast<float>((i * 5) % 4) * 0.08f),
            0.55f + (static_cast<float>((i * 3) % 6) * 0.13f),
        };
        addBoxOnGround("RoadEdgeStone_" + std::to_string(i + 1),
                       x,
                       z,
                       rockSize,
                       ri::math::Vec3{0.0f, static_cast<float>((i * 23) % 180), static_cast<float>((i % 3) - 1) * 4.0f},
                       (i % 3 == 0) ? ri::math::Vec3{0.35f, 0.38f, 0.31f}
                                    : ri::math::Vec3{0.43f, 0.42f, 0.36f},
                       "ruin-road-stone");
    }

    {
        struct MaterialShowcaseSample {
            std::string name;
            std::string materialKey;
            float x;
            float z;
            ri::math::Vec3 color;
        };
        const std::array<MaterialShowcaseSample, 6> samples{{
            {"MaterialShowcase_OakPlanks", "material-showcase-oak-planks", 12.0f, 68.0f, {0.86f, 0.70f, 0.46f}},
            {"MaterialShowcase_MossStone", "material-showcase-moss-stone", 14.5f, 68.0f, {0.52f, 0.56f, 0.48f}},
            {"MaterialShowcase_GoldBlock", "material-showcase-gold-block", 17.0f, 68.0f, {1.0f, 0.84f, 0.36f}},
            {"MaterialShowcase_CopperBlock", "material-showcase-copper-block", 19.5f, 68.0f, {0.94f, 0.58f, 0.42f}},
            {"MaterialShowcase_Prismarine", "material-showcase-prismarine", 22.0f, 68.0f, {0.55f, 0.82f, 0.78f}},
            {"MaterialShowcase_Deepslate", "material-showcase-deepslate", 24.5f, 68.0f, {0.55f, 0.56f, 0.60f}},
        }};
        addBoxOnGround("MaterialShowcase_Pad",
                       18.0f,
                       64.5f,
                       {16.0f, 0.12f, 4.8f},
                       {0.0f, 0.0f, 0.0f},
                       {0.34f, 0.36f, 0.32f},
                       "material-showcase-pad");
        for (const MaterialShowcaseSample& sample : samples) {
            addBoxOnGround(sample.name,
                           sample.x,
                           sample.z,
                           {1.05f, 1.05f, 1.05f},
                           {0.0f, static_cast<float>((sample.x + sample.z) * 3.0f), 0.0f},
                           sample.color,
                           sample.materialKey);
        }
        addBoxOnGround("MaterialShowcase_LabelPost",
                       10.5f,
                       64.5f,
                       {0.18f, 2.4f, 0.18f},
                       {0.0f, 0.0f, 0.0f},
                       {0.72f, 0.74f, 0.68f},
                       "material-showcase-label");
    }

    spawnScatter(ruinAssets, ruinCount, true);
    spawnScatter(rockAssets, rockCount, false);
    spawnScatter(bushAssets, bushCount, false);

    const int pathWallTreeCount = sourcePackReady ? 34 : 86;
    const int pathTallTreeCount = sourcePackReady ? 6 : 14;
    const int pathSmallTreeCount = sourcePackReady ? 22 : 68;
    for (int i = 0; i < pathWallTreeCount; ++i) {
        const float z = 78.0f - (static_cast<float>(i) * (sourcePackReady ? 2.35f : 1.95f));
        const float side = (i % 2 == 0) ? -1.0f : 1.0f;
        const float forestWallOffset = RuinPathHalfWidth(z) + 7.0f + static_cast<float>((i * 11) % 7) * 1.05f;
        const float x = RuinPathCenterX(z) + side * forestWallOffset;
        const ri::math::Vec3 root{x, sampleTerrainHeight(x, z), z};
        addForestConiferTree(root,
                             9.8f + static_cast<float>((i * 17) % 9) * 0.72f,
                             static_cast<float>((i * 29) % 360),
                             i,
                             "PathConifer_" + std::to_string(i + 1));
    }

    for (int i = 0; i < pathTallTreeCount; ++i) {
        const float z = 74.0f - (static_cast<float>(i) * 3.8f);
        const float side = (i % 2 == 0) ? -1.0f : 1.0f;
        const float x = RuinPathCenterX(z) + side * (14.0f + static_cast<float>((i * 5) % 4) * 2.6f);
        const ri::math::Vec3 root{x, sampleTerrainHeight(x, z), z};
        addForestConiferTree(root,
                             11.6f + static_cast<float>((i * 19) % 5) * 0.9f,
                             static_cast<float>((i * 43) % 360),
                             i + 1,
                             "PathConiferTall_" + std::to_string(i + 1));
    }

    for (int i = 0; i < pathSmallTreeCount; ++i) {
        const float z = 74.0f - (static_cast<float>(i) * 2.15f);
        const float side = (i % 2 == 0) ? -1.0f : 1.0f;
        const float x = RuinPathCenterX(z) + side * (RuinPathHalfWidth(z) + 2.2f + static_cast<float>((i * 5) % 4) * 0.48f);
        const ri::math::Vec3 root{x, sampleTerrainHeight(x, z), z};
        addForestConiferTree(root,
                             2.8f + static_cast<float>((i * 7) % 5) * 0.46f,
                             static_cast<float>((i * 47) % 360),
                             i + 2,
                             "PathConiferSmall_" + std::to_string(i + 1));
    }

    for (int i = 0; i < 18; ++i) {
        const float angle = static_cast<float>(i) * 23.0f;
        const float x = std::sin(ri::math::DegreesToRadians(angle)) * (8.8f + static_cast<float>(i % 4));
        const float z = 25.0f + std::cos(ri::math::DegreesToRadians(angle)) * (9.0f + static_cast<float>((i + 1) % 5));
        const ScatterAsset& bush = bushAssets[static_cast<std::size_t>(i % static_cast<int>(bushAssets.size()))];
        addImported("HeroBushCluster_" + std::to_string(i + 1),
                    bush.sourcePath,
                    groundPoint(x, z),
                    {0.0f, static_cast<float>((i * 31) % 360), 0.0f},
                    {4.8f + static_cast<float>(i % 3) * 0.8f,
                     4.8f + static_cast<float>(i % 3) * 0.8f,
                     4.8f + static_cast<float>(i % 3) * 0.8f});
    }

    if (generateTrees && treeCount > 0) {
        for (int i = 0; i < treeCount; ++i) {
            const ri::math::Vec3 root = PickScatterPoint(rng, scatterExtent, false, clearings);
            const float groundY = sampleTerrainHeight(root.x, root.z);
            const float yawBase = rng.NextRange(-180.0f, 180.0f);
            addForestConiferTree(ri::math::Vec3{root.x, groundY, root.z},
                                 rng.NextRange(7.2f, 15.5f),
                                 yawBase,
                                 rng.NextIndex(static_cast<int>(botdTreeVariants.empty() ? 4 : botdTreeVariants.size())),
                                 "ScatterConifer_" + std::to_string(i + 1));
        }
    }

        const ri::math::Vec3 stone{0.43f, 0.42f, 0.36f};
        const ri::math::Vec3 darkStone{0.28f, 0.30f, 0.27f};
        const ri::math::Vec3 moss{0.18f, 0.31f, 0.16f};

        addBoxOnGround("RuinedGateway_LeftPier", -4.8f, 38.0f, {1.6f, 5.8f, 1.5f}, {0.0f, -4.0f, 0.0f}, stone, "hero-ruin-stone");
        addBoxOnGround("RuinedGateway_RightPier", 4.6f, 37.2f, {1.5f, 4.7f, 1.5f}, {0.0f, 5.0f, 0.0f}, stone, "hero-ruin-stone");
        addBoxOnGround("RuinedGateway_BrokenLintel", -0.8f, 37.7f, {8.4f, 1.0f, 1.2f}, {0.0f, 2.0f, -7.0f}, stone, "hero-ruin-stone");
        addBoxOnGround("RuinedGateway_FallenLintel", 3.7f, 32.7f, {1.2f, 0.75f, 7.4f}, {0.0f, -32.0f, 10.0f}, darkStone, "hero-ruin-dark-stone");
        addBoxOnGround("OvergrownFoundation_LeftWall", -8.8f, 22.0f, {1.1f, 2.4f, 15.5f}, {0.0f, 3.0f, 0.0f}, stone, "hero-ruin-stone");
        addBoxOnGround("OvergrownFoundation_RightWall", 8.8f, 23.0f, {1.1f, 1.8f, 13.5f}, {0.0f, -6.0f, 0.0f}, stone, "hero-ruin-stone");
        addBoxOnGround("OvergrownFoundation_BackWall", 0.0f, 14.0f, {16.2f, 2.2f, 1.1f}, {0.0f, 2.0f, 0.0f}, stone, "hero-ruin-stone");
        addBoxOnGround("SunkenThreshold", 0.0f, 30.4f, {7.6f, 0.28f, 2.8f}, {0.0f, 0.0f, 0.0f}, darkStone, "hero-ruin-dark-stone");
        addBoxOnGround("CrackedStep_1", 0.0f, 33.8f, {6.4f, 0.22f, 1.5f}, {0.0f, 1.5f, 0.0f}, darkStone, "hero-ruin-dark-stone");
        addBoxOnGround("CrackedStep_2", -0.4f, 35.3f, {5.0f, 0.24f, 1.3f}, {0.0f, -2.0f, 0.0f}, darkStone, "hero-ruin-dark-stone");

        for (int i = 0; i < heroRubbleCount; ++i) {
            const float angle = static_cast<float>(i) * 37.0f;
            const float radius = 5.0f + static_cast<float>((i * 5) % 7) * 0.95f;
            const float x = std::sin(ri::math::DegreesToRadians(angle)) * radius;
            const float z = 23.0f + (std::cos(ri::math::DegreesToRadians(angle)) * radius);
            addBoxOnGround("RuinBlockRubble_" + std::to_string(i + 1),
                           x,
                           z,
                           {0.75f + static_cast<float>(i % 3) * 0.18f,
                            0.34f + static_cast<float>((i + 1) % 4) * 0.13f,
                            0.7f + static_cast<float>((i + 2) % 4) * 0.16f},
                           {static_cast<float>((i % 5) - 2) * 5.0f, angle, static_cast<float>((i % 7) - 3) * 3.0f},
                           (i % 4 == 0) ? moss : stone,
                           "hero-ruin-rubble");
        }

        const int mossCushionCount = sourcePackReady ? 4 : 11;
        for (int i = 0; i < mossCushionCount; ++i) {
            const float x = -6.0f + static_cast<float>(i) * 1.25f;
            const float z = 27.0f + std::sin(static_cast<float>(i) * 1.7f) * 4.0f;
            addPrimitive("MossCushion_" + std::to_string(i + 1),
                         PrimitiveType::Sphere,
                         ri::math::Vec3{x, sampleTerrainHeight(x, z) + 0.25f, z},
                         {},
                         {1.2f, 0.32f, 0.9f},
                         moss,
                         "moss-cushion",
                         ShadingModel::Lit);
        }

        const auto heroImport = [&](const std::string& nodeName,
                                    const fs::path& sourcePath,
                                    const float x,
                                    const float z,
                                    const ri::math::Vec3& rotation,
                                    const ri::math::Vec3& scale,
                                    const float colliderRadius,
                                    const float colliderHeight) {
            const ri::math::Vec3 p = groundPoint(x, z);
            addImported(nodeName, sourcePath, p, rotation, scale);
            if (colliderRadius > 0.0f && colliderHeight > 0.0f) {
                addCollider("hero-" + nodeName,
                            {p.x - colliderRadius, p.y, p.z - colliderRadius},
                            {p.x + colliderRadius, p.y + colliderHeight, p.z + colliderRadius});
            }
        };

        heroImport("HeroBusStop_ClaimedByMoss", postApocRoot / "Bus_Stop_Rural" / "MS_Bus_Stop_Rural.fbx",
                   -11.5f, 43.0f, {0.0f, 18.0f, 0.0f}, {4.4f, 4.4f, 4.4f}, 5.6f, 4.2f);
        heroImport("HeroRoadEndsSign", postApocRoot / "Sign_Public_Road_Ends" / "MS_Sign_Public_Road_Ends.fbx",
                   5.8f, 53.2f, {180.0f, -16.0f, 0.0f}, {3.3f, 3.3f, 3.3f}, 1.1f, 2.8f);
        heroImport("HeroLightPoleLean", postApocRoot / "Pole_Light_Rural" / "MS_Pole_Light_Rural.fbx",
                   -7.8f, 31.2f, {0.0f, 38.0f, -7.0f}, {3.5f, 3.5f, 3.5f}, 1.2f, 5.2f);
        heroImport("HeroFireplaceTowerBack", postApocRoot / "Fireplace_Tower" / "MS_Fireplace_Tower.fbx",
                   10.8f, 11.8f, {0.0f, -28.0f, 0.0f}, {3.6f, 3.6f, 3.6f}, 3.6f, 7.0f);
        heroImport("HeroPlankPile", postApocRoot / "Planks" / "MS_Plank_Pile.fbx",
                   -3.0f, 20.2f, {0.0f, 31.0f, 0.0f}, {3.2f, 3.2f, 3.2f}, 2.4f, 1.2f);
        heroImport("HeroMailboxTilted", postApocRoot / "Mailbox" / "MS_Mailbox.fbx",
                   7.4f, 45.4f, {0.0f, -24.0f, 9.0f}, {3.0f, 3.0f, 3.0f}, 0.9f, 1.7f);
        heroImport("HeroControlBox", postApocRoot / "Control_Box" / "MS_Control_Box.fbx",
                   -6.2f, 17.0f, {0.0f, 48.0f, 0.0f}, {2.7f, 2.7f, 2.7f}, 1.2f, 1.8f);
        heroImport("HeroPalletRotting", postApocRoot / "Pallet" / "MS_Pallet.fbx",
                   5.8f, 24.8f, {0.0f, -42.0f, 0.0f}, {3.1f, 3.1f, 3.1f}, 1.8f, 0.7f);

    world.playerRig = scene.CreateNode("PlayerRig", world.handles.root);
    const float spawnGroundY = sampleTerrainHeight(guaranteedSpawn.x, guaranteedSpawn.z);
    const ri::math::Vec3 spawnPosition{guaranteedSpawn.x, spawnGroundY + 2.4f, guaranteedSpawn.z};
    scene.GetNode(world.playerRig).localTransform.position = spawnPosition;
    world.playerCameraNode = scene.CreateNode("PlayerCameraNode", world.playerRig);
    const int playerCamera = scene.AddCamera(Camera{
        .name = "WildernessPlayerCamera",
        .projection = ProjectionType::Perspective,
        .fieldOfViewDegrees = 80.0f,
        .nearClip = 0.05f,
        .farClip = 1600.0f,
    });
    scene.AttachCamera(world.playerCameraNode, playerCamera);
    scene.GetNode(world.playerCameraNode).localTransform.rotationDegrees = ri::math::Vec3{0.0f, 0.0f, 0.0f};
    world.handles.crate = world.playerRig;
    world.handles.beacon = world.playerCameraNode;

    const fs::path engineTexturesRoot = workspaceRoot / "Assets" / "Textures";
    AbsolutizeMaterialTexturePaths(scene);
    ApplyForestRuinsShowcaseMaterials(scene, engineTexturesRoot);
    AbsolutizeMaterialTexturePaths(scene);

    return world;
}

void AnimateForestRuinsWorld(World&, double) {
}

StarterScene BuildForestRuinsEditorScene(std::string_view sceneName, const fs::path& gameRoot) {
    World world = BuildForestRuinsWorld(sceneName, gameRoot);
    return StarterScene{
        .scene = std::move(world.scene),
        .handles = world.handles,
    };
}

void AnimateForestRuinsEditorScene(StarterScene& starterScene, double elapsedSeconds) {
    (void)elapsedSeconds;
    (void)starterScene;
}

} // namespace ri::games::forestruins
