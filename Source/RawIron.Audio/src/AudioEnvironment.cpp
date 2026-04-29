#include "RawIron/Audio/AudioEnvironment.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace ri::audio {
namespace {

double ClampOrDefault(double value, double defaultValue, double minValue, double maxValue) noexcept {
    if (!std::isfinite(value)) {
        return defaultValue;
    }
    return std::max(minValue, std::min(maxValue, value));
}

std::string Trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(first, (last - first) + 1));
}

std::string JoinLabels(const std::vector<std::string>& values) {
    std::ostringstream builder;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            builder << ',';
        }
        builder << values[index];
    }
    return builder.str();
}

std::vector<std::string> NormalizeVolumeLabels(const std::vector<std::string>& raw) {
    std::set<std::string> deduped;
    for (const std::string& entry : raw) {
        const std::string trimmed = Trim(entry);
        if (!trimmed.empty()) {
            deduped.insert(trimmed);
        }
    }
    return std::vector<std::string>(deduped.begin(), deduped.end());
}

} // namespace

double ClampAudioVolume(double value, double defaultValue) noexcept {
    return ClampOrDefault(value, defaultValue, 0.0, 1.0);
}

double ClampAudioPlaybackRate(double value, double defaultValue) noexcept {
    return ClampOrDefault(value, defaultValue, 0.5, 1.5);
}

double ClampAudioPan(double value, double defaultValue) noexcept {
    return ClampOrDefault(value, defaultValue, -1.0, 1.0);
}

double ClampAudioOcclusion(double value, double defaultValue) noexcept {
    return ClampOrDefault(value, defaultValue, 0.0, 1.0);
}

AudioEnvironmentProfile NormalizeAudioEnvironmentProfile(const AudioEnvironmentProfileInput& input) noexcept {
    const std::string explicitLabel = Trim(input.label);
    const std::vector<std::string> normalizedVolumes = NormalizeVolumeLabels(input.activeVolumes);
    std::string label = explicitLabel;
    if (label.empty()) {
        label = normalizedVolumes.empty() ? std::string("environment") : JoinLabels(normalizedVolumes);
    }

    AudioEnvironmentProfile normalized{};
    normalized.label = std::move(label);
    normalized.reverbMix = ClampOrDefault(input.reverbMix.value_or(0.0), 0.0, 0.0, 1.0);
    normalized.echoDelayMs = ClampOrDefault(input.echoDelayMs.value_or(0.0), 0.0, 0.0, 2000.0);
    normalized.echoFeedback = ClampOrDefault(input.echoFeedback.value_or(0.0), 0.0, 0.0, 0.95);
    normalized.dampening = ClampOrDefault(input.dampening.value_or(0.0), 0.0, 0.0, 1.0);
    normalized.volumeScale = ClampOrDefault(input.volumeScale.value_or(1.0), 1.0, 0.2, 2.0);
    normalized.playbackRate = ClampAudioPlaybackRate(input.playbackRate.value_or(1.0), 1.0);
    normalized.activeVolumes = normalizedVolumes;
    return normalized;
}


AudioResolvedPlayback MixPlaybackWithEnvironment(
    const AudioPlaybackRequest& request,
    const std::optional<AudioEnvironmentProfile>& environment) noexcept {
    const double baseVolume = ClampAudioVolume(request.volume);
    const double baseRate = ClampAudioPlaybackRate(request.playbackRate);
    const double basePan = ClampAudioPan(request.pan);
    const double baseOcclusion = ClampAudioOcclusion(request.occlusion);
    const double baseDistance = std::max(0.0, std::isfinite(request.distanceMeters) ? request.distanceMeters : 0.0);

    if (!environment.has_value()) {
        const double dryVolume = ClampAudioVolume(baseVolume * (1.0 - (0.75 * baseOcclusion)));
        const double distanceAttenuation = 1.0 / (1.0 + (baseDistance / 8.0));
        return AudioResolvedPlayback{
            .volume = ClampAudioVolume(dryVolume * distanceAttenuation),
            .playbackRate = baseRate,
            .pan = basePan,
            .occlusion = baseOcclusion,
            .distanceMeters = baseDistance,
            .profile = std::nullopt,
        };
    }

    const AudioEnvironmentProfile& profile = *environment;
    const double occlusionVolume = baseVolume * (1.0 - (0.75 * baseOcclusion));
    const double distanceAttenuation = 1.0 / (1.0 + (baseDistance / 8.0));
    const double nextVolume =
        ClampAudioVolume(occlusionVolume * distanceAttenuation * profile.volumeScale * (1.0 - (profile.dampening * 0.2)));
    const double nextPlaybackRate = ClampAudioPlaybackRate(baseRate * profile.playbackRate);
    return AudioResolvedPlayback{
        .volume = nextVolume,
        .playbackRate = nextPlaybackRate,
        .pan = basePan,
        .occlusion = baseOcclusion,
        .distanceMeters = baseDistance,
        .profile = profile,
    };
}

std::string BuildAudioEnvironmentSignature(const std::optional<AudioEnvironmentProfile>& profile) noexcept {
    if (!profile.has_value()) {
        return "none";
    }

    std::ostringstream builder;
    builder << profile->label << '|'
            << profile->reverbMix << '|'
            << profile->echoDelayMs << '|'
            << profile->echoFeedback << '|'
            << profile->dampening << '|'
            << profile->volumeScale << '|'
            << profile->playbackRate << '|'
            << JoinLabels(profile->activeVolumes);
    return builder.str();
}

std::optional<AudioEchoLayer> TryResolveAudioEchoLayer(double sourceVolume,
                                                        double sourcePlaybackRate,
                                                        const AudioEnvironmentProfile& profile) noexcept {
    if (!(profile.reverbMix > 0.0) || !(profile.echoDelayMs > 0.0) || !(profile.echoFeedback > 0.0)) {
        return std::nullopt;
    }

    const double echoVolume = ClampAudioVolume(sourceVolume * profile.reverbMix * profile.echoFeedback, 0.0);
    if (!(echoVolume > 0.001)) {
        return std::nullopt;
    }

    return AudioEchoLayer{
        .volume = echoVolume,
        .playbackRate = ClampAudioPlaybackRate(sourcePlaybackRate * kAudioEchoPlaybackRateScale),
    };
}

} // namespace ri::audio
