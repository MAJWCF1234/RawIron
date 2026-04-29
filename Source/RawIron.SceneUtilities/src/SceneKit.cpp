#include "RawIron/Scene/SceneKit.h"

#include "RawIron/Math/Vec3.h"
#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Scene/Raycast.h"
#include "RawIron/Scene/SceneUtils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ri::scene {

namespace {

struct DemoRig {
    Scene scene{"SceneKit"};
    int root = kInvalidHandle;
    int stage = kInvalidHandle;
    int hero = kInvalidHandle;
    OrbitCameraHandles orbitCamera{};
};

struct ExampleDescriptor {
    const char* slug;
    const char* title;
    const char* officialUrl;
    const char* rawIronTrack;
    const char* statusLabel;
};

constexpr std::array<ExampleDescriptor, 10> kExampleDescriptors = {{
    {"scene_controls_orbit", "Orbit controls", "reference://scene_controls_orbit",
     "orbit camera + helpers + preview shell", "foundation-live"},
    {"scene_geometry_cube", "Geometry cube", "reference://scene_geometry_cube",
     "primitive mesh nodes + materials + transforms", "foundation-live"},
    {"scene_interactive_cubes", "Interactive cubes", "reference://scene_interactive_cubes",
     "scene raycast utilities + primitive picking + input shell", "foundation-live"},
    {"scene_terrain_raycast", "Terrain raycasting",
     "reference://scene_terrain_raycast",
     "custom terrain mesh + scene raycast utilities", "preview-live"},
    {"scene_lighting_spotlights", "Spot lights", "reference://scene_lighting_spotlights",
     "spot-light descriptors + software preview lighting", "preview-live"},
    {"scene_loader_gltf", "glTF loader", "reference://scene_loader_gltf",
     "asset import pipeline + scene instantiation", "preview-live"},
    {"scene_animation_keyframes", "Animation keyframes",
     "reference://scene_animation_keyframes",
     "scene-authored keyframe sampling preview", "preview-live"},
    {"scene_instancing_performance", "Instancing performance",
     "reference://scene_instancing_performance",
     "high-count repeated node preview", "preview-live"},
    {"scene_materials_envmaps", "Environment maps",
     "reference://scene_materials_envmaps",
     "reflection-bay material staging preview", "preview-live"},
    {"scene_audio_orientation", "Positional audio orientation",
     "reference://scene_audio_orientation",
     "listener/source layout preview for native audio integration", "preview-live"},
}};

SceneKitMilestoneResult MakeResult(const ExampleDescriptor& descriptor) {
    SceneKitMilestoneResult result{};
    result.slug = descriptor.slug;
    result.title = descriptor.title;
    result.officialUrl = descriptor.officialUrl;
    result.rawIronTrack = descriptor.rawIronTrack;
    result.statusLabel = descriptor.statusLabel;
    result.scene = Scene(result.title);
    return result;
}

std::optional<ExampleDescriptor> FindDescriptor(std::string_view slug) {
    for (const ExampleDescriptor& descriptor : kExampleDescriptors) {
        if (descriptor.slug == slug) {
            return descriptor;
        }
    }
    return std::nullopt;
}

DemoRig BuildBaseDemoRig(std::string_view sceneName, bool includeHero = false, float stageSize = 10.0f) {
    DemoRig rig{};
    rig.scene = Scene(std::string(sceneName));
    rig.root = rig.scene.CreateNode("World");

    LightNodeOptions sun{};
    sun.nodeName = "Sun";
    sun.parent = rig.root;
    sun.transform.rotationDegrees = ri::math::Vec3{-42.0f, 28.0f, 0.0f};
    sun.light = Light{
        .name = "Sun",
        .type = LightType::Directional,
        .color = ri::math::Vec3{1.0f, 0.94f, 0.88f},
        .intensity = 3.8f,
    };
    (void)AddLightNode(rig.scene, sun);

    OrbitCameraOptions orbit{};
    orbit.parent = rig.root;
    orbit.camera = Camera{
        .name = "SceneKitCamera",
        .projection = ProjectionType::Perspective,
        .fieldOfViewDegrees = 64.0f,
        .nearClip = 0.1f,
        .farClip = 500.0f,
    };
    orbit.orbit = OrbitCameraState{
        .target = ri::math::Vec3{0.0f, 0.65f, 0.0f},
        .distance = 7.4f,
        .yawDegrees = 156.0f,
        .pitchDegrees = -18.0f,
    };
    rig.orbitCamera = AddOrbitCamera(rig.scene, orbit);

    PrimitiveNodeOptions stage{};
    stage.nodeName = "Stage";
    stage.parent = rig.root;
    stage.primitive = PrimitiveType::Plane;
    stage.materialName = "StageMaterial";
    stage.shadingModel = ShadingModel::Lit;
    stage.baseColor = ri::math::Vec3{0.30f, 0.24f, 0.20f};
    stage.transform.position = ri::math::Vec3{0.0f, -0.25f, 0.0f};
    stage.transform.scale = ri::math::Vec3{stageSize, 1.0f, stageSize};
    rig.stage = AddPrimitiveNode(rig.scene, stage);

    GridHelperOptions grid{};
    grid.parent = rig.root;
    grid.nodeName = "Grid";
    grid.size = stageSize * 0.95f;
    grid.color = ri::math::Vec3{0.18f, 0.22f, 0.28f};
    grid.transform.position = ri::math::Vec3{0.0f, -0.23f, 0.0f};
    (void)AddGridHelper(rig.scene, grid);

    AxesHelperOptions axes{};
    axes.parent = rig.root;
    axes.nodeName = "Axes";
    axes.axisLength = 1.6f;
    axes.axisThickness = 0.05f;
    axes.transform.position = ri::math::Vec3{-stageSize * 0.36f, 0.0f, -stageSize * 0.36f};
    (void)AddAxesHelper(rig.scene, axes);

    if (includeHero) {
        PrimitiveNodeOptions hero{};
        hero.nodeName = "HeroCube";
        hero.parent = rig.root;
        hero.primitive = PrimitiveType::Cube;
        hero.materialName = "HeroMaterial";
        hero.shadingModel = ShadingModel::Lit;
        hero.baseColor = ri::math::Vec3{0.32f, 0.55f, 0.88f};
        hero.transform.position = ri::math::Vec3{0.0f, 0.55f, 0.0f};
        hero.transform.rotationDegrees = ri::math::Vec3{-12.0f, 34.0f, 8.0f};
        hero.transform.scale = ri::math::Vec3{1.2f, 1.2f, 1.2f};
        rig.hero = AddPrimitiveNode(rig.scene, hero);
    }

    return rig;
}

int AddCustomMeshNode(Scene& scene,
                      const std::string& nodeName,
                      int parent,
                      Mesh mesh,
                      const Transform& transform,
                      std::string materialName,
                      const ri::math::Vec3& baseColor,
                      ShadingModel shadingModel = ShadingModel::Lit) {
    mesh.name = nodeName + "Mesh";
    mesh.primitive = PrimitiveType::Custom;
    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    mesh.indexCount = static_cast<int>(mesh.indices.size());

    const int materialHandle = scene.AddMaterial(Material{
        .name = std::move(materialName),
        .shadingModel = shadingModel,
        .baseColor = baseColor,
    });
    const int meshHandle = scene.AddMesh(std::move(mesh));
    const int nodeHandle = scene.CreateNode(nodeName, parent);
    scene.GetNode(nodeHandle).localTransform = transform;
    scene.AttachMesh(nodeHandle, meshHandle, materialHandle);
    return nodeHandle;
}

Mesh BuildTerrainMesh(int rows = 20, int columns = 20, float spacing = 0.48f) {
    Mesh mesh{};
    mesh.primitive = PrimitiveType::Custom;
    mesh.positions.reserve(static_cast<std::size_t>(rows * columns));
    mesh.indices.reserve(static_cast<std::size_t>((rows - 1) * (columns - 1) * 6));

    const float halfWidth = static_cast<float>(columns - 1) * spacing * 0.5f;
    const float halfDepth = static_cast<float>(rows - 1) * spacing * 0.5f;
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const float x = static_cast<float>(column) * spacing - halfWidth;
            const float z = static_cast<float>(row) * spacing - halfDepth;
            const float radial = std::sqrt((x * x) + (z * z));
            const float y = std::sin(x * 0.85f) * 0.28f +
                            std::cos(z * 0.65f) * 0.24f -
                            (radial * 0.08f);
            mesh.positions.push_back(ri::math::Vec3{x, y, z});
        }
    }

    for (int row = 0; row + 1 < rows; ++row) {
        for (int column = 0; column + 1 < columns; ++column) {
            const int topLeft = row * columns + column;
            const int topRight = topLeft + 1;
            const int bottomLeft = topLeft + columns;
            const int bottomRight = bottomLeft + 1;
            mesh.indices.push_back(topLeft);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(topRight);
            mesh.indices.push_back(topRight);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(bottomRight);
        }
    }

    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    mesh.indexCount = static_cast<int>(mesh.indices.size());
    return mesh;
}

