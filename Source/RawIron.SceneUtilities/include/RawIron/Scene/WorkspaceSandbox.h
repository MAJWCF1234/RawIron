#pragma once

#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/Scene.h"

#include <string_view>

namespace ri::scene {

/// Default editor / tooling sandbox scene (grid, orbit camera, sample primitives). Not game content.
struct StarterSceneHandles {
    int root = kInvalidHandle;
    int sun = kInvalidHandle;
    int grid = kInvalidHandle;
    int floor = kInvalidHandle;
    int crate = kInvalidHandle;
    int beacon = kInvalidHandle;
    AxesHelperHandles axes{};
    OrbitCameraHandles orbitCamera{};
};

struct StarterScene {
    Scene scene;
    StarterSceneHandles handles;
};

[[nodiscard]] StarterScene BuildStarterScene(std::string_view sceneName);
/// Animates authored props (crate / beacon clips) without moving the orbit camera rig.
void AnimateStarterSceneProps(StarterScene& starterScene, double elapsedSeconds);
/// Idle orbit-camera motion used when no scripted camera review is driving the rig.
void AnimateStarterSceneOrbitPreview(StarterScene& starterScene, double elapsedSeconds);
void AnimateStarterScene(StarterScene& starterScene, double elapsedSeconds);

} // namespace ri::scene
