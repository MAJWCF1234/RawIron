#include "RawIron/Games/CubeTest/CubeTestWorld.h"

#include "RawIron/Content/GameManifest.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/StructuralBrush.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace ri::games::cubetest {

namespace {

namespace fs = std::filesystem;

constexpr const char* kLrtPackageRelativePath = "Assets\\Packages\\LRT - Texture Pack - RT28.8 - 128x";

std::string PackageTexture(const fs::path& workspaceRoot, const std::string_view relativePath) {
    return (workspaceRoot / fs::path(kLrtPackageRelativePath) / fs::path(relativePath)).lexically_normal().string();
}

ri::scene::PrimitiveNodeOptions CubeMaterialOptions(const int parent, const fs::path& workspaceRoot) {
    ri::scene::PrimitiveNodeOptions options{};
    options.nodeName = "CubeTest_MappedCube";
    options.parent = parent;
    options.primitive = ri::scene::PrimitiveType::Cube;
    options.transform.position = {0.0f, 0.72f, 0.0f};
    options.transform.rotationDegrees = {0.0f, 28.0f, 0.0f};
    options.transform.scale = {1.2f, 1.2f, 1.2f};
    options.materialName = "cube-test-chiseled-quartz-full-map";
    options.materialStyle = ri::scene::MaterialStyle::Standard;
    options.materialWorkflow = ri::scene::MaterialWorkflow::SpecGloss;
    options.baseColor = {1.0f, 1.0f, 1.0f};
    options.baseColorTexture = PackageTexture(workspaceRoot, "tile/RT_chiseled_quartz_block.png");
    options.normalTexture = PackageTexture(workspaceRoot, "tile/RT_chiseled_quartz_block_n.png");
    options.ormTexture = PackageTexture(workspaceRoot, "tile/RT_chiseled_quartz_block_s.png");
    options.detailTexture = options.baseColorTexture;
    options.textureTiling = {1.0f, 1.0f};
    options.metallic = 0.02f;
    options.roughness = 0.48f;
    return options;
}

ri::scene::PrimitiveNodeOptions MaterialSampleOptions(const int parent,
                                                      const fs::path& workspaceRoot,
                                                      const std::string_view name,
                                                      const std::string_view textureStem,
                                                      const ri::math::Vec3& position,
                                                      const ri::math::Vec3& scale,
                                                      const float metallic,
                                                      const float roughness) {
    const std::string stem(textureStem);
    ri::scene::PrimitiveNodeOptions options{};
    options.nodeName = std::string(name);
    options.parent = parent;
    options.primitive = ri::scene::PrimitiveType::Cube;
    options.transform.position = position;
    options.transform.rotationDegrees = {0.0f, -18.0f, 0.0f};
    options.transform.scale = scale;
    options.materialName = std::string(name) + "-full-map";
    options.materialStyle = ri::scene::MaterialStyle::Standard;
    options.materialWorkflow = ri::scene::MaterialWorkflow::SpecGloss;
    options.baseColor = {1.0f, 1.0f, 1.0f};
    options.baseColorTexture = PackageTexture(workspaceRoot, "tile/" + stem + ".png");
    options.normalTexture = PackageTexture(workspaceRoot, "tile/" + stem + "_n.png");
    options.ormTexture = PackageTexture(workspaceRoot, "tile/" + stem + "_s.png");
    options.detailTexture = options.baseColorTexture;
    options.textureTiling = {1.0f, 1.0f};
    options.metallic = metallic;
    options.roughness = roughness;
    return options;
}

void ApplyStructuralMetadata(ri::scene::Node& node,
                             std::string brushId,
                             const ri::scene::StructuralBrushSemanticRole role,
                             const ri::scene::StructuralBrushCollisionPolicy collision,
                             const ri::scene::StructuralBrushNavigationPolicy navigation) {
    node.structuralBrush.brushId = std::move(brushId);
    node.structuralBrush.region = "cube-test-platform";
    node.structuralBrush.operation = ri::scene::StructuralBrushOperation::Solid;
    node.structuralBrush.role = role;
    node.structuralBrush.collision = collision;
    node.structuralBrush.visibility = ri::scene::StructuralBrushVisibilityPolicy::Occluder;
    node.structuralBrush.navigation = navigation;
    node.structuralBrush.rebuildScope = ri::scene::StructuralBrushRebuildScope::Local;
    node.structuralBrush.visualMesh = {
        .meshId = node.name + ".m-mesh",
        .materialSetId = node.name + ".materials",
        .uvSetId = node.name + ".uv0",
        .renderable = true,
    };
    node.structuralBrush.physicsMesh = {
        .meshId = node.name + ".p-mesh",
        .rigidBodyShape = "box",
        .simulationShape = "static",
        .physicalMaterial = navigation == ri::scene::StructuralBrushNavigationPolicy::Walkable ? "brushed-concrete" : "test-crystal",
        .participatesInSimulation = collision != ri::scene::StructuralBrushCollisionPolicy::None,
    };
    node.structuralBrush.queryMesh = {
        .meshId = node.name + ".q-mesh",
        .raycastShape = "box",
        .placementShape = "box",
        .interactionShape = "box",
        .raycastable = true,
        .traceable = collision != ri::scene::StructuralBrushCollisionPolicy::None,
        .placeable = true,
        .interactable = true,
    };
    node.structuralBrush.informationLayer = {
        .semanticGraphId = "ssg://cube-test/" + node.name,
        .gameplayMeaning = navigation == ri::scene::StructuralBrushNavigationPolicy::Walkable
            ? "Walkable platform and baseline BSP/query partition seed."
            : "Fully mapped visual cube for M/P/Q/I channel validation.",
        .relations = {"owned-by:cube-test", "partition:platform-cell-0"},
        .reportable = true,
    };
}

int AddMarkerBox(ri::scene::Scene& scene,
                 const int parent,
                 const std::string_view name,
                 const ri::math::Vec3& position,
                 const ri::math::Vec3& scale,
                 const ri::math::Vec3& color,
                 const ri::math::Vec3& emissive = {}) {
    ri::scene::PrimitiveNodeOptions marker{};
    marker.nodeName = std::string(name);
    marker.parent = parent;
    marker.primitive = ri::scene::PrimitiveType::Cube;
    marker.transform.position = position;
    marker.transform.scale = scale;
    marker.materialName = std::string(name) + "-material";
    marker.baseColor = color;
    marker.emissiveColor = emissive;
    marker.roughness = 0.62f;
    return ri::scene::AddPrimitiveNode(scene, marker);
}

} // namespace

