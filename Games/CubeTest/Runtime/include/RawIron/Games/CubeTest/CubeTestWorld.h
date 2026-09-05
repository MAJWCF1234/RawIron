#pragma once

#include "RawIron/Scene/Scene.h"
#include "RawIron/Trace/TraceScene.h"
#include "RawIron/World/PortalTravel.h"
#include "RawIron/World/InteractivePropField.h"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ri::games::cubetest {

struct CubeTestWorld {
    ri::scene::Scene scene;
    bool materialCalibration = false;
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
    int spriteNode = ri::scene::kInvalidHandle;
    int shaderBallNode = ri::scene::kInvalidHandle;
    int coffeeModelNode = ri::scene::kInvalidHandle;
    int exporterInstanceBatch = ri::scene::kInvalidHandle;
    int interactionRoomRoot = ri::scene::kInvalidHandle;
    int projectileRoomRoot = ri::scene::kInvalidHandle;
    int teleportRoomRoot = ri::scene::kInvalidHandle;
    std::vector<int> spriteFrameMeshes{};
    std::array<std::vector<ri::math::Vec3>, 4> spriteLayouts{};
    std::vector<ri::trace::TraceCollider> colliders;
    std::vector<ri::world::PortalTravelVolume> portals;
    std::vector<int> interactionPropNodes{};
    std::vector<ri::world::InteractivePropState> interactionProps{};
    ri::world::InteractivePropFieldOptions interactionField{};
    double interactionSimulationTime = 0.0;
    std::vector<int> projectilePropNodes{};
    std::vector<ri::world::InteractivePropState> projectileProps{};
    ri::world::InteractivePropFieldOptions projectileField{};
    double projectileSimulationTime = 0.0;
};

[[nodiscard]] CubeTestWorld BuildCubeTestWorld(
    std::string_view sceneName = "Cube Test",
    const std::filesystem::path& workspaceRoot = {});
// Isolated static renderer fixture; no gallery assets, portals, or dynamic props.
[[nodiscard]] CubeTestWorld BuildCubeTestCalibrationWorld(const std::filesystem::path& workspaceRoot, bool normalComparison = false);
void AnimateCubeTestWorld(CubeTestWorld& world, double elapsedSeconds, bool simulateDynamicProps = true);
void AnimateCubeTestWorldJiggle(CubeTestWorld& world, double elapsedSeconds);
void ConfigureCookedTextureCube(CubeTestWorld& world,
                                std::vector<std::string> logicalTexturePaths,
                                float framesPerSecond = 0.75f);
[[nodiscard]] const std::vector<std::string>& CubeTestCookedTextureSequence();
[[nodiscard]] ri::world::InteractivePropEmissionResult EmitCubeTestProjectile(
    CubeTestWorld& world,
    const ri::math::Vec3& origin,
    const ri::math::Vec3& direction);

} // namespace ri::games::cubetest
