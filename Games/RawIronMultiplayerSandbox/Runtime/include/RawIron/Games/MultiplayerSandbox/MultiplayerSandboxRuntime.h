#pragma once

#include "RawIron/Core/CommandLine.h"

#include <filesystem>
#include <optional>
#include <string>

namespace ri::games::multiplayersandbox {

struct StandaloneOptions {
    std::filesystem::path workspaceRoot{};
    std::filesystem::path gameRoot{};
    std::string gameId = "rawiron-multiplayer-sandbox";
    int frames = 1800;
    int tickHz = 60;
    int netTick = 60;
    int serverTick = 125;
    int maxPeers = 128;
    int bots = 24;
    bool botsReliable = false;
    bool issueJoinCode = true;
    bool simulateNet = false;
    int simDelayMs = 0;
    int simJitterMs = 0;
    int simLossPct = 0;
    std::string netMode = "listen";
    std::string rendezvous = "eos";
    std::optional<std::string> joinCode{};
    bool use3DStandalone = true;
    int width = 1280;
    int height = 720;
    int benchmarkFrames = 0;
    bool hybridHdr = true;
};

bool RunStandalone(const StandaloneOptions& options,
                   const ri::core::CommandLine& commandLine,
                   std::string* error);

bool RunStandalone3D(const StandaloneOptions& options,
                     const ri::core::CommandLine& commandLine,
                     std::string* error);

} // namespace ri::games::multiplayersandbox