CubeTestWorld BuildCubeTestWorld(const std::string_view sceneName, const fs::path& requestedWorkspaceRoot) {
    const fs::path workspaceRoot = requestedWorkspaceRoot.empty()
        ? ri::content::DetectWorkspaceRoot(fs::current_path())
        : requestedWorkspaceRoot;
    CubeTestWorld world{};
    world.scene = ri::scene::Scene(std::string(sceneName));
    world.rootNode = world.scene.CreateNode("CubeTest_Root");

    ri::scene::PrimitiveNodeOptions platform{};
    platform.nodeName = "CubeTest_Platform";
    platform.parent = world.rootNode;
    platform.primitive = ri::scene::PrimitiveType::Cube;
    platform.transform.position = {0.0f, -0.12f, 0.0f};
    platform.transform.scale = {16.0f, 0.24f, 16.0f};
    platform.materialName = "cube-test-platform";
    platform.baseColor = {0.62f, 0.66f, 0.62f};
    platform.textureTiling = {8.0f, 8.0f};
    platform.roughness = 0.74f;
    world.platformNode = ri::scene::AddPrimitiveNode(world.scene, platform);
    ApplyStructuralMetadata(world.scene.GetNode(world.platformNode),
                            "cube-test-platform",
                            ri::scene::StructuralBrushSemanticRole::Floor,
                            ri::scene::StructuralBrushCollisionPolicy::Player,
                            ri::scene::StructuralBrushNavigationPolicy::Walkable);

    world.cubeNode = ri::scene::AddPrimitiveNode(
        world.scene, CubeMaterialOptions(world.rootNode, workspaceRoot));
    ApplyStructuralMetadata(world.scene.GetNode(world.cubeNode),
                            "cube-test-mapped-cube",
                            ri::scene::StructuralBrushSemanticRole::Structure,
                            ri::scene::StructuralBrushCollisionPolicy::Query,
                            ri::scene::StructuralBrushNavigationPolicy::Ignored);

    world.goldSampleNode = ri::scene::AddPrimitiveNode(
        world.scene,
        MaterialSampleOptions(world.rootNode,
                              workspaceRoot,
                              "CubeTest_GoldSpecSample",
                              "rt2_gold_block",
                              {-3.35f, 0.55f, 1.85f},
                              {0.95f, 0.95f, 0.95f},
                              0.82f,
                              0.22f));
    ApplyStructuralMetadata(world.scene.GetNode(world.goldSampleNode),
                            "cube-test-gold-spec-sample",
                            ri::scene::StructuralBrushSemanticRole::Decor,
                            ri::scene::StructuralBrushCollisionPolicy::Query,
                            ri::scene::StructuralBrushNavigationPolicy::Ignored);
    world.copperSampleNode = ri::scene::AddPrimitiveNode(
        world.scene,
        MaterialSampleOptions(world.rootNode,
                              workspaceRoot,
                              "CubeTest_CopperNormalSample",
                              "rt2_chiseled_copper",
                              {3.35f, 0.55f, 1.85f},
                              {0.95f, 0.95f, 0.95f},
                              0.64f,
                              0.30f));
    ApplyStructuralMetadata(world.scene.GetNode(world.copperSampleNode),
                            "cube-test-copper-normal-sample",
                            ri::scene::StructuralBrushSemanticRole::Decor,
                            ri::scene::StructuralBrushCollisionPolicy::Query,
                            ri::scene::StructuralBrushNavigationPolicy::Ignored);
    world.ironSampleNode = ri::scene::AddPrimitiveNode(
        world.scene,
        MaterialSampleOptions(world.rootNode,
                              workspaceRoot,
                              "CubeTest_IronRoughnessSample",
                              "rt2_iron_block",
                              {0.0f, 0.45f, 3.85f},
                              {0.78f, 0.78f, 0.78f},
                              0.72f,
                              0.46f));
    ApplyStructuralMetadata(world.scene.GetNode(world.ironSampleNode),
                            "cube-test-iron-roughness-sample",
                            ri::scene::StructuralBrushSemanticRole::Decor,
                            ri::scene::StructuralBrushCollisionPolicy::Query,
                            ri::scene::StructuralBrushNavigationPolicy::Ignored);

    world.crystalSampleNode = ri::scene::AddPrimitiveNode(
        world.scene,
        MaterialSampleOptions(world.rootNode,
                              workspaceRoot,
                              "CubeTest_CrystalGlassSample",
                              "rt2_diamond_block",
                              {-3.35f, 0.45f, -2.15f},
                              {0.82f, 0.82f, 0.82f},
                              0.08f,
                              0.14f));
    if (world.crystalSampleNode != ri::scene::kInvalidHandle) {
        ri::scene::Material& crystalMaterial = world.scene.GetMaterial(world.scene.GetNode(world.crystalSampleNode).material);
        crystalMaterial.materialStyle = ri::scene::MaterialStyle::Crystal;
        crystalMaterial.transparent = true;
        crystalMaterial.opacity = 0.62f;
        crystalMaterial.doubleSided = true;
        crystalMaterial.emissiveColor = {0.08f, 0.18f, 0.22f};
        ApplyStructuralMetadata(world.scene.GetNode(world.crystalSampleNode),
                                "cube-test-crystal-glass-sample",
                                ri::scene::StructuralBrushSemanticRole::Decor,
                                ri::scene::StructuralBrushCollisionPolicy::Query,
                                ri::scene::StructuralBrushNavigationPolicy::Ignored);
    }

    {
        ri::scene::StructuralBrushSpawnOptions portalBrush{};
        portalBrush.nodeName = "CubeTest_PortalBrush";
        portalBrush.parent = world.rootNode;
        portalBrush.structuralType = "box";
        portalBrush.transform.position = {4.8f, 1.35f, -1.6f};
        portalBrush.transform.rotationDegrees = {0.0f, -24.0f, 0.0f};
        portalBrush.transform.scale = {1.4f, 2.6f, 0.22f};
        portalBrush.metadata.brushId = "cube-test-portal";
        portalBrush.metadata.region = "cube-test-platform";
        portalBrush.metadata.role = ri::scene::StructuralBrushSemanticRole::Portal;
        portalBrush.metadata.operation = ri::scene::StructuralBrushOperation::Subtract;
        portalBrush.metadata.collision = ri::scene::StructuralBrushCollisionPolicy::None;
        portalBrush.metadata.visibility = ri::scene::StructuralBrushVisibilityPolicy::Portal;
        portalBrush.metadata.navigation = ri::scene::StructuralBrushNavigationPolicy::Ignored;
        portalBrush.metadata.rebuildScope = ri::scene::StructuralBrushRebuildScope::Local;
        portalBrush.metadata.visualMesh.renderable = true;
        portalBrush.metadata.queryMesh.raycastable = true;
        portalBrush.metadata.queryMesh.traceable = false;
        portalBrush.metadata.informationLayer.reportable = true;
        portalBrush.metadata.informationLayer.gameplayMeaning = "Hybrid HDR portal subtract brush for render validation.";
        world.portalBrushNode = ri::scene::AddStructuralBrushNode(world.scene, portalBrush);
    }

    AddMarkerBox(world.scene,
                 world.rootNode,
                 "CubeTest_NorthTrim",
                 {0.0f, 0.035f, 8.05f},
                 {16.4f, 0.08f, 0.12f},
                 {0.84f, 0.92f, 0.95f},
                 {0.04f, 0.07f, 0.08f});
    AddMarkerBox(world.scene,
                 world.rootNode,
                 "CubeTest_SouthTrim",
                 {0.0f, 0.035f, -8.05f},
                 {16.4f, 0.08f, 0.12f},
                 {0.84f, 0.92f, 0.95f},
                 {0.04f, 0.07f, 0.08f});
    AddMarkerBox(world.scene,
                 world.rootNode,
                 "CubeTest_EastTrim",
                 {8.05f, 0.035f, 0.0f},
                 {0.12f, 0.08f, 16.4f},
                 {0.84f, 0.92f, 0.95f},
                 {0.04f, 0.07f, 0.08f});
    AddMarkerBox(world.scene,
                 world.rootNode,
                 "CubeTest_WestTrim",
                 {-8.05f, 0.035f, 0.0f},
                 {0.12f, 0.08f, 16.4f},
                 {0.84f, 0.92f, 0.95f},
                 {0.04f, 0.07f, 0.08f});
    AddMarkerBox(world.scene,
                 world.rootNode,
                 "CubeTest_BlueAxis",
                 {0.0f, 0.045f, 0.0f},
                 {0.10f, 0.08f, 13.6f},
                 {0.16f, 0.42f, 0.95f},
                 {0.02f, 0.05f, 0.12f});
    AddMarkerBox(world.scene,
                 world.rootNode,
                 "CubeTest_AmberAxis",
                 {0.0f, 0.05f, 0.0f},
                 {13.6f, 0.08f, 0.10f},
                 {1.0f, 0.62f, 0.18f},
                 {0.12f, 0.06f, 0.02f});
    for (const float x : {-7.2f, 7.2f}) {
        for (const float z : {-7.2f, 7.2f}) {
            AddMarkerBox(world.scene,
                         world.rootNode,
                         "CubeTest_CornerPost",
                         {x, 0.46f, z},
                         {0.34f, 0.92f, 0.34f},
                         {0.94f, 0.88f, 0.52f},
                         {0.04f, 0.035f, 0.01f});
        }
    }

    ri::scene::LightNodeOptions sun{};
    sun.nodeName = "CubeTest_Sun";
    sun.parent = world.rootNode;
    sun.transform.rotationDegrees = {-48.0f, -34.0f, 0.0f};
    sun.light.name = "CubeTest_Sun";
    sun.light.type = ri::scene::LightType::Directional;
    sun.light.color = {1.0f, 0.96f, 0.88f};
    sun.light.intensity = 3.8f;
    ri::scene::AddLightNode(world.scene, sun);

    ri::scene::LightNodeOptions fill{};
    fill.nodeName = "CubeTest_CubeFill";
    fill.parent = world.rootNode;
    fill.transform.position = {-3.6f, 3.8f, -3.8f};
    fill.light.name = "CubeTest_CubeFill";
    fill.light.type = ri::scene::LightType::Point;
    fill.light.color = {0.42f, 0.80f, 1.0f};
    fill.light.intensity = 1.8f;
    fill.light.range = 12.0f;
    ri::scene::AddLightNode(world.scene, fill);

    ri::scene::LightNodeOptions rim{};
    rim.nodeName = "CubeTest_WarmRim";
    rim.parent = world.rootNode;
    rim.transform.position = {4.6f, 2.7f, -4.2f};
    rim.light.name = "CubeTest_WarmRim";
    rim.light.type = ri::scene::LightType::Point;
    rim.light.color = {1.0f, 0.62f, 0.30f};
    rim.light.intensity = 1.15f;
    rim.light.range = 10.0f;
    ri::scene::AddLightNode(world.scene, rim);

    for (const float angleDegrees : {0.0f, 90.0f, 180.0f, 270.0f}) {
        const float radians = ri::math::DegreesToRadians(angleDegrees);
        ri::scene::LightNodeOptions ring{};
        ring.nodeName = "CubeTest_RenderRing_" + std::to_string(static_cast<int>(angleDegrees));
        ring.parent = world.rootNode;
        ring.transform.position = {
            std::sin(radians) * 6.8f,
            2.8f,
            std::cos(radians) * 6.8f,
        };
        ring.light.name = ring.nodeName;
        ring.light.type = ri::scene::LightType::Point;
        ring.light.color = {0.72f, 0.86f, 1.0f};
        ring.light.intensity = 0.85f;
        ring.light.range = 8.5f;
        ri::scene::AddLightNode(world.scene, ring);
    }

    world.playerRig = world.scene.CreateNode("CubeTest_PlayerRig", world.rootNode);
    world.scene.GetNode(world.playerRig).localTransform.position = {0.0f, 1.82f, -7.4f};
    world.playerCameraNode = world.scene.CreateNode("CubeTest_PlayerCamera", world.playerRig);
    world.scene.GetNode(world.playerCameraNode).localTransform.rotationDegrees = {-5.0f, 0.0f, 0.0f};
    ri::scene::Camera camera{};
    camera.name = "CubeTest_PlayerCamera";
    camera.fieldOfViewDegrees = 72.0f;
    camera.nearClip = 0.04f;
    camera.farClip = 160.0f;
    world.scene.AttachCamera(world.playerCameraNode, world.scene.AddCamera(camera));

    world.colliders.push_back(ri::trace::TraceCollider{
        .id = "cube-test-platform",
        .bounds = ri::spatial::Aabb{
            .min = {-8.0f, -0.24f, -8.0f},
            .max = {8.0f, 0.02f, 8.0f},
        },
        .structural = true,
        .dynamic = false,
        .simulationTags = {"structural", "walkable", "platform", "p-mesh"},
        .simulationFlags = 1U,
    });

    world.colliders.push_back(ri::trace::TraceCollider{
        .id = "cube-test-query-cube",
        .bounds = ri::spatial::Aabb{
            .min = {-0.6f, 0.12f, -0.6f},
            .max = {0.6f, 1.32f, 0.6f},
        },
        .structural = true,
        .dynamic = false,
        .simulationTags = {"structural", "query", "q-mesh", "interaction"},
        .simulationFlags = 2U,
    });

    return world;
}

