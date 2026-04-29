#pragma once

#include "RawIron/Audio/AudioManager.h"

#include <cstdint>
#include <memory>
#include <string>

namespace ri::audio {

/// Optional tuning for the miniaudio device / mixer. Pass `nullptr` to `CreateMiniaudioAudioBackend`
/// for defaults suited to typical 2D/sprite-style playback.
struct MiniaudioBackendSettings {
    /// When true (default), clip playback uses miniaudio's non-spatialized path so stereo files are
    /// heard as authored (center/mix) without requiring listener or sound positions. Set false when
    /// you drive `ma_sound_set_position` / listener APIs yourself.
    bool noSpatialization = true;
    /// Hint for the engine/device period length in milliseconds. Larger values increase latency;
    /// smaller values increase scheduling overhead. Zero leaves miniaudio's default period sizing.
    std::uint32_t periodSizeInMilliseconds = 0;
    /// Optional listener transform bootstrap (used when any sound enables spatialization).
    float listenerPosition[3] = {0.0f, 0.0f, 0.0f};
    float listenerDirection[3] = {0.0f, 0.0f, -1.0f};
    float listenerWorldUp[3] = {0.0f, 1.0f, 0.0f};
};

/// Opens the default native output device (WASAPI / Core Audio / ALSA / PulseAudio, etc.) via miniaudio.
/// Clip paths in `AudioClipRequest` are native filesystem paths (or paths relative to the process cwd /
/// content roots your app sets up). The web prototype does not serve or bundle these assets.
/// Linked statically into RawIron.Audio — no browser stack and no separate audio DLL.
///
/// On Windows, UTF-8 paths are opened via the wide-character miniaudio entry point so non-ASCII
/// filenames under user directories work reliably.
///
/// @param errorMessage optional; receives a short diagnostic if initialization fails
/// @param settings optional; nullptr selects defaults (`noSpatialization = true`, native period length)
[[nodiscard]] std::shared_ptr<AudioBackend> CreateMiniaudioAudioBackend(
    std::string* errorMessage = nullptr,
    const MiniaudioBackendSettings* settings = nullptr);

} // namespace ri::audio
