#pragma once

#include "RawIron/Core/FixedStepAccumulator.h"

namespace ri::core {

/// One frame of a game-style clock: wall time plus fixed-step simulation advances.
struct GameSimulationTick {
    double realtimeSeconds = 0.0;
    double realDeltaSeconds = 0.0;
    FixedStepAdvanceResult fixed{};
};

/// Combines wall-clock progression with `FixedStepAccumulator` for standard game loops:
/// measure `realDeltaSeconds`, run `fixed.stepCount` simulation ticks at `fixed.stepDeltaSeconds`,
/// then render using `fixed.interpolationAlpha` between the last two simulation states.
class GameSimulationClock {
public:
    explicit GameSimulationClock(FixedStepConfig config = {});

    void Reset(double simulationTimeSeconds = 0.0) noexcept;

    /// Advance by a measured frame delta (non-negative). Updates realtime and fixed-step state.
    [[nodiscard]] GameSimulationTick Tick(double realDeltaSeconds) noexcept;

    [[nodiscard]] double RealtimeSeconds() const noexcept {
        return realtimeSeconds_;
    }

    [[nodiscard]] double SimulationTimeSeconds() const noexcept {
        return accumulator_.SimulationTimeSeconds();
    }

    [[nodiscard]] const FixedStepAccumulator& Accumulator() const noexcept {
        return accumulator_;
    }

private:
    FixedStepAccumulator accumulator_;
    double realtimeSeconds_ = 0.0;
};

} // namespace ri::core
