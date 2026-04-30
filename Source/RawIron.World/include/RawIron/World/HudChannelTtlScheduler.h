#pragma once

#include "RawIron/World/PresentationState.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace ri::world {

/// Per-channel transient HUD lines with **overwrite** reschedule semantics: a new \p ttlMs for the
/// same \p channel replaces any prior timer, giving deterministic last-writer wins behaviour under bursts.
class HudChannelTtlScheduler {
public:
    void Schedule(std::string channel, std::string text, double ttlMs);
    bool ClearHudDismissTimer(std::string_view channel);
    void Advance(double elapsedMs);
    void Clear();

    [[nodiscard]] std::optional<TimedPresentationEntry> Active(std::string_view channel) const;

private:
    std::unordered_map<std::string, TimedPresentationEntry> channels_{};
};

} // namespace ri::world
