#include "RawIron/Content/EngineAssets.h"
#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/GameRuntimeSupport.h"
#include "RawIron/Content/ScriptScalars.h"
#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/CrashDiagnostics.h"
#include "RawIron/Core/Host.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Core/MainLoop.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Editor/BundledGamePreviews.h"
#include "RawIron/Editor/PreviewSceneRegistry.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Runtime/ExperiencePresets.h"
#include "RawIron/Scene/Components.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/PrimitivesCsvIO.h"
#include "RawIron/Scene/SceneStateIO.h"
#include "RawIron/Scene/StructuralBrush.h"
#include "RawIron/Scene/WorkspaceSandbox.h"
#include "RawIron/Scene/SceneUtils.h"
#include "RawIron/Core/Detail/JsonScan.h"
#include "RawIron/World/Instrumentation.h"
#include "RawIron/World/InventoryState.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iomanip>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

namespace fs = std::filesystem;

[[nodiscard]] std::string DescribeOptionalAssetState(const fs::path& path, const bool checkSqliteHeader = false) {
    std::error_code ec{};
    if (!fs::exists(path, ec)) {
        return "missing";
    }
    if (fs::is_directory(path, ec)) {
        return "invalid-dir";
    }
    const std::uintmax_t sizeBytes = fs::file_size(path, ec);
    if (ec || sizeBytes == 0U) {
        return "empty";
    }
    if (checkSqliteHeader) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open()) {
            return "unreadable";
        }
        char header[16]{};
        stream.read(header, sizeof(header));
        const std::streamsize readCount = stream.gcount();
        static constexpr const char* kSqliteMagic = "SQLite format 3";
        if (readCount < 15 || std::string_view(header, 15) != kSqliteMagic) {
            return "invalid-header";
        }
    }
    return "ok";
}

double ResolveFixedDeltaSeconds(const ri::core::CommandLine& commandLine, int fallbackTickHz) {
    const std::optional<int> tickHz = commandLine.TryGetInt("--tick-hz");
    if (!tickHz.has_value() || *tickHz <= 0) {
        return 1.0 / static_cast<double>(fallbackTickHz);
    }
    return 1.0 / static_cast<double>(*tickHz);
}

struct EditorSceneConfig {
    fs::path workspaceRoot;
    fs::path sceneStatePath;
    std::optional<ri::content::GameManifest> gameManifest;
    /// From manifest `editorPreviewScene`, or `"starter"` when absent / no project loaded.
    std::string editorPreviewScene = "starter";
    std::string sceneName = "EditorWorkspace";
    std::string windowTitle = "RawIron Editor";
    std::string workspaceLabel = "Authoring (no game manifest)";
    std::string statusMessage;
};

bool LooksLikeWorkspaceRoot(const fs::path& path) {
    std::error_code ec{};
    return fs::exists(path / "CMakeLists.txt", ec)
        && fs::exists(path / "Source", ec)
        && fs::exists(path / "Games", ec);
}

fs::path BuildEditorSceneStatePath(const fs::path& workspaceRoot, std::string_view sceneId) {
    return workspaceRoot / "Saved" / "Editor" / std::string(sceneId) / "scene_state.ri_state";
}

enum class WorkspaceResourceCategory {
    Manifest,
    Level,
    Script,
    Test,
    UiScreen,
    Menu,
    Asset,
    Other,
};

struct WorkspaceGameEntry {
    std::string id;
    std::string displayName;
    fs::path rootPath;
};

struct WorkspaceResourceEntry {
    fs::path absolutePath;
    std::string relativePathUtf8;
    WorkspaceResourceCategory category = WorkspaceResourceCategory::Other;
};

[[nodiscard]] std::string WorkspaceCategoryLabel(const WorkspaceResourceCategory category) {
    switch (category) {
        case WorkspaceResourceCategory::Manifest:
            return "Manifest";
        case WorkspaceResourceCategory::Level:
            return "Level";
        case WorkspaceResourceCategory::Script:
            return "Script";
        case WorkspaceResourceCategory::Test:
            return "Test";
        case WorkspaceResourceCategory::UiScreen:
            return "Screen / UI";
        case WorkspaceResourceCategory::Menu:
            return "Menu";
        case WorkspaceResourceCategory::Asset:
            return "Asset";
        case WorkspaceResourceCategory::Other:
            return "Other";
    }
    return "Other";
}

[[nodiscard]] WorkspaceResourceCategory ClassifyRelativeGamePath(const fs::path& relativePath) {
    const fs::path norm = relativePath.lexically_normal();
    if (norm.filename() == "manifest.json") {
        return WorkspaceResourceCategory::Manifest;
    }
    if (norm.begin() == norm.end()) {
        return WorkspaceResourceCategory::Other;
    }
    fs::path firstComponent = *norm.begin();
    std::string low = firstComponent.string();
    for (char& ch : low) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    if (low == "levels") {
        return WorkspaceResourceCategory::Level;
    }
    if (low == "scripts") {
        return WorkspaceResourceCategory::Script;
    }
    if (low == "tests") {
        return WorkspaceResourceCategory::Test;
    }
    if (low == "assets") {
        return WorkspaceResourceCategory::Asset;
    }
    if (low == "ui" || low == "screens") {
        return WorkspaceResourceCategory::UiScreen;
    }
    if (low == "menus") {
        return WorkspaceResourceCategory::Menu;
    }
    return WorkspaceResourceCategory::Other;
}

[[nodiscard]] bool IsLikelyTextResourcePath(const fs::path& path) {
    std::string ext = path.extension().string();
    for (char& ch : ext) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    static constexpr const char* kTextExt[] = {".csv",  ".riscript", ".json", ".md",    ".txt", ".xml",
                                               ".yaml", ".yml",      ".ini",  ".hlsl", ".glsl", ".ripalette",
                                               ".css"};
    for (const char* e : kTextExt) {
        if (ext == e) {
            return true;
        }
    }
    return false;
}

void CollectResourcesUnderTree(const fs::path& gameRoot,
                               const fs::path& subdir,
                               WorkspaceResourceCategory forcedCategory,
                               std::vector<WorkspaceResourceEntry>& out) {
    std::error_code ec{};
    const fs::path base = gameRoot / subdir;
    if (!fs::exists(base, ec)) {
        return;
    }
    const fs::directory_options opts = fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator end{};
    for (fs::recursive_directory_iterator it(base, opts, ec); !ec && it != end; ++it) {
        if (!it->is_regular_file()) {
            continue;
        }
        const fs::path abs = it->path();
        std::error_code relEc{};
        const fs::path rel = fs::relative(abs, gameRoot, relEc);
        if (relEc) {
            continue;
        }
        const WorkspaceResourceCategory cat = (forcedCategory == WorkspaceResourceCategory::Other)
            ? ClassifyRelativeGamePath(rel)
            : forcedCategory;
        out.push_back(WorkspaceResourceEntry{
            .absolutePath = abs,
            .relativePathUtf8 = rel.generic_string(),
            .category = cat,
        });
    }
}

[[nodiscard]] std::vector<WorkspaceGameEntry> EnumerateWorkspaceGames(const fs::path& workspaceRoot) {
    std::vector<WorkspaceGameEntry> games;
    std::error_code ec{};
    const fs::path gamesRoot = workspaceRoot / "Games";
    if (!fs::exists(gamesRoot, ec)) {
        return games;
    }
    for (const fs::directory_entry& entry : fs::directory_iterator(gamesRoot, ec)) {
        if (!entry.is_directory()) {
            continue;
        }
        const fs::path manifestPath = entry.path() / "manifest.json";
        if (!fs::exists(manifestPath, ec)) {
            continue;
        }
        const std::optional<ri::content::GameManifest> manifest = ri::content::LoadGameManifest(manifestPath);
        if (!manifest.has_value()) {
            continue;
        }
        games.push_back(WorkspaceGameEntry{
            .id = manifest->id,
            .displayName = manifest->name.empty() ? manifest->id : manifest->name,
            .rootPath = manifest->rootPath,
        });
    }
    std::sort(games.begin(), games.end(), [](const WorkspaceGameEntry& a, const WorkspaceGameEntry& b) {
        return a.displayName < b.displayName;
    });
    return games;
}

[[nodiscard]] std::vector<WorkspaceResourceEntry> CollectWorkspaceGameResources(const fs::path& gameRoot) {
    std::vector<WorkspaceResourceEntry> resources;
    const ri::content::GameRuntimeSupportData supportData = ri::content::LoadGameRuntimeSupportData(gameRoot);
    std::error_code ec{};
    const fs::path manifestPath = gameRoot / "manifest.json";
    if (fs::exists(manifestPath, ec)) {
        const fs::path rel = fs::relative(manifestPath, gameRoot, ec);
        resources.push_back(WorkspaceResourceEntry{
            .absolutePath = manifestPath,
            .relativePathUtf8 = rel.generic_string(),
            .category = WorkspaceResourceCategory::Manifest,
        });
    }
    CollectResourcesUnderTree(gameRoot, "levels", WorkspaceResourceCategory::Level, resources);
    CollectResourcesUnderTree(gameRoot, "scripts", WorkspaceResourceCategory::Script, resources);
    CollectResourcesUnderTree(gameRoot, "tests", WorkspaceResourceCategory::Test, resources);
    CollectResourcesUnderTree(gameRoot, "assets", WorkspaceResourceCategory::Asset, resources);
    CollectResourcesUnderTree(gameRoot, "ui", WorkspaceResourceCategory::UiScreen, resources);
    CollectResourcesUnderTree(gameRoot, "screens", WorkspaceResourceCategory::UiScreen, resources);
    CollectResourcesUnderTree(gameRoot, "menus", WorkspaceResourceCategory::Menu, resources);

    std::sort(resources.begin(), resources.end(), [&supportData](const WorkspaceResourceEntry& a, const WorkspaceResourceEntry& b) {
        const int aScore = ri::content::ComputeResourcePriorityScore(supportData, a.relativePathUtf8);
        const int bScore = ri::content::ComputeResourcePriorityScore(supportData, b.relativePathUtf8);
        if (aScore != bScore) {
            return aScore > bScore;
        }
        return a.relativePathUtf8 < b.relativePathUtf8;
    });
    resources.erase(std::unique(resources.begin(),
                                resources.end(),
                                [](const WorkspaceResourceEntry& a, const WorkspaceResourceEntry& b) {
                                    return a.absolutePath == b.absolutePath;
                                }),
                    resources.end());
    return resources;
}

fs::path ResolveEditorWorkspaceRoot(const fs::path& fallbackWorkspaceRoot,
                                    const std::optional<ri::content::GameManifest>& manifest,
                                    const std::optional<fs::path>& explicitGameRoot) {
    if (explicitGameRoot.has_value()) {
        const fs::path detected = ri::content::DetectWorkspaceRoot(*explicitGameRoot);
        if (LooksLikeWorkspaceRoot(detected)) {
            return detected;
        }
        if (manifest.has_value()) {
            return manifest->rootPath;
        }
        return *explicitGameRoot;
    }

    if (manifest.has_value()) {
        const fs::path detected = ri::content::DetectWorkspaceRoot(manifest->rootPath);
        if (LooksLikeWorkspaceRoot(detected)) {
            return detected;
        }
        return manifest->rootPath;
    }

    return fallbackWorkspaceRoot;
}

[[nodiscard]] fs::path NormalizePathForConfig(std::string_view rawPath) {
    if (rawPath.empty()) {
        return {};
    }
    std::error_code ec{};
    fs::path normalized = fs::weakly_canonical(fs::path(rawPath), ec);
    if (ec) {
        // Preserve user-provided path when canonicalization fails so follow-up errors are still actionable.
        return fs::path(rawPath);
    }
    return normalized;
}

void EnsureProjectDevConfig(const fs::path& gameRoot) {
    if (gameRoot.empty()) {
        return;
    }
    std::error_code ec{};
    const fs::path configDir = gameRoot / "config";
    fs::create_directories(configDir, ec);
    if (ec) {
        return;
    }
    const fs::path projectDevPath = configDir / "project.dev";
    if (fs::exists(projectDevPath, ec)) {
        return;
    }
    std::ofstream stream(projectDevPath, std::ios::out | std::ios::trunc);
    if (!stream.is_open()) {
        return;
    }
    stream << "# RawIron Project Dev Config v1\n"
           << "last_opened_profile=1\n"
           << "local_debug_overlay=0\n";
}

EditorSceneConfig ResolveSceneConfig(const ri::core::CommandLine& commandLine) {
    EditorSceneConfig config{};
    fs::path defaultWorkspaceRoot = ri::content::DetectWorkspaceRoot(fs::current_path());
    if (const auto workspaceRoot = commandLine.GetValue("--workspace-root");
        workspaceRoot.has_value() && !workspaceRoot->empty()) {
        defaultWorkspaceRoot = NormalizePathForConfig(*workspaceRoot);
    }
    config.workspaceRoot = defaultWorkspaceRoot;

    std::optional<std::string> experiencePreset{};
    if (const auto preset = commandLine.GetValue("--experience"); preset.has_value() && !preset->empty()) {
        experiencePreset = *preset;
    } else if (const auto presetFallback = commandLine.GetValue("--preset");
               presetFallback.has_value() && !presetFallback->empty()) {
        experiencePreset = *presetFallback;
    }
    std::optional<ri::runtime::ExperiencePresetPatch> experiencePatch{};
    if (experiencePreset.has_value()) {
        experiencePatch = ri::runtime::ResolveExperiencePreset(*experiencePreset);
        if (!experiencePatch.has_value()) {
            std::string supported = "Supported presets:";
            for (const std::string_view name : ri::runtime::SupportedExperiencePresets()) {
                supported += " ";
                supported += std::string(name);
            }
            config.statusMessage = "Unknown --experience preset '" + *experiencePreset + "'. " + supported;
            return config;
        }
    }

    std::optional<ri::content::GameManifest> manifest;
    std::optional<fs::path> explicitGameRoot{};
    const std::optional<std::string> projectRootArg = commandLine.GetValue("--project-root");
    const std::optional<std::string> gameRootArg =
        projectRootArg.has_value() ? projectRootArg : commandLine.GetValue("--game-root");
    if (gameRootArg.has_value() && !gameRootArg->empty()) {
        explicitGameRoot = NormalizePathForConfig(*gameRootArg);
        manifest = ri::content::LoadGameManifest(*explicitGameRoot / "manifest.json");
        if (!manifest.has_value()) {
            config.statusMessage = "Unable to load game manifest from --project-root/--game-root.";
            return config;
        }
    }

    const std::optional<std::string> projectArg = commandLine.GetValue("--project");
    const std::optional<std::string> gameArg = projectArg.has_value() ? projectArg : commandLine.GetValue("--game");
    if (!manifest.has_value() && gameArg.has_value() && !gameArg->empty()) {
        manifest = ri::content::ResolveGameManifest(config.workspaceRoot, *gameArg);
        if (!manifest.has_value()) {
            config.statusMessage = "Unable to resolve game manifest for '" + *gameArg + "'.";
            const std::vector<WorkspaceGameEntry> availableGames = EnumerateWorkspaceGames(config.workspaceRoot);
            if (!availableGames.empty()) {
                config.statusMessage += " Available projects:";
                for (const WorkspaceGameEntry& entry : availableGames) {
                    config.statusMessage += " ";
                    config.statusMessage += entry.id;
                }
            }
            return config;
        }
    } else if (experiencePatch.has_value() && !experiencePatch->gameId.empty()) {
        manifest = ri::content::ResolveGameManifest(config.workspaceRoot, experiencePatch->gameId);
        if (!manifest.has_value()) {
            config.statusMessage =
                "Unable to resolve game manifest for experience preset '" + *experiencePreset + "'.";
            return config;
        }
    }

    config.workspaceRoot = ResolveEditorWorkspaceRoot(defaultWorkspaceRoot, manifest, explicitGameRoot);
    config.sceneStatePath = BuildEditorSceneStatePath(config.workspaceRoot, "starter");

    if (!manifest.has_value()) {
        return config;
    }

    EnsureProjectDevConfig(manifest->rootPath);
    manifest = ri::content::LoadGameManifest(manifest->manifestPath);
    if (!manifest.has_value()) {
        config.statusMessage = "Unable to reload game manifest after project bootstrap.";
        return config;
    }
    config.gameManifest = manifest;
    const std::string manifestVersionLabel = manifest->version.empty() ? "v?" : "v" + manifest->version;
    const std::string manifestAuthorLabel = manifest->author.empty() ? "unknown" : manifest->author;
    config.workspaceLabel = std::string("Authoring — ") + manifest->name + " " + manifestVersionLabel;
    config.windowTitle =
        std::string("RawIron Editor — ") + manifest->name + " " + manifestVersionLabel + " (" + manifestAuthorLabel + ")";
    config.sceneStatePath = BuildEditorSceneStatePath(config.workspaceRoot, manifest->id);
    config.sceneName = "EditorWorkspace_" + manifest->id;
    config.editorPreviewScene =
        manifest->editorPreviewScene.empty() ? "starter" : manifest->editorPreviewScene;
    config.statusMessage = "Opened game project '" + manifest->name + "' " + manifestVersionLabel
        + " by " + manifestAuthorLabel + ".";
    if (!manifest->primaryLevel.empty()) {
        config.statusMessage += " Primary level: " + manifest->primaryLevel + ".";
    }
    const ri::content::ScriptScalarMap uiScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/ui.riscript"));
    const ri::content::ScriptScalarMap audioScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/audio.riscript"));
    const ri::content::ScriptScalarMap streamingScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/streaming.riscript"));
    const ri::content::ScriptScalarMap localizationScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/localization.riscript"));
    const ri::content::ScriptScalarMap physicsScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/physics.riscript"));
    const ri::content::ScriptScalarMap postprocessScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/postprocess.riscript"));
    const ri::content::ScriptScalarMap initScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/init.riscript"));
    const ri::content::ScriptScalarMap gameCfgScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "config/game.cfg"));
    const ri::content::ScriptScalarMap networkScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/network.riscript"));
    const ri::content::ScriptScalarMap persistenceScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/persistence.riscript"));
    const ri::content::ScriptScalarMap aiScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/ai.riscript"));
    const ri::content::ScriptScalarMap pluginsScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/plugins.riscript"));
    const ri::content::ScriptScalarMap animationScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/animation.riscript"));
    const ri::content::ScriptScalarMap vfxScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/vfx.riscript"));
    const ri::content::ScriptScalarMap networkCfgScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "config/network.cfg"));
    const ri::content::ScriptScalarMap buildProfileScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "config/build.profile"));
    const ri::content::ScriptScalarMap securityPolicyScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "config/security.policy"));
    const ri::content::ScriptScalarMap pluginsPolicyScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "config/plugins.policy"));
    const ri::content::GameRuntimeSupportData runtimeSupportData =
        ri::content::LoadGameRuntimeSupportData(manifest->rootPath);
    if (!uiScalars.empty()) {
        config.statusMessage += " UI{diag="
            + std::string(ri::content::ScriptScalarOrBool(uiScalars, "show_runtime_diagnostics", false) ? "1" : "0")
            + ",crosshair="
            + std::to_string(ri::content::ScriptScalarOrIntClamped(uiScalars, "crosshair_mode", 1, 0, 4))
            + ",scale="
            + std::to_string(ri::content::ScriptScalarOrClamped(uiScalars, "crosshair_scale", 1.0f, 0.1f, 4.0f))
            + "}.";
    }
    if (!gameCfgScalars.empty()) {
        config.statusMessage += " CFG{runtime="
            + std::to_string(ri::content::ScriptScalarOrIntClamped(gameCfgScalars, "runtime_profile", 1, 0, 16))
            + ",editor="
            + std::to_string(ri::content::ScriptScalarOrIntClamped(gameCfgScalars, "editor_profile", 1, 0, 16))
            + "}.";
    }
    if (!audioScalars.empty()) {
        config.statusMessage += " Audio{master="
            + std::to_string(ri::content::ScriptScalarOrClamped(audioScalars, "audio_master_gain", 1.0f, 0.0f, 4.0f))
            + ",envBlend="
            + std::to_string(ri::content::ScriptScalarOrClamped(audioScalars, "audio_environment_blend", 1.0f, 0.0f, 2.0f))
            + "}.";
    }
    if (!streamingScalars.empty()) {
        config.statusMessage += " Streaming{budget="
            + std::to_string(
                ri::content::ScriptScalarOrClamped(streamingScalars, "streaming_budget_scale", 1.0f, 0.1f, 8.0f))
            + ",autosave="
            + std::string(ri::content::ScriptScalarOrBool(streamingScalars, "checkpoint_autosave_enabled", true) ? "1" : "0")
            + "}.";
    }
    if (!localizationScalars.empty()) {
        config.statusMessage += " Loc{default="
            + std::to_string(ri::content::ScriptScalarOrIntClamped(localizationScalars, "default_locale", 0, 0, 16))
            + "}.";
    }
    if (!physicsScalars.empty()) {
        config.statusMessage += " Physics{gravity="
            + std::to_string(ri::content::ScriptScalarOrClamped(physicsScalars, "global_gravity_scale", 1.0f, 0.1f, 4.0f))
            + "}.";
    }
    if (!postprocessScalars.empty()) {
        config.statusMessage += " PostFx{quality="
            + std::to_string(ri::content::ScriptScalarOrIntClamped(postprocessScalars, "postprocess_quality", 1, 0, 3))
            + "}.";
    }
    if (!initScalars.empty()) {
        config.statusMessage += " Init{warmup="
            + std::to_string(ri::content::ScriptScalarOrIntClamped(initScalars, "warmup_frames", 2, 0, 120))
            + "}.";
    }
    if (!networkScalars.empty() || !persistenceScalars.empty() || !aiScalars.empty() || !pluginsScalars.empty()
        || !animationScalars.empty() || !vfxScalars.empty() || !networkCfgScalars.empty()
        || !buildProfileScalars.empty() || !securityPolicyScalars.empty() || !pluginsPolicyScalars.empty()) {
        config.statusMessage += " RuntimeExt{network="
            + std::to_string(networkScalars.size())
            + ",persistence="
            + std::to_string(persistenceScalars.size())
            + ",ai="
            + std::to_string(aiScalars.size())
            + ",plugins="
            + std::to_string(pluginsScalars.size())
            + ",animation="
            + std::to_string(animationScalars.size())
            + ",vfx="
            + std::to_string(vfxScalars.size())
            + ",networkCfg="
            + std::to_string(networkCfgScalars.size())
            + ",buildProfile="
            + std::to_string(buildProfileScalars.size())
            + ",security="
            + std::to_string(securityPolicyScalars.size())
            + ",pluginsPolicy="
            + std::to_string(pluginsPolicyScalars.size())
            + ",streamRules="
            + std::to_string(runtimeSupportData.streamingPrioritiesByPath.size())
            + ",lookupKeys="
            + std::to_string(runtimeSupportData.lookupIndex.size())
            + ",gen13xTriggers="
            + std::to_string(runtimeSupportData.levelTriggers.size())
            + ",gen13xOcclusion="
            + std::to_string(runtimeSupportData.occlusionVolumes.size())
            + ",gen137AudioZones="
            + std::to_string(runtimeSupportData.audioZones.size())
            + ",gen137Lods="
            + std::to_string(runtimeSupportData.lodRanges.size())
            + "}.";
    }
    const std::string navmeshPath = ri::content::ResolveLookupValueOr(
        "levels.navmesh",
        "levels/assembly.navmesh",
        runtimeSupportData);
    const std::string zonesPath = ri::content::ResolveLookupValueOr(
        "levels.zones",
        "levels/assembly.zones.csv",
        runtimeSupportData);
    const std::string dependenciesPath = ri::content::ResolveLookupValueOr(
        "assets.dependencies",
        "assets/dependencies.json",
        runtimeSupportData);
    const std::string streamingManifestPath = ri::content::ResolveLookupValueOr(
        "assets.streaming_manifest",
        "assets/streaming.manifest",
        runtimeSupportData);
    const std::string schemaDbPath = ri::content::ResolveLookupValueOr(
        "data.schema",
        "data/schema.db",
        runtimeSupportData);
    const std::string lookupPath = ri::content::ResolveLookupValueOr(
        "data.lookup",
        "data/lookup.index",
        runtimeSupportData);
    config.statusMessage += " Assets{navmesh="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, navmeshPath))
        + ",zones="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, zonesPath))
        + ",aiNodes="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "levels/assembly.ai.nodes"))
        + ",deps="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, dependenciesPath))
        + ",streamingManifest="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, streamingManifestPath))
        + ",shadersManifest="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "assets/shaders.manifest"))
        + ",schema="
        + DescribeOptionalAssetState(
            ri::content::ResolveGameAssetPath(manifest->rootPath, schemaDbPath),
            true)
        + ",lookup="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, lookupPath))
        + ",entityRegistry="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "data/entity.registry"))
        + ",aiBehavior="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "ai/behavior.tree"))
        + ",aiBlackboard="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "ai/blackboard.json"))
        + ",aiFactions="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "ai/factions.cfg"))
        + ",lighting="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "levels/assembly.lighting.csv"))
        + ",cinematics="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "levels/assembly.cinematics.csv"))
        + ",pluginsManifest="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "plugins/manifest.plugins"))
        + ",pluginsLoadOrder="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "plugins/load_order.cfg"))
        + ",pluginsRegistry="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "plugins/registry.json"))
        + ",pluginsHooks="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "plugins/hooks.riplugin"))
        + ",animationGraph="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "assets/animation.graph"))
        + ",vfxManifest="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "assets/vfx.manifest"))
        + ",triggersCsv="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "levels/assembly.triggers.csv"))
        + ",occlusionCsv="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "levels/assembly.occlusion.csv"))
        + ",audioZones="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "levels/assembly.audio.zones"))
        + ",lodsCsv="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "levels/assembly.lods.csv"))
        + ",materialsManifest="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "assets/materials.manifest"))
        + ",audioBanks="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "assets/audio.banks"))
        + ",fontsManifest="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "assets/fonts.manifest"))
        + ",saveSchema="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "data/save.schema"))
        + ",achievementsRegistry="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "data/achievements.registry"))
        + ",perceptionCfg="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "ai/perception.cfg"))
        + ",squadTactics="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "ai/squad.tactics"))
        + ",uiLayout="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "ui/layout.xml"))
        + ",uiStyling="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "ui/styling.css"))
        + ",gameplayTests="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "tests/gameplay.test.riscript"))
        + ",renderingTests="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "tests/rendering.test.riscript"))
        + ",networkTests="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "tests/network.test.riscript"))
        + ",uiTests="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "tests/ui.test.riscript"))
        + ",telemetry="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "data/telemetry.db"), true)
        + "}.";
    config.statusMessage += " Gen13x{materials="
        + std::to_string(runtimeSupportData.materialsById.size())
        + ",audioBanks="
        + std::to_string(runtimeSupportData.audioBankPathById.size())
        + ",fonts="
        + std::to_string(runtimeSupportData.fontPathByFontKey.size())
        + ",saveSchema="
        + (runtimeSupportData.saveSchemaVersion.has_value()
               ? std::to_string(*runtimeSupportData.saveSchemaVersion)
               : std::string("-"))
        + ",achievements="
        + std::to_string(runtimeSupportData.achievementIdsByPlatform.size())
        + ",perceptionKeys="
        + std::to_string(runtimeSupportData.perceptionScalars.size())
        + ",squads="
        + std::to_string(runtimeSupportData.squadTactics.size())
        + ",audioZones="
        + std::to_string(runtimeSupportData.audioZones.size())
        + ",lods="
        + std::to_string(runtimeSupportData.lodRanges.size())
        + "}.";
    const std::vector<std::string> formatIssues = ri::content::ValidateGameProjectFormat(*manifest);
    if (!formatIssues.empty()) {
        std::string combined = "Game format issues:";
        for (const std::string& issue : formatIssues) {
            combined += " ";
            combined += issue;
        }
        if (!config.statusMessage.empty()) {
            config.statusMessage += " ";
        }
        config.statusMessage += combined;
    }
    if (experiencePatch.has_value()) {
        if (experiencePatch->windowTitle.has_value()) {
            config.windowTitle = *experiencePatch->windowTitle + " - Creator Editor";
        }
        config.statusMessage = "Opened creator experience preset '" + *experiencePreset + "'.";
    }

    return config;
}

