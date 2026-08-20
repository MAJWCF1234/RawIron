#pragma once

#include "RawIron/Scene/Scene.h"
#include "RawIron/Trace/TraceScene.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ri::games::cubetest {

struct CubeTestWorld {
    ri::scene::Scene scene;
    int rootNode = ri::scene::kInvalidHandle;
    int platformNode = ri::scene::kInvalidHandle;
    int cubeNode = ri::scene::kInvalidHandle;
    int goldSampleNode = ri::scene::kInvalidHandle;
    int copperSampleNode = ri::scene::kInvalidHandle;
    int ironSampleNode = ri::scene::kInvalidHandle;
    int crystalSampleNode = ri::scene::kInvalidHandle;
    int portalBrushNode = ri::scene::kInvalidHandle;
    int playerRig = ri::scene::kInvalidHandle;
    int playerCameraNode = ri::scene::kInvalidHandle;
    std::vector<ri::trace::TraceCollider> colliders;
};

[[nodiscard]] CubeTestWorld BuildCubeTestWorld(
    std::string_view sceneName = "Cube Test",
    const std::filesystem::path& workspaceRoot = {});
void AnimateCubeTestWorld(CubeTestWorld& world, double elapsedSeconds);
void AnimateCubeTestWorldJiggle(CubeTestWorld& world, double elapsedSeconds);
void ConfigureCookedTextureCube(CubeTestWorld& world,
                                std::vector<std::string> logicalTexturePaths,
                                float framesPerSecond = 0.75f);
[[nodiscard]] const std::vector<std::string>& CubeTestCookedTextureSequence();

} // namespace ri::games::cubetest
