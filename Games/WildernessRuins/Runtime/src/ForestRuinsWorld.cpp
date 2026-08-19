#include "RawIron/Games/ForestRuins/ForestRuinsRuntime.h"

#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/ScriptScalars.h"
#include "RawIron/Core/Log.h"
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
#include <functional>
#include <limits>
#include <memory>
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
    /// Target height range in meters after import fit (not a raw FBX multiplier).
    float minScale = 1.0f;
    float maxScale = 1.0f;
    float colliderRadius = 0.0f;
    float colliderHeight = 0.0f;
};

/// Whole-pack FBX/GLB files often ship in cm / huge authoring units. Import at identity,
/// then rescale so world height / footprint match gameplay meters.
[[nodiscard]] bool FitImportedNodeToMeters(Scene& scene,
                                           const int nodeHandle,
                                           const float groundY,
                                           const float targetHeightMeters,
                                           const float maxFootprintMeters) {
    if (nodeHandle == kInvalidHandle) {
        return false;
    }
    const std::optional<WorldBounds> bounds = ComputeNodeWorldBounds(scene, nodeHandle, true);
    if (!bounds.has_value()) {
        return false;
    }
    const ri::math::Vec3 size = GetBoundsSize(*bounds);
    if (size.y <= 1.0e-4f) {
        return false;
    }
    const float safeTargetHeight = std::max(targetHeightMeters, 0.05f);
    const float safeMaxFootprint = std::max(maxFootprintMeters, safeTargetHeight * 0.5f);
    float uniform = safeTargetHeight / size.y;
    const float footprint = std::max(size.x, size.z);
    if (footprint > 1.0e-4f && (footprint * uniform) > safeMaxFootprint) {
        uniform = safeMaxFootprint / footprint;
    }
    // Guard against degenerate / inverted authoring bounds.
    uniform = std::clamp(uniform, 1.0e-6f, 1000.0f);
    Transform& local = scene.GetNode(nodeHandle).localTransform;
    local.scale = local.scale * uniform;
    SnapNodeMeshBaseToGround(scene, nodeHandle, groundY);
    return true;
}

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
    fs::path packRoot{};
    fs::path propsRoot{};
    fs::path treesRoot{};
    fs::path rocksRoot{};
    fs::path forestTexturesRoot{};
    fs::path looseTexturesRoot{};
    fs::path lrtRoot{};
    fs::path skiesRoot{};
    fs::path botdBillboardsRoot{};
    fs::path exportedMeshesRoot{};
    fs::path bushesMeshesRoot{};
    fs::path groundDiffuse{};
    fs::path groundNormal{};
    fs::path barkDiffuse{};
    bool usesPsxPack = false;
};

[[nodiscard]] fs::path PreferExistingPath(const fs::path& preferred, const fs::path& fallback) {
    std::error_code ec{};
    if (fs::exists(preferred, ec) && !ec) {
        return preferred;
    }
    return fallback;
}

