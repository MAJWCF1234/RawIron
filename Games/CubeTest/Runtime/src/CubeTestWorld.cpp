#include "RawIron/Games/CubeTest/CubeTestWorld.h"
#include "RawIron/Scene/MaterialCalibration.h"
#include "RawIron/Games/CubeTest/CubeTestGallery.h"
#include "RawIron/Scene/StructuralPrimitiveBundle.h"

#include "RawIron/Content/GameManifest.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Scene/StructuralBrush.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace ri::games::cubetest {

namespace {

namespace fs = std::filesystem;

fs::path ThreeJsReferenceAsset(const fs::path& workspaceRoot, const fs::path& relativePath) {
    return (workspaceRoot / "Games" / "CubeTest" / "assets" / "reference" / "threejs-r185" / relativePath)
        .lexically_normal();
}

ri::scene::PrimitiveNodeOptions CubeMaterialOptions(const int parent, const fs::path& workspaceRoot) {
    ri::scene::PrimitiveNodeOptions options{};
    options.nodeName = "CubeTest_MappedCube";
    options.parent = parent;
    options.primitive = ri::scene::PrimitiveType::Cube;
    options.transform.position = {0.0f, 0.72f, 0.0f};
    options.transform.rotationDegrees = {0.0f, 28.0f, 0.0f};
    options.transform.scale = {1.2f, 1.2f, 1.2f};
    options.materialName = "cube-test-threejs-hardwood";
    options.materialStyle = ri::scene::MaterialStyle::Standard;
    options.materialWorkflow = ri::scene::MaterialWorkflow::MetalRough;
    options.baseColor = {1.0f, 1.0f, 1.0f};
    options.baseColorTexture = ThreeJsReferenceAsset(workspaceRoot, "textures/hardwood2_diffuse.jpg").string();
    // Do not reinterpret unrelated bump/specular images as normal/ORM data.
    options.textureTiling = {1.0f, 1.0f};
    options.metallic = 0.02f;
    options.roughness = 0.48f;
    return options;
}

ri::scene::PrimitiveNodeOptions MaterialSampleOptions(const int parent,
                                                      const fs::path& workspaceRoot,
                                                      const std::string_view name,
                                                      const ri::math::Vec3& baseTint,
                                                      const ri::math::Vec3& position,
                                                      const ri::math::Vec3& scale,
                                                      const float metallic,
                                                      const float roughness) {
    // Same upstream UV grid for each sample: only authored PBR values/tint vary.
    ri::scene::PrimitiveNodeOptions options{};
    options.nodeName = std::string(name);
    options.parent = parent;
    options.primitive = ri::scene::PrimitiveType::Cube;
    options.transform.position = position;
    options.transform.rotationDegrees = {0.0f, -18.0f, 0.0f};
    options.transform.scale = scale;
    options.materialName = std::string(name) + "-threejs-uv-grid";
    options.materialStyle = ri::scene::MaterialStyle::Standard;
    options.materialWorkflow = ri::scene::MaterialWorkflow::MetalRough;
    options.baseColor = baseTint;
    options.baseColorTexture = ThreeJsReferenceAsset(workspaceRoot, "textures/uv_grid_opengl.jpg").string();
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

constexpr float kCapabilityRoomSpacing = 26.0f;
constexpr std::size_t kSpriteCount = 512U;

int AddCapabilityPlatform(ri::scene::Scene& scene,
                          const int parent,
                          const std::string_view name,
                          const float centerX,
                          const ri::math::Vec3& color) {
    ri::scene::PrimitiveNodeOptions platform{};
    platform.nodeName = std::string(name);
    platform.parent = parent;
    platform.primitive = ri::scene::PrimitiveType::Cube;
    platform.transform.position = {centerX, -0.12f, 0.0f};
    platform.transform.scale = {16.0f, 0.24f, 16.0f};
    platform.materialName = std::string(name) + "-material";
    platform.baseColor = color;
    platform.roughness = 0.78f;
    return ri::scene::AddPrimitiveNode(scene, platform);
}

void AddGeometryExhibit(CubeTestWorld& world, const fs::path& workspaceRoot,
                        std::string name, std::string type, const ri::structural::StructuralPrimitiveOptions& shape,
                        ri::math::Vec3 position, ri::math::Vec3 rotation = {}, ri::math::Vec3 scale = {1,1,1}) {
    ri::scene::StructuralPrimitiveBundleParams params;
    params.nodeName = name;
    params.parent = world.rootNode;
    params.primitiveTypeField = std::move(type);
    params.shape = shape;
    params.transform.position = position;
    params.transform.rotationDegrees = rotation;
    params.transform.scale = scale;
    params.material.materialName = name + "-UV-grid";
    params.material.baseColorTexture = ThreeJsReferenceAsset(workspaceRoot, "textures/uv_grid_opengl.jpg").string();
    params.material.roughness = .48f;
    params.material.metallic = .08f;
    params.material.doubleSided = true;
    const int node = ri::scene::SpawnStructuralPrimitiveBundle(world.scene,params).node;
    if (node == ri::scene::kInvalidHandle) throw std::runtime_error("Structural exhibit failed: " + name);
    AddMarkerBox(world.scene,world.rootNode,name+"_Plinth",{position.x,.15f,position.z},
                 {3.6f,.3f,3.6f},{.20f,.25f,.30f});
    // Conservative exhibit collision; all gallery travel lanes remain clear.
    if (const auto bounds = ri::scene::ComputeNodeWorldBounds(world.scene,node)) {
        world.colliders.push_back({.id=name+"-display",.bounds={.min=bounds->min,.max=bounds->max},.structural=true,.dynamic=false,
            .simulationTags={"exhibit","procedural"},.simulationFlags=2U});
    }
    world.colliders.push_back({.id=name+"-plinth",.bounds={.min={position.x-1.8f,0,position.z-1.8f},
        .max={position.x+1.8f,.3f,position.z+1.8f}},.structural=true,.dynamic=false,
        .simulationTags={"exhibit","plinth"},.simulationFlags=2U});
}

void AddProceduralCapabilityRooms(CubeTestWorld& world, const fs::path& workspaceRoot) {
    // Only authoring data lives here: every exhibit goes through the existing
    // structural primitive dispatcher, bundle, brush metadata and scene adapter.
    ri::structural::StructuralPrimitiveOptions shape;
    const float latheX = FindCubeTestRoom("lathe")->centerX;
    AddCapabilityPlatform(world.scene,world.rootNode,"CubeTest_LathePlatform",latheX,{.14f,.24f,.28f});
    shape.closedProfile = false;
    for (int i=0;i<50;++i) shape.points.push_back({(std::sin(i*.2f)*std::sin(i*.1f)*15+50)*.024f,i*.048f,0});
    shape.radialSegments=20;
    AddGeometryExhibit(world,workspaceRoot,"CubeTest_Procedural_Lathe20","revolve",shape,{latheX-1.8f,.35f,-3});
    shape.radialSegments=96;
    AddGeometryExhibit(world,workspaceRoot,"CubeTest_Procedural_Lathe96","revolve",shape,{latheX-1.8f,.35f,3});
    shape.radialSegments=64; shape.sweepDegrees=258;
    AddGeometryExhibit(world,workspaceRoot,"CubeTest_Procedural_LatheCutaway","revolve",shape,{latheX+2.5f,.35f,3});

    const float tubeX = FindCubeTestRoom("tubes")->centerX;
    AddCapabilityPlatform(world.scene,world.rootNode,"CubeTest_TubePlatform",tubeX,{.21f,.16f,.28f});
    shape={}; shape.pathSegments=96; shape.sides=20; shape.thickness=.18f;
    shape.points={{-1.2f,.4f,-1},{-.7f,1.5f,.8f},{0,2.5f,0},{.8f,1.4f,-1},{1.2f,.5f,1}};
    AddGeometryExhibit(world,workspaceRoot,"CubeTest_Procedural_OpenTube","spline_sweep",shape,{tubeX-1.8f,.35f,-3});
    shape.points={{1.25f,1.3f,0},{0,2.5f,1},{-1.25f,1.3f,0},{0,.3f,-1}};
    shape.pathSegments=128; shape.thickness=.22f; shape.closedPath=true; shape.sides=6;
    AddGeometryExhibit(world,workspaceRoot,"CubeTest_Procedural_ClosedTube6","spline_sweep",shape,{tubeX-1.8f,.35f,3});
    shape.sides=24;
    AddGeometryExhibit(world,workspaceRoot,"CubeTest_Procedural_ClosedTube24","spline_sweep",shape,{tubeX+2.5f,.35f,3});

    const float surfaceX = FindCubeTestRoom("surfaces")->centerX;
    AddCapabilityPlatform(world.scene,world.rootNode,"CubeTest_SurfacePlatform",surfaceX,{.18f,.27f,.19f});
    shape={}; shape.radialSegments=64; shape.sides=24; shape.thickness=.124f;
    AddGeometryExhibit(world,workspaceRoot,"CubeTest_Procedural_Torus","torus",shape,{surfaceX-1.8f,1.8f,-3},{52,0,0},{3.06f,3.06f,3.06f});
    shape.radialSegments=96; shape.sides=16; shape.thickness=.137f;
    AddGeometryExhibit(world,workspaceRoot,"CubeTest_Procedural_Mobius","mobius",shape,{surfaceX-1.8f,1.8f,3},{48,0,0},{3.28f,3.28f,3.28f});
    shape={}; shape.cellsX=40; shape.cellsY=40; shape.depth=1.12f;
    AddGeometryExhibit(world,workspaceRoot,"CubeTest_Procedural_Saddle","parametric_patch",shape,{surfaceX+2.5f,1.5f,3},{},{2.8f,2.8f,2.8f});
}

void AddPortalGate(ri::scene::Scene& scene,
                   const int parent,
                   const std::string_view name,
                   const float x,
                   const ri::math::Vec3& color) {
    const std::string prefix(name);
    AddMarkerBox(scene, parent, prefix + "_NorthPillar", {x, 1.15f, -1.25f}, {0.34f, 2.3f, 0.34f}, color, color * 0.28f);
    AddMarkerBox(scene, parent, prefix + "_SouthPillar", {x, 1.15f, 1.25f}, {0.34f, 2.3f, 0.34f}, color, color * 0.28f);
    AddMarkerBox(scene, parent, prefix + "_Lintel", {x, 2.35f, 0.0f}, {0.34f, 0.34f, 2.84f}, color, color * 0.34f);

    ri::scene::PrimitiveNodeOptions field{};
    field.nodeName = prefix + "_TravelField";
    field.parent = parent;
    field.primitive = ri::scene::PrimitiveType::Cube;
    field.transform.position = {x, 1.12f, 0.0f};
    field.transform.scale = {0.06f, 2.0f, 2.0f};
    field.materialName = prefix + "-field-material";
    field.shadingModel = ri::scene::ShadingModel::Unlit;
    field.baseColor = color;
    field.emissiveColor = color * 0.85f;
    field.transparent = true;
    field.opacity = 0.34f;
    field.doubleSided = true;
    field.additiveBlend = true;
    ri::scene::AddPrimitiveNode(scene, field);
}

std::array<std::vector<ri::math::Vec3>, 4> BuildSpriteLayouts(const ri::math::Vec3& center) {
    std::array<std::vector<ri::math::Vec3>, 4> layouts{};
    for (auto& layout : layouts) layout.reserve(kSpriteCount);

    // Preserve the reference example's authored proportions and equations under one uniform
    // metres-per-CSS-pixel conversion. The deterministic PRNG is intentional for repeatable tests.
    constexpr float referenceScale = 0.0022f;

    constexpr int amountX = 16;
    constexpr int amountZ = 32;
    constexpr float referenceSpacing = 150.0f;
    constexpr float offsetX = ((amountX - 1) * referenceSpacing) * 0.5f;
    constexpr float offsetZ = ((amountZ - 1) * referenceSpacing) * 0.5f;
    for (std::size_t index = 0; index < kSpriteCount; ++index) {
        const float referenceX = static_cast<float>(static_cast<int>(index) % amountX) * referenceSpacing;
        const float referenceZ = static_cast<float>(static_cast<int>(index) / amountX) * referenceSpacing;
        const float referenceY = (std::sin(referenceX * 0.5f) + std::sin(referenceZ * 0.5f)) * 200.0f;
        layouts[0].push_back(center + ri::math::Vec3{
            (referenceX - offsetX) * referenceScale,
            referenceY * referenceScale,
            (referenceZ - offsetZ) * referenceScale});
    }

    constexpr int cubeAmount = 8;
    constexpr float cubeOffset = ((cubeAmount - 1) * referenceSpacing) * 0.5f;
    for (std::size_t index = 0; index < kSpriteCount; ++index) {
        const int xIndex = static_cast<int>(index) % cubeAmount;
        const int yIndex = (static_cast<int>(index) / cubeAmount) % cubeAmount;
        const int zIndex = static_cast<int>(index) / (cubeAmount * cubeAmount);
        layouts[1].push_back(center + ri::math::Vec3{
            (static_cast<float>(xIndex) * referenceSpacing - cubeOffset) * referenceScale,
            (static_cast<float>(yIndex) * referenceSpacing - cubeOffset) * referenceScale,
            (static_cast<float>(zIndex) * referenceSpacing - cubeOffset) * referenceScale});
    }

    std::uint32_t randomState = 0x52415749U;
    auto randomSigned = [&randomState]() {
        randomState = randomState * 1664525U + 1013904223U;
        return static_cast<float>((randomState >> 8U) & 0xFFFFFFU) / 8388607.5f - 1.0f;
    };
    for (std::size_t index = 0; index < kSpriteCount; ++index) {
        layouts[2].push_back(center + ri::math::Vec3{
            randomSigned() * 2000.0f * referenceScale,
            randomSigned() * 2000.0f * referenceScale,
            randomSigned() * 2000.0f * referenceScale});
    }

    constexpr float sphereRadius = 750.0f * referenceScale;
    for (std::size_t index = 0; index < kSpriteCount; ++index) {
        const float phi = std::acos(-1.0f + (2.0f * static_cast<float>(index)) / static_cast<float>(kSpriteCount));
        const float theta = std::sqrt(static_cast<float>(kSpriteCount) * ri::math::kPi) * phi;
        layouts[3].push_back(center + ri::math::Vec3{
            sphereRadius * std::cos(theta) * std::sin(phi),
            sphereRadius * std::cos(phi),
            sphereRadius * std::sin(theta) * std::sin(phi)});
    }
    return layouts;
}

void AddSpriteCapabilityRoom(CubeTestWorld& world, const fs::path& workspaceRoot) {
    const float centerX = kCapabilityRoomSpacing;
    AddCapabilityPlatform(world.scene, world.rootNode, "CubeTest_SpriteRoomPlatform", centerX, {0.24f, 0.34f, 0.46f});
    world.spriteLayouts = BuildSpriteLayouts({centerX, 4.0f, 0.0f});

    const int material = world.scene.AddMaterial(ri::scene::Material{
        .name = "cube-test-native-sprite",
        .shadingModel = ri::scene::ShadingModel::Unlit,
        .baseColor = {1.0f, 1.0f, 1.0f},
        .baseColorTexture = ThreeJsReferenceAsset(workspaceRoot, "textures/sprite.png").string(),
        .emissiveColor = {0.06f, 0.10f, 0.14f},
        .roughness = 1.0f,
        .opacity = 1.0f,
        .alphaCutoff = 0.01f,
        .doubleSided = true,
        .transparent = true,
        .additiveBlend = false,
    });

    constexpr int framesPerTransition = 8;
    constexpr float transitionWindowSeconds = 4.0f;
    constexpr float referenceSpriteSize = 64.0f * 0.0022f;
    std::vector<ri::math::Vec3> centers(kSpriteCount);
    std::vector<float> sizes(kSpriteCount);
    world.spriteFrameMeshes.reserve(world.spriteLayouts.size() * framesPerTransition);
    for (std::size_t fromLayout = 0; fromLayout < world.spriteLayouts.size(); ++fromLayout) {
        const std::size_t toLayout = (fromLayout + 1U) % world.spriteLayouts.size();
        for (int frame = 0; frame < framesPerTransition; ++frame) {
            const float transitionTime = transitionWindowSeconds * static_cast<float>(frame)
                / static_cast<float>(framesPerTransition - 1);
            for (std::size_t index = 0; index < kSpriteCount; ++index) {
                const std::uint32_t durationHash = static_cast<std::uint32_t>(index) * 2654435761U;
                const float duration = 2.0f
                    + static_cast<float>((durationHash >> 24U) & 0xFFU) / 255.0f * 2.0f;
                const float linear = std::clamp(transitionTime / duration, 0.0f, 1.0f);
                const float eased = linear <= 0.0f ? 0.0f : linear >= 1.0f ? 1.0f
                    : linear < 0.5f ? std::pow(2.0f, 20.0f * linear - 10.0f) * 0.5f
                                    : (2.0f - std::pow(2.0f, -20.0f * linear + 10.0f)) * 0.5f;
                centers[index] = ri::math::Lerp(
                    world.spriteLayouts[fromLayout][index], world.spriteLayouts[toLayout][index], eased);
                const float referenceX = (centers[index].x - centerX) / 0.0022f;
                const float pulse = std::sin((std::floor(referenceX) + transitionTime * 1000.0f) * 0.002f) * 0.3f + 1.0f;
                sizes[index] = referenceSpriteSize * pulse;
            }
            world.spriteFrameMeshes.push_back(world.scene.AddMesh(ri::scene::MakeBillboardCloudMesh(
                "CubeTest_NativeSpriteCloud_" + std::to_string(fromLayout) + "_" + std::to_string(frame),
                centers,
                sizes)));
        }
    }
    world.spriteNode = world.scene.CreateNode("CubeTest_NativeSpriteCloud", world.rootNode);
    world.scene.AttachMesh(world.spriteNode, world.spriteFrameMeshes.front(), material);
}

void AddNormalsCapabilityRoom(CubeTestWorld& world, const fs::path& workspaceRoot) {
    const float centerX = kCapabilityRoomSpacing * 2.0f;
    AddCapabilityPlatform(world.scene, world.rootNode, "CubeTest_GltfNormalsRoomPlatform", centerX, {0.38f, 0.30f, 0.44f});

    // Faithful asset target for three.js `webgl_materials_normalmap.html`. Raw Iron imports the
    // original glTF geometry, then binds its authored maps through our Material contract so the
    // desktop and OpenXR renderers exercise the same scene data rather than a copied WebGL demo.
    const int leePerrySmithMaterial = world.scene.AddMaterial(ri::scene::Material{
        .name = "threejs-lee-perry-smith-normal-material",
        .materialWorkflow = ri::scene::MaterialWorkflow::SpecGloss,
        .baseColor = {0.937f, 0.937f, 0.937f},
        .baseColorTexture = ThreeJsReferenceAsset(
            workspaceRoot, "models/gltf/LeePerrySmith/Map-COL.jpg").string(),
        .metallic = 0.0f,
        .roughness = 0.34f,
        .normalTexture = ThreeJsReferenceAsset(
            workspaceRoot, "models/gltf/LeePerrySmith/Infinite-Level_02_Tangent_SmoothUV.jpg").string(),
        .normalScale = {1.0f, 1.0f},
        .ormTexture = ThreeJsReferenceAsset(
            workspaceRoot, "models/gltf/LeePerrySmith/Map-SPEC.jpg").string(),
    });
    const std::size_t nodeCountBeforeHeadImport = world.scene.NodeCount();
    std::string headImportError;
    const int leePerrySmithHead = ri::scene::AddGltfModelNode(
        world.scene,
        ri::scene::GltfModelOptions{
            .sourcePath = ThreeJsReferenceAsset(
                workspaceRoot, "models/gltf/LeePerrySmith/LeePerrySmith.glb"),
            .wrapperNodeName = "CubeTest_ThreeJsLeePerrySmithHead",
            .parent = world.rootNode,
            .transform = ri::scene::Transform{
                .position = {centerX, 1.65f, -5.2f},
                .rotationDegrees = {0.0f, 180.0f, 0.0f},
                .scale = {0.6f, 0.6f, 0.6f},
            },
        },
        &headImportError);
    if (leePerrySmithHead == ri::scene::kInvalidHandle) {
        ri::core::LogInfo("Cube Test Lee Perry-Smith reference import failed: " + headImportError);
    } else {
        // glTF import intentionally preserves its own material. This demo overrides only the
        // newly appended render nodes with the reference example's externally supplied maps.
        for (std::size_t nodeIndex = nodeCountBeforeHeadImport; nodeIndex < world.scene.NodeCount(); ++nodeIndex) {
            ri::scene::Node& node = world.scene.GetNode(static_cast<int>(nodeIndex));
            if (node.mesh != ri::scene::kInvalidHandle) node.material = leePerrySmithMaterial;
        }
    }

    const int comparison=ri::scene::AddNormalMappingComparisonPanels(world.scene, {
        {}, ThreeJsReferenceAsset(workspaceRoot,"textures/NormalMapOpenGL.png").string(),
        ThreeJsReferenceAsset(workspaceRoot,"textures/NormalMapDirectX.png").string()},world.rootNode);
    auto& comparisonTransform=world.scene.GetNode(comparison).localTransform;
    comparisonTransform.position={centerX+3.0f,3.4f,0};
    comparisonTransform.rotationDegrees={0,90,0};

}

void AddExporterCapabilityRoom(CubeTestWorld& world, const fs::path& workspaceRoot) {
    const float centerX = kCapabilityRoomSpacing * 3.0f;
    AddCapabilityPlatform(world.scene, world.rootNode, "CubeTest_GltfExporterRoomPlatform", centerX, {0.42f, 0.38f, 0.22f});

    const int hierarchyRoot = world.scene.CreateNode("CubeTest_ExportHierarchy", world.rootNode);
    world.scene.GetNode(hierarchyRoot).localTransform.position = {centerX, 0.0f, 0.0f};
    ri::scene::PrimitiveNodeOptions parent{};
    parent.nodeName = "CubeTest_ExportParentCube";
    parent.parent = hierarchyRoot;
    parent.primitive = ri::scene::PrimitiveType::Cube;
    parent.transform.position = {0.0f, 1.2f, 0.0f};
    parent.transform.scale = {1.8f, 1.8f, 1.8f};
    parent.materialName = "export-parent-pbr";
    parent.baseColor = {0.94f, 0.55f, 0.16f};
    parent.baseColorTexture = ThreeJsReferenceAsset(workspaceRoot, "textures/hardwood2_diffuse.jpg").string();
    parent.metallic = 0.56f;
    parent.roughness = 0.24f;
    const int parentCube = ri::scene::AddPrimitiveNode(world.scene, parent);
    ri::scene::PrimitiveNodeOptions child{};
    child.nodeName = "CubeTest_ExportChildSphere";
    child.parent = parentCube;
    child.primitive = ri::scene::PrimitiveType::Sphere;
    child.transform.position = {0.0f, 1.25f, 0.0f};
    child.transform.scale = {0.42f, 0.42f, 0.42f};
    child.materialName = "export-child-transparent";
    child.baseColor = {0.32f, 0.72f, 1.0f};
    child.baseColorTexture = ThreeJsReferenceAsset(workspaceRoot, "textures/uv_grid_opengl.jpg").string();
    child.transparent = true;
    child.opacity = 0.62f;
    child.doubleSided = true;
    ri::scene::AddPrimitiveNode(world.scene, child);

    const int instanceMesh = world.scene.AddMesh(ri::scene::Mesh{
        .name = "CubeTest_ExportInstanceMesh", .primitive = ri::scene::PrimitiveType::Cube,
        .vertexCount = 24, .indexCount = 36});
    const int instanceMaterial = world.scene.AddMaterial(ri::scene::Material{
        .name = "export-instance-material", .shadingModel = ri::scene::ShadingModel::Unlit,
        .baseColor = {0.92f,0.82f,0.28f}, .emissiveColor = {0.16f,0.10f,0.01f}, .roughness = 0.8f});
    ri::scene::MeshInstanceBatch batch{};
    batch.name = "CubeTest_ExportInstanceBatch";
    batch.parent = world.rootNode;
    batch.mesh = instanceMesh;
    batch.material = instanceMaterial;
    for (int index = 0; index < 50; ++index) {
        const float angle = static_cast<float>(index) * (2.0f * ri::math::kPi / 50.0f);
        batch.transforms.push_back(ri::scene::Transform{
            .position = {centerX + std::cos(angle) * 4.4f, 1.0f + static_cast<float>(index % 5) * 0.42f, std::sin(angle) * 4.4f},
            .rotationDegrees = {0.0f, static_cast<float>(index * 19), 0.0f},
            .scale = {0.28f, 0.28f, 0.28f}});
    }
    world.exporterInstanceBatch = world.scene.AddMeshInstanceBatch(std::move(batch));

    std::string shaderBallError;
    world.shaderBallNode = ri::scene::AddGltfModelNode(
        world.scene,
        ri::scene::GltfModelOptions{
            .sourcePath = ThreeJsReferenceAsset(workspaceRoot, "models/gltf/ShaderBall.glb"),
            .wrapperNodeName = "CubeTest_ThreeJsShaderBall",
            .parent = hierarchyRoot,
            .transform = ri::scene::Transform{
                .position = {2.7f, 1.1f, -2.5f},
                .rotationDegrees = {0.0f, -35.0f, 0.0f},
                .scale = {1.1f, 1.1f, 1.1f},
            },
        },
        &shaderBallError);
    if (world.shaderBallNode == ri::scene::kInvalidHandle) {
        ri::core::LogInfo("Cube Test ShaderBall reference import failed: " + shaderBallError);
    }

    std::string coffeeError;
    world.coffeeModelNode = ri::scene::AddGltfModelNode(
        world.scene,
        ri::scene::GltfModelOptions{
            .sourcePath = ThreeJsReferenceAsset(workspaceRoot, "models/gltf/coffeemat.glb"),
            .wrapperNodeName = "CubeTest_ThreeJsCompressedCoffee",
            .parent = hierarchyRoot,
            .transform = ri::scene::Transform{
                .position = {-2.7f, 0.12f, -2.5f},
                .rotationDegrees = {0.0f, 35.0f, 0.0f},
                // Source model is about 209 units tall. A ~3 m exhibit must not
                // enclose/shadow neighboring 16 m rooms or the entire gallery.
                .scale = {0.014f, 0.014f, 0.014f},
            },
        },
        &coffeeError);
    if (world.coffeeModelNode == ri::scene::kInvalidHandle) {
        ri::core::LogInfo("Cube Test compressed coffee reference import failed: " + coffeeError);
    }
}

void AddInteractionCapabilityRoom(CubeTestWorld& world) {
    const float centerX = kCapabilityRoomSpacing * 4.0f;
    AddCapabilityPlatform(
        world.scene, world.rootNode, "CubeTest_XrInteractionRoomPlatform", centerX, {0.20f, 0.42f, 0.36f});
    world.interactionRoomRoot = world.scene.CreateNode("CubeTest_XrInteractionRoom", world.rootNode);
    AddMarkerBox(world.scene, world.interactionRoomRoot, "CubeTest_XrInteractionNorthRail",
                 {centerX, 1.35f, -5.8f}, {10.5f, 0.10f, 0.10f}, {0.16f, 0.86f, 0.78f}, {0.02f, 0.22f, 0.18f});
    AddMarkerBox(world.scene, world.interactionRoomRoot, "CubeTest_XrInteractionSouthRail",
                 {centerX, 1.35f, 5.8f}, {10.5f, 0.10f, 0.10f}, {1.0f, 0.42f, 0.16f}, {0.24f, 0.06f, 0.01f});

    world.interactionField.bounds = {
        .min = {centerX - 5.2f, 0.02f, -5.2f},
        .max = {centerX + 5.2f, 4.8f, 5.2f}};
    world.interactionField.gravity = {0.0f, -9.81f, 0.0f};
    world.interactionField.linearDampingPerSecond = 0.12f;
    world.interactionField.restitution = 0.76f;
    world.interactionField.resolvePropContacts = true;

    constexpr int propCount = 24;
    world.interactionPropNodes.reserve(propCount);
    world.interactionProps.reserve(propCount);
    for (int index = 0; index < propCount; ++index) {
        const int column = index % 6;
        const int layer = index / 6;
        const float x = centerX - 3.5f + static_cast<float>(column) * 1.4f;
        const float y = 0.55f + static_cast<float>(layer) * 0.72f;
        const float z = -2.0f + static_cast<float>((index * 5) % 9) * 0.5f;
        const ri::math::Vec3 color{
            0.24f + static_cast<float>((index * 37) % 60) / 100.0f,
            0.30f + static_cast<float>((index * 23) % 55) / 100.0f,
            0.34f + static_cast<float>((index * 41) % 60) / 100.0f};
        const int node = AddMarkerBox(
            world.scene,
            world.interactionRoomRoot,
            "CubeTest_XrInteractiveCube_" + std::to_string(index),
            {x, y, z},
            {0.34f, 0.34f, 0.34f},
            color,
            color * 0.08f);
        world.interactionPropNodes.push_back(node);
        world.interactionProps.push_back(ri::world::InteractivePropState{
            .id = "xr-interactive-cube-" + std::to_string(index),
            .position = {x, y, z},
            .halfExtents = {0.17f, 0.17f, 0.17f},
            .velocity = {
                static_cast<float>((index % 3) - 1) * 0.32f,
                0.15f + static_cast<float>(index % 5) * 0.08f,
                static_cast<float>(((index + 1) % 3) - 1) * 0.28f},
            .angularVelocityDegrees = {18.0f + index, 32.0f - index * 0.4f, 12.0f}});
    }
}

void AddProjectileCapabilityRoom(CubeTestWorld& world) {
    const float centerX = kCapabilityRoomSpacing * 5.0f;
    AddCapabilityPlatform(
        world.scene, world.rootNode, "CubeTest_ProjectileRoomPlatform", centerX, {0.34f, 0.20f, 0.18f});
    world.projectileRoomRoot = world.scene.CreateNode("CubeTest_ProjectileRoom", world.rootNode);
    AddMarkerBox(world.scene, world.projectileRoomRoot, "CubeTest_ProjectileBackstop",
                 {centerX + 5.15f, 1.65f, 0.0f}, {0.18f, 3.3f, 10.4f},
                 {0.62f, 0.20f, 0.16f}, {0.18f, 0.02f, 0.01f});
    AddMarkerBox(world.scene, world.projectileRoomRoot, "CubeTest_ProjectileFiringLine",
                 {centerX - 3.6f, 0.04f, 0.0f}, {0.12f, 0.08f, 9.2f},
                 {1.0f, 0.72f, 0.18f}, {0.18f, 0.07f, 0.01f});

    world.projectileField.bounds = {
        .min = {centerX - 5.2f, 0.02f, -5.2f},
        .max = {centerX + 5.0f, 5.2f, 5.2f}};
    world.projectileField.gravity = {0.0f, -9.81f, 0.0f};
    world.projectileField.linearDampingPerSecond = 0.08f;
    world.projectileField.restitution = 0.58f;
    world.projectileField.resolvePropContacts = true;

    constexpr int targetCount = 18;
    constexpr int projectileCount = 32;
    world.projectilePropNodes.reserve(targetCount + projectileCount);
    world.projectileProps.reserve(targetCount + projectileCount);
    for (int index = 0; index < targetCount; ++index) {
        const int column = index % 6;
        const int row = index / 6;
        const ri::math::Vec3 position{
            centerX + 2.0f + static_cast<float>(row) * 0.55f,
            0.27f + static_cast<float>(column) * 0.52f,
            -1.3f + static_cast<float>(row) * 1.3f};
        const ri::math::Vec3 color{
            0.72f + static_cast<float>(column % 2) * 0.16f,
            0.22f + static_cast<float>(row) * 0.16f,
            0.14f + static_cast<float>((column + row) % 3) * 0.13f};
        const int node = AddMarkerBox(
            world.scene, world.projectileRoomRoot,
            "CubeTest_ProjectileTarget_" + std::to_string(index),
            position, {0.48f, 0.48f, 0.48f}, color, color * 0.06f);
        world.projectilePropNodes.push_back(node);
        world.projectileProps.push_back({
            .id = "projectile-target-" + std::to_string(index),
            .position = position,
            .halfExtents = {0.24f, 0.24f, 0.24f},
            .inverseMass = 0.72f});
    }
    for (int index = 0; index < projectileCount; ++index) {
        const ri::math::Vec3 color = index % 2 == 0
            ? ri::math::Vec3{0.20f, 0.72f, 1.0f}
            : ri::math::Vec3{1.0f, 0.42f, 0.14f};
        const int node = AddMarkerBox(
            world.scene, world.projectileRoomRoot,
            "CubeTest_PooledProjectile_" + std::to_string(index),
            {centerX, -10.0f, 0.0f}, {0.24f, 0.24f, 0.24f}, color, color * 0.12f);
        world.scene.GetNode(node).localTransform.scale = {};
        world.projectilePropNodes.push_back(node);
        world.projectileProps.push_back({
            .id = "pooled-projectile-" + std::to_string(index),
            .position = {centerX, -10.0f, 0.0f},
            .halfExtents = {0.12f, 0.12f, 0.12f},
            .inverseMass = 1.8f,
            .active = false});
    }
}

void AddTeleportCapabilityRoom(CubeTestWorld& world) {
    const float centerX = kCapabilityRoomSpacing * 6.0f;
    AddCapabilityPlatform(
        world.scene, world.rootNode, "CubeTest_TeleportRoomPlatform", centerX, {0.20f, 0.28f, 0.48f});
    world.teleportRoomRoot = world.scene.CreateNode("CubeTest_TeleportRoom", world.rootNode);
    const struct Pad {
        const char* name;
        ri::math::Vec3 position;
        ri::math::Vec3 scale;
        ri::math::Vec3 color;
    } pads[]{
        {"CubeTest_TeleportPadLow", {centerX + 1.8f, 0.25f, -2.6f}, {2.8f, 0.50f, 2.8f}, {0.16f, 0.72f, 0.92f}},
        {"CubeTest_TeleportPadHigh", {centerX + 3.6f, 0.75f, 1.8f}, {3.0f, 1.50f, 3.0f}, {0.82f, 0.34f, 0.96f}},
        {"CubeTest_TeleportPadCenter", {centerX - 0.2f, 0.45f, 2.7f}, {2.4f, 0.90f, 2.4f}, {0.24f, 0.92f, 0.52f}},
    };
    for (const Pad& pad : pads) {
        AddMarkerBox(
            world.scene, world.teleportRoomRoot, pad.name,
            pad.position, pad.scale, pad.color, pad.color * 0.08f);
        world.colliders.push_back({
            .id = std::string(pad.name) + "-p-mesh",
            .bounds = {
                .min = pad.position - pad.scale * 0.5f,
                .max = pad.position + pad.scale * 0.5f},
            .structural = true,
            .dynamic = false,
            .simulationTags = {"structural", "teleport-landing", "p-mesh"},
            .simulationFlags = 1U});
    }
    AddMarkerBox(
        world.scene, world.teleportRoomRoot, "CubeTest_TeleportClearanceReject",
        {centerX + 3.6f, 2.45f, 1.8f}, {2.2f, 0.25f, 2.2f},
        {1.0f, 0.18f, 0.20f}, {0.24f, 0.01f, 0.01f});
    world.colliders.push_back({
        .id = "cube-test-teleport-clearance-reject",
        .bounds = {
            .min = {centerX + 2.5f, 2.325f, 0.7f},
            .max = {centerX + 4.7f, 2.575f, 2.9f}},
        .structural = true,
        .dynamic = false,
        .simulationTags = {"structural", "teleport-clearance-reject", "p-mesh"},
        .simulationFlags = 1U});
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
                              {0.95f, 0.72f, 0.18f},
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
                              {0.86f, 0.42f, 0.26f},
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
                              {0.54f, 0.59f, 0.64f},
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
                              {0.28f, 0.82f, 0.96f},
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

    AddSpriteCapabilityRoom(world, workspaceRoot);
    AddNormalsCapabilityRoom(world, workspaceRoot);
    AddExporterCapabilityRoom(world, workspaceRoot);
    AddInteractionCapabilityRoom(world);
    AddProjectileCapabilityRoom(world);
    AddTeleportCapabilityRoom(world);

    AddProceduralCapabilityRooms(world, workspaceRoot);
    const auto rooms = CubeTestRoomGuides();
    for (std::size_t i=1; i<rooms.size(); ++i) {
        AddPortalGate(world.scene,world.rootNode,"CubeTest_Portal_"+std::string(rooms[i-1].id)+"To"+std::string(rooms[i].id),
                      rooms[i-1].centerX+7.25f,{.18f,.72f,1});
        AddPortalGate(world.scene,world.rootNode,"CubeTest_Portal_"+std::string(rooms[i].id)+"To"+std::string(rooms[i-1].id),
                      rooms[i].centerX-7.25f,{.96f,.48f,.20f});
    }

    ri::scene::LightNodeOptions sun{};
    sun.nodeName = "CubeTest_Sun";
    sun.parent = world.rootNode;
    sun.transform.rotationDegrees = {48.0f, -34.0f, 0.0f};
    sun.light.name = "CubeTest_Sun";
    sun.light.type = ri::scene::LightType::Directional;
    sun.light.color = {1.0f, 0.96f, 0.88f};
    // Keep the neutral material gallery below the clipping shoulder.  This maps to
    // the engine's calibrated directional-light range, rather than a raw HDR value.
    sun.light.intensity = 1.0f;
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

    for (std::size_t room = 1; room < rooms.size(); ++room) {
        const float centerX = rooms[room].centerX;
        world.colliders.push_back(ri::trace::TraceCollider{
            .id = "cube-test-capability-platform-" + std::to_string(room),
            .bounds = {.min = {centerX - 8.0f, -0.24f, -8.0f}, .max = {centerX + 8.0f, 0.02f, 8.0f}},
            .structural = true,
            .dynamic = false,
            .simulationTags = {"structural", "walkable", "capability-room", "p-mesh"},
            .simulationFlags = 1U,
        });
    }

    world.colliders.push_back(ri::trace::TraceCollider{
        .id = "cube-test-export-parent-cube",
        .bounds = {.min = {77.1f, 0.3f, -0.9f}, .max = {78.9f, 2.1f, 0.9f}},
        .structural = true,
        .dynamic = false,
        .simulationTags = {"structural", "export-test", "hierarchy"},
        .simulationFlags = 2U,
    });

    for (std::size_t i=1; i<rooms.size(); ++i) {
        const auto& from = rooms[i-1];
        const auto& to = rooms[i];
        world.portals.push_back({.id=std::string(from.id)+"-to-"+std::string(to.id),
            .triggerBounds={.min={from.centerX+6.78f,.02f,-.92f},.max={from.centerX+7.72f,2.25f,.92f}},
            .destinationFeet=CubeTestRoomArrival(to),.destinationYawDegrees=90,
            .destinationId=std::string(to.id),.label="To "+std::string(to.title)});
        world.portals.push_back({.id=std::string(to.id)+"-to-"+std::string(from.id),
            .triggerBounds={.min={to.centerX-7.72f,.02f,-.92f},.max={to.centerX-6.78f,2.25f,.92f}},
            .destinationFeet={from.centerX+6.25f,.20f,0},.destinationYawDegrees=-90,
            .destinationId=std::string(from.id),.label="To "+std::string(from.title)});
    }

    return world;
}

CubeTestWorld BuildCubeTestCalibrationWorld(const std::filesystem::path& workspaceRoot, bool normalComparison) {
    const ri::scene::MaterialCalibrationTextures textures{
        ThreeJsReferenceAsset(workspaceRoot, "textures/hardwood2_diffuse.jpg").string(),
        ThreeJsReferenceAsset(workspaceRoot, "textures/NormalMapOpenGL.png").string(),
        ThreeJsReferenceAsset(workspaceRoot, "textures/NormalMapDirectX.png").string(),
    };
    auto fixture = normalComparison ? ri::scene::BuildNormalMappingComparisonScene(textures)
                                    : ri::scene::BuildMaterialCalibrationScene(textures);
    CubeTestWorld world;
    world.scene = std::move(fixture.scene);
    world.materialCalibration = true;
    world.rootNode = fixture.root;
    world.platformNode = fixture.floor;
    world.playerRig = fixture.cameraRig;
    world.playerCameraNode = fixture.camera;
    return world;
}

void AnimateCubeTestWorld(CubeTestWorld& world, const double elapsedSeconds, const bool simulateDynamicProps) {
    if (world.cubeNode == ri::scene::kInvalidHandle) {
        return;
    }
    ri::scene::Node& cube = world.scene.GetNode(world.cubeNode);
    cube.localTransform.rotationDegrees = {
        12.0f + static_cast<float>(std::sin(elapsedSeconds * 0.7) * 7.0),
        28.0f + static_cast<float>(elapsedSeconds * 38.0),
        5.0f,
    };

    if (world.spriteNode != ri::scene::kInvalidHandle && !world.spriteFrameMeshes.empty()) {
        constexpr double layoutSeconds = 6.0;
        constexpr double transitionSeconds = 4.0;
        const std::size_t fromLayout = static_cast<std::size_t>(std::floor(elapsedSeconds / layoutSeconds)) % world.spriteLayouts.size();
        const double localSeconds = std::fmod(std::max(elapsedSeconds, 0.0), layoutSeconds);
        constexpr std::size_t framesPerTransition = 8U;
        const float progress = static_cast<float>(std::clamp(localSeconds / transitionSeconds, 0.0, 1.0));
        const std::size_t localFrame = std::min(
            static_cast<std::size_t>(std::lround(progress * static_cast<float>(framesPerTransition - 1U))),
            framesPerTransition - 1U);
        world.scene.GetNode(world.spriteNode).mesh =
            world.spriteFrameMeshes[fromLayout * framesPerTransition + localFrame];
    }

    if (!world.interactionProps.empty()
        && world.interactionProps.size() == world.interactionPropNodes.size()) {
        const float deltaSeconds = world.interactionSimulationTime <= 0.0
            ? 1.0f / 60.0f
            : static_cast<float>(std::clamp(elapsedSeconds - world.interactionSimulationTime, 0.0, 0.10));
        world.interactionSimulationTime = elapsedSeconds;
        const ri::world::InteractivePropStepReport report = ri::world::StepInteractivePropField(
            world.interactionProps, simulateDynamicProps ? deltaSeconds : 0.0f, world.interactionField);
        for (std::size_t index = 0; index < world.interactionProps.size(); ++index) {
            const ri::world::InteractivePropState& prop = world.interactionProps[index];
            ri::scene::Transform& transform =
                world.scene.GetNode(world.interactionPropNodes[index]).localTransform;
            transform.position = prop.position;
            transform.rotationDegrees = transform.rotationDegrees
                + prop.angularVelocityDegrees * deltaSeconds;
        }
        (void)report;
    }

    if (!world.projectileProps.empty()
        && world.projectileProps.size() == world.projectilePropNodes.size()) {
        const float deltaSeconds = world.projectileSimulationTime <= 0.0
            ? 1.0f / 60.0f
            : static_cast<float>(std::clamp(elapsedSeconds - world.projectileSimulationTime, 0.0, 0.10));
        world.projectileSimulationTime = elapsedSeconds;
        (void)ri::world::StepInteractivePropField(
            world.projectileProps, simulateDynamicProps ? deltaSeconds : 0.0f, world.projectileField);
        for (std::size_t index = 0; index < world.projectileProps.size(); ++index) {
            const ri::world::InteractivePropState& prop = world.projectileProps[index];
            ri::scene::Transform& transform =
                world.scene.GetNode(world.projectilePropNodes[index]).localTransform;
            transform.position = prop.position;
            transform.scale = prop.active
                ? prop.halfExtents * 2.0f
                : ri::math::Vec3{};
            if (prop.active) {
                transform.rotationDegrees = transform.rotationDegrees
                    + prop.angularVelocityDegrees * deltaSeconds;
            }
        }
    }
}

ri::world::InteractivePropEmissionResult EmitCubeTestProjectile(
    CubeTestWorld& world,
    const ri::math::Vec3& origin,
    const ri::math::Vec3& direction) {
    if (world.projectileProps.size() <= 18U) return {};
    if (origin.x < world.projectileField.bounds.min.x
        || origin.x > world.projectileField.bounds.max.x
        || origin.y < world.projectileField.bounds.min.y
        || origin.y > world.projectileField.bounds.max.y
        || origin.z < world.projectileField.bounds.min.z
        || origin.z > world.projectileField.bounds.max.z) return {};
    ri::world::InteractivePropEmissionResult result = ri::world::EmitInteractiveProp(
        std::span<ri::world::InteractivePropState>(world.projectileProps).subspan(18U),
        {.position = origin,
         .direction = direction,
         .angularVelocityDegrees = {180.0f, 240.0f, 120.0f},
         .speed = 11.5f,
         .lifetimeSeconds = 8.0f,
         .recycleOldest = true});
    if (result.propIndex >= 0) result.propIndex += 18;
    return result;
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
