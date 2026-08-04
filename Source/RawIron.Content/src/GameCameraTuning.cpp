#include "RawIron/Content/GameCameraTuning.h"

namespace ri::content {
namespace {

[[nodiscard]] float ScalarPrefer(const ScriptScalarMap& preferred,
                                 const ScriptScalarMap& fallback,
                                 const char* key,
                                 const float current,
                                 const float minValue,
                                 const float maxValue) {
    const float fromFallback = ScriptScalarOrClamped(fallback, key, current, minValue, maxValue);
    return ScriptScalarOrClamped(preferred, key, fromFallback, minValue, maxValue);
}

} // namespace

GameCameraTuningScalars LoadGameCameraTuningScalars(const ScriptScalarMap& gameplay,
                                                    const ScriptScalarMap& rendering,
                                                    const ScriptScalarMap& postprocess,
                                                    const GameCameraTuningScalars& defaults) {
    GameCameraTuningScalars tuning = defaults;
    tuning.bobAmplitude =
        ScriptScalarOrClamped(gameplay, "head_bob_amplitude", tuning.bobAmplitude, 0.0f, 0.2f);
    tuning.bobFrequencyHz =
        ScriptScalarOrClamped(gameplay, "head_bob_frequency", tuning.bobFrequencyHz, 0.1f, 6.0f);
    tuning.bobSprintScale =
        ScriptScalarOrClamped(gameplay, "head_bob_sprint_scale", tuning.bobSprintScale, 1.0f, 3.0f);
    // Games historically used either camera_height or camera_base_height.
    tuning.cameraBaseHeight =
        ScriptScalarOrClamped(gameplay, "camera_height", tuning.cameraBaseHeight, 0.5f, 3.0f);
    tuning.cameraBaseHeight =
        ScriptScalarOrClamped(gameplay, "camera_base_height", tuning.cameraBaseHeight, 0.5f, 3.0f);
    // Prefer postprocess when present, else rendering (matches prior game load order).
    tuning.fovBaseDegrees =
        ScalarPrefer(postprocess, rendering, "fov_base", tuning.fovBaseDegrees, 45.0f, 120.0f);
    tuning.fovSprintAddDegrees =
        ScalarPrefer(postprocess, rendering, "fov_sprint_add", tuning.fovSprintAddDegrees, 0.0f, 25.0f);
    tuning.fovLerpPerSecond =
        ScalarPrefer(postprocess, rendering, "fov_lerp_per_second", tuning.fovLerpPerSecond, 0.5f, 40.0f);
    return tuning;
}

} // namespace ri::content
