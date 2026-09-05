#include "RawIron/Core/Log.h"
#include "RawIron/Core/CommandLine.h"
#include "RawIron/Games/CubeTest/CubeTestAuthority.h"
#include "RawIron/Games/CubeTest/CubeTestWorld.h"
#include "RawIron/Games/CubeTest/CubeTestGallery.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Render/PreviewTexture.h"
#include "RawIron/Trace/MovementController.h"
#include "RawIron/Trace/TraceScene.h"
#include "RawIron/Trace/TeleportTargeting.h"
#include "RawIron/Runtime/RuntimeCore.h"
#include "RawIron/World/PortalTravel.h"
#include "RawIron/World/InteractivePropGrab.h"
#include "RawIron/XR/OpenXrRuntime.h"
#include "RawIron/XR/HardwareSceneBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {
using ri::xr::HardwareTextureAtlas;
using ri::xr::BuildHardwareTextureAtlas;
using ri::xr::BuildHardwareScene;
using ri::xr::AppendHardwareMesh;

ri::spatial::Aabb BuildVrPlayerBounds(const ri::math::Vec3& feet) {
    return {
        .min = {feet.x - 0.25f, feet.y, feet.z - 0.25f},
        .max = {feet.x + 0.25f, feet.y + 1.8f, feet.z + 0.25f}};
}

ri::math::Vec3 VrFeetFromBounds(const ri::spatial::Aabb& bounds) {
    return {
        (bounds.min.x + bounds.max.x) * 0.5f,
        bounds.min.y,
        (bounds.min.z + bounds.max.z) * 0.5f};
}

struct VrLocomotionState {
    const ri::games::cubetest::CubeTestWorld* world = nullptr;
    ri::trace::TraceScene traceScene{};
    ri::trace::MovementControllerState movement{};
    ri::trace::MovementControllerOptions options{};
    ri::world::PortalTravelerState portalTraveler{};
};

void ResolveVrLocomotion(void* user,
                         const ri::xr::HardwareLocomotionInput& input,
                         float origin[3],
                         float& yawDegrees) {
    auto& state = *static_cast<VrLocomotionState*>(user);
    const ri::trace::MovementInput movementInput{
        .moveForward = input.moveForward,
        .moveRight = input.moveRight,
        .viewForwardWorld = {input.viewForward[0], input.viewForward[1], input.viewForward[2]},
        .viewRightWorld = {input.viewRight[0], input.viewRight[1], input.viewRight[2]},
        .jumpPressed = input.jumpPressed};
    state.movement = ri::trace::SimulateMovementControllerStep(
                         state.traceScene,
                         state.movement,
                         movementInput,
                         input.deltaSeconds,
                         state.options)
                         .state;
    const ri::world::PortalTravelResult portal = ri::world::UpdatePortalTraveler(
        state.world->portals,
        state.movement.body.bounds,
        input.deltaSeconds,
        state.portalTraveler);
    if (portal.traveled) {
        state.movement.body.bounds = BuildVrPlayerBounds(portal.destinationFeet);
        if (!portal.preserveVelocity) state.movement.body.velocity = {};
        state.movement.onGround = true;
        yawDegrees = portal.destinationYawDegrees;
    }
    const ri::math::Vec3 feet = VrFeetFromBounds(state.movement.body.bounds);
    origin[0] = feet.x;
    origin[1] = feet.y;
    origin[2] = feet.z;
}

struct VrInteractionState {
    ri::games::cubetest::CubeTestWorld* world = nullptr;
    const HardwareTextureAtlas* atlas = nullptr;
    std::vector<ri::xr::HardwareSceneVertex> vertices{};
    std::array<ri::world::InteractivePropGrab, 2> grabs{};
    VrLocomotionState* locomotion = nullptr;
    std::array<ri::trace::TeleportTargetingResult, 2> teleportTargets{};
    ri::runtime::RuntimeCore* authorityRuntime = nullptr;
    ri::runtime::AuthoritativeNetModule* netcode = nullptr;
    int authorityFrameIndex = 0;
    double authorityElapsedSeconds = 0.0;
};

