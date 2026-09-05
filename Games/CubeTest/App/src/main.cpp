#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Games/CubeTest/CubeTestRuntime.h"
#include "RawIron/Games/CubeTest/CubeTestGallery.h"

#include <algorithm>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    const ri::core::CommandLine commandLine(argc, argv);
    if (commandLine.HasFlag("--gallery-help")) {
        ri::core::LogInfo(ri::games::cubetest::CubeTestGalleryHelp());
        return 0;
    }
    if (commandLine.HasFlag("--help") || commandLine.HasFlag("-h")) {
        ri::core::LogInfo("RawIron.CubeTestGame options:");
        ri::core::LogInfo("  --game-root=<path>              Direct game root containing manifest.json");
        ri::core::LogInfo("  --workspace-root=<path>         Workspace root");
        ri::core::LogInfo("  --width=<px> --height=<px>      Window size");
        ri::core::LogInfo("  --benchmark-frames=<n>          Auto-exit after n rendered frames");
        ri::core::LogInfo("  --frame-times=<csv>             Record CPU present intervals; requires 2..100000 benchmark frames");
        ri::core::LogInfo("  --background                    Keep the owned native Vulkan window hidden");
        ri::core::LogInfo("  --normal-comparison             Neutral native GL / DX-converted / DX-unconverted rows; second row mirrors U");
        ri::core::LogInfo("  --material-calibration          Static isolated material fixture with neutral render settings");
        ri::core::LogInfo("  --gallery-help                  Print all room guides; F1 shows the current room in play");
        ri::core::LogInfo("  --capture-native=<bmp>          Save first GPU frame and exit; works with --background");
        ri::core::LogInfo("  --cooked-texture-demo           Opt into the separate RAWIRONX32 streaming test");
        ri::core::LogInfo("  --jiggle-test                   Rotate/bob material samples to stress wobble/shimmer");
        ri::core::LogInfo("  --save-jiggle-preview           Save a deterministic jiggle preview sequence and exit");
        ri::core::LogInfo("  --jiggle-frames=<n>             Frames for --save-jiggle-preview (default: 8)");
        ri::core::LogInfo("  --hybrid-hdr                    Enable experimental native hybrid HDR/G-buffer presentation");
        ri::core::LogInfo("  --no-hybrid-hdr                 Use the stable direct native Vulkan PBR path (default)");
        ri::core::LogInfo("  --extended-post                 Enable the full advanced shader-port stack");
        ri::core::LogInfo("  --save-preview                  Render a still preview and exit");
        ri::core::LogInfo("  --output=<path>                 Output path for --save-preview");
        ri::core::LogInfo("  --preview-hide-node=<name>      Hide one named node in headless preview captures");
        ri::core::LogInfo("  --export-gltf=<path>            Export the native capability gallery as glTF 2.0");
        ri::core::LogInfo("  --start-room=<name>             Choose a room listed by --gallery-help");
        ri::core::LogInfo("  --net-mode=<mode>               offline (default), listen, dedicated, or client");
        ri::core::LogInfo("  --port=<n> --connect-host=<h> --connect-port=<n>  Authority session endpoint");
        return 0;
    }

    ri::games::cubetest::StandaloneOptions options{};
    options.normalComparison = commandLine.HasFlag("--normal-comparison");
    options.materialCalibration = commandLine.HasFlag("--material-calibration") || options.normalComparison;
    if (const auto capture = commandLine.GetValue("--capture-native")) {
        options.nativeCapturePath = *capture;
    }
    options.cookedTextureDemo = commandLine.HasFlag("--cooked-texture-demo");
    if (const auto timings = commandLine.GetValue("--frame-times")) options.frameTimesPath = *timings;
    if (const auto gameRoot = commandLine.GetValue("--game-root"); gameRoot.has_value() && !gameRoot->empty()) {
        options.gameRoot = std::filesystem::path(*gameRoot);
    }
    if (const auto workspaceRoot = commandLine.GetValue("--workspace-root"); workspaceRoot.has_value() && !workspaceRoot->empty()) {
        options.workspaceRoot = std::filesystem::path(*workspaceRoot);
    }
    if (const auto exportPath = commandLine.GetValue("--export-gltf"); exportPath.has_value() && !exportPath->empty()) {
        options.exportGltfPath = std::filesystem::path(*exportPath);
    }
    if (const auto startRoom = commandLine.GetValue("--start-room"); startRoom.has_value() && !startRoom->empty()) {
        options.startRoom = *startRoom;
    }
    options.width = std::clamp(commandLine.GetIntOr("--width", options.width), 64, 3840);
    options.height = std::clamp(commandLine.GetIntOr("--height", options.height), 64, 2160);
    options.benchmarkFrames = std::max(0, commandLine.GetIntOr("--benchmark-frames", options.benchmarkFrames));
    if (!options.nativeCapturePath.empty() && options.benchmarkFrames == 0) options.benchmarkFrames = 1;
    options.backgroundWindow = commandLine.HasFlag("--background");
    options.extendedPostProcess = commandLine.HasFlag("--extended-post");
    options.jiggleTest = commandLine.HasFlag("--jiggle-test");
    options.jigglePreviewFrames = std::max(0, commandLine.GetIntOr("--jiggle-frames", options.jigglePreviewFrames));
    if (commandLine.HasFlag("--hybrid-hdr")) {
        options.hybridHdr = true;
    }
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
