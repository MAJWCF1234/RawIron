#pragma once

namespace ri::trace {

/// First-person camera feel sampled from gameplay/rendering riscript.
/// Games load via Content and pass this into SampleFirstPersonView — they do not own bob/FOV math.
struct FirstPersonCameraFeel {
    float bobAmplitude = 0.014f;
    float bobFrequencyHz = 1.35f;
    float bobSprintScale = 1.35f;
    float fovBaseDegrees = 78.0f;
    float fovSprintAddDegrees = 6.0f;
    float fovLerpPerSecond = 10.0f;
    float cameraBaseHeight = 1.62f;
};

struct FirstPersonViewSample {
    float eyeHeightAboveFeet = 1.62f;
    float fovDegrees = 78.0f;
};

/// Compute bobbed eye height and lerped FOV for one frame.
/// `fovPulseExtraDegrees` is an optional game/showcase overlay on top of the shared feel.
[[nodiscard]] FirstPersonViewSample SampleFirstPersonView(const FirstPersonCameraFeel& feel,
                                                          float elapsedSeconds,
                                                          float planarSpeed,
                                                          float sprintSpeedReference,
                                                          bool sprintHeld,
                                                          float& currentFovDegrees,
                                                          float deltaSeconds,
                                                          float fovPulseExtraDegrees = 0.0f);

} // namespace ri::trace
