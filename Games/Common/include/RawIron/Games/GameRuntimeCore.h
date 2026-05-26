#pragma once

#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/GameRuntimeSupport.h"
#include "RawIron/Core/Host.h"
#include "RawIron/Runtime/RuntimeCore.h"
#include "RawIron/Runtime/RuntimeEventBus.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace ri::games {

struct GameRuntimeBootServices {
    std::shared_ptr<ri::content::GameManifest> manifest;
    std::shared_ptr<ri::content::GameRuntimeSupportData> support;
};

/// Per-frame simulation hook owned by \ref RuntimeCore (invoked from \ref GameSimulationTickModule).
using GameSimulationTick = std::function<void(const ri::core::FrameContext& frame)>;

struct GameRuntimeCoreOptions {
    GameRuntimeBootServices services{};
    GameSimulationTick simulationTick{};
    bool registerDefaultRuntimeModules = true;
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

[[nodiscard]] ri::runtime::RuntimeCore CreateGameRuntimeCore(
    const ri::content::GameManifest& manifest,
    std::string_view moduleName,
    ri::runtime::RuntimePaths paths,
    GameRuntimeCoreOptions options);

[[nodiscard]] std::unique_ptr<ri::runtime::RuntimeModule> MakeGameSimulationTickModule(GameSimulationTick tick);

[[nodiscard]] bool AttachGameSimulationTick(ri::runtime::RuntimeCore& runtime, GameSimulationTick tick);

[[nodiscard]] bool StartupGameRuntimeCore(ri::runtime::RuntimeCore& runtime, std::string* error = nullptr);

[[nodiscard]] bool StartupGameRuntimeCore(ri::runtime::RuntimeCore& runtime,
                                          const ri::core::CommandLine& commandLine,
                                          std::string* error = nullptr);

[[nodiscard]] ri::runtime::RuntimeEventBus& RuntimeEventBusFrom(ri::runtime::RuntimeCore& runtime) noexcept;

void BindRuntimeEventBus(ri::runtime::RuntimeCore& runtime, ri::runtime::RuntimeEventBus*& slot) noexcept;

[[nodiscard]] ri::core::FrameContext BuildGameRuntimeFrameContext(int frameIndex,
                                                                  double deltaSeconds,
                                                                  double elapsedSeconds,
                                                                  double realtimeSeconds);

[[nodiscard]] ri::core::FrameContext BuildGameRuntimeFrameContext(const ri::runtime::RuntimeContext& context);

void LogGameRuntimeSupportSummary(const ri::content::GameRuntimeSupportData& support);

} // namespace ri::games
