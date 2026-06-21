#pragma once

#include "RawIron/Core/CommandLine.h"

#include <filesystem>
#include <string>

namespace ri::games::cubetest {

struct StandaloneOptions {
    std::filesystem::path workspaceRoot{};
    std::filesystem::path gameRoot{};
    std::string gameId = "cube-test";
    int width = 1280;
    int height = 720;
    int benchmarkFrames = 0;
    bool hybridHdr = true;
};

bool RunStandalone(const StandaloneOptions& options,
                   const ri::core::CommandLine& commandLine,
                   std::string* error);

} // namespace ri::games::cubetest
