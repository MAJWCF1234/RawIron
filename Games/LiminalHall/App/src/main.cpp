#include "RawIron/Content/GameManifest.h"
#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Games/LiminalHall/LiminalHallBenchmark.h"
#include "RawIron/Games/LiminalHall/LiminalHallWorld.h"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>

namespace {

namespace fs = std::filesystem;

bool LooksLikeWorkspaceRoot(const fs::path& path) {
    std::error_code ec{};
    return fs::exists(path / "CMakeLists.txt", ec)
        && fs::exists(path / "Source", ec)
        && fs::exists(path / "Games", ec);
}

fs::path ResolveWorkspaceRoot(const std::optional<std::string>& workspaceRootArg,
                              const std::optional<std::string>& gameRootArg) {
    if (workspaceRootArg.has_value() && !workspaceRootArg->empty()) {
        return fs::weakly_canonical(fs::path(*workspaceRootArg));
    }

    if (gameRootArg.has_value() && !gameRootArg->empty()) {
        const fs::path explicitGameRoot = fs::weakly_canonical(fs::path(*gameRootArg));
        const fs::path detectedWorkspace = ri::content::DetectWorkspaceRoot(explicitGameRoot);
        if (LooksLikeWorkspaceRoot(detectedWorkspace)) {
            return detectedWorkspace;
        }
        return explicitGameRoot;
    }

    return ri::content::DetectWorkspaceRoot(fs::current_path());
}

} // namespace