Mesh BuildPyramidMesh() {
    Mesh mesh{};
    mesh.primitive = PrimitiveType::Custom;
    mesh.positions = {
        {-0.40f, -0.30f, -0.40f},
        {0.40f, -0.30f, -0.40f},
        {0.40f, -0.30f, 0.40f},
        {-0.40f, -0.30f, 0.40f},
        {0.0f, 0.26f, 0.82f},
    };
    mesh.indices = {
        0, 1, 2, 0, 2, 3,
        0, 4, 1, 1, 4, 2,
        2, 4, 3, 3, 4, 0,
    };
    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    mesh.indexCount = static_cast<int>(mesh.indices.size());
    return mesh;
}

ri::math::Vec3 SampleKeyframedPosition(float t) {
    const std::array<ri::math::Vec3, 4> positions = {{
        {-2.2f, 0.55f, -1.3f},
        {-0.6f, 1.15f, -0.2f},
        {1.0f, 0.85f, 0.7f},
        {2.4f, 0.55f, 1.2f},
    }};

    const float clamped = std::clamp(t, 0.0f, 1.0f);
    const float segmentFloat = clamped * static_cast<float>(positions.size() - 1);
    const int segment = std::min(static_cast<int>(segmentFloat), static_cast<int>(positions.size() - 2));
    const float localT = segmentFloat - static_cast<float>(segment);
    return ri::math::Lerp(positions[static_cast<std::size_t>(segment)],
                          positions[static_cast<std::size_t>(segment + 1)],
                          localT);
}

