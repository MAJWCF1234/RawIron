#include "RawIron/Core/FixedStepAccumulator.h"

#include <algorithm>
#include <cmath>

namespace ri::core {

FixedStepAccumulator::FixedStepAccumulator(FixedStepConfig config)
    : config_(config) {
    if (!std::isfinite(config_.fixedDeltaSeconds) || config_.fixedDeltaSeconds <= 0.0) {
        config_.fixedDeltaSeconds = 1.0 / 60.0;
    }
    if (config_.maxCatchUpSteps == 0U) {
        config_.maxCatchUpSteps = 1U;
    }
    if (!std::isfinite(config_.maxFrameDeltaSeconds) || config_.maxFrameDeltaSeconds <= 0.0) {
        config_.maxFrameDeltaSeconds = config_.fixedDeltaSeconds * static_cast<double>(config_.maxCatchUpSteps);
    }
}

void FixedStepAccumulator::Reset(double simulationTimeSeconds) noexcept {
    simulationTimeSeconds_ =
        std::isfinite(simulationTimeSeconds) ? std::max(0.0, simulationTimeSeconds) : 0.0;
    accumulatorSeconds_ = 0.0;
}

FixedStepAdvanceResult FixedStepAccumulator::Advance(double realDeltaSeconds) noexcept {
    const double inputDelta =
        std::isfinite(realDeltaSeconds) ? std::max(0.0, realDeltaSeconds) : 0.0;
    const double clamped = std::min(inputDelta, config_.maxFrameDeltaSeconds);
    const bool wasClamped = inputDelta > config_.maxFrameDeltaSeconds;

    accumulatorSeconds_ += clamped;

    std::size_t steps = 0;
    while (accumulatorSeconds_ >= config_.fixedDeltaSeconds && steps < config_.maxCatchUpSteps) {
        accumulatorSeconds_ -= config_.fixedDeltaSeconds;
        simulationTimeSeconds_ += config_.fixedDeltaSeconds;
        ++steps;
    }

    if (steps == config_.maxCatchUpSteps && accumulatorSeconds_ >= config_.fixedDeltaSeconds) {
        accumulatorSeconds_ = std::fmod(accumulatorSeconds_, config_.fixedDeltaSeconds);
    }

    const double alpha = config_.fixedDeltaSeconds > 0.0
        ? std::clamp(accumulatorSeconds_ / config_.fixedDeltaSeconds, 0.0, 1.0)
        : 0.0;

    return FixedStepAdvanceResult{
        .stepCount = steps,
        .stepDeltaSeconds = config_.fixedDeltaSeconds,
        .interpolationAlpha = alpha,
        .clampedFrameDeltaSeconds = clamped,
        .frameDeltaClamped = wasClamped,
    };
}

double FixedStepAccumulator::SimulationTimeSeconds() const noexcept {
    return simulationTimeSeconds_;
}

double FixedStepAccumulator::AccumulatorSeconds() const noexcept {
    return accumulatorSeconds_;
}

} // namespace ri::core
