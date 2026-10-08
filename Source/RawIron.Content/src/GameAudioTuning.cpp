#include "RawIron/Content/GameAudioTuning.h"

#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/ScriptScalars.h"

namespace ri::content {

GameAudioTuningScalars LoadGameAudioTuningScalars(const std::filesystem::path& gameRoot) {
    const ScriptScalarMap scalars =
        LoadScriptScalars(ResolveGameAssetPath(gameRoot, "scripts/audio.riscript"));
    return LoadGameAudioTuningScalars(scalars);
}

GameAudioTuningScalars LoadGameAudioTuningScalars(const ScriptScalarMap& scalars) {
    GameAudioTuningScalars tuning{};
    if (scalars.empty()) {
        return tuning;
    }
    tuning.loaded = true;
    tuning.masterGain = ScriptScalarOrClamped(scalars, "audio_master_gain", 1.0f, 0.0f, 4.0f);
    tuning.environmentBlend =
        ScriptScalarOrClamped(scalars, "audio_environment_blend", 1.0f, 0.0f, 2.0f);
    return tuning;
}

} // namespace ri::content
