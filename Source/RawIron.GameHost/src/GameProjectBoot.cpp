#include "RawIron/GameHost/GameProjectBoot.h"

#include "RawIron/Core/Log.h"

namespace ri::gamehost {

ri::runtime::RuntimeCore PreparedGameProject::CreateRuntime() const {
    return ri::games::CreateGameRuntimeCore(Manifest(), Manifest().runtimeModule, paths, services);
}

std::optional<PreparedGameProject> PrepareGameProject(
    const GameProjectBootOptions& options, std::string* error) {
    const auto fail = [&](const std::string& reason) -> std::optional<PreparedGameProject> {
        if (error) *error = reason;
        return std::nullopt;
    };
    const auto workspace = options.workspaceRoot.empty()
        ? ri::content::DetectWorkspaceRoot(options.gameRoot.empty()
            ? std::filesystem::current_path() : options.gameRoot) : options.workspaceRoot;
    auto manifest = options.gameRoot.empty()
        ? ri::content::ResolveGameManifest(workspace, options.gameId)
        : ri::content::LoadGameManifest(options.gameRoot / "manifest.json");
    if (!manifest) return fail("Game boot: cannot resolve manifest for '" + options.gameId + "'.");
    if (!options.expectedRuntimeModule.empty() && manifest->runtimeModule != options.expectedRuntimeModule) {
        return fail("Game boot: executable mounts '" + options.expectedRuntimeModule
            + "' but manifest requests '" + manifest->runtimeModule + "'.");
    }
    const auto issues = ri::content::ValidateGameProjectFormat(*manifest);
    if (!issues.empty()) {
        std::string message = "Game boot: project format validation failed";
        for (const auto& issue : issues) message += "\n - " + issue;
        return fail(message);
    }
    std::string contractError;
    if (!ri::games::EnforceGameConfigContracts(manifest->rootPath, options.configContract, &contractError)) {
        return fail(contractError);
    }
    PreparedGameProject project{};
    project.paths = ri::games::BuildGameRuntimePaths(*manifest, workspace, options.checkpointStorageRoot);
    project.services.manifest = std::make_shared<ri::content::GameManifest>(std::move(*manifest));
    project.services.support = std::make_shared<ri::content::GameRuntimeSupportData>(
        ri::content::LoadGameRuntimeSupportData(project.Manifest().rootPath));
    project.services.scripts = std::make_shared<ri::content::GameScriptBundle>(
        ri::content::LoadGameScriptBundle(project.Manifest().rootPath, {.logMissing=false}));
    ri::core::LogInfo("Game boot prepared: " + project.Manifest().id
        + " module=" + project.Manifest().runtimeModule);
    ri::games::LogGameRuntimeSupportSummary(*project.services.support);
    return project;
}

} // namespace ri::gamehost