int main(int argc, char** argv) {
    const ri::core::CommandLine commandLine(argc, argv);
    if (commandLine.HasFlag("--help") || commandLine.HasFlag("-h")) {
        ri::core::LogInfo("RawIron.LiminalGame options:");
        ri::core::LogInfo("  --game=<id>                 Game manifest id (default: liminal-hall)");
        ri::core::LogInfo("  --game-root=<path>          Direct path containing manifest.json");
        ri::core::LogInfo("  --workspace-root=<path>     Workspace root for manifest/assets lookup");
        ri::core::LogInfo("  --width=<px> --height=<px>  Window client size (interactive default 2560x1440)");
        ri::core::LogInfo("  --renderer=vulkan|vulkan-native  Aliases for native Vulkan swapchain (default: vulkan)");
        ri::core::LogInfo("  --present-mode=auto|mailbox|immediate|fifo  Swapchain present mode override (default: auto)");
        ri::core::LogInfo("  --render-quality=competitive|balanced|cinematic  Native Vulkan quality tier (default: balanced)");
        ri::core::LogInfo("  --benchmark-frames=<n>      Auto-exit standalone after n presented frames and log FPS");
        ri::core::LogInfo("  --window-title=<text>       Override window title");
        ri::core::LogInfo("  --mouse-sensitivity=<float> Degrees-per-pixel override");
        ri::core::LogInfo("  --no-mouse-capture          Disable recenter/capture for debugging");
        ri::core::LogInfo("  --start-from-checkpoint     Resume startup from persisted checkpoint slot if present");
        ri::core::LogInfo("  --checkpoint-slot=<id>      Checkpoint slot id (default: autosave)");
        ri::core::LogInfo("  --resume-query=<query>      URL-style query string (?startFromCheckpoint=1&checkpointSlot=...)");
        ri::core::LogInfo("  --checkpoint-storage-root=<path>  Override Saved/Checkpoints root directory");
        ri::core::LogInfo("  --bench                     Headless CPU software-render speed test (Liminal Hall scene)");
        ri::core::LogInfo("  --bench-frames=<n>          Timed frames for --bench (default 90, max 600)");
        return 0;
    }
    ri::games::liminal::StandaloneOptions options{};
    if (const auto game = commandLine.GetValue("--game"); game.has_value() && !game->empty()) {
        options.gameId = *game;
    }
    const auto gameRoot = commandLine.GetValue("--game-root");
    const auto workspaceRoot = commandLine.GetValue("--workspace-root");
    if (gameRoot.has_value() && !gameRoot->empty()) {
        options.gameRoot = std::filesystem::path(*gameRoot);
    }
    options.workspaceRoot = ResolveWorkspaceRoot(workspaceRoot, gameRoot);
    options.width = std::clamp(commandLine.GetIntOr("--width", options.width), 64, 3840);
    options.height = std::clamp(commandLine.GetIntOr("--height", options.height), 64, 2160);
    const bool userSetViewportWidth = commandLine.GetValue("--width").has_value();
    const bool userSetViewportHeight = commandLine.GetValue("--height").has_value();
    if (const auto renderer = commandLine.GetValue("--renderer"); renderer.has_value() && !renderer->empty()) {
        if (*renderer == "vulkan" || *renderer == "vulkan-native") {
            options.renderer = ri::games::liminal::StandaloneRenderer::VulkanNative;
        } else {
            ri::core::LogInfo("Invalid --renderer value; using default native Vulkan presenter.");
        }
    }
    if (const auto presentMode = commandLine.GetValue("--present-mode");
        presentMode.has_value() && !presentMode->empty()) {
        if (*presentMode == "auto") {
            options.presentMode = ri::games::liminal::StandalonePresentMode::Auto;
        } else if (*presentMode == "mailbox") {
            options.presentMode = ri::games::liminal::StandalonePresentMode::Mailbox;
        } else if (*presentMode == "immediate") {
            options.presentMode = ri::games::liminal::StandalonePresentMode::Immediate;
        } else if (*presentMode == "fifo" || *presentMode == "vsync") {
            options.presentMode = ri::games::liminal::StandalonePresentMode::Fifo;
        } else {
            ri::core::LogInfo("Invalid --present-mode value; using auto.");
        }
    }
    if (const auto renderQuality = commandLine.GetValue("--render-quality");
        renderQuality.has_value() && !renderQuality->empty()) {
        if (*renderQuality == "competitive") {
            options.renderQuality = ri::games::liminal::RenderQuality::Competitive;
        } else if (*renderQuality == "balanced") {
            options.renderQuality = ri::games::liminal::RenderQuality::Balanced;
        } else if (*renderQuality == "cinematic") {
            options.renderQuality = ri::games::liminal::RenderQuality::Cinematic;
        } else {
            ri::core::LogInfo("Invalid --render-quality value; using balanced.");
        }
    }
    options.benchmarkFrames = std::max(0, commandLine.GetIntOr("--benchmark-frames", 0));
    if (const auto windowTitle = commandLine.GetValue("--window-title");
        windowTitle.has_value() && !windowTitle->empty()) {
        options.windowTitle = *windowTitle;
    }
    options.captureMouse = !commandLine.HasFlag("--no-mouse-capture");
    options.startFromCheckpoint = commandLine.HasFlag("--start-from-checkpoint");
    if (const auto checkpointSlot = commandLine.GetValue("--checkpoint-slot");
        checkpointSlot.has_value() && !checkpointSlot->empty()) {
        options.checkpointSlot = *checkpointSlot;
    }
    if (const auto resumeQuery = commandLine.GetValue("--resume-query");
        resumeQuery.has_value() && !resumeQuery->empty()) {
        options.resumeQuery = *resumeQuery;
    }
    if (const auto checkpointStorageRoot = commandLine.GetValue("--checkpoint-storage-root");
        checkpointStorageRoot.has_value() && !checkpointStorageRoot->empty()) {
        options.checkpointStorageRoot = std::filesystem::path(*checkpointStorageRoot);
    }
    if (const auto mouseSensitivity = commandLine.GetValue("--mouse-sensitivity");
        mouseSensitivity.has_value() && !mouseSensitivity->empty()) {
        try {
            options.mouseSensitivityDegreesPerPixel = std::stof(*mouseSensitivity);
        } catch (...) {
            ri::core::LogInfo("Invalid --mouse-sensitivity value; using script/default.");
        }
    }
    const bool headless = commandLine.HasFlag("--headless");
    const bool bench = commandLine.HasFlag("--bench");

    // Showcase defaults for interactive play: 1440p unless the user explicitly overrides it.
    if (!headless && !bench) {
        if (!userSetViewportWidth && !userSetViewportHeight) {
            options.width = 2560;
            options.height = 1440;
        }
    }

    std::string error;
    if (bench) {
        const int benchWidth = std::clamp(commandLine.GetIntOr("--width", 640), 64, 3840);
        const int benchHeight = std::clamp(commandLine.GetIntOr("--height", 360), 64, 2160);
        const int benchFramesRaw = commandLine.GetIntOr("--bench-frames", 90);
        const std::uint32_t benchTimed =
            static_cast<std::uint32_t>(std::clamp(benchFramesRaw, 10, 600));
        std::string report;
        if (!ri::games::liminal::RunLiminalHallSoftwareRenderBenchmark(
                options.workspaceRoot, benchWidth, benchHeight, 8U, benchTimed, &report, &error)) {
            if (!error.empty()) {
                ri::core::LogSection("Benchmark failure");
                ri::core::LogInfo(error);
            }
            return 1;
        }
        ri::core::LogSection("Software render benchmark");
        ri::core::LogInfo(report);
        return 0;
    }
    if (headless) {
        ri::core::LogSection("Liminal Game Failure");
        ri::core::LogInfo("Headless software capture has been retired. Standalone is Vulkan-native only.");
        return 1;
    }
    if (!ri::games::liminal::RunStandalone(options, &error)) {
        if (!error.empty()) {
            ri::core::LogSection("Liminal Game Failure");
            ri::core::LogInfo(error);
        }
        return 1;
    }
    return 0;
}
