#pragma once

#include <cstddef>

namespace ri::core {

struct FixedStepConfig {
    double fixedDeltaSeconds = 1.0 / 60.0;
    std::size_t maxCatchUpSteps = 4;
    double maxFrameDeltaSeconds = 0.25;
};

struct FixedStepAdvanceResult {
    std::size_t stepCount = 0;
    double stepDeltaSeconds = 0.0;
    double interpolationAlpha = 0.0;
    double clampedFrameDeltaSeconds = 0.0;
    bool frameDeltaClamped = false;
};

class FixedStepAccumulator {
public:
    explicit FixedStepAccumulator(FixedStepConfig config = {});

    void Reset(double simulationTimeSeconds = 0.0) noexcept;
    [[nodiscard]] FixedStepAdvanceResult Advance(double realDeltaSeconds) noexcept;

    [[nodiscard]] double SimulationTimeSeconds() const noexcept;
    [[nodiscard]] double AccumulatorSeconds() const noexcept;

private:
    FixedStepConfig config_{};
    double simulationTimeSeconds_ = 0.0;
    double accumulatorSeconds_ = 0.0;
};

} // namespace ri::core