float SampleKeyframedYaw(float t) {
    const std::array<float, 4> rotations = {{0.0f, 56.0f, 128.0f, 212.0f}};
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    const float segmentFloat = clamped * static_cast<float>(rotations.size() - 1);
    const int segment = std::min(static_cast<int>(segmentFloat), static_cast<int>(rotations.size() - 2));
    const float localT = segmentFloat - static_cast<float>(segment);
    const float start = rotations[static_cast<std::size_t>(segment)];
    const float end = rotations[static_cast<std::size_t>(segment + 1)];
    return start + ((end - start) * localT);
}

bool RunRenderValidation(const SceneKitMilestoneCallbacks& callbacks,
                         const std::string& slug,
                         const Scene& scene,
                         int cameraNode,
                         std::string& detail) {
    if (!callbacks.renderValidator) {
        return true;
    }
    return callbacks.renderValidator(slug, scene, cameraNode, detail);
}

bool FinalizeResult(SceneKitMilestoneResult& result, const SceneKitMilestoneCallbacks& callbacks) {
    if (!result.passed) {
        return false;
    }
    result.passed = RunRenderValidation(callbacks, result.slug, result.scene, result.cameraNode, result.detail);
    return result.passed;
}

SceneKitMilestoneResult EvaluateGeometryCube(const ExampleDescriptor& descriptor,
                                             const SceneKitMilestoneCallbacks& callbacks) {
    DemoRig rig = BuildBaseDemoRig("SceneKitGeometryCube", true);
    SceneKitMilestoneResult result = MakeResult(descriptor);
    result.scene = std::move(rig.scene);
    result.cameraNode = rig.orbitCamera.cameraNode;
    result.focusNode = rig.hero;
    const bool hasHero = FindNodeByName(result.scene, "HeroCube").has_value();
    const std::size_t renderables = CollectRenderableNodes(result.scene).size();
    result.passed = result.cameraNode != kInvalidHandle && hasHero && renderables >= 4U;
    result.detail = "hero=HeroCube renderables=" + std::to_string(renderables);
    FinalizeResult(result, callbacks);
    return result;
}

SceneKitMilestoneResult EvaluateOrbitControls(const ExampleDescriptor& descriptor,
                                              const SceneKitMilestoneCallbacks& callbacks) {
    DemoRig rig = BuildBaseDemoRig("SceneKitOrbitControls", true);
    const ri::math::Vec3 before = rig.scene.ComputeWorldPosition(rig.orbitCamera.cameraNode);
    OrbitCameraState adjusted = rig.orbitCamera.orbit;
    adjusted.yawDegrees += 38.0f;
    adjusted.pitchDegrees -= 7.0f;
    adjusted.distance += 1.2f;
    adjusted.target = ri::math::Vec3{0.45f, 0.80f, -0.10f};
    SetOrbitCameraState(rig.scene, rig.orbitCamera, adjusted);
    const bool framed = FrameNodesWithOrbitCamera(rig.scene, rig.orbitCamera, {rig.hero}, 1.30f);
    const ri::math::Vec3 after = rig.scene.ComputeWorldPosition(rig.orbitCamera.cameraNode);

    SceneKitMilestoneResult result = MakeResult(descriptor);
    result.scene = std::move(rig.scene);
    result.cameraNode = rig.orbitCamera.cameraNode;
    result.focusNode = rig.hero;
    result.passed = framed && ri::math::Distance(before, after) > 0.5f;
    result.detail = "camera moved from " + ri::math::ToString(before) +
                    " to " + ri::math::ToString(after);
    FinalizeResult(result, callbacks);
    return result;
}

SceneKitMilestoneResult EvaluateInteractiveCubes(const ExampleDescriptor& descriptor,
                                                 const SceneKitMilestoneCallbacks& callbacks) {
    DemoRig rig = BuildBaseDemoRig("SceneKitInteractiveCubes", false);
    std::vector<int> cubeHandles;
    const std::array<ri::math::Vec3, 5> positions = {{
        {-2.2f, 0.45f, 0.6f},
        {-1.1f, 0.60f, -0.4f},
        {0.0f, 0.75f, 0.0f},
        {1.1f, 0.55f, 0.5f},
        {2.2f, 0.40f, -0.6f},
    }};
    const std::array<ri::math::Vec3, 5> colors = {{
        {0.82f, 0.28f, 0.22f},
        {0.90f, 0.58f, 0.18f},
        {0.30f, 0.64f, 0.94f},
        {0.30f, 0.80f, 0.42f},
        {0.78f, 0.36f, 0.84f},
    }};

    for (std::size_t index = 0; index < positions.size(); ++index) {
        PrimitiveNodeOptions cube{};
        cube.nodeName = index == 2U ? "PickTargetCube" : "InteractiveCube" + std::to_string(index + 1);
        cube.parent = rig.root;
        cube.primitive = PrimitiveType::Cube;
        cube.materialName = cube.nodeName + "Material";
        cube.baseColor = colors[index];
        cube.transform.position = positions[index];
        cube.transform.scale = ri::math::Vec3{0.72f, 0.72f, 0.72f};
        cubeHandles.push_back(AddPrimitiveNode(rig.scene, cube));
    }

    (void)FrameNodesWithOrbitCamera(rig.scene, rig.orbitCamera, cubeHandles, 1.55f);
    const std::optional<Ray> ray = BuildPerspectiveCameraRay(rig.scene, rig.orbitCamera.cameraNode, 0.5f, 0.5f, 1.0f);
    const std::optional<RaycastHit> hit = ray.has_value() ? RaycastSceneNearest(rig.scene, *ray) : std::nullopt;

    SceneKitMilestoneResult result = MakeResult(descriptor);
    result.scene = std::move(rig.scene);
    result.cameraNode = rig.orbitCamera.cameraNode;
    result.focusNode = cubeHandles[2];
    result.passed = hit.has_value() && result.scene.GetNode(hit->node).name == "PickTargetCube";
    result.detail = hit.has_value()
        ? ("picked " + result.scene.GetNode(hit->node).name + " at " + ri::math::ToString(hit->position))
        : "no cube selected";
    FinalizeResult(result, callbacks);
    return result;
}

