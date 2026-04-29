#include "RawIron/Content/GameManifest.h"
#include "RawIron/Core/Detail/JsonScan.h"

#include <algorithm>
#include <array>
#include <unordered_set>

namespace ri::content {

namespace fs = std::filesystem;
namespace {
constexpr std::string_view kCurrentGameFormatContract = "rawiron-game-v1.3.7";

constexpr std::array kRequiredGameFiles = {
    "manifest.json",
    "README.md",
    "scripts/gameplay.riscript",
    "scripts/rendering.riscript",
    "scripts/logic.riscript",
    "scripts/ui.riscript",
    "scripts/audio.riscript",
    "scripts/streaming.riscript",
    "scripts/localization.riscript",
    "scripts/physics.riscript",
    "scripts/postprocess.riscript",
    "scripts/init.riscript",
    "scripts/state.riscript",
    "scripts/network.riscript",
    "scripts/persistence.riscript",
    "scripts/ai.riscript",
    "scripts/plugins.riscript",
    "scripts/animation.riscript",
    "scripts/vfx.riscript",
    "config/game.cfg",
    "config/input.map",
    "config/project.dev",
    "config/network.cfg",
    "config/build.profile",
    "config/security.policy",
    "config/plugins.policy",
    "levels/assembly.primitives.csv",
    "levels/assembly.colliders.csv",
    "levels/assembly.navmesh",
    "levels/assembly.zones.csv",
    "levels/assembly.ai.nodes",
    "levels/assembly.lighting.csv",
    "levels/assembly.cinematics.csv",
    "levels/assembly.triggers.csv",
    "levels/assembly.occlusion.csv",
    "levels/assembly.audio.zones",
    "levels/assembly.lods.csv",
    "assets/palette.ripalette",
    "assets/layers.config",
    "assets/manifest.assets",
    "assets/metadata.json",
    "assets/dependencies.json",
    "assets/streaming.manifest",
    "assets/shaders.manifest",
    "assets/animation.graph",
    "assets/vfx.manifest",
    "assets/materials.manifest",
    "assets/audio.banks",
    "assets/fonts.manifest",
    "data/schema.db",
    "data/lookup.index",
    "data/entity.registry",
    "data/telemetry.db",
    "data/save.schema",
    "data/achievements.registry",
    "plugins/manifest.plugins",
    "plugins/load_order.cfg",
    "plugins/registry.json",
    "plugins/hooks.riplugin",
    "ai/behavior.tree",
    "ai/blackboard.json",
    "ai/factions.cfg",
    "ai/perception.cfg",
    "ai/squad.tactics",
    "ui/layout.xml",
    "ui/styling.css",
    "tests/gameplay.test.riscript",
    "tests/rendering.test.riscript",
    "tests/network.test.riscript",
    "tests/ui.test.riscript",
};

constexpr std::array<std::string_view, 3> kAllowedManifestTypes = {
    "test-game",
    "game",
    "experience",
};

bool IsSemanticVersionTriplet(std::string_view value) {
    int componentCount = 0;
    std::size_t cursor = 0;
    while (cursor < value.size()) {
        std::size_t nextDot = value.find('.', cursor);
        const std::size_t end = (nextDot == std::string_view::npos) ? value.size() : nextDot;
        if (end == cursor) {
            return false;
        }
        for (std::size_t i = cursor; i < end; ++i) {
            if (value[i] < '0' || value[i] > '9') {
                return false;
            }
        }
        ++componentCount;
        if (nextDot == std::string_view::npos) {
            break;
        }
        cursor = nextDot + 1;
    }
    return componentCount == 3;
}

bool IsSlugToken(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    if (value.front() == '-' || value.back() == '-') {
        return false;
    }
    for (const char ch : value) {
        const bool lower = (ch >= 'a' && ch <= 'z');
        const bool digit = (ch >= '0' && ch <= '9');
        if (!lower && !digit && ch != '-') {
            return false;
        }
    }
    return true;
}

bool EndsWith(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size()
        && value.substr(value.size() - suffix.size()) == suffix;
}

bool IsAllowedManifestType(std::string_view value) {
    return std::find(kAllowedManifestTypes.begin(), kAllowedManifestTypes.end(), value) != kAllowedManifestTypes.end();
}

bool IsNonEmptyFile(const fs::path& path) {
    std::error_code ec{};
    if (!fs::exists(path, ec) || fs::is_directory(path, ec)) {
        return false;
    }
    return fs::file_size(path, ec) > 0U;
}

} // namespace

bool LooksLikeWorkspaceRoot(const fs::path& path) {
    return fs::exists(path / "CMakeLists.txt")
        && fs::exists(path / "Source")
        && fs::exists(path / "Games");
}

fs::path DetectWorkspaceRoot(const fs::path& startPath) {
    fs::path current = startPath;
    if (current.empty()) {
        current = fs::current_path();
    }
    if (fs::is_regular_file(current)) {
        current = current.parent_path();
    }

    while (!current.empty()) {
        if (LooksLikeWorkspaceRoot(current)) {
            return current;
        }
        const fs::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    return startPath.empty() ? fs::current_path() : startPath;
}

std::optional<GameManifest> LoadGameManifest(const fs::path& manifestPath) {
    const std::string text = ri::core::detail::ReadTextFile(manifestPath);
    if (text.empty()) {
        return std::nullopt;
    }

    GameManifest manifest{};
    manifest.id = ri::core::detail::ExtractJsonString(text, "id").value_or("");
    manifest.name = ri::core::detail::ExtractJsonString(text, "name").value_or("");
    manifest.format = ri::core::detail::ExtractJsonString(text, "format").value_or("");
    manifest.type = ri::core::detail::ExtractJsonString(text, "type").value_or("");
    manifest.entry = ri::core::detail::ExtractJsonString(text, "entry").value_or("");
    manifest.version = ri::core::detail::ExtractJsonString(text, "version").value_or("");
    manifest.author = ri::core::detail::ExtractJsonString(text, "author").value_or("");
    manifest.editorProjectArg = ri::core::detail::ExtractJsonString(text, "editorProjectArg").value_or("");
    manifest.primaryLevel = ri::core::detail::ExtractJsonString(text, "primaryLevel").value_or("");
    manifest.description = ri::core::detail::ExtractJsonString(text, "description").value_or("");
    manifest.editorOpenArgs = ri::core::detail::ExtractJsonStringArray(text, "editorOpenArgs");
    manifest.editorPreviewScene = ri::core::detail::ExtractJsonString(text, "editorPreviewScene").value_or("");
    manifest.manifestPath = manifestPath;
    manifest.rootPath = manifestPath.parent_path();

    if (manifest.id.empty() || manifest.entry.empty()) {
        return std::nullopt;
    }

    if (manifest.name.empty()) {
        manifest.name = manifest.id;
    }

    return manifest;
}

std::optional<GameManifest> ResolveGameManifest(const fs::path& workspaceRoot, std::string_view gameId) {
    const fs::path gamesRoot = workspaceRoot / "Games";
    if (!fs::exists(gamesRoot) || !fs::is_directory(gamesRoot)) {
        return std::nullopt;
    }

    const fs::path directManifest = gamesRoot / std::string(gameId) / "manifest.json";
    if (fs::exists(directManifest)) {
        if (const std::optional<GameManifest> manifest = LoadGameManifest(directManifest);
            manifest.has_value() && manifest->id == gameId) {
            return manifest;
        }
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(gamesRoot)) {
        if (!entry.is_directory()) {
            continue;
        }
        const fs::path manifestPath = entry.path() / "manifest.json";
        if (!fs::exists(manifestPath)) {
            continue;
        }
        const std::optional<GameManifest> manifest = LoadGameManifest(manifestPath);
        if (manifest.has_value() && manifest->id == gameId) {
            return manifest;
        }
    }

    return std::nullopt;
}

std::vector<std::string> ValidateGameProjectFormat(const GameManifest& manifest) {
    std::vector<std::string> issues{};
    const fs::path root = manifest.rootPath;
    if (root.empty()) {
        issues.push_back("game root path is empty.");
        return issues;
    }

    if (manifest.id.empty()) {
        issues.push_back("manifest.id must be a non-empty string.");
    } else if (!IsSlugToken(manifest.id)) {
        issues.push_back("manifest.id must use lowercase slug format (a-z, 0-9, hyphen).");
    }
    if (manifest.name.empty()) {
        issues.push_back("manifest.name must be a non-empty string.");
    }
    if (manifest.type.empty()) {
        issues.push_back("manifest.type must be a non-empty string.");
    } else if (!IsAllowedManifestType(manifest.type)) {
        issues.push_back("manifest.type must be one of: test-game, game, experience.");
    }
    if (manifest.entry.empty()) {
        issues.push_back("manifest.entry must be a non-empty string.");
    }

    if (manifest.format != kCurrentGameFormatContract) {
        issues.push_back("manifest.format must be \"" + std::string(kCurrentGameFormatContract) + "\".");
    }
    if (manifest.version.empty()) {
        issues.push_back("manifest.version must be a non-empty string.");
    } else if (!IsSemanticVersionTriplet(manifest.version)) {
        issues.push_back("manifest.version must use semantic triplet format (e.g. \"1.3.7\").");
    }
    if (manifest.author.empty()) {
        issues.push_back("manifest.author must be a non-empty string.");
    }
    if (manifest.editorPreviewScene.empty()) {
        issues.push_back("manifest.editorPreviewScene must be a non-empty string.");
    } else if (!IsSlugToken(manifest.editorPreviewScene)) {
        issues.push_back("manifest.editorPreviewScene must use lowercase slug format (a-z, 0-9, hyphen).");
    }
    if (manifest.editorProjectArg.empty()) {
        issues.push_back("manifest.editorProjectArg must be a non-empty string.");
    } else {
        const std::string expectedArg = "--game=" + manifest.id;
        if (manifest.editorProjectArg != expectedArg) {
            issues.push_back("manifest.editorProjectArg must match \"" + expectedArg + "\".");
        }
        if (std::find(manifest.editorOpenArgs.begin(), manifest.editorOpenArgs.end(), manifest.editorProjectArg)
            == manifest.editorOpenArgs.end()) {
            issues.push_back("manifest.editorOpenArgs must include manifest.editorProjectArg.");
        }
        std::unordered_set<std::string> uniqueArgs;
        for (const std::string& arg : manifest.editorOpenArgs) {
            if (arg.empty()) {
                issues.push_back("manifest.editorOpenArgs cannot contain empty tokens.");
                break;
            }
            if (!uniqueArgs.insert(arg).second) {
                issues.push_back("manifest.editorOpenArgs cannot contain duplicate tokens.");
                break;
            }
        }
    }
    if (manifest.primaryLevel.empty()) {
        issues.push_back("manifest.primaryLevel must be a non-empty string.");
    } else if (!EndsWith(manifest.primaryLevel, ".primitives.csv")) {
        issues.push_back("manifest.primaryLevel must point to a .primitives.csv file.");
    } else {
        const fs::path resolvedPrimaryLevel = ResolveGameAssetPath(root, manifest.primaryLevel);
        if (resolvedPrimaryLevel.empty()) {
            issues.push_back("manifest.primaryLevel must resolve under the game root.");
        } else if (!fs::exists(resolvedPrimaryLevel)) {
            issues.push_back("missing primary level file at manifest.primaryLevel.");
        }
    }
    for (const std::string_view requiredPath : kRequiredGameFiles) {
        if (!fs::exists(root / fs::path(requiredPath))) {
            issues.push_back("missing " + std::string(requiredPath) + ".");
        }
    }
    if (const fs::path inputMapPath = root / "config" / "input.map";
        fs::exists(inputMapPath) && !IsNonEmptyFile(inputMapPath)) {
        issues.push_back("config/input.map must be a non-empty file.");
    }
    if (const fs::path assetsManifestPath = root / "assets" / "manifest.assets";
        fs::exists(assetsManifestPath) && !IsNonEmptyFile(assetsManifestPath)) {
        issues.push_back("assets/manifest.assets must be a non-empty file.");
    }
    if (const fs::path layersConfigPath = root / "assets" / "layers.config";
        fs::exists(layersConfigPath) && !IsNonEmptyFile(layersConfigPath)) {
        issues.push_back("assets/layers.config must be a non-empty file.");
    }
    if (const fs::path networkConfigPath = root / "config" / "network.cfg";
        fs::exists(networkConfigPath) && !IsNonEmptyFile(networkConfigPath)) {
        issues.push_back("config/network.cfg must be a non-empty file.");
    }
    if (const fs::path buildProfilePath = root / "config" / "build.profile";
        fs::exists(buildProfilePath) && !IsNonEmptyFile(buildProfilePath)) {
        issues.push_back("config/build.profile must be a non-empty file.");
    }
    if (const fs::path aiScriptPath = root / "scripts" / "ai.riscript";
        fs::exists(aiScriptPath) && !IsNonEmptyFile(aiScriptPath)) {
        issues.push_back("scripts/ai.riscript must be a non-empty file.");
    }
    if (const fs::path securityPolicyPath = root / "config" / "security.policy";
        fs::exists(securityPolicyPath) && !IsNonEmptyFile(securityPolicyPath)) {
        issues.push_back("config/security.policy must be a non-empty file.");
    }
    if (const fs::path aiNodesPath = root / "levels" / "assembly.ai.nodes";
        fs::exists(aiNodesPath) && !IsNonEmptyFile(aiNodesPath)) {
        issues.push_back("levels/assembly.ai.nodes must be a non-empty file.");
    }
    if (const fs::path metadataPath = root / "assets" / "metadata.json"; fs::exists(metadataPath)) {
        const std::string metadataText = ri::core::detail::ReadTextFile(metadataPath);
        if (metadataText.empty()) {
            issues.push_back("assets/metadata.json must be a non-empty file.");
        } else if (!ri::core::detail::ExtractJsonInt(metadataText, "rawironMetadataVersion").has_value()) {
            issues.push_back("assets/metadata.json must include integer key rawironMetadataVersion.");
        }
    }
    if (const fs::path dependenciesPath = root / "assets" / "dependencies.json";
        fs::exists(dependenciesPath) && !IsNonEmptyFile(dependenciesPath)) {
        issues.push_back("assets/dependencies.json must be a non-empty file.");
    }
    if (const fs::path zonesPath = root / "levels" / "assembly.zones.csv";
        fs::exists(zonesPath) && !IsNonEmptyFile(zonesPath)) {
        issues.push_back("levels/assembly.zones.csv must be a non-empty file.");
    }
    if (const fs::path streamingManifestPath = root / "assets" / "streaming.manifest";
        fs::exists(streamingManifestPath) && !IsNonEmptyFile(streamingManifestPath)) {
        issues.push_back("assets/streaming.manifest must be a non-empty file.");
    }
    if (const fs::path lookupIndexPath = root / "data" / "lookup.index";
        fs::exists(lookupIndexPath) && !IsNonEmptyFile(lookupIndexPath)) {
        issues.push_back("data/lookup.index must be a non-empty file.");
    }
    if (const fs::path shadersManifestPath = root / "assets" / "shaders.manifest";
        fs::exists(shadersManifestPath) && !IsNonEmptyFile(shadersManifestPath)) {
        issues.push_back("assets/shaders.manifest must be a non-empty file.");
    }
    if (const fs::path entityRegistryPath = root / "data" / "entity.registry";
        fs::exists(entityRegistryPath) && !IsNonEmptyFile(entityRegistryPath)) {
        issues.push_back("data/entity.registry must be a non-empty file.");
    }
    if (const fs::path aiBehaviorPath = root / "ai" / "behavior.tree";
        fs::exists(aiBehaviorPath) && !IsNonEmptyFile(aiBehaviorPath)) {
        issues.push_back("ai/behavior.tree must be a non-empty file.");
    }
    if (const fs::path aiBlackboardPath = root / "ai" / "blackboard.json";
        fs::exists(aiBlackboardPath) && !IsNonEmptyFile(aiBlackboardPath)) {
        issues.push_back("ai/blackboard.json must be a non-empty file.");
    }
    if (const fs::path aiFactionsPath = root / "ai" / "factions.cfg";
        fs::exists(aiFactionsPath) && !IsNonEmptyFile(aiFactionsPath)) {
        issues.push_back("ai/factions.cfg must be a non-empty file.");
    }
    if (const fs::path pluginsScriptPath = root / "scripts" / "plugins.riscript";
        fs::exists(pluginsScriptPath) && !IsNonEmptyFile(pluginsScriptPath)) {
        issues.push_back("scripts/plugins.riscript must be a non-empty file.");
    }
    if (const fs::path animationScriptPath = root / "scripts" / "animation.riscript";
        fs::exists(animationScriptPath) && !IsNonEmptyFile(animationScriptPath)) {
        issues.push_back("scripts/animation.riscript must be a non-empty file.");
    }
    if (const fs::path vfxScriptPath = root / "scripts" / "vfx.riscript";
        fs::exists(vfxScriptPath) && !IsNonEmptyFile(vfxScriptPath)) {
        issues.push_back("scripts/vfx.riscript must be a non-empty file.");
    }
    if (const fs::path pluginsPolicyPath = root / "config" / "plugins.policy";
        fs::exists(pluginsPolicyPath) && !IsNonEmptyFile(pluginsPolicyPath)) {
        issues.push_back("config/plugins.policy must be a non-empty file.");
    }
    if (const fs::path lightingCsvPath = root / "levels" / "assembly.lighting.csv";
        fs::exists(lightingCsvPath) && !IsNonEmptyFile(lightingCsvPath)) {
        issues.push_back("levels/assembly.lighting.csv must be a non-empty file.");
    }
    if (const fs::path cinematicsCsvPath = root / "levels" / "assembly.cinematics.csv";
        fs::exists(cinematicsCsvPath) && !IsNonEmptyFile(cinematicsCsvPath)) {
        issues.push_back("levels/assembly.cinematics.csv must be a non-empty file.");
    }
    if (const fs::path animationGraphPath = root / "assets" / "animation.graph";
        fs::exists(animationGraphPath) && !IsNonEmptyFile(animationGraphPath)) {
        issues.push_back("assets/animation.graph must be a non-empty file.");
    }
    if (const fs::path vfxManifestPath = root / "assets" / "vfx.manifest";
        fs::exists(vfxManifestPath) && !IsNonEmptyFile(vfxManifestPath)) {
        issues.push_back("assets/vfx.manifest must be a non-empty file.");
    }
    if (const fs::path telemetryDbPath = root / "data" / "telemetry.db";
        fs::exists(telemetryDbPath) && !IsNonEmptyFile(telemetryDbPath)) {
        issues.push_back("data/telemetry.db must be a non-empty file.");
    }
    if (const fs::path pluginsManifestPath = root / "plugins" / "manifest.plugins";
        fs::exists(pluginsManifestPath) && !IsNonEmptyFile(pluginsManifestPath)) {
        issues.push_back("plugins/manifest.plugins must be a non-empty file.");
    }
    if (const fs::path pluginsLoadOrderPath = root / "plugins" / "load_order.cfg";
        fs::exists(pluginsLoadOrderPath) && !IsNonEmptyFile(pluginsLoadOrderPath)) {
        issues.push_back("plugins/load_order.cfg must be a non-empty file.");
    }
    if (const fs::path pluginsRegistryPath = root / "plugins" / "registry.json";
        fs::exists(pluginsRegistryPath) && !IsNonEmptyFile(pluginsRegistryPath)) {
        issues.push_back("plugins/registry.json must be a non-empty file.");
    }
    if (const fs::path pluginsHooksPath = root / "plugins" / "hooks.riplugin";
        fs::exists(pluginsHooksPath) && !IsNonEmptyFile(pluginsHooksPath)) {
        issues.push_back("plugins/hooks.riplugin must be a non-empty file.");
    }
    if (const fs::path triggersCsvPath = root / "levels" / "assembly.triggers.csv";
        fs::exists(triggersCsvPath) && !IsNonEmptyFile(triggersCsvPath)) {
        issues.push_back("levels/assembly.triggers.csv must be a non-empty file.");
    }
    if (const fs::path occlusionCsvPath = root / "levels" / "assembly.occlusion.csv";
        fs::exists(occlusionCsvPath) && !IsNonEmptyFile(occlusionCsvPath)) {
        issues.push_back("levels/assembly.occlusion.csv must be a non-empty file.");
    }
    if (const fs::path materialsManifestPath = root / "assets" / "materials.manifest";
        fs::exists(materialsManifestPath) && !IsNonEmptyFile(materialsManifestPath)) {
        issues.push_back("assets/materials.manifest must be a non-empty file.");
    }
    if (const fs::path audioBanksPath = root / "assets" / "audio.banks";
        fs::exists(audioBanksPath) && !IsNonEmptyFile(audioBanksPath)) {
        issues.push_back("assets/audio.banks must be a non-empty file.");
    }
    if (const fs::path fontsManifestPath = root / "assets" / "fonts.manifest";
        fs::exists(fontsManifestPath) && !IsNonEmptyFile(fontsManifestPath)) {
        issues.push_back("assets/fonts.manifest must be a non-empty file.");
    }
    if (const fs::path saveSchemaPath = root / "data" / "save.schema";
        fs::exists(saveSchemaPath) && !IsNonEmptyFile(saveSchemaPath)) {
        issues.push_back("data/save.schema must be a non-empty file.");
    }
    if (const fs::path achievementsRegistryPath = root / "data" / "achievements.registry";
        fs::exists(achievementsRegistryPath) && !IsNonEmptyFile(achievementsRegistryPath)) {
        issues.push_back("data/achievements.registry must be a non-empty file.");
    }
    if (const fs::path perceptionCfgPath = root / "ai" / "perception.cfg";
        fs::exists(perceptionCfgPath) && !IsNonEmptyFile(perceptionCfgPath)) {
        issues.push_back("ai/perception.cfg must be a non-empty file.");
    }
    if (const fs::path squadTacticsPath = root / "ai" / "squad.tactics";
        fs::exists(squadTacticsPath) && !IsNonEmptyFile(squadTacticsPath)) {
        issues.push_back("ai/squad.tactics must be a non-empty file.");
    }
    if (const fs::path uiLayoutPath = root / "ui" / "layout.xml";
        fs::exists(uiLayoutPath) && !IsNonEmptyFile(uiLayoutPath)) {
        issues.push_back("ui/layout.xml must be a non-empty file.");
    }
    if (const fs::path uiStylingPath = root / "ui" / "styling.css";
        fs::exists(uiStylingPath) && !IsNonEmptyFile(uiStylingPath)) {
        issues.push_back("ui/styling.css must be a non-empty file.");
    }
    if (const fs::path gameplayTestsPath = root / "tests" / "gameplay.test.riscript";
        fs::exists(gameplayTestsPath) && !IsNonEmptyFile(gameplayTestsPath)) {
        issues.push_back("tests/gameplay.test.riscript must be a non-empty file.");
    }
    if (const fs::path audioZonesPath = root / "levels" / "assembly.audio.zones";
        fs::exists(audioZonesPath) && !IsNonEmptyFile(audioZonesPath)) {
        issues.push_back("levels/assembly.audio.zones must be a non-empty file.");
    }
    if (const fs::path lodsPath = root / "levels" / "assembly.lods.csv";
        fs::exists(lodsPath) && !IsNonEmptyFile(lodsPath)) {
        issues.push_back("levels/assembly.lods.csv must be a non-empty file.");
    }
    if (const fs::path renderingTestsPath = root / "tests" / "rendering.test.riscript";
        fs::exists(renderingTestsPath) && !IsNonEmptyFile(renderingTestsPath)) {
        issues.push_back("tests/rendering.test.riscript must be a non-empty file.");
    }
    if (const fs::path networkTestsPath = root / "tests" / "network.test.riscript";
        fs::exists(networkTestsPath) && !IsNonEmptyFile(networkTestsPath)) {
        issues.push_back("tests/network.test.riscript must be a non-empty file.");
    }
    if (const fs::path uiTestsPath = root / "tests" / "ui.test.riscript";
        fs::exists(uiTestsPath) && !IsNonEmptyFile(uiTestsPath)) {
        issues.push_back("tests/ui.test.riscript must be a non-empty file.");
    }

    return issues;
}

std::filesystem::path ResolveGameAssetPath(const std::filesystem::path& gameRoot, std::string_view relativeUtf8) {
    if (gameRoot.empty()) {
        return {};
    }
    std::filesystem::path relative(relativeUtf8);
    if (relative.is_absolute()) {
        return {};
    }

    const std::filesystem::path combined = (gameRoot / relative).lexically_normal();
    const std::filesystem::path rootNorm = gameRoot.lexically_normal();

    auto r = rootNorm.begin();
    auto c = combined.begin();
    for (; r != rootNorm.end(); ++r, ++c) {
        if (c == combined.end() || *c != *r) {
            return {};
        }
    }
    return combined;
}

} // namespace ri::content


