#pragma once

#include "RawIron/Content/GameManifest.h"

#include <filesystem>
#include <string>

namespace ri::editor {

struct PlaytestLaunchResult {
    bool launched = false;
    std::string message;
};

[[nodiscard]] PlaytestLaunchResult LaunchPlaytestForManifest(void* nativeWindowHandle,
                                                             const ri::content::GameManifest& manifest,
                                                             const std::filesystem::path& workspaceRoot);

} // namespace ri::editor
