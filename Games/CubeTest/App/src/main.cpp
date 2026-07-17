#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Games/CubeTest/CubeTestRuntime.h"

#include <algorithm>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    const ri::core::CommandLine commandLine(argc, argv);
    if (commandLine.HasFlag("--help") || commandLine.HasFlag("-h")) {
        ri::core::LogInfo("RawIron.CubeTestGame options:");
        ri::core::LogInfo("  --game-root=<path>              Direct game root containing manifest.json");
        ri::core::LogInfo("  --workspace-root=<path>         Workspace root");
        ri::core::LogInfo("  --width=<px> --height=<px>      Window size");
        ri::core::LogInfo("  --benchmark-frames=<n>          Auto-exit after n rendered frames");
        ri::core::LogInfo("  --background                    Keep the owned native Vulkan window hidden");
        ri::core::LogInfo("  --jiggle-test                   Rotate/bob material samples to stress wobble/shimmer");
        ri::core::LogInfo("  --save-jiggle-preview           Save a deterministic jiggle preview sequence and exit");
        ri::core::LogInfo("  --jiggle-frames=<n>             Frames for --save-jiggle-preview (default: 8)");
        ri::core::LogInfo("  --hybrid-hdr                    Enable native hybrid HDR/G-buffer presentation (default: on)");
        ri::core::LogInfo("  --no-hybrid-hdr                 Disable hybrid HDR");
        ri::core::LogInfo("  --extended-post                 Enable the full advanced shader-port stack");
        ri::core::LogInfo("  --save-preview                  Render a still preview and exit");
        ri::core::LogInfo("  --output=<path>                 Output path for --save-preview");
        ri::core::LogInfo("  --preview-hide-node=<name>      Hide one named node in headless preview captures");
        return 0;
    }

    ri::games::cubetest::StandaloneOptions options{};
    if (const auto gameRoot = commandLine.GetValue("--game-root"); gameRoot.has_value() && !gameRoot->empty()) {
        options.gameRoot = std::filesystem::path(*gameRoot);
    }
    if (const auto workspaceRoot = commandLine.GetValue("--workspace-root"); workspaceRoot.has_value() && !workspaceRoot->empty()) {
        options.workspaceRoot = std::filesystem::path(*workspaceRoot);
    }
    options.width = std::clamp(commandLine.GetIntOr("--width", options.width), 64, 3840);
    options.height = std::clamp(commandLine.GetIntOr("--height", options.height), 64, 2160);
    options.benchmarkFrames = std::max(0, commandLine.GetIntOr("--benchmark-frames", options.benchmarkFrames));
    options.backgroundWindow = commandLine.HasFlag("--background");
    options.extendedPostProcess = commandLine.HasFlag("--extended-post");
    options.jiggleTest = commandLine.HasFlag("--jiggle-test");
    options.jigglePreviewFrames = std::max(0, commandLine.GetIntOr("--jiggle-frames", options.jigglePreviewFrames));
    if (commandLine.HasFlag("--no-hybrid-hdr")) {
        options.hybridHdr = false;
    }

    std::string error;
    if (!ri::games::cubetest::RunStandalone(options, commandLine, &error)) {
        if (!error.empty()) {
            ri::core::LogSection("RawIron Cube Test Failure");
            ri::core::LogInfo(error);
        }
        return 1;
    }
    return 0;
}