void AnimateCubeTestWorld(CubeTestWorld& world, const double elapsedSeconds) {
    if (world.cubeNode == ri::scene::kInvalidHandle) {
        return;
    }
    ri::scene::Node& cube = world.scene.GetNode(world.cubeNode);
    cube.localTransform.rotationDegrees = {
        12.0f + static_cast<float>(std::sin(elapsedSeconds * 0.7) * 7.0),
        28.0f + static_cast<float>(elapsedSeconds * 38.0),
        5.0f,
    };
}

void AnimateCubeTestWorldJiggle(CubeTestWorld& world, const double elapsedSeconds) {
    if (world.cubeNode != ri::scene::kInvalidHandle) {
        ri::scene::Node& cube = world.scene.GetNode(world.cubeNode);
        cube.localTransform.rotationDegrees = {
            static_cast<float>(std::sin(elapsedSeconds * 1.7) * 4.0),
            28.0f + static_cast<float>(elapsedSeconds * 42.0),
            static_cast<float>(std::sin(elapsedSeconds * 2.3) * 2.5),
        };
        cube.localTransform.position.y = 0.72f + static_cast<float>(std::sin(elapsedSeconds * 3.1) * 0.035);
    }
    const auto jiggleSample = [&world, elapsedSeconds](const int node, const float phase, const float spin) {
        if (node == ri::scene::kInvalidHandle) {
            return;
        }
        ri::scene::Node& sample = world.scene.GetNode(node);
        sample.localTransform.rotationDegrees.y = -18.0f + static_cast<float>(elapsedSeconds * spin);
        sample.localTransform.rotationDegrees.x = static_cast<float>(std::sin(elapsedSeconds * 2.0 + phase) * 5.0);
        sample.localTransform.position.y = 0.55f + static_cast<float>(std::sin(elapsedSeconds * 3.7 + phase) * 0.025);
    };
    jiggleSample(world.goldSampleNode, 0.0f, 64.0f);
    jiggleSample(world.copperSampleNode, 1.7f, -58.0f);
    jiggleSample(world.crystalSampleNode, 2.4f, 48.0f);
    if (world.ironSampleNode != ri::scene::kInvalidHandle) {
        ri::scene::Node& iron = world.scene.GetNode(world.ironSampleNode);
        iron.localTransform.rotationDegrees.y = static_cast<float>(elapsedSeconds * 38.0);
        iron.localTransform.position.z = 3.85f + static_cast<float>(std::sin(elapsedSeconds * 2.6) * 0.045);
    }
}

