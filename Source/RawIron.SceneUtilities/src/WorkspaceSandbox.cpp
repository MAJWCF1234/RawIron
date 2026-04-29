#include "RawIron/Scene/WorkspaceSandbox.h"

#include "RawIron/Scene/Animation.h"
#include "RawIron/Scene/Helpers.h"

#include <cmath>

namespace ri::scene {

namespace {

AnimationClip BuildStarterSceneClip(const StarterSceneHandles& handles) {
    AnimationClip clip{};
    clip.name = "StarterSceneIdle";
    clip.durationSeconds = 4.0;
    clip.looping = true;
    clip.nodeTracks[handles.crate] = {
        TransformKeyframe{.timeSeconds = 0.0, .transform = Transform{.position = {0.0f, 0.5f, 0.0f}, .rotationDegrees = {0.0f, 0.0f, 0.0f}, .scale = {1.0f, 1.0f, 1.0f}}},
        TransformKeyframe{.timeSeconds = 2.0, .transform = Transform{.position = {0.0f, 0.5f, 0.0f}, .rotationDegrees = {0.0f, 110.0f, 0.0f}, .scale = {1.0f, 1.0f, 1.0f}}},
        TransformKeyframe{.timeSeconds = 4.0, .transform = Transform{.position = {0.0f, 0.5f, 0.0f}, .rotationDegrees = {0.0f, 220.0f, 0.0f}, .scale = {1.0f, 1.0f, 1.0f}}},
    };
    clip.nodeTracks[handles.beacon] = {
        TransformKeyframe{.timeSeconds = 0.0, .transform = Transform{.position = {0.0f, 1.25f, 0.0f}, .rotationDegrees = {0.0f, 0.0f, 0.0f}, .scale = {0.35f, 0.35f, 0.35f}}},
        TransformKeyframe{.timeSeconds = 1.0, .transform = Transform{.position = {0.0f, 1.45f, 0.0f}, .rotationDegrees = {0.0f, 90.0f, 0.0f}, .scale = {0.35f, 0.35f, 0.35f}}},
        TransformKeyframe{.timeSeconds = 2.0, .transform = Transform{.position = {0.0f, 1.25f, 0.0f}, .rotationDegrees = {0.0f, 180.0f, 0.0f}, .scale = {0.35f, 0.35f, 0.35f}}},
        TransformKeyframe{.timeSeconds = 3.0, .transform = Transform{.position = {0.0f, 1.05f, 0.0f}, .rotationDegrees = {0.0f, 270.0f, 0.0f}, .scale = {0.35f, 0.35f, 0.35f}}},
        TransformKeyframe{.timeSeconds = 4.0, .transform = Transform{.position = {0.0f, 1.25f, 0.0f}, .rotationDegrees = {0.0f, 360.0f, 0.0f}, .scale = {0.35f, 0.35f, 0.35f}}},
    };
    return clip;
}

} // namespace

StarterScene BuildStarterScene(const std::string_view sceneName) {
    StarterScene starterScene{Scene(std::string(sceneName)), {}};
    Scene& scene = starterScene.scene;

    starterScene.handles.root = scene.CreateNode("World");

    LightNodeOptions sun{};
    sun.nodeName = "Sun";
    sun.parent = starterScene.handles.root;
    sun.transform.rotationDegrees = ri::math::Vec3{-45.0f, 35.0f, 0.0f};
    sun.light = Light{
        .name = "Sun",
        .type = LightType::Directional,
        .color = ri::math::Vec3{1.00f, 0.95f, 0.90f},
        .intensity = 4.0f,
    };
    starterScene.handles.sun = AddLightNode(scene, sun);

    OrbitCameraOptions orbitCamera{};
    orbitCamera.parent = starterScene.handles.root;
    orbitCamera.camera = Camera{
        .name = "MainCamera",
        .projection = ProjectionType::Perspective,
        .fieldOfViewDegrees = 70.0f,
        .nearClip = 0.1f,
        .farClip = 500.0f,
    };
    orbitCamera.orbit = OrbitCameraState{
        .target = ri::math::Vec3{0.0f, 1.0f, 0.0f},
        .distance = 6.0f,
        .yawDegrees = 180.0f,
        .pitchDegrees = -10.0f,
    };
    starterScene.handles.orbitCamera = AddOrbitCamera(scene, orbitCamera);

    GridHelperOptions grid{};
    grid.parent = starterScene.handles.root;
    grid.nodeName = "Grid";
    grid.size = 14.0f;
    grid.transform.position = ri::math::Vec3{0.0f, -0.5f, 0.0f};
    starterScene.handles.grid = AddGridHelper(scene, grid);
    starterScene.handles.floor = starterScene.handles.grid;

    AxesHelperOptions axes{};
    axes.parent = starterScene.handles.root;
    axes.transform.position = ri::math::Vec3{0.0f, 0.0f, 0.0f};
    axes.axisLength = 1.5f;
    axes.axisThickness = 0.06f;
    starterScene.handles.axes = AddAxesHelper(scene, axes);

    PrimitiveNodeOptions crate{};
    crate.nodeName = "Crate";
    crate.parent = starterScene.handles.root;
    crate.primitive = PrimitiveType::Cube;
    crate.materialName = "crate";
    crate.baseColor = ri::math::Vec3{0.72f, 0.68f, 0.62f};
    crate.baseColorTexture = "ri_psx_wall_vent.png";
    crate.textureTiling = ri::math::Vec2{1.0f, 1.0f};
    crate.transform.position = ri::math::Vec3{0.0f, 0.5f, 0.0f};
    starterScene.handles.crate = AddPrimitiveNode(scene, crate);

    PrimitiveNodeOptions beacon{};
    beacon.nodeName = "Beacon";
    beacon.parent = starterScene.handles.crate;
    beacon.primitive = PrimitiveType::Sphere;
    beacon.materialName = "beacon";
    beacon.shadingModel = ShadingModel::Unlit;
    beacon.baseColor = ri::math::Vec3{1.00f, 0.65f, 0.15f};
    beacon.transform.position = ri::math::Vec3{0.0f, 1.25f, 0.0f};
    beacon.transform.scale = ri::math::Vec3{0.35f, 0.35f, 0.35f};
    starterScene.handles.beacon = AddPrimitiveNode(scene, beacon);

    LightNodeOptions beaconLight{};
    beaconLight.nodeName = "BeaconGlow";
    beaconLight.parent = starterScene.handles.beacon;
    beaconLight.light = Light{
        .name = "BeaconGlow",
        .type = LightType::Point,
        .color = ri::math::Vec3{1.00f, 0.70f, 0.25f},
        .intensity = 12.0f,
        .range = 6.0f,
    };
    (void)AddLightNode(scene, beaconLight);

    return starterScene;
}

void AnimateStarterSceneProps(StarterScene& starterScene, const double elapsedSeconds) {
    Scene& scene = starterScene.scene;
    const AnimationClip clip = BuildStarterSceneClip(starterScene.handles);
    ApplyAnimationClip(scene, clip, elapsedSeconds);
}

void AnimateStarterSceneOrbitPreview(StarterScene& starterScene, const double elapsedSeconds) {
    Scene& scene = starterScene.scene;
    OrbitCameraState orbitState = starterScene.handles.orbitCamera.orbit;
    orbitState.yawDegrees = 180.0f + static_cast<float>(std::sin(elapsedSeconds * 0.45) * 25.0);
    orbitState.distance = 6.0f + static_cast<float>(std::sin(elapsedSeconds * 0.55) * 0.35);
    orbitState.target.x = static_cast<float>(std::sin(elapsedSeconds * 0.75) * 1.2);
    SetOrbitCameraState(scene, starterScene.handles.orbitCamera, orbitState);
}

void AnimateStarterScene(StarterScene& starterScene, const double elapsedSeconds) {
    AnimateStarterSceneProps(starterScene, elapsedSeconds);
    AnimateStarterSceneOrbitPreview(starterScene, elapsedSeconds);
}

} // namespace ri::scene