SceneKitMilestoneResult EvaluateTerrainRaycast(const ExampleDescriptor& descriptor,
                                               const SceneKitMilestoneCallbacks& callbacks) {
    DemoRig rig = BuildBaseDemoRig("SceneKitTerrainRaycast", false, 12.0f);
    const int terrain = AddCustomMeshNode(
        rig.scene,
        "TerrainMesh",
        rig.root,
        BuildTerrainMesh(),
        Transform{},
        "TerrainMaterial",
        ri::math::Vec3{0.34f, 0.48f, 0.31f});

    const Ray probe{
        .origin = ri::math::Vec3{0.72f, 5.0f, -0.35f},
        .direction = ri::math::Vec3{0.0f, -1.0f, 0.0f},
    };
    const std::optional<RaycastHit> hit = RaycastNode(rig.scene, terrain, probe);
    int marker = kInvalidHandle;
    if (hit.has_value()) {
        PrimitiveNodeOptions markerCube{};
        markerCube.nodeName = "TerrainHitMarker";
        markerCube.parent = rig.root;
        markerCube.primitive = PrimitiveType::Cube;
        markerCube.materialName = "TerrainHitMarkerMaterial";
        markerCube.shadingModel = ShadingModel::Unlit;
        markerCube.baseColor = ri::math::Vec3{1.0f, 0.72f, 0.28f};
        markerCube.transform.position = hit->position + ri::math::Vec3{0.0f, 0.22f, 0.0f};
        markerCube.transform.scale = ri::math::Vec3{0.26f, 0.26f, 0.26f};
        marker = AddPrimitiveNode(rig.scene, markerCube);
        (void)FrameNodesWithOrbitCamera(rig.scene, rig.orbitCamera, {terrain, marker}, 1.25f);
    }

    SceneKitMilestoneResult result = MakeResult(descriptor);
    result.scene = std::move(rig.scene);
    result.cameraNode = rig.orbitCamera.cameraNode;
    result.focusNode = marker != kInvalidHandle ? marker : terrain;
    result.passed = hit.has_value() && hit->normal.y > 0.35f;
    result.detail = hit.has_value()
        ? ("terrain hit at " + ri::math::ToString(hit->position) + " normal=" + ri::math::ToString(hit->normal))
        : "terrain ray missed";
    FinalizeResult(result, callbacks);
    return result;
}

SceneKitMilestoneResult EvaluateSpotLights(const ExampleDescriptor& descriptor,
                                           const SceneKitMilestoneCallbacks& callbacks) {
    DemoRig rig = BuildBaseDemoRig("SceneKitSpotLights", false);

    const std::array<ri::math::Vec3, 3> propPositions = {{
        {-1.8f, 0.65f, -0.2f},
        {0.0f, 0.95f, 0.0f},
        {1.8f, 0.55f, 0.3f},
    }};
    for (std::size_t index = 0; index < propPositions.size(); ++index) {
        PrimitiveNodeOptions prop{};
        prop.nodeName = "SpotProp" + std::to_string(index + 1);
        prop.parent = rig.root;
        prop.primitive = PrimitiveType::Cube;
        prop.materialName = prop.nodeName + "Material";
        prop.baseColor = index == 1U
            ? ri::math::Vec3{0.80f, 0.82f, 0.86f}
            : ri::math::Vec3{0.50f, 0.40f, 0.32f};
        prop.transform.position = propPositions[index];
        prop.transform.scale = index == 1U
            ? ri::math::Vec3{0.85f, 1.80f, 0.85f}
            : ri::math::Vec3{0.70f, 1.10f, 0.70f};
        (void)AddPrimitiveNode(rig.scene, prop);
    }

    LightNodeOptions warmSpot{};
    warmSpot.nodeName = "WarmSpot";
    warmSpot.parent = rig.root;
    warmSpot.transform.position = ri::math::Vec3{-2.2f, 3.6f, -1.2f};
    warmSpot.transform.rotationDegrees = ri::math::Vec3{-72.0f, -32.0f, 0.0f};
    warmSpot.light = Light{
        .name = "WarmSpot",
        .type = LightType::Spot,
        .color = ri::math::Vec3{1.0f, 0.78f, 0.44f},
        .intensity = 12.0f,
        .range = 10.0f,
        .spotAngleDegrees = 30.0f,
    };
    (void)AddLightNode(rig.scene, warmSpot);

    LightNodeOptions coolSpot{};
    coolSpot.nodeName = "CoolSpot";
    coolSpot.parent = rig.root;
    coolSpot.transform.position = ri::math::Vec3{2.1f, 3.4f, -1.4f};
    coolSpot.transform.rotationDegrees = ri::math::Vec3{-68.0f, 28.0f, 0.0f};
    coolSpot.light = Light{
        .name = "CoolSpot",
        .type = LightType::Spot,
        .color = ri::math::Vec3{0.48f, 0.72f, 1.0f},
        .intensity = 11.0f,
        .range = 10.0f,
        .spotAngleDegrees = 34.0f,
    };
    (void)AddLightNode(rig.scene, coolSpot);

    SceneKitMilestoneResult result = MakeResult(descriptor);
    result.scene = std::move(rig.scene);
    result.cameraNode = rig.orbitCamera.cameraNode;
    result.focusNode = FindNodeByName(result.scene, "SpotProp2").value_or(kInvalidHandle);
    result.passed = CollectLightNodes(result.scene).size() >= 3U;
    result.detail = "lights=" + std::to_string(CollectLightNodes(result.scene).size()) + " includes two spotlights";
    FinalizeResult(result, callbacks);
    return result;
}

