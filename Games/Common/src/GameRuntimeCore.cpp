#include "RawIron/Games/GameRuntimeCore.h"

#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Log.h"

#include <utility>

namespace ri::games {
namespace {

class GameRuntimeModule final : public ri::runtime::RuntimeModule {
public:
    GameRuntimeModule(std::string moduleName, GameRuntimeBootServices services)
        : moduleName_(std::move(moduleName)),
          services_(std::move(services)) {}

    [[nodiscard]] std::string_view Name() const noexcept override {
        return moduleName_;
    }

    bool OnRuntimeStartup(ri::runtime::RuntimeContext& context,
                          const ri::core::CommandLine&) override {
        if (services_.manifest != nullptr) {
            context.Services().Register<ri::content::GameManifest>(services_.manifest);
        }
        if (services_.support != nullptr) {
            context.Services().Register<ri::content::GameRuntimeSupportData>(services_.support);
        }
        ri::core::LogInfo("Runtime core mounted module: " + moduleName_);
        context.Events().Emit("runtime.game.mounted", ri::runtime::RuntimeEvent{
            .id = {},
            .type = {},
            .fields = {
                {"module", moduleName_},
                {"manifest", services_.manifest != nullptr ? services_.manifest->id : std::string{}},
            },
        });
        return true;
    }

private:
    std::string moduleName_;
    GameRuntimeBootServices services_;
};

} // namespace

ri::runtime::RuntimePaths BuildGameRuntimePaths(const ri::content::GameManifest& manifest,
                                                const std::filesystem::path& workspaceRoot,
                                                const std::filesystem::path& checkpointStorageRoot) {
    ri::runtime::RuntimePaths paths{};
    paths.workspaceRoot = workspaceRoot.empty()
        ? ri::content::DetectWorkspaceRoot(manifest.rootPath)
        : workspaceRoot;
    paths.gameRoot = manifest.rootPath;
    paths.saveRoot = checkpointStorageRoot.empty()
        ? (paths.workspaceRoot / "Saved")
        : checkpointStorageRoot;
    paths.configRoot = manifest.rootPath / "config";
    return paths;
}

ri::runtime::RuntimeCore CreateGameRuntimeCore(const ri::content::GameManifest& manifest,
                                               const std::string_view moduleName,
                                               ri::runtime::RuntimePaths paths,
                                               GameRuntimeBootServices services) {
    ri::runtime::RuntimeCore runtime(
        ri::runtime::RuntimeIdentity{
            .id = manifest.id,
            .displayName = manifest.name,
            .mode = "game",
            .instanceId = {},
        },
        std::move(paths));
    runtime.AddModule(std::make_unique<GameRuntimeModule>(std::string(moduleName), std::move(services)));
    return runtime;
}

bool StartupGameRuntimeCore(ri::runtime::RuntimeCore& runtime, std::string* error) {
    char arg0[] = "RawIron.GameRuntime";
    char* argv[] = {arg0};
    const ri::core::CommandLine commandLine(1, argv);
    if (runtime.Startup(commandLine)) {
        return true;
    }
    if (error != nullptr) {
        *error = std::string(runtime.Context().FailureReason());
        if (error->empty()) {
            *error = "Runtime core startup failed.";
        }
    }
    return false;
}

ri::core::FrameContext BuildGameRuntimeFrameContext(const int frameIndex,
                                                    const double deltaSeconds,
                                                    const double elapsedSeconds,
                                                    const double realtimeSeconds) {
    return ri::core::FrameContext{
        .frameIndex = frameIndex,
        .deltaSeconds = deltaSeconds,
        .elapsedSeconds = elapsedSeconds,
        .realtimeSeconds = realtimeSeconds,
        .realDeltaSeconds = deltaSeconds,
    };
}

void LogGameRuntimeSupportSummary(const ri::content::GameRuntimeSupportData& support) {
    const ri::content::AudioZoneRow* audioAtOrigin =
        ri::content::FindAudioZoneAtPoint(0.0f, 0.0f, 0.0f, support);
    const float lodScaleSample = support.lodRanges.empty()
        ? 1.0f
        : ri::content::ComputeLodScaleForDistance(support.lodRanges.front().id, 24.0f, support);
    ri::core::LogInfo(std::string("Runtime support data: triggers=")
        + std::to_string(support.levelTriggers.size()) + ", occlusion="
        + std::to_string(support.occlusionVolumes.size()) + ", materials="
        + std::to_string(support.materialsById.size()) + ", achievements="
        + std::to_string(support.achievementIdsByPlatform.size()) + ", audioZones="
        + std::to_string(support.audioZones.size()) + ", lodRanges="
        + std::to_string(support.lodRanges.size()) + ", audioAtOrigin="
        + std::string(audioAtOrigin != nullptr ? audioAtOrigin->id : "none")
        + ", lodScale@24m=" + std::to_string(lodScaleSample));
}

} // namespace ri::games
