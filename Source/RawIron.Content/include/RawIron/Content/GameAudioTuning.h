#pragma once

#include <filesystem>

namespace ri::content {

/// Parsed `scripts/audio.riscript` scalars. Engine-owned load path; Audio applies them.
struct GameAudioTuningScalars {
    float masterGain = 1.0f;
    float environmentBlend = 1.0f;
    /// False when the script was missing or empty (defaults remain valid).
    bool loaded = false;
};

[[nodiscard]] GameAudioTuningScalars LoadGameAudioTuningScalars(const std::filesystem::path& gameRoot);

} // namespace ri::content