void ConfigureCookedTextureCube(CubeTestWorld& world,
                                std::vector<std::string> logicalTexturePaths,
                                const float framesPerSecond) {
    if (world.cubeNode == ri::scene::kInvalidHandle || logicalTexturePaths.empty()) {
        return;
    }
    const ri::scene::Node& cube = world.scene.GetNode(world.cubeNode);
    if (cube.material == ri::scene::kInvalidHandle) {
        return;
    }
    ri::scene::Material& material = world.scene.GetMaterial(cube.material);
    material.baseColorTexture = logicalTexturePaths.front();
    material.baseColorTextureFrames = std::move(logicalTexturePaths);
    material.baseColorTextureFramesPerSecond = std::max(framesPerSecond, 0.0f);
    material.normalTexture.clear();
    material.ormTexture.clear();
    material.detailTexture.clear();
    material.materialWorkflow = ri::scene::MaterialWorkflow::MetalRough;
    material.metallic = 0.05f;
    material.roughness = 0.64f;
}

const std::vector<std::string>& CubeTestCookedTextureSequence() {
    static const std::vector<std::string> textures{
        "1_basic_refined/1_stone/1_clay/red_clay_brick_6ey1v.png",
        "1_basic_refined/1_stone/4_marble/blue_marble_hs6d8.png",
        "1_basic_refined/2_roof/oxidized_copper_roof_hkw9b.png",
        "2_advanced_refined/2_metal/rusty_iron_block_d2zgw.png",
        "1_basic_refined/1_stone/3_limestone/mossy_dark_gray_limestone_cobbles_8p7c2.png",
    };
    return textures;
}

} // namespace ri::games::cubetest