class EditorHost final : public ri::core::Host {
public:
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "RawIron.Editor";
    }

    [[nodiscard]] std::string_view GetMode() const noexcept override {
        return "editor";
    }

    void OnStartup(const ri::core::CommandLine& commandLine) override {
        dumpSceneEveryFrame_ = commandLine.HasFlag("--dump-scene-every-frame");
        dumpScene_ = !commandLine.HasFlag("--no-scene-dump");
        logEveryFrame_ = commandLine.HasFlag("--log-every-frame");
        sceneConfig_ = ResolveSceneConfig(commandLine);
        ri::editor::RegisterBundledGameEditorPreviews();
        starterScene_ = ri::editor::BuildEditorWorkspaceScene(
            sceneConfig_.editorPreviewScene,
            sceneConfig_.sceneName,
            sceneConfig_.gameManifest.has_value() ? sceneConfig_.gameManifest->rootPath : fs::path{});

        ri::core::LogSection("Editor Startup");
        ri::core::LogInfo("Authoring shell: place cube/plane primitives, move them in the scene, then export to the game's levels/assembly.primitives.csv (Ctrl+E).");
        ri::core::LogInfo("Scene graph and helpers run in the shared runtime; this is the live edit buffer for level geometry.");
        ri::core::LogInfo("Workspace: " + sceneConfig_.workspaceLabel);
        if (!sceneConfig_.statusMessage.empty()) {
            ri::core::LogInfo(sceneConfig_.statusMessage);
        }
        ri::core::LogInfo("Workspace root: " + sceneConfig_.workspaceRoot.string());
        if (sceneConfig_.gameManifest.has_value()) {
            ri::core::LogInfo("Game root: " + sceneConfig_.gameManifest->rootPath.string());
        }
        ri::core::LogInfo("Scene state: " + sceneConfig_.sceneStatePath.string());
        ri::core::LogInfo("Root nodes: " + std::to_string(ri::scene::CollectRootNodes(starterScene_.scene).size()));
        ri::core::LogInfo("Grid path: " + ri::scene::DescribeNodePath(starterScene_.scene, starterScene_.handles.grid));

        if (dumpScene_) {
            ri::core::LogSection("Editor Scene");
            ri::core::LogInfo(starterScene_.scene.Describe());
        }
    }

    [[nodiscard]] bool OnFrame(const ri::core::FrameContext& frame) override {
        ri::editor::AnimateEditorWorkspaceScene(sceneConfig_.editorPreviewScene, starterScene_, frame.elapsedSeconds);

        if (frame.frameIndex == 0 || logEveryFrame_) {
            const ri::scene::Scene& scene = starterScene_.scene;
            const ri::scene::Node& orbitRig = scene.GetNode(starterScene_.handles.orbitCamera.root);
            const ri::math::Vec3 cameraPosition = scene.ComputeWorldPosition(starterScene_.handles.orbitCamera.cameraNode);
            ri::core::LogInfo(
                "Editor frame " + std::to_string(frame.frameIndex) +
                " orbitYaw=" + std::to_string(orbitRig.localTransform.rotationDegrees.y) +
                " camera=" + ri::math::ToString(cameraPosition));
        }

        if (dumpSceneEveryFrame_) {
            const ri::scene::Scene& scene = starterScene_.scene;
            ri::core::LogSection("Editor Scene Frame " + std::to_string(frame.frameIndex));
            ri::core::LogInfo(scene.Describe());
        }

        return true;
    }

    void OnShutdown() override {
        ri::core::LogSection("Editor Shutdown");
        ri::core::LogInfo("Editor host shutdown complete.");
    }

private:
    bool dumpScene_ = true;
    bool dumpSceneEveryFrame_ = false;
    bool logEveryFrame_ = false;
    EditorSceneConfig sceneConfig_{};
    ri::scene::StarterScene starterScene_{};
};

#if defined(_WIN32)
[[nodiscard]] std::wstring Utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int count =
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (count <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), count);
    return wide;
}

[[nodiscard]] std::string WideToUtf8(std::wstring_view wide) {
    if (wide.empty()) {
        return {};
    }
    const int count = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0,
                                           nullptr, nullptr);
    if (count <= 0) {
        return {};
    }
    std::string utf8(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), count, nullptr,
                        nullptr);
    return utf8;
}

[[nodiscard]] std::string PathToUtf8(const fs::path& path) {
    return WideToUtf8(path.wstring());
}

std::wstring Widen(const std::string& value) {
    return Utf8ToWide(value);
}

void FillRectColor(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void DrawPanelFrame(HDC dc, const RECT& rect, COLORREF fill, COLORREF highlight, COLORREF shadow) {
    FillRectColor(dc, rect, fill);
    HPEN highlightPen = CreatePen(PS_SOLID, 1, highlight);
    HPEN shadowPen = CreatePen(PS_SOLID, 1, shadow);
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, highlightPen));

    MoveToEx(dc, rect.left, rect.bottom - 1, nullptr);
    LineTo(dc, rect.left, rect.top);
    LineTo(dc, rect.right - 1, rect.top);

    SelectObject(dc, shadowPen);
    MoveToEx(dc, rect.left, rect.bottom - 1, nullptr);
    LineTo(dc, rect.right - 1, rect.bottom - 1);
    LineTo(dc, rect.right - 1, rect.top);

    SelectObject(dc, oldPen);
    DeleteObject(highlightPen);
    DeleteObject(shadowPen);
}

void DrawInsetFrame(HDC dc, const RECT& rect, COLORREF fill, COLORREF highlight, COLORREF shadow) {
    FillRectColor(dc, rect, fill);
    HPEN shadowPen = CreatePen(PS_SOLID, 1, shadow);
    HPEN highlightPen = CreatePen(PS_SOLID, 1, highlight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, shadowPen));

    MoveToEx(dc, rect.left, rect.bottom - 1, nullptr);
    LineTo(dc, rect.left, rect.top);
    LineTo(dc, rect.right - 1, rect.top);

    SelectObject(dc, highlightPen);
    MoveToEx(dc, rect.left, rect.bottom - 1, nullptr);
    LineTo(dc, rect.right - 1, rect.bottom - 1);
    LineTo(dc, rect.right - 1, rect.top);

    SelectObject(dc, oldPen);
    DeleteObject(shadowPen);
    DeleteObject(highlightPen);
}

void DrawTextLine(HDC dc, const RECT& rect, const std::string& text, COLORREF color, HFONT font, UINT format) {
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font));
    const std::wstring wide = Widen(text);
    RECT mutableRect = rect;
    DrawTextW(dc, wide.c_str(), static_cast<int>(wide.size()), &mutableRect, format);
    SelectObject(dc, oldFont);
}

HFONT CreateUiFont(int height, int weight, const wchar_t* faceName = L"Segoe UI") {
    return CreateFontW(
        height,
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        faceName);
}

[[nodiscard]] int ComputeNodeDepth(const ri::scene::Scene& scene, int nodeIndex) {
    int depth = 0;
    int parent = scene.GetNode(nodeIndex).parent;
    while (parent != ri::scene::kInvalidHandle) {
        ++depth;
        parent = scene.GetNode(parent).parent;
    }
    return depth;
}

void AppendHierarchyDfs(const ri::scene::Scene& scene,
                        int nodeIndex,
                        std::vector<int>& out,
                        const int omitSubtreeRoot) {
    if (omitSubtreeRoot >= 0 && nodeIndex == omitSubtreeRoot) {
        return;
    }
    out.push_back(nodeIndex);
    const ri::scene::Node& node = scene.GetNode(nodeIndex);
    for (const int child : node.children) {
        if (omitSubtreeRoot >= 0 && child == omitSubtreeRoot) {
            continue;
        }
        AppendHierarchyDfs(scene, child, out, omitSubtreeRoot);
    }
}

[[nodiscard]] std::vector<int> BuildHierarchyDrawOrder(const ri::scene::Scene& scene,
                                                       const int omitSubtreeRoot = ri::scene::kInvalidHandle) {
    std::vector<int> order;
    order.reserve(scene.NodeCount());
    const std::vector<int> roots = ri::scene::CollectRootNodes(scene);
    for (const int root : roots) {
        AppendHierarchyDfs(scene, root, order, omitSubtreeRoot);
    }
    return order;
}

/// Drops mesh attachments so geometry does not render (used when moving authored nodes into editor trash).
void DetachMeshesInSubtree(ri::scene::Scene& scene, const int nodeHandle) {
    if (nodeHandle < 0 || static_cast<std::size_t>(nodeHandle) >= scene.NodeCount()) {
        return;
    }
    ri::scene::Node& node = scene.GetNode(nodeHandle);
    if (node.mesh != ri::scene::kInvalidHandle) {
        scene.AttachMesh(nodeHandle, ri::scene::kInvalidHandle, ri::scene::kInvalidHandle);
    }
    for (const int child : node.children) {
        DetachMeshesInSubtree(scene, child);
    }
}

[[nodiscard]] std::optional<int> FindDrawOrderIndex(const std::vector<int>& order, std::size_t nodeIndex) {
    const int needle = static_cast<int>(nodeIndex);
    for (int i = 0; i < static_cast<int>(order.size()); ++i) {
        if (order[static_cast<std::size_t>(i)] == needle) {
            return i;
        }
    }
    return std::nullopt;
}

enum class RawIronFlatProjection {
    TopXz,
    FrontXy,
    SideZy,
};

void DcStrokeLine(HDC dc, LONG x1, LONG y1, LONG x2, LONG y2, COLORREF color, int width = 1) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

[[nodiscard]] float PickNiceGridStep(float worldDiameter) {
    const float raw = worldDiameter / 9.0f;
    const float safe = std::max(raw, 0.0001f);
    const float power = std::pow(10.0f, std::floor(std::log10(safe)));
    const float mantissa = safe / power;
    float factor = 1.0f;
    if (mantissa < 1.5f) {
        factor = 1.0f;
    } else if (mantissa < 3.5f) {
        factor = 2.0f;
    } else if (mantissa < 7.0f) {
        factor = 5.0f;
    } else {
        factor = 10.0f;
    }
    return factor * power;
}

[[nodiscard]] std::optional<ri::scene::WorldBounds> TryMergeRenderableBounds(const ri::scene::Scene& scene) {
    const std::vector<int> handles = ri::scene::CollectRenderableNodes(scene);
    return ri::scene::ComputeCombinedWorldBounds(scene, handles, true);
}

[[nodiscard]] ri::scene::WorldBounds DefaultEditorBounds() {
    return ri::scene::WorldBounds{
        .min = ri::math::Vec3{-10.0f, -4.0f, -10.0f},
        .max = ri::math::Vec3{10.0f, 10.0f, 10.0f},
    };
}

void ProjectRawIronTop(const RECT& cell,
                      float centerX,
                      float centerZ,
                      float halfSpan,
                      float wx,
                      float wz,
                      LONG& sx,
                      LONG& sy) {
    constexpr float kPad = 5.0f;
    const float uw = static_cast<float>(cell.right - cell.left) - kPad * 2.0f;
    const float uh = static_cast<float>(cell.bottom - cell.top) - kPad * 2.0f;
    const float minX = centerX - halfSpan;
    const float minZ = centerZ - halfSpan;
    const float u = (wx - minX) / (halfSpan * 2.0f);
    const float v = (wz - minZ) / (halfSpan * 2.0f);
    sx = cell.left + static_cast<LONG>(kPad + std::clamp(u, -0.02f, 1.02f) * uw);
    sy = cell.top + static_cast<LONG>(kPad + (1.0f - std::clamp(v, -0.02f, 1.02f)) * uh);
}

void ProjectRawIronFront(const RECT& cell,
                        float centerX,
                        float centerY,
                        float halfSpan,
                        float wx,
                        float wy,
                        LONG& sx,
                        LONG& sy) {
    constexpr float kPad = 5.0f;
    const float uw = static_cast<float>(cell.right - cell.left) - kPad * 2.0f;
    const float uh = static_cast<float>(cell.bottom - cell.top) - kPad * 2.0f;
    const float minX = centerX - halfSpan;
    const float minY = centerY - halfSpan;
    const float u = (wx - minX) / (halfSpan * 2.0f);
    const float v = (wy - minY) / (halfSpan * 2.0f);
    sx = cell.left + static_cast<LONG>(kPad + std::clamp(u, -0.02f, 1.02f) * uw);
    sy = cell.top + static_cast<LONG>(kPad + (1.0f - std::clamp(v, -0.02f, 1.02f)) * uh);
}

void ProjectRawIronSide(const RECT& cell,
                       float centerZ,
                       float centerY,
                       float halfSpan,
                       float wz,
                       float wy,
                       LONG& sx,
                       LONG& sy) {
    constexpr float kPad = 5.0f;
    const float uw = static_cast<float>(cell.right - cell.left) - kPad * 2.0f;
    const float uh = static_cast<float>(cell.bottom - cell.top) - kPad * 2.0f;
    const float minZ = centerZ - halfSpan;
    const float minY = centerY - halfSpan;
    const float u = (wz - minZ) / (halfSpan * 2.0f);
    const float v = (wy - minY) / (halfSpan * 2.0f);
    sx = cell.left + static_cast<LONG>(kPad + std::clamp(u, -0.02f, 1.02f) * uw);
    sy = cell.top + static_cast<LONG>(kPad + (1.0f - std::clamp(v, -0.02f, 1.02f)) * uh);
}

struct OrthoFrameAxes {
    float cxA = 0.0f;
    float cxB = 0.0f;
    float halfSpan = 1.0f;
};

[[nodiscard]] OrthoFrameAxes ComputeOrthoFrame(const ri::scene::Scene& scene, RawIronFlatProjection projection) {
    OrthoFrameAxes out{};
    ri::scene::WorldBounds bounds = DefaultEditorBounds();
    if (const auto merged = TryMergeRenderableBounds(scene); merged.has_value()) {
        bounds = *merged;
    }
    const ri::math::Vec3 center = ri::scene::GetBoundsCenter(bounds);
    const ri::math::Vec3 size = ri::scene::GetBoundsSize(bounds);
    const float margin = std::max(ri::math::Length(size) * 0.08f, 1.25f);
    if (projection == RawIronFlatProjection::TopXz) {
        out.cxA = center.x;
        out.cxB = center.z;
        out.halfSpan = std::max(std::max(size.x, size.z) * 0.5f + margin, 6.0f);
    } else if (projection == RawIronFlatProjection::FrontXy) {
        out.cxA = center.x;
        out.cxB = center.y;
        out.halfSpan = std::max(std::max(size.x, size.y) * 0.5f + margin, 6.0f);
    } else {
        out.cxA = center.z;
        out.cxB = center.y;
        out.halfSpan = std::max(std::max(size.z, size.y) * 0.5f + margin, 6.0f);
    }
    return out;
}

void ScreenToRawIronTopInv(const RECT& plot, int mx, int my, float cx, float cz, float halfSpan, float& wx, float& wz) {
    constexpr float kPad = 5.0f;
    const float uw = static_cast<float>(plot.right - plot.left) - kPad * 2.0f;
    const float uh = static_cast<float>(plot.bottom - plot.top) - kPad * 2.0f;
    const float u = (static_cast<float>(mx) - static_cast<float>(plot.left) - kPad) / std::max(uw, 0.001f);
    const float v = 1.0f - (static_cast<float>(my) - static_cast<float>(plot.top) - kPad) / std::max(uh, 0.001f);
    const float uu = std::clamp(u, 0.0f, 1.0f);
    const float vv = std::clamp(v, 0.0f, 1.0f);
    wx = (cx - halfSpan) + uu * (halfSpan * 2.0f);
    wz = (cz - halfSpan) + vv * (halfSpan * 2.0f);
}

void ScreenToRawIronFrontInv(const RECT& plot,
                            int mx,
                            int my,
                            float cx,
                            float cy,
                            float halfSpan,
                            float& wx,
                            float& wy) {
    constexpr float kPad = 5.0f;
    const float uw = static_cast<float>(plot.right - plot.left) - kPad * 2.0f;
    const float uh = static_cast<float>(plot.bottom - plot.top) - kPad * 2.0f;
    const float u = (static_cast<float>(mx) - static_cast<float>(plot.left) - kPad) / std::max(uw, 0.001f);
    const float v = 1.0f - (static_cast<float>(my) - static_cast<float>(plot.top) - kPad) / std::max(uh, 0.001f);
    const float uu = std::clamp(u, 0.0f, 1.0f);
    const float vv = std::clamp(v, 0.0f, 1.0f);
    wx = (cx - halfSpan) + uu * (halfSpan * 2.0f);
    wy = (cy - halfSpan) + vv * (halfSpan * 2.0f);
}

void ScreenToRawIronSideInv(const RECT& plot,
                           int mx,
                           int my,
                           float cz,
                           float cy,
                           float halfSpan,
                           float& wz,
                           float& wy) {
    constexpr float kPad = 5.0f;
    const float uw = static_cast<float>(plot.right - plot.left) - kPad * 2.0f;
    const float uh = static_cast<float>(plot.bottom - plot.top) - kPad * 2.0f;
    const float u = (static_cast<float>(mx) - static_cast<float>(plot.left) - kPad) / std::max(uw, 0.001f);
    const float v = 1.0f - (static_cast<float>(my) - static_cast<float>(plot.top) - kPad) / std::max(uh, 0.001f);
    const float uu = std::clamp(u, 0.0f, 1.0f);
    const float vv = std::clamp(v, 0.0f, 1.0f);
    wz = (cz - halfSpan) + uu * (halfSpan * 2.0f);
    wy = (cy - halfSpan) + vv * (halfSpan * 2.0f);
}

[[nodiscard]] std::optional<int> PickRenderableInOrthoView(const RECT& plot,
                                                             RawIronFlatProjection projection,
                                                             int mx,
                                                             int my,
                                                             const ri::scene::Scene& scene) {
    if (plot.right <= plot.left + 4 || plot.bottom <= plot.top + 4) {
        return std::nullopt;
    }
    const OrthoFrameAxes frame = ComputeOrthoFrame(scene, projection);
    float a0 = 0.0f;
    float a1 = 0.0f;
    if (projection == RawIronFlatProjection::TopXz) {
        ScreenToRawIronTopInv(plot, mx, my, frame.cxA, frame.cxB, frame.halfSpan, a0, a1);
    } else if (projection == RawIronFlatProjection::FrontXy) {
        ScreenToRawIronFrontInv(plot, mx, my, frame.cxA, frame.cxB, frame.halfSpan, a0, a1);
    } else {
        ScreenToRawIronSideInv(plot, mx, my, frame.cxA, frame.cxB, frame.halfSpan, a0, a1);
    }

    float bestD2 = std::numeric_limits<float>::infinity();
    int best = ri::scene::kInvalidHandle;
    for (const int handle : ri::scene::CollectRenderableNodes(scene)) {
        const std::optional<ri::scene::WorldBounds> bounds =
            ri::scene::ComputeNodeWorldBounds(scene, handle, true);
        if (!bounds.has_value()) {
            continue;
        }

        bool inside = false;
        float pc0 = 0.0f;
        float pc1 = 0.0f;
        if (projection == RawIronFlatProjection::TopXz) {
            inside = a0 >= bounds->min.x && a0 <= bounds->max.x && a1 >= bounds->min.z && a1 <= bounds->max.z;
            const ri::math::Vec3 center = ri::scene::GetBoundsCenter(*bounds);
            pc0 = center.x;
            pc1 = center.z;
        } else if (projection == RawIronFlatProjection::FrontXy) {
            inside = a0 >= bounds->min.x && a0 <= bounds->max.x && a1 >= bounds->min.y && a1 <= bounds->max.y;
            const ri::math::Vec3 center = ri::scene::GetBoundsCenter(*bounds);
            pc0 = center.x;
            pc1 = center.y;
        } else {
            inside = a0 >= bounds->min.z && a0 <= bounds->max.z && a1 >= bounds->min.y && a1 <= bounds->max.y;
            const ri::math::Vec3 center = ri::scene::GetBoundsCenter(*bounds);
            pc0 = center.z;
            pc1 = center.y;
        }
        if (!inside) {
            continue;
        }

        const float dx = a0 - pc0;
        const float dy = a1 - pc1;
        const float dist2 = dx * dx + dy * dy;
        if (dist2 < bestD2) {
            bestD2 = dist2;
            best = handle;
        }
    }

    if (best == ri::scene::kInvalidHandle) {
        return std::nullopt;
    }
    return best;
}

[[nodiscard]] fs::path EditorOrbitSidecarPath(const fs::path& sceneStatePath) {
    return sceneStatePath.parent_path() / "editor_orbit.ri_cam";
}

bool TryLoadEditorOrbitSidecar(const fs::path& sceneStatePath, ri::scene::OrbitCameraState& out) {
    const fs::path path = EditorOrbitSidecarPath(sceneStatePath);
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return false;
    }
    std::string magic;
    stream >> magic;
    if (magic != "RAWIRON_EDITOR_ORBIT_V1") {
        return false;
    }
    stream >> out.target.x >> out.target.y >> out.target.z;
    stream >> out.distance;
    stream >> out.yawDegrees >> out.pitchDegrees;
    return stream.good();
}

bool TryLoadEditorOrbitStateFromPath(const fs::path& path, ri::scene::OrbitCameraState& out) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return false;
    }
    std::string magic;
    stream >> magic;
    if (magic != "RAWIRON_EDITOR_ORBIT_V1") {
        return false;
    }
    stream >> out.target.x >> out.target.y >> out.target.z;
    stream >> out.distance;
    stream >> out.yawDegrees >> out.pitchDegrees;
    return stream.good();
}

bool SaveEditorOrbitSidecar(const fs::path& sceneStatePath, const ri::scene::OrbitCameraState& orbit) {
    const fs::path path = EditorOrbitSidecarPath(sceneStatePath);
    std::error_code ec{};
    fs::create_directories(path.parent_path(), ec);
    (void)ec;
    std::ofstream stream(path, std::ios::trunc);
    if (!stream.is_open()) {
        return false;
    }
    stream << "RAWIRON_EDITOR_ORBIT_V1\n";
    stream << std::fixed << std::setprecision(8);
    stream << orbit.target.x << " " << orbit.target.y << " " << orbit.target.z << "\n";
    stream << orbit.distance << "\n";
    stream << orbit.yawDegrees << " " << orbit.pitchDegrees << "\n";
    return static_cast<bool>(stream);
}

bool SaveEditorOrbitStateToPath(const fs::path& path, const ri::scene::OrbitCameraState& orbit) {
    std::error_code ec{};
    fs::create_directories(path.parent_path(), ec);
    (void)ec;
    std::ofstream stream(path, std::ios::trunc);
    if (!stream.is_open()) {
        return false;
    }
    stream << "RAWIRON_EDITOR_ORBIT_V1\n";
    stream << std::fixed << std::setprecision(8);
    stream << orbit.target.x << " " << orbit.target.y << " " << orbit.target.z << "\n";
    stream << orbit.distance << "\n";
    stream << orbit.yawDegrees << " " << orbit.pitchDegrees << "\n";
    return static_cast<bool>(stream);
}

