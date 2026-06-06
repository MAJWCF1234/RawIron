#pragma once

#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/WorkspaceSandbox.h"
#include "RawIron/Trace/TraceScene.h"

#include <filesystem>
#include <string_view>
#include <vector>

namespace ri::games::multiplayersandbox {

struct World {
    ri::scene::Scene scene;
    ri::scene::StarterSceneHandles handles;
    int playerRig = ri::scene::kInvalidHandle;
    int playerCameraNode = ri::scene::kInvalidHandle;
    int catalogRoot = ri::scene::kInvalidHandle;
    int brushHallRoot = ri::scene::kInvalidHandle;
    int inspectionRig = ri::scene::kInvalidHandle;
    int inspectionCube = ri::scene::kInvalidHandle;
    ri::math::Vec3 catalogExtents{};
    std::vector<ri::trace::TraceCollider> colliders;
};

[[nodiscard]] World BuildWorld(std::string_view sceneName, const std::filesystem::path& gameRoot);
void AnimateWorld(World& world, double elapsedSeconds);

} // namespace ri::games::multiplayersandbox
