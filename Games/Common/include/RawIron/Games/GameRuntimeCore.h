#pragma once

#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/GameRuntimeSupport.h"
#include "RawIron/Core/Host.h"
#include "RawIron/Runtime/RuntimeCore.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace ri::games {

struct GameRuntimeBootServices {
    std::shared_ptr<ri::content::GameManifest> manifest;
    std::shared_ptr<ri::content::GameRuntimeSupportData> support;
};

[[nodiscard]] ri::runtime::RuntimePaths BuildGameRuntimePaths(
    const ri::content::GameManifest& manifest,
    const std::filesystem::path& workspaceRoot,
    const std::filesystem::path& checkpointStorageRoot = {});

[[nodiscard]] ri::runtime::RuntimeCore CreateGameRuntimeCore(
    const ri::content::GameManifest& manifest,
    std::string_view moduleName,
    ri::runtime::RuntimePaths paths,
    GameRuntimeBootServices services);

[[nodiscard]] bool StartupGameRuntimeCore(ri::runtime::RuntimeCore& runtime, std::string* error = nullptr);

[[nodiscard]] ri::core::FrameContext BuildGameRuntimeFrameContext(int frameIndex,
                                                                  double deltaSeconds,
                                                                  double elapsedSeconds,
                                                                  double realtimeSeconds);

void LogGameRuntimeSupportSummary(const ri::content::GameRuntimeSupportData& support);

} // namespace ri::games
