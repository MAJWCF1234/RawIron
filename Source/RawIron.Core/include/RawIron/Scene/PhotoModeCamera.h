#pragma once

#include "RawIron/Math/Mat4.h"

#include <algorithm>
#include <cmath>

namespace ri::scene {

/// Optional projection overrides for still-frame / capture style rendering without mutating scene `Camera` assets.
struct PhotoModeCameraOverrides {
    bool enabled = false;
    /// When > 0 and `enabled`, replaces authored vertical FOV (degrees) for this submission only, after optional
    /// `fieldOfViewOverrideIsHorizontal` conversion using `aspectRatio` passed to `ResolvePhotoModeFieldOfViewDegrees`.
    float fieldOfViewDegreesOverride = 0.0f;
    /// When `fieldOfViewDegreesOverride` is 0 and `enabled`, multiply authored vertical FOV (1 = unchanged).
    float fieldOfViewScale = 1.0f;
    /// When true with `fieldOfViewDegreesOverride > 0`, the override is horizontal FOV (degrees).
    bool fieldOfViewOverrideIsHorizontal = false;
};

/// True when photo mode is on and will change vertical FOV vs the authored camera (scale ≠ 1 or positive override).
[[nodiscard]] inline bool PhotoModeFieldOfViewActive(const PhotoModeCameraOverrides& photo) noexcept {
    if (!photo.enabled) {
        return false;
    }
    if (photo.fieldOfViewDegreesOverride > 0.0f) {
        return true;
    }
    return std::abs(photo.fieldOfViewScale - 1.0f) > 0.0005f;
}

/// Resolves vertical field of view (degrees) used for perspective projection. `aspectRatio` is width / height;
/// required when applying a horizontal override (`fieldOfViewOverrideIsHorizontal` with override > 0).
[[nodiscard]] inline float ResolvePhotoModeFieldOfViewDegrees(float authoredFieldOfViewDegrees,
                                                              const PhotoModeCameraOverrides& photo,
                                                              float aspectRatio = 1.0f) {
    if (!photo.enabled) {
        return authoredFieldOfViewDegrees;
    }
    if (photo.fieldOfViewDegreesOverride > 0.0f) {
        float verticalDegrees = photo.fieldOfViewDegreesOverride;
        if (photo.fieldOfViewOverrideIsHorizontal) {
            verticalDegrees = std::clamp(verticalDegrees, 1.0f, 160.0f);
            const float safeAspect = std::max(aspectRatio, 0.001f);
            const float halfHorizontalRadians = ri::math::DegreesToRadians(verticalDegrees * 0.5f);
            verticalDegrees =
                ri::math::RadiansToDegrees(2.0f * std::atan(std::tan(halfHorizontalRadians) / safeAspect));
        }
        return std::clamp(verticalDegrees, 1.0f, 160.0f);
    }
    const float scale = std::max(photo.fieldOfViewScale, 0.01f);
    return std::clamp(authoredFieldOfViewDegrees * scale, 1.0f, 160.0f);
}

} // namespace ri::scene
