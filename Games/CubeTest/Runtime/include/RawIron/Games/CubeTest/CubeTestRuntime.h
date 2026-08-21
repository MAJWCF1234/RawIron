#pragma once

#include "RawIron/Core/CommandLine.h"

#include <filesystem>
#include <string>

namespace ri::games::cubetest {

struct StandaloneOptions {
    std::filesystem::path workspaceRoot{};
    std::filesystem::path gameRoot{};
    std::filesystem::path exportGltfPath{};
    std::string gameId = "cube-test";
    std::string startRoom = "baseline";
    int width = 1280;
    int height = 720;
    int benchmarkFrames = 0;
    bool hybridHdr = true;
    bool backgroundWindow = false;
    bool extendedPostProcess = false;
    bool jiggleTest = false;
    int jigglePreviewFrames = 0;
};

bool RunStandalone(const StandaloneOptions& options,
                   const ri::core::CommandLine& commandLine,
                   std::string* error);

} // namespace ri::games::cubetest
