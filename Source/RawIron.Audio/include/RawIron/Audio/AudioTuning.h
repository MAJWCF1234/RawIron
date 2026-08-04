#pragma once

#include "RawIron/Audio/AudioManager.h"

#include <string>
#include <vector>

namespace ri::audio {

/// Authoring contract from `scripts/audio.riscript` after content load.
struct AudioTuningContract {
    double masterGain = 1.0;
    double environmentBlend = 1.0;
};

/// Clamp authored master gain into the attenuation-only mixer range [0,1] and push it to the manager.
/// Returns the applied gain. When authored > 1, writes an explanation into `clampMessage` if provided.
[[nodiscard]] double ApplyAudioMasterGain(AudioManager& manager,
                                          double authoredMasterGain,
                                          std::string* clampMessage = nullptr);

/// Interpolate authored environment colouring toward each effect's neutral value by `environmentBlend`.
/// `0` = dry, `1` = authored as-is, `2` = exaggerated.
[[nodiscard]] AudioEnvironmentProfileInput BlendAudioEnvironmentProfile(
    const std::string& label,
    const std::vector<std::string>& activeVolumes,
    float reverbMix,
    float echoDelayMs,
    float echoFeedback,
    float dampening,
    float volumeScale,
    float playbackRate,
    double environmentBlend,
    double ambientVolumeScale = 1.0);

} // namespace ri::audio