SceneKitMilestoneResult EvaluateGltfLoader(const ExampleDescriptor& descriptor,
                                           const SceneKitMilestoneOptions& options,
                                           const SceneKitMilestoneCallbacks& callbacks) {
    DemoRig rig = BuildBaseDemoRig("SceneKitGltfLoader", false);
    const std::filesystem::path gltfPath = options.assetRoot / "scenekit_triangle.gltf";

    std::string loadError;
    const int modelNode = AddGltfModelNode(
        rig.scene,
        GltfModelOptions{
            .sourcePath = gltfPath,
            .wrapperNodeName = "GltfPreview",
            .parent = rig.root,
            .transform =
                Transform{
                    .position = ri::math::Vec3{0.0f, 0.45f, 0.0f},
                    .rotationDegrees = ri::math::Vec3{-18.0f, 18.0f, 0.0f},
                    .scale = ri::math::Vec3{1.8f, 1.8f, 1.8f},
                },
        },
        &loadError);
    if (modelNode != kInvalidHandle) {
        (void)FrameNodesWithOrbitCamera(rig.scene, rig.orbitCamera, {modelNode}, 1.65f);
    }

    SceneKitMilestoneResult result = MakeResult(descriptor);
    result.scene = std::move(rig.scene);
    result.cameraNode = rig.orbitCamera.cameraNode;
    result.focusNode = modelNode;
    if (modelNode == kInvalidHandle) {
        result.passed = false;
        result.detail = loadError;
        return result;
    }

    result.passed = result.scene.MeshCount() >= 1U;
    result.detail = "loaded " + gltfPath.filename().string() +
                    " meshes=" + std::to_string(result.scene.MeshCount());
    FinalizeResult(result, callbacks);
    return result;
}

SceneKitMilestoneResult EvaluateAnimationKeyframes(const ExampleDescriptor& descriptor,
                                                   const SceneKitMilestoneCallbacks& callbacks) {
    DemoRig rig = BuildBaseDemoRig("SceneKitAnimationKeyframes", false);
    const std::array<ri::math::Vec3, 4> keys = {{
        SampleKeyframedPosition(0.00f),
        SampleKeyframedPosition(0.33f),
        SampleKeyframedPosition(0.66f),
        SampleKeyframedPosition(1.00f),
    }};

    for (std::size_t index = 0; index < keys.size(); ++index) {
        PrimitiveNodeOptions marker{};
        marker.nodeName = "KeyMarker" + std::to_string(index + 1);
        marker.parent = rig.root;
        marker.primitive = PrimitiveType::Cube;
        marker.materialName = marker.nodeName + "Material";
        marker.shadingModel = ShadingModel::Unlit;
        marker.baseColor = ri::math::Vec3{0.24f + (0.14f * static_cast<float>(index)), 0.72f, 0.92f};
        marker.transform.position = keys[index] + ri::math::Vec3{0.0f, -0.08f, 0.0f};
        marker.transform.scale = ri::math::Vec3{0.18f, 0.18f, 0.18f};
        (void)AddPrimitiveNode(rig.scene, marker);
    }

    const float sampleT = 0.62f;
    PrimitiveNodeOptions animated{};
    animated.nodeName = "AnimatedCube";
    animated.parent = rig.root;
    animated.primitive = PrimitiveType::Cube;
    animated.materialName = "AnimatedCubeMaterial";
    animated.baseColor = ri::math::Vec3{0.96f, 0.62f, 0.22f};
    animated.transform.position = SampleKeyframedPosition(sampleT);
    animated.transform.rotationDegrees = ri::math::Vec3{0.0f, SampleKeyframedYaw(sampleT), 0.0f};
    animated.transform.scale = ri::math::Vec3{0.62f, 0.62f, 0.62f};
    const int animatedCube = AddPrimitiveNode(rig.scene, animated);
    (void)FrameNodesWithOrbitCamera(rig.scene, rig.orbitCamera, {animatedCube}, 2.2f);

    SceneKitMilestoneResult result = MakeResult(descriptor);
    result.scene = std::move(rig.scene);
    result.cameraNode = rig.orbitCamera.cameraNode;
    result.focusNode = animatedCube;
    const ri::math::Vec3 sampledPosition = result.scene.GetNode(animatedCube).localTransform.position;
    result.passed = ri::math::Distance(sampledPosition, keys.front()) > 0.25f &&
                    ri::math::Distance(sampledPosition, keys.back()) > 0.25f;
    result.detail = "sampled t=0.62 position=" + ri::math::ToString(sampledPosition) +
                    " yaw=" + std::to_string(static_cast<int>(SampleKeyframedYaw(sampleT)));
    FinalizeResult(result, callbacks);
    return result;
}

