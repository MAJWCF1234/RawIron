#include "RawIron/World/HudChannelTtlScheduler.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ri::world {
namespace {

double SanitizeDurationMs(double durationMs) {
    if (!std::isfinite(durationMs)) {
        return 4000.0;
    }
    return std::clamp(durationMs, 0.0, 180000.0);
}

} // namespace

void HudChannelTtlScheduler::Schedule(std::string channel, std::string text, const double ttlMs) {
    const double safe = SanitizeDurationMs(ttlMs);
    channels_[std::move(channel)] = TimedPresentationEntry{
        .text = std::move(text),
        .durationMs = safe,
        .remainingMs = safe,
        .severity = PresentationSeverity::Normal,
    };
}

bool HudChannelTtlScheduler::ClearHudDismissTimer(const std::string_view channel) {
    const auto it = channels_.find(std::string(channel));
    if (it == channels_.end()) {
        return false;
    }
    channels_.erase(it);
    return true;
}

void HudChannelTtlScheduler::Advance(const double elapsedMs) {
    for (auto it = channels_.begin(); it != channels_.end();) {
        TimedPresentationEntry& entry = it->second;
        if (!std::isfinite(elapsedMs) || elapsedMs <= 0.0) {
            ++it;
            continue;
        }
        entry.remainingMs = std::max(0.0, entry.remainingMs - elapsedMs);
        if (entry.remainingMs <= 0.0) {
            it = channels_.erase(it);
        } else {
            ++it;
        }
    }
}

void HudChannelTtlScheduler::Clear() {
    channels_.clear();
}

std::optional<TimedPresentationEntry> HudChannelTtlScheduler::Active(const std::string_view channel) const {
    const std::string key(channel);
    const auto it = channels_.find(key);
    if (it == channels_.end()) {
        return std::nullopt;
    }
    return it->second;
}

} // namespace ri::world
