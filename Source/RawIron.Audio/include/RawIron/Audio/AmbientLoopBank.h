#pragma once

#include "RawIron/Audio/AudioManager.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace ri::audio {

/// One ambient contribution the bank should keep alive this frame.
struct AmbientLoopIntent {
    std::string id;
    std::string audioPath;
    float desiredVolume = 0.0f;
    float normalizedFalloff = 0.0f;
};

/// Engine-owned ambient loop voice manager. Games feed intents from world mix state; they do not
/// own CreateLoopingSound / prune maps themselves. Voice count is a config knob (default 2).
class AmbientLoopBank {
public:
    void Sync(AudioManager& manager,
              const std::filesystem::path& contentRoot,
              const std::vector<AmbientLoopIntent>& intents,
              std::size_t voiceLimit = 2U);

    void Clear(AudioManager& manager);

    [[nodiscard]] std::size_t ActiveCount() const noexcept { return loops_.size(); }

private:
    std::unordered_map<std::string, std::shared_ptr<ManagedSound>> loops_{};
};

} // namespace ri::audio
