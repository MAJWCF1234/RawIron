#include "RawIron/Games/MultiplayerSandbox/MultiplayerSandboxRuntime.h"
#include "RawIron/Games/MultiplayerSandbox/MultiplayerSandboxWorld.h"

#include "RawIron/Content/EngineAssets.h"
#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/GameRuntimeSupport.h"
#include "RawIron/Content/ScriptScalars.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Games/GameConfigContracts.h"
#include "RawIron/Games/GameRuntimeCore.h"
#include "RawIron/Render/ShaderConfig.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Render/SoftwarePreview.h"
#include "RawIron/Render/VulkanPreviewPresenter.h"
#include "RawIron/Runtime/BotClients.h"
#include "RawIron/Runtime/RuntimeCore.h"
#include "RawIron/Runtime/RuntimeNetcode.h"
#include "RawIron/Scene/SceneStructuralTraceFeed.h"
#include "RawIron/Spatial/Aabb.h"
#include "RawIron/Trace/MovementController.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <cctype>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace ri::games::multiplayersandbox {

#if defined(_WIN32)
namespace {

namespace fs = std::filesystem;

struct NativeRenderTuning {
    int qualityTier = 1;
    float exposure = 1.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;
    float fogDensity = 0.0035f;
};

struct RuntimeState {
    HWND hwnd = nullptr;
    bool mouseCaptured = false;
    float rawMouseAccumX = 0.0f;
    float rawMouseAccumY = 0.0f;
    bool captureCursorHidden = false;
    bool captureMouse = true;
    float yawDegrees = 55.0f;
    float pitchDegrees = -5.0f;
    float elapsedSeconds = 0.0f;
    float lastSimulationDeltaSeconds = 1.0f / 60.0f;
    World world{};
    ri::trace::TraceScene traceScene{};
    ri::trace::MovementControllerState movement{};
    ri::trace::MovementControllerOptions movementOptions{};
    ri::render::software::ScenePreviewOptions previewOptions{};
    NativeRenderTuning nativeRenderTuning{};
    std::chrono::steady_clock::time_point lastTick = std::chrono::steady_clock::now();
    bool jumpHeldLastFrame = false;
    float mouseSensitivityDegreesPerPixel = 0.10f;
    float cameraBaseHeight = 1.62f;
    float bobAmplitude = 0.006f;
    float bobFrequencyHz = 1.25f;
    float bobSprintScale = 1.25f;
    float fovBaseDegrees = 78.0f;
    float fovSprintAddDegrees = 4.0f;
    float fovLerpPerSecond = 10.0f;
    float currentFovDegrees = 78.0f;
    fs::path nativeSkyEquirectRelative{};
    ri::render::ShaderPresentationConfig shaderPresentation{};
};

ri::runtime::RendezvousProviderKind ParseRendezvous(const std::string& v) {
    if (v == "direct") return ri::runtime::RendezvousProviderKind::DirectToken;
    if (v == "eos") return ri::runtime::RendezvousProviderKind::EpicOnlineServices;
    return ri::runtime::RendezvousProviderKind::None;
}

ri::runtime::NetMode ParseNetMode(const std::string& v) {
    if (v == "dedicated") return ri::runtime::NetMode::Dedicated;
    if (v == "listen") return ri::runtime::NetMode::ListenHost;
    if (v == "hybrid") return ri::runtime::NetMode::HybridP2P;
    return ri::runtime::NetMode::ClientOnly;
}

std::optional<ri::content::GameManifest> ResolveStandaloneGameManifest(const StandaloneOptions& options) {
    if (!options.gameRoot.empty()) {
        return ri::content::LoadGameManifest(options.gameRoot / "manifest.json");
    }
    const fs::path workspaceRoot =
        options.workspaceRoot.empty() ? ri::content::DetectWorkspaceRoot(fs::current_path()) : options.workspaceRoot;
    return ri::content::ResolveGameManifest(workspaceRoot, options.gameId);
}

fs::path ResolvePreviewOutputPath(const ri::core::CommandLine& commandLine) {
    if (const std::optional<std::string> output = commandLine.GetValue("--output"); output.has_value() && !output->empty()) {
        return fs::path(*output);
    }
    return fs::current_path() / "rawiron_multiplayer_sandbox_preview.bmp";
}

ri::spatial::Aabb BuildPlayerBounds(const ri::math::Vec3& feet) {
    return ri::spatial::Aabb{
        .min = ri::math::Vec3{feet.x - 0.25f, feet.y, feet.z - 0.25f},
        .max = ri::math::Vec3{feet.x + 0.25f, feet.y + 1.8f, feet.z + 0.25f},
    };
}

ri::math::Vec3 FeetFromBounds(const ri::spatial::Aabb& bounds) {
    return ri::math::Vec3{
        (bounds.min.x + bounds.max.x) * 0.5f,
        bounds.min.y,
        (bounds.min.z + bounds.max.z) * 0.5f,
    };
}

[[nodiscard]] bool IsSkiesImageExtension(const std::string& ext) {
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".hdr" || ext == ".exr";
}

[[nodiscard]] int SkyTexturePreferenceRank(const fs::path& path) {
    std::string name = path.filename().string();
    for (char& character : name) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    if (name.find("studio") != std::string::npos || name.find("neutral") != std::string::npos) {
        return 0;
    }
    if (name.find("sky") != std::string::npos || name.find("overcast") != std::string::npos) {
        return 1;
    }
    return 2;
}

void CollectSkiesImageFiles(const fs::path& directory,
                            const int maxDepth,
                            const int depth,
                            std::vector<fs::path>& out) {
    if (depth > maxDepth) {
        return;
    }
    std::error_code ec{};
    if (!fs::is_directory(directory, ec) || ec) {
        return;
    }
    for (const fs::directory_entry& entry : fs::directory_iterator(directory, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_directory()) {
            CollectSkiesImageFiles(entry.path(), maxDepth, depth + 1, out);
            continue;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string ext = entry.path().extension().string();
        for (char& character : ext) {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        if (IsSkiesImageExtension(ext)) {
            out.push_back(entry.path());
        }
    }
}

[[nodiscard]] fs::path PickSkiesEquirectRelative(const fs::path& textureRoot) {
    if (textureRoot.empty()) {
        return {};
    }
    std::vector<fs::path> candidates{};
    CollectSkiesImageFiles(textureRoot / "Skies", 4, 0, candidates);
    if (candidates.empty()) {
        return {};
    }
    std::sort(candidates.begin(), candidates.end(), [](const fs::path& left, const fs::path& right) {
        const int leftRank = SkyTexturePreferenceRank(left);
        const int rightRank = SkyTexturePreferenceRank(right);
        if (leftRank != rightRank) {
            return leftRank < rightRank;
        }
        return left.filename().string() < right.filename().string();
    });

    std::error_code relativeError{};
    const fs::path relative = fs::relative(candidates.front(), textureRoot, relativeError);
    if (!relativeError && !relative.empty()) {
        return relative.lexically_normal();
    }
    std::error_code canonicalError{};
    const fs::path canonical = fs::weakly_canonical(candidates.front(), canonicalError);
    return canonicalError ? candidates.front().lexically_normal() : canonical;
}

void LogBenchmarkResults(const int frameCount,
                         const std::chrono::steady_clock::time_point startTime,
                         const std::chrono::steady_clock::time_point endTime) {
    if (frameCount <= 0) {
        return;
    }
    const double seconds = std::chrono::duration<double>(endTime - startTime).count();
    if (seconds <= 0.0) {
        return;
    }
    const double fps = static_cast<double>(frameCount) / seconds;
    const double milliseconds = (seconds * 1000.0) / static_cast<double>(frameCount);
    ri::core::LogSection("Standalone FPS");
    ri::core::LogInfo("Renderer: vulkan");
    ri::core::LogInfo("Frames: " + std::to_string(frameCount)
                      + " elapsed=" + std::to_string(seconds)
                      + "s avg=" + std::to_string(fps)
                      + " FPS (" + std::to_string(milliseconds) + " ms/frame)");
}

void RegisterRawMouseForWindow(HWND hwnd) {
    RAWINPUTDEVICE device{};
    device.usUsagePage = 0x01;
    device.usUsage = 0x02;
    device.dwFlags = 0;
    device.hwndTarget = hwnd;
    RegisterRawInputDevices(&device, 1, sizeof(device));
}

void SandboxStandaloneWin32Hook(void* user,
                                void* hwndVoid,
                                unsigned int message,
                                std::uint64_t wParam,
                                std::int64_t lParam) {
    auto* state = static_cast<RuntimeState*>(user);
    if (state == nullptr) {
        return;
    }
    HWND hwnd = static_cast<HWND>(hwndVoid);

    switch (message) {
    case WM_CREATE:
        RegisterRawMouseForWindow(hwnd);
        break;
    case WM_INPUT: {
        if (!state->captureMouse || GetForegroundWindow() != hwnd) {
            break;
        }
        HRAWINPUT handle = reinterpret_cast<HRAWINPUT>(lParam);
        UINT byteSize = 0;
        if (GetRawInputData(handle, RID_INPUT, nullptr, &byteSize, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1)) {
            break;
        }
        std::vector<std::uint8_t> buffer(byteSize);
        if (GetRawInputData(handle, RID_INPUT, buffer.data(), &byteSize, sizeof(RAWINPUTHEADER)) ==
            static_cast<UINT>(-1)) {
            break;
        }
        const RAWINPUT* raw = reinterpret_cast<const RAWINPUT*>(buffer.data());
        if (raw->header.dwType != RIM_TYPEMOUSE) {
            break;
        }
        const RAWMOUSE& mouse = raw->data.mouse;
        if ((mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0) {
            break;
        }
        state->rawMouseAccumX += static_cast<float>(mouse.lLastX);
        state->rawMouseAccumY += static_cast<float>(mouse.lLastY);
        break;
    }
    case WM_ACTIVATE:
        if (LOWORD(static_cast<WPARAM>(wParam)) == WA_INACTIVE) {
            ClipCursor(nullptr);
            state->mouseCaptured = false;
            if (state->captureCursorHidden) {
                ShowCursor(TRUE);
                state->captureCursorHidden = false;
            }
        }
        break;
    default:
        break;
    }
}

void UpdateMouseLook(RuntimeState& state) {
    if (!state.captureMouse) {
        if (state.captureCursorHidden) {
            ShowCursor(TRUE);
            state.captureCursorHidden = false;
        }
        state.mouseCaptured = false;
        state.rawMouseAccumX = 0.0f;
        state.rawMouseAccumY = 0.0f;
        return;
    }
    if (state.hwnd == nullptr || GetForegroundWindow() != state.hwnd) {
        state.mouseCaptured = false;
        state.rawMouseAccumX = 0.0f;
        state.rawMouseAccumY = 0.0f;
        ClipCursor(nullptr);
        if (state.captureCursorHidden) {
            ShowCursor(TRUE);
            state.captureCursorHidden = false;
        }
        return;
    }

    RECT client{};
    if (!GetClientRect(state.hwnd, &client)) {
        return;
    }
    POINT upperLeft{client.left, client.top};
    POINT lowerRight{client.right, client.bottom};
    ClientToScreen(state.hwnd, &upperLeft);
    ClientToScreen(state.hwnd, &lowerRight);
    const RECT screenClip{upperLeft.x, upperLeft.y, lowerRight.x, lowerRight.y};

    if (!state.mouseCaptured) {
        ClipCursor(&screenClip);
        state.rawMouseAccumX = 0.0f;
        state.rawMouseAccumY = 0.0f;
        state.mouseCaptured = true;
        if (!state.captureCursorHidden) {
            ShowCursor(FALSE);
            state.captureCursorHidden = true;
        }
        return;
    }

    const float dx = state.rawMouseAccumX;
    const float dy = state.rawMouseAccumY;
    state.rawMouseAccumX = 0.0f;
    state.rawMouseAccumY = 0.0f;

    state.yawDegrees += dx * state.mouseSensitivityDegreesPerPixel;
    state.pitchDegrees =
        std::clamp(state.pitchDegrees + (dy * state.mouseSensitivityDegreesPerPixel), -84.0f, 84.0f);
}

ri::trace::MovementInput ReadMovementInput(RuntimeState& state) {
    auto axis = [](const int positiveKey, const int negativeKey) -> float {
        const bool positive = (GetAsyncKeyState(positiveKey) & 0x8000) != 0;
        const bool negative = (GetAsyncKeyState(negativeKey) & 0x8000) != 0;
        if (positive == negative) {
            return 0.0f;
        }
        return positive ? 1.0f : -1.0f;
    };

    const float yawRadians = ri::math::DegreesToRadians(state.yawDegrees);
    const ri::math::Vec3 forward{std::sin(yawRadians), 0.0f, std::cos(yawRadians)};
    const ri::math::Vec3 right = ri::math::Normalize(ri::math::Cross(ri::math::Vec3{0.0f, 1.0f, 0.0f}, forward));

    const bool jumpHeldNow = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
    const bool jumpPressedEdge = jumpHeldNow && !state.jumpHeldLastFrame;
    state.jumpHeldLastFrame = jumpHeldNow;

    return ri::trace::MovementInput{
        .moveForward = axis('W', 'S'),
        .moveRight = axis('D', 'A'),
        .viewForwardWorld = forward,
        .viewRightWorld = right,
        .sprintHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0,
        .jumpPressed = jumpPressedEdge,
        .applyShortJumpGravity = !jumpHeldNow,
    };
}

void SyncInitialView(RuntimeState& state) {
    const ri::math::Vec3 feet = FeetFromBounds(state.movement.body.bounds);
    const ri::math::Vec3 eye{feet.x, feet.y + state.cameraBaseHeight, feet.z};

    ri::scene::Node& rig = state.world.scene.GetNode(state.world.playerRig);
    rig.localTransform.position = eye;
    rig.localTransform.rotationDegrees = ri::math::Vec3{0.0f, state.yawDegrees, 0.0f};

    ri::scene::Node& cameraNode = state.world.scene.GetNode(state.world.playerCameraNode);
    cameraNode.localTransform.position = ri::math::Vec3{};
    cameraNode.localTransform.rotationDegrees = ri::math::Vec3{state.pitchDegrees, 0.0f, 0.0f};
    if (cameraNode.camera != ri::scene::kInvalidHandle) {
        state.world.scene.GetCamera(cameraNode.camera).fieldOfViewDegrees = state.currentFovDegrees;
    }
}

void SimulateAndApplyView(RuntimeState& state, const ri::trace::MovementInput& input, const float dt) {
    state.elapsedSeconds += dt;
    state.movement = ri::trace::SimulateMovementControllerStep(
                         state.traceScene, state.movement, input, dt, state.movementOptions)
                         .state;

    const ri::math::Vec3 feet = FeetFromBounds(state.movement.body.bounds);
    const ri::math::Vec3 planarVelocity{state.movement.body.velocity.x, 0.0f, state.movement.body.velocity.z};
    const float planarSpeed = ri::math::Length(planarVelocity);
    const float sprintSpeedRef = std::max(0.01f, state.movementOptions.maxSprintGroundSpeed);
    const float movementNorm = std::clamp(planarSpeed / sprintSpeedRef, 0.0f, 1.0f);
    const float bobScale = (input.sprintHeld ? state.bobSprintScale : 1.0f) * movementNorm;
    const float bobPhase = static_cast<float>((state.elapsedSeconds * state.bobFrequencyHz) * 6.283185307179586);
    const float cameraHeight = state.cameraBaseHeight + (std::sin(bobPhase) * state.bobAmplitude * bobScale);
    const ri::math::Vec3 eye{feet.x, feet.y + cameraHeight, feet.z};

    ri::scene::Node& rig = state.world.scene.GetNode(state.world.playerRig);
    rig.localTransform.position = eye;
    rig.localTransform.rotationDegrees = ri::math::Vec3{0.0f, state.yawDegrees, 0.0f};

    ri::scene::Node& cameraNode = state.world.scene.GetNode(state.world.playerCameraNode);
    cameraNode.localTransform.position = ri::math::Vec3{};
    cameraNode.localTransform.rotationDegrees = ri::math::Vec3{state.pitchDegrees, 0.0f, 0.0f};
    if (cameraNode.camera != ri::scene::kInvalidHandle) {
        const float targetFov = state.fovBaseDegrees + (input.sprintHeld ? state.fovSprintAddDegrees : 0.0f);
        const float blendAlpha = std::clamp(dt * state.fovLerpPerSecond, 0.0f, 1.0f);
        state.currentFovDegrees += (targetFov - state.currentFovDegrees) * blendAlpha;
        state.world.scene.GetCamera(cameraNode.camera).fieldOfViewDegrees = state.currentFovDegrees;
    }

    AnimateWorld(state.world, state.elapsedSeconds);
}

void TickStandaloneFrame(RuntimeState& state) {
    if ((GetAsyncKeyState(VK_ESCAPE) & 0x0001) != 0 && state.hwnd != nullptr) {
        PostMessageW(state.hwnd, WM_CLOSE, 0, 0);
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = now - state.lastTick;
    state.lastTick = now;
    const float dt = std::clamp(static_cast<float>(elapsed.count()), 1.0f / 180.0f, 1.0f / 45.0f);
    state.lastSimulationDeltaSeconds = dt;

    UpdateMouseLook(state);
    SimulateAndApplyView(state, ReadMovementInput(state), dt);
}

bool InitializeRuntimeState(const StandaloneOptions& options,
                            const ri::content::GameManifest& manifest,
                            RuntimeState& state) {
    const ri::content::ScriptScalarMap gameplay =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/gameplay.riscript"));
    const ri::content::ScriptScalarMap rendering =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/rendering.riscript"));
    const ri::content::ScriptScalarMap physics =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/physics.riscript"));
    const ri::content::ScriptScalarMap postprocess =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/postprocess.riscript"));
    const ri::content::ScriptScalarMap ui =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/ui.riscript"));

    std::string contractError;
    if (!ri::games::EnforceGameConfigContracts(
            manifest.rootPath,
            ri::games::GameConfigContractOptions{.mode = ri::games::GameConfigContractMode::Balanced},
            &contractError)) {
        ri::core::LogInfo(contractError);
        return false;
    }

    state.world = BuildWorld(manifest.name.empty() ? "RawIron Multiplayer Sandbox" : manifest.name, manifest.rootPath);
    const ri::spatial::SpatialIndexOptions bspOptions{
        .maxLeafSize = static_cast<std::size_t>(
            ri::content::ScriptScalarOrIntClamped(gameplay, "bsp_max_leaf_size", 12, 2, 128)),
        .maxDepth = static_cast<std::size_t>(
            ri::content::ScriptScalarOrIntClamped(gameplay, "bsp_max_depth", 10, 1, 24)),
    };
    state.traceScene = ri::trace::TraceScene(state.world.colliders, bspOptions);
    ri::core::LogInfo("Trace collider count: " + std::to_string(state.traceScene.ColliderCount()));
    {
        std::string colliderIds;
        for (std::size_t index = 0; index < state.world.colliders.size(); ++index) {
            if (index > 0U) {
                colliderIds += ",";
            }
            colliderIds += state.world.colliders[index].id;
        }
        ri::core::LogInfo("Trace colliders: " + colliderIds);
    }

    state.movementOptions.simulateStamina = false;
    state.movementOptions.maxGroundSpeed = ri::content::ScriptScalarOr(gameplay, "walk_speed", 7.0f);
    state.movementOptions.maxSprintGroundSpeed = ri::content::ScriptScalarOr(gameplay, "sprint_speed", 11.5f);
    state.movementOptions.maxAirSpeed = ri::content::ScriptScalarOr(gameplay, "air_speed", 10.0f);
    state.movementOptions.jumpSpeed = ri::content::ScriptScalarOr(gameplay, "jump_speed", 8.0f);
    state.movementOptions.gravity = ri::content::ScriptScalarOr(
        physics,
        "movement_gravity",
        ri::content::ScriptScalarOr(gameplay, "gravity", 26.0f));
    state.movementOptions.fallGravityMultiplier = ri::content::ScriptScalarOr(
        physics,
        "movement_fall_gravity_multiplier",
        ri::content::ScriptScalarOr(gameplay, "fall_gravity_multiplier", 1.25f));
    state.movementOptions.groundAcceleration =
        ri::content::ScriptScalarOr(gameplay, "ground_acceleration", state.movementOptions.groundAcceleration);
    state.movementOptions.airAcceleration =
        ri::content::ScriptScalarOr(gameplay, "air_acceleration", state.movementOptions.airAcceleration);
    state.movementOptions.groundFriction =
        ri::content::ScriptScalarOr(gameplay, "ground_friction", state.movementOptions.groundFriction);
    state.movementOptions.stopSpeed = ri::content::ScriptScalarOr(gameplay, "stop_speed", state.movementOptions.stopSpeed);
    state.movementOptions.airControl =
        ri::content::ScriptScalarOrClamped(gameplay, "air_control", state.movementOptions.airControl, 0.0f, 1.0f);
    state.movementOptions.coyoteTimeSeconds =
        ri::content::ScriptScalarOrClamped(gameplay, "coyote_time", state.movementOptions.coyoteTimeSeconds, 0.0f, 0.5f);
    state.movementOptions.jumpBufferTimeSeconds = ri::content::ScriptScalarOrClamped(
        gameplay, "jump_buffer_time", state.movementOptions.jumpBufferTimeSeconds, 0.0f, 0.5f);
    state.movementOptions.lowJumpGravityMultiplier = ri::content::ScriptScalarOrClamped(
        gameplay, "low_jump_gravity_multiplier", state.movementOptions.lowJumpGravityMultiplier, 1.0f, 4.0f);
    state.movementOptions.maxFallSpeed =
        ri::content::ScriptScalarOrClamped(gameplay, "max_fall_speed", state.movementOptions.maxFallSpeed, 4.0f, 120.0f);
    state.movementOptions.groundProbeJumpMaxDown = ri::content::ScriptScalarOrClamped(
        gameplay, "ground_probe_jump_max_down", state.movementOptions.groundProbeJumpMaxDown, 0.0f, 1.5f);
    state.movementOptions.groundAdhesionSpeed = ri::content::ScriptScalarOrClamped(
        gameplay, "ground_adhesion_speed", state.movementOptions.groundAdhesionSpeed, 0.0f, 8.0f);
    state.movementOptions.refineStructuralTraceHit = ri::scene::MakeStructuralMeshTraceRefiner(state.world.scene);

    const float movementSpeedScale =
        ri::content::ScriptScalarOrClamped(gameplay, "movement_speed_scale", 1.0f, 0.45f, 1.5f);
    state.movementOptions.maxGroundSpeed =
        std::clamp(state.movementOptions.maxGroundSpeed * movementSpeedScale, 2.0f, 16.0f);
    state.movementOptions.maxSprintGroundSpeed =
        std::clamp(state.movementOptions.maxSprintGroundSpeed * movementSpeedScale, 3.0f, 18.0f);
    state.movementOptions.maxAirSpeed =
        std::clamp(state.movementOptions.maxAirSpeed * movementSpeedScale, 2.0f, 18.0f);

    const ri::math::Vec3 fallbackSpawn{
        -state.world.catalogExtents.x + 28.5f,
        4.30f,
        -state.world.catalogExtents.z + 12.3f,
    };
    const ri::math::Vec3 spawnFeet{
        ri::content::ScriptScalarOr(gameplay, "spawn_x", fallbackSpawn.x),
        ri::content::ScriptScalarOr(gameplay, "spawn_y", fallbackSpawn.y),
        ri::content::ScriptScalarOr(gameplay, "spawn_z", fallbackSpawn.z),
    };
    state.cameraBaseHeight =
        ri::content::ScriptScalarOrClamped(gameplay, "camera_height", state.cameraBaseHeight, 0.8f, 2.2f);
    state.yawDegrees = ri::content::ScriptScalarOrClamped(gameplay, "spawn_yaw", 68.0f, -180.0f, 180.0f);
    state.pitchDegrees = ri::content::ScriptScalarOrClamped(gameplay, "spawn_pitch", -12.0f, -84.0f, 84.0f);
    state.movement.body.bounds = BuildPlayerBounds(spawnFeet);
    state.movement.onGround = true;

    state.bobAmplitude = ri::content::ScriptScalarOrClamped(gameplay, "head_bob_amplitude", state.bobAmplitude, 0.0f, 0.2f);
    state.bobFrequencyHz = ri::content::ScriptScalarOrClamped(gameplay, "head_bob_frequency", state.bobFrequencyHz, 0.1f, 6.0f);
    state.bobSprintScale =
        ri::content::ScriptScalarOrClamped(gameplay, "head_bob_sprint_scale", state.bobSprintScale, 1.0f, 3.0f);
    state.mouseSensitivityDegreesPerPixel = std::clamp(
        ri::content::ScriptScalarOr(gameplay, "mouse_sensitivity", state.mouseSensitivityDegreesPerPixel),
        0.01f,
        2.0f);
    state.fovBaseDegrees = ri::content::ScriptScalarOrClamped(
        postprocess,
        "fov_base",
        ri::content::ScriptScalarOrClamped(rendering, "fov_base", state.fovBaseDegrees, 45.0f, 120.0f),
        45.0f,
        120.0f);
    state.fovSprintAddDegrees = ri::content::ScriptScalarOrClamped(
        postprocess,
        "fov_sprint_add",
        ri::content::ScriptScalarOrClamped(rendering, "fov_sprint_add", state.fovSprintAddDegrees, 0.0f, 25.0f),
        0.0f,
        25.0f);
    state.fovLerpPerSecond = ri::content::ScriptScalarOrClamped(
        postprocess,
        "fov_lerp_per_second",
        ri::content::ScriptScalarOrClamped(rendering, "fov_lerp_per_second", state.fovLerpPerSecond, 0.5f, 40.0f),
        0.5f,
        40.0f);
    state.currentFovDegrees = state.fovBaseDegrees;
    SyncInitialView(state);

    state.previewOptions.width = options.width;
    state.previewOptions.height = options.height;
    state.previewOptions.pointSampleTextures = false;
    state.previewOptions.adaptiveTextureSampling = true;
    state.previewOptions.clearTop = ri::math::Vec3{
        ri::content::ScriptScalarOr(rendering, "clear_top_r", 0.34f),
        ri::content::ScriptScalarOr(rendering, "clear_top_g", 0.42f),
        ri::content::ScriptScalarOr(rendering, "clear_top_b", 0.55f),
    };
    state.previewOptions.clearBottom = ri::math::Vec3{
        ri::content::ScriptScalarOr(rendering, "clear_bottom_r", 0.05f),
        ri::content::ScriptScalarOr(rendering, "clear_bottom_g", 0.08f),
        ri::content::ScriptScalarOr(rendering, "clear_bottom_b", 0.12f),
    };
    state.previewOptions.fogColor = ri::math::Vec3{
        ri::content::ScriptScalarOr(rendering, "fog_r", 0.40f),
        ri::content::ScriptScalarOr(rendering, "fog_g", 0.47f),
        ri::content::ScriptScalarOr(rendering, "fog_b", 0.55f),
    };
    state.previewOptions.ambientLight = ri::math::Vec3{
        ri::content::ScriptScalarOr(rendering, "ambient_r", 0.06f),
        ri::content::ScriptScalarOr(rendering, "ambient_g", 0.07f),
        ri::content::ScriptScalarOr(rendering, "ambient_b", 0.09f),
    };
    state.nativeRenderTuning.exposure = ri::content::ScriptScalarOrClamped(
        postprocess,
        "native_exposure",
        ri::content::ScriptScalarOrClamped(rendering, "native_exposure", 1.08f, 0.5f, 2.5f),
        0.5f,
        2.5f);
    state.nativeRenderTuning.contrast = ri::content::ScriptScalarOrClamped(
        postprocess,
        "native_contrast",
        ri::content::ScriptScalarOrClamped(rendering, "native_contrast", 1.12f, 0.7f, 1.75f),
        0.7f,
        1.6f);
    state.nativeRenderTuning.saturation = ri::content::ScriptScalarOrClamped(
        postprocess,
        "native_saturation",
        ri::content::ScriptScalarOrClamped(rendering, "native_saturation", 1.08f, 0.0f, 1.8f),
        0.0f,
        1.8f);
    state.nativeRenderTuning.fogDensity = ri::content::ScriptScalarOrClamped(
        postprocess,
        "native_fog_density",
        ri::content::ScriptScalarOrClamped(rendering, "native_fog_density", 0.0015f, 0.0f, 0.05f),
        0.0f,
        0.05f);
    state.nativeRenderTuning.qualityTier =
        ri::content::ScriptScalarOrIntClamped(ui, "native_quality_tier", state.nativeRenderTuning.qualityTier, 0, 2);

    std::string shaderCfgError;
    if (ri::render::TryLoadShaderCfgFromRoot(manifest.rootPath, &state.shaderPresentation, &shaderCfgError)) {
        ri::core::LogInfo("Loaded sandbox shader.cfg from game root.");
    } else if (!shaderCfgError.empty()) {
        ri::core::LogInfo("shader.cfg: " + shaderCfgError);
    }

    const ri::math::Vec3 center = ri::spatial::Center(state.movement.body.bounds);
    const std::optional<ri::trace::TraceHit> groundProbe = state.traceScene.FindGroundHit(
        center,
        ri::trace::GroundTraceOptions{
            .maxDistance = 2.0f,
            .structuralOnly = true,
            .ignoreId = {},
            .minNormalY = 0.5f,
        });
    if (groundProbe.has_value()) {
        ri::core::LogInfo(
            "Spawn ground probe: hit=" + groundProbe->id +
            " y=" + std::to_string(groundProbe->point.y) +
            " distance=" + std::to_string(center.y - groundProbe->point.y));
    } else {
        ri::core::LogInfo("Spawn ground probe: no hit");
    }

    const fs::path workspaceForTextures =
        !options.workspaceRoot.empty() ? options.workspaceRoot : ri::content::DetectWorkspaceRoot(manifest.rootPath);
    fs::path executablePath{};
    wchar_t moduleWide[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, moduleWide, MAX_PATH) > 0) {
        executablePath = fs::path(std::wstring(moduleWide));
    }
    const fs::path textureDir = ri::content::PickEngineTexturesDirectory(workspaceForTextures, executablePath);
    if (!textureDir.empty()) {
        state.previewOptions.textureRoot = textureDir;
        state.nativeSkyEquirectRelative = PickSkiesEquirectRelative(textureDir);
        ri::core::LogInfo("Texture library: " + textureDir.string());
    } else {
        ri::core::LogInfo("Texture library not found; preview will render without texture files.");
    }

    ri::core::LogInfo(
        "Sandbox 3D experience: spawned engine primitive catalog rows with sandbox-owned world host.");
    ri::core::LogInfo(
        "Movement tuning walk=" + std::to_string(state.movementOptions.maxGroundSpeed)
        + " sprint=" + std::to_string(state.movementOptions.maxSprintGroundSpeed)
        + " mouse=" + std::to_string(state.mouseSensitivityDegreesPerPixel));
    ri::core::LogInfo(
        "Native Vulkan quality tier=" + std::to_string(state.nativeRenderTuning.qualityTier)
        + " exposure=" + std::to_string(state.nativeRenderTuning.exposure)
        + " fogDensity=" + std::to_string(state.nativeRenderTuning.fogDensity));
    return true;
}

bool RunStandaloneNativeVulkanLoop(const StandaloneOptions& options,
                                   RuntimeState& state,
                                   ri::runtime::RuntimeCore& runtime,
                                   std::string* error) {
    state.lastTick = std::chrono::steady_clock::now();
    state.hwnd = nullptr;
    const bool benchmarking = options.benchmarkFrames > 0;
    int benchmarkedFrames = 0;
    int runtimeFrameIndex = 0;
    const auto benchmarkStart = std::chrono::steady_clock::now();
    const fs::path textureRootForVulkan = state.previewOptions.textureRoot.value_or(fs::path{});
    ri::core::LogInfo(std::string("Native Vulkan hybrid HDR: ") + (options.hybridHdr ? "on" : "off"));

    const ri::render::vulkan::VulkanPreviewWindowOptions windowOptions{
        .windowTitle = "RawIron Multiplayer Sandbox",
        .presentModePreference = ri::render::vulkan::VulkanPresentModePreference::Auto,
        .textureRoot = textureRootForVulkan,
        .messageUserData = &state,
        .onWin32Message = &SandboxStandaloneWin32Hook,
        .outClientHwnd = &state.hwnd,
        .enableHybridHdrPresentation = options.hybridHdr,
        .shaderPresentation = state.shaderPresentation,
    };

    const ri::render::vulkan::VulkanNativeSceneFrameCallback buildFrame =
        [&state, &options, &benchmarkedFrames, &runtimeFrameIndex, &runtime, &textureRootForVulkan](
            ri::render::vulkan::VulkanNativeSceneFrame& frame,
            std::string*) {
            const ri::core::FrameContext runtimeFrame = ri::games::BuildGameRuntimeFrameContext(
                runtimeFrameIndex,
                static_cast<double>(state.lastSimulationDeltaSeconds),
                static_cast<double>(state.elapsedSeconds),
                static_cast<double>(GetTickCount64()) / 1000.0);
            ++runtimeFrameIndex;
            if (!runtime.Frame(runtimeFrame)) {
                if (state.hwnd != nullptr) {
                    PostMessageW(state.hwnd, WM_CLOSE, 0, 0);
                }
                return true;
            }

            frame.scene = &state.world.scene;
            frame.cameraNode = state.world.playerCameraNode;
            frame.textureRoot = textureRootForVulkan;
            frame.skyEquirectTextureRelative = state.nativeSkyEquirectRelative;
            frame.animationTimeSeconds = static_cast<double>(state.elapsedSeconds);
            frame.renderQualityTier = state.nativeRenderTuning.qualityTier;
            frame.renderExposure = state.nativeRenderTuning.exposure;
            frame.renderContrast = state.nativeRenderTuning.contrast;
            frame.renderSaturation = state.nativeRenderTuning.saturation;
            frame.renderFogDensity = state.nativeRenderTuning.fogDensity;
            frame.useEnvironmentClear = true;
            frame.environmentClearTop = state.previewOptions.clearTop;
            frame.environmentClearBottom = state.previewOptions.clearBottom;
            frame.nativeFogColorNear = state.previewOptions.fogColor;
            frame.nativeFogColorFar = state.previewOptions.fogColor * 1.06f;
            frame.nativeAmbientLight = state.previewOptions.ambientLight;
            frame.postProcess.timeSeconds =
                std::isfinite(state.elapsedSeconds) ? static_cast<float>(state.elapsedSeconds) : 0.0f;

            if (options.benchmarkFrames > 0) {
                ++benchmarkedFrames;
                if (benchmarkedFrames >= options.benchmarkFrames && state.hwnd != nullptr) {
                    PostMessageW(state.hwnd, WM_CLOSE, 0, 0);
                }
            }
            return true;
        };

    const bool ok = ri::render::vulkan::RunVulkanNativeSceneLoop(
        options.width,
        options.height,
        buildFrame,
        windowOptions,
        error);
    if (benchmarking && ok) {
        LogBenchmarkResults(benchmarkedFrames, benchmarkStart, std::chrono::steady_clock::now());
    }
    ClipCursor(nullptr);
    if (state.captureCursorHidden) {
        ShowCursor(TRUE);
        state.captureCursorHidden = false;
    }
    state.hwnd = nullptr;
    return ok;
}

void AttachSandboxNetModules(ri::runtime::RuntimeCore& runtime,
                             const StandaloneOptions& options,
                             const ri::core::CommandLine& commandLine) {
    ri::runtime::AuthoritativeNetConfig net{};
    net.mode = ParseNetMode(options.netMode);
    net.bindEndpoint.host = "0.0.0.0";
    net.bindEndpoint.port = static_cast<std::uint16_t>(commandLine.GetIntOr("--port", 27015));
    net.p2pBindEndpoint.port = static_cast<std::uint16_t>(commandLine.GetIntOr("--p2p-port", 27115));
    net.connectEndpoint.host = commandLine.GetValue("--connect-host").value_or("127.0.0.1");
    net.connectEndpoint.port = static_cast<std::uint16_t>(commandLine.GetIntOr("--connect-port", 27015));
    net.rendezvousProvider = ParseRendezvous(options.rendezvous);
    net.issueJoinCodeOnStartup = options.issueJoinCode;
    if (options.joinCode.has_value()) {
        net.joinCodeToResolve = *options.joinCode;
    }
    net.enableP2PPlane = (net.mode == ri::runtime::NetMode::HybridP2P);
    net.tickRate = options.netTick;
    net.serverTickRate = options.serverTick;
    net.maxPeers = options.maxPeers;
    net.latencySimulation.enabled = options.simulateNet;
    net.latencySimulation.baseDelayMs = options.simDelayMs;
    net.latencySimulation.jitterMs = options.simJitterMs;
    net.latencySimulation.packetLossPercent = static_cast<float>(options.simLossPct);

    auto netModule = std::make_unique<ri::runtime::AuthoritativeNetModule>(net);
    ri::runtime::AuthoritativeNetModule* netPtr = netModule.get();
    runtime.AddModule(std::move(netModule));

    ri::runtime::BotSwarmConfig swarm{};
    swarm.botCount = options.bots;
    swarm.commandChannel = 0;
    swarm.reliableCommands = options.botsReliable;
    runtime.AddModule(std::make_unique<ri::runtime::BotSwarmModule>(netPtr, swarm));
}

bool CaptureStandalonePreviewSnapshot(const RuntimeState& state,
                                      const fs::path& outputPath,
                                      std::string* error) {
    ri::render::software::ScenePreviewCache cache{};
    const ri::render::software::SoftwareImage image = ri::render::software::RenderScenePreview(
        state.world.scene,
        state.world.playerCameraNode,
        state.previewOptions,
        &cache);

    if (!outputPath.parent_path().empty()) {
        std::error_code ec{};
        fs::create_directories(outputPath.parent_path(), ec);
        if (ec) {
            if (error != nullptr) {
                *error = "Failed to create preview output directory: " + outputPath.parent_path().string();
            }
            return false;
        }
    }

    if (!ri::render::software::SaveBmp(image, outputPath.string())) {
        if (error != nullptr) {
            *error = "Failed to save preview image: " + outputPath.string();
        }
        return false;
    }

    ri::core::LogInfo("Sandbox preview snapshot saved: " + outputPath.string());
    ri::core::LogInfo("Snapshot size: " + std::to_string(image.width) + "x" + std::to_string(image.height));
    return true;
}

} // namespace
#endif

bool RunStandalone3D(const StandaloneOptions& options,
                     const ri::core::CommandLine& commandLine,
                     std::string* error) {
    try {
#if defined(_WIN32)
        const std::optional<ri::content::GameManifest> manifest = ResolveStandaloneGameManifest(options);
        if (!manifest.has_value()) {
            if (error != nullptr) {
                *error = "Unable to resolve game manifest for '" + options.gameId + "'.";
            }
            return false;
        }
        const std::vector<std::string> formatIssues = ri::content::ValidateGameProjectFormat(*manifest);
        if (!formatIssues.empty()) {
            if (error != nullptr) {
                *error = "Game format validation failed:";
                for (const std::string& issue : formatIssues) {
                    *error += " " + issue;
                }
            }
            return false;
        }

        auto manifestService = std::make_shared<ri::content::GameManifest>(*manifest);
        auto supportService = std::make_shared<ri::content::GameRuntimeSupportData>(
            ri::content::LoadGameRuntimeSupportData(manifest->rootPath));
        ri::games::LogGameRuntimeSupportSummary(*supportService);

        ri::runtime::RuntimeCore runtime = ri::games::CreateGameRuntimeCore(
            *manifest,
            "RawIron.Game.MultiplayerSandbox",
            ri::games::BuildGameRuntimePaths(*manifest, options.workspaceRoot),
            ri::games::GameRuntimeBootServices{
                .manifest = std::move(manifestService),
                .support = std::move(supportService),
            });

        AttachSandboxNetModules(runtime, options, commandLine);

        RuntimeState state{};
        if (!InitializeRuntimeState(options, *manifest, state)) {
            if (error != nullptr && error->empty()) {
                *error = "Failed to initialize RawIron Multiplayer Sandbox 3D state.";
            }
            return false;
        }
        const bool savePreview = commandLine.HasFlag("--save-preview") || commandLine.GetValue("--output").has_value();
        if (savePreview) {
            const fs::path outputPath = ResolvePreviewOutputPath(commandLine);
            if (!CaptureStandalonePreviewSnapshot(state, outputPath, error)) {
                return false;
            }
            ri::core::LogSection("RawIron Multiplayer Sandbox Preview");
            ri::core::LogInfo("Snapshot demo completed without entering the live Vulkan loop.");
            return true;
        }
        if (!ri::games::AttachGameSimulationTick(runtime, [&state](const ri::core::FrameContext&) {
                TickStandaloneFrame(state);
            })) {
            ri::core::LogInfo("Runtime core: failed to attach sandbox game simulation tick module.");
        }
        if (!ri::games::StartupGameRuntimeCore(runtime, commandLine, error)) {
            return false;
        }

        ri::core::LogSection("RawIron Multiplayer Sandbox Standalone");
        ri::core::LogInfo("Game: " + manifest->name + " (" + manifest->id + ")");
        ri::core::LogInfo("Game root: " + manifest->rootPath.string());
        ri::core::LogInfo("Presenter: vulkan-native");

        std::string runtimeError;
        const bool ok = RunStandaloneNativeVulkanLoop(options, state, runtime, &runtimeError);
        runtime.Shutdown();
        if (!ok) {
            if (error != nullptr) {
                *error = runtimeError.empty() ? "Native Vulkan scene loop failed without reporting an error." : runtimeError;
            }
            return false;
        }
        return true;
#else
        if (error != nullptr) {
            *error = "Standalone currently requires Windows.";
        }
        return false;
#endif
    } catch (const std::exception& exception) {
        if (error != nullptr) {
            *error = exception.what();
        }
        return false;
    }
}

} // namespace ri::games::multiplayersandbox
