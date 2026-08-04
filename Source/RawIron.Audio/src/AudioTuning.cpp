#include "RawIron/Audio/AudioTuning.h"

#include <algorithm>
#include <cmath>

namespace ri::audio {
namespace {

[[nodiscard]] double BlendFromNeutral(const double neutral,
                                      const float authored,
                                      const double blend) noexcept {
    return neutral + ((static_cast<double>(authored) - neutral) * blend);
}

} // namespace

double ApplyAudioMasterGain(AudioManager& manager,
                            const double authoredMasterGain,
                            std::string* clampMessage) {
    const double finiteGain = std::isfinite(authoredMasterGain) ? authoredMasterGain : 1.0;
    const double applied = std::clamp(finiteGain, 0.0, 1.0);
    if (clampMessage != nullptr && finiteGain > 1.0) {
        *clampMessage = "audio_master_gain=" + std::to_string(finiteGain)
            + " exceeds the attenuation-only mixer range; clamped to 1.0.";
    } else if (clampMessage != nullptr) {
        clampMessage->clear();
    }
    manager.SetMasterLinearGain(applied);
    return applied;
}

AudioEnvironmentProfileInput BlendAudioEnvironmentProfile(const std::string& label,
                                                          const std::vector<std::string>& activeVolumes,
                                                          const float reverbMix,
                                                          const float echoDelayMs,
                                                          const float echoFeedback,
                                                          const float dampening,
                                                          const float volumeScale,
                                                          const float playbackRate,
                                                          const double environmentBlend,
                                                          const double ambientVolumeScale) {
    const double blend = std::isfinite(environmentBlend) ? environmentBlend : 1.0;
    AudioEnvironmentProfileInput profile{};
    profile.label = label;
    profile.activeVolumes = activeVolumes;
    profile.reverbMix = std::clamp(BlendFromNeutral(0.0, reverbMix, blend), 0.0, 1.0);
    profile.echoDelayMs = echoDelayMs;
    profile.echoFeedback = std::clamp(BlendFromNeutral(0.0, echoFeedback, blend), 0.0, 1.0);
    profile.dampening = std::clamp(BlendFromNeutral(0.0, dampening, blend), 0.0, 1.0);
    profile.volumeScale = std::clamp(
        BlendFromNeutral(1.0, volumeScale, blend) * ambientVolumeScale, 0.1, 2.0);
    profile.playbackRate = std::clamp(static_cast<double>(playbackRate), 0.5, 1.5);
    return profile;
}

} // namespace ri::audio
