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
                                                             const std::filesystem::path& workspaceRoot,
                                                             const std::filesystem::path& logicAuthoringPath = {});

[[nodiscard]] bool CanResolvePlaytestExecutable(const ri::content::GameManifest& manifest);

[[nodiscard]] bool ResolveDedicatedPlaytestExecutable(const ri::content::GameManifest& manifest);

} // namespace ri::editor