[[nodiscard]] bool IsRemoteAuthorityClient(const VrInteractionState& state) {
    return state.netcode != nullptr && state.netcode->Config().role == ri::runtime::NetRole::Client;
}

void AppendTeleportArc(std::vector<ri::xr::HardwareSceneVertex>& output,
                       const std::vector<ri::math::Vec3>& points,
                       const ri::math::Vec3& color) {
    constexpr float halfWidth = 0.018f;
    for (std::size_t index = 1; index < points.size(); ++index) {
        const ri::math::Vec3 delta = points[index] - points[index - 1U];
        ri::math::Vec3 side = ri::math::Cross(delta, {0.0f, 1.0f, 0.0f});
        if (ri::math::LengthSquared(side) <= 1.0e-8f) side = {1.0f, 0.0f, 0.0f};
        side = ri::math::Normalize(side) * halfWidth;
        const ri::math::Vec3 corners[]{
            points[index - 1U] - side,
            points[index - 1U] + side,
            points[index] + side,
            points[index] - side};
        constexpr int triangles[]{0, 1, 2, 0, 2, 3};
        for (const int corner : triangles) {
            output.push_back({
                {corners[corner].x, corners[corner].y, corners[corner].z},
                {color.x, color.y, color.z}});
        }
    }
}

ri::xr::HardwareInteractionFrameOutput UpdateVrInteraction(
    void* user,
    const ri::xr::HardwareInteractionFrameInput& input) {
    auto& state = *static_cast<VrInteractionState*>(user);
    ri::xr::HardwareInteractionFrameOutput output{};
    if (state.world == nullptr || state.atlas == nullptr) return output;

    const float deltaSeconds = std::clamp(input.deltaSeconds, 0.0f, 0.05f);
    if (state.authorityRuntime != nullptr) {
        state.authorityElapsedSeconds += static_cast<double>(deltaSeconds);
        const ri::core::FrameContext frame{
            .frameIndex = state.authorityFrameIndex++,
            .deltaSeconds = static_cast<double>(deltaSeconds),
            .elapsedSeconds = state.authorityElapsedSeconds,
            .realtimeSeconds = state.authorityElapsedSeconds,
            .realDeltaSeconds = static_cast<double>(deltaSeconds),
        };
        if (!state.authorityRuntime->Frame(frame)) {
            ri::core::LogInfo("VR authority runtime frame failed: "
                              + std::string(state.authorityRuntime->Context().FailureReason()));
            state.authorityRuntime = nullptr;
        }
    }
    for (std::size_t handIndex = 0; handIndex < std::size(input.hands); ++handIndex) {
        const ri::xr::HardwareInteractionHandInput& hand = input.hands[handIndex];
        const ri::math::Vec3 rayOrigin{
            hand.aimOrigin[0], hand.aimOrigin[1], hand.aimOrigin[2]};
        const ri::math::Vec3 rayDirection{
            hand.aimDirection[0], hand.aimDirection[1], hand.aimDirection[2]};
        const std::uint32_t owner = static_cast<std::uint32_t>(handIndex + 1U);
        const ri::world::InteractivePropSelection hover = hand.tracked
            ? ri::world::SelectInteractiveProp(state.world->interactionProps, rayOrigin, rayDirection, 4.5f)
            : ri::world::InteractivePropSelection{};
        // Explicit selection is a qualified event; merely hovering is never haptic input.
        if (hand.tracked && hand.selectPressed && hover.propIndex >= 0) {
            output.hapticEvent[handIndex] = ri::xr::HapticEvent::Selection;
            output.hapticAmplitude[handIndex] = 0.12f;
            output.hapticDurationSeconds[handIndex] = 0.012f;
        }
        if (hand.tracked && hand.teleportHeld && ri::games::cubetest::CubeTestRoomAt(rayOrigin.x).id == "teleport"
            && state.locomotion != nullptr) {
            state.teleportTargets[handIndex] = ri::trace::ResolveTeleportTarget(
                state.locomotion->traceScene, rayOrigin, rayDirection);
        }
        if (hand.tracked && hand.teleportReleased && state.teleportTargets[handIndex].validLanding
            && state.locomotion != nullptr) {
            const ri::math::Vec3 destination =
                state.teleportTargets[handIndex].destinationFeet;
            state.locomotion->movement.body.bounds = BuildVrPlayerBounds(destination);
            state.locomotion->movement.body.velocity = {};
            state.locomotion->movement.onGround = true;
            output.teleportRequested = true;
            output.teleportDestinationFeet[0] = destination.x;
            output.teleportDestinationFeet[1] = destination.y;
            output.teleportDestinationFeet[2] = destination.z;
            state.teleportTargets[handIndex] = {};
        } else if (!hand.tracked || (!hand.teleportHeld && !hand.teleportReleased)) {
            state.teleportTargets[handIndex] = {};
        }
        if (hand.tracked && hand.selectPressed) {
            bool fired = false;
            if (IsRemoteAuthorityClient(state)) {
                ri::runtime::NetPacket packet{};
                packet.channel = 0U;
                packet.reliable = true;
                packet.payload = ri::games::cubetest::CubeTestAuthorityBridge::BuildProjectileCommand(
                    rayOrigin + rayDirection * 1.4f, rayDirection);
                fired = state.netcode->SendPacket(0U, packet, ri::runtime::NetChannelKind::Authority);
            } else {
                fired = ri::games::cubetest::EmitCubeTestProjectile(
                    *state.world,
                    rayOrigin + rayDirection * 1.4f,
                    rayDirection).propIndex >= 0;
            }
            static_cast<void>(fired);
        }
        auto& grab = state.grabs[handIndex];
        if (!IsRemoteAuthorityClient(state) && hand.tracked && hand.grabPressed
            && ri::world::BeginRayPropGrab(grab, state.world->interactionProps, owner, rayOrigin, rayDirection)) {
            output.hapticEvent[handIndex] = ri::xr::HapticEvent::Grab;
            output.hapticAmplitude[handIndex] = 0.18f;
            output.hapticDurationSeconds[handIndex] = 0.02f;
        }
        if (grab.propIndex >= 0) {
            if (!hand.tracked || IsRemoteAuthorityClient(state)) {
                ri::world::ReleaseRayPropGrab(grab, state.world->interactionProps, false);
            } else {
                (void)ri::world::UpdateRayPropGrab(grab, state.world->interactionProps, rayOrigin, rayDirection, deltaSeconds);
                if (hand.grabReleased || !hand.grabHeld)
                    ri::world::ReleaseRayPropGrab(grab, state.world->interactionProps);
            }
        }
    }

    if (!IsRemoteAuthorityClient(state)) {
        (void)ri::world::StepInteractivePropField(state.world->interactionProps, deltaSeconds, state.world->interactionField);
        (void)ri::world::StepInteractivePropField(state.world->projectileProps, deltaSeconds, state.world->projectileField);
    }
    state.vertices.clear();
    const auto appendField = [&](const std::vector<ri::world::InteractivePropState>& props,
                                 const std::vector<int>& nodes) {
        for (std::size_t index = 0; index < props.size(); ++index) {
            const ri::world::InteractivePropState& prop = props[index];
            if (!prop.active) continue;
            ri::scene::Node& node = state.world->scene.GetNode(nodes[index]);
            node.localTransform.position = prop.position;
            node.localTransform.scale = prop.halfExtents * 2.0f;
            node.localTransform.rotationDegrees = node.localTransform.rotationDegrees
                + prop.angularVelocityDegrees * deltaSeconds;
            AppendHardwareMesh(
                state.vertices,
                state.world->scene.GetMesh(node.mesh),
                state.world->scene.GetMaterial(node.material),
                state.world->scene.ComputeWorldMatrix(nodes[index]),
                *state.atlas);
        }
    };
    appendField(state.world->interactionProps, state.world->interactionPropNodes);
    appendField(state.world->projectileProps, state.world->projectilePropNodes);
    for (std::size_t handIndex = 0; handIndex < std::size(input.hands); ++handIndex) {
        if (!input.hands[handIndex].teleportHeld) continue;
        const ri::trace::TeleportTargetingResult& teleport = state.teleportTargets[handIndex];
        AppendTeleportArc(
            state.vertices,
            teleport.arcPoints,
            teleport.validLanding
                ? ri::math::Vec3{0.18f, 1.0f, 0.48f}
                : ri::math::Vec3{1.0f, 0.16f, 0.12f});
    }
    output.vertices = state.vertices.data();
    output.vertexCount = state.vertices.size();
    return output;
}