SceneKitMilestoneResult EvaluateInstancingPerformance(const ExampleDescriptor& descriptor,
                                                      const SceneKitMilestoneCallbacks& callbacks) {
    DemoRig rig = BuildBaseDemoRig("SceneKitInstancing", false, 14.0f);
    const int sharedMesh = rig.scene.AddMesh(Mesh{
        .name = "InstanceCubeMesh",
        .primitive = PrimitiveType::Cube,
        .positions = {},
        .indices = {},
    });
    const int sharedMaterial = rig.scene.AddMaterial(Material{
        .name = "InstanceCubeMaterial",
        .shadingModel = ShadingModel::Lit,
        .baseColor = ri::math::Vec3{0.42f, 0.52f, 0.78f},
    });
    const int instanceBatch = rig.scene.AddMeshInstanceBatch(MeshInstanceBatch{
        .name = "InstanceCubeBatch",
        .parent = rig.root,
        .mesh = sharedMesh,
        .material = sharedMaterial,
        .transforms = {},
    });

    for (int z = 0; z < 12; ++z) {
        for (int x = 0; x < 12; ++x) {
            rig.scene.AddMeshInstance(instanceBatch, Transform{
                .position = ri::math::Vec3{
                    -4.2f + (static_cast<float>(x) * 0.76f),
                    0.22f + (std::sin(static_cast<float>(x + z) * 0.45f) * 0.12f),
                    -4.2f + (static_cast<float>(z) * 0.76f),
                },
                .rotationDegrees = ri::math::Vec3{0.0f, static_cast<float>((x * 9 + z * 5) % 360), 0.0f},
                .scale = ri::math::Vec3{0.26f, 0.26f, 0.26f},
            });
        }
    }
    PrimitiveNodeOptions focusAnchor{};
    focusAnchor.nodeName = "InstanceFocusAnchor";
    focusAnchor.parent = rig.root;
    focusAnchor.primitive = PrimitiveType::Cube;
    focusAnchor.materialName = "InstanceFocusAnchorMaterial";
    focusAnchor.shadingModel = ShadingModel::Unlit;
    focusAnchor.baseColor = ri::math::Vec3{0.92f, 0.82f, 0.34f};
    focusAnchor.transform.position = ri::math::Vec3{0.0f, 0.45f, 0.0f};
    focusAnchor.transform.scale = ri::math::Vec3{0.32f, 0.32f, 0.32f};
    const int focusAnchorHandle = AddPrimitiveNode(rig.scene, focusAnchor);
    (void)FrameNodesWithOrbitCamera(rig.scene, rig.orbitCamera, {focusAnchorHandle}, 2.2f);

    SceneKitMilestoneResult result = MakeResult(descriptor);
    result.scene = std::move(rig.scene);
    result.cameraNode = rig.orbitCamera.cameraNode;
    result.focusNode = focusAnchorHandle;
    result.passed = result.scene.MeshInstanceCount() >= 100U;
    result.detail = "mesh-instance batch count=" + std::to_string(result.scene.MeshInstanceBatchCount()) +
                    " instances=" + std::to_string(result.scene.MeshInstanceCount());
    FinalizeResult(result, callbacks);
    return result;
}

float ComputeSpeakerYawDegrees(const ri::math::Vec3& speakerPosition, const ri::math::Vec3& listenerPosition) {
    const ri::math::Vec3 offset = listenerPosition - speakerPosition;
    constexpr float kRadiansToDegrees = 57.29577951308232f;
    return static_cast<float>(std::atan2(offset.x, offset.z) * kRadiansToDegrees);
}

