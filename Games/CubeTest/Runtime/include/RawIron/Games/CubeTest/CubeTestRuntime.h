#pragma once

#include "RawIron/Core/CommandLine.h"

#include <filesystem>
#include <string>

namespace ri::games::cubetest {

struct StandaloneOptions {
    std::filesystem::path workspaceRoot{};
    std::filesystem::path gameRoot{};
    std::filesystem::path exportGltfPath{};
    std::filesystem::path nativeCapturePath{};
    std::filesystem::path frameTimesPath{};
    std::string gameId = "cube-test";
    std::string startRoom = "baseline";
    int width = 1280;
    int height = 720;
    int benchmarkFrames = 0;
    // The direct native Vulkan PBR path is the stable default. Hybrid HDR remains
    // available for explicit evaluation until its screen-space/composite path has
    // image-capture regression coverage on real hardware.
    bool hybridHdr = false;
    bool backgroundWindow = false;
    bool materialCalibration = false;
    bool normalComparison = false;
    bool cookedTextureDemo = false;
    bool extendedPostProcess = false;
    bool jiggleTest = false;
    int jigglePreviewFrames = 0;
};

bool RunStandalone(const StandaloneOptions& options,
                   const ri::core::CommandLine& commandLine,
                   std::string* error);

} // namespace ri::games::cubetest