bool ConfigureProcessRuntime(const ri::core::CommandLine& commandLine, std::string& error) {
    std::filesystem::path runtimeManifest{};
    if (const auto explicitRuntime = commandLine.GetValue("--runtime-json"); explicitRuntime.has_value()) {
        runtimeManifest = *explicitRuntime;
    }
#if defined(_WIN32)
    if (runtimeManifest.empty() && commandLine.HasFlag("--steamvr")) {
        wchar_t value[32768]{};
        DWORD valueBytes = sizeof(value);
        const LSTATUS status = RegGetValueW(
            HKEY_CURRENT_USER,
            L"SOFTWARE\\Khronos\\OpenXR\\1",
            L"ActiveRuntime",
            RRF_RT_REG_SZ,
            nullptr,
            value,
            &valueBytes);
        if (status == ERROR_SUCCESS) {
            runtimeManifest = std::filesystem::path(value);
        }
    }
#endif
    if (runtimeManifest.empty()) {
        if (commandLine.HasFlag("--steamvr")) {
            error = "--steamvr could not find a per-user SteamVR OpenXR runtime manifest.";
            return false;
        }
        return true;
    }
    if (!std::filesystem::is_regular_file(runtimeManifest)) {
        error = "OpenXR runtime manifest does not exist: " + runtimeManifest.string();
        return false;
    }
#if defined(_WIN32)
    if (!SetEnvironmentVariableW(L"XR_RUNTIME_JSON", runtimeManifest.c_str())) {
        error = "Could not apply the process-local OpenXR runtime manifest.";
        return false;
    }
#else
    error = "--runtime-json process override is not implemented on this platform.";
    return false;
#endif
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const ri::core::CommandLine commandLine(argc, argv);
    if (commandLine.HasFlag("--gallery-help")) {
        ri::core::LogInfo(ri::games::cubetest::CubeTestGalleryHelp());
        return 0;
    }
    if (commandLine.HasFlag("--help") || commandLine.HasFlag("-h")) {
        ri::core::LogInfo("RawIron.VRShowcase options:");
        ri::core::LogInfo("  --steamvr              Use the per-user SteamVR OpenXR manifest for this process only");
        ri::core::LogInfo("  --runtime-json=<path>  Use an explicit OpenXR runtime manifest for this process only");
        ri::core::LogInfo("  --frames=<n>           Submit a bounded headset test (default: 300)");
        ri::core::LogInfo("  --start-room=<name>    choose a room listed by --gallery-help");
        ri::core::LogInfo("  --net-mode=<mode>      offline (default), listen, dedicated, or client");
        ri::core::LogInfo("  --port=<n> --connect-host=<host> --connect-port=<n>  Authority session endpoint");
        ri::core::LogInfo("  --probe-only           Discover runtime/system/actions without starting a session");
        return 0;
    }
    std::string error;
    if (!ConfigureProcessRuntime(commandLine, error)) {
        ri::core::LogInfo(error);
        return 1;
    }
    // Build through the same game/world API as desktop. XR is a host and input/view mode,
    // never a forked copy of the showcase content or structural primitive graph.
    ri::games::cubetest::CubeTestWorld world =
        ri::games::cubetest::BuildCubeTestWorld("Raw Iron VR Capability Showcase");
    auto authorityBridge = std::make_shared<ri::games::cubetest::CubeTestAuthorityBridge>(&world);
    ri::runtime::RuntimeCore authorityRuntime(
        {.id = "rawiron.vrshowcase", .displayName = "Raw Iron VR Showcase", .mode = "xr"},
        ri::runtime::DetectRuntimePaths(std::filesystem::current_path()));
    ri::runtime::AuthoritativeNetConfig authorityConfig =
        ri::games::cubetest::BuildCubeTestAuthorityConfig(commandLine, authorityBridge);
    auto authorityModule = std::make_unique<ri::runtime::AuthoritativeNetModule>(authorityConfig);
    ri::runtime::AuthoritativeNetModule* const authorityNetcode = authorityModule.get();
    authorityRuntime.AddModule(std::move(authorityModule));
    if (!authorityRuntime.Startup(commandLine)) {
        ri::core::LogInfo("VR authority runtime startup failed: "
                          + std::string(authorityRuntime.Context().FailureReason()));
        return 1;
    }
    ri::math::Vec3 vrSpawnFeet{0.0f, 0.02f, -7.4f};
    if (const auto startRoom = commandLine.GetValue("--start-room"); startRoom.has_value()) {
        const auto* room = ri::games::cubetest::FindCubeTestRoom(*startRoom);
        if (!room) {
            ri::core::LogInfo("Unknown start room; use --gallery-help for available IDs.");
            authorityRuntime.Shutdown();
            return 1;
        }
        if (room->id != "baseline") vrSpawnFeet = ri::games::cubetest::CubeTestRoomArrival(*room);
    }
    const HardwareTextureAtlas hardwareAtlas = BuildHardwareTextureAtlas(world.scene);
    if (!hardwareAtlas.errors.empty()) {
        for (const auto& diagnostic : hardwareAtlas.errors) ri::core::LogInfo(diagnostic);
        authorityRuntime.Shutdown();
        return 1;
    }
    std::vector<int> dynamicNodes = world.interactionPropNodes;
    dynamicNodes.insert(
        dynamicNodes.end(), world.projectilePropNodes.begin(), world.projectilePropNodes.end());
    const std::vector<ri::xr::HardwareSceneVertex> hardwareVertices =
        BuildHardwareScene(world.scene, hardwareAtlas, dynamicNodes);
    VrInteractionState interaction{};
    interaction.world = &world;
    interaction.atlas = &hardwareAtlas;
    interaction.vertices.reserve(
        (world.interactionProps.size() + world.projectileProps.size()) * 36U + 512U);
    VrLocomotionState locomotion{};
    locomotion.world = &world;
    locomotion.traceScene = ri::trace::TraceScene(world.colliders);
    locomotion.movement.body.bounds = BuildVrPlayerBounds(vrSpawnFeet);
    locomotion.movement.onGround = true;
    locomotion.options.simulateStamina = false;
    locomotion.options.maxGroundSpeed = 3.2f;
    locomotion.options.maxAirSpeed = 3.2f;
    locomotion.options.groundAcceleration = 45.0f;
    locomotion.options.airAcceleration = 16.0f;
    interaction.locomotion = &locomotion;
    interaction.authorityRuntime = &authorityRuntime;
    interaction.netcode = authorityNetcode;

    ri::xr::OpenXrRuntime runtime;
    if (!runtime.Initialize("Raw Iron VR Showcase", error)) {
        ri::core::LogSection("Raw Iron OpenXR / SteamVR");
        ri::core::LogInfo(error);
        ri::core::LogInfo("The shared showcase world still validated with "
                          + std::to_string(world.scene.NodeCount()) + " nodes.");
        return 2;
    }

    const ri::xr::RuntimeInfo& info = runtime.Info();
    ri::core::LogSection("Raw Iron OpenXR / SteamVR");
    ri::core::LogInfo("Runtime: " + info.runtimeName + " | HMD: " + info.systemName);
    ri::core::LogInfo("Stereo views: " + std::to_string(info.stereoViews.size())
                      + " | Shared scene nodes: " + std::to_string(world.scene.NodeCount()));
    for (const std::string& warning : info.warnings) {
        ri::core::LogInfo("OpenXR binding warning: " + warning);
    }
    const std::size_t initializationWarningCount = info.warnings.size();
    if (commandLine.HasFlag("--probe-only")) {
        ri::core::LogInfo("OpenXR system, Vulkan requirements, and Raw Iron action schema are ready.");
        runtime.PollEvents();
        authorityRuntime.Shutdown();
        return 0;
    }

    const int requestedFrames = std::clamp(commandLine.GetIntOr("--frames", 300), 1, 36000);
    ri::xr::OpenXrVulkanSession session(runtime);
    ri::xr::VulkanSessionRunReport report{};
    const ri::xr::HardwareSceneView hardwareScene{
        hardwareVertices.data(),
        hardwareVertices.size(),
        {vrSpawnFeet.x, vrSpawnFeet.y, vrSpawnFeet.z},
        0.05f,
        500.0f,
        hardwareAtlas.rgba.data(),
        HardwareTextureAtlas::kSize,
        HardwareTextureAtlas::kSize,
        &locomotion,
        &ResolveVrLocomotion,
        (world.interactionProps.size() + world.projectileProps.size()) * 36U + 512U,
        &interaction,
        &UpdateVrInteraction,
        ri::xr::HardwareTurnMode::Smooth,
        120.0f,
        30.0f};
    ri::core::LogInfo("Hardware Cube Test triangles: "
                      + std::to_string(hardwareVertices.size() / 3U)
                      + " | textures=" + std::to_string(hardwareAtlas.loadedTextures));
    if (!session.RunFrames(
            static_cast<std::uint32_t>(requestedFrames), report, error, &hardwareScene)) {
        ri::core::LogInfo("OpenXR Vulkan session failed: " + error);
        authorityRuntime.Shutdown();
        return 3;
    }
    for (std::size_t index = initializationWarningCount; index < info.warnings.size(); ++index) {
        ri::core::LogInfo("OpenXR session warning: " + info.warnings[index]);
    }
    ri::core::LogInfo(
        "OpenXR Vulkan frames: submitted=" + std::to_string(report.submittedFrames)
        + " stereoValid=" + std::to_string(report.validStereoFrames)
        + " leftTracked=" + std::to_string(report.leftControllerTrackedFrames)
        + " rightTracked=" + std::to_string(report.rightControllerTrackedFrames)
        + " focused=" + std::to_string(report.focusedFrames)
        + " leftActionActive=" + std::to_string(report.leftPoseActionActiveFrames)
        + " rightActionActive=" + std::to_string(report.rightPoseActionActiveFrames)
        + " aimSources=" + std::to_string(report.aimPoseBoundSourceCount)
        + " leftHandJoints=" + std::to_string(report.leftArticulatedHandFrames)
        + " rightHandJoints=" + std::to_string(report.rightArticulatedHandFrames)
        + " locomotion=" + std::to_string(report.locomotionInputFrames)
        + " snapTurns=" + std::to_string(report.snapTurnCount)
        + " selects=" + std::to_string(report.selectPressCount)
        + " dynamic=" + std::to_string(report.dynamicSceneFrames)
        + " haptics=" + std::to_string(report.hapticPulseCount)
        + " jumps=" + std::to_string(report.jumpPressCount)
        + " teleports=" + std::to_string(report.teleportCount)
        + " swapchain=" + std::to_string(report.width) + "x" + std::to_string(report.height));
    ri::core::LogInfo(
        "OpenXR interaction profiles: left="
        + (report.leftInteractionProfile.empty() ? std::string("unbound") : report.leftInteractionProfile)
        + " right="
        + (report.rightInteractionProfile.empty() ? std::string("unbound") : report.rightInteractionProfile));
    authorityRuntime.Shutdown();
    return 0;
}