[[nodiscard]] ForestSceneLayout MakeForestSceneLayout(const fs::path& workspaceRoot, const fs::path& gameRoot) {
    (void)workspaceRoot;
    ForestSceneLayout layout{};
    // Licensed PSX collection copied into the game. World/ holds distinct landmark packs
    // (house, gas station, trailer park, industrial…) — not the old MS roadside set.
    layout.packRoot = gameRoot / "assets" / "PsxPack";
    layout.propsRoot = layout.packRoot / "World";
    layout.treesRoot = layout.packRoot / "Nature" / "Trees";
    layout.rocksRoot = layout.packRoot / "Nature" / "Rocks";
    layout.forestTexturesRoot = layout.packRoot / "Nature" / "Forest";
    layout.looseTexturesRoot = layout.packRoot / "Textures";
    layout.lrtRoot = layout.packRoot / "LRT";
    layout.skiesRoot = layout.packRoot / "Skies";
    layout.exportedMeshesRoot = layout.treesRoot;
    layout.bushesMeshesRoot = layout.packRoot / "Nature" / "Plants";
    layout.botdBillboardsRoot.clear();
    layout.usesPsxPack = fs::is_directory(layout.packRoot) && fs::is_directory(layout.propsRoot);

    layout.groundDiffuse = PreferExistingPath(layout.forestTexturesRoot / "forestshortgrass.png",
                                              layout.forestTexturesRoot / "forestwildground.png");
    layout.groundDiffuse =
        PreferExistingPath(layout.groundDiffuse, layout.looseTexturesRoot / "grass_2.png");
    layout.groundNormal.clear();
    layout.barkDiffuse = PreferExistingPath(layout.looseTexturesRoot / "pine_bark_1.png",
                                            layout.looseTexturesRoot / "tree_bark_5.png");
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

void CollectMeshFilesRecursive(const fs::path& root,
                               const std::function<bool(const fs::path&)>& accept,
                               std::vector<fs::path>& out) {
    std::error_code ec{};
    if (!fs::is_directory(root, ec) || ec) {
        return;
    }
    for (const fs::directory_entry& entry :
         fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
        if (ec || !entry.is_regular_file() || !IsMeshExtension(entry.path())) {
            continue;
        }
        if (accept(entry.path())) {
            out.push_back(entry.path());
        }
    }
}

[[nodiscard]] std::vector<fs::path> DiscoverPsxTreeMeshes(const ForestSceneLayout& forest) {
    std::vector<fs::path> meshes;
    const auto acceptTree = [](const fs::path& path) {
        const std::string stem = ToLowerAscii(path.stem().string());
        if (stem.find("stump") != std::string::npos || stem.find("rock") != std::string::npos) {
            return false;
        }
        // Prefer GLB when both exist later via sort; accept pine/aspen/tree/dead packs.
        return stem.find("pine") != std::string::npos || stem.find("aspen") != std::string::npos
            || stem.find("tree") != std::string::npos || stem.find("dead") != std::string::npos
            || stem.find("fallen") != std::string::npos;
    };
    CollectMeshFilesRecursive(forest.treesRoot, acceptTree, meshes);
    CollectMeshFilesRecursive(forest.packRoot / "Nature" / "Retro" / "models" / "FBX" / "trees", acceptTree, meshes);
    CollectMeshFilesRecursive(forest.packRoot / "Nature" / "TreePack" / "models", acceptTree, meshes);
    CollectMeshFilesRecursive(forest.packRoot / "Nature" / "DeadTrees", acceptTree, meshes);
    // Prefer .glb then .fbx, drop winter variants for a greener forest.
    meshes.erase(std::remove_if(meshes.begin(),
                                meshes.end(),
                                [](const fs::path& path) {
                                    return ToLowerAscii(path.stem().string()).find("winter") != std::string::npos;
                                }),
                 meshes.end());
    std::sort(meshes.begin(), meshes.end(), [](const fs::path& a, const fs::path& b) {
        const bool aGlb = ToLowerAscii(a.extension().string()) == ".glb";
        const bool bGlb = ToLowerAscii(b.extension().string()) == ".glb";
        if (aGlb != bGlb) {
            return aGlb;
        }
        return a.generic_string() < b.generic_string();
    });
    constexpr std::size_t kMaxTreeVariants = 28U;
    if (meshes.size() > kMaxTreeVariants) {
        meshes.resize(kMaxTreeVariants);
    }
    return meshes;
}

[[nodiscard]] std::vector<ScatterAsset> DiscoverPsxRockAssets(const fs::path& rocksRoot) {
    std::vector<ScatterAsset> assets;
    std::error_code ec{};
    if (!fs::is_directory(rocksRoot, ec) || ec) {
        return assets;
    }
    for (const fs::directory_entry& entry : fs::directory_iterator(rocksRoot, ec)) {
        if (ec || !entry.is_regular_file() || !IsMeshExtension(entry.path())) {
            continue;
        }
        const std::string stemLower = ToLowerAscii(entry.path().stem().string());
        if (stemLower.find("rock") == std::string::npos) {
            continue;
        }
        const bool large = stemLower.find("large") != std::string::npos;
        assets.push_back(ScatterAsset{
            .namePrefix = "Rock",
            .sourcePath = entry.path(),
            .minScale = large ? 0.55f : 0.28f,
            .maxScale = large ? 1.35f : 0.75f,
            .colliderRadius = large ? 1.4f : 0.7f,
            .colliderHeight = large ? 1.2f : 0.7f,
        });
    }
    std::sort(assets.begin(), assets.end(), [](const ScatterAsset& a, const ScatterAsset& b) {
        return a.sourcePath.generic_string() < b.sourcePath.generic_string();
    });
    return assets;
}

[[nodiscard]] std::vector<ScatterAsset> DiscoverPsxBushAssets(const ForestSceneLayout& forest) {
    std::vector<ScatterAsset> assets;
    const auto pushMesh = [&assets](const fs::path& path, const char* prefix, const float minS, const float maxS) {
        assets.push_back(ScatterAsset{
            .namePrefix = prefix,
            .sourcePath = path,
            .minScale = minS,
            .maxScale = maxS,
            .colliderRadius = 0.9f,
            .colliderHeight = 1.3f,
        });
    };
    const auto acceptBush = [](const fs::path& path) {
        const std::string stem = ToLowerAscii(path.stem().string());
        return stem.find("bush") != std::string::npos || stem.find("bracken") != std::string::npos
            || stem.find("grass") != std::string::npos || stem.find("goldenrod") != std::string::npos
            || stem.find("aster") != std::string::npos || stem.find("dogbane") != std::string::npos
            || stem.find("rhus") != std::string::npos;
    };
    std::vector<fs::path> meshes;
    CollectMeshFilesRecursive(forest.packRoot / "Nature" / "Plants", acceptBush, meshes);
    CollectMeshFilesRecursive(forest.packRoot / "Nature" / "Retro" / "models" / "FBX" / "bushes", acceptBush, meshes);
    CollectMeshFilesRecursive(forest.packRoot / "Nature" / "TreePack" / "models", acceptBush, meshes);
    for (const fs::path& mesh : meshes) {
        const std::string stem = ToLowerAscii(mesh.stem().string());
        if (stem.find("winter") != std::string::npos) {
            continue;
        }
        const bool grass = stem.find("grass") != std::string::npos;
        pushMesh(mesh, grass ? "Grass" : "Brush", grass ? 0.35f : 0.55f, grass ? 0.85f : 1.35f);
    }
    std::vector<fs::path> debris;
    CollectMeshFilesRecursive(forest.treesRoot,
                              [](const fs::path& path) {
                                  const std::string stem = ToLowerAscii(path.stem().string());
                                  return stem.find("dead_branch") != std::string::npos
                                      || stem.find("stump") != std::string::npos;
                              },
                              debris);
    for (const fs::path& mesh : debris) {
        pushMesh(mesh, "Debris", 0.4f, 1.1f);
    }
    return assets;
}

[[nodiscard]] std::vector<ScatterAsset> DiscoverWorldPropScatter(const fs::path& worldRoot) {
    std::vector<ScatterAsset> assets;
    // Small props only — never scatter whole landmark / prop-pack scenes.
    const std::array<std::pair<const char*, const char*>, 6> smallProps{{
        {"Campfire", "MS_Campfire.fbx"},
        {"Firepot", "MS_Firepot.fbx"},
        {"Sawbuck", "MS_Sawbuck.fbx"},
        {"Planter", "MS_Planter_Box.fbx"},
        {"Board_Message", "MS_Board_Message.fbx"},
        {"Totem", "MS_Totem_Welcome.fbx"},
    }};
    for (const auto& [folder, file] : smallProps) {
        const fs::path path = worldRoot / folder / file;
        if (!fs::exists(path)) {
            continue;
        }
        const bool tall = folder == std::string_view{"Totem"};
        assets.push_back(ScatterAsset{
            .namePrefix = folder,
            .sourcePath = path,
            .minScale = tall ? 2.2f : 0.9f,
            .maxScale = tall ? 3.4f : 1.8f,
            .colliderRadius = tall ? 1.2f : 1.0f,
            .colliderHeight = tall ? 3.2f : 1.6f,
        });
    }
    return assets;
}

[[nodiscard]] bool SourceForestPackReady(const ForestSceneLayout& forest) {
    return forest.usesPsxPack && fs::exists(forest.groundDiffuse)
        && !DiscoverPsxTreeMeshes(forest).empty() && fs::is_directory(forest.propsRoot);
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

void ApplyForestRuinsShowcaseMaterials(ri::scene::Scene& scene, const fs::path& lrtPackageRoot) {
    if (lrtPackageRoot.empty() || !fs::exists(lrtPackageRoot)) {
        return;
    }
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
                            "tile/RT_oak_log.png",
                            "tile/RT_oak_log_n.png",
                            "tile/RT_oak_log_s.png",
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
                            "tile/RT_gold_block.png",
                            "tile/RT_gold_block_n.png",
                            "tile/RT_gold_block_s.png",
                            ri::math::Vec2{1.0f, 1.0f});
            material.materialWorkflow = ri::scene::MaterialWorkflow::SpecGloss;
            material.metallic = 0.92f;
            continue;
        }
        if (ContainsAny(key, {"material-showcase-copper-block"})) {
            setLayeredStone(material,
                            material.baseColor,
                            0.28f,
                            "tile/RT_raw_copper_block.png",
                            "tile/RT_raw_copper_block_n.png",
                            "tile/RT_raw_copper_block_s.png",
                            ri::math::Vec2{1.0f, 1.0f});
            material.materialWorkflow = ri::scene::MaterialWorkflow::SpecGloss;
            material.metallic = 0.88f;
            continue;
        }
        if (ContainsAny(key, {"material-showcase-prismarine"})) {
            setLayeredStone(material,
                            material.baseColor,
                            0.52f,
                            "tile/RT_prismarine_bricks.png",
                            "tile/RT_prismarine_bricks_n.png",
                            "tile/RT_prismarine_bricks_s.png",
                            ri::math::Vec2{1.4f, 1.4f});
            continue;
        }
        if (ContainsAny(key, {"material-showcase-deepslate"})) {
            setLayeredStone(material,
                            material.baseColor,
                            0.66f,
                            "tile/RT_deepslate_tiles.png",
                            "tile/RT_deepslate_tiles_n.png",
                            "tile/RT_deepslate_tiles_s.png",
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

void PushForestCollider(World& world, std::string id, const ri::math::Vec3& min, const ri::math::Vec3& max) {
    world.colliders.push_back(ri::trace::TraceCollider{
        .id = std::move(id),
        .bounds = ri::spatial::Aabb{.min = min, .max = max},
        .structural = true,
    });
}

[[nodiscard]] float SampleForestTerrainHeight(const ProceduralTerrainOptions& terrain,
                                              const float worldX,
                                              const float worldZ) {
    const float ridge =
        std::sin(worldX * terrain.heightFrequency) * std::cos(worldZ * terrain.heightFrequency * 0.78f);
    const float swell = std::sin((worldX + worldZ) * terrain.heightFrequency * 0.45f);
    const float detail =
        std::sin(worldX * terrain.detailFrequency) * std::sin(worldZ * terrain.detailFrequency * 1.31f);
    return (ridge * terrain.heightAmplitude) + (swell * terrain.heightAmplitude * 0.55f)
        + (detail * terrain.detailAmplitude);
}

struct ForestRuinsScenePopulateContext {
    World& world;
    Scene& scene;
    const ForestSceneLayout& forest;
    const fs::path& workspaceRoot;
    const fs::path& gameRoot;
    const ProceduralTerrainOptions& terrain;
    const ri::content::ScriptScalarMap& gameplay;
    bool useBotdForestTrees = false;
    bool useExportedConiferMeshes = false;
    int botdVerticalBillboardMesh = ri::scene::kInvalidHandle;
    const std::vector<BotdBillboardVariant>& botdTreeVariants;
    const std::vector<fs::path>& exportedConiferMeshes;
};

struct ForestRuinsHeroPopulateCallbacks {
    std::function<void(const std::string&,
                       const fs::path&,
                       float,
                       float,
                       const ri::math::Vec3&,
                       const ri::math::Vec3&,
                       float,
                       float)>
        heroImport;
    std::function<void(const std::string&, float, float, const ri::math::Vec3&, const ri::math::Vec3&, const ri::math::Vec3&, const std::string&)>
        addBoxOnGround;
    std::function<void(const std::string&,
                       PrimitiveType,
                       const ri::math::Vec3&,
                       const ri::math::Vec3&,
                       const ri::math::Vec3&,
                       const ri::math::Vec3&,
                       const std::string&,
                       ShadingModel)>
        addPrimitive;
    std::function<float(float, float)> sampleTerrainHeight;
    bool sourcePackReady = false;
    int heroRubbleCount = 0;
    fs::path postApocRoot;
};

struct ForestRuinsScatterBundle {
    World* world = nullptr;
    Scene* scene = nullptr;
    ProceduralTerrainOptions terrain{};
    SceneModelTemplateRegistry scatterModelTemplates{};
    std::unordered_map<std::string, int> exportedConiferTemplates;
    int coniferTemplateParent = kInvalidHandle;
    int worldRoot = kInvalidHandle;
    int coniferTrunkBatch = kInvalidHandle;
    std::array<int, 4> coniferBillboardBatches{
        kInvalidHandle,
        kInvalidHandle,
        kInvalidHandle,
        kInvalidHandle,
    };
    bool useBotdForestTrees = false;
    int botdVerticalBillboardMesh = kInvalidHandle;
    std::vector<BotdBillboardVariant> botdTreeVariants;
    std::vector<fs::path> exportedConiferMeshes;
    std::string barkTexturePath;
    std::function<bool(const std::string&,
                       const fs::path&,
                       const ri::math::Vec3&,
                       const ri::math::Vec3&,
                       const ri::math::Vec3&)>
        addImported;
    std::function<void(std::string, const ri::math::Vec3&, const ri::math::Vec3&)> addCollider;
    std::function<int(const std::string&, float, float, const ri::math::Vec3&, const ri::math::Vec3&, const ri::math::Vec3&, const std::string&)>
        addBoxOnGround;
    std::function<int(const std::string&,
                      PrimitiveType,
                      const ri::math::Vec3&,
                      const ri::math::Vec3&,
                      const ri::math::Vec3&,
                      const ri::math::Vec3&,
                      const std::string&,
                      ShadingModel)>
        addPrimitive;
    std::function<void(const ri::math::Vec3&, float, float, int, const std::string&)> addForestConiferTree;
    std::function<float(float, float)> sampleTerrainHeight;
    std::function<ri::math::Vec3(float, float)> groundPoint;
    std::vector<ScatterAsset> ruinAssets;
    std::vector<ScatterAsset> rockAssets;
    std::vector<ScatterAsset> bushAssets;
    DeterministicRng rng{};
    std::vector<Clearing> clearings;
    ri::math::Vec3 guaranteedSpawn{};
    float spawnReserveRadius = 11.5f;
    float scatterExtent = 170.0f;
    int scatterSeed = 1337;
    int ruinCount = 0;
    int rockCount = 0;
    int bushCount = 0;
    int treeCount = 0;
    bool generateTrees = false;
    bool sourcePackReady = false;
    bool useExportedConiferMeshes = false;
    float exportedTreeReferenceHeight = 24.0f;
    int botdTreeVariantCount = 0;
    int heroRubbleCount = 0;
    fs::path postApocRoot;
    ForestRuinsHeroPopulateCallbacks heroCallbacks;
};

[[nodiscard]] std::unique_ptr<ForestRuinsScatterBundle> PrepareForestRuinsScatterBundle(
    const ForestRuinsScenePopulateContext& ctx,
    ri::math::Vec3& guaranteedSpawn);

void ExecuteForestRuinsScatterBundle(ForestRuinsScatterBundle& bundle);

void WireForestRuinsScatterCallbacks(ForestRuinsScatterBundle& bundle);

void PopulateForestRuinsHeroCluster(const ForestRuinsHeroPopulateCallbacks& callbacks);

[[nodiscard]] ri::math::Vec3 PopulateForestRuinsSceneContent(const ForestRuinsScenePopulateContext& ctx) {
    ri::math::Vec3 guaranteedSpawn{};
    std::unique_ptr<ForestRuinsScatterBundle> bundle = PrepareForestRuinsScatterBundle(ctx, guaranteedSpawn);
    ExecuteForestRuinsScatterBundle(*bundle);
    PopulateForestRuinsHeroCluster(bundle->heroCallbacks);
    return guaranteedSpawn;
}

[[nodiscard]] std::unique_ptr<ForestRuinsScatterBundle> PrepareForestRuinsScatterBundle(
    const ForestRuinsScenePopulateContext& ctx,
    ri::math::Vec3& guaranteedSpawn) {
    auto bundle = std::make_unique<ForestRuinsScatterBundle>();
    World& world = ctx.world;
    Scene& scene = ctx.scene;
    const ForestSceneLayout& forest = ctx.forest;
    const fs::path& workspaceRoot = ctx.workspaceRoot;
    const ProceduralTerrainOptions& terrain = ctx.terrain;
    const ri::content::ScriptScalarMap& gameplay = ctx.gameplay;
    const bool useBotdForestTrees = ctx.useBotdForestTrees;
    const bool useExportedConiferMeshes = ctx.useExportedConiferMeshes;
    const int botdVerticalBillboardMesh = ctx.botdVerticalBillboardMesh;
    const std::vector<BotdBillboardVariant>& botdTreeVariants = ctx.botdTreeVariants;
    const std::vector<fs::path>& exportedConiferMeshes = ctx.exportedConiferMeshes;

    auto addCollider = [&](std::string id, const ri::math::Vec3& min, const ri::math::Vec3& max) {
        PushForestCollider(world, std::move(id), min, max);
    };

    auto sampleTerrainHeight = [&terrain](const float worldX, const float worldZ) -> float {
        return SampleForestTerrainHeight(terrain, worldX, worldZ);
    };

    SceneModelTemplateRegistry scatterModelTemplates{};

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

    const std::string barkTexturePath =
        fs::exists(forest.barkDiffuse) ? ToAbsoluteAssetPath(forest.barkDiffuse) : std::string{};
    int coniferTrunkBatch = ri::scene::kInvalidHandle;
    std::array<int, 4> coniferBillboardBatches{
        ri::scene::kInvalidHandle,
        ri::scene::kInvalidHandle,
        ri::scene::kInvalidHandle,
        ri::scene::kInvalidHandle,
    };
    if (!useBotdForestTrees && !useExportedConiferMeshes) {
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
        // Legacy Generated atlas was removed with the PsxPack migration. Prefer a pack leaf
        // albedo when available; otherwise keep an untextured green proxy (never a dead path).
        fs::path atlasPath = forest.packRoot / "LRT" / "tile" / "RT_oak_leaves.png";
        if (!fs::exists(atlasPath) && fs::exists(forest.groundDiffuse)) {
            atlasPath = forest.groundDiffuse;
        }
        const int coniferBillboardMaterial = scene.AddMaterial(Material{
            .name = "psx-conifer-billboard-fallback",
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

    const fs::path postApocRoot = forest.propsRoot; // PsxPack/World landmarks + small props
    const fs::path forestRocksRoot = forest.rocksRoot;
    const bool sourcePackReady = SourceForestPackReady(forest);
    (void)workspaceRoot;

    std::vector<ScatterAsset> ruinAssets = DiscoverWorldPropScatter(postApocRoot);
    std::vector<ScatterAsset> rockAssets = DiscoverPsxRockAssets(forestRocksRoot);
    std::vector<ScatterAsset> bushAssets = DiscoverPsxBushAssets(forest);

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
    std::unordered_map<std::string, float> exportedConiferReferenceHeights;
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
                                                // Fail closed: placeholders would be cloned across the forest.
                                                .createPlaceholderOnFailure = false,
                                            },
                                            &importError);
                if (templateRoot != kInvalidHandle) {
                    exportedConiferTemplates.emplace(meshKey, templateRoot);
                    float measured = forest.usesPsxPack ? 8.0f : 24.0f;
                    if (const std::optional<WorldBounds> bounds =
                            ComputeNodeWorldBounds(scene, templateRoot, true)) {
                        measured = std::max(0.25f, GetBoundsSize(*bounds).y);
                    }
                    exportedConiferReferenceHeights.emplace(meshKey, measured);
                }
            }
            if (templateRoot == kInvalidHandle) {
                return;
            }
            float referenceHeight = forest.usesPsxPack ? 8.0f : 24.0f;
            const auto refIt = exportedConiferReferenceHeights.find(meshKey);
            if (refIt != exportedConiferReferenceHeights.end()) {
                referenceHeight = refIt->second;
            }
            // Match FitImportedNodeToMeters: allow large unit-scale corrections (cm → m).
            const float meshScale = std::clamp(targetHeight / std::max(referenceHeight, 1.0e-3f), 1.0e-6f, 1000.0f);
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
    // Only suppress automatic proxies when the pack is actually content-ready.
    // An empty PsxPack directory is a content failure, not a reason to hide stand-ins
    // unless the script explicitly opts out of proxies.
    const bool packContentReady = SourceForestPackReady(forest);
    const bool allowTreeProxies = !packContentReady
        || ri::content::ScriptScalarOrBool(gameplay, "scatter_tree_proxies", false)
        || ri::content::ScriptScalarOrBool(gameplay, "scatter_tree_billboards", false);
    const bool generateTrees = useExportedConiferMeshes || allowTreeProxies;

    DeterministicRng rng{};
    rng.state ^= static_cast<std::uint64_t>(scatterSeed) * 0x9E3779B97F4A7C15ULL;

    // Fresh layout: spawn hollow + authored ruin pockets, then organic forest clearings.
    // Premise only — forest with human wreckage islands. No old highway / gateway parade.
    guaranteedSpawn = ri::math::Vec3{
        ri::content::ScriptScalarOr(gameplay, "spawn_x", 0.0f),
        0.0f,
        ri::content::ScriptScalarOr(gameplay, "spawn_z", 8.0f),
    };
    const float spawnReserveRadius = 14.0f;
    std::vector<Clearing> clearings;
    clearings.reserve(static_cast<std::size_t>(clearingCount) + 8U);
    clearings.push_back(Clearing{.center = guaranteedSpawn, .radius = spawnReserveRadius});
    clearings.push_back(Clearing{.center = ri::math::Vec3{-42.0f, 0.0f, 36.0f}, .radius = 18.0f}); // camp
    clearings.push_back(Clearing{.center = ri::math::Vec3{48.0f, 0.0f, 28.0f}, .radius = 16.0f});  // utility
    clearings.push_back(Clearing{.center = ri::math::Vec3{38.0f, 0.0f, -34.0f}, .radius = 17.0f}); // roadside remnant
    clearings.push_back(Clearing{.center = ri::math::Vec3{-36.0f, 0.0f, -40.0f}, .radius = 19.0f}); // cabin debris
    clearings.push_back(Clearing{.center = ri::math::Vec3{8.0f, 0.0f, -62.0f}, .radius = 15.0f});  // dump
    clearings.push_back(Clearing{.center = ri::math::Vec3{-8.0f, 0.0f, 54.0f}, .radius = 12.0f});  // overlook
    for (int i = 0; i < clearingCount; ++i) {
        clearings.push_back(Clearing{
            .center = ri::math::Vec3{
                rng.NextRange(-scatterExtent * 0.78f, scatterExtent * 0.78f),
                0.0f,
                rng.NextRange(-scatterExtent * 0.78f, scatterExtent * 0.78f),
            },
            .radius = rng.NextRange(clearingRadiusMin, clearingRadiusMax),
        });
    }
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

    ForestRuinsScatterBundle& out = *bundle;
    out.world = &world;
    out.scene = &scene;
    out.terrain = terrain;
    out.scatterModelTemplates = std::move(scatterModelTemplates);
    out.exportedConiferTemplates = std::move(exportedConiferTemplates);
    out.coniferTemplateParent = coniferTemplateParent;
    out.worldRoot = world.handles.root;
    out.coniferTrunkBatch = coniferTrunkBatch;
    out.coniferBillboardBatches = coniferBillboardBatches;
    out.useBotdForestTrees = useBotdForestTrees;
    out.useExportedConiferMeshes = useExportedConiferMeshes;
    out.exportedTreeReferenceHeight = forest.usesPsxPack ? 8.0f : 24.0f;
    out.botdVerticalBillboardMesh = botdVerticalBillboardMesh;
    out.botdTreeVariants = botdTreeVariants;
    out.exportedConiferMeshes = exportedConiferMeshes;
    out.barkTexturePath = barkTexturePath;
    out.ruinAssets = std::move(ruinAssets);
    out.rockAssets = std::move(rockAssets);
    out.bushAssets = std::move(bushAssets);
    out.rng = rng;
    out.clearings = std::move(clearings);
    out.guaranteedSpawn = guaranteedSpawn;
    out.spawnReserveRadius = spawnReserveRadius;
    out.scatterExtent = scatterExtent;
    out.scatterSeed = scatterSeed;
    out.ruinCount = ruinCount;
    out.rockCount = rockCount;
    out.bushCount = bushCount;
    out.treeCount = treeCount;
    out.generateTrees = generateTrees;
    out.sourcePackReady = sourcePackReady;
    out.botdTreeVariantCount = static_cast<int>(botdTreeVariants.size());
    out.heroRubbleCount = sourcePackReady ? 5 : 18;
    out.postApocRoot = postApocRoot;
    return bundle;
}

void WireForestRuinsScatterCallbacks(ForestRuinsScatterBundle& bundle) {
    ForestRuinsScatterBundle* self = &bundle;
    self->sampleTerrainHeight = [self](const float worldX, const float worldZ) {
        return SampleForestTerrainHeight(self->terrain, worldX, worldZ);
    };
    self->addCollider = [self](std::string id, const ri::math::Vec3& min, const ri::math::Vec3& max) {
        PushForestCollider(*self->world, std::move(id), min, max);
    };
    self->addImported = [self](const std::string& nodeName,
                               const fs::path& sourcePath,
                               const ri::math::Vec3& position,
                               const ri::math::Vec3& rotation,
                               const ri::math::Vec3& scale) -> bool {
        if (!fs::exists(sourcePath)) {
            return false;
        }
        const float targetHeight = std::max(scale.x, 0.05f);
        const float maxFootprint = std::max(scale.y > 1.0e-3f ? scale.y : (targetHeight * 3.2f), targetHeight);
        std::string importError;
        const int instance = InstantiateSceneModelTemplate(
            *self->scene,
            self->scatterModelTemplates,
            sourcePath,
            self->worldRoot,
            nodeName,
            Transform{
                .position = position,
                .rotationDegrees = rotation,
                .scale = ri::math::Vec3{1.0f, 1.0f, 1.0f},
            },
            position.y,
            ImportedModelOptions{
                .sourcePath = sourcePath,
                .nodeName = "Template_" + sourcePath.stem().string(),
                // Fail closed: placeholders would still return a handle and attract colliders.
                .createPlaceholderOnFailure = false,
            },
            &importError);
        if (instance == kInvalidHandle) {
            return false;
        }
        if (!FitImportedNodeToMeters(*self->scene, instance, position.y, targetHeight, maxFootprint)) {
            ri::core::LogInfo("Wilderness Ruins: WARN failed to fit imported scatter model '"
                              + sourcePath.filename().string() + "' to meters");
            return false;
        }
        return true;
    };
    self->addPrimitive = [self](const std::string& nodeName,
                                const PrimitiveType primitive,
                                const ri::math::Vec3& position,
                                const ri::math::Vec3& rotation,
                                const ri::math::Vec3& scale,
                                const ri::math::Vec3& baseColor,
                                const std::string& materialName,
                                const ShadingModel shadingModel) {
        PrimitiveNodeOptions primitiveOptions{};
        primitiveOptions.nodeName = nodeName;
        primitiveOptions.parent = self->worldRoot;
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
        return AddPrimitiveNode(*self->scene, primitiveOptions);
    };
    self->addBoxOnGround = [self](const std::string& nodeName,
                                  const float x,
                                  const float z,
                                  const ri::math::Vec3& size,
                                  const ri::math::Vec3& rotation,
                                  const ri::math::Vec3& color,
                                  const std::string& materialName) {
        const float y = self->sampleTerrainHeight(x, z) + (size.y * 0.5f);
        return self->addPrimitive(nodeName,
                                  PrimitiveType::Cube,
                                  ri::math::Vec3{x, y, z},
                                  rotation,
                                  size,
                                  color,
                                  materialName,
                                  ShadingModel::Lit);
    };
    self->groundPoint = [self](const float x, const float z) {
        return ri::math::Vec3{x, self->sampleTerrainHeight(x, z), z};
    };
    self->addForestConiferTree = [self](const ri::math::Vec3& root,
                                        const float targetHeight,
                                        const float yawDegrees,
                                        const int variantIndex,
                                        const std::string& nodePrefix) {
        if (self->useExportedConiferMeshes && !self->exportedConiferMeshes.empty()) {
            const fs::path& meshPath =
                self->exportedConiferMeshes[static_cast<std::size_t>(std::abs(variantIndex))
                                          % self->exportedConiferMeshes.size()];
            const std::string meshKey = meshPath.lexically_normal().generic_string();
            int templateRoot = kInvalidHandle;
            const auto cachedTemplate = self->exportedConiferTemplates.find(meshKey);
            if (cachedTemplate != self->exportedConiferTemplates.end()) {
                templateRoot = cachedTemplate->second;
            } else {
                std::string importError;
                templateRoot = AddModelNode(*self->scene,
                                            ImportedModelOptions{
                                                .sourcePath = meshPath,
                                                .nodeName = "ConiferTemplate_" + meshPath.stem().string(),
                                                .parent = self->coniferTemplateParent,
                                                .transform = Transform{},
                                                .snapMeshBaseToGround = false,
                                                .createPlaceholderOnFailure = false,
                                            },
                                            &importError);
                if (templateRoot != kInvalidHandle) {
                    self->exportedConiferTemplates.emplace(meshKey, templateRoot);
                } else if (!importError.empty()) {
                    ri::core::LogInfo("Wilderness Ruins: WARN tree mesh import failed '"
                                      + meshPath.filename().string() + "': " + importError);
                }
            }
            if (templateRoot == kInvalidHandle) {
                return;
            }
            float referenceHeight =
                self->exportedTreeReferenceHeight > 0.0f ? self->exportedTreeReferenceHeight : 8.0f;
            if (const std::optional<WorldBounds> bounds =
                    ComputeNodeWorldBounds(*self->scene, templateRoot, true)) {
                referenceHeight = std::max(0.25f, GetBoundsSize(*bounds).y);
            }
            // Match FitImportedNodeToMeters: allow large unit-scale corrections (cm → m).
            const float meshScale = std::clamp(targetHeight / std::max(referenceHeight, 1.0e-3f), 1.0e-6f, 1000.0f);
            const Transform placement{
                .position = root,
                .rotationDegrees = ri::math::Vec3{0.0f, yawDegrees, 0.0f},
                .scale = ri::math::Vec3{meshScale, meshScale, meshScale},
            };
            const int instanceRoot = CloneSceneSubtree(
                *self->scene, templateRoot, self->worldRoot, nodePrefix + "_Mesh", placement);
            if (instanceRoot != kInvalidHandle) {
                SnapNodeMeshBaseToGround(*self->scene, instanceRoot, root.y);
            }
            return;
        }
        if (self->useBotdForestTrees && !self->botdTreeVariants.empty()) {
            const BotdBillboardVariant& variant =
                self->botdTreeVariants[static_cast<std::size_t>(std::abs(variantIndex))
                                       % self->botdTreeVariants.size()];
            const float heightScale = std::clamp(targetHeight / std::max(variant.height, 1.0f), 0.35f, 2.4f);
            if (!fs::exists(variant.albedoPath)) {
                return;
            }
            const float treeWidth = variant.width * heightScale;
            const float treeHeight = variant.height * heightScale;
            if (self->botdVerticalBillboardMesh != kInvalidHandle) {
                Material billboardMaterial{
                    .name = "conifer-billboard-" + variant.label,
                    .shadingModel = ShadingModel::Lit,
                    .baseColor = ri::math::Vec3{1.0f, 1.0f, 1.0f},
                    .baseColorTexture = ToAbsoluteAssetPath(variant.albedoPath),
                    .roughness = 0.96f,
                    .alphaCutoff = 0.28f,
                    .doubleSided = true,
                };
                if (!variant.normalPath.empty() && fs::exists(variant.normalPath)) {
                    billboardMaterial.normalTexture = ToAbsoluteAssetPath(variant.normalPath);
                }
                const int billboardMaterialHandle = self->scene->AddMaterial(billboardMaterial);
                const auto addCrossedBillboard = [&](const std::string& planeName,
                                                     const float yawOffset,
                                                     const float widthScale) {
                    const int node = self->scene->CreateNode(planeName, self->worldRoot);
                    self->scene->GetNode(node).localTransform = Transform{
                        .position = root,
                        .rotationDegrees = ri::math::Vec3{0.0f, yawDegrees + yawOffset, 0.0f},
                        .scale = ri::math::Vec3{treeWidth * widthScale, treeHeight, 1.0f},
                    };
                    self->scene->AttachMesh(node, self->botdVerticalBillboardMesh, billboardMaterialHandle);
                };
                addCrossedBillboard(nodePrefix + "_Billboard_1", 0.0f, 1.0f);
                addCrossedBillboard(nodePrefix + "_Billboard_2", 90.0f, 0.92f);
            }
            return;
        }
        if (self->coniferTrunkBatch == kInvalidHandle) {
            return;
        }
        const float safeHeight = std::max(targetHeight, 1.0f);
        const float safeRadius = std::max(targetHeight * 0.22f, 0.35f);
        const float trunkHeight = safeHeight * 0.43f;
        self->scene->AddMeshInstance(self->coniferTrunkBatch,
                                     Transform{
                                         .position = ri::math::Vec3{root.x, root.y + (trunkHeight * 0.5f), root.z},
                                         .rotationDegrees = ri::math::Vec3{0.0f, yawDegrees, 0.0f},
                                         .scale = ri::math::Vec3{safeRadius * 0.16f, trunkHeight, safeRadius * 0.16f},
                                     });
        const std::size_t variantSlot = static_cast<std::size_t>(std::abs(variantIndex) % 4);
        const float billboardWidth = std::max(safeRadius * 1.75f, safeHeight * 0.38f);
        if (self->coniferBillboardBatches[variantSlot] != kInvalidHandle) {
            self->scene->AddMeshInstance(self->coniferBillboardBatches[variantSlot],
                                         Transform{
                                             .position = root,
                                             .rotationDegrees = ri::math::Vec3{0.0f, yawDegrees, 0.0f},
                                             .scale = ri::math::Vec3{billboardWidth, safeHeight, 1.0f},
                                         });
            self->scene->AddMeshInstance(self->coniferBillboardBatches[variantSlot],
                                         Transform{
                                             .position = ri::math::Vec3{root.x, root.y + 0.04f, root.z},
                                             .rotationDegrees = ri::math::Vec3{0.0f, yawDegrees + 93.0f, 0.0f},
                                             .scale = ri::math::Vec3{billboardWidth * 0.92f, safeHeight * 0.98f, 1.0f},
                                         });
        }
    };
}

void SpawnForestRuinsAssetScatter(ForestRuinsScatterBundle& bundle,
                                  const std::vector<ScatterAsset>& assets,
                                  const int count,
                                  const bool useClearings) {
    if (assets.empty() || count <= 0) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        const ScatterAsset& asset =
            assets[static_cast<std::size_t>(bundle.rng.NextIndex(static_cast<int>(assets.size())))];
        if (!fs::exists(asset.sourcePath)) {
            continue;
        }
        const float targetHeight = bundle.rng.NextRange(asset.minScale, asset.maxScale);
        ri::math::Vec3 position = PickScatterPoint(bundle.rng, bundle.scatterExtent, useClearings, bundle.clearings);
        position.y = bundle.sampleTerrainHeight(position.x, position.z);
        const ri::math::Vec3 spawnDelta = position - bundle.guaranteedSpawn;
        if ((spawnDelta.x * spawnDelta.x) + (spawnDelta.z * spawnDelta.z)
            < (bundle.spawnReserveRadius * bundle.spawnReserveRadius)) {
            position = PickScatterPoint(bundle.rng, bundle.scatterExtent, useClearings, bundle.clearings);
            position.y = bundle.sampleTerrainHeight(position.x, position.z);
        }
        const ri::math::Vec3 rotation{0.0f, bundle.rng.NextRange(-180.0f, 180.0f), 0.0f};
        // x=target height (m), y=max footprint (m)
        const ri::math::Vec3 scale{targetHeight, targetHeight * 2.8f, 1.0f};
        const std::string nodeName = asset.namePrefix + "_" + std::to_string(i + 1) + "_"
            + std::to_string(bundle.scatterSeed);
        if (!bundle.addImported(nodeName, asset.sourcePath, position, rotation, scale)) {
            continue;
        }
        if (asset.colliderRadius > 0.0f && asset.colliderHeight > 0.0f) {
            bundle.addCollider("scatter-" + nodeName,
                               ri::math::Vec3{
                                   position.x - asset.colliderRadius,
                                   position.y,
                                   position.z - asset.colliderRadius,
                               },
                               ri::math::Vec3{
                                   position.x + asset.colliderRadius,
                                   position.y + asset.colliderHeight,
                                   position.z + asset.colliderRadius,
                               });
        }
    }
}

void PopulateForestRuinsClearingRingTrees(ForestRuinsScatterBundle& bundle) {
    // Ring the authored ruin pockets so forest presses in on human spaces.
    const int authoredClearings = (std::min)(static_cast<int>(bundle.clearings.size()), 7);
    int treeSerial = 0;
    for (int c = 0; c < authoredClearings; ++c) {
        const Clearing& clearing = bundle.clearings[static_cast<std::size_t>(c)];
        const int ringCount = (c == 0) ? 22 : 16;
        for (int i = 0; i < ringCount; ++i) {
            const float angle = (static_cast<float>(i) / static_cast<float>(ringCount)) * 360.0f
                + static_cast<float>((c * 17) % 40);
            const float radius = clearing.radius + 3.5f + static_cast<float>((i * 3 + c) % 5) * 1.1f;
            const float x = clearing.center.x + std::cos(ri::math::DegreesToRadians(angle)) * radius;
            const float z = clearing.center.z + std::sin(ri::math::DegreesToRadians(angle)) * radius;
            const ri::math::Vec3 root{x, bundle.sampleTerrainHeight(x, z), z};
            ++treeSerial;
            bundle.addForestConiferTree(root,
                                        8.5f + static_cast<float>((i + c) % 7) * 0.85f,
                                        angle + 90.0f,
                                        treeSerial,
                                        "RingPine_" + std::to_string(treeSerial));
        }
    }
}

void PopulateForestRuinsForestScatter(ForestRuinsScatterBundle& bundle) {
    if (!bundle.generateTrees || bundle.treeCount <= 0) {
        return;
    }
    for (int i = 0; i < bundle.treeCount; ++i) {
        const ri::math::Vec3 root = PickScatterPoint(bundle.rng, bundle.scatterExtent, false, bundle.clearings);
        const float groundY = bundle.sampleTerrainHeight(root.x, root.z);
        const float yawBase = bundle.rng.NextRange(-180.0f, 180.0f);
        const int variantCount = static_cast<int>(bundle.exportedConiferMeshes.size());
        bundle.addForestConiferTree(ri::math::Vec3{root.x, groundY, root.z},
                                    bundle.rng.NextRange(6.5f, 14.0f),
                                    yawBase,
                                    bundle.rng.NextIndex(variantCount > 0 ? variantCount : 4),
                                    "ForestPine_" + std::to_string(i + 1));
    }
}

void ExecuteForestRuinsScatterBundle(ForestRuinsScatterBundle& bundle) {
    WireForestRuinsScatterCallbacks(bundle);
    const auto& addBoxOnGround = bundle.addBoxOnGround;

    // Tiny broken-asphalt scars only inside the roadside remnant clearing — not a highway.
    const ri::math::Vec3 remnantCenter{38.0f, 0.0f, -34.0f};
    for (int i = 0; i < 5; ++i) {
        const float ox = static_cast<float>((i % 3) - 1) * 3.4f;
        const float oz = static_cast<float>((i / 3) * 4 - 2) * 1.8f;
        addBoxOnGround("AsphaltScar_" + std::to_string(i + 1),
                       remnantCenter.x + ox,
                       remnantCenter.z + oz,
                       ri::math::Vec3{4.8f + static_cast<float>(i % 2), 0.06f, 3.2f},
                       ri::math::Vec3{0.0f, static_cast<float>((i * 23) % 40) - 20.0f, 0.0f},
                       (i % 2 == 0) ? ri::math::Vec3{0.12f, 0.12f, 0.11f} : ri::math::Vec3{0.09f, 0.10f, 0.08f},
                       "old-road-moss-dirt");
    }

    // Soft leaf-litter pads in the spawn hollow so it reads as a lived-in forest floor.
    for (int i = 0; i < 10; ++i) {
        const float angle = static_cast<float>(i) * 36.0f;
        const float radius = 3.0f + static_cast<float>(i % 4) * 1.4f;
        addBoxOnGround("SpawnLitter_" + std::to_string(i + 1),
                       bundle.guaranteedSpawn.x + std::cos(ri::math::DegreesToRadians(angle)) * radius,
                       bundle.guaranteedSpawn.z + std::sin(ri::math::DegreesToRadians(angle)) * radius,
                       ri::math::Vec3{2.2f, 0.04f, 1.6f},
                       ri::math::Vec3{0.0f, angle, 0.0f},
                       ri::math::Vec3{0.10f, 0.14f, 0.08f},
                       "leaf-litter-shadow");
    }

    SpawnForestRuinsAssetScatter(bundle, bundle.ruinAssets, bundle.ruinCount, true);
    SpawnForestRuinsAssetScatter(bundle, bundle.rockAssets, bundle.rockCount, false);
    SpawnForestRuinsAssetScatter(bundle, bundle.bushAssets, bundle.bushCount, false);
    PopulateForestRuinsClearingRingTrees(bundle);
    PopulateForestRuinsForestScatter(bundle);

    ForestRuinsScatterBundle* self = &bundle;
    bundle.heroCallbacks.heroImport = [self](const std::string& nodeName,
                                             const fs::path& sourcePath,
                                             const float x,
                                             const float z,
                                             const ri::math::Vec3& rotation,
                                             const ri::math::Vec3& scale,
                                             const float colliderRadius,
                                             const float colliderHeight) {
        const ri::math::Vec3 p = self->groundPoint(x, z);
        if (!self->addImported(nodeName, sourcePath, p, rotation, scale)) {
            return;
        }
        if (colliderRadius > 0.0f && colliderHeight > 0.0f) {
            self->addCollider("hero-" + nodeName,
                              {p.x - colliderRadius, p.y, p.z - colliderRadius},
                              {p.x + colliderRadius, p.y + colliderHeight, p.z + colliderRadius});
        }
    };
    bundle.heroCallbacks.addBoxOnGround = bundle.addBoxOnGround;
    bundle.heroCallbacks.addPrimitive = bundle.addPrimitive;
    bundle.heroCallbacks.sampleTerrainHeight = bundle.sampleTerrainHeight;
    bundle.heroCallbacks.sourcePackReady = bundle.sourcePackReady;
    bundle.heroCallbacks.heroRubbleCount = bundle.heroRubbleCount;
    bundle.heroCallbacks.postApocRoot = bundle.postApocRoot;
}

void PopulateForestRuinsHeroCluster(const ForestRuinsHeroPopulateCallbacks& callbacks) {
    const fs::path& world = callbacks.postApocRoot;
    // scale args are meters: height + max horizontal footprint (fitted after import).
    const auto importWorld = [&](const char* nodeName,
                                 const fs::path& relative,
                                 const float x,
                                 const float z,
                                 const ri::math::Vec3& rotation,
                                 const float targetHeightMeters,
                                 const float maxFootprintMeters,
                                 const float colliderRadius,
                                 const float colliderHeight) {
        callbacks.heroImport(nodeName,
                             world / relative,
                             x,
                             z,
                             rotation,
                             ri::math::Vec3{targetHeightMeters, maxFootprintMeters, 1.0f},
                             colliderRadius,
                             colliderHeight);
    };

    // Distinct landmark packs — abandoned house / gas station / trailer / industrial / diner.
    // These are NOT the old Modular Survival roadside props.
    importWorld("LandmarkAbandonedHouse",
                "Abandoned_House/Models/Abandoned_House.fbx",
                -36.0f,
                -40.0f,
                {0.0f, 40.0f, 0.0f},
                9.5f,
                28.0f,
                10.0f,
                7.0f);
    importWorld("LandmarkGasStation",
                "Gas_station/Models/Gas_station.fbx",
                40.0f,
                -34.0f,
                {0.0f, -125.0f, 0.0f},
                8.5f,
                34.0f,
                12.0f,
                6.0f);
    importWorld("LandmarkTrailerPark",
                "Trailer_Park/Models/Trailer_Park.fbx",
                -42.0f,
                36.0f,
                {0.0f, 18.0f, 0.0f},
                7.5f,
                42.0f,
                14.0f,
                5.5f);
    importWorld("LandmarkIndustrial",
                "IndustrialHorror/Industrial_exterior_v2/Models/IndustrialHorror_PS_like.fbx",
                48.0f,
                28.0f,
                {0.0f, -55.0f, 0.0f},
                14.0f,
                36.0f,
                12.0f,
                10.0f);
    importWorld("LandmarkDiner",
                "DINER/Models/DINER.fbx",
                8.0f,
                -62.0f,
                {0.0f, 200.0f, 0.0f},
                8.0f,
                30.0f,
                11.0f,
                5.5f);
    importWorld("LandmarkSixTwelve",
                "SixTwelve/Models/6twelve.fbx",
                -8.0f,
                54.0f,
                {0.0f, 155.0f, 0.0f},
                7.5f,
                26.0f,
                9.0f,
                5.5f);

    // Small human traces near spawn / landmarks
    importWorld("SpawnCampfire", "Campfire/MS_Campfire.fbx", 3.5f, 10.0f, {0.0f, 25.0f, 0.0f}, 1.2f, 2.4f, 1.4f, 1.2f);
    importWorld("SpawnTotem", "Totem/MS_Totem_Welcome.fbx", -4.0f, 14.0f, {0.0f, -30.0f, 0.0f}, 3.2f, 2.0f, 1.2f, 3.4f);
    importWorld("SpawnSawbuck", "Sawbuck/MS_Sawbuck.fbx", 6.0f, 5.5f, {0.0f, 70.0f, 0.0f}, 1.4f, 2.2f, 1.2f, 1.3f);
    importWorld("HouseFirepot", "Firepot/MS_Firepot.fbx", -30.0f, -36.0f, {0.0f, 10.0f, 0.0f}, 1.3f, 1.8f, 1.0f, 1.2f);
    importWorld("GasPropsCluster",
                "Gas_station/Models/Gas_station_Props.fbx",
                34.0f,
                -40.0f,
                {0.0f, 40.0f, 0.0f},
                3.5f,
                16.0f,
                6.0f,
                3.0f);
    importWorld("TrailerPropsCluster",
                "Trailer_Park/Models/Trailer_Park_Props.fbx",
                -48.0f,
                30.0f,
                {0.0f, -20.0f, 0.0f},
                3.5f,
                18.0f,
                6.0f,
                3.0f);
    importWorld("DinerObjects",
                "DINER/Models/Objects.fbx",
                14.0f,
                -58.0f,
                {0.0f, 90.0f, 0.0f},
                2.8f,
                12.0f,
                5.0f,
                2.5f);
    // Skip Forest_Set — full forest scenes often span kilometers in authoring units.

    (void)callbacks.heroRubbleCount;
    if (!callbacks.sourcePackReady) {
        ri::core::LogInfo(
            "Wilderness Ruins: hero landmarks skipped or sparse — PsxPack content not ready "
            "(dirs may exist without importable trees/ground/props)");
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
    const std::vector<BotdBillboardVariant> botdTreeVariants{};
    const std::vector<fs::path> exportedConiferMeshes = DiscoverPsxTreeMeshes(forest);
    const bool useExportedConiferMeshes = !exportedConiferMeshes.empty();
    // BOTD billboards are retired; PsxPack worlds use mesh trees only (see generateTrees gating).
    const bool useBotdForestTrees = false;
    const int botdVerticalBillboardMesh = ri::scene::kInvalidHandle;
    if (forest.usesPsxPack && !SourceForestPackReady(forest)) {
        ri::core::LogInfo(
            "Wilderness Ruins: PsxPack directories present but content not ready "
            "(need ground texture, Nature/Trees meshes, and World props)");
    }
    if (!useExportedConiferMeshes) {
        ri::core::LogInfo("Wilderness Ruins: PSX tree meshes missing under assets/PsxPack/Nature/Trees");
    } else {
        ri::core::LogInfo("Wilderness Ruins: discovered " + std::to_string(exportedConiferMeshes.size())
                          + " PSX tree mesh files under assets/PsxPack (import happens at scatter time)");
    }
    const ri::content::ScriptScalarMap gameplay = ri::content::LoadScriptScalars(gameRoot / "scripts" / "gameplay.riscript");

    world.handles.root = scene.CreateNode("WildernessRuinsLayer");

    LightNodeOptions sun{};
    sun.nodeName = "SunLight";
    sun.parent = world.handles.root;
    sun.transform.rotationDegrees = ri::math::Vec3{-28.0f, 210.0f, 0.0f};
    sun.light = Light{
        .name = "SunLight",
        .type = LightType::Directional,
        .color = ri::math::Vec3{0.88f, 0.80f, 0.66f},
        .intensity = 1.15f,
    };
    world.handles.sun = AddLightNode(scene, sun);

    LightNodeOptions bounce{};
    bounce.nodeName = "BounceFill";
    bounce.parent = world.handles.root;
    bounce.transform.position = ri::math::Vec3{0.0f, 5.0f, 8.0f};
    bounce.light = Light{
        .name = "BounceFill",
        .type = LightType::Point,
        .color = ri::math::Vec3{0.34f, 0.40f, 0.36f},
        .intensity = 1.25f,
        .range = 70.0f,
    };
    (void)AddLightNode(scene, bounce);

    OrbitCameraOptions orbitCamera{};
    orbitCamera.parent = world.handles.root;
    orbitCamera.camera = Camera{
        .name = "EditorOrbitCamera",
        .projection = ProjectionType::Perspective,
        .fieldOfViewDegrees = 72.0f,
        .nearClip = 0.05f,
        .farClip = 2000.0f,
    };
    orbitCamera.orbit = OrbitCameraState{
        .target = ri::math::Vec3{0.0f, 3.0f, 8.0f},
        .distance = 28.0f,
        .yawDegrees = 145.0f,
        .pitchDegrees = -22.0f,
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

    ProceduralTerrainOptions terrain{};
    terrain.nodeName = "ForestTerrain";
    terrain.parent = world.handles.root;
    terrain.materialName = "wilderness-ground";
    terrain.baseColor = ri::math::Vec3{0.32f, 0.38f, 0.24f};
    terrain.baseColorTexture = ToAbsoluteAssetPath(forest.groundDiffuse);
    if (!forest.groundNormal.empty() && fs::exists(forest.groundNormal)) {
        terrain.normalTexture = ToAbsoluteAssetPath(forest.groundNormal);
    }
    terrain.textureTiling = ri::math::Vec2{56.0f, 56.0f};
    terrain.resolutionX = 112;
    terrain.resolutionZ = 112;
    terrain.sizeX = 420.0f;
    terrain.sizeZ = 420.0f;
    terrain.heightAmplitude = 2.4f;
    terrain.heightFrequency = 0.014f;
    terrain.detailAmplitude = 0.55f;
    terrain.detailFrequency = 0.11f;
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

    const ri::math::Vec3 guaranteedSpawn = PopulateForestRuinsSceneContent({
        .world = world,
        .scene = scene,
        .forest = forest,
        .workspaceRoot = workspaceRoot,
        .gameRoot = gameRoot,
        .terrain = terrain,
        .gameplay = gameplay,
        .useBotdForestTrees = useBotdForestTrees,
        .useExportedConiferMeshes = useExportedConiferMeshes,
        .botdVerticalBillboardMesh = botdVerticalBillboardMesh,
        .botdTreeVariants = botdTreeVariants,
        .exportedConiferMeshes = exportedConiferMeshes,
    });

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

    AbsolutizeMaterialTexturePaths(scene);
    ApplyForestRuinsShowcaseMaterials(scene, forest.lrtRoot);
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
