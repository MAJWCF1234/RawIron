#include "RawIron/Games/ForestRuins/ForestRuinsRuntime.h"

#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/ScriptScalars.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/ModelLoader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
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
    const bool editorWorkspaceScene = sceneName.find("EditorWorkspace") != std::string_view::npos;
    const ri::content::ScriptScalarMap gameplay = ri::content::LoadScriptScalars(gameRoot / "scripts" / "gameplay.riscript");

    world.handles.root = scene.CreateNode("WildernessRuinsLayer");

    LightNodeOptions sun{};
    sun.nodeName = "SunLight";
    sun.parent = world.handles.root;
    sun.transform.rotationDegrees = ri::math::Vec3{-42.0f, 34.0f, 0.0f};
    sun.light = Light{
        .name = "SunLight",
        .type = LightType::Directional,
        .color = ri::math::Vec3{1.00f, 0.92f, 0.74f},
        .intensity = 3.05f,
    };
    world.handles.sun = AddLightNode(scene, sun);

    LightNodeOptions bounce{};
    bounce.nodeName = "BounceFill";
    bounce.parent = world.handles.root;
    bounce.transform.position = ri::math::Vec3{0.0f, 4.0f, 0.0f};
    bounce.light = Light{
        .name = "BounceFill",
        .type = LightType::Point,
        .color = ri::math::Vec3{0.36f, 0.50f, 0.42f},
        .intensity = 2.15f,
        .range = 96.0f,
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
    terrain.baseColorTexture = (workspaceRoot / "Assets" / "Source" / "Forest Scene" / "Assets"
                                / "Astra113 Assets" / "Textures" / "Forest_Ground_diffuseOriginal.png")
                                   .generic_string();
    terrain.textureTiling = ri::math::Vec2{168.0f, 168.0f};
    terrain.resolutionX = 96;
    terrain.resolutionZ = 96;
    if (editorWorkspaceScene) {
        terrain.resolutionX = 160;
        terrain.resolutionZ = 160;
    }
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

    auto addImported = [&](const std::string& nodeName,
                           const fs::path& sourcePath,
                           const ri::math::Vec3& position,
                           const ri::math::Vec3& rotation,
                           const ri::math::Vec3& scale) {
        if (!fs::exists(sourcePath)) {
            return;
        }
        std::string importError;
        float importScale = 0.025f;
        if (PathContainsAscii(sourcePath, "forest scene")) {
            importScale = 0.055f;
        } else if (PathContainsAscii(sourcePath, "post apocalypse")) {
            importScale = 0.012f;
        }
        const ri::math::Vec3 calibratedScale = scale * importScale;
        (void)AddModelNode(scene,
                           ImportedModelOptions{
                               .sourcePath = sourcePath,
                               .nodeName = nodeName,
                               .parent = world.handles.root,
                               .transform = Transform{
                                   .position = position,
                                   .rotationDegrees = rotation,
                                   .scale = calibratedScale,
                               },
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

    const int coniferTrunkMesh = scene.AddMesh(Mesh{
        .name = "instanced-conifer-trunk",
        .primitive = PrimitiveType::Cube,
        .positions = {},
        .texCoords = {},
        .indices = {},
    });
    const int coniferTrunkMaterial = scene.AddMaterial(Material{
        .name = "wet-conifer-bark",
        .shadingModel = ShadingModel::Lit,
        .baseColor = ri::math::Vec3{0.20f, 0.13f, 0.075f},
        .roughness = 0.98f,
    });
    const int coniferTrunkBatch = scene.AddMeshInstanceBatch(MeshInstanceBatch{
        .name = "ConiferTrunkInstances",
        .parent = world.handles.root,
        .mesh = coniferTrunkMesh,
        .material = coniferTrunkMaterial,
        .transforms = {},
    });
    const fs::path speedTreeConiferBillboardTexture =
        gameRoot / "Assets" / "Generated" / "conifer_desktop_atlas_billboards.png";
    const int coniferBillboardMaterial = scene.AddMaterial(Material{
        .name = "speedtree-conifer-atlas",
        .shadingModel = ShadingModel::Unlit,
        .baseColor = ri::math::Vec3{0.34f, 0.48f, 0.30f},
        .baseColorTexture = speedTreeConiferBillboardTexture.generic_string(),
        .emissiveColor = ri::math::Vec3{0.0f, 0.0f, 0.0f},
        .roughness = 0.98f,
        .alphaCutoff = 0.18f,
        .doubleSided = true,
    });
    const std::array<int, 4> coniferBillboardMeshes{
        scene.AddMesh(MakeVerticalBillboardMesh(0.00f, 0.25f)),
        scene.AddMesh(MakeVerticalBillboardMesh(0.25f, 0.50f)),
        scene.AddMesh(MakeVerticalBillboardMesh(0.50f, 0.75f)),
        scene.AddMesh(MakeVerticalBillboardMesh(0.75f, 1.00f)),
    };
    const std::array<int, 4> coniferBillboardBatches{
        scene.AddMeshInstanceBatch(MeshInstanceBatch{
            .name = "ConiferAtlasBillboardA",
            .parent = world.handles.root,
            .mesh = coniferBillboardMeshes[0],
            .material = coniferBillboardMaterial,
            .transforms = {},
        }),
        scene.AddMeshInstanceBatch(MeshInstanceBatch{
            .name = "ConiferAtlasBillboardB",
            .parent = world.handles.root,
            .mesh = coniferBillboardMeshes[1],
            .material = coniferBillboardMaterial,
            .transforms = {},
        }),
        scene.AddMeshInstanceBatch(MeshInstanceBatch{
            .name = "ConiferAtlasBillboardC",
            .parent = world.handles.root,
            .mesh = coniferBillboardMeshes[2],
            .material = coniferBillboardMaterial,
            .transforms = {},
        }),
        scene.AddMeshInstanceBatch(MeshInstanceBatch{
            .name = "ConiferAtlasBillboardD",
            .parent = world.handles.root,
            .mesh = coniferBillboardMeshes[3],
            .material = coniferBillboardMaterial,
            .transforms = {},
        }),
    };
    auto addInstancedConiferTree = [&](const ri::math::Vec3& root,
                                       const float height,
                                       const float crownRadius,
                                       const float yawDegrees,
                                       const int colorVariant) {
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
    const fs::path forestRocksRoot = workspaceRoot / "Assets" / "Source" / "Forest Scene" / "Assets" / "Assets"
        / "Rocks and Boulders 2" / "Rocks" / "Source" / "Models";
    const fs::path bushesRoot = workspaceRoot / "Assets" / "Source" / "Forest Scene" / "Assets"
        / "Astra113 Assets" / "YughuesFreeBushes2018" / "Meshes";

    struct ScatterAsset {
        const char* namePrefix = "";
        fs::path sourcePath{};
        float minScale = 1.0f;
        float maxScale = 1.0f;
        float colliderRadius = 0.0f;
        float colliderHeight = 0.0f;
    };

    std::vector<ScatterAsset> ruinAssets{
        {"RuinBusStop", postApocRoot / "Bus_Stop_Rural" / "MS_Bus_Stop_Rural.fbx", 2.8f, 4.2f, 3.2f, 4.2f},
        {"RuinBarrier", postApocRoot / "Barrier_Road" / "MS_Barrier_Road.fbx", 2.1f, 3.4f, 1.8f, 2.3f},
        {"RuinFence", postApocRoot / "Fence_Wood" / "MS_Fence_Wood.fbx", 3.4f, 5.0f, 2.5f, 3.4f},
        {"RuinPole", postApocRoot / "Pole_Light_Rural" / "MS_Pole_Light_Rural.fbx", 2.6f, 3.6f, 1.5f, 5.0f},
        {"RuinBrickPile", postApocRoot / "Brick_Pile" / "MS_Brick_Pile.fbx", 1.8f, 3.0f, 1.6f, 1.8f},
        {"RuinCableReel", postApocRoot / "Cable_Reel" / "MS_Cable_Reel.fbx", 1.8f, 2.8f, 1.3f, 1.8f},
        {"RuinTower", postApocRoot / "Fireplace_Tower" / "MS_Fireplace_Tower.fbx", 2.4f, 3.7f, 2.7f, 5.2f},
        {"RuinTent", postApocRoot / "Tent_Civilian" / "MS_Tent_Civilian.fbx", 2.4f, 3.8f, 2.4f, 2.5f},
        {"RuinBillboard", postApocRoot / "Sign_Billboard" / "MS_Sign_Billboard.fbx", 2.7f, 4.2f, 3.3f, 4.8f},
        {"RuinTransformer", postApocRoot / "Transformer_Box" / "MS_Transformer_Box.fbx", 1.8f, 3.1f, 1.8f, 2.5f},
        {"RuinCrate", postApocRoot / "Crate" / "MS_Crate.fbx", 1.7f, 2.8f, 1.1f, 1.6f},
    };
    std::vector<ScatterAsset> rockAssets{
        {"Rock", forestRocksRoot / "Rock1A.fbx", 2.4f, 5.0f, 1.9f, 2.4f},
        {"Rock", forestRocksRoot / "Rock1B.fbx", 2.2f, 4.8f, 1.8f, 2.3f},
        {"Rock", forestRocksRoot / "Rock2.fbx", 2.4f, 5.1f, 2.0f, 2.6f},
        {"Rock", forestRocksRoot / "Rock3.fbx", 2.6f, 5.6f, 2.2f, 2.8f},
        {"Rock", forestRocksRoot / "Rock4A.fbx", 2.3f, 4.9f, 1.9f, 2.4f},
        {"Rock", forestRocksRoot / "Rock5A.fbx", 2.4f, 5.0f, 2.0f, 2.5f},
        {"Rock", forestRocksRoot / "Rock6C.fbx", 2.8f, 5.8f, 2.3f, 3.0f},
    };
    std::vector<ScatterAsset> bushAssets{
        {"Bush", bushesRoot / "Bush01.FBX", 1.8f, 3.4f, 1.4f, 1.5f},
        {"Bush", bushesRoot / "Bush02.FBX", 1.8f, 3.5f, 1.4f, 1.5f},
        {"Bush", bushesRoot / "Bush03.FBX", 1.8f, 3.6f, 1.4f, 1.6f},
        {"Bush", bushesRoot / "Bush04.FBX", 1.8f, 3.6f, 1.4f, 1.6f},
        {"Bush", bushesRoot / "Bush05.FBX", 1.8f, 3.7f, 1.4f, 1.7f},
    };

    struct TreeBillboardVariant {
        fs::path texturePath{};
        float width = 3.2f;
        float height = 8.8f;
        ri::math::Vec3 tint{0.92f, 1.0f, 0.86f};
    };
    const std::array<TreeBillboardVariant, 4> treeVariants{
        TreeBillboardVariant{
            workspaceRoot / "Assets" / "Source" / "Forest Scene" / "Assets" / "Assets" / "Conifers [BOTD]"
                / "Sources" / "Billboards" / "Billboard Bare" / "Conifer Bare [Albedo].png",
            2.9f,
            8.2f,
            ri::math::Vec3{0.45f, 0.52f, 0.34f},
        },
        TreeBillboardVariant{
            workspaceRoot / "Assets" / "Source" / "Forest Scene" / "Assets" / "Assets" / "Conifers [BOTD]"
                / "Sources" / "Billboards" / "Billboard Small" / "Conifer Small [Albedo].png",
            3.0f,
            7.6f,
            ri::math::Vec3{0.40f, 0.56f, 0.34f},
        },
        TreeBillboardVariant{
            workspaceRoot / "Assets" / "Source" / "Forest Scene" / "Assets" / "Assets" / "Conifers [BOTD]"
                / "Sources" / "Billboards" / "Billboard Medium" / "Conifer Medium [Albedo].png",
            3.4f,
            9.2f,
            ri::math::Vec3{0.36f, 0.50f, 0.30f},
        },
        TreeBillboardVariant{
            workspaceRoot / "Assets" / "Source" / "Forest Scene" / "Assets" / "Assets" / "Conifers [BOTD]"
                / "Sources" / "Billboards" / "Billboard Tall" / "Conifer Tall [Albedo].png",
            4.0f,
            12.4f,
            ri::math::Vec3{0.32f, 0.44f, 0.28f},
        },
    };

    auto addConiferBillboardCluster = [&](const std::string& nodeName,
                                          const ri::math::Vec3& root,
                                          const TreeBillboardVariant& variant,
                                          const float scale,
                                          const float yawBase,
                                          const float alphaCutoff,
                                          const bool addTrunk) {
        if (!fs::exists(variant.texturePath)) {
            return;
        }
        const float treeWidth = variant.width * scale;
        const float treeHeight = variant.height * scale;
        if (addTrunk) {
            addPrimitive(nodeName + "_Trunk",
                         PrimitiveType::Cube,
                         {root.x, root.y + (treeHeight * 0.31f), root.z},
                         {0.0f, yawBase, 0.0f},
                         {treeWidth * 0.10f, treeHeight * 0.62f, treeWidth * 0.10f},
                         {0.27f, 0.17f, 0.10f},
                         "conifer-trunk");
        }
        const int planeCount = addTrunk ? 3 : 2;
        for (int planeIndex = 0; planeIndex < planeCount; ++planeIndex) {
            PrimitiveNodeOptions billboard{};
            billboard.nodeName = nodeName + "_Billboard_" + std::to_string(planeIndex + 1);
            billboard.parent = world.handles.root;
            billboard.primitive = PrimitiveType::Plane;
            billboard.materialName = "conifer-billboard";
            billboard.shadingModel = ShadingModel::Lit;
            billboard.baseColor = variant.tint;
            billboard.baseColorTexture = variant.texturePath.generic_string();
            billboard.textureTiling = ri::math::Vec2{1.0f, 1.0f};
            billboard.alphaCutoff = alphaCutoff;
            billboard.doubleSided = true;
            billboard.roughness = 0.96f;
            billboard.metallic = 0.0f;
            billboard.emissiveColor = ri::math::Vec3{0.0f, 0.0f, 0.0f};
            billboard.transform.position = ri::math::Vec3{root.x, root.y + (treeHeight * 0.5f), root.z};
            billboard.transform.rotationDegrees =
                ri::math::Vec3{90.0f, yawBase + (static_cast<float>(planeIndex) * (180.0f / planeCount)), 0.0f};
            billboard.transform.scale = ri::math::Vec3{treeWidth, 1.0f, treeHeight};
            (void)AddPrimitiveNode(scene, billboard);
        }
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
    const bool generateTrees = ri::content::ScriptScalarOrBool(gameplay, "scatter_tree_proxies", true)
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
            const std::string nodeName =
                std::string(asset.namePrefix) + "_" + std::to_string(i + 1) + "_" + std::to_string(scatterSeed);
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

    for (int i = 0; i < 27; ++i) {
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
    for (int i = 0; i < 42; ++i) {
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
    for (int i = 0; i < 76; ++i) {
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
    for (int i = 0; i < 34; ++i) {
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

    auto addHeroRuinCluster = [&]() {
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

        for (int i = 0; i < 18; ++i) {
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

        for (int i = 0; i < 11; ++i) {
            const float x = -6.0f + static_cast<float>(i) * 1.25f;
            const float z = 27.0f + std::sin(static_cast<float>(i) * 1.7f) * 4.0f;
            addPrimitive("MossCushion_" + std::to_string(i + 1),
                         PrimitiveType::Sphere,
                         ri::math::Vec3{x, sampleTerrainHeight(x, z) + 0.25f, z},
                         {},
                         {1.2f, 0.32f, 0.9f},
                         moss,
                         "moss-cushion",
                         ShadingModel::Unlit);
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
                   5.8f, 53.2f, {0.0f, -16.0f, 0.0f}, {3.3f, 3.3f, 3.3f}, 1.1f, 2.8f);
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
    };

    addHeroRuinCluster();

    spawnScatter(ruinAssets, ruinCount, true);
    spawnScatter(rockAssets, rockCount, false);
    spawnScatter(bushAssets, bushCount, false);

    for (int i = 0; i < 86; ++i) {
        const float z = 78.0f - (static_cast<float>(i) * 1.95f);
        const float side = (i % 2 == 0) ? -1.0f : 1.0f;
        const float forestWallOffset = RuinPathHalfWidth(z) + 7.0f + static_cast<float>((i * 11) % 7) * 1.05f;
        const float x = RuinPathCenterX(z) + side * forestWallOffset;
        const ri::math::Vec3 root{x, sampleTerrainHeight(x, z), z};
        addInstancedConiferTree(root,
                                9.8f + static_cast<float>((i * 17) % 9) * 0.72f,
                                2.35f + static_cast<float>((i * 13) % 6) * 0.32f,
                                static_cast<float>((i * 29) % 360),
                                i);
    }

    for (int i = 0; i < 14; ++i) {
        const float z = 74.0f - (static_cast<float>(i) * 3.8f);
        const float side = (i % 2 == 0) ? -1.0f : 1.0f;
        const float x = RuinPathCenterX(z) + side * (14.0f + static_cast<float>((i * 5) % 4) * 2.6f);
        const ri::math::Vec3 root{x, sampleTerrainHeight(x, z), z};
        addInstancedConiferTree(root,
                                11.6f + static_cast<float>((i * 19) % 5) * 0.9f,
                                2.7f + static_cast<float>((i * 7) % 4) * 0.36f,
                                static_cast<float>((i * 43) % 360),
                                i + 1);
    }

    for (int i = 0; i < 68; ++i) {
        const float z = 74.0f - (static_cast<float>(i) * 2.15f);
        const float side = (i % 2 == 0) ? -1.0f : 1.0f;
        const float x = RuinPathCenterX(z) + side * (RuinPathHalfWidth(z) + 2.2f + static_cast<float>((i * 5) % 4) * 0.48f);
        const ri::math::Vec3 root{x, sampleTerrainHeight(x, z), z};
        addInstancedConiferTree(root,
                                2.8f + static_cast<float>((i * 7) % 5) * 0.46f,
                                0.85f + static_cast<float>((i * 11) % 4) * 0.18f,
                                static_cast<float>((i * 47) % 360),
                                i + 2);
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
            addInstancedConiferTree(ri::math::Vec3{root.x, groundY, root.z},
                                    rng.NextRange(7.2f, 15.5f),
                                    rng.NextRange(1.85f, 3.75f),
                                    yawBase,
                                    rng.NextIndex(3));
            if ((i % 7) == 0) {
                const TreeBillboardVariant& variant =
                    treeVariants[static_cast<std::size_t>(rng.NextIndex(static_cast<int>(treeVariants.size())))];
                addConiferBillboardCluster("DistantConiferBillboard_" + std::to_string(i + 1),
                                           ri::math::Vec3{root.x + rng.NextRange(-1.1f, 1.1f), groundY, root.z},
                                           variant,
                                           rng.NextRange(0.62f, 0.92f),
                                           yawBase + 37.0f,
                                           0.46f,
                                           false);
            }
        }
    }

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
