#include "RawIron/Trace/FirstPersonCameraFeel.h"

#include <algorithm>
#include <cmath>

namespace ri::trace {

FirstPersonViewSample SampleFirstPersonView(const FirstPersonCameraFeel& feel,
                                            const float elapsedSeconds,
                                            const float planarSpeed,
                                            const float sprintSpeedReference,
                                            const bool sprintHeld,
                                            float& currentFovDegrees,
                                            const float deltaSeconds,
                                            const float fovPulseExtraDegrees) {
    const float sprintRef = std::max(0.01f, sprintSpeedReference);
    const float movementNorm = std::clamp(planarSpeed / sprintRef, 0.0f, 1.0f);
    const float bobScale = (sprintHeld ? feel.bobSprintScale : 1.0f) * movementNorm;
    const float bobPhase = static_cast<float>((elapsedSeconds * feel.bobFrequencyHz) * 6.283185307179586);
    const float bobVertical = std::sin(bobPhase) * feel.bobAmplitude * bobScale;

    const float targetFov = feel.fovBaseDegrees + (sprintHeld ? feel.fovSprintAddDegrees : 0.0f)
        + fovPulseExtraDegrees;
    const float blendAlpha = std::clamp(deltaSeconds * feel.fovLerpPerSecond, 0.0f, 1.0f);
    currentFovDegrees += (targetFov - currentFovDegrees) * blendAlpha;

    return FirstPersonViewSample{
        .eyeHeightAboveFeet = feel.cameraBaseHeight + bobVertical,
        .fovDegrees = currentFovDegrees,
    };
}

} // namespace ri::trace
