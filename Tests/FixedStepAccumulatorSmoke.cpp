#include "RawIron/Core/FixedStepAccumulator.h"

#include <cstdlib>

namespace {

bool Expect(bool condition) {
    return condition;
}

} // namespace

int main() {
    ri::core::FixedStepConfig config{};
    config.fixedDeltaSeconds = 1.0 / 60.0;
    config.maxCatchUpSteps = 1;
    config.maxFrameDeltaSeconds = 1.0 / 120.0;

    ri::core::FixedStepAccumulator accumulator(config);
    const ri::core::FixedStepAdvanceResult result = accumulator.Advance(1.0 / 60.0);

    if (!Expect(result.stepCount == 1U)) {
        return EXIT_FAILURE;
    }
    if (!Expect(result.clampedFrameDeltaSeconds >= config.fixedDeltaSeconds)) {
        return EXIT_FAILURE;
    }
    if (!Expect(!result.frameDeltaClamped)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
