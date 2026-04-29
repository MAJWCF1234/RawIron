#include "RawIron/Core/GameSimulationClock.h"

#include <algorithm>
#include <cmath>

namespace ri::core {

GameSimulationClock::GameSimulationClock(FixedStepConfig config)
    : accumulator_(config) {}

void GameSimulationClock::Reset(double simulationTimeSeconds) noexcept {
    realtimeSeconds_ = 0.0;
    accumulator_.Reset(simulationTimeSeconds);
}

GameSimulationTick GameSimulationClock::Tick(double realDeltaSeconds) noexcept {
    const double clampedReal =
        std::isfinite(realDeltaSeconds) ? std::max(0.0, realDeltaSeconds) : 0.0;
    realtimeSeconds_ += clampedReal;

    GameSimulationTick out{};
    out.realtimeSeconds = realtimeSeconds_;
    out.realDeltaSeconds = clampedReal;
    out.fixed = accumulator_.Advance(clampedReal);
    return out;
}

} // namespace ri::core
