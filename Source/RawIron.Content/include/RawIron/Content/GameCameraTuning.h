#pragma once

#include "RawIron/Content/ScriptScalars.h"

namespace ri::content {

/// First-person camera feel scalars loaded from gameplay + rendering riscript.
/// Applied by `ri::trace::SampleFirstPersonView` — Content only owns the load contract.
struct GameCameraTuningScalars {
    float bobAmplitude = 0.014f;
    float bobFrequencyHz = 1.35f;
    float bobSprintScale = 1.35f;
    float fovBaseDegrees = 78.0f;
    float fovSprintAddDegrees = 6.0f;
    float fovLerpPerSecond = 10.0f;
    float cameraBaseHeight = 1.62f;
};

[[nodiscard]] GameCameraTuningScalars LoadGameCameraTuningScalars(
    const ScriptScalarMap& gameplay,
    const ScriptScalarMap& rendering,
    const ScriptScalarMap& postprocess = {},
    const GameCameraTuningScalars& defaults = {});

} // namespace ri::content