SceneKitMilestoneResult EvaluateEnvironmentMaps(const ExampleDescriptor& descriptor,
                                                const SceneKitMilestoneCallbacks& callbacks) {
    DemoRig rig = BuildBaseDemoRig("SceneKitEnvMaps", false, 9.0f);
    const std::array<ri::math::Vec3, 5> wallColors = {{
        {0.36f, 0.56f, 0.88f}, {0.88f, 0.46f, 0.32f}, {0.42f, 0.78f, 0.48f},
        {0.84f, 0.76f, 0.34f}, {0.54f, 0.42f, 0.88f},
    }};
    const std::array<Transform, 5> wallTransforms = {{
        Transform{.position = ri::math::Vec3{0.0f, 2.1f, -3.4f}, .rotationDegrees = ri::math::Vec3{90.0f, 0.0f, 0.0f}, .scale = ri::math::Vec3{5.0f, 1.0f, 2.4f}},
        Transform{.position = ri::math::Vec3{-3.4f, 2.1f, 0.0f}, .rotationDegrees = ri::math::Vec3{90.0f, 90.0f, 0.0f}, .scale = ri::math::Vec3{5.0f, 1.0f, 2.4f}},
        Transform{.position = ri::math::Vec3{3.4f, 2.1f, 0.0f}, .rotationDegrees = ri::math::Vec3{90.0f, -90.0f, 0.0f}, .scale = ri::math::Vec3{5.0f, 1.0f, 2.4f}},
        Transform{.position = ri::math::Vec3{0.0f, 2.1f, 3.4f}, .rotationDegrees = ri::math::Vec3{90.0f, 180.0f, 0.0f}, .scale = ri::math::Vec3{5.0f, 1.0f, 2.4f}},
        Transform{.position = ri::math::Vec3{0.0f, 4.3f, 0.0f}, .rotationDegrees = ri::math::Vec3{180.0f, 0.0f, 0.0f}, .scale = ri::math::Vec3{5.0f, 1.0f, 5.0f}},
    }};

    for (std::size_t index = 0; index < wallTransforms.size(); ++index) {
        PrimitiveNodeOptions wall{};
        wall.nodeName = "EnvCard" + std::to_string(index + 1);
        wall.parent = rig.root;
        wall.primitive = PrimitiveType::Plane;
        wall.materialName = wall.nodeName + "Material";
        wall.shadingModel = ShadingModel::Unlit;
        wall.baseColor = wallColors[index];
        wall.transform = wallTransforms[index];
        (void)AddPrimitiveNode(rig.scene, wall);
    }

    PrimitiveNodeOptions hero{};
    hero.nodeName = "EnvHero";
    hero.parent = rig.root;
    hero.primitive = PrimitiveType::Cube;
    hero.materialName = "EnvHeroMaterial";
    hero.shadingModel = ShadingModel::Lit;
    hero.baseColor = ri::math::Vec3{0.82f, 0.84f, 0.86f};
    hero.transform.position = ri::math::Vec3{0.0f, 0.95f, 0.0f};
    hero.transform.rotationDegrees = ri::math::Vec3{0.0f, 34.0f, 0.0f};
    hero.transform.scale = ri::math::Vec3{1.2f, 1.2f, 1.2f};
    const int heroCube = AddPrimitiveNode(rig.scene, hero);

    LightNodeOptions rim{};
    rim.nodeName = "EnvRim";
    rim.parent = rig.root;
    rim.transform.position = ri::math::Vec3{1.8f, 2.8f, -1.4f};
    rim.light = Light{.name = "EnvRim", .type = LightType::Point, .color = ri::math::Vec3{1.0f, 0.96f, 0.84f}, .intensity = 5.8f, .range = 8.5f};
    (void)AddLightNode(rig.scene, rim);
    (void)FrameNodesWithOrbitCamera(rig.scene, rig.orbitCamera, {heroCube}, 1.55f);

    SceneKitMilestoneResult result = MakeResult(descriptor);
    result.scene = std::move(rig.scene);
    result.cameraNode = rig.orbitCamera.cameraNode;
    result.focusNode = heroCube;
    result.passed = FindNodeByName(result.scene, "EnvHero").has_value() &&
                    FindNodeByName(result.scene, "EnvCard5").has_value();
    result.detail = "environment cards=5 reflection-bay surrogate ready";
    FinalizeResult(result, callbacks);
    return result;
}

SceneKitMilestoneResult EvaluateAudioOrientation(const ExampleDescriptor& descriptor,
                                                 const SceneKitMilestoneCallbacks& callbacks) {
    DemoRig rig = BuildBaseDemoRig("SceneKitAudioOrientation", false, 11.0f);
    OrbitCameraState orbit = rig.orbitCamera.orbit;
    orbit.target = ri::math::Vec3{0.0f, 1.0f, 0.0f};
    orbit.distance = 9.4f;
    orbit.yawDegrees = 148.0f;
    orbit.pitchDegrees = -24.0f;
    SetOrbitCameraState(rig.scene, rig.orbitCamera, orbit);

    PrimitiveNodeOptions listener{};
    listener.nodeName = "AudioListener";
    listener.parent = rig.root;
    listener.primitive = PrimitiveType::Cube;
    listener.materialName = "AudioListenerMaterial";
    listener.shadingModel = ShadingModel::Unlit;
    listener.baseColor = ri::math::Vec3{0.24f, 0.92f, 0.82f};
    listener.transform.position = ri::math::Vec3{0.0f, 0.85f, 0.0f};
    listener.transform.scale = ri::math::Vec3{0.28f, 1.10f, 0.28f};
    const int listenerNode = AddPrimitiveNode(rig.scene, listener);

    const ri::math::Vec3 listenerPosition = rig.scene.GetNode(listenerNode).localTransform.position;
    const std::array<ri::math::Vec3, 2> speakerPositions = {{
        {-2.6f, 0.95f, 1.8f},
        {2.7f, 0.95f, -1.6f},
    }};
    const std::array<ri::math::Vec3, 2> speakerColors = {{
        {1.0f, 0.72f, 0.32f},
        {0.46f, 0.72f, 1.0f},
    }};

    std::array<float, 2> facingScores = {0.0f, 0.0f};
    for (std::size_t index = 0; index < speakerPositions.size(); ++index) {
        Transform speakerTransform{};
        speakerTransform.position = speakerPositions[index];
        speakerTransform.rotationDegrees = ri::math::Vec3{0.0f, ComputeSpeakerYawDegrees(speakerPositions[index], listenerPosition), 0.0f};
        const int speakerNode = AddCustomMeshNode(
            rig.scene,
            "AudioSource" + std::to_string(index + 1),
            rig.root,
            BuildPyramidMesh(),
            speakerTransform,
            "AudioSourceMaterial" + std::to_string(index + 1),
            speakerColors[index],
            ShadingModel::Lit);

        const ri::math::Vec3 toListener = ri::math::Normalize(listenerPosition - speakerPositions[index]);
        const float yawRadians = ri::math::DegreesToRadians(rig.scene.GetNode(speakerNode).localTransform.rotationDegrees.y);
        const ri::math::Vec3 forward = ri::math::Normalize(ri::math::Vec3{std::sin(yawRadians), 0.0f, std::cos(yawRadians)});
        facingScores[index] = ri::math::Dot(toListener, forward);
    }

    SceneKitMilestoneResult result = MakeResult(descriptor);
    result.scene = std::move(rig.scene);
    result.cameraNode = rig.orbitCamera.cameraNode;
    result.focusNode = listenerNode;
    result.passed = facingScores[0] > 0.95f && facingScores[1] > 0.95f;
    result.detail = "listener with 2 sources facing scores=" + std::to_string(static_cast<int>(facingScores[0] * 100.0f)) + "/" +
                    std::to_string(static_cast<int>(facingScores[1] * 100.0f));
    FinalizeResult(result, callbacks);
    return result;
}

