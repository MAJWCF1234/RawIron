#include "RawIron/Games/CubeTest/CubeTestRuntime.h"

#include "RawIron/Content/EngineAssets.h"
#include "RawIron/Content/GameManifest.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Games/CubeTest/CubeTestWorld.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Render/SoftwarePreview.h"
#include "RawIron/Render/VulkanPreviewPresenter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <optional>

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

fs::path ResolvePreviewOutputPath(const ri::core::CommandLine& commandLine) {
    if (const std::optional<std::string> output = commandLine.GetValue("--output"); output.has_value() && !output->empty()) {
        return fs::path(*output);
    }
    return fs::current_path() / "cube_test_preview.bmp";
}

bool SavePreview(const CubeTestWorld& world,
                 const fs::path& textureRoot,
                 const fs::path& outputPath,
                 std::string* error) {
    ri::render::software::ScenePreviewOptions options{};
    options.width = 960;
    options.height = 540;
    options.textureRoot = textureRoot;
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

    ri::render::software::ScenePreviewCache cache{};
    const ri::render::software::SoftwareImage image =
        ri::render::software::RenderScenePreview(world.scene, world.playerCameraNode, options, &cache);
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

#if defined(_WIN32)
struct PlayState {
    CubeTestWorld world{};
    HWND hwnd = nullptr;
    bool mouseLook = false;
    POINT lastMouse{};
    ri::math::Vec3 position{0.0f, 1.82f, -7.4f};
    float yawDegrees = 0.0f;
    float pitchDegrees = -5.0f;
    std::chrono::steady_clock::time_point lastTick{};
};

float KeyAxis(const int positive, const int negative) {
    const float plus = (GetAsyncKeyState(positive) & 0x8000) != 0 ? 1.0f : 0.0f;
    const float minus = (GetAsyncKeyState(negative) & 0x8000) != 0 ? 1.0f : 0.0f;
    return plus - minus;
}

void UpdateCameraNodes(PlayState& state) {
    ri::scene::Node& rig = state.world.scene.GetNode(state.world.playerRig);
    rig.localTransform.position = state.position;
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

    if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0 && state.hwnd != nullptr) {
        PostMessageW(state.hwnd, WM_CLOSE, 0, 0);
    }

    state.yawDegrees += KeyAxis(VK_RIGHT, VK_LEFT) * 92.0f * deltaSeconds;
    state.pitchDegrees = std::clamp(state.pitchDegrees + KeyAxis(VK_DOWN, VK_UP) * 72.0f * deltaSeconds, -80.0f, 78.0f);

    const float yawRadians = ri::math::DegreesToRadians(state.yawDegrees);
    const ri::math::Vec3 forward{std::sin(yawRadians), 0.0f, std::cos(yawRadians)};
    const ri::math::Vec3 right{std::cos(yawRadians), 0.0f, -std::sin(yawRadians)};
    ri::math::Vec3 wish = forward * KeyAxis('W', 'S') + right * KeyAxis('D', 'A');
    if (ri::math::LengthSquared(wish) > 0.0001f) {
        wish = ri::math::Normalize(wish);
        const float speed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 ? 7.5f : 4.2f;
        state.position = state.position + (wish * speed * deltaSeconds);
        state.position.x = std::clamp(state.position.x, -7.25f, 7.25f);
        state.position.z = std::clamp(state.position.z, -7.25f, 7.25f);
    }

    UpdateCameraNodes(state);
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
            state->yawDegrees += static_cast<float>(dx) * 0.12f;
            state->pitchDegrees = std::clamp(state->pitchDegrees + static_cast<float>(dy) * 0.10f, -80.0f, 78.0f);
        }
        break;
    default:
        break;
    }
    (void)lParam;
}

bool RunNativeLoop(const StandaloneOptions& options,
                   const fs::path& textureRoot,
                   std::string* error) {
    PlayState state{};
    state.world = BuildCubeTestWorld("Cube Test");
    UpdateCameraNodes(state);

    int frameCount = 0;
    const ri::render::vulkan::VulkanPreviewWindowOptions windowOptions{
        .windowTitle = "RawIron Cube Test",
        .presentModePreference = ri::render::vulkan::VulkanPresentModePreference::Auto,
        .textureRoot = textureRoot,
        .messageUserData = &state,
        .onWin32Message = &CubeTestWin32Hook,
        .outClientHwnd = &state.hwnd,
        .enableHybridHdrPresentation = options.hybridHdr,
        .initialRenderQualityTier = 2,
    };

    const auto started = std::chrono::steady_clock::now();
    const ri::render::vulkan::VulkanNativeSceneFrameCallback buildFrame =
        [&state, &textureRoot, &options, &frameCount, started](ri::render::vulkan::VulkanNativeSceneFrame& frame, std::string*) {
            TickPlayState(state);
            const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            AnimateCubeTestWorld(state.world, elapsed);

            frame.scene = &state.world.scene;
            frame.cameraNode = state.world.playerCameraNode;
            frame.textureRoot = textureRoot;
            frame.animationTimeSeconds = elapsed;
            frame.renderQualityTier = 2;
            frame.renderExposure = 1.02f;
            frame.renderContrast = 1.08f;
            frame.renderSaturation = 1.0f;
            frame.renderFogDensity = 0.002f;
            frame.renderFogStart = 22.0f;
            frame.renderFogEnd = 96.0f;
            frame.renderFogStrength = 0.20f;
            frame.useEnvironmentClear = true;
            frame.environmentClearTop = {0.50f, 0.62f, 0.70f};
            frame.environmentClearBottom = {0.30f, 0.34f, 0.34f};
            frame.nativeAmbientLight = {0.34f, 0.36f, 0.34f};

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
    const fs::path workspaceRoot = options.workspaceRoot.empty()
        ? ri::content::DetectWorkspaceRoot(fs::current_path())
        : options.workspaceRoot;
    const fs::path textureRoot = ri::content::PickEngineTexturesDirectory(workspaceRoot, executablePath);

    CubeTestWorld previewWorld = BuildCubeTestWorld("Cube Test");
    if (commandLine.HasFlag("--save-preview")) {
        return SavePreview(previewWorld, textureRoot, ResolvePreviewOutputPath(commandLine), error);
    }

#if defined(_WIN32)
    ri::core::LogInfo("Cube Test controls: WASD move, Shift sprint, right mouse drag or arrow keys look, Esc quit.");
    return RunNativeLoop(options, textureRoot, error);
#else
    if (error != nullptr) {
        *error = "Cube Test standalone currently requires the Windows Vulkan preview loop.";
    }
    return false;
#endif
}

} // namespace ri::games::cubetest
