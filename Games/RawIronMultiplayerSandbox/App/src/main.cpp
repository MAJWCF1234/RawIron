#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Games/MultiplayerSandbox/MultiplayerSandboxRuntime.h"

#include <algorithm>
#include <filesystem>

int main(int argc, char** argv) {
    const ri::core::CommandLine commandLine(argc, argv);
    if (commandLine.HasFlag("--help") || commandLine.HasFlag("-h")) {
        ri::core::LogInfo("RawIron.MultiplayerSandboxGame options:");
        ri::core::LogInfo("  --runtime-mode=play|net         play=3D game host (default), net=headless net harness");
        ri::core::LogInfo("  --game=<id>                     Game manifest id (default: rawiron-multiplayer-sandbox)");
        ri::core::LogInfo("  --game-root=<path>              Direct game root containing manifest.json");
        ri::core::LogInfo("  --workspace-root=<path>         Workspace root");
        ri::core::LogInfo("  --frames=<n>                    Runtime frames to simulate (default: 1800)");
        ri::core::LogInfo("  --width=<px> --height=<px>      3D play mode window size");
        ri::core::LogInfo("  --benchmark-frames=<n>          3D play mode benchmark auto-exit");
        ri::core::LogInfo("  --hybrid-hdr                    Enable native hybrid HDR/G-buffer presentation (default: on)");
        ri::core::LogInfo("  --no-hybrid-hdr                 Disable hybrid HDR (forward-only presentation)");
        ri::core::LogInfo("  --save-preview                  Render a sandbox still and exit");
        ri::core::LogInfo("  --output=<path>                 Output path for --save-preview");
        ri::core::LogInfo("  --tick-hz=<n>                   Core tick rate (default: 60)");
        ri::core::LogInfo("  --net-mode=dedicated|listen|hybrid|client");
        ri::core::LogInfo("  --rendezvous=eos|direct         Join-code provider");
        ri::core::LogInfo("  --issue-join-code               Host emits join code at startup");
        ri::core::LogInfo("  --join-code=<code>              Resolve and connect by code");
        ri::core::LogInfo("  --port=<n> --connect-host=<h> --connect-port=<n>");
        ri::core::LogInfo("  --max-peers=<n> --bots=<n> --bot-reliable");
        ri::core::LogInfo("  --sim-net --sim-delay-ms=<n> --sim-jitter-ms=<n> --sim-loss-pct=<n>");
        return 0;
    }

    ri::games::multiplayersandbox::StandaloneOptions options{};
    if (const auto game = commandLine.GetValue("--game"); game.has_value() && !game->empty()) {
        options.gameId = *game;
    }
    if (const auto gameRoot = commandLine.GetValue("--game-root"); gameRoot.has_value() && !gameRoot->empty()) {
        options.gameRoot = std::filesystem::path(*gameRoot);
    }
    if (const auto workspaceRoot = commandLine.GetValue("--workspace-root"); workspaceRoot.has_value() && !workspaceRoot->empty()) {
        options.workspaceRoot = std::filesystem::path(*workspaceRoot);
    }

    options.frames = std::max(1, commandLine.GetIntOr("--frames", options.frames));
    options.width = std::clamp(commandLine.GetIntOr("--width", options.width), 64, 3840);
    options.height = std::clamp(commandLine.GetIntOr("--height", options.height), 64, 2160);
    options.benchmarkFrames = std::max(0, commandLine.GetIntOr("--benchmark-frames", options.benchmarkFrames));
    if (commandLine.HasFlag("--no-hybrid-hdr")) {
        options.hybridHdr = false;
    }
    if (const auto runtimeMode = commandLine.GetValue("--runtime-mode"); runtimeMode.has_value() && !runtimeMode->empty()) {
        options.use3DStandalone = (*runtimeMode != "net");
    }
    ri::core::LogInfo(std::string("Sandbox runtime mode: ") + (options.use3DStandalone ? "play" : "net"));
    options.tickHz = std::max(1, commandLine.GetIntOr("--tick-hz", options.tickHz));
    options.netTick = std::max(1, commandLine.GetIntOr("--net-tick", options.netTick));
    options.serverTick = std::max(1, commandLine.GetIntOr("--server-tick", options.serverTick));
    options.maxPeers = std::max(1, commandLine.GetIntOr("--max-peers", options.maxPeers));
    options.bots = std::max(0, commandLine.GetIntOr("--bots", options.bots));
    options.botsReliable = commandLine.HasFlag("--bot-reliable");
    options.issueJoinCode = commandLine.HasFlag("--issue-join-code");
    options.simulateNet = commandLine.HasFlag("--sim-net");
    options.simDelayMs = std::max(0, commandLine.GetIntOr("--sim-delay-ms", options.simDelayMs));
    options.simJitterMs = std::max(0, commandLine.GetIntOr("--sim-jitter-ms", options.simJitterMs));
    options.simLossPct = std::clamp(commandLine.GetIntOr("--sim-loss-pct", options.simLossPct), 0, 100);
    if (const auto netMode = commandLine.GetValue("--net-mode"); netMode.has_value() && !netMode->empty()) {
        options.netMode = *netMode;
    }
    if (const auto rendezvous = commandLine.GetValue("--rendezvous"); rendezvous.has_value() && !rendezvous->empty()) {
        options.rendezvous = *rendezvous;
    }
    if (const auto joinCode = commandLine.GetValue("--join-code"); joinCode.has_value() && !joinCode->empty()) {
        options.joinCode = *joinCode;
    }

    std::string error;
    if (!ri::games::multiplayersandbox::RunStandalone(options, commandLine, &error)) {
        if (!error.empty()) {
            ri::core::LogSection("RawIron Multiplayer Sandbox Failure");
            ri::core::LogInfo(error);
        }
        return 1;
    }
    return 0;
}