std::optional<SceneKitMilestoneResult> EvaluateDescriptor(const ExampleDescriptor& descriptor,
                                                          const SceneKitMilestoneOptions& options,
                                                          const SceneKitMilestoneCallbacks& callbacks) {
    const std::string_view slug = descriptor.slug;
    if (slug == "scene_geometry_cube") return EvaluateGeometryCube(descriptor, callbacks);
    if (slug == "scene_controls_orbit") return EvaluateOrbitControls(descriptor, callbacks);
    if (slug == "scene_interactive_cubes") return EvaluateInteractiveCubes(descriptor, callbacks);
    if (slug == "scene_terrain_raycast") return EvaluateTerrainRaycast(descriptor, callbacks);
    if (slug == "scene_lighting_spotlights") return EvaluateSpotLights(descriptor, callbacks);
    if (slug == "scene_loader_gltf") return EvaluateGltfLoader(descriptor, options, callbacks);
    if (slug == "scene_animation_keyframes") return EvaluateAnimationKeyframes(descriptor, callbacks);
    if (slug == "scene_instancing_performance") return EvaluateInstancingPerformance(descriptor, callbacks);
    if (slug == "scene_materials_envmaps") return EvaluateEnvironmentMaps(descriptor, callbacks);
    if (slug == "scene_audio_orientation") return EvaluateAudioOrientation(descriptor, callbacks);
    return std::nullopt;
}

} // namespace

std::vector<SceneKitExampleDefinition> GetSceneKitExampleDefinitions() {
    std::vector<SceneKitExampleDefinition> definitions;
    definitions.reserve(kExampleDescriptors.size());
    for (const ExampleDescriptor& descriptor : kExampleDescriptors) {
        definitions.push_back(SceneKitExampleDefinition{
            .slug = descriptor.slug,
            .title = descriptor.title,
            .officialUrl = descriptor.officialUrl,
            .rawIronTrack = descriptor.rawIronTrack,
            .statusLabel = descriptor.statusLabel,
        });
    }
    return definitions;
}

SceneKitPreview BuildLitCubeSceneKitPreview() {
    if (const std::optional<SceneKitPreview> preview = BuildSceneKitPreview("scene_geometry_cube"); preview.has_value()) {
        return *preview;
    }
    return SceneKitPreview{};
}

std::optional<SceneKitPreview> BuildSceneKitPreview(std::string_view slug, const SceneKitMilestoneOptions& options) {
    const std::optional<SceneKitMilestoneResult> result = BuildSceneKitMilestone(slug, options);
    if (!result.has_value()) {
        return std::nullopt;
    }

    return SceneKitPreview{
        .slug = result->slug,
        .title = result->title,
        .officialUrl = result->officialUrl,
        .rawIronTrack = result->rawIronTrack,
        .statusLabel = result->statusLabel,
        .detail = result->detail,
        .scene = result->scene,
        .orbitCamera = OrbitCameraHandles{.cameraNode = result->cameraNode},
        .focusNode = result->focusNode,
    };
}

std::optional<SceneKitMilestoneResult> BuildSceneKitMilestone(std::string_view slug,
                                                              const SceneKitMilestoneOptions& options,
                                                              const SceneKitMilestoneCallbacks& callbacks) {
    const std::optional<ExampleDescriptor> descriptor = FindDescriptor(slug);
    if (!descriptor.has_value()) {
        return std::nullopt;
    }
    return EvaluateDescriptor(*descriptor, options, callbacks);
}

std::vector<SceneKitMilestoneResult> RunSceneKitMilestoneChecks(const SceneKitMilestoneOptions& options,
                                                                const SceneKitMilestoneCallbacks& callbacks) {
    std::vector<SceneKitMilestoneResult> results;
    results.reserve(kExampleDescriptors.size());
    for (const ExampleDescriptor& descriptor : kExampleDescriptors) {
        if (const std::optional<SceneKitMilestoneResult> result = EvaluateDescriptor(descriptor, options, callbacks);
            result.has_value()) {
            results.push_back(*result);
        }
    }
    return results;
}

bool AllSceneKitMilestonesPassed(const std::vector<SceneKitMilestoneResult>& results) {
    for (const SceneKitMilestoneResult& result : results) {
        if (!result.passed) {
            return false;
        }
    }
    return !results.empty();
}

} // namespace ri::scene
