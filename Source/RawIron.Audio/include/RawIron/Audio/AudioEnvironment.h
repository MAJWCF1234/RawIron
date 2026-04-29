#pragma once

#include "RawIron/Audio/AudioManager.h"

#include <optional>
#include <string>

namespace ri::audio {

/// Perceived loudness clamp used for authored clips and derived echo sends.
[[nodiscard]] double ClampAudioVolume(double value, double defaultValue = 1.0) noexcept;

/// Playback-rate clamp shared by the web prototype and native backends.
[[nodiscard]] double ClampAudioPlaybackRate(double value, double defaultValue = 1.0) noexcept;
[[nodiscard]] double ClampAudioPan(double value, double defaultValue = 0.0) noexcept;
[[nodiscard]] double ClampAudioOcclusion(double value, double defaultValue = 0.0) noexcept;

/// Deterministic normalization for volume-trigger / reverb authoring payloads.
[[nodiscard]] AudioEnvironmentProfile NormalizeAudioEnvironmentProfile(
    const AudioEnvironmentProfileInput& input) noexcept;

/// Applies `volumeScale`, `dampening` (as a mild wet attenuation), and `playbackRate` from the profile.
[[nodiscard]] AudioResolvedPlayback MixPlaybackWithEnvironment(
    const AudioPlaybackRequest& request,
    const std::optional<AudioEnvironmentProfile>& environment) noexcept;

/// Stable signature for deduplicating `SetEnvironmentProfile` work (ordering-independent for `activeVolumes`).
[[nodiscard]] std::string BuildAudioEnvironmentSignature(
    const std::optional<AudioEnvironmentProfile>& profile) noexcept;

/// Echo tap derived from the primary dry send (matches legacy `scheduleEcho` intent).
struct AudioEchoLayer {
    double volume = 0.0;
    double playbackRate = 1.0;
};

/// Slight rate pull-down so delayed taps sit behind the dry transient in the mix.
inline constexpr double kAudioEchoPlaybackRateScale = 0.985;

/// Returns `std::nullopt` when echo is disabled or would be inaudible.
[[nodiscard]] std::optional<AudioEchoLayer> TryResolveAudioEchoLayer(
    double sourceVolume,
    double sourcePlaybackRate,
    const AudioEnvironmentProfile& profile) noexcept;

} // namespace ri::audio
