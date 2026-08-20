#include "RawIron/Games/CubeTest/CubeTestRuntime.h"

#include "RawIron/Content/EngineAssets.h"
#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/GameRuntimeSupport.h"
#include "RawIron/Content/RipakArchive.h"
#include "RawIron/Content/ScriptScalars.h"
#include "RawIron/Content/ShaderAsset.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Games/GameConfigContracts.h"
#include "RawIron/Games/GamePluginRuntimeBridge.h"
#include "RawIron/Games/GameRuntimeCore.h"
#include "RawIron/Games/CubeTest/CubeTestWorld.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Render/SoftwarePreview.h"
#include "RawIron/Render/VulkanPreviewPresenter.h"
#include "RawIron/Runtime/HostChrome.h"
#include "RawIron/Runtime/HostInputService.h"
#include "RawIron/Runtime/RuntimeCore.h"
#include "RawIron/Scene/SceneUtils.h"
#include "RawIron/Spatial/Aabb.h"
#include "RawIron/Trace/KeyboardMovementInput.h"
#include "RawIron/Trace/MovementController.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace ri::games::cubetest {

namespace {

namespace fs = std::filesystem;

constexpr std::string_view kRawIronX32Index = "indexes/RAWIRONX32.index.json";

std::shared_ptr<const ri::content::CookedTexturePack> TryMountRawIronX32(
    const fs::path& workspaceRoot,
    const fs::path& looseTextureRoot) {
    const std::array candidates{
        looseTextureRoot.parent_path() / "RAWIRONX32.ripak",
        workspaceRoot / "Assets" / "RAWIRONX32.ripak",
        workspaceRoot.parent_path() / "Assets" / "RAWIRONX32.ripak",
    };
    for (const fs::path& candidate : candidates) {
        std::error_code error;
        if (!fs::is_regular_file(candidate, error) || error) {
            continue;
        }
        try {
            auto pack = std::make_shared<ri::content::CookedTexturePack>(
                ri::content::CookedTexturePack::Open(candidate, kRawIronX32Index));
            for (const std::string& logicalPath : CubeTestCookedTextureSequence()) {
                if (pack->Find(logicalPath) == nullptr) {
                    throw std::runtime_error("RAWIRONX32 is missing cube texture: " + logicalPath);
                }
            }
            ri::core::LogInfo("Cube Test mounted cooked textures: " + candidate.string());
            return pack;
        } catch (const std::exception& mountError) {
            ri::core::LogInfo("Cube Test could not mount " + candidate.string() + ": " + mountError.what());
        }
    }
    ri::core::LogInfo("Cube Test RAWIRONX32 mount unavailable; retaining loose LRT cube textures.");
    return nullptr;
}

fs::path ResolvePreviewOutputPath(const ri::core::CommandLine& commandLine) {
    if (const std::optional<std::string> output = commandLine.GetValue("--output"); output.has_value() && !output->empty()) {
        return fs::path(*output);
    }
    return fs::current_path() / "cube_test_preview.bmp";
}

std::optional<ri::content::GameManifest> ResolveStandaloneGameManifest(
    const StandaloneOptions& options,
    const fs::path& resolvedWorkspaceRoot) {
    if (!options.gameRoot.empty()) {
        return ri::content::LoadGameManifest(options.gameRoot / "manifest.json");
    }
    return ri::content::ResolveGameManifest(resolvedWorkspaceRoot, options.gameId);
}

bool SavePreview(const CubeTestWorld& world,
                 const fs::path& textureRoot,
                 std::shared_ptr<const ri::content::CookedTexturePack> cookedTexturePack,
                 const fs::path& outputPath,
                 const double animationSeconds,
                 const std::string_view hiddenNodeName,
                 ri::render::software::ScenePreviewCache* sharedCache,
                 std::string* error) {
    ri::render::software::ScenePreviewOptions options{};
    options.width = 960;
    options.height = 540;
    options.textureRoot = textureRoot;
    options.cookedTexturePack = std::move(cookedTexturePack);
    options.clearTop = {0.58f, 0.66f, 0.72f};
    options.clearBottom = {0.32f, 0.35f, 0.36f};
    options.fogColor = {0.50f, 0.55f, 0.58f};
    options.fogStartDepth = 18.0f;
    options.fogEndDepth = 80.0f;
    options.fogStrength = 0.28f;
    options.ambientLight = {0.26f, 0.28f, 0.30f};
    options.previewExposure = 1.08f;
    options.previewContrast = 1.08f;
    options.previewSaturation = 1.0f;
    options.animationTimeSeconds = animationSeconds;
    if (!hiddenNodeName.empty()) {
        const std::optional<int> hiddenNode = ri::scene::FindNodeByName(world.scene, hiddenNodeName);
        if (hiddenNode.has_value()) {
            options.hiddenNodeHandles.push_back(*hiddenNode);
        }
    }

    ri::render::software::ScenePreviewCache localCache{};
    ri::render::software::ScenePreviewCache* const cache =
        sharedCache != nullptr ? sharedCache : &localCache;
    const ri::render::software::SoftwareImage image =
        ri::render::software::RenderScenePreview(world.scene, world.playerCameraNode, options, cache);
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
    ri::core::LogInfo("Cube Test preview saved: " + outputPath.string());
    return true;
}

bool SaveJigglePreviewSequence(const fs::path& textureRoot,
                               const std::shared_ptr<const ri::content::CookedTexturePack>& cookedTexturePack,
                               const fs::path& workspaceRoot,
                               const fs::path& outputPath,
                               const int frames,
                               const std::string_view hiddenNodeName,
                               std::string* error) {
    const int frameCount = std::clamp(frames, 1, 120);
    const fs::path parent = outputPath.parent_path();
    const std::string stem = outputPath.stem().string();
    const std::string ext = outputPath.extension().empty() ? ".bmp" : outputPath.extension().string();
    CubeTestWorld frameWorld = BuildCubeTestWorld("Cube Test Jiggle Preview", workspaceRoot);
    if (cookedTexturePack) {
        ConfigureCookedTextureCube(frameWorld, CubeTestCookedTextureSequence());
    }
    ri::render::software::ScenePreviewCache previewCache{};
    for (int index = 0; index < frameCount; ++index) {
        const double seconds = static_cast<double>(index) / 12.0;
        AnimateCubeTestWorldJiggle(frameWorld, seconds);
        const fs::path framePath = parent / (stem + "_" + std::to_string(index) + ext);
        if (!SavePreview(
                frameWorld, textureRoot, cookedTexturePack, framePath, seconds,
                hiddenNodeName, &previewCache, error)) {
            return false;
        }
    }
    return true;
}

#if defined(_WIN32)
struct PlayState {
    CubeTestWorld world{};
    HWND hwnd = nullptr;
    bool mouseLook = false;
    POINT lastMouse{};
    float yawDegrees = 0.0f;
    float pitchDegrees = -5.0f;
    std::chrono::steady_clock::time_point lastTick{};
    float elapsedSeconds = 0.0f;
    float lastDeltaSeconds = 1.0f / 60.0f;
    float mouseSensitivity = 0.12f;
    float cameraHeight = 1.62f;
    ri::math::Vec3 spawnFeet{0.0f, 0.20f, -7.4f};
    ri::trace::TraceScene traceScene{};
    ri::trace::MovementControllerState movement{};
    ri::trace::MovementControllerOptions movementOptions{};
    ri::trace::KeyboardMovementEdges movementEdges{};
    /// Mounted HostInput service — games query, engine owns Update.
    ri::runtime::HostInputService* hostInput = nullptr;
    int renderQualityTier = 2;
    float renderExposure = 1.02f;
    float renderContrast = 1.08f;
    float renderSaturation = 1.0f;
    float renderFogDensity = 0.002f;
    float fogStart = 22.0f;
    float fogEnd = 96.0f;
    float fogStrength = 0.20f;
    ri::math::Vec3 clearTop{0.50f, 0.62f, 0.70f};
    ri::math::Vec3 clearBottom{0.30f, 0.34f, 0.34f};
    ri::math::Vec3 ambientLight{0.34f, 0.36f, 0.34f};
    ri::games::GamePluginRuntimeHost pluginHost{};
};

ri::spatial::Aabb BuildPlayerBounds(const ri::math::Vec3& feet) {
    return ri::spatial::Aabb{
        .min = {feet.x - 0.25f, feet.y, feet.z - 0.25f},
        .max = {feet.x + 0.25f, feet.y + 1.8f, feet.z + 0.25f},
    };
}

ri::math::Vec3 FeetFromBounds(const ri::spatial::Aabb& bounds) {
    return {
        (bounds.min.x + bounds.max.x) * 0.5f,
        bounds.min.y,
        (bounds.min.z + bounds.max.z) * 0.5f,
    };
}

void UpdateCameraNodes(PlayState& state) {
    const ri::math::Vec3 feet = FeetFromBounds(state.movement.body.bounds);
    ri::scene::Node& rig = state.world.scene.GetNode(state.world.playerRig);
    rig.localTransform.position = {feet.x, feet.y + state.cameraHeight, feet.z};
    rig.localTransform.rotationDegrees = {0.0f, state.yawDegrees, 0.0f};

    ri::scene::Node& camera = state.world.scene.GetNode(state.world.playerCameraNode);
    camera.localTransform.rotationDegrees = {state.pitchDegrees, 0.0f, 0.0f};
}

void TickPlayState(PlayState& state) {
    const auto now = std::chrono::steady_clock::now();
    const float deltaSeconds = state.lastTick.time_since_epoch().count() == 0
        ? 1.0f / 60.0f
        : std::clamp(std::chrono::duration<float>(now - state.lastTick).count(), 0.001f, 0.05f);
    state.lastTick = now;
    state.lastDeltaSeconds = deltaSeconds;
    state.elapsedSeconds += deltaSeconds;

    if (state.hostInput == nullptr) {
        return;
    }
    state.hostInput->Sync(state.hwnd);
    const ri::runtime::HostChromeActions chrome = ri::runtime::PollHostChrome(*state.hostInput);
    if (chrome.quitRequested && state.hwnd != nullptr) {
        PostMessageW(state.hwnd, WM_CLOSE, 0, 0);
    }
    const ri::core::KeyboardFocusGate& focus = state.hostInput->Focus();

    state.yawDegrees +=
        ri::trace::KeyboardAxis(focus, VK_RIGHT, VK_LEFT) * 92.0f * deltaSeconds;
    state.pitchDegrees = std::clamp(
        state.pitchDegrees + ri::trace::KeyboardAxis(focus, VK_DOWN, VK_UP) * 72.0f * deltaSeconds,
        -80.0f,
        78.0f);

    const ri::trace::MovementInput movementInput =
        ri::trace::BuildKeyboardMovementInput(focus, state.yawDegrees, state.movementEdges);
    state.movement = ri::trace::SimulateMovementControllerStep(
                         state.traceScene,
                         state.movement,
                         movementInput,
                         deltaSeconds,
                         state.movementOptions)
                         .state;
    if (FeetFromBounds(state.movement.body.bounds).y < -5.0f) {
        state.movement = {};
        state.movement.body.bounds = BuildPlayerBounds(state.spawnFeet);
        state.movement.onGround = true;
    }

    UpdateCameraNodes(state);
    (void)ri::games::TickGamePluginRuntime(state.pluginHost, static_cast<double>(state.elapsedSeconds));
}

void CubeTestWin32Hook(void* user, void* hwnd, const unsigned int message, const std::uint64_t, const std::int64_t lParam) {
    auto* state = static_cast<PlayState*>(user);
    if (state == nullptr) {
        return;
    }
    state->hwnd = static_cast<HWND>(hwnd);
    switch (message) {
    case WM_RBUTTONDOWN:
        state->mouseLook = true;
        SetCapture(state->hwnd);
        GetCursorPos(&state->lastMouse);
        ShowCursor(FALSE);
        break;
    case WM_RBUTTONUP:
        state->mouseLook = false;
        ReleaseCapture();
        ShowCursor(TRUE);
        break;
    case WM_MOUSEMOVE:
        if (state->mouseLook) {
            POINT cursor{};
            GetCursorPos(&cursor);
            const LONG dx = cursor.x - state->lastMouse.x;
            const LONG dy = cursor.y - state->lastMouse.y;
            state->lastMouse = cursor;
            state->yawDegrees += static_cast<float>(dx) * state->mouseSensitivity;
            state->pitchDegrees = std::clamp(
                state->pitchDegrees + static_cast<float>(dy) * state->mouseSensitivity * 0.84f,
                -80.0f,
                78.0f);
        }
        break;
    default:
        break;
    }
    (void)lParam;
}

bool RunNativeLoop(const StandaloneOptions& options,
                   const ri::content::GameManifest& manifest,
                   const fs::path& textureRoot,
                   const std::shared_ptr<const ri::content::CookedTexturePack>& cookedTexturePack,
                   ri::runtime::RuntimeCore& runtime,
                   const ri::core::CommandLine& commandLine,
                   std::string* error) {
    PlayState state{};
    state.world = BuildCubeTestWorld(
        "Cube Test", ri::content::DetectWorkspaceRoot(manifest.rootPath));
    if (cookedTexturePack) {
        ConfigureCookedTextureCube(state.world, CubeTestCookedTextureSequence());
    }
    const ri::content::ScriptScalarMap gameplay = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/gameplay.riscript"));
    const ri::content::ScriptScalarMap rendering = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/rendering.riscript"));
    const ri::content::ScriptScalarMap postprocess = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/postprocess.riscript"));
    const ri::content::ScriptScalarMap physics = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/physics.riscript"));
    const ri::content::ScriptScalarMap plugins = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/plugins.riscript"));
    const ri::content::ScriptScalarMap pluginsPolicy = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest.rootPath, "config/plugins.policy"));
    state.movementOptions.simulateStamina = false;
    state.movementOptions.maxGroundSpeed = ri::content::ScriptScalarOrClamped(gameplay, "walk_speed", 4.2f, 0.5f, 24.0f);
    state.movementOptions.maxSprintGroundSpeed = ri::content::ScriptScalarOrClamped(
        gameplay, "sprint_speed", 7.5f, 0.5f, 32.0f);
    state.movementOptions.maxAirSpeed = ri::content::ScriptScalarOrClamped(gameplay, "air_speed", 6.0f, 0.5f, 32.0f);
    state.movementOptions.jumpSpeed = ri::content::ScriptScalarOrClamped(gameplay, "jump_speed", 7.2f, 0.5f, 32.0f);
    state.movementOptions.groundAcceleration = ri::content::ScriptScalarOr(
        gameplay, "ground_acceleration", state.movementOptions.groundAcceleration);
    state.movementOptions.airAcceleration = ri::content::ScriptScalarOr(
        gameplay, "air_acceleration", state.movementOptions.airAcceleration);
    state.movementOptions.groundFriction = ri::content::ScriptScalarOr(
        gameplay, "ground_friction", state.movementOptions.groundFriction);
    state.movementOptions.stopSpeed = ri::content::ScriptScalarOr(
        gameplay, "stop_speed", state.movementOptions.stopSpeed);
    state.movementOptions.airControl = ri::content::ScriptScalarOrClamped(
        gameplay, "air_control", state.movementOptions.airControl, 0.0f, 1.0f);
    state.movementOptions.coyoteTimeSeconds = ri::content::ScriptScalarOrClamped(
        gameplay, "coyote_time", state.movementOptions.coyoteTimeSeconds, 0.0f, 0.5f);
    state.movementOptions.jumpBufferTimeSeconds = ri::content::ScriptScalarOrClamped(
        gameplay, "jump_buffer_time", state.movementOptions.jumpBufferTimeSeconds, 0.0f, 0.5f);
    state.movementOptions.lowJumpGravityMultiplier = ri::content::ScriptScalarOrClamped(
        gameplay, "low_jump_gravity_multiplier", state.movementOptions.lowJumpGravityMultiplier, 1.0f, 4.0f);
    state.movementOptions.maxFallSpeed = ri::content::ScriptScalarOrClamped(
        gameplay, "max_fall_speed", state.movementOptions.maxFallSpeed, 4.0f, 120.0f);
    state.movementOptions.gravity = ri::content::ScriptScalarOrClamped(
        physics, "movement_gravity", state.movementOptions.gravity, 1.0f, 64.0f);
    state.movementOptions.fallGravityMultiplier = ri::content::ScriptScalarOrClamped(
        physics, "movement_fall_gravity_multiplier", state.movementOptions.fallGravityMultiplier, 0.5f, 4.0f);
    state.mouseSensitivity = ri::content::ScriptScalarOrClamped(
        gameplay, "mouse_sensitivity", state.mouseSensitivity, 0.01f, 2.0f);
    state.cameraHeight = ri::content::ScriptScalarOrClamped(gameplay, "camera_height", 1.62f, 0.8f, 2.2f);
    state.spawnFeet = {
        ri::content::ScriptScalarOr(gameplay, "spawn_x", state.spawnFeet.x),
        ri::content::ScriptScalarOr(gameplay, "spawn_y", state.spawnFeet.y),
        ri::content::ScriptScalarOr(gameplay, "spawn_z", state.spawnFeet.z),
    };
    state.traceScene = ri::trace::TraceScene(state.world.colliders);
    state.movement.body.bounds = BuildPlayerBounds(state.spawnFeet);
    state.movement.onGround = true;
    state.yawDegrees = ri::content::ScriptScalarOrClamped(gameplay, "spawn_yaw", state.yawDegrees, -180.0f, 180.0f);
    state.pitchDegrees = ri::content::ScriptScalarOrClamped(gameplay, "spawn_pitch", state.pitchDegrees, -80.0f, 78.0f);
    state.renderQualityTier = ri::content::ScriptScalarOrIntClamped(postprocess, "postprocess_quality", 2, 0, 3);
    state.renderExposure = ri::content::ScriptScalarOrClamped(postprocess, "native_exposure", 1.02f, 0.5f, 2.5f);
    state.renderContrast = ri::content::ScriptScalarOrClamped(postprocess, "native_contrast", 1.08f, 0.7f, 1.6f);
    state.renderSaturation = ri::content::ScriptScalarOrClamped(postprocess, "native_saturation", 1.0f, 0.0f, 1.8f);
    state.renderFogDensity = ri::content::ScriptScalarOrClamped(postprocess, "native_fog_density", 0.002f, 0.0f, 0.05f);
    state.fogStart = ri::content::ScriptScalarOrClamped(rendering, "fog_start", state.fogStart, 0.0f, 5000.0f);
    state.fogEnd = ri::content::ScriptScalarOrClamped(rendering, "fog_end", state.fogEnd, state.fogStart + 0.1f, 10000.0f);
    state.fogStrength = ri::content::ScriptScalarOrClamped(rendering, "fog_strength", state.fogStrength, 0.0f, 1.0f);
    state.clearTop = {
        ri::content::ScriptScalarOrClamped(rendering, "clear_top_r", state.clearTop.x, 0.0f, 1.0f),
        ri::content::ScriptScalarOrClamped(rendering, "clear_top_g", state.clearTop.y, 0.0f, 1.0f),
        ri::content::ScriptScalarOrClamped(rendering, "clear_top_b", state.clearTop.z, 0.0f, 1.0f),
    };
    state.clearBottom = {
        ri::content::ScriptScalarOrClamped(rendering, "clear_bottom_r", state.clearBottom.x, 0.0f, 1.0f),
        ri::content::ScriptScalarOrClamped(rendering, "clear_bottom_g", state.clearBottom.y, 0.0f, 1.0f),
        ri::content::ScriptScalarOrClamped(rendering, "clear_bottom_b", state.clearBottom.z, 0.0f, 1.0f),
    };
    state.ambientLight = {
        ri::content::ScriptScalarOrClamped(rendering, "ambient_r", state.ambientLight.x, 0.0f, 1.0f),
        ri::content::ScriptScalarOrClamped(rendering, "ambient_g", state.ambientLight.y, 0.0f, 1.0f),
        ri::content::ScriptScalarOrClamped(rendering, "ambient_b", state.ambientLight.z, 0.0f, 1.0f),
    };
    ri::games::BootstrapGamePluginRuntime(state.pluginHost, manifest.rootPath);
    state.pluginHost.renderBoostActive = ri::games::ResolvePluginRenderBoost(
        plugins,
        pluginsPolicy,
        state.pluginHost.session.projectData.manifestEntries.size());
    ri::games::ApplyGamePluginRenderTuning(
        state.pluginHost,
        {
            .qualityTier = &state.renderQualityTier,
            .exposure = &state.renderExposure,
            .ambientLight = &state.ambientLight,
        });
    UpdateCameraNodes(state);

    if (!ri::games::AttachGameSimulationTick(runtime, [&state, &options](const ri::core::FrameContext&) {
            TickPlayState(state);
            if (options.jiggleTest) {
                AnimateCubeTestWorldJiggle(state.world, state.elapsedSeconds);
            } else {
                AnimateCubeTestWorld(state.world, state.elapsedSeconds);
            }
        })) {
        if (error != nullptr) {
            *error = "Failed to attach Cube Test simulation to RuntimeCore.";
        }
        return false;
    }
    if (!ri::games::StartupGameRuntimeCore(runtime, commandLine, error)) {
        return false;
    }
    state.hostInput = ri::runtime::TryGetHostInputService(runtime.Context());

    int frameCount = 0;
    ri::render::ShaderPresentationConfig shaderPresentation{};
    std::vector<std::string> shaderManifestErrors;
    if (!ri::content::TryLoadRawIronShaderPresentation(
            manifest.rootPath, &shaderPresentation, &shaderManifestErrors)
        && !shaderManifestErrors.empty()) {
        ri::core::LogInfo("Cube Test native shader manifest: " + shaderManifestErrors.front());
    }
    const ri::render::vulkan::VulkanPreviewWindowOptions windowOptions{
        .windowTitle = "RawIron Cube Test",
        .presentModePreference = ri::render::vulkan::VulkanPresentModePreference::Auto,
        .textureRoot = textureRoot,
        .cookedTexturePack = cookedTexturePack,
        .messageUserData = &state,
        .onWin32Message = &CubeTestWin32Hook,
        .outClientHwnd = &state.hwnd,
        .showWindow = !options.backgroundWindow,
        .enableHybridHdrPresentation = options.hybridHdr,
        .enableExtendedPostProcessShader = options.extendedPostProcess,
        .initialRenderQualityTier = state.renderQualityTier,
        .shaderPresentation = shaderPresentation,
    };

    int runtimeFrameIndex = 0;
    const ri::render::vulkan::VulkanNativeSceneFrameCallback buildFrame =
        [&state, &textureRoot, &cookedTexturePack, &options, &frameCount, &runtimeFrameIndex, &runtime](
            ri::render::vulkan::VulkanNativeSceneFrame& frame,
            std::string* frameError) {
            const ri::core::FrameContext runtimeFrame = ri::games::BuildGameRuntimeFrameContext(
                runtimeFrameIndex++,
                state.lastDeltaSeconds,
                state.elapsedSeconds,
                static_cast<double>(GetTickCount64()) / 1000.0);
            if (!runtime.Frame(runtimeFrame)) {
                if (frameError != nullptr) {
                    *frameError = std::string(runtime.Context().FailureReason());
                    if (frameError->empty()) {
                        *frameError = "Cube Test RuntimeCore frame failed.";
                    }
                }
                return false;
            }

            frame.scene = &state.world.scene;
            frame.suppressUnchangedFrames = false;
            frame.cameraNode = state.world.playerCameraNode;
            frame.textureRoot = textureRoot;
            frame.cookedTexturePack = cookedTexturePack;
            frame.animationTimeSeconds = state.elapsedSeconds;
            frame.renderQualityTier = state.renderQualityTier;
            frame.renderExposure = state.renderExposure;
            frame.renderContrast = state.renderContrast;
            frame.renderSaturation = state.renderSaturation;
            frame.renderFogDensity = state.renderFogDensity;
            frame.renderFogStart = state.fogStart;
            frame.renderFogEnd = state.fogEnd;
            frame.renderFogStrength = state.fogStrength;
            frame.useEnvironmentClear = true;
            frame.environmentClearTop = state.clearTop;
            frame.environmentClearBottom = state.clearBottom;
            frame.nativeAmbientLight = state.ambientLight;

            if (options.benchmarkFrames > 0 && ++frameCount >= options.benchmarkFrames && state.hwnd != nullptr) {
                PostMessageW(state.hwnd, WM_CLOSE, 0, 0);
            }
            return true;
        };

    const bool ok = ri::render::vulkan::RunVulkanNativeSceneLoop(
        options.width,
        options.height,
        buildFrame,
        windowOptions,
        error);
    ShowCursor(TRUE);
    ReleaseCapture();
    return ok;
}
#endif

} // namespace