#if defined(_WIN32)
[[nodiscard]] fs::path ResolveEditorModulePath() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }
    return fs::path(buffer);
}

[[nodiscard]] fs::path FindBuildRoot(const fs::path& executablePath) {
    fs::path current = executablePath.parent_path();
    while (!current.empty()) {
        std::error_code ec{};
        if (fs::exists(current / "CMakeCache.txt", ec)) {
            return current;
        }
        const fs::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return {};
}

[[nodiscard]] std::optional<std::string> TryResolveCurrentBuildConfiguration(const fs::path& editorExe) {
    const fs::path parent = editorExe.parent_path();
    const fs::path owner = parent.parent_path();
    if (owner.filename() == "RawIron.Editor") {
        const std::string configuration = parent.filename().string();
        if (!configuration.empty()) {
            return configuration;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::string QuoteCommandLineArgument(std::string_view value) {
    if (value.find_first_of(" \t\"") == std::string_view::npos) {
        return std::string(value);
    }

    std::string quoted;
    quoted.reserve(value.size() + 8U);
    quoted.push_back('"');
    for (const char ch : value) {
        if (ch == '"') {
            quoted += "\\\"";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted.push_back('"');
    return quoted;
}

[[nodiscard]] std::wstring BuildShellExecuteParameterString(const std::vector<std::string>& args) {
    std::string joined;
    for (std::size_t index = 0; index < args.size(); ++index) {
        if (index > 0U) {
            joined.push_back(' ');
        }
        joined += QuoteCommandLineArgument(args[index]);
    }
    return Utf8ToWide(joined);
}

[[nodiscard]] fs::path ResolveBundledGameExecutable(const ri::content::GameManifest& manifest) {
    const fs::path editorExe = ResolveEditorModulePath();
    if (editorExe.empty()) {
        return {};
    }
    const fs::path buildRoot = FindBuildRoot(editorExe);
    if (buildRoot.empty()) {
        return {};
    }

    const std::optional<std::string> configuration = TryResolveCurrentBuildConfiguration(editorExe);
    std::vector<fs::path> candidates;
    const std::string executableName = manifest.entry + ".exe";
    const fs::path gameFolderName = manifest.rootPath.filename();

    const auto appendCandidate = [&candidates](const fs::path& candidate) {
        if (!candidate.empty()) {
            candidates.push_back(candidate);
        }
    };
    const auto appendConfigCandidate = [&appendCandidate, &configuration](fs::path base) {
        if (configuration.has_value()) {
            appendCandidate(base / *configuration);
        }
        appendCandidate(std::move(base));
    };

    appendConfigCandidate(buildRoot / "Games" / gameFolderName / "App" / executableName);
    appendConfigCandidate(buildRoot / "Apps" / manifest.entry / executableName);

    for (const fs::path& candidate : candidates) {
        std::error_code ec{};
        if (!candidate.empty() && fs::exists(candidate, ec)) {
            return fs::weakly_canonical(candidate, ec);
        }
    }
    return {};
}
#endif

void DrawRawIronOrthoGrid(HDC dc,
                         const RECT& cell,
                         RawIronFlatProjection projection,
                         float cxA,
                         float cxB,
                         float halfSpan) {
    const COLORREF lineA = RGB(105, 105, 105);
    const COLORREF lineB = RGB(88, 88, 88);
    const float step = PickNiceGridStep(halfSpan * 2.0f);
    const float startA = std::floor((cxA - halfSpan) / step) * step;
    const float startB = std::floor((cxB - halfSpan) / step) * step;

    for (float a = startA; a <= cxA + halfSpan + step * 0.5f; a += step) {
        LONG x1 = 0;
        LONG y1 = 0;
        LONG x2 = 0;
        LONG y2 = 0;
        if (projection == RawIronFlatProjection::TopXz) {
            ProjectRawIronTop(cell, cxA, cxB, halfSpan, a, cxB - halfSpan, x1, y1);
            ProjectRawIronTop(cell, cxA, cxB, halfSpan, a, cxB + halfSpan, x2, y2);
        } else if (projection == RawIronFlatProjection::FrontXy) {
            ProjectRawIronFront(cell, cxA, cxB, halfSpan, a, cxB - halfSpan, x1, y1);
            ProjectRawIronFront(cell, cxA, cxB, halfSpan, a, cxB + halfSpan, x2, y2);
        } else {
            ProjectRawIronSide(cell, cxA, cxB, halfSpan, a, cxB - halfSpan, x1, y1);
            ProjectRawIronSide(cell, cxA, cxB, halfSpan, a, cxB + halfSpan, x2, y2);
        }
        DcStrokeLine(dc, x1, y1, x2, y2, lineA, 1);
    }

    for (float b = startB; b <= cxB + halfSpan + step * 0.5f; b += step) {
        LONG x1 = 0;
        LONG y1 = 0;
        LONG x2 = 0;
        LONG y2 = 0;
        if (projection == RawIronFlatProjection::TopXz) {
            ProjectRawIronTop(cell, cxA, cxB, halfSpan, cxA - halfSpan, b, x1, y1);
            ProjectRawIronTop(cell, cxA, cxB, halfSpan, cxA + halfSpan, b, x2, y2);
        } else if (projection == RawIronFlatProjection::FrontXy) {
            ProjectRawIronFront(cell, cxA, cxB, halfSpan, cxA - halfSpan, b, x1, y1);
            ProjectRawIronFront(cell, cxA, cxB, halfSpan, cxA + halfSpan, b, x2, y2);
        } else {
            ProjectRawIronSide(cell, cxA, cxB, halfSpan, cxA - halfSpan, b, x1, y1);
            ProjectRawIronSide(cell, cxA, cxB, halfSpan, cxA + halfSpan, b, x2, y2);
        }
        DcStrokeLine(dc, x1, y1, x2, y2, lineB, 1);
    }
}

void DrawRawIronFlatSceneView(HDC dc,
                              const RECT& cell,
                              const ri::scene::Scene& scene,
                              std::size_t selectedNode,
                              const ri::math::Vec3& orbitFocus,
                              RawIronFlatProjection projection,
                              const char* title,
                              HFONT labelFont) {
    DrawInsetFrame(dc, cell, RGB(40, 40, 40), RGB(150, 150, 150), RGB(16, 16, 16));
    RECT inner{cell.left + 2, cell.top + 2, cell.right - 2, cell.bottom - 2};
    FillRectColor(dc, inner, RGB(56, 56, 56));

    DrawTextLine(dc,
                 RECT{inner.left + 6, inner.top + 4, inner.right - 6, inner.top + 22},
                 std::string(title),
                 RGB(255, 255, 200),
                 labelFont,
                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    RECT plot{inner.left + 4, inner.top + 24, inner.right - 4, inner.bottom - 4};
    if (plot.right <= plot.left + 8 || plot.bottom <= plot.top + 8) {
        return;
    }

    ri::scene::WorldBounds bounds = DefaultEditorBounds();
    if (const auto merged = TryMergeRenderableBounds(scene); merged.has_value()) {
        bounds = *merged;
    }

    const ri::math::Vec3 center = ri::scene::GetBoundsCenter(bounds);
    const ri::math::Vec3 size = ri::scene::GetBoundsSize(bounds);
    const float margin = std::max(ri::math::Length(size) * 0.08f, 1.25f);

    float cxA = 0.0f;
    float cxB = 0.0f;
    float halfSpan = 8.0f;
    if (projection == RawIronFlatProjection::TopXz) {
        cxA = center.x;
        cxB = center.z;
        halfSpan = std::max(std::max(size.x, size.z) * 0.5f + margin, 6.0f);
        DrawRawIronOrthoGrid(dc, plot, projection, cxA, cxB, halfSpan);
        LONG fx = 0;
        LONG fy = 0;
        ProjectRawIronTop(plot, cxA, cxB, halfSpan, orbitFocus.x, orbitFocus.z, fx, fy);
        DcStrokeLine(dc, fx - 6, fy, fx + 6, fy, RGB(255, 220, 80), 1);
        DcStrokeLine(dc, fx, fy - 6, fx, fy + 6, RGB(255, 220, 80), 1);
    } else if (projection == RawIronFlatProjection::FrontXy) {
        cxA = center.x;
        cxB = center.y;
        halfSpan = std::max(std::max(size.x, size.y) * 0.5f + margin, 6.0f);
        DrawRawIronOrthoGrid(dc, plot, projection, cxA, cxB, halfSpan);
        LONG fx = 0;
        LONG fy = 0;
        ProjectRawIronFront(plot, cxA, cxB, halfSpan, orbitFocus.x, orbitFocus.y, fx, fy);
        DcStrokeLine(dc, fx - 6, fy, fx + 6, fy, RGB(255, 220, 80), 1);
        DcStrokeLine(dc, fx, fy - 6, fx, fy + 6, RGB(255, 220, 80), 1);
    } else {
        cxA = center.z;
        cxB = center.y;
        halfSpan = std::max(std::max(size.z, size.y) * 0.5f + margin, 6.0f);
        DrawRawIronOrthoGrid(dc, plot, projection, cxA, cxB, halfSpan);
        LONG fx = 0;
        LONG fy = 0;
        ProjectRawIronSide(plot, cxA, cxB, halfSpan, orbitFocus.z, orbitFocus.y, fx, fy);
        DcStrokeLine(dc, fx - 6, fy, fx + 6, fy, RGB(255, 220, 80), 1);
        DcStrokeLine(dc, fx, fy - 6, fx, fy + 6, RGB(255, 220, 80), 1);
    }

    const std::vector<int> renderables = ri::scene::CollectRenderableNodes(scene);
    for (const int handle : renderables) {
        const std::optional<ri::scene::WorldBounds> nb = ri::scene::ComputeNodeWorldBounds(scene, handle, true);
        if (!nb.has_value()) {
            continue;
        }
        const bool isSelected = static_cast<std::size_t>(handle) == selectedNode;
        const COLORREF stroke = isSelected ? RGB(255, 255, 40) : RGB(200, 200, 200);
        const int penW = isSelected ? 2 : 1;

        const float minx = nb->min.x;
        const float maxx = nb->max.x;
        const float miny = nb->min.y;
        const float maxy = nb->max.y;
        const float minz = nb->min.z;
        const float maxz = nb->max.z;

        LONG ax = 0;
        LONG ay = 0;
        LONG bx = 0;
        LONG by = 0;
        LONG cx = 0;
        LONG cy = 0;
        LONG dx = 0;
        LONG dy = 0;

        if (projection == RawIronFlatProjection::TopXz) {
            ProjectRawIronTop(plot, cxA, cxB, halfSpan, minx, minz, ax, ay);
            ProjectRawIronTop(plot, cxA, cxB, halfSpan, maxx, minz, bx, by);
            ProjectRawIronTop(plot, cxA, cxB, halfSpan, maxx, maxz, cx, cy);
            ProjectRawIronTop(plot, cxA, cxB, halfSpan, minx, maxz, dx, dy);
        } else if (projection == RawIronFlatProjection::FrontXy) {
            ProjectRawIronFront(plot, cxA, cxB, halfSpan, minx, miny, ax, ay);
            ProjectRawIronFront(plot, cxA, cxB, halfSpan, maxx, miny, bx, by);
            ProjectRawIronFront(plot, cxA, cxB, halfSpan, maxx, maxy, cx, cy);
            ProjectRawIronFront(plot, cxA, cxB, halfSpan, minx, maxy, dx, dy);
        } else {
            ProjectRawIronSide(plot, cxA, cxB, halfSpan, minz, miny, ax, ay);
            ProjectRawIronSide(plot, cxA, cxB, halfSpan, maxz, miny, bx, by);
            ProjectRawIronSide(plot, cxA, cxB, halfSpan, maxz, maxy, cx, cy);
            ProjectRawIronSide(plot, cxA, cxB, halfSpan, minz, maxy, dx, dy);
        }

        DcStrokeLine(dc, ax, ay, bx, by, stroke, penW);
        DcStrokeLine(dc, bx, by, cx, cy, stroke, penW);
        DcStrokeLine(dc, cx, cy, dx, dy, stroke, penW);
        DcStrokeLine(dc, dx, dy, ax, ay, stroke, penW);
    }
}

struct AuthoringToolbarRects {
    RECT addCube{};
    RECT addPlane{};
    RECT exportCsv{};
    RECT play{};
};

struct StructuralBrushPreset {
    const char* label = "box";
    const char* structuralType = "box";
    int radialSegments = 16;
    int sides = 16;
    int hemisphereSegments = 6;
    float thickness = 0.16f;
    float topRadius = 0.18f;
    float bottomRadius = 0.5f;
    float length = 0.5f;
    float spanDegrees = 180.0f;
    float ridgeRatio = 0.34f;
    const char* archStyle = "round";
};

/// Structural brush presets used by the editor. Includes the native primitive set plus useful variants.
inline constexpr std::array<StructuralBrushPreset, 24> kStructuralBrushPresets{{
    {.label = "box", .structuralType = "box"},
    {.label = "plane", .structuralType = "plane"},
    {.label = "arch_round", .structuralType = "arch", .thickness = 0.16f, .spanDegrees = 180.0f, .archStyle = "round"},
    {.label = "arch_gothic", .structuralType = "arch", .thickness = 0.18f, .archStyle = "gothic"},
    {.label = "hollow_box", .structuralType = "hollow_box"},
    {.label = "ramp", .structuralType = "ramp"},
    {.label = "wedge", .structuralType = "wedge"},
    {.label = "cylinder", .structuralType = "cylinder", .radialSegments = 16},
    {.label = "cylinder_hi", .structuralType = "cylinder", .radialSegments = 24},
    {.label = "cone", .structuralType = "cone", .sides = 16},
    {.label = "pyramid", .structuralType = "pyramid", .sides = 4},
    {.label = "capsule", .structuralType = "capsule", .radialSegments = 16, .hemisphereSegments = 6, .length = 0.5f},
    {.label = "capsule_tall", .structuralType = "capsule", .radialSegments = 16, .hemisphereSegments = 8, .length = 1.2f},
    {.label = "frustum", .structuralType = "frustum", .radialSegments = 16, .topRadius = 0.18f, .bottomRadius = 0.5f},
    {.label = "frustum_wide", .structuralType = "frustum", .radialSegments = 16, .topRadius = 0.30f, .bottomRadius = 0.8f},
    {.label = "geodesic_sphere", .structuralType = "geodesic_sphere"},
    {.label = "hexahedron", .structuralType = "hexahedron"},
    {.label = "convex_hull", .structuralType = "convex_hull"},
    {.label = "roof_gable", .structuralType = "roof_gable"},
    {.label = "hipped_roof", .structuralType = "hipped_roof"},
    {.label = "roof_gable_sharp", .structuralType = "roof_gable", .ridgeRatio = 0.24f},
    {.label = "hipped_roof_flat", .structuralType = "hipped_roof", .ridgeRatio = 0.48f},
    {.label = "arch_round_wide", .structuralType = "arch", .thickness = 0.12f, .spanDegrees = 240.0f, .archStyle = "round"},
    {.label = "arch_round_thick", .structuralType = "arch", .thickness = 0.30f, .spanDegrees = 180.0f, .archStyle = "round"},
}};

[[nodiscard]] ri::structural::StructuralPrimitiveOptions StructuralShapeFromPreset(const StructuralBrushPreset& preset) {
    ri::structural::StructuralPrimitiveOptions shape{};
    shape.radialSegments = preset.radialSegments;
    shape.sides = preset.sides;
    shape.hemisphereSegments = preset.hemisphereSegments;
    shape.thickness = preset.thickness;
    shape.topRadius = preset.topRadius;
    shape.bottomRadius = preset.bottomRadius;
    shape.length = preset.length;
    shape.spanDegrees = preset.spanDegrees;
    shape.ridgeRatio = preset.ridgeRatio;
    shape.archStyle = preset.archStyle;
    return shape;
}

[[nodiscard]] std::string SanitizeBrushLabelForName(std::string_view label) {
    std::string out;
    out.reserve(label.size());
    for (const unsigned char ch : label) {
        if (std::isalnum(ch) != 0) {
            out.push_back(static_cast<char>(ch));
        } else if (ch == '_' || ch == '-') {
            out.push_back('_');
        }
    }
    if (out.empty()) {
        return "brush";
    }
    return out;
}

[[nodiscard]] ri::math::Vec3 StructuralBrushSpawnPosition(const std::string_view structuralType,
                                                            const ri::math::Vec3& orbitTarget) {
    if (structuralType == "plane") {
        return {orbitTarget.x, 0.0f, orbitTarget.z};
    }
    return {orbitTarget.x, orbitTarget.y + 0.5f, orbitTarget.z};
}

[[nodiscard]] AuthoringToolbarRects ComputeAuthoringToolbarRects(const RECT& toolStrip) {
    const LONG rowTop = toolStrip.top + 8;
    const LONG rowBot = toolStrip.bottom - 8;
    const LONG x0 = toolStrip.left + 578;
    AuthoringToolbarRects rects{};
    rects.addCube = {x0, rowTop, x0 + 88, rowBot};
    rects.addPlane = {x0 + 94, rowTop, x0 + 188, rowBot};
    rects.exportCsv = {x0 + 194, rowTop, x0 + 284, rowBot};
    rects.play = {x0 + 290, rowTop, x0 + 368, rowBot};
    return rects;
}

class RawIronEditorWindow {
public:
    explicit RawIronEditorWindow(const ri::core::CommandLine& commandLine)
        : logEveryFrame_(commandLine.HasFlag("--log-every-frame")),
          dumpScene_(commandLine.HasFlag("--dump-scene-every-frame")),
          sceneConfig_(ResolveSceneConfig(commandLine)),
          statsOverlayVisible_(commandLine.HasFlag("--stats-overlay")),
          statsOverlayState_(true),
          autoOrbitPreview_(commandLine.HasFlag("--auto-orbit")) {
        ri::editor::RegisterBundledGameEditorPreviews();
        starterScene_ = ri::editor::BuildEditorWorkspaceScene(
            sceneConfig_.editorPreviewScene,
            sceneConfig_.sceneName,
            sceneConfig_.gameManifest.has_value() ? sceneConfig_.gameManifest->rootPath : fs::path{});
        editorOrbitState_ = starterScene_.handles.orbitCamera.orbit;
        (void)TryLoadEditorOrbitSidecar(sceneConfig_.sceneStatePath, editorOrbitState_);
        ApplyEditorOrbitToScene();
        statsOverlayState_.SetAttached(true);
        statsOverlayState_.SetVisible(statsOverlayVisible_);
        if (!sceneConfig_.statusMessage.empty()) {
            lastIoStatus_ = sceneConfig_.statusMessage + "  ";
        }
        lastIoStatus_ += "Camera: drag in CAMERA, wheel zooms. Tab: full 3D / quad.";
        if (autoOrbitPreview_) {
            lastIoStatus_ += "  (--auto-orbit: demo camera motion on)";
        }
        RefreshWorkspaceGamesAndResources();
        EnsureEditorTrashFolder();
        lastAutosaveSteady_ = std::chrono::steady_clock::now();
        std::error_code ec{};
        if (fs::exists(ResolveAutosaveScenePath(), ec)) {
            lastIoStatus_ += "  Autosave found (Ctrl+Shift+L loads it).";
        }
    }

    int Run(HINSTANCE instance) {
        const wchar_t* className = L"RawIronEditorWindow";
        WNDCLASSW windowClass{};
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = &RawIronEditorWindow::WindowProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = className;
        windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&windowClass);

        hwnd_ = CreateWindowExW(
            0,
            className,
            Widen(sceneConfig_.windowTitle).c_str(),
            WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1520,
            900,
            nullptr,
            nullptr,
            instance,
            this);
        if (hwnd_ == nullptr) {
            return 1;
        }

        titleFont_ = CreateUiFont(-20, FW_BOLD, L"Segoe UI");
        headerFont_ = CreateUiFont(-15, FW_SEMIBOLD, L"Segoe UI");
        bodyFont_ = CreateUiFont(-14, FW_NORMAL, L"Segoe UI");
        smallFont_ = CreateUiFont(-12, FW_NORMAL, L"Segoe UI");

        SetTimer(hwnd_, 1, 33, nullptr);
        lastTick_ = std::chrono::steady_clock::now();

        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        DeleteObject(titleFont_);
        DeleteObject(headerFont_);
        DeleteObject(bodyFont_);
        DeleteObject(smallFont_);
        return static_cast<int>(message.wParam);
    }

private:
    enum class EditMode {
        Translate,
        Rotate,
        Scale,
    };
    enum class LeftPanelMode {
        Scene,
        Resources,
    };
    enum class InspectorPanel {
        Node,
        Brush,
        Gameplay,
        Files,
    };
    static constexpr int kHierarchyRowHeight_ = 25;
    static constexpr int kHierarchyBottomGutter_ = 26;
    static constexpr int kLeftPanelTabHeight_ = 24;
    static constexpr int kLeftPanelGameStripHeight_ = 28;
    static constexpr int kResourceListRowHeight_ = 22;
    static constexpr std::size_t kMaxResourceFileBytes_ = 512U * 1024U;
    struct EditorLayout {
        RECT toolStrip{};
        RECT hierarchy{};
        RECT hierarchyInner{};
        RECT viewport{};
        RECT viewportInner{};
        RECT inspectorInner{};
    };
    struct TransformEditAction {
        std::size_t nodeIndex = 0;
        ri::scene::Transform before{};
        ri::scene::Transform after{};
    };
    struct SceneGraphEditAction {
        ri::scene::Scene beforeScene{};
        ri::scene::Scene afterScene{};
        std::size_t beforeSelectedNode = 0;
        std::size_t afterSelectedNode = 0;
    };
    using EditorEditAction = std::variant<TransformEditAction, SceneGraphEditAction>;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        RawIronEditorWindow* self = nullptr;
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<LPCREATESTRUCTW>(lParam);
            self = static_cast<RawIronEditorWindow*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->hwnd_ = hwnd;
        } else {
            self = reinterpret_cast<RawIronEditorWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (self == nullptr) {
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }

        switch (message) {
            case WM_COMMAND: {
                if (self->resourceTextEditHwnd_ != nullptr
                    && reinterpret_cast<HWND>(lParam) == self->resourceTextEditHwnd_
                    && HIWORD(wParam) == EN_CHANGE) {
                    self->resourceFileDirty_ = true;
                }
            } break;
            case WM_TIMER:
                self->OnTick();
                return 0;
            case WM_KEYDOWN:
                return self->OnKeyDown(wParam);
            case WM_LBUTTONDOWN:
                return self->OnLeftButtonDown(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
            case WM_LBUTTONUP:
                return self->OnLeftButtonUp(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
            case WM_MOUSEMOVE:
                return self->OnMouseMove(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)), wParam);
            case WM_CAPTURECHANGED:
                self->OnCaptureLost();
                break;
            case WM_MOUSEWHEEL: {
                const short delta = static_cast<short>(HIWORD(wParam));
                const int screenX = static_cast<short>(LOWORD(lParam));
                const int screenY = static_cast<short>(HIWORD(lParam));
                if (self->OnMouseWheel(delta, screenX, screenY)) {
                    return 0;
                }
                break;
            }
            case WM_SIZE:
                InvalidateRect(hwnd, nullptr, FALSE);
                break;
            case WM_PAINT:
                self->Paint();
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_CLOSE:
                if (!self->ResolveDirtyResourceBeforeContextSwitch("closing the editor")) {
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                self->DestroyResourceTextEditorControl();
                PostQuitMessage(0);
                return 0;
            default:
                break;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    void OnTick() {
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> delta = now - lastTick_;
        lastTick_ = now;
        elapsedSeconds_ += delta.count();
        statsOverlayState_.RecordFrameDeltaSeconds(delta.count());
        statsOverlayState_.SetAttached(true);
        statsOverlayState_.SetVisible(statsOverlayVisible_);
        ri::editor::AnimateEditorWorkspaceScene(sceneConfig_.editorPreviewScene, starterScene_, elapsedSeconds_);
        if (!autoOrbitPreview_) {
            ApplyEditorOrbitToScene();
        } else {
            editorOrbitState_ = starterScene_.handles.orbitCamera.orbit;
        }
        MaybeAutosaveState();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void ApplyEditorOrbitToScene() {
        editorOrbitState_.pitchDegrees = std::clamp(editorOrbitState_.pitchDegrees, -85.0f, 85.0f);
        editorOrbitState_.distance = std::clamp(editorOrbitState_.distance, 0.75f, 180.0f);
        ri::scene::SetOrbitCameraState(starterScene_.scene, starterScene_.handles.orbitCamera, editorOrbitState_);
    }

    void RefreshWorkspaceGamesAndResources() {
        workspaceGames_ = EnumerateWorkspaceGames(sceneConfig_.workspaceRoot);
        focusedWorkspaceGameIndex_ = 0;
        if (!workspaceGames_.empty() && sceneConfig_.gameManifest.has_value()) {
            const std::string& wantId = sceneConfig_.gameManifest->id;
            for (std::size_t i = 0; i < workspaceGames_.size(); ++i) {
                if (workspaceGames_[i].id == wantId) {
                    focusedWorkspaceGameIndex_ = static_cast<int>(i);
                    break;
                }
            }
        }
        RefreshWorkspaceResourceRows();
    }

    void RefreshWorkspaceResourceRows() {
        if (!ResolveDirtyResourceBeforeContextSwitch("refreshing resources")) {
            if (hwnd_ != nullptr) {
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return;
        }
        resourceCatalogEntries_.clear();
        selectedResourceRow_ = -1;
        resourceCatalogScrollTopRow_ = 0;
        loadedResourceAbsolutePath_.clear();
        loadedResourceUtf8_.clear();
        resourceEditorAuxMessage_.clear();
        resourceFileDirty_ = false;
        DestroyResourceTextEditorControl();
        if (focusedWorkspaceGameIndex_ >= 0 &&
            focusedWorkspaceGameIndex_ < static_cast<int>(workspaceGames_.size())) {
            resourceCatalogEntries_ = CollectWorkspaceGameResources(
                workspaceGames_[static_cast<std::size_t>(focusedWorkspaceGameIndex_)].rootPath);
        }
        if (hwnd_ != nullptr) {
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }

    void SelectWorkspaceResourceRow(const int rowIndex) {
        if (rowIndex < 0 || rowIndex >= static_cast<int>(resourceCatalogEntries_.size())) {
            return;
        }
        if (rowIndex == selectedResourceRow_) {
            inspectorPanel_ = InspectorPanel::Files;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        if (!ResolveDirtyResourceBeforeContextSwitch("opening another resource")) {
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        selectedResourceRow_ = rowIndex;
        inspectorPanel_ = InspectorPanel::Files;
        loadedResourceUtf8_.clear();
        resourceEditorAuxMessage_.clear();
        resourceFileDirty_ = false;
        DestroyResourceTextEditorControl();

        const WorkspaceResourceEntry& entry =
            resourceCatalogEntries_[static_cast<std::size_t>(rowIndex)];
        loadedResourceAbsolutePath_ = entry.absolutePath;

        std::error_code ec{};
        const std::uintmax_t fileSize = fs::file_size(loadedResourceAbsolutePath_, ec);
        if (ec || fileSize > static_cast<std::uintmax_t>(kMaxResourceFileBytes_)) {
            resourceEditorAuxMessage_ =
                ec ? std::string("Unable to read file metadata.") : std::string("File too large for embedded editor.");
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }

        if (!IsLikelyTextResourcePath(loadedResourceAbsolutePath_)) {
            resourceEditorAuxMessage_ =
                "Binary / unknown extension — use Explorer to open beside the workspace.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }

        loadedResourceUtf8_ = ri::core::detail::ReadTextFile(loadedResourceAbsolutePath_);
        lastIoStatus_ = "Resource: " + entry.relativePathUtf8;
        {
            const EditorLayout layout = ComputeLayout();
            EnsureResourceTextEditorCreated();
            LayoutResourceTextEditorControl(layout.inspectorInner);
#if defined(_WIN32)
            if (resourceTextEditHwnd_ != nullptr) {
                SetFocus(resourceTextEditHwnd_);
            }
#endif
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void DestroyResourceTextEditorControl() {
#if defined(_WIN32)
        if (resourceTextEditHwnd_ != nullptr) {
            DestroyWindow(resourceTextEditHwnd_);
            resourceTextEditHwnd_ = nullptr;
        }
#endif
    }

#if defined(_WIN32)
    void EnsureResourceTextEditorCreated() {
        if (resourceTextEditHwnd_ != nullptr || hwnd_ == nullptr) {
            return;
        }
        if (inspectorPanel_ != InspectorPanel::Files || !resourceEditorAuxMessage_.empty()) {
            return;
        }
        if (loadedResourceAbsolutePath_.empty() || !IsLikelyTextResourcePath(loadedResourceAbsolutePath_)) {
            return;
        }
        resourceTextEditHwnd_ =
            CreateWindowExW(WS_EX_CLIENTEDGE,
                            L"EDIT",
                            Utf8ToWide(loadedResourceUtf8_).c_str(),
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE |
                                ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
                            0,
                            0,
                            10,
                            10,
                            hwnd_,
                            reinterpret_cast<HMENU>(static_cast<ULONG_PTR>(2051)),
                            GetModuleHandleW(nullptr),
                            nullptr);
        if (resourceTextEditHwnd_ != nullptr) {
            SendMessageW(resourceTextEditHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont_), MAKELPARAM(TRUE, 0));
            resourceFileDirty_ = false;
            SetFocus(resourceTextEditHwnd_);
        }
    }

    void LayoutResourceTextEditorControl(const RECT& inspectorInner) {
        if (resourceTextEditHwnd_ == nullptr || hwnd_ == nullptr) {
            return;
        }
        const bool visible = inspectorPanel_ == InspectorPanel::Files && resourceEditorAuxMessage_.empty()
            && !loadedResourceAbsolutePath_.empty() && IsLikelyTextResourcePath(loadedResourceAbsolutePath_);
        if (!visible) {
            ShowWindow(resourceTextEditHwnd_, SW_HIDE);
            return;
        }
        constexpr int kInspectorMetaHeight = 118;
        const int top = inspectorInner.top + kInspectorMetaHeight;
        const int bottom = static_cast<int>(inspectorInner.bottom) - 10;
        ShowWindow(resourceTextEditHwnd_, SW_SHOW);
        MoveWindow(resourceTextEditHwnd_,
                   inspectorInner.left + 10,
                   top,
                   std::max(40, static_cast<int>(inspectorInner.right - inspectorInner.left - 20)),
                   std::max(40, bottom - top),
                   TRUE);
        SetWindowPos(resourceTextEditHwnd_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }

    [[nodiscard]] bool SaveActiveResourceFileFromEditor() {
        if (resourceTextEditHwnd_ == nullptr || loadedResourceAbsolutePath_.empty()) {
            return false;
        }
        const int len = GetWindowTextLengthW(resourceTextEditHwnd_);
        std::wstring wide(static_cast<std::size_t>(len + 2), L'\0');
        const int copied =
            len <= 0 ? 0 : GetWindowTextW(resourceTextEditHwnd_, wide.data(), len + 1);
        if (copied < 0) {
            return false;
        }
        wide.resize(static_cast<std::size_t>(std::max(0, copied)));
        const std::string utf8 = WideToUtf8(wide);
        if (!ri::core::detail::WriteTextFile(loadedResourceAbsolutePath_, utf8)) {
            return false;
        }
        loadedResourceUtf8_ = utf8;
        resourceFileDirty_ = false;
        lastIoStatus_ = "Saved resource: " + loadedResourceAbsolutePath_.filename().string();
        return true;
    }

    void OpenActiveResourceInExplorer() const {
        if (loadedResourceAbsolutePath_.empty()) {
            return;
        }
        std::wstring arg = L"/select,\"";
        arg += loadedResourceAbsolutePath_.wstring();
        arg += L"\"";
        ShellExecuteW(hwnd_, L"open", L"explorer.exe", arg.c_str(), nullptr, SW_SHOWNORMAL);
    }
#else
    void EnsureResourceTextEditorCreated() {
    }
    void LayoutResourceTextEditorControl(const RECT& /*inspectorInner*/) {
    }
    [[nodiscard]] bool SaveActiveResourceFileFromEditor() {
        return false;
    }
    void OpenActiveResourceInExplorer() const {
    }
#endif

    [[nodiscard]] bool ResolveDirtyResourceBeforeContextSwitch(std::string_view action) {
        if (!resourceFileDirty_) {
            return true;
        }

        const std::string fileName = loadedResourceAbsolutePath_.empty()
            ? std::string("resource")
            : loadedResourceAbsolutePath_.filename().string();

#if defined(_WIN32)
        if (hwnd_ != nullptr) {
            const std::string prompt =
                "Save changes to '" + fileName + "' before " + std::string(action) +
                "?\n\nChoose No to discard the unsaved edits, or Cancel to keep editing.";
            const int choice = MessageBoxW(hwnd_,
                                           Widen(prompt).c_str(),
                                           L"RawIron Editor - Unsaved Resource",
                                           MB_ICONWARNING | MB_YESNOCANCEL);
            if (choice == IDCANCEL) {
                lastIoStatus_ = "Canceled " + std::string(action) + " to keep editing " + fileName + ".";
                return false;
            }
            if (choice == IDNO) {
                resourceFileDirty_ = false;
                lastIoStatus_ = "Discarded unsaved edits to " + fileName + ".";
                return true;
            }
        }
#endif

        if (SaveActiveResourceFileFromEditor()) {
            lastIoStatus_ = "Saved " + fileName + " before " + std::string(action) + ".";
            return true;
        }

        lastIoStatus_ = "Save failed for " + fileName + "; kept resource open.";
        return false;
    }

    [[nodiscard]] bool SetInspectorPanel(InspectorPanel panel) {
        if (inspectorPanel_ == panel) {
            return true;
        }
        if (inspectorPanel_ == InspectorPanel::Files && panel != InspectorPanel::Files) {
            if (!ResolveDirtyResourceBeforeContextSwitch("switching inspector tabs")) {
                return false;
            }
            DestroyResourceTextEditorControl();
        }
        inspectorPanel_ = panel;
        return true;
    }

    [[nodiscard]] int LeftPanelContentTop(const RECT& hierarchyInner) const {
        int top = hierarchyInner.top + 6 + kLeftPanelTabHeight_;
        if (leftPanelMode_ == LeftPanelMode::Resources) {
            top += kLeftPanelGameStripHeight_ + 4;
        }
        return top;
    }

    [[nodiscard]] int LeftPanelSceneListBottom(const RECT& hierarchyInner) const {
        return hierarchyInner.bottom - kHierarchyBottomGutter_;
    }

    [[nodiscard]] int CountVisibleSceneRows(const RECT& hierarchyInner) const {
        const int h =
            std::max(0, LeftPanelSceneListBottom(hierarchyInner) - LeftPanelContentTop(hierarchyInner) - 8);
        return std::max(1, h / kHierarchyRowHeight_);
    }

    [[nodiscard]] int CountVisibleResourceRows(const RECT& hierarchyInner) const {
        const int h =
            std::max(0, LeftPanelSceneListBottom(hierarchyInner) - LeftPanelContentTop(hierarchyInner) - 8);
        return std::max(1, h / kResourceListRowHeight_);
    }

    void UpdateCameraPlotRect(const RECT& viewportInner) {
        constexpr int kBannerHeight = 24;
        constexpr int kMetaStrip = 26;
        const RECT menuBanner{viewportInner.left + 4,
                              viewportInner.top + 6,
                              viewportInner.right - 4,
                              viewportInner.top + 6 + kBannerHeight};
        const RECT quadArea{viewportInner.left + 4,
                            menuBanner.bottom + 4,
                            viewportInner.right - 4,
                            viewportInner.bottom - 4 - kMetaStrip};

        if (full3DViewport_) {
            RECT plot{quadArea.left + 6, quadArea.top + 22, quadArea.right - 6, quadArea.bottom - 6};
            if (plot.right > plot.left + 8 && plot.bottom > plot.top + 8) {
                cameraPlotRect_ = plot;
            } else {
                cameraPlotRect_ = quadArea;
            }
            return;
        }

        if (quadArea.right <= quadArea.left + 32 || quadArea.bottom <= quadArea.top + 32) {
            cameraPlotRect_ = quadArea;
            return;
        }

        const int midX = (quadArea.left + quadArea.right) / 2;
        const int midY = (quadArea.top + quadArea.bottom) / 2;
        const RECT cellCamera{midX + 1, midY + 1, quadArea.right, quadArea.bottom};
        const RECT cameraInner{cellCamera.left + 2, cellCamera.top + 2, cellCamera.right - 2, cellCamera.bottom - 2};
        RECT plot{cameraInner.left + 4, cameraInner.top + 24, cameraInner.right - 4, cameraInner.bottom - 4};
        if (plot.right > plot.left + 4 && plot.bottom > plot.top + 4) {
            cameraPlotRect_ = plot;
        } else {
            cameraPlotRect_ = cameraInner;
        }
    }

    [[nodiscard]] std::vector<int> HierarchyDrawOrder() const {
        return BuildHierarchyDrawOrder(starterScene_.scene, editorTrashFolderHandle_);
    }

    void EnsureHierarchySelectionVisible(const RECT& hierarchyInner) {
        const std::vector<int> order = HierarchyDrawOrder();
        if (order.empty()) {
            hierarchyScrollTopRow_ = 0;
            return;
        }
        const std::optional<int> pos = FindDrawOrderIndex(order, selectedNode_);
        const int listTop = LeftPanelContentTop(hierarchyInner);
        const int listBottom = LeftPanelSceneListBottom(hierarchyInner);
        const int innerHeight = std::max(0, listBottom - listTop - 8);
        const int visibleRows = std::max(1, innerHeight / kHierarchyRowHeight_);
        const int maxScroll = std::max(0, static_cast<int>(order.size()) - visibleRows);
        hierarchyScrollTopRow_ = std::clamp(hierarchyScrollTopRow_, 0, maxScroll);
        if (!pos.has_value()) {
            return;
        }
        if (*pos < hierarchyScrollTopRow_) {
            hierarchyScrollTopRow_ = *pos;
        } else if (*pos >= hierarchyScrollTopRow_ + visibleRows) {
            hierarchyScrollTopRow_ = *pos - visibleRows + 1;
        }
        hierarchyScrollTopRow_ = std::clamp(hierarchyScrollTopRow_, 0, maxScroll);
    }

    [[nodiscard]] bool OnMouseWheel(short wheelDelta, int screenX, int screenY) {
        POINT point{screenX, screenY};
        ScreenToClient(hwnd_, &point);
        const EditorLayout layout = ComputeLayout();
        UpdateCameraPlotRect(layout.viewportInner);
        if (PtInRect(&cameraPlotRect_, point) != FALSE) {
            if (!autoOrbitPreview_) {
                const float steps = static_cast<float>(wheelDelta) / static_cast<float>(WHEEL_DELTA);
                const float factor = std::exp(-steps * 0.14f);
                editorOrbitState_.distance *= factor;
                ApplyEditorOrbitToScene();
                lastIoStatus_ = "Camera: zoom.";
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (PtInRect(&layout.hierarchyInner, point) == FALSE) {
            return false;
        }

        const int notches = std::max(1, std::abs(wheelDelta) / WHEEL_DELTA);
        const int step = 3 * notches;
        if (leftPanelMode_ == LeftPanelMode::Resources) {
            const int visibleRows = CountVisibleResourceRows(layout.hierarchyInner);
            const int maxScroll =
                std::max(0, static_cast<int>(resourceCatalogEntries_.size()) - visibleRows);
            resourceCatalogScrollTopRow_ += wheelDelta > 0 ? -step : step;
            resourceCatalogScrollTopRow_ = std::clamp(resourceCatalogScrollTopRow_, 0, maxScroll);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }

        const std::vector<int> order = HierarchyDrawOrder();
        const int visibleRows = CountVisibleSceneRows(layout.hierarchyInner);
        const int maxScroll = std::max(0, static_cast<int>(order.size()) - visibleRows);
        hierarchyScrollTopRow_ += wheelDelta > 0 ? -step : step;
        hierarchyScrollTopRow_ = std::clamp(hierarchyScrollTopRow_, 0, maxScroll);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return true;
    }

    LRESULT OnKeyDown(WPARAM key) {
        const bool controlHeld = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const bool altHeld = (GetKeyState(VK_MENU) & 0x8000) != 0;
        const EditorLayout layoutForNav = ComputeLayout();
        const std::vector<int> drawOrder = HierarchyDrawOrder();
        const int visibleSceneRows = CountVisibleSceneRows(layoutForNav.hierarchyInner);
        const int maxSceneScroll = std::max(0, static_cast<int>(drawOrder.size()) - visibleSceneRows);
        const int visibleResRows = CountVisibleResourceRows(layoutForNav.hierarchyInner);
        const int maxResourceScroll =
            std::max(0, static_cast<int>(resourceCatalogEntries_.size()) - visibleResRows);

        if (leftPanelMode_ == LeftPanelMode::Resources) {
            if (key == VK_PRIOR) {
                resourceCatalogScrollTopRow_ = std::max(0, resourceCatalogScrollTopRow_ - visibleResRows);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            if (key == VK_NEXT) {
                resourceCatalogScrollTopRow_ =
                    std::min(maxResourceScroll, resourceCatalogScrollTopRow_ + visibleResRows);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            if (!resourceCatalogEntries_.empty()) {
                if (key == VK_UP) {
                    const int next = selectedResourceRow_ <= 0
                        ? static_cast<int>(resourceCatalogEntries_.size()) - 1
                        : selectedResourceRow_ - 1;
                    SelectWorkspaceResourceRow(next);
                    return 0;
                }
                if (key == VK_DOWN) {
                    const int next = selectedResourceRow_ + 1 >= static_cast<int>(resourceCatalogEntries_.size())
                        ? 0
                        : selectedResourceRow_ + 1;
                    SelectWorkspaceResourceRow(next);
                    return 0;
                }
                if (key == VK_HOME) {
                    SelectWorkspaceResourceRow(0);
                    return 0;
                }
                if (key == VK_END) {
                    SelectWorkspaceResourceRow(static_cast<int>(resourceCatalogEntries_.size()) - 1);
                    return 0;
                }
            }
        } else {
            if (key == VK_PRIOR) {
                hierarchyScrollTopRow_ = std::max(0, hierarchyScrollTopRow_ - visibleSceneRows);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            if (key == VK_NEXT) {
                hierarchyScrollTopRow_ =
                    std::min(maxSceneScroll, hierarchyScrollTopRow_ + visibleSceneRows);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }

            if (key == VK_UP && !drawOrder.empty()) {
                const std::optional<int> currentPos = FindDrawOrderIndex(drawOrder, selectedNode_);
                int nextPos = 0;
                if (currentPos.has_value() && *currentPos > 0) {
                    nextPos = *currentPos - 1;
                } else {
                    nextPos = static_cast<int>(drawOrder.size()) - 1;
                }
                selectedNode_ = static_cast<std::size_t>(drawOrder[static_cast<std::size_t>(nextPos)]);
                EnsureHierarchySelectionVisible(layoutForNav.hierarchyInner);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            if (key == VK_DOWN && !drawOrder.empty()) {
                const std::optional<int> currentPos = FindDrawOrderIndex(drawOrder, selectedNode_);
                int nextPos = 0;
                if (currentPos.has_value() && *currentPos + 1 < static_cast<int>(drawOrder.size())) {
                    nextPos = *currentPos + 1;
                } else {
                    nextPos = 0;
                }
                selectedNode_ = static_cast<std::size_t>(drawOrder[static_cast<std::size_t>(nextPos)]);
                EnsureHierarchySelectionVisible(layoutForNav.hierarchyInner);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            if (key == VK_HOME && !drawOrder.empty()) {
                selectedNode_ = static_cast<std::size_t>(drawOrder.front());
                EnsureHierarchySelectionVisible(layoutForNav.hierarchyInner);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            if (key == VK_END && !drawOrder.empty()) {
                selectedNode_ = static_cast<std::size_t>(drawOrder.back());
                EnsureHierarchySelectionVisible(layoutForNav.hierarchyInner);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
        }
        if (key == VK_TAB) {
            full3DViewport_ = !full3DViewport_;
            lastIoStatus_ = full3DViewport_ ? "Layout: full 3D (Tab for quad views)." : "Layout: quad views (Tab for full 3D).";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == VK_SPACE) {
            autoOrbitPreview_ = !autoOrbitPreview_;
            lastIoStatus_ = autoOrbitPreview_ ? "Camera: auto-orbit preview ON." : "Camera: auto-orbit preview OFF.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && shiftHeld && key == 'Q') {
            DestroyWindow(hwnd_);
            return 0;
        }
        if (key == VK_ESCAPE) {
            if (static_cast<int>(selectedNode_) != starterScene_.handles.root &&
                starterScene_.handles.root != ri::scene::kInvalidHandle) {
                selectedNode_ = static_cast<std::size_t>(starterScene_.handles.root);
                lastIoStatus_ = "Selection cleared (World root). Ctrl+Shift+Q exits the editor.";
            } else {
                lastIoStatus_ = "Tip: Ctrl+Shift+Q to exit · Del removes authored mesh nodes.";
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == VK_F6) {
            statsOverlayVisible_ = !statsOverlayVisible_;
            statsOverlayState_.SetVisible(statsOverlayVisible_);
            lastIoStatus_ = statsOverlayVisible_
                ? "Diagnostics overlay visible."
                : "Diagnostics overlay hidden.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && key == 'Z') {
            lastIoStatus_ = UndoLastEdit() ? "Undo applied." : "Nothing to undo.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && key == 'Y') {
            lastIoStatus_ = RedoLastEdit() ? "Redo applied." : "Nothing to redo.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && shiftHeld && key == 'S') {
            TrySaveTimestampedSceneSnapshot();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && key == 'S') {
            if (inspectorPanel_ == InspectorPanel::Files && resourceFileDirty_) {
                lastIoStatus_ =
                    SaveActiveResourceFileFromEditor() ? "Saved active resource file." : "Failed to save resource file.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            const bool sceneSaved = ri::scene::SaveSceneNodeTransforms(starterScene_.scene, ResolveSceneStatePath());
            const bool orbitSaved = SaveEditorOrbitSidecar(ResolveSceneStatePath(), editorOrbitState_);
            if (sceneSaved && orbitSaved) {
                lastIoStatus_ = "Saved scene transforms and orbit camera.";
                autosavePending_ = false;
                lastAutosaveSteady_ = std::chrono::steady_clock::now();
            } else if (sceneSaved) {
                lastIoStatus_ = "Saved scene transforms (orbit sidecar failed).";
            } else {
                lastIoStatus_ = "Failed to save editor scene state.";
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && key == 'E') {
            TryExportAssemblyPrimitivesCsv();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && key == 'R') {
            TryResetSelectedTransform();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && shiftHeld && key == 'C') {
            AddAuthoringPrimitive(ri::scene::PrimitiveType::Cube);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && shiftHeld && key == 'P') {
            AddAuthoringPrimitive(ri::scene::PrimitiveType::Plane);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && shiftHeld && key == 'B') {
            SpawnStructuralBrushAtFocus();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && shiftHeld && key >= '1' && key <= '9') {
            SelectStructuralBrushPresetByDigit(static_cast<int>(key - '0'));
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && key == 'D') {
            TryDuplicateSelectedNode();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && shiftHeld && key == 'N') {
            TryCreateGroupNode();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && shiftHeld && key == 'G') {
            TryUngroupSelectedNode();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && key == 'G') {
            TryGroupSelectedNode();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && shiftHeld && key == 'W') {
            TryReparentSelectedToWorldRoot();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == VK_OEM_4) {
            CycleStructuralBrushPreset(-1);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == VK_OEM_6) {
            CycleStructuralBrushPreset(1);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == VK_DELETE) {
            TryDeleteSelectedNode();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && shiftHeld && key == 'L') {
            TryLoadAutosaveState();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (controlHeld && key == 'L') {
            const bool sceneLoaded = ri::scene::LoadSceneNodeTransforms(starterScene_.scene, ResolveSceneStatePath());
            const bool orbitLoaded = TryLoadEditorOrbitSidecar(sceneConfig_.sceneStatePath, editorOrbitState_);
            if (orbitLoaded) {
                ApplyEditorOrbitToScene();
            }
            if (sceneLoaded && orbitLoaded) {
                lastIoStatus_ = "Loaded scene transforms and orbit camera.";
            } else if (sceneLoaded) {
                lastIoStatus_ = "Loaded scene transforms (no orbit sidecar).";
            } else {
                lastIoStatus_ = "Failed to load editor scene state.";
            }
            autosavePending_ = false;
            lastAutosaveSteady_ = std::chrono::steady_clock::now();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (shiftHeld && key == 'F') {
            TryFrameAllRenderables();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == 'F') {
            if (!autoOrbitPreview_ && selectedNode_ < starterScene_.scene.NodeCount()) {
                const std::vector<int> handles = {static_cast<int>(selectedNode_)};
                if (ri::scene::FrameNodesWithOrbitCamera(starterScene_.scene,
                                                         starterScene_.handles.orbitCamera,
                                                         handles,
                                                         1.35f)) {
                    editorOrbitState_ = starterScene_.handles.orbitCamera.orbit;
                    ApplyEditorOrbitToScene();
                    lastIoStatus_ = "Framed selection in orbit camera (Ctrl+S saves orbit).";
                } else {
                    lastIoStatus_ = "Could not frame selection.";
                }
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == 'T') {
            editMode_ = EditMode::Translate;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == 'R') {
            editMode_ = EditMode::Rotate;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == 'U') {
            editMode_ = EditMode::Scale;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == 'X') {
            activeAxis_ = 0;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == 'Y') {
            activeAxis_ = 1;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == 'Z') {
            activeAxis_ = 2;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == '1') {
            if (!SetInspectorPanel(InspectorPanel::Node)) {
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == '2') {
            if (!SetInspectorPanel(InspectorPanel::Brush)) {
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == '3') {
            if (!SetInspectorPanel(InspectorPanel::Gameplay)) {
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == '4') {
            (void)SetInspectorPanel(InspectorPanel::Files);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (!controlHeld && !shiftHeld && key == VK_OEM_COMMA) {
            TrySelectAdjacentAuthoredNode(-1);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (!controlHeld && !shiftHeld && key == VK_OEM_PERIOD) {
            TrySelectAdjacentAuthoredNode(1);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == 'I' && inspectorPanel_ == InspectorPanel::Gameplay) {
            CycleInventoryPresentation();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == 'O' && inspectorPanel_ == InspectorPanel::Gameplay) {
            creatorInventoryPolicy_.allowOffHand = !creatorInventoryPolicy_.allowOffHand;
            lastIoStatus_ = creatorInventoryPolicy_.allowOffHand ? "Gameplay policy: off-hand enabled." : "Gameplay policy: off-hand disabled.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (!controlHeld && editMode_ == EditMode::Translate &&
            (key == 'W' || key == 'A' || key == 'S' || key == 'D' || key == 'Q' || key == 'E')) {
            if (IsProtectedEditorNode(static_cast<int>(selectedNode_))) {
                lastIoStatus_ = "Edit blocked: select an authored node, not World/rigs/helpers.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            const float step = ApplyStepModifiers(0.1f, shiftHeld, altHeld);
            ri::math::Vec3 delta{0.0f, 0.0f, 0.0f};
            switch (key) {
                case 'W': delta.z += step; break;
                case 'S': delta.z -= step; break;
                case 'A': delta.x -= step; break;
                case 'D': delta.x += step; break;
                case 'E': delta.y += step; break;
                case 'Q': delta.y -= step; break;
            }
            ApplySelectedNodeTranslationDelta(delta);
            lastIoStatus_ = shiftHeld ? "Translate: WASDQE (fine)." :
                (altHeld ? "Translate: WASDQE (coarse)." : "Translate: WASDQE.");
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (!controlHeld && editMode_ == EditMode::Rotate &&
            (key == 'W' || key == 'A' || key == 'S' || key == 'D' || key == 'Q' || key == 'E')) {
            if (IsProtectedEditorNode(static_cast<int>(selectedNode_))) {
                lastIoStatus_ = "Edit blocked: select an authored node, not World/rigs/helpers.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            const float step = ApplyStepModifiers(2.5f, shiftHeld, altHeld);
            ri::math::Vec3 delta{0.0f, 0.0f, 0.0f};
            switch (key) {
                case 'W': delta.x += step; break;
                case 'S': delta.x -= step; break;
                case 'A': delta.y -= step; break;
                case 'D': delta.y += step; break;
                case 'Q': delta.z -= step; break;
                case 'E': delta.z += step; break;
            }
            ApplySelectedNodeRotationDelta(delta);
            lastIoStatus_ = shiftHeld ? "Rotate: WASDQE (fine)." :
                (altHeld ? "Rotate: WASDQE (coarse)." : "Rotate: WASDQE.");
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (!controlHeld && editMode_ == EditMode::Scale &&
            (key == 'W' || key == 'A' || key == 'S' || key == 'D' || key == 'Q' || key == 'E')) {
            if (IsProtectedEditorNode(static_cast<int>(selectedNode_))) {
                lastIoStatus_ = "Edit blocked: select an authored node, not World/rigs/helpers.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            const float step = ApplyStepModifiers(0.08f, shiftHeld, altHeld);
            ri::math::Vec3 delta{0.0f, 0.0f, 0.0f};
            switch (key) {
                case 'W': delta.x += step; break;
                case 'S': delta.x -= step; break;
                case 'A': delta.y -= step; break;
                case 'D': delta.y += step; break;
                case 'Q': delta.z -= step; break;
                case 'E': delta.z += step; break;
            }
            if (selectedNode_ < starterScene_.scene.NodeCount()) {
                ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(selectedNode_));
                const ri::scene::Transform before = node.localTransform;
                node.localTransform.scale.x = std::max(0.01f, node.localTransform.scale.x + delta.x);
                node.localTransform.scale.y = std::max(0.01f, node.localTransform.scale.y + delta.y);
                node.localTransform.scale.z = std::max(0.01f, node.localTransform.scale.z + delta.z);
                const ri::scene::Transform after = node.localTransform;
                PushEditAction(TransformEditAction{selectedNode_, before, after});
            }
            lastIoStatus_ = shiftHeld ? "Scale: WASDQE (fine)." :
                (altHeld ? "Scale: WASDQE (coarse)." : "Scale: WASDQE.");
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == 'W' || key == 'S') {
            if (IsProtectedEditorNode(static_cast<int>(selectedNode_))) {
                lastIoStatus_ = "Edit blocked: select an authored node, not World/rigs/helpers.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            const float step = ApplyStepModifiers(0.08f, shiftHeld, altHeld);
            const float direction = key == 'W' ? 1.0f : -1.0f;
            ApplySelectedNodeEdit(direction * step);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        return 0;
    }

    LRESULT OnLeftButtonDown(int x, int y) {
        const POINT pick{x, y};
#if defined(_WIN32)
        const HWND hitChild =
            ChildWindowFromPointEx(hwnd_, pick, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED);
        if (hitChild != nullptr && hitChild != hwnd_) {
            SetFocus(hitChild);
        } else {
            SetFocus(hwnd_);
        }
#else
        SetFocus(hwnd_);
#endif
        const EditorLayout layout = ComputeLayout();
        UpdateCameraPlotRect(layout.viewportInner);
        POINT point{x, y};

        const auto hitRect = [&point](const RECT& rect) {
            return PtInRect(&rect, point) != FALSE;
        };

        const RECT translateButton{layout.toolStrip.left + 194, layout.toolStrip.top + 8, layout.toolStrip.left + 268, layout.toolStrip.bottom - 8};
        const RECT rotateButton{layout.toolStrip.left + 274, layout.toolStrip.top + 8, layout.toolStrip.left + 342, layout.toolStrip.bottom - 8};
        const RECT scaleButton{layout.toolStrip.left + 348, layout.toolStrip.top + 8, layout.toolStrip.left + 416, layout.toolStrip.bottom - 8};
        const RECT axisXButton{layout.toolStrip.left + 422, layout.toolStrip.top + 8, layout.toolStrip.left + 468, layout.toolStrip.bottom - 8};
        const RECT axisYButton{layout.toolStrip.left + 474, layout.toolStrip.top + 8, layout.toolStrip.left + 520, layout.toolStrip.bottom - 8};
        const RECT axisZButton{layout.toolStrip.left + 526, layout.toolStrip.top + 8, layout.toolStrip.left + 572, layout.toolStrip.bottom - 8};
        const AuthoringToolbarRects authoringTools = ComputeAuthoringToolbarRects(layout.toolStrip);

        if (hitRect(translateButton)) {
            editMode_ = EditMode::Translate;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (hitRect(rotateButton)) {
            editMode_ = EditMode::Rotate;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (hitRect(scaleButton)) {
            editMode_ = EditMode::Scale;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (hitRect(axisXButton)) {
            activeAxis_ = 0;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (hitRect(axisYButton)) {
            activeAxis_ = 1;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (hitRect(axisZButton)) {
            activeAxis_ = 2;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        if (hitRect(authoringTools.addCube)) {
            AddAuthoringPrimitive(ri::scene::PrimitiveType::Cube);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (hitRect(authoringTools.addPlane)) {
            AddAuthoringPrimitive(ri::scene::PrimitiveType::Plane);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (hitRect(authoringTools.exportCsv)) {
            TryExportAssemblyPrimitivesCsv();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (hitRect(authoringTools.play)) {
            TryLaunchPlayer();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        const RECT sceneTabLeft{layout.hierarchyInner.left + 6,
                                layout.hierarchyInner.top + 4,
                                layout.hierarchyInner.left + 78,
                                layout.hierarchyInner.top + 4 + kLeftPanelTabHeight_};
        const RECT resourcesTabLeft{layout.hierarchyInner.left + 82,
                                    layout.hierarchyInner.top + 4,
                                    layout.hierarchyInner.left + 190,
                                    layout.hierarchyInner.top + 4 + kLeftPanelTabHeight_};
        if (hitRect(sceneTabLeft)) {
            leftPanelMode_ = LeftPanelMode::Scene;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (hitRect(resourcesTabLeft)) {
            leftPanelMode_ = LeftPanelMode::Resources;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        const int tabStripBottom = layout.hierarchyInner.top + 4 + kLeftPanelTabHeight_;
        if (leftPanelMode_ == LeftPanelMode::Resources && !workspaceGames_.empty()) {
            const RECT gamePrev{layout.hierarchyInner.left + 6,
                                tabStripBottom + 4,
                                layout.hierarchyInner.left + 34,
                                tabStripBottom + 4 + kLeftPanelGameStripHeight_};
            const RECT gameNext{layout.hierarchyInner.right - 34,
                                  tabStripBottom + 4,
                                  layout.hierarchyInner.right - 6,
                                  tabStripBottom + 4 + kLeftPanelGameStripHeight_};
            if (hitRect(gamePrev)) {
                if (!ResolveDirtyResourceBeforeContextSwitch("switching games")) {
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    return 0;
                }
                focusedWorkspaceGameIndex_ =
                    (focusedWorkspaceGameIndex_ - 1 + static_cast<int>(workspaceGames_.size()))
                    % static_cast<int>(workspaceGames_.size());
                RefreshWorkspaceResourceRows();
                lastIoStatus_ =
                    "Game: " + workspaceGames_[static_cast<std::size_t>(focusedWorkspaceGameIndex_)].displayName;
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            if (hitRect(gameNext)) {
                if (!ResolveDirtyResourceBeforeContextSwitch("switching games")) {
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    return 0;
                }
                focusedWorkspaceGameIndex_ =
                    (focusedWorkspaceGameIndex_ + 1) % static_cast<int>(workspaceGames_.size());
                RefreshWorkspaceResourceRows();
                lastIoStatus_ =
                    "Game: " + workspaceGames_[static_cast<std::size_t>(focusedWorkspaceGameIndex_)].displayName;
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
        }

        if (hitRect(layout.hierarchyInner)) {
            const int listTop = LeftPanelContentTop(layout.hierarchyInner);
            const int listBottom = LeftPanelSceneListBottom(layout.hierarchyInner);
            const int listHeight = std::max(0, listBottom - listTop - 8);
            const int relativeY = y - listTop;
            if (relativeY >= 0 && relativeY < listHeight) {
                if (leftPanelMode_ == LeftPanelMode::Resources) {
                    const int row = resourceCatalogScrollTopRow_ + (relativeY / kResourceListRowHeight_);
                    if (row >= 0 && row < static_cast<int>(resourceCatalogEntries_.size())) {
                        SelectWorkspaceResourceRow(row);
                        return 0;
                    }
                } else {
                    const std::vector<int> order = HierarchyDrawOrder();
                    const int row = hierarchyScrollTopRow_ + (relativeY / kHierarchyRowHeight_);
                    if (row >= 0 && row < static_cast<int>(order.size())) {
                        selectedNode_ = static_cast<std::size_t>(order[static_cast<std::size_t>(row)]);
                        EnsureHierarchySelectionVisible(layout.hierarchyInner);
                        InvalidateRect(hwnd_, nullptr, FALSE);
                        return 0;
                    }
                }
            }
        }

        const RECT nodeTab{layout.inspectorInner.left + 6, layout.inspectorInner.top + 6, layout.inspectorInner.left + 68, layout.inspectorInner.top + 30};
        const RECT brushTab{layout.inspectorInner.left + 72, layout.inspectorInner.top + 6, layout.inspectorInner.left + 138, layout.inspectorInner.top + 30};
        const RECT gameplayTab{layout.inspectorInner.left + 142, layout.inspectorInner.top + 6, layout.inspectorInner.left + 218, layout.inspectorInner.top + 30};
        const RECT filesTab{layout.inspectorInner.left + 222, layout.inspectorInner.top + 6, layout.inspectorInner.left + 286, layout.inspectorInner.top + 30};
        if (hitRect(nodeTab)) {
            if (!SetInspectorPanel(InspectorPanel::Node)) {
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (hitRect(brushTab)) {
            if (!SetInspectorPanel(InspectorPanel::Brush)) {
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (hitRect(gameplayTab)) {
            if (!SetInspectorPanel(InspectorPanel::Gameplay)) {
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (hitRect(filesTab)) {
            (void)SetInspectorPanel(InspectorPanel::Files);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        if (inspectorPanel_ == InspectorPanel::Brush) {
            const RECT brushPresetPrev{layout.inspectorInner.left + 10,
                                         layout.inspectorInner.top + 42,
                                         layout.inspectorInner.left + 52,
                                         layout.inspectorInner.top + 66};
            const RECT brushPresetNext{layout.inspectorInner.left + 56,
                                         layout.inspectorInner.top + 42,
                                         layout.inspectorInner.left + 98,
                                         layout.inspectorInner.top + 66};
            if (hitRect(brushPresetPrev)) {
                CycleStructuralBrushPreset(-1);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            if (hitRect(brushPresetNext)) {
                CycleStructuralBrushPreset(1);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
        }

        if (inspectorPanel_ == InspectorPanel::Files) {
            const RECT saveResourceBtn{
                layout.inspectorInner.left + 10,
                layout.inspectorInner.top + 66,
                layout.inspectorInner.left + 108,
                layout.inspectorInner.top + 92};
            const RECT explorerBtn{layout.inspectorInner.left + 114,
                                     layout.inspectorInner.top + 66,
                                     layout.inspectorInner.right - 10,
                                     layout.inspectorInner.top + 92};
            if (hitRect(saveResourceBtn)) {
                lastIoStatus_ = SaveActiveResourceFileFromEditor() ? "Saved resource file." : "Save failed.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            if (hitRect(explorerBtn)) {
                OpenActiveResourceInExplorer();
                lastIoStatus_ = "Opened Explorer beside resource.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
        }

        if (inspectorPanel_ == InspectorPanel::Gameplay) {
            const RECT inventoryModeRow{layout.inspectorInner.left + 10, layout.inspectorInner.top + 66, layout.inspectorInner.right - 10, layout.inspectorInner.top + 88};
            const RECT offHandRow{layout.inspectorInner.left + 10, layout.inspectorInner.top + 126, layout.inspectorInner.right - 10, layout.inspectorInner.top + 148};
            if (hitRect(inventoryModeRow)) {
                CycleInventoryPresentation();
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            if (hitRect(offHandRow)) {
                creatorInventoryPolicy_.allowOffHand = !creatorInventoryPolicy_.allowOffHand;
                lastIoStatus_ = creatorInventoryPolicy_.allowOffHand ? "Gameplay policy: off-hand enabled." : "Gameplay policy: off-hand disabled.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
        }

        if (!full3DViewport_ && TryRawIronOrthoSelectAt(x, y, layout)) {
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        if (hitRect(cameraPlotRect_) && !autoOrbitPreview_) {
            cameraDragActive_ = true;
            lastDragX_ = x;
            lastDragY_ = y;
            SetCapture(hwnd_);
            lastIoStatus_ = "Camera: drag to orbit (release to stop).";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        return 0;
    }

    LRESULT OnLeftButtonUp(int x, int y) {
        (void)x;
        (void)y;
        if (cameraDragActive_) {
            cameraDragActive_ = false;
            if (GetCapture() == hwnd_) {
                ReleaseCapture();
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    }

    LRESULT OnMouseMove(int x, int y, WPARAM flags) {
        if (cameraDragActive_ && (flags & MK_LBUTTON) != 0 && !autoOrbitPreview_) {
            const int dx = x - lastDragX_;
            const int dy = y - lastDragY_;
            lastDragX_ = x;
            lastDragY_ = y;
            editorOrbitState_.yawDegrees += static_cast<float>(dx) * 0.38f;
            editorOrbitState_.pitchDegrees -= static_cast<float>(dy) * 0.38f;
            ApplyEditorOrbitToScene();
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    }

    void OnCaptureLost() {
        cameraDragActive_ = false;
    }

    [[nodiscard]] bool TryRawIronOrthoSelectAt(int x, int y, const EditorLayout& layout) {
        constexpr int kBannerHeight = 24;
        constexpr int kMetaStrip = 26;
        const RECT menuBanner{layout.viewportInner.left + 4,
                              layout.viewportInner.top + 6,
                              layout.viewportInner.right - 4,
                              layout.viewportInner.top + 6 + kBannerHeight};
        const RECT quadArea{layout.viewportInner.left + 4,
                            menuBanner.bottom + 4,
                            layout.viewportInner.right - 4,
                            layout.viewportInner.bottom - 4 - kMetaStrip};
        if (quadArea.right <= quadArea.left + 32 || quadArea.bottom <= quadArea.top + 32) {
            return false;
        }

        const POINT point{x, y};
        const int midX = (quadArea.left + quadArea.right) / 2;
        const int midY = (quadArea.top + quadArea.bottom) / 2;
        const RECT cellTop{quadArea.left, quadArea.top, midX - 1, midY - 1};
        const RECT cellSide{midX + 1, quadArea.top, quadArea.right, midY - 1};
        const RECT cellFront{quadArea.left, midY + 1, midX - 1, quadArea.bottom};
        const auto plotRect = [](const RECT& cell) {
            const RECT inner{cell.left + 2, cell.top + 2, cell.right - 2, cell.bottom - 2};
            return RECT{inner.left + 4, inner.top + 24, inner.right - 4, inner.bottom - 4};
        };

        const RECT plotTop = plotRect(cellTop);
        const RECT plotSide = plotRect(cellSide);
        const RECT plotFront = plotRect(cellFront);

        if (PtInRect(&plotTop, point) != FALSE) {
            if (const auto handle =
                    PickRenderableInOrthoView(plotTop, RawIronFlatProjection::TopXz, x, y, starterScene_.scene)) {
                selectedNode_ = static_cast<std::size_t>(*handle);
                lastIoStatus_ = "TOP: selected renderable.";
                EnsureHierarchySelectionVisible(layout.hierarchyInner);
                return true;
            }
            return false;
        }
        if (PtInRect(&plotSide, point) != FALSE) {
            if (const auto handle =
                    PickRenderableInOrthoView(plotSide, RawIronFlatProjection::SideZy, x, y, starterScene_.scene)) {
                selectedNode_ = static_cast<std::size_t>(*handle);
                lastIoStatus_ = "SIDE: selected renderable.";
                EnsureHierarchySelectionVisible(layout.hierarchyInner);
                return true;
            }
            return false;
        }
        if (PtInRect(&plotFront, point) != FALSE) {
            if (const auto handle =
                    PickRenderableInOrthoView(plotFront, RawIronFlatProjection::FrontXy, x, y, starterScene_.scene)) {
                selectedNode_ = static_cast<std::size_t>(*handle);
                lastIoStatus_ = "FRONT: selected renderable.";
                EnsureHierarchySelectionVisible(layout.hierarchyInner);
                return true;
            }
            return false;
        }
        return false;
    }

    void TryLaunchPlayer() {
#if defined(_WIN32)
        std::optional<ri::content::GameManifest> targetManifest{};
        if (!workspaceGames_.empty() && focusedWorkspaceGameIndex_ >= 0 &&
            focusedWorkspaceGameIndex_ < static_cast<int>(workspaceGames_.size())) {
            targetManifest = ri::content::LoadGameManifest(
                workspaceGames_[static_cast<std::size_t>(focusedWorkspaceGameIndex_)].rootPath / "manifest.json");
        }
        if (!targetManifest.has_value()) {
            targetManifest = sceneConfig_.gameManifest;
        }
        if (!targetManifest.has_value()) {
            lastIoStatus_ = "Play: no project manifest is active.";
            return;
        }

        const fs::path gameExe = ResolveBundledGameExecutable(*targetManifest);
        if (gameExe.empty()) {
            lastIoStatus_ = "Play: could not find built executable for " + targetManifest->entry + ".";
            return;
        }

        std::vector<std::string> args{};
        args.push_back("--game-root");
        args.push_back(PathToUtf8(targetManifest->rootPath));
        args.push_back("--workspace-root");
        args.push_back(PathToUtf8(sceneConfig_.workspaceRoot));

        const std::wstring exePath = gameExe.wstring();
        const std::wstring parameters = BuildShellExecuteParameterString(args);
        const std::wstring cwd = sceneConfig_.workspaceRoot.wstring();
        const HINSTANCE result =
            ShellExecuteW(hwnd_,
                          L"open",
                          exePath.c_str(),
                          parameters.empty() ? nullptr : parameters.c_str(),
                          cwd.empty() ? nullptr : cwd.c_str(),
                          SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) > 32) {
            lastIoStatus_ = "Play: launched " + targetManifest->name + ".";
        } else {
            lastIoStatus_ = "Play: could not start " + targetManifest->entry + ".";
        }
#else
        lastIoStatus_ = "Play: supported on Windows builds.";
#endif
    }

    [[nodiscard]] fs::path ResolveSceneStatePath() const {
        return sceneConfig_.sceneStatePath;
    }

    void TryExportAssemblyPrimitivesCsv() {
        fs::path outputPath;
        std::string destinationSummary;
        if (!workspaceGames_.empty() && focusedWorkspaceGameIndex_ >= 0 &&
            focusedWorkspaceGameIndex_ < static_cast<int>(workspaceGames_.size())) {
            const WorkspaceGameEntry& game = workspaceGames_[static_cast<std::size_t>(focusedWorkspaceGameIndex_)];
            outputPath = game.rootPath / "levels" / "assembly.primitives.csv";
            destinationSummary = game.displayName + " → levels/assembly.primitives.csv";
        } else {
            outputPath = ResolveSceneStatePath().parent_path() / "assembly.primitives.export.csv";
            destinationSummary = "editor session folder (no workspace game focused)";
        }

        std::error_code ec{};
        fs::create_directories(outputPath.parent_path(), ec);
        if (ec) {
            lastIoStatus_ = "Export primitives: could not create folder " + outputPath.parent_path().string();
            return;
        }

        std::string error;
        if (!ri::scene::TryExportAssemblyPrimitivesCsv(starterScene_.scene, starterScene_.handles.root, outputPath,
                                                       &error)) {
            lastIoStatus_ = "Export primitives failed: " + error;
            return;
        }

        std::size_t customRenderableCount = 0;
        for (const int handle : ri::scene::CollectRenderableNodes(starterScene_.scene)) {
            const ri::scene::Node& node = starterScene_.scene.GetNode(handle);
            if (node.mesh == ri::scene::kInvalidHandle) {
                continue;
            }
            const ri::scene::Mesh& mesh = starterScene_.scene.GetMesh(node.mesh);
            if (mesh.primitive == ri::scene::PrimitiveType::Custom) {
                customRenderableCount += 1U;
            }
        }

        lastIoStatus_ = "Exported game-format assembly primitives (" + destinationSummary + "). Full path: " +
                        outputPath.string();
        if (customRenderableCount > 0U) {
            lastIoStatus_ += "  Skipped custom/brush meshes: " + std::to_string(customRenderableCount) +
                             " (CSV supports cube/plane rows only).";
        }
    }

    [[nodiscard]] std::string NextAuthoringPrimitiveBasename(const std::string_view prefix) const {
        int maxIndex = 0;
        for (const ri::scene::Node& node : starterScene_.scene.Nodes()) {
            const std::string& name = node.name;
            if (name.size() <= prefix.size()) {
                continue;
            }
            if (name.compare(0, prefix.size(), prefix) != 0) {
                continue;
            }
            int parsed = 0;
            bool anyDigit = false;
            bool bad = false;
            for (std::size_t i = prefix.size(); i < name.size(); ++i) {
                const char ch = name[i];
                if (ch < '0' || ch > '9') {
                    bad = true;
                    break;
                }
                anyDigit = true;
                parsed = parsed * 10 + static_cast<int>(ch - '0');
            }
            if (!bad && anyDigit) {
                maxIndex = std::max(maxIndex, parsed);
            }
        }
        return std::string(prefix) + std::to_string(maxIndex + 1);
    }

    [[nodiscard]] std::string NextStructuralBrushBasename(const std::string_view structuralType) const {
        const std::string prefix = std::string("Brush_") + std::string(structuralType) + "_";
        int maxIndex = 0;
        for (const ri::scene::Node& node : starterScene_.scene.Nodes()) {
            const std::string& name = node.name;
            if (name.size() <= prefix.size()) {
                continue;
            }
            if (name.compare(0, prefix.size(), prefix) != 0) {
                continue;
            }
            int parsed = 0;
            bool anyDigit = false;
            bool bad = false;
            for (std::size_t i = prefix.size(); i < name.size(); ++i) {
                const char ch = name[i];
                if (ch < '0' || ch > '9') {
                    bad = true;
                    break;
                }
                anyDigit = true;
                parsed = parsed * 10 + static_cast<int>(ch - '0');
            }
            if (!bad && anyDigit) {
                maxIndex = std::max(maxIndex, parsed);
            }
        }
        return prefix + std::to_string(maxIndex + 1);
    }

    [[nodiscard]] const StructuralBrushPreset& CurrentStructuralBrushPreset() const {
        return kStructuralBrushPresets[structuralBrushPresetIndex_ % kStructuralBrushPresets.size()];
    }

    void AddAuthoringPrimitive(const ri::scene::PrimitiveType primitive) {
        if (starterScene_.handles.root == ri::scene::kInvalidHandle) {
            lastIoStatus_ = "Cannot add primitive: scene has no world root.";
            return;
        }

        ri::scene::PrimitiveNodeOptions options{};
        options.parent = starterScene_.handles.root;
        options.primitive = primitive;
        options.shadingModel = ri::scene::ShadingModel::Lit;
        options.textureTiling = ri::math::Vec2{2.0f, 2.0f};
        options.baseColorTexture = "ri_psx_wall_vent.png";

        const ri::math::Vec3 focus = starterScene_.handles.orbitCamera.orbit.target;
        if (primitive == ri::scene::PrimitiveType::Cube) {
            options.nodeName = NextAuthoringPrimitiveBasename("Block_");
            options.materialName = "author_block";
            options.baseColor = ri::math::Vec3{0.62f, 0.66f, 0.72f};
            options.transform.position = ri::math::Vec3{focus.x, focus.y + 0.5f, focus.z};
            options.transform.scale = ri::math::Vec3{1.0f, 1.0f, 1.0f};
        } else if (primitive == ri::scene::PrimitiveType::Plane) {
            options.nodeName = NextAuthoringPrimitiveBasename("Slab_");
            options.materialName = "author_slab";
            options.baseColor = ri::math::Vec3{0.42f, 0.48f, 0.44f};
            options.transform.position = ri::math::Vec3{focus.x, 0.0f, focus.z};
            options.transform.scale = ri::math::Vec3{6.0f, 1.0f, 6.0f};
        } else {
            lastIoStatus_ = "Export pipeline currently authors cube/plane primitives only.";
            return;
        }

        const int newHandle = ri::scene::AddPrimitiveNode(starterScene_.scene, options);
        selectedNode_ = static_cast<std::size_t>(newHandle);
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        lastIoStatus_ = "Added " + options.nodeName + " (" + ri::scene::ToString(primitive) +
                        ") under World. Adjust with WASDQE (Shift fine / Alt coarse) · Export with Ctrl+E.";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void CycleStructuralBrushPreset(const int delta) {
        const int n = static_cast<int>(kStructuralBrushPresets.size());
        int idx = static_cast<int>(structuralBrushPresetIndex_);
        idx += delta;
        idx %= n;
        if (idx < 0) {
            idx += n;
        }
        structuralBrushPresetIndex_ = static_cast<std::size_t>(idx);
        const StructuralBrushPreset& preset = CurrentStructuralBrushPreset();
        lastIoStatus_ =
            "Structural brush preset [" + std::to_string(static_cast<int>(structuralBrushPresetIndex_) + 1) +
            "/" + std::to_string(kStructuralBrushPresets.size()) + "]: " + std::string(preset.label) +
            " (" + std::string(preset.structuralType) + ")  ([ / ] cycle · Ctrl+Shift+1..9 quick select · Ctrl+Shift+B place)";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void SelectStructuralBrushPresetByDigit(const int oneBasedDigit) {
        if (oneBasedDigit < 1 || oneBasedDigit > 9) {
            return;
        }
        const std::size_t idx = static_cast<std::size_t>(oneBasedDigit - 1);
        if (idx >= kStructuralBrushPresets.size()) {
            lastIoStatus_ = "No structural preset bound to Ctrl+Shift+" + std::to_string(oneBasedDigit) + ".";
            return;
        }
        structuralBrushPresetIndex_ = idx;
        const StructuralBrushPreset& preset = CurrentStructuralBrushPreset();
        lastIoStatus_ = "Selected structural preset " + std::string(preset.label) + " (" +
                        std::string(preset.structuralType) + ") via Ctrl+Shift+" + std::to_string(oneBasedDigit) + ".";
    }

    void SpawnStructuralBrushAtFocus() {
        if (starterScene_.handles.root == ri::scene::kInvalidHandle) {
            lastIoStatus_ = "Cannot spawn brush: scene has no world root.";
            return;
        }
        if (!SetInspectorPanel(InspectorPanel::Brush)) {
            return;
        }
        const StructuralBrushPreset& preset = CurrentStructuralBrushPreset();
        const std::string_view type = preset.structuralType;
        ri::scene::StructuralBrushSpawnOptions opt{};
        opt.structuralType = type;
        opt.shape = StructuralShapeFromPreset(preset);
        opt.parent = starterScene_.handles.root;
        opt.nodeName = NextStructuralBrushBasename(SanitizeBrushLabelForName(preset.label));
        opt.transform.position =
            StructuralBrushSpawnPosition(type, starterScene_.handles.orbitCamera.orbit.target);
        opt.materialName = std::string("brush_") + SanitizeBrushLabelForName(preset.label);
        opt.baseColorTexture = "ri_psx_wall_vent.png";
        opt.textureTiling = ri::math::Vec2{2.0f, 2.0f};
        opt.baseColor = ri::math::Vec3{0.58f, 0.62f, 0.68f};

        const int newHandle = ri::scene::AddStructuralBrushNode(starterScene_.scene, opt);
        if (newHandle == ri::scene::kInvalidHandle) {
            lastIoStatus_ =
                "Structural brush '" + std::string(type) + "' produced no mesh (internal compiler issue).";
            return;
        }
        selectedNode_ = static_cast<std::size_t>(newHandle);
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        lastIoStatus_ = "Placed structural brush '" + std::string(preset.label) + "' (" +
                        std::string(preset.structuralType) + ") as " + opt.nodeName +
                        " (Custom mesh). Game CSV export lists procedural cube/plane rows only.";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void EnsureEditorTrashFolder() {
        if (editorTrashFolderHandle_ >= 0) {
            return;
        }
        if (starterScene_.handles.root == ri::scene::kInvalidHandle) {
            return;
        }
        editorTrashFolderHandle_ =
            starterScene_.scene.CreateNode("EditorTrash", starterScene_.handles.root);
    }

    [[nodiscard]] bool IsProtectedEditorNode(const int handle) const {
        if (handle < 0) {
            return true;
        }
        const ri::scene::StarterSceneHandles& handles = starterScene_.handles;
        if (handle == handles.root || handle == handles.sun || handle == handles.grid ||
            handle == handles.floor) {
            return true;
        }
        if (handle == handles.orbitCamera.root || handle == handles.orbitCamera.swivel ||
            handle == handles.orbitCamera.cameraNode) {
            return true;
        }
        if (handle == handles.axes.root || handle == handles.axes.xAxis || handle == handles.axes.yAxis ||
            handle == handles.axes.zAxis) {
            return true;
        }
        if (handle == editorTrashFolderHandle_) {
            return true;
        }
        return false;
    }

    [[nodiscard]] std::string MakeUniqueNodeName(const std::string& baseName) const {
        auto nameExists = [this](const std::string& candidate) {
            for (const ri::scene::Node& node : starterScene_.scene.Nodes()) {
                if (node.name == candidate) {
                    return true;
                }
            }
            return false;
        };

        std::string candidate = baseName;
        if (!nameExists(candidate)) {
            return candidate;
        }
        for (int suffix = 1; suffix < 10000; ++suffix) {
            candidate = baseName + "_" + std::to_string(suffix);
            if (!nameExists(candidate)) {
                return candidate;
            }
        }
        return baseName + "_node";
    }

    [[nodiscard]] static ri::math::Vec3 EulerDegreesFromRotation3x3(const ri::math::Mat4& rotationOnly) {
        const float sy = std::sqrt(rotationOnly.m[0][0] * rotationOnly.m[0][0] +
                                   rotationOnly.m[1][0] * rotationOnly.m[1][0]);
        constexpr float kRadToDeg = 180.0f / ri::math::kPi;
        if (sy > 1.0e-6f) {
            const float x = std::atan2(rotationOnly.m[2][1], rotationOnly.m[2][2]);
            const float y = std::atan2(-rotationOnly.m[2][0], sy);
            const float z = std::atan2(rotationOnly.m[1][0], rotationOnly.m[0][0]);
            return ri::math::Vec3{x * kRadToDeg, y * kRadToDeg, z * kRadToDeg};
        }
        const float x = std::atan2(-rotationOnly.m[1][2], rotationOnly.m[1][1]);
        const float y = std::atan2(-rotationOnly.m[2][0], sy);
        return ri::math::Vec3{x * kRadToDeg, y * kRadToDeg, 0.0f};
    }

    [[nodiscard]] static ri::scene::Transform TransformFromMatrix(const ri::math::Mat4& matrix) {
        ri::scene::Transform transform{};
        transform.position = ri::math::ExtractTranslation(matrix);
        transform.scale = ri::math::ExtractScale(matrix);

        ri::math::Mat4 rotationOnly = matrix;
        for (int column = 0; column < 3; ++column) {
            const float scaleValue = column == 0
                ? transform.scale.x
                : (column == 1 ? transform.scale.y : transform.scale.z);
            const float inverseScale = std::fabs(scaleValue) > 1.0e-8f ? 1.0f / scaleValue : 0.0f;
            rotationOnly.m[0][column] *= inverseScale;
            rotationOnly.m[1][column] *= inverseScale;
            rotationOnly.m[2][column] *= inverseScale;
        }
        transform.rotationDegrees = EulerDegreesFromRotation3x3(rotationOnly);
        return transform;
    }

    [[nodiscard]] bool TryAssignLocalTransformFromWorld(const int nodeHandle, const ri::math::Mat4& worldMatrix) {
        if (nodeHandle < 0 || static_cast<std::size_t>(nodeHandle) >= starterScene_.scene.NodeCount()) {
            return false;
        }
        const int parent = starterScene_.scene.GetNode(nodeHandle).parent;
        if (parent == ri::scene::kInvalidHandle) {
            starterScene_.scene.GetNode(nodeHandle).localTransform = TransformFromMatrix(worldMatrix);
            return true;
        }
        ri::math::Mat4 parentWorldInverse{};
        if (!ri::math::TryInvertAffineMat4(starterScene_.scene.ComputeWorldMatrix(parent), parentWorldInverse)) {
            return false;
        }
        const ri::math::Mat4 local = ri::math::Multiply(parentWorldInverse, worldMatrix);
        starterScene_.scene.GetNode(nodeHandle).localTransform = TransformFromMatrix(local);
        return true;
    }

    [[nodiscard]] float ApplyStepModifiers(const float baseStep, const bool fine, const bool coarse) const {
        if (fine) {
            return baseStep * 0.25f;
        }
        if (coarse) {
            return baseStep * 4.0f;
        }
        return baseStep;
    }

    [[nodiscard]] bool IsEditableAuthoredNode(const int handle) const {
        return handle >= 0 && static_cast<std::size_t>(handle) < starterScene_.scene.NodeCount() &&
            !IsProtectedEditorNode(handle);
    }

    void TryResetSelectedTransform() {
        if (!IsEditableAuthoredNode(static_cast<int>(selectedNode_))) {
            lastIoStatus_ = "Reset blocked: select an authored node, not World/rigs/helpers.";
            return;
        }
        ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(selectedNode_));
        const ri::scene::Transform before = node.localTransform;
        node.localTransform = ri::scene::Transform{};
        const ri::scene::Transform after = node.localTransform;
        PushEditAction(TransformEditAction{selectedNode_, before, after});
        lastIoStatus_ = "Reset selected node transform.";
    }

    void TryFrameAllRenderables() {
        if (autoOrbitPreview_) {
            lastIoStatus_ = "Frame all unavailable while auto-orbit preview is running.";
            return;
        }
        const std::vector<int> handles = ri::scene::CollectRenderableNodes(starterScene_.scene);
        if (handles.empty()) {
            lastIoStatus_ = "Frame all: no renderable nodes.";
            return;
        }
        if (ri::scene::FrameNodesWithOrbitCamera(starterScene_.scene,
                                                 starterScene_.handles.orbitCamera,
                                                 handles,
                                                 1.25f)) {
            editorOrbitState_ = starterScene_.handles.orbitCamera.orbit;
            ApplyEditorOrbitToScene();
            lastIoStatus_ = "Framed all renderables in orbit camera.";
        } else {
            lastIoStatus_ = "Frame all failed.";
        }
    }

    void TryReparentSelectedToWorldRoot() {
        const ri::scene::Scene beforeScene = starterScene_.scene;
        const std::size_t beforeSelectedNode = selectedNode_;
        if (!IsEditableAuthoredNode(static_cast<int>(selectedNode_))) {
            lastIoStatus_ = "Reparent blocked: select an authored node.";
            return;
        }
        if (starterScene_.handles.root == ri::scene::kInvalidHandle) {
            lastIoStatus_ = "Reparent failed: scene has no world root.";
            return;
        }
        const int nodeHandle = static_cast<int>(selectedNode_);
        if (starterScene_.scene.GetNode(nodeHandle).parent == starterScene_.handles.root) {
            lastIoStatus_ = "Reparent: node is already under World.";
            return;
        }
        const ri::math::Mat4 world = starterScene_.scene.ComputeWorldMatrix(nodeHandle);
        if (!starterScene_.scene.SetParent(nodeHandle, starterScene_.handles.root)) {
            lastIoStatus_ = "Reparent failed: could not attach to World.";
            return;
        }
        if (!TryAssignLocalTransformFromWorld(nodeHandle, world)) {
            lastIoStatus_ = "Reparent warning: parent changed, but world transform was not preserved.";
            return;
        }
        lastIoStatus_ = "Reparented selected node under World (world transform preserved).";
        RecordSceneGraphEdit(beforeScene, beforeSelectedNode);
    }

    void TrySelectAdjacentAuthoredNode(const int direction) {
        const std::vector<int> order = HierarchyDrawOrder();
        if (order.empty()) {
            return;
        }
        int startIndex = -1;
        for (std::size_t i = 0; i < order.size(); ++i) {
            if (order[i] == static_cast<int>(selectedNode_)) {
                startIndex = static_cast<int>(i);
                break;
            }
        }
        if (startIndex < 0) {
            startIndex = 0;
        }
        const int n = static_cast<int>(order.size());
        for (int step = 1; step <= n; ++step) {
            int candidate = startIndex + (direction > 0 ? step : -step);
            candidate %= n;
            if (candidate < 0) {
                candidate += n;
            }
            const int handle = order[static_cast<std::size_t>(candidate)];
            if (!IsProtectedEditorNode(handle)) {
                selectedNode_ = static_cast<std::size_t>(handle);
                const EditorLayout layout = ComputeLayout();
                EnsureHierarchySelectionVisible(layout.hierarchyInner);
                lastIoStatus_ = "Selection: " + starterScene_.scene.GetNode(handle).name;
                return;
            }
        }
        lastIoStatus_ = "No authored node available in hierarchy.";
    }

    void TrySaveTimestampedSceneSnapshot() {
        const fs::path basePath = ResolveSceneStatePath();
        std::error_code ec{};
        fs::create_directories(basePath.parent_path(), ec);
        if (ec) {
            lastIoStatus_ = "Snapshot failed: could not create save folder.";
            return;
        }

        const auto now = std::chrono::system_clock::now();
        const std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm localTm{};
#if defined(_WIN32)
        localtime_s(&localTm, &tt);
#else
        localtime_r(&tt, &localTm);
#endif
        std::ostringstream stamp{};
        stamp << std::put_time(&localTm, "%Y%m%d_%H%M%S");
        const std::string token = stamp.str();

        const fs::path snapshotPath =
            basePath.parent_path() / ("scene_state_snapshot_" + token + ".ri_state");
        const fs::path orbitSnapshotPath =
            basePath.parent_path() / ("editor_orbit_snapshot_" + token + ".ri_cam");
        const bool sceneSaved = ri::scene::SaveSceneNodeTransforms(starterScene_.scene, snapshotPath);
        const bool orbitSaved = SaveEditorOrbitStateToPath(orbitSnapshotPath, editorOrbitState_);
        if (sceneSaved && orbitSaved) {
            lastIoStatus_ = "Snapshot saved: " + snapshotPath.filename().string();
        } else if (sceneSaved) {
            lastIoStatus_ = "Snapshot saved, but orbit snapshot failed.";
        } else {
            lastIoStatus_ = "Snapshot failed while writing scene transforms.";
        }
    }

    [[nodiscard]] fs::path ResolveAutosaveScenePath() const {
        return ResolveSceneStatePath().parent_path() / "autosave_scene_state.ri_state";
    }

    [[nodiscard]] fs::path ResolveAutosaveOrbitPath() const {
        return ResolveSceneStatePath().parent_path() / "autosave_editor_orbit.ri_cam";
    }

    void TryLoadAutosaveState() {
        const bool sceneLoaded =
            ri::scene::LoadSceneNodeTransforms(starterScene_.scene, ResolveAutosaveScenePath());
        const bool orbitLoaded = TryLoadEditorOrbitStateFromPath(ResolveAutosaveOrbitPath(), editorOrbitState_);
        if (orbitLoaded) {
            ApplyEditorOrbitToScene();
        }
        if (sceneLoaded && orbitLoaded) {
            lastIoStatus_ = "Loaded autosave scene + orbit.";
            autosavePending_ = false;
            lastAutosaveSteady_ = std::chrono::steady_clock::now();
        } else if (sceneLoaded) {
            lastIoStatus_ = "Loaded autosave scene (orbit autosave missing).";
            autosavePending_ = false;
            lastAutosaveSteady_ = std::chrono::steady_clock::now();
        } else {
            lastIoStatus_ = "No autosave state available to load.";
        }
    }

    void MaybeAutosaveState() {
        if (!autosavePending_) {
            return;
        }
        const auto now = std::chrono::steady_clock::now();
        if (now - lastAutosaveSteady_ < kAutosaveInterval_) {
            return;
        }
        const bool sceneSaved =
            ri::scene::SaveSceneNodeTransforms(starterScene_.scene, ResolveAutosaveScenePath());
        const bool orbitSaved = SaveEditorOrbitStateToPath(ResolveAutosaveOrbitPath(), editorOrbitState_);
        if (sceneSaved && orbitSaved) {
            autosavePending_ = false;
            lastAutosaveSteady_ = now;
            if (elapsedSeconds_ - lastAutosaveStatusSeconds_ > 6.0) {
                lastIoStatus_ = "Autosaved scene state.";
                lastAutosaveStatusSeconds_ = elapsedSeconds_;
            }
        }
    }

    void PushEditAction(EditorEditAction action) {
        undoStack_.push_back(std::move(action));
        if (undoStack_.size() > kMaxUndoActions) {
            undoStack_.erase(undoStack_.begin());
        }
        redoStack_.clear();
        autosavePending_ = true;
    }

    void RecordSceneGraphEdit(const ri::scene::Scene& beforeScene, const std::size_t beforeSelectedNode) {
        PushEditAction(SceneGraphEditAction{
            .beforeScene = beforeScene,
            .afterScene = starterScene_.scene,
            .beforeSelectedNode = beforeSelectedNode,
            .afterSelectedNode = selectedNode_,
        });
    }

    void RebindEditorTrashFolderAfterSceneReplace() {
        editorTrashFolderHandle_ = ri::scene::kInvalidHandle;
        const std::size_t nodeCount = starterScene_.scene.NodeCount();
        for (std::size_t index = 0; index < nodeCount; ++index) {
            const ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(index));
            if (node.name == "EditorTrash") {
                editorTrashFolderHandle_ = static_cast<int>(index);
                break;
            }
        }
        if (editorTrashFolderHandle_ == ri::scene::kInvalidHandle) {
            EnsureEditorTrashFolder();
        }
        if (starterScene_.scene.NodeCount() == 0) {
            selectedNode_ = 0;
        } else if (selectedNode_ >= starterScene_.scene.NodeCount()) {
            selectedNode_ = starterScene_.scene.NodeCount() - 1;
        }
    }

    void TryCreateGroupNode() {
        if (!SetInspectorPanel(InspectorPanel::Node)) {
            return;
        }
        const ri::scene::Scene beforeScene = starterScene_.scene;
        const std::size_t beforeSelectedNode = selectedNode_;
        const int selected = static_cast<int>(selectedNode_);
        int parent = starterScene_.handles.root;
        if (selected >= 0 && static_cast<std::size_t>(selected) < starterScene_.scene.NodeCount()) {
            parent = selected;
        }
        const std::string name = MakeUniqueNodeName("Group");
        const int created = starterScene_.scene.CreateNode(name, parent);
        selectedNode_ = static_cast<std::size_t>(created);
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        lastIoStatus_ = "Created group node '" + name + "'.";
        RecordSceneGraphEdit(beforeScene, beforeSelectedNode);
    }

    void TryGroupSelectedNode() {
        const ri::scene::Scene beforeScene = starterScene_.scene;
        const std::size_t beforeSelectedNode = selectedNode_;
        const int selected = static_cast<int>(selectedNode_);
        if (selected < 0 || static_cast<std::size_t>(selected) >= starterScene_.scene.NodeCount() ||
            IsProtectedEditorNode(selected)) {
            lastIoStatus_ = "Group: select an authored node (not editor rigs/helpers).";
            return;
        }

        const ri::scene::Node nodeSnapshot = starterScene_.scene.GetNode(selected);
        if (nodeSnapshot.parent == ri::scene::kInvalidHandle) {
            lastIoStatus_ = "Group: selected node has no parent.";
            return;
        }
        if (!SetInspectorPanel(InspectorPanel::Node)) {
            return;
        }

        const std::string groupName = MakeUniqueNodeName(nodeSnapshot.name + "_group");
        const int group = starterScene_.scene.CreateNode(groupName, nodeSnapshot.parent);
        ri::scene::Node& groupNode = starterScene_.scene.GetNode(group);
        groupNode.localTransform = nodeSnapshot.localTransform;
        if (!starterScene_.scene.SetParent(selected, group)) {
            lastIoStatus_ = "Group failed: could not re-parent selected node.";
            return;
        }

        starterScene_.scene.GetNode(selected).localTransform = ri::scene::Transform{};
        selectedNode_ = static_cast<std::size_t>(group);
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        lastIoStatus_ = "Grouped node under '" + groupName + "' (pivot group).";
        RecordSceneGraphEdit(beforeScene, beforeSelectedNode);
    }

    void TryUngroupSelectedNode() {
        const ri::scene::Scene beforeScene = starterScene_.scene;
        const std::size_t beforeSelectedNode = selectedNode_;
        EnsureEditorTrashFolder();
        const int selected = static_cast<int>(selectedNode_);
        if (selected < 0 || static_cast<std::size_t>(selected) >= starterScene_.scene.NodeCount() ||
            IsProtectedEditorNode(selected)) {
            lastIoStatus_ = "Ungroup: select a regular authored group node.";
            return;
        }

        ri::scene::Node& group = starterScene_.scene.GetNode(selected);
        if (group.parent == ri::scene::kInvalidHandle) {
            lastIoStatus_ = "Ungroup failed: group has no parent.";
            return;
        }
        if (group.mesh != ri::scene::kInvalidHandle || group.camera != ri::scene::kInvalidHandle ||
            group.light != ri::scene::kInvalidHandle) {
            lastIoStatus_ = "Ungroup only supports transform-only group nodes.";
            return;
        }
        if (group.children.empty()) {
            lastIoStatus_ = "Ungroup: selected group has no children.";
            return;
        }

        const int parent = group.parent;
        const std::vector<int> children = group.children;
        for (const int child : children) {
            const ri::math::Mat4 childWorld = starterScene_.scene.ComputeWorldMatrix(child);
            if (!starterScene_.scene.SetParent(child, parent)) {
                lastIoStatus_ = "Ungroup failed: could not move all children.";
                return;
            }
            if (!TryAssignLocalTransformFromWorld(child, childWorld)) {
                lastIoStatus_ = "Ungroup failed: could not preserve child transform.";
                return;
            }
        }

        if (!starterScene_.scene.SetParent(selected, editorTrashFolderHandle_)) {
            lastIoStatus_ = "Ungroup warning: children moved, but could not hide old group node.";
            return;
        }

        selectedNode_ = static_cast<std::size_t>(children.front());
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        lastIoStatus_ = "Ungrouped children and moved empty group to EditorTrash.";
        RecordSceneGraphEdit(beforeScene, beforeSelectedNode);
    }

    void TryDeleteSelectedNode() {
        const ri::scene::Scene beforeScene = starterScene_.scene;
        const std::size_t beforeSelectedNode = selectedNode_;
        EnsureEditorTrashFolder();
        const int sel = static_cast<int>(selectedNode_);
        if (IsProtectedEditorNode(sel)) {
            lastIoStatus_ = "Cannot delete World, rigs, orbit camera, helpers, or trash.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        DetachMeshesInSubtree(starterScene_.scene, sel);
        if (!starterScene_.scene.SetParent(sel, editorTrashFolderHandle_)) {
            lastIoStatus_ = "Delete failed — could not re-parent node.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        selectedNode_ = static_cast<std::size_t>(starterScene_.handles.root);
        lastIoStatus_ = "Removed geometry from the working scene (hidden EditorTrash folder).";
        RecordSceneGraphEdit(beforeScene, beforeSelectedNode);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void TryDuplicateSelectedNode() {
        const ri::scene::Scene beforeScene = starterScene_.scene;
        const std::size_t beforeSelectedNode = selectedNode_;
        const int sel = static_cast<int>(selectedNode_);
        if (sel < 0 || IsProtectedEditorNode(sel)) {
            lastIoStatus_ = "Duplicate: pick an authored mesh node (not rigs or helpers).";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        const ri::scene::Node src = starterScene_.scene.GetNode(sel);
        if (src.mesh == ri::scene::kInvalidHandle) {
            lastIoStatus_ = "Duplicate requires a mesh on the selected node.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        const std::string dupName = src.name + "_copy";
        const int dup = starterScene_.scene.CreateNode(dupName, src.parent);
        starterScene_.scene.GetNode(dup).localTransform = src.localTransform;
        starterScene_.scene.GetNode(dup).localTransform.position.x += 1.0f;
        starterScene_.scene.AttachMesh(dup, src.mesh, src.material);
        selectedNode_ = static_cast<std::size_t>(dup);
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        lastIoStatus_ = "Duplicated mesh node as " + dupName + " (offset +1.0 X, shared mesh/material handles).";
        RecordSceneGraphEdit(beforeScene, beforeSelectedNode);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void ApplySelectedNodeEdit(float delta) {
        if (selectedNode_ >= starterScene_.scene.NodeCount()) {
            return;
        }

        ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(selectedNode_));
        const ri::scene::Transform before = node.localTransform;
        if (editMode_ == EditMode::Translate) {
            if (activeAxis_ == 0) {
                node.localTransform.position.x += delta;
            } else if (activeAxis_ == 1) {
                node.localTransform.position.y += delta;
            } else {
                node.localTransform.position.z += delta;
            }
        } else if (editMode_ == EditMode::Rotate) {
            if (activeAxis_ == 0) {
                node.localTransform.rotationDegrees.x += delta;
            } else if (activeAxis_ == 1) {
                node.localTransform.rotationDegrees.y += delta;
            } else {
                node.localTransform.rotationDegrees.z += delta;
            }
        } else {
            auto clampScale = [](float value) {
                return std::max(0.01f, value);
            };
            if (activeAxis_ == 0) {
                node.localTransform.scale.x = clampScale(node.localTransform.scale.x + delta);
            } else if (activeAxis_ == 1) {
                node.localTransform.scale.y = clampScale(node.localTransform.scale.y + delta);
            } else {
                node.localTransform.scale.z = clampScale(node.localTransform.scale.z + delta);
            }
        }

        const ri::scene::Transform after = node.localTransform;
        if (before.position.x == after.position.x &&
            before.position.y == after.position.y &&
            before.position.z == after.position.z &&
            before.rotationDegrees.x == after.rotationDegrees.x &&
            before.rotationDegrees.y == after.rotationDegrees.y &&
            before.rotationDegrees.z == after.rotationDegrees.z &&
            before.scale.x == after.scale.x &&
            before.scale.y == after.scale.y &&
            before.scale.z == after.scale.z) {
            return;
        }

        PushEditAction(TransformEditAction{selectedNode_, before, after});
    }

    void ApplySelectedNodeTranslationDelta(const ri::math::Vec3& delta) {
        if (selectedNode_ >= starterScene_.scene.NodeCount()) {
            return;
        }
        if (delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f) {
            return;
        }

        ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(selectedNode_));
        const ri::scene::Transform before = node.localTransform;
        node.localTransform.position.x += delta.x;
        node.localTransform.position.y += delta.y;
        node.localTransform.position.z += delta.z;
        const ri::scene::Transform after = node.localTransform;

        PushEditAction(TransformEditAction{selectedNode_, before, after});
    }

    void ApplySelectedNodeRotationDelta(const ri::math::Vec3& deltaDegrees) {
        if (selectedNode_ >= starterScene_.scene.NodeCount()) {
            return;
        }
        if (deltaDegrees.x == 0.0f && deltaDegrees.y == 0.0f && deltaDegrees.z == 0.0f) {
            return;
        }

        ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(selectedNode_));
        const ri::scene::Transform before = node.localTransform;
        node.localTransform.rotationDegrees.x += deltaDegrees.x;
        node.localTransform.rotationDegrees.y += deltaDegrees.y;
        node.localTransform.rotationDegrees.z += deltaDegrees.z;
        const ri::scene::Transform after = node.localTransform;

        PushEditAction(TransformEditAction{selectedNode_, before, after});
    }

    [[nodiscard]] bool UndoLastEdit() {
        if (undoStack_.empty()) {
            return false;
        }

        const EditorEditAction action = undoStack_.back();
        undoStack_.pop_back();
        if (std::holds_alternative<TransformEditAction>(action)) {
            const TransformEditAction& transformAction = std::get<TransformEditAction>(action);
            if (transformAction.nodeIndex >= starterScene_.scene.NodeCount()) {
                return false;
            }
            ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(transformAction.nodeIndex));
            node.localTransform = transformAction.before;
            selectedNode_ = transformAction.nodeIndex;
            redoStack_.push_back(action);
            return true;
        }

        const SceneGraphEditAction& sceneAction = std::get<SceneGraphEditAction>(action);
        starterScene_.scene = sceneAction.beforeScene;
        selectedNode_ = sceneAction.beforeSelectedNode;
        RebindEditorTrashFolderAfterSceneReplace();
        redoStack_.push_back(action);
        return true;
    }

    [[nodiscard]] bool RedoLastEdit() {
        if (redoStack_.empty()) {
            return false;
        }

        const EditorEditAction action = redoStack_.back();
        redoStack_.pop_back();
        if (std::holds_alternative<TransformEditAction>(action)) {
            const TransformEditAction& transformAction = std::get<TransformEditAction>(action);
            if (transformAction.nodeIndex >= starterScene_.scene.NodeCount()) {
                return false;
            }
            ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(transformAction.nodeIndex));
            node.localTransform = transformAction.after;
            selectedNode_ = transformAction.nodeIndex;
            undoStack_.push_back(action);
            return true;
        }

        const SceneGraphEditAction& sceneAction = std::get<SceneGraphEditAction>(action);
        starterScene_.scene = sceneAction.afterScene;
        selectedNode_ = sceneAction.afterSelectedNode;
        RebindEditorTrashFolderAfterSceneReplace();
        undoStack_.push_back(action);
        return true;
    }

    [[nodiscard]] std::string EditModeLabel() const {
        switch (editMode_) {
            case EditMode::Translate:
                return "Translate";
            case EditMode::Rotate:
                return "Rotate";
            case EditMode::Scale:
                return "Scale";
        }
        return "Translate";
    }

    [[nodiscard]] std::string AxisLabel() const {
        if (activeAxis_ == 0) {
            return "X";
        }
        if (activeAxis_ == 1) {
            return "Y";
        }
        return "Z";
    }

    [[nodiscard]] std::string InspectorPanelLabel() const {
        switch (inspectorPanel_) {
            case InspectorPanel::Node: return "Node";
            case InspectorPanel::Brush: return "Brush";
            case InspectorPanel::Gameplay: return "Gameplay";
            case InspectorPanel::Files: return "Files";
        }
        return "Node";
    }

    [[nodiscard]] std::string EditStepLabel() const {
        if (editMode_ == EditMode::Translate) {
            return "0.10u";
        }
        if (editMode_ == EditMode::Rotate) {
            return "2.50deg";
        }
        return "0.08s";
    }

    [[nodiscard]] std::string InventoryPresentationLabel() const {
        switch (creatorInventoryPolicy_.presentation) {
            case ri::world::InventoryPresentationMode::Disabled: return "disabled";
            case ri::world::InventoryPresentationMode::HiddenDataOnly: return "hidden_data_only";
            case ri::world::InventoryPresentationMode::Visible: return "visible";
        }
        return "visible";
    }

    void CycleInventoryPresentation() {
        switch (creatorInventoryPolicy_.presentation) {
            case ri::world::InventoryPresentationMode::Visible:
                creatorInventoryPolicy_.presentation = ri::world::InventoryPresentationMode::HiddenDataOnly;
                lastIoStatus_ = "Gameplay policy: inventory is now hidden_data_only.";
                break;
            case ri::world::InventoryPresentationMode::HiddenDataOnly:
                creatorInventoryPolicy_.presentation = ri::world::InventoryPresentationMode::Disabled;
                lastIoStatus_ = "Gameplay policy: inventory is now disabled.";
                break;
            case ri::world::InventoryPresentationMode::Disabled:
                creatorInventoryPolicy_.presentation = ri::world::InventoryPresentationMode::Visible;
                lastIoStatus_ = "Gameplay policy: inventory is now visible.";
                break;
        }
    }

    [[nodiscard]] std::string NodeKindLabel(const ri::scene::Node& node) const {
        if (node.camera != ri::scene::kInvalidHandle) {
            return "Camera";
        }
        if (node.light != ri::scene::kInvalidHandle) {
            return "Light";
        }
        if (node.mesh != ri::scene::kInvalidHandle) {
            return "Mesh";
        }
        return "Transform";
    }

    [[nodiscard]] EditorLayout ComputeLayout() const {
        RECT client{};
        GetClientRect(hwnd_, &client);
        EditorLayout layout{};
        layout.toolStrip = RECT{10, 66, client.right - 10, 106};
        layout.hierarchy = RECT{10, 116, 332, client.bottom - 92};
        layout.viewport = RECT{342, 116, client.right - 378, client.bottom - 92};
        const RECT inspector{client.right - 368, 116, client.right - 10, client.bottom - 92};
        layout.hierarchyInner = RECT{layout.hierarchy.left + 8, layout.hierarchy.top + 36, layout.hierarchy.right - 8, layout.hierarchy.bottom - 8};
        layout.viewportInner = RECT{layout.viewport.left + 8, layout.viewport.top + 36, layout.viewport.right - 8, layout.viewport.bottom - 8};
        layout.inspectorInner = RECT{inspector.left + 8, inspector.top + 36, inspector.right - 8, inspector.bottom - 8};
        return layout;
    }

    void DrawToolbarButton(HDC dc, const RECT& rect, const std::string& label, bool active) {
        DrawPanelFrame(
            dc,
            rect,
            active ? RGB(96, 104, 120) : RGB(74, 80, 90),
            active ? RGB(228, 234, 245) : RGB(182, 188, 198),
            active ? RGB(32, 36, 44) : RGB(40, 44, 52));
        DrawTextLine(dc, rect, label, active ? RGB(255, 244, 195) : RGB(232, 236, 242), smallFont_,
                     DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }

    void DrawPanelHeader(HDC dc, const RECT& panelRect, const std::string& title, const std::string& meta = {}) {
        RECT header{panelRect.left + 2, panelRect.top + 2, panelRect.right - 2, panelRect.top + 30};
        FillRectColor(dc, header, RGB(86, 92, 104));
        DrawTextLine(dc, RECT{header.left + 10, header.top + 4, header.right - 120, header.bottom - 4},
                     title, RGB(244, 244, 242), headerFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        if (!meta.empty()) {
            DrawTextLine(dc, RECT{header.left + 120, header.top + 4, header.right - 10, header.bottom - 4},
                         meta, RGB(208, 214, 224), smallFont_, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        }
    }

    void DrawViewportPreview(HDC dc, const RECT& targetRect) {
        const int width = std::max(1L, targetRect.right - targetRect.left);
        const int height = std::max(1L, targetRect.bottom - targetRect.top);
        if (starterScene_.handles.orbitCamera.cameraNode == ri::scene::kInvalidHandle) {
            FillRectColor(dc, targetRect, RGB(32, 36, 42));
            return;
        }

        ri::render::software::ScenePreviewOptions options{};
        options.width = width;
        options.height = height;

        std::filesystem::path editorExe{};
        wchar_t moduleWide[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, moduleWide, MAX_PATH) > 0) {
            editorExe = std::filesystem::path(std::wstring(moduleWide));
        }
        const std::filesystem::path textureDir =
            ri::content::PickEngineTexturesDirectory(sceneConfig_.workspaceRoot, editorExe);
        if (!textureDir.empty()) {
            options.textureRoot = textureDir;
        }
        options.hiddenNodeHandles = {
            starterScene_.handles.grid,
            starterScene_.handles.axes.root,
            starterScene_.handles.axes.xAxis,
            starterScene_.handles.axes.yAxis,
            starterScene_.handles.axes.zAxis,
        };

        ri::editor::ConfigureEditorViewportForPreview(sceneConfig_.editorPreviewScene, options);

        const ri::render::software::SoftwareImage image =
            ri::render::software::RenderScenePreview(starterScene_.scene, starterScene_.handles.orbitCamera.cameraNode, options);
        if (image.pixels.empty()) {
            FillRectColor(dc, targetRect, RGB(32, 36, 42));
            return;
        }

        BITMAPINFO bitmapInfo{};
        bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth = image.width;
        bitmapInfo.bmiHeader.biHeight = -image.height;
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 24;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;

        StretchDIBits(dc,
                      targetRect.left,
                      targetRect.top,
                      width,
                      height,
                      0,
                      0,
                      image.width,
                      image.height,
                      image.pixels.data(),
                      &bitmapInfo,
                      DIB_RGB_COLORS,
                      SRCCOPY);
    }

    [[nodiscard]] ri::world::RuntimeStatsOverlaySnapshot BuildRuntimeStatsOverlaySnapshot() const {
        ri::world::RuntimeStatsOverlaySnapshot snapshot{};
        snapshot.metrics = statsOverlayState_.GetMetrics();
        snapshot.sceneNodes = starterScene_.scene.NodeCount();
        snapshot.rootNodes = ri::scene::CollectRootNodes(starterScene_.scene).size();
        snapshot.renderables = ri::scene::CollectRenderableNodes(starterScene_.scene).size();
        for (const ri::scene::Node& node : starterScene_.scene.Nodes()) {
            if (node.light != ri::scene::kInvalidHandle) {
                snapshot.lights += 1;
            }
            if (node.camera != ri::scene::kInvalidHandle) {
                snapshot.cameras += 1;
            }
        }
        snapshot.selectedNode = selectedNode_;
        snapshot.modeLabel = sceneConfig_.gameManifest.has_value() ? "project" : "starter";
        snapshot.sceneLabel = sceneConfig_.gameManifest.has_value()
            ? sceneConfig_.gameManifest->id
            : sceneConfig_.sceneName;
        return snapshot;
    }

    void DrawRuntimeStatsOverlay(HDC dc, const RECT& viewportRect) {
        if (!statsOverlayVisible_) {
            return;
        }

        const std::vector<std::string> lines =
            ri::world::FormatRuntimeStatsOverlayLines(BuildRuntimeStatsOverlaySnapshot(), 6);
        if (lines.empty()) {
            return;
        }

        const int lineHeight = 18;
        const int panelWidth = 296;
        const int panelHeight = 12 + static_cast<int>(lines.size()) * lineHeight;
        RECT panel{
            viewportRect.right - panelWidth - 12,
            viewportRect.top + 12,
            viewportRect.right - 12,
            viewportRect.top + 12 + panelHeight
        };
        DrawPanelFrame(dc, panel, RGB(58, 64, 74), RGB(214, 220, 228), RGB(20, 24, 28));

        int top = panel.top + 6;
        for (std::size_t index = 0; index < lines.size(); ++index) {
            DrawTextLine(dc,
                         RECT{panel.left + 10, top, panel.right - 10, top + lineHeight},
                         lines[index],
                         index == 0U ? RGB(255, 244, 195) : RGB(214, 222, 230),
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            top += lineHeight;
        }
    }

    void DrawRawIronQuadViewportBlock(HDC dc, const RECT& viewportInner) {
        const ri::math::Vec3 orbitFocus = starterScene_.handles.orbitCamera.orbit.target;

        constexpr int kBannerHeight = 24;
        RECT menuBanner{viewportInner.left + 4,
                         viewportInner.top + 6,
                         viewportInner.right - 4,
                         viewportInner.top + 6 + kBannerHeight};
        FillRectColor(dc, menuBanner, RGB(192, 192, 192));
        DrawInsetFrame(dc, menuBanner, RGB(210, 210, 210), RGB(252, 252, 252), RGB(96, 96, 96));
        DrawTextLine(dc,
                     RECT{menuBanner.left + 8, menuBanner.top + 3, menuBanner.right - 8, menuBanner.bottom - 3},
                     "File      Map      View      Tools      Go      Window      Help",
                     RGB(16, 16, 16),
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        constexpr int kMetaStrip = 26;
        RECT quadArea{viewportInner.left + 4,
                      menuBanner.bottom + 4,
                      viewportInner.right - 4,
                      viewportInner.bottom - 4 - kMetaStrip};

        const auto drawCameraAndMeta = [&]() {
            if (cameraPlotRect_.right > cameraPlotRect_.left + 8 && cameraPlotRect_.bottom > cameraPlotRect_.top + 8) {
                DrawInsetFrame(dc, cameraPlotRect_, RGB(24, 26, 30), RGB(112, 118, 128), RGB(18, 20, 24));
                DrawViewportPreview(dc, cameraPlotRect_);
                DrawRuntimeStatsOverlay(dc, cameraPlotRect_);
            }
            RECT viewMeta{quadArea.left + 6, quadArea.bottom + 4, quadArea.right - 6, viewportInner.bottom - 4};
            const std::string mode =
                autoOrbitPreview_ ? "auto-orbit demo" : "drag orbit / wheel zoom / Tab layout";
            DrawTextLine(dc,
                         viewMeta,
                         "Camera " +
                             ri::math::ToString(starterScene_.scene.ComputeWorldPosition(
                                 starterScene_.handles.orbitCamera.cameraNode)) +
                             "  |  " + EditModeLabel() + "  |  Axis " + AxisLabel() + "  |  " + mode,
                         RGB(32, 32, 32),
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        };

        if (quadArea.right <= quadArea.left + 32 || quadArea.bottom <= quadArea.top + 32) {
            DrawInsetFrame(dc, quadArea, RGB(32, 32, 32), RGB(120, 120, 120), RGB(16, 16, 16));
            DrawViewportPreview(dc, quadArea);
            return;
        }

        if (full3DViewport_) {
            DrawInsetFrame(dc, quadArea, RGB(36, 36, 36), RGB(210, 200, 120), RGB(18, 18, 18));
            DrawTextLine(dc,
                         RECT{quadArea.left + 8, quadArea.top + 6, quadArea.right - 8, quadArea.top + 24},
                         "CAMERA (full 3D)   Tab = quad views",
                         RGB(255, 255, 210),
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            drawCameraAndMeta();
            return;
        }

        const int midX = (quadArea.left + quadArea.right) / 2;
        const int midY = (quadArea.top + quadArea.bottom) / 2;
        RECT cellTop{quadArea.left, quadArea.top, midX - 1, midY - 1};
        RECT cellSide{midX + 1, quadArea.top, quadArea.right, midY - 1};
        RECT cellFront{quadArea.left, midY + 1, midX - 1, quadArea.bottom};
        RECT cellCamera{midX + 1, midY + 1, quadArea.right, quadArea.bottom};

        DcStrokeLine(dc, midX, quadArea.top, midX, quadArea.bottom, RGB(24, 24, 24), 2);
        DcStrokeLine(dc, quadArea.left, midY, quadArea.right, midY, RGB(24, 24, 24), 2);

        DrawRawIronFlatSceneView(dc,
                               cellTop,
                               starterScene_.scene,
                               selectedNode_,
                               orbitFocus,
                               RawIronFlatProjection::TopXz,
                               "TOP (X / Z)",
                               smallFont_);
        DrawRawIronFlatSceneView(dc,
                               cellSide,
                               starterScene_.scene,
                               selectedNode_,
                               orbitFocus,
                               RawIronFlatProjection::SideZy,
                               "SIDE (Z / Y)",
                               smallFont_);
        DrawRawIronFlatSceneView(dc,
                               cellFront,
                               starterScene_.scene,
                               selectedNode_,
                               orbitFocus,
                               RawIronFlatProjection::FrontXy,
                               "FRONT (X / Y)",
                               smallFont_);

        DrawInsetFrame(dc, cellCamera, RGB(28, 28, 28), RGB(210, 210, 140), RGB(12, 12, 12));
        RECT cameraInner{cellCamera.left + 2, cellCamera.top + 2, cellCamera.right - 2, cellCamera.bottom - 2};
        DrawTextLine(dc,
                     RECT{cameraInner.left + 6, cameraInner.top + 4, cameraInner.right - 6, cameraInner.top + 22},
                     "CAMERA (3D)",
                     RGB(255, 255, 210),
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        if (cameraPlotRect_.right > cameraPlotRect_.left + 4 && cameraPlotRect_.bottom > cameraPlotRect_.top + 4) {
            DrawInsetFrame(dc, cameraPlotRect_, RGB(24, 26, 30), RGB(112, 118, 128), RGB(18, 20, 24));
            DrawViewportPreview(dc, cameraPlotRect_);
            DrawRuntimeStatsOverlay(dc, cameraPlotRect_);
        }

        RECT viewMeta{quadArea.left + 6, quadArea.bottom + 4, quadArea.right - 6, viewportInner.bottom - 4};
        const std::string mode =
            autoOrbitPreview_ ? "auto-orbit demo" : "drag in CAMERA / wheel / Tab full 3D";
        DrawTextLine(dc,
                     viewMeta,
                     "Camera " +
                         ri::math::ToString(
                             starterScene_.scene.ComputeWorldPosition(starterScene_.handles.orbitCamera.cameraNode)) +
                         "  |  " + EditModeLabel() + "  |  Axis " + AxisLabel() + "  |  " + mode,
                     RGB(32, 32, 32),
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }

    void Paint() {
        PAINTSTRUCT paint{};
        HDC windowDc = BeginPaint(hwnd_, &paint);
        RECT client{};
        GetClientRect(hwnd_, &client);
        const int width = std::max(1L, client.right - client.left);
        const int height = std::max(1L, client.bottom - client.top);

        HDC dc = CreateCompatibleDC(windowDc);
        HBITMAP backBuffer = CreateCompatibleBitmap(windowDc, width, height);
        HGDIOBJ oldBitmap = SelectObject(dc, backBuffer);

        const COLORREF kWindowBg = RGB(120, 122, 126);
        const COLORREF kPanelFill = RGB(176, 178, 182);
        const COLORREF kPanelDark = RGB(72, 74, 78);
        const COLORREF kPanelLight = RGB(236, 238, 242);
        const COLORREF kInsetFill = RGB(96, 98, 102);
        const COLORREF kViewportFill = RGB(88, 90, 94);
        FillRectColor(dc, client, kWindowBg);

        RECT topBar{0, 0, client.right, 56};
        DrawPanelFrame(dc, topBar, RGB(200, 202, 206), RGB(252, 252, 252), RGB(110, 112, 116));
        DrawTextLine(dc, RECT{16, 8, 620, 30}, "RawIron Editor", RGB(24, 24, 24), titleFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        DrawTextLine(dc, RECT{16, 30, client.right - 16, 50},
                     "Author here in the viewport, then export to your game's levels folder (toolbar Export or Ctrl+E).",
                     RGB(28, 28, 28),
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        DrawTextLine(dc, RECT{260, 8, 760, 30}, sceneConfig_.workspaceLabel,
                     RGB(40, 40, 120), bodyFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        DrawTextLine(dc, RECT{440, 8, client.right - 16, 30},
                     "RawIron quad views  |  Project  |  Session",
                     RGB(60, 60, 60), smallFont_, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

        RECT toolStrip{10, 66, client.right - 10, 106};
        DrawPanelFrame(dc, toolStrip, kPanelFill, kPanelLight, kPanelDark);
        DrawTextLine(dc, RECT{toolStrip.left + 12, toolStrip.top + 8, toolStrip.left + 72, toolStrip.bottom - 8},
                     "Tool", RGB(32, 32, 32), headerFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        DrawToolbarButton(dc, RECT{toolStrip.left + 78, toolStrip.top + 8, toolStrip.left + 130, toolStrip.bottom - 8},
                          "Select", true);
        DrawToolbarButton(dc, RECT{toolStrip.left + 134, toolStrip.top + 8, toolStrip.left + 188, toolStrip.bottom - 8},
                          "Camera", false);
        DrawToolbarButton(dc, RECT{toolStrip.left + 194, toolStrip.top + 8, toolStrip.left + 268, toolStrip.bottom - 8},
                          "Move", editMode_ == EditMode::Translate);
        DrawToolbarButton(dc, RECT{toolStrip.left + 274, toolStrip.top + 8, toolStrip.left + 342, toolStrip.bottom - 8},
                          "Rotate", editMode_ == EditMode::Rotate);
        DrawToolbarButton(dc, RECT{toolStrip.left + 348, toolStrip.top + 8, toolStrip.left + 416, toolStrip.bottom - 8},
                          "Scale", editMode_ == EditMode::Scale);
        DrawToolbarButton(dc, RECT{toolStrip.left + 422, toolStrip.top + 8, toolStrip.left + 468, toolStrip.bottom - 8},
                          "X", activeAxis_ == 0);
        DrawToolbarButton(dc, RECT{toolStrip.left + 474, toolStrip.top + 8, toolStrip.left + 520, toolStrip.bottom - 8},
                          "Y", activeAxis_ == 1);
        DrawToolbarButton(dc, RECT{toolStrip.left + 526, toolStrip.top + 8, toolStrip.left + 572, toolStrip.bottom - 8},
                          "Z", activeAxis_ == 2);
        const AuthoringToolbarRects authoringPaint = ComputeAuthoringToolbarRects(toolStrip);
        DrawToolbarButton(dc, authoringPaint.addCube, "+ Cube", false);
        DrawToolbarButton(dc, authoringPaint.addPlane, "+ Plane", false);
        DrawToolbarButton(dc, authoringPaint.exportCsv, "Export", false);
        DrawToolbarButton(dc, authoringPaint.play, "Play", false);
        DrawTextLine(dc, RECT{toolStrip.left + 954, toolStrip.top + 8, toolStrip.right - 12, toolStrip.bottom - 8},
                     "Step " + EditStepLabel() + "  |  Undo " + std::to_string(undoStack_.size()) +
                         "  |  Redo " + std::to_string(redoStack_.size()),
                     RGB(224, 230, 238), smallFont_, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

        RECT hierarchy{10, 116, 332, client.bottom - 92};
        RECT viewport{342, 116, client.right - 378, client.bottom - 92};
        RECT inspector{client.right - 368, 116, client.right - 10, client.bottom - 92};
        RECT statusBar{10, client.bottom - 82, client.right - 10, client.bottom - 10};
        DrawPanelFrame(dc, hierarchy, kPanelFill, kPanelLight, kPanelDark);
        DrawPanelFrame(dc, viewport, kPanelFill, kPanelLight, kPanelDark);
        DrawPanelFrame(dc, inspector, kPanelFill, kPanelLight, kPanelDark);
        DrawPanelFrame(dc, statusBar, RGB(168, 170, 174), RGB(252, 252, 252), RGB(96, 98, 102));

        const std::string leftPanelMeta =
            leftPanelMode_ == LeftPanelMode::Scene
                ? (std::to_string(starterScene_.scene.NodeCount()) + " nodes")
                : (std::to_string(workspaceGames_.size()) + " games · " +
                   std::to_string(resourceCatalogEntries_.size()) + " files");
        DrawPanelHeader(dc,
                        hierarchy,
                        leftPanelMode_ == LeftPanelMode::Scene ? "Scene Graph" : "Game Archive",
                        leftPanelMeta);
        DrawPanelHeader(dc,
                        viewport,
                        "Authoring views",
                        sceneConfig_.gameManifest.has_value() ? sceneConfig_.gameManifest->id : "starter");
        DrawPanelHeader(dc, inspector, "Inspector", InspectorPanelLabel());

        RECT hierarchyInner{hierarchy.left + 8, hierarchy.top + 36, hierarchy.right - 8, hierarchy.bottom - 8};
        RECT viewportInner{viewport.left + 8, viewport.top + 36, viewport.right - 8, viewport.bottom - 8};
        RECT inspectorInner{inspector.left + 8, inspector.top + 36, inspector.right - 8, inspector.bottom - 8};
        DrawInsetFrame(dc, hierarchyInner, kInsetFill, RGB(154, 160, 170), RGB(26, 29, 35));
        DrawInsetFrame(dc, viewportInner, kViewportFill, RGB(154, 160, 170), RGB(24, 26, 30));
        DrawInsetFrame(dc, inspectorInner, kInsetFill, RGB(154, 160, 170), RGB(26, 29, 35));

        const auto& nodes = starterScene_.scene.Nodes();
        const std::vector<int> hierarchyOrder = HierarchyDrawOrder();

        DrawToolbarButton(dc,
                         RECT{hierarchyInner.left + 6,
                              hierarchyInner.top + 4,
                              hierarchyInner.left + 78,
                              hierarchyInner.top + 4 + kLeftPanelTabHeight_},
                         "Scene",
                         leftPanelMode_ == LeftPanelMode::Scene);
        DrawToolbarButton(dc,
                         RECT{hierarchyInner.left + 82,
                              hierarchyInner.top + 4,
                              hierarchyInner.left + 190,
                              hierarchyInner.top + 4 + kLeftPanelTabHeight_},
                         "Resources",
                         leftPanelMode_ == LeftPanelMode::Resources);

        const int tabBottom = hierarchyInner.top + 4 + kLeftPanelTabHeight_;
        if (leftPanelMode_ == LeftPanelMode::Resources && !workspaceGames_.empty()) {
            const WorkspaceGameEntry& focus =
                workspaceGames_[static_cast<std::size_t>(focusedWorkspaceGameIndex_)];
            DrawToolbarButton(dc,
                             RECT{hierarchyInner.left + 6,
                                  tabBottom + 4,
                                  hierarchyInner.left + 34,
                                  tabBottom + 4 + kLeftPanelGameStripHeight_},
                             "<",
                             false);
            DrawToolbarButton(dc,
                             RECT{hierarchyInner.right - 34,
                                  tabBottom + 4,
                                  hierarchyInner.right - 6,
                                  tabBottom + 4 + kLeftPanelGameStripHeight_},
                             ">",
                             false);
            DrawTextLine(dc,
                         RECT{hierarchyInner.left + 40,
                              tabBottom + 6,
                              hierarchyInner.right - 40,
                              tabBottom + kLeftPanelGameStripHeight_},
                         focus.displayName,
                         RGB(28, 28, 120),
                         bodyFont_,
                         DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }

        const int listTop = LeftPanelContentTop(hierarchyInner);
        const int listBottom = LeftPanelSceneListBottom(hierarchyInner);
        const int listPixels = std::max(0, listBottom - listTop - 8);

        if (leftPanelMode_ == LeftPanelMode::Scene) {
            const int visibleHierarchyRows = std::max(0, listPixels / kHierarchyRowHeight_);
            const int maxHierarchyScroll =
                std::max(0, static_cast<int>(hierarchyOrder.size()) - visibleHierarchyRows);
            hierarchyScrollTopRow_ = std::clamp(hierarchyScrollTopRow_, 0, maxHierarchyScroll);

            if (!hierarchyOrder.empty() && visibleHierarchyRows > 0 && maxHierarchyScroll > 0) {
                const std::string scrollHint =
                    "Rows " + std::to_string(hierarchyScrollTopRow_ + 1) + "-" +
                    std::to_string(std::min(static_cast<int>(hierarchyOrder.size()),
                                            hierarchyScrollTopRow_ + visibleHierarchyRows)) +
                    " of " + std::to_string(hierarchyOrder.size()) + "  |  wheel / PgUp PgDn";
                DrawTextLine(dc,
                             RECT{hierarchyInner.left + 6,
                                  hierarchyInner.bottom - 22,
                                  hierarchyInner.right - 6,
                                  hierarchyInner.bottom - 4},
                             scrollHint,
                             RGB(36, 38, 42),
                             smallFont_,
                             DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            }

            int y = listTop;
            for (int row = 0; row < visibleHierarchyRows; ++row) {
                const int orderIndex = hierarchyScrollTopRow_ + row;
                if (orderIndex >= static_cast<int>(hierarchyOrder.size())) {
                    break;
                }
                const int nodeIndex = hierarchyOrder[static_cast<std::size_t>(orderIndex)];
                if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= nodes.size()) {
                    continue;
                }
                const ri::scene::Node& node = nodes[static_cast<std::size_t>(nodeIndex)];
                const int depth = ComputeNodeDepth(starterScene_.scene, nodeIndex);
                const int indent = 8 + depth * 14;
                RECT rowRect{hierarchyInner.left + 6, y, hierarchyInner.right - 6, y + 24};
                if (static_cast<std::size_t>(nodeIndex) == selectedNode_) {
                    FillRectColor(dc, rowRect, RGB(0, 0, 128));
                }
                DrawTextLine(dc,
                             RECT{rowRect.left + indent, rowRect.top, rowRect.right - 90, rowRect.bottom},
                             std::to_string(nodeIndex) + "  " + node.name,
                             static_cast<std::size_t>(nodeIndex) == selectedNode_ ? RGB(255, 255, 160)
                                                                                : RGB(16, 16, 16),
                             bodyFont_,
                             DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                DrawTextLine(dc,
                             RECT{rowRect.right - 84, rowRect.top, rowRect.right - 8, rowRect.bottom},
                             NodeKindLabel(node),
                             static_cast<std::size_t>(nodeIndex) == selectedNode_ ? RGB(255, 255, 200)
                                                                                  : RGB(56, 56, 56),
                             smallFont_,
                             DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
                y += kHierarchyRowHeight_;
            }
        } else {
            const int visibleResourceRows = std::max(0, listPixels / kResourceListRowHeight_);
            const int maxResourceScroll =
                std::max(0, static_cast<int>(resourceCatalogEntries_.size()) - visibleResourceRows);
            resourceCatalogScrollTopRow_ =
                std::clamp(resourceCatalogScrollTopRow_, 0, maxResourceScroll);

            if (!resourceCatalogEntries_.empty() && visibleResourceRows > 0 && maxResourceScroll > 0) {
                const std::string scrollHint =
                    "Rows " + std::to_string(resourceCatalogScrollTopRow_ + 1) + "-" +
                    std::to_string(std::min(static_cast<int>(resourceCatalogEntries_.size()),
                                            resourceCatalogScrollTopRow_ + visibleResourceRows)) +
                    " of " + std::to_string(resourceCatalogEntries_.size()) + "  |  wheel / PgUp PgDn";
                DrawTextLine(dc,
                             RECT{hierarchyInner.left + 6,
                                  hierarchyInner.bottom - 22,
                                  hierarchyInner.right - 6,
                                  hierarchyInner.bottom - 4},
                             scrollHint,
                             RGB(36, 38, 42),
                             smallFont_,
                             DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            }

            if (workspaceGames_.empty()) {
                DrawTextLine(dc,
                             RECT{hierarchyInner.left + 8,
                                  listTop,
                                  hierarchyInner.right - 8,
                                  listTop + 48},
                             "No game folders under workspace Games/. Open a project with --game.",
                             RGB(180, 90, 90),
                             smallFont_,
                             DT_LEFT | DT_WORDBREAK);
            } else {
                int ry = listTop;
                for (int row = 0; row < visibleResourceRows; ++row) {
                    const int idx = resourceCatalogScrollTopRow_ + row;
                    if (idx >= static_cast<int>(resourceCatalogEntries_.size())) {
                        break;
                    }
                    const WorkspaceResourceEntry& entry =
                        resourceCatalogEntries_[static_cast<std::size_t>(idx)];
                    RECT rowRect{hierarchyInner.left + 6,
                                 ry,
                                 hierarchyInner.right - 6,
                                 ry + kResourceListRowHeight_};
                    if (idx == selectedResourceRow_) {
                        FillRectColor(dc, rowRect, RGB(40, 60, 96));
                    }
                    DrawTextLine(dc,
                                 RECT{rowRect.left + 6, rowRect.top, rowRect.right - 110, rowRect.bottom},
                                 entry.relativePathUtf8,
                                 idx == selectedResourceRow_ ? RGB(255, 255, 210) : RGB(16, 16, 16),
                                 smallFont_,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                    DrawTextLine(dc,
                                 RECT{rowRect.right - 102, rowRect.top, rowRect.right - 8, rowRect.bottom},
                                 WorkspaceCategoryLabel(entry.category),
                                 idx == selectedResourceRow_ ? RGB(220, 230, 255) : RGB(56, 56, 56),
                                 smallFont_,
                                 DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                    ry += kResourceListRowHeight_;
                }
            }
        }

        DrawToolbarButton(dc, RECT{inspectorInner.left + 6, inspectorInner.top + 6, inspectorInner.left + 68, inspectorInner.top + 30},
                          "Node", inspectorPanel_ == InspectorPanel::Node);
        DrawToolbarButton(dc, RECT{inspectorInner.left + 72, inspectorInner.top + 6, inspectorInner.left + 138, inspectorInner.top + 30},
                          "Brush", inspectorPanel_ == InspectorPanel::Brush);
        DrawToolbarButton(dc, RECT{inspectorInner.left + 142, inspectorInner.top + 6, inspectorInner.left + 218, inspectorInner.top + 30},
                          "Gameplay", inspectorPanel_ == InspectorPanel::Gameplay);
        DrawToolbarButton(dc, RECT{inspectorInner.left + 222, inspectorInner.top + 6, inspectorInner.left + 286, inspectorInner.top + 30},
                          "Files", inspectorPanel_ == InspectorPanel::Files);

        int infoTop = inspectorInner.top + 42;
        if (inspectorPanel_ == InspectorPanel::Files) {
            DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 22},
                         "Project files (levels, scripts, screens, menus, assets)",
                         RGB(224, 224, 236),
                         headerFont_,
                         DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            infoTop += 26;
            if (selectedResourceRow_ >= 0
                && selectedResourceRow_ < static_cast<int>(resourceCatalogEntries_.size())) {
                const WorkspaceResourceEntry& sel =
                    resourceCatalogEntries_[static_cast<std::size_t>(selectedResourceRow_)];
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 18},
                             sel.relativePathUtf8,
                             RGB(210, 220, 240),
                             smallFont_,
                             DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
                infoTop += 22;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 18},
                             "Category: " + WorkspaceCategoryLabel(sel.category),
                             RGB(200, 200, 200),
                             smallFont_,
                             DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop += 22;
            } else {
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 36},
                             "Pick a file in Resources, or switch tabs for scene nodes.",
                             RGB(180, 180, 190),
                             smallFont_,
                             DT_LEFT | DT_WORDBREAK);
                infoTop += 40;
            }
            if (!resourceEditorAuxMessage_.empty()) {
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 48},
                             resourceEditorAuxMessage_,
                             RGB(220, 160, 120),
                             smallFont_,
                             DT_LEFT | DT_WORDBREAK);
                infoTop += 52;
            }
            DrawToolbarButton(dc,
                             RECT{inspectorInner.left + 10, inspectorInner.top + 66, inspectorInner.left + 108, inspectorInner.top + 92},
                             resourceFileDirty_ ? "Save*" : "Save",
                             false);
            DrawToolbarButton(dc,
                             RECT{inspectorInner.left + 114, inspectorInner.top + 66, inspectorInner.right - 10, inspectorInner.top + 92},
                             "Explorer",
                             false);
            DrawTextLine(dc,
                         RECT{inspectorInner.left + 10, inspectorInner.top + 98, inspectorInner.right - 10, inspectorInner.top + 114},
                         "Ctrl+S saves resource when Files + modified. Key 4 opens Files tab.",
                         RGB(200, 196, 160),
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else if (!nodes.empty() && selectedNode_ < nodes.size()) {
            const ri::scene::Node& node = nodes[selectedNode_];
            const ri::math::Vec3 worldPos = starterScene_.scene.ComputeWorldPosition(static_cast<int>(selectedNode_));
            infoTop = inspectorInner.top + 42;
            if (inspectorPanel_ == InspectorPanel::Node) {
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             "Name: " + node.name, RGB(226, 226, 226), bodyFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop += 24;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             "Path: " + ri::scene::DescribeNodePath(starterScene_.scene, static_cast<int>(selectedNode_)),
                             RGB(210, 210, 210), smallFont_, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
                infoTop += 22;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             "Kind: " + NodeKindLabel(node),
                             RGB(228, 216, 170), smallFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop += 20;
                if (node.mesh != ri::scene::kInvalidHandle && node.mesh >= 0 &&
                    static_cast<std::size_t>(node.mesh) < starterScene_.scene.MeshCount()) {
                    const ri::scene::Mesh& mesh = starterScene_.scene.GetMesh(node.mesh);
                    DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                                 "Mesh primitive: " + ri::scene::ToString(mesh.primitive),
                                 RGB(200, 210, 220), smallFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                    infoTop += 20;
                }
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             "Local Pos: " + ri::math::ToString(node.localTransform.position),
                             RGB(200, 200, 200), smallFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop += 20;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             "Local Rot: " + ri::math::ToString(node.localTransform.rotationDegrees),
                             RGB(200, 200, 200), smallFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop += 20;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             "Local Scale: " + ri::math::ToString(node.localTransform.scale),
                             RGB(200, 200, 200), smallFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop += 20;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             "Edit Mode: " + EditModeLabel() + " (" + AxisLabel() + ")",
                             RGB(214, 208, 168), smallFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop += 20;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             "Grouping: Ctrl+G group  |  Ctrl+Shift+G ungroup  |  Ctrl+Shift+N new group",
                             RGB(214, 208, 168), smallFont_, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
                infoTop += 20;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             "Ops: Ctrl+R reset  |  Shift+F frame all  |  Ctrl+Shift+W parent to World",
                             RGB(214, 208, 168), smallFont_, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
                infoTop += 20;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             "World Pos: " + ri::math::ToString(worldPos),
                             RGB(200, 220, 200), smallFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            } else if (inspectorPanel_ == InspectorPanel::Brush) {
                DrawToolbarButton(dc,
                                  RECT{inspectorInner.left + 10,
                                       inspectorInner.top + 42,
                                       inspectorInner.left + 52,
                                       inspectorInner.top + 66},
                                  "<",
                                  false);
                DrawToolbarButton(dc,
                                  RECT{inspectorInner.left + 56,
                                       inspectorInner.top + 42,
                                       inspectorInner.left + 98,
                                       inspectorInner.top + 66},
                                  ">",
                                  false);
                DrawTextLine(dc,
                             RECT{inspectorInner.left + 104,
                                  inspectorInner.top + 44,
                                  inspectorInner.right - 10,
                                  inspectorInner.top + 64},
                             std::string(CurrentStructuralBrushPreset().label) +
                                 " (" + std::string(CurrentStructuralBrushPreset().structuralType) + ")",
                             RGB(228, 236, 248),
                             bodyFont_,
                             DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                infoTop = inspectorInner.top + 74;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             "Structural primitives",
                             RGB(240, 240, 236), headerFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop += 24;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 36},
                             "[ / ] or click < > · Ctrl+Shift+1..9 quick preset · Ctrl+Shift+B spawn · Del removes authored mesh · Ctrl+D duplicate · Ctrl+G group",
                             RGB(210, 215, 222), smallFont_, DT_LEFT | DT_WORDBREAK);
                infoTop += 42;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 44},
                             "Uses ri::structural BuildPrimitiveMesh (same family as structural CSG tooling). CSV export emits cube/plane assembly rows only.",
                             RGB(200, 206, 214), smallFont_, DT_LEFT | DT_WORDBREAK);
                infoTop += 52;
                const auto bounds =
                    ri::scene::ComputeNodeWorldBounds(starterScene_.scene, static_cast<int>(selectedNode_), false);
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             "Selection: " + node.name + "  |  " + NodeKindLabel(node),
                             RGB(226, 226, 226), smallFont_, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
                infoTop += 22;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             std::string("Mesh Attached: ") + (node.mesh != ri::scene::kInvalidHandle ? "yes" : "no"),
                             RGB(200, 200, 200), smallFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop += 20;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             bounds.has_value()
                                 ? "Bounds Size: " + ri::math::ToString(ri::scene::GetBoundsSize(*bounds))
                                 : "Bounds Size: n/a",
                             RGB(200, 220, 200), smallFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop += 20;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             bounds.has_value()
                                 ? "Bounds Center: " + ri::math::ToString(ri::scene::GetBoundsCenter(*bounds))
                                 : "Bounds Center: n/a",
                             RGB(200, 220, 200), smallFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            } else {
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             "Creator Runtime Policy", RGB(240, 240, 236), headerFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop += 24;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             "Inventory Mode: " + InventoryPresentationLabel(),
                             RGB(226, 226, 226), bodyFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop += 22;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             std::string("Gameplay Storage: ") + (creatorInventoryPolicy_.presentation == ri::world::InventoryPresentationMode::Disabled ? "off" : "on"),
                             RGB(200, 220, 200), smallFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop += 20;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             std::string("Inventory UI: ") + (creatorInventoryPolicy_.presentation == ri::world::InventoryPresentationMode::Visible ? "visible" : "hidden"),
                             RGB(200, 220, 200), smallFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop += 20;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             std::string("Off-hand Slot: ") + (creatorInventoryPolicy_.allowOffHand ? "enabled" : "disabled"),
                             RGB(200, 220, 200), smallFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop += 20;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             "Hotbar Slots: " + std::to_string(creatorInventoryPolicy_.hotbarSize),
                             RGB(200, 200, 200), smallFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop += 20;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 20},
                             "Backpack Slots: " + std::to_string(creatorInventoryPolicy_.backpackSize),
                             RGB(200, 200, 200), smallFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop += 28;
                DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 42},
                             "Controls: 1/2/3 switch inspector pages. I cycles inventory mode. O toggles off-hand.",
                             RGB(214, 208, 168), smallFont_, DT_LEFT | DT_WORDBREAK);
            }
        } else {
            infoTop = inspectorInner.top + 42;
            DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 48},
                         "Select a scene node or use Resources to open project files (levels, scripts, UI, menus).",
                         RGB(200, 200, 200),
                         smallFont_,
                         DT_LEFT | DT_WORDBREAK);
        }

        {
            const int sessionTop = inspectorInner.bottom - 54;
            DrawTextLine(dc, RECT{inspectorInner.left + 10, sessionTop, inspectorInner.right - 10, sessionTop + 18},
                         "Session", RGB(240, 240, 236), headerFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            DrawTextLine(dc,
                         RECT{inspectorInner.left + 10,
                              sessionTop + 22,
                              inspectorInner.right - 10,
                              sessionTop + 40},
                         "Undo Depth: " + std::to_string(undoStack_.size()) + "  |  Redo Depth: " +
                             std::to_string(redoStack_.size()),
                         RGB(200, 200, 200),
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

        UpdateCameraPlotRect(viewportInner);
        DrawRawIronQuadViewportBlock(dc, viewportInner);

        const ri::scene::Node& camNode = starterScene_.scene.GetNode(starterScene_.handles.orbitCamera.cameraNode);
        const std::string consoleLine =
            "t=" + std::to_string(elapsedSeconds_) +
            " | camera=" + ri::math::ToString(starterScene_.scene.ComputeWorldPosition(starterScene_.handles.orbitCamera.cameraNode)) +
            " | yaw=" + std::to_string(camNode.localTransform.rotationDegrees.y) +
            " | nodeCount=" + std::to_string(starterScene_.scene.NodeCount());
        DrawTextLine(dc, RECT{statusBar.left + 12, statusBar.top + 8, statusBar.right - 12, statusBar.top + 28},
                     consoleLine, RGB(20, 60, 20), smallFont_, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        DrawTextLine(dc, RECT{statusBar.left + 12, statusBar.top + 30, statusBar.right - 12, statusBar.top + 50},
                     "Esc clear sel · Ctrl+Shift+Q quit · Space auto-orbit · Tab view · T/R/U modes · WASDQE edit (Shift fine / Alt coarse) · Ctrl+R reset · Shift+F frame all · Ctrl+Shift+W to World · ,/. authored cycle · Ctrl+Shift+S snapshot · Ctrl+Shift+L autosave load · Ctrl+E export · Ctrl+Z/Y · Ctrl+S/L · F6",
                     RGB(32, 32, 32), smallFont_, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        DrawTextLine(dc, RECT{statusBar.left + 12, statusBar.top + 50, statusBar.right - 12, statusBar.bottom - 8},
                     "State: " + lastIoStatus_ + "  |  Scene file: " + ResolveSceneStatePath().string(),
                     RGB(40, 40, 100), smallFont_, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        EnsureResourceTextEditorCreated();
        LayoutResourceTextEditorControl(inspectorInner);

        const int savedWindowDc = SaveDC(windowDc);
#if defined(_WIN32)
        if (inspectorPanel_ == InspectorPanel::Files && resourceTextEditHwnd_ != nullptr
            && IsWindow(resourceTextEditHwnd_)) {
            RECT editWindowRect{};
            GetWindowRect(resourceTextEditHwnd_, &editWindowRect);
            POINT topLeft{editWindowRect.left, editWindowRect.top};
            POINT bottomRight{editWindowRect.right, editWindowRect.bottom};
            ScreenToClient(hwnd_, &topLeft);
            ScreenToClient(hwnd_, &bottomRight);
            ExcludeClipRect(windowDc, topLeft.x, topLeft.y, bottomRight.x, bottomRight.y);
        }
#endif
        BitBlt(windowDc, 0, 0, width, height, dc, 0, 0, SRCCOPY);
        RestoreDC(windowDc, savedWindowDc);
        SelectObject(dc, oldBitmap);
        DeleteObject(backBuffer);
        DeleteDC(dc);
        EndPaint(hwnd_, &paint);
    }

    HWND hwnd_ = nullptr;
    HFONT titleFont_ = nullptr;
    HFONT headerFont_ = nullptr;
    HFONT bodyFont_ = nullptr;
    HFONT smallFont_ = nullptr;
    bool logEveryFrame_ = false;
    bool dumpScene_ = false;
    EditorSceneConfig sceneConfig_{};
    std::string lastIoStatus_ = "No scene I/O action yet.";
    static constexpr std::size_t kMaxUndoActions = 256;
    static constexpr std::chrono::seconds kAutosaveInterval_{90};
    std::vector<EditorEditAction> undoStack_;
    std::vector<EditorEditAction> redoStack_;
    EditMode editMode_ = EditMode::Translate;
    InspectorPanel inspectorPanel_ = InspectorPanel::Node;
    int activeAxis_ = 0;
    /// Preset into `kStructuralBrushPresets` (structural / brush spawn).
    std::size_t structuralBrushPresetIndex_ = 0;
    std::size_t selectedNode_ = 0;
    int hierarchyScrollTopRow_ = 0;
    double elapsedSeconds_ = 0.0;
    std::chrono::steady_clock::time_point lastTick_{};
    std::chrono::steady_clock::time_point lastAutosaveSteady_{};
    double lastAutosaveStatusSeconds_ = -999.0;
    bool autosavePending_ = false;
    ri::scene::StarterScene starterScene_{};
    ri::world::InventoryPolicy creatorInventoryPolicy_{};
    bool statsOverlayVisible_ = false;
    ri::world::RuntimeStatsOverlayState statsOverlayState_{true};
    ri::scene::OrbitCameraState editorOrbitState_{};
    bool autoOrbitPreview_ = false;
    bool full3DViewport_ = false;
    bool cameraDragActive_ = false;
    int lastDragX_ = 0;
    int lastDragY_ = 0;
    RECT cameraPlotRect_{};

    /// Deleted authored nodes move here (meshes stripped); subtree hidden from hierarchy list.
    int editorTrashFolderHandle_ = ri::scene::kInvalidHandle;

    LeftPanelMode leftPanelMode_ = LeftPanelMode::Scene;
    std::vector<WorkspaceGameEntry> workspaceGames_;
    int focusedWorkspaceGameIndex_ = 0;
    std::vector<WorkspaceResourceEntry> resourceCatalogEntries_;
    int selectedResourceRow_ = -1;
    int resourceCatalogScrollTopRow_ = 0;

    fs::path loadedResourceAbsolutePath_;
    std::string loadedResourceUtf8_;
    std::string resourceEditorAuxMessage_;
#if defined(_WIN32)
    HWND resourceTextEditHwnd_ = nullptr;
#endif
    bool resourceFileDirty_ = false;
};
#endif

} // namespace

int main(int argc, char** argv) {
    ri::core::InitializeCrashDiagnostics();
    try {
        ri::core::CommandLine commandLine(argc, argv);
        const bool hasFrameBudgetArg = commandLine.GetValue("--frames").has_value();

#if defined(_WIN32)
        const bool forceHeadless = commandLine.HasFlag("--headless") || commandLine.HasFlag("--cli-editor");
        const bool forceGui = commandLine.HasFlag("--editor-ui");
        if (!forceHeadless && (forceGui || !hasFrameBudgetArg)) {
            RawIronEditorWindow window(commandLine);
            return window.Run(GetModuleHandleW(nullptr));
        }
#endif

        EditorHost host;

        ri::core::MainLoopOptions options;
        options.maxFrames = commandLine.GetIntOr("--frames", hasFrameBudgetArg ? 4 : 0);
        options.fixedDeltaSeconds = ResolveFixedDeltaSeconds(commandLine, 30);
        options.verboseFrames = commandLine.HasFlag("--verbose-frames");
        options.paceToFixedDelta = !commandLine.HasFlag("--unpaced");

        return ri::core::RunMainLoop(host, commandLine, options);
    } catch (const std::exception&) {
        ri::core::LogCurrentExceptionWithStackTrace("Editor Failure");
        return 1;
    }
}
