#include "RawIron/Games/CubeTest/CubeTestWorld.h"

#include "RawIron/Scene/Helpers.h"

#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>

namespace ri::games::cubetest {

namespace {

namespace fs = std::filesystem;

constexpr const char* kLrtPackageRelativePath = "Assets\\Packages\\LRT - Texture Pack - RT28.8 - 128x";

std::string PackageTexture(const std::string_view relativePath) {
    return (fs::current_path() / fs::path(kLrtPackageRelativePath) / fs::path(relativePath)).lexically_normal().string();
}

ri::scene::PrimitiveNodeOptions CubeMaterialOptions(const int parent) {
    ri::scene::PrimitiveNodeOptions options{};
    options.nodeName = "CubeTest_MappedCube";
    options.parent = parent;
    options.primitive = ri::scene::PrimitiveType::Cube;
    options.transform.position = {0.0f, 1.1f, 0.0f};
    options.transform.rotationDegrees = {0.0f, 28.0f, 0.0f};
    options.transform.scale = {2.0f, 2.0f, 2.0f};
    options.materialName = "cube-test-diamond-full-map";
    options.materialStyle = ri::scene::MaterialStyle::Crystal;
    options.materialWorkflow = ri::scene::MaterialWorkflow::SpecGloss;
    options.baseColor = {1.0f, 1.0f, 1.0f};
    options.baseColorTexture = PackageTexture("tile/rt2_diamond_block.png");
    options.normalTexture = PackageTexture("tile/rt2_diamond_block_n.png");
    options.ormTexture = PackageTexture("tile/rt2_diamond_block_s.png");
    options.detailTexture = options.baseColorTexture;
    options.textureTiling = {1.0f, 1.0f};
    options.metallic = 0.12f;
    options.roughness = 0.34f;
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

} // namespace

CubeTestWorld BuildCubeTestWorld(const std::string_view sceneName) {
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
    platform.baseColor = {0.78f, 0.80f, 0.76f};
    platform.baseColorTexture = PackageTexture("tile/RT_smooth_stone.png");
    platform.normalTexture = PackageTexture("tile/RT_smooth_stone_n.png");
    platform.ormTexture = PackageTexture("tile/RT_smooth_stone_s.png");
    platform.detailTexture = platform.baseColorTexture;
    platform.textureTiling = {8.0f, 8.0f};
    platform.roughness = 0.74f;
    world.platformNode = ri::scene::AddPrimitiveNode(world.scene, platform);
    ApplyStructuralMetadata(world.scene.GetNode(world.platformNode),
                            "cube-test-platform",
                            ri::scene::StructuralBrushSemanticRole::Floor,
                            ri::scene::StructuralBrushCollisionPolicy::Player,
                            ri::scene::StructuralBrushNavigationPolicy::Walkable);

    world.cubeNode = ri::scene::AddPrimitiveNode(world.scene, CubeMaterialOptions(world.rootNode));
    ApplyStructuralMetadata(world.scene.GetNode(world.cubeNode),
                            "cube-test-mapped-cube",
                            ri::scene::StructuralBrushSemanticRole::Structure,
                            ri::scene::StructuralBrushCollisionPolicy::Query,
                            ri::scene::StructuralBrushNavigationPolicy::Ignored);

    ri::scene::LightNodeOptions sun{};
    sun.nodeName = "CubeTest_Sun";
    sun.parent = world.rootNode;
    sun.transform.rotationDegrees = {-48.0f, -34.0f, 0.0f};
    sun.light.name = "CubeTest_Sun";
    sun.light.type = ri::scene::LightType::Directional;
    sun.light.color = {1.0f, 0.96f, 0.88f};
    sun.light.intensity = 3.2f;
    ri::scene::AddLightNode(world.scene, sun);

    ri::scene::LightNodeOptions fill{};
    fill.nodeName = "CubeTest_CubeFill";
    fill.parent = world.rootNode;
    fill.transform.position = {-3.2f, 3.8f, -3.4f};
    fill.light.name = "CubeTest_CubeFill";
    fill.light.type = ri::scene::LightType::Point;
    fill.light.color = {0.42f, 0.80f, 1.0f};
    fill.light.intensity = 1.8f;
    fill.light.range = 12.0f;
    ri::scene::AddLightNode(world.scene, fill);

    world.playerRig = world.scene.CreateNode("CubeTest_PlayerRig", world.rootNode);
    world.scene.GetNode(world.playerRig).localTransform.position = {0.0f, 1.62f, -6.0f};
    world.playerCameraNode = world.scene.CreateNode("CubeTest_PlayerCamera", world.playerRig);
    world.scene.GetNode(world.playerCameraNode).localTransform.rotationDegrees = {-7.5f, 0.0f, 0.0f};
    ri::scene::Camera camera{};
    camera.name = "CubeTest_PlayerCamera";
    camera.fieldOfViewDegrees = 78.0f;
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
            .min = {-1.0f, 0.1f, -1.0f},
            .max = {1.0f, 2.1f, 1.0f},
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
    cube.localTransform.rotationDegrees.y = 28.0f + static_cast<float>(std::sin(elapsedSeconds * 0.35) * 5.0);
}

} // namespace ri::games::cubetest