bool RunStandalone(const StandaloneOptions& options,
                   const ri::core::CommandLine& commandLine,
                   std::string* error) {
    fs::path executablePath{};
#if defined(_WIN32)
    wchar_t moduleWide[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, moduleWide, MAX_PATH) > 0) {
        executablePath = fs::path(std::wstring(moduleWide));
    }
#endif
    fs::path workspaceRoot = options.workspaceRoot;
    if (workspaceRoot.empty()) {
        workspaceRoot = ri::content::DetectWorkspaceRoot(fs::current_path());
        if (!ri::content::LooksLikeWorkspaceRoot(workspaceRoot) && !executablePath.empty()) {
            const fs::path executableWorkspace = ri::content::DetectWorkspaceRoot(executablePath);
            if (ri::content::LooksLikeWorkspaceRoot(executableWorkspace)) {
                workspaceRoot = executableWorkspace;
            }
        }
    }
    const fs::path textureRoot = ri::content::PickEngineTexturesDirectory(workspaceRoot, executablePath);
    const std::shared_ptr<const ri::content::CookedTexturePack> cookedTexturePack =
        TryMountRawIronX32(workspaceRoot, textureRoot);

    const std::optional<ri::content::GameManifest> manifest =
        ResolveStandaloneGameManifest(options, workspaceRoot);
    if (!manifest.has_value()) {
        if (error != nullptr) {
            *error = "Unable to resolve Cube Test manifest for game id '" + options.gameId + "'.";
        }
        return false;
    }
    const std::vector<std::string> formatIssues = ri::content::ValidateGameProjectFormat(*manifest);
    if (!formatIssues.empty()) {
        if (error != nullptr) {
            *error = "Cube Test project format validation failed:";
            for (const std::string& issue : formatIssues) {
                *error += " " + issue;
            }
        }
        return false;
    }
    std::string contractError;
    if (!ri::games::EnforceGameConfigContracts(
            manifest->rootPath,
            ri::games::GameConfigContractOptions{.mode = ri::games::GameConfigContractMode::Balanced},
            &contractError)) {
        if (error != nullptr) {
            *error = contractError;
        }
        return false;
    }

    auto manifestService = std::make_shared<ri::content::GameManifest>(*manifest);
    auto supportService = std::make_shared<ri::content::GameRuntimeSupportData>(
        ri::content::LoadGameRuntimeSupportData(manifest->rootPath));
    ri::games::LogGameRuntimeSupportSummary(*supportService);

    CubeTestWorld previewWorld = BuildCubeTestWorld("Cube Test", workspaceRoot);
    if (cookedTexturePack) {
        ConfigureCookedTextureCube(previewWorld, CubeTestCookedTextureSequence());
    }
    const std::string hiddenPreviewNode =
        commandLine.GetValue("--preview-hide-node").value_or(std::string{});
    if (commandLine.HasFlag("--save-jiggle-preview") || options.jigglePreviewFrames > 0) {
        const int frames = options.jigglePreviewFrames > 0 ? options.jigglePreviewFrames : 8;
        return SaveJigglePreviewSequence(
            textureRoot,
            cookedTexturePack,
            workspaceRoot,
            ResolvePreviewOutputPath(commandLine),
            frames,
            hiddenPreviewNode,
            error);
    }
    if (commandLine.HasFlag("--save-preview")) {
        return SavePreview(
            previewWorld,
            textureRoot,
            cookedTexturePack,
            ResolvePreviewOutputPath(commandLine),
            0.0,
            hiddenPreviewNode,
            nullptr,
            error);
    }

#if defined(_WIN32)
    ri::runtime::RuntimeCore runtime = ri::games::CreateGameRuntimeCore(
        *manifest,
        "RawIron.Game.CubeTest",
        ri::games::BuildGameRuntimePaths(*manifest, workspaceRoot),
        ri::games::GameRuntimeBootServices{
            .manifest = std::move(manifestService),
            .support = std::move(supportService),
        });
    ri::core::LogInfo(
        "Cube Test controls: WASD move, Shift sprint, Space jump, right mouse drag or arrow keys look, Esc quit.");
    const bool ok = RunNativeLoop(
        options, *manifest, textureRoot, cookedTexturePack, runtime, commandLine, error);
    runtime.Shutdown();
    return ok;
#else
    if (error != nullptr) {
        *error = "Cube Test standalone currently requires the Windows Vulkan preview loop.";
    }
    return false;
#endif
}

} // namespace ri::games::cubetest
