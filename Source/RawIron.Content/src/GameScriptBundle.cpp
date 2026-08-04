#include "RawIron/Content/GameScriptBundle.h"

#include "RawIron/Content/GameManifest.h"
#include "RawIron/Core/Log.h"

namespace ri::content {
namespace {

void LoadNamed(ScriptScalarMap& destination,
               const std::filesystem::path& gameRoot,
               const char* relativePath,
               const char* label,
               const bool logMissing) {
    destination = LoadScriptScalars(ResolveGameAssetPath(gameRoot, relativePath));
    if (logMissing && destination.empty()) {
        ri::core::LogInfo(std::string(label) + " not found or empty; using defaults.");
    }
}

} // namespace

GameScriptBundle LoadGameScriptBundle(const std::filesystem::path& gameRoot,
                                      const GameScriptBundleLoadOptions& options) {
    GameScriptBundle bundle{};
    LoadNamed(bundle.gameplay, gameRoot, "scripts/gameplay.riscript", "Gameplay tuning script", options.logMissing);
    LoadNamed(bundle.rendering, gameRoot, "scripts/rendering.riscript", "Rendering tuning script", options.logMissing);
    LoadNamed(bundle.ui, gameRoot, "scripts/ui.riscript", "UI tuning script", options.logMissing);
    LoadNamed(bundle.audio, gameRoot, "scripts/audio.riscript", "Audio tuning script", options.logMissing);
    LoadNamed(bundle.streaming, gameRoot, "scripts/streaming.riscript", "Streaming tuning script", options.logMissing);
    LoadNamed(
        bundle.localization, gameRoot, "scripts/localization.riscript", "Localization tuning script", options.logMissing);
    LoadNamed(bundle.physics, gameRoot, "scripts/physics.riscript", "Physics tuning script", options.logMissing);
    LoadNamed(
        bundle.postprocess, gameRoot, "scripts/postprocess.riscript", "Postprocess tuning script", options.logMissing);
    LoadNamed(bundle.init, gameRoot, "scripts/init.riscript", "Init tuning script", options.logMissing);
    LoadNamed(bundle.network, gameRoot, "scripts/network.riscript", "Network tuning script", options.logMissing);
    LoadNamed(
        bundle.persistence, gameRoot, "scripts/persistence.riscript", "Persistence tuning script", options.logMissing);
    LoadNamed(bundle.ai, gameRoot, "scripts/ai.riscript", "AI tuning script", options.logMissing);
    LoadNamed(bundle.plugins, gameRoot, "scripts/plugins.riscript", "Plugins tuning script", options.logMissing);
    LoadNamed(bundle.animation, gameRoot, "scripts/animation.riscript", "Animation tuning script", options.logMissing);
    LoadNamed(bundle.vfx, gameRoot, "scripts/vfx.riscript", "VFX tuning script", options.logMissing);
    LoadNamed(bundle.gameCfg, gameRoot, "config/game.cfg", "Game cfg", options.logMissing);
    LoadNamed(bundle.networkCfg, gameRoot, "config/network.cfg", "Network cfg", options.logMissing);
    LoadNamed(bundle.buildProfile, gameRoot, "config/build.profile", "Build profile", options.logMissing);
    LoadNamed(bundle.securityPolicy, gameRoot, "config/security.policy", "Security policy", options.logMissing);
    LoadNamed(bundle.pluginsPolicy, gameRoot, "config/plugins.policy", "Plugins policy", options.logMissing);
    return bundle;
}

} // namespace ri::content
