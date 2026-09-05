#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace ri::xr {
enum class HapticEvent { None, Selection, Grab, Contact };
struct HapticPulse { float amplitude = 0; float durationSeconds = 0; };

// Central output safety gate. Hover/render state has no qualified event. No queued pulse
// survives tracking/focus loss; a rejected request cannot consume another hand's budget.
class HapticPolicy {
public:
    HapticPulse Request(std::size_t hand, HapticEvent event, bool focused, bool tracked,
        double nowSeconds, float amplitude, float durationSeconds) {
        if (hand >= next_.size() || !focused || !tracked
            || (event != HapticEvent::Selection && event != HapticEvent::Grab && event != HapticEvent::Contact)
            || !std::isfinite(nowSeconds) || nowSeconds < 0 || !std::isfinite(amplitude)
            || !std::isfinite(durationSeconds) || amplitude <= 0 || durationSeconds <= 0
            || nowSeconds < next_[hand]) return {};
        next_[hand] = nowSeconds + 0.1; // At most 10 short event pulses per hand per second.
        return {std::min(amplitude, 0.35f), std::min(durationSeconds, 0.05f)};
    }
private:
    std::array<double, 2> next_{};
};
} // namespace ri::xr
