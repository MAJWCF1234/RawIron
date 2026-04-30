#pragma once

#include "RawIron/Math/Vec3.h"
#include "RawIron/Trace/MovementController.h"

namespace ri::trace {

/// World-space eye heights relative to feet for common FPS stances (meters, Y-up).
struct CameraFeetRig {
    float eyeHeightStanding = 1.65f;
    float eyeHeightCrouching = 1.15f;
    float eyeHeightProne = 0.35f;
};

[[nodiscard]] inline float CameraEyeHeightForStance(const MovementStance stance,
                                                    const CameraFeetRig& rig = {}) noexcept {
    switch (stance) {
    case MovementStance::Crouching:
        return rig.eyeHeightCrouching;
    case MovementStance::Prone:
        return rig.eyeHeightProne;
    case MovementStance::Standing:
    default:
        return rig.eyeHeightStanding;
    }
}

/// When the camera (eye) transform is authoritative, recover feet on the ground plane at `worldUp`
/// so the character capsule can be re-rooted under cinematics, possession swaps, or editor fly modes.
[[nodiscard]] inline ri::math::Vec3 CameraFeetPositionFromEye(const ri::math::Vec3& eyeWorld,
                                                              const MovementStance stance,
                                                              const ri::math::Vec3& worldUp = {0.0f, 1.0f, 0.0f},
                                                              const CameraFeetRig& rig = {}) noexcept {
    const float h = CameraEyeHeightForStance(stance, rig);
    const ri::math::Vec3 up = ri::math::Normalize(worldUp);
    return eyeWorld - (up * h);
}

/// **Yaw-frame** feet solve: strips pitch/roll from `viewForwardWorld`, builds a horizontal basis
/// (right, up, forward), and subtracts `localOffset` where components are **right / up / forward**
/// in that yaw-only frame. Supports eye lead on XZ ("weapon forward"), lateral eye bias, and VR
/// neck offsets without coupling feet to camera pitch (common third-person / cinematic workflow).
[[nodiscard]] inline ri::math::Vec3 CameraFeetPositionFromEyeLocalOffset(
    const ri::math::Vec3& eyeWorld,
    const ri::math::Vec3& viewForwardWorld,
    const ri::math::Vec3& localOffsetRightUpForward,
    const ri::math::Vec3& worldUp = {0.0f, 1.0f, 0.0f}) noexcept {
    const ri::math::Vec3 up = ri::math::Normalize(worldUp);
    ri::math::Vec3 forward = viewForwardWorld;
    forward.y = 0.0f;
    if (ri::math::LengthSquared(forward) < 1e-12f) {
        forward = {0.0f, 0.0f, 1.0f};
    } else {
        forward = ri::math::Normalize(forward);
    }
    ri::math::Vec3 right = ri::math::Cross(up, forward);
    if (ri::math::LengthSquared(right) < 1e-12f) {
        right = {1.0f, 0.0f, 0.0f};
    } else {
        right = ri::math::Normalize(right);
    }
    forward = ri::math::Normalize(ri::math::Cross(right, up));
    return eyeWorld - (right * localOffsetRightUpForward.x) - (up * localOffsetRightUpForward.y)
        - (forward * localOffsetRightUpForward.z);
}

/// Convenience: stance eye height on Y, optional forward lead on XZ from `CameraFeetPositionFromEyeLocalOffset`.
[[nodiscard]] inline ri::math::Vec3 CameraFeetPositionFromEyeWithLead(const ri::math::Vec3& eyeWorld,
                                                                     const ri::math::Vec3& viewForwardWorld,
                                                                     const MovementStance stance,
                                                                     float forwardLead,
                                                                     const ri::math::Vec3& worldUp = {0.0f, 1.0f, 0.0f},
                                                                     const CameraFeetRig& rig = {}) noexcept {
    const float h = CameraEyeHeightForStance(stance, rig);
    return CameraFeetPositionFromEyeLocalOffset(
        eyeWorld,
        viewForwardWorld,
        {0.0f, h, forwardLead},
        worldUp);
}

} // namespace ri::trace
