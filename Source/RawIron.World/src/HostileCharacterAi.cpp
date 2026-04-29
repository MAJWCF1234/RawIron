#include "RawIron/World/HostileCharacterAi.h"

#include "RawIron/Math/Angles.h"

#include <algorithm>
#include <cmath>

namespace ri::world {
namespace {

constexpr float kEpsilon = 0.0001f;

float HorizontalDistance(const ri::math::Vec3& a, const ri::math::Vec3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt((dx * dx) + (dz * dz));
}

ri::math::Vec3 HorizontalDirection(const ri::math::Vec3& from, const ri::math::Vec3& to) {
    ri::math::Vec3 d{to.x - from.x, 0.0f, to.z - from.z};
    return ri::math::Normalize(d);
}

float ClampFinite(float value, float fallback, float minV, float maxV) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, minV, maxV);
}

} // namespace

void HostileCharacterAi::Configure(const HostileCharacterAiDefinition& definition) {
    def_ = definition;
    SanitizeDefinition();
    ResetRuntimeState();
}

const HostileCharacterAiDefinition& HostileCharacterAi::Definition() const {
    return def_;
}

void HostileCharacterAi::SanitizeDefinition() {
    def_.patrolSpeed = ClampFinite(def_.patrolSpeed, 1.4f, 0.05f, 48.0f);
    def_.chaseSpeed = ClampFinite(def_.chaseSpeed, 3.2f, 0.05f, 48.0f);
    def_.alertMoveSpeed = ClampFinite(def_.alertMoveSpeed, 2.0f, 0.05f, 48.0f);
    def_.returnSpeed = ClampFinite(def_.returnSpeed, 1.6f, 0.05f, 48.0f);
    def_.lightRetreatSpeed = ClampFinite(def_.lightRetreatSpeed, 2.4f, 0.05f, 48.0f);
    def_.lightRetreatStandoffDistance =
        ClampFinite(def_.lightRetreatStandoffDistance, 2.0f, 0.25f, 80.0f);
    def_.turnSpeedRadiansPerSecond = ClampFinite(def_.turnSpeedRadiansPerSecond, 5.0f, 0.1f, 48.0f);
    def_.patrolTurnSpeedRadiansPerSecond =
        ClampFinite(def_.patrolTurnSpeedRadiansPerSecond, 2.5f, 0.05f, 48.0f);
    def_.attackTurnSpeedMultiplier = ClampFinite(def_.attackTurnSpeedMultiplier, 2.5f, 1.0f, 10.0f);
    def_.patrolMinMoveAlignment = ClampFinite(def_.patrolMinMoveAlignment, 0.22f, 0.0f, 1.0f);
    def_.chaseMinMoveAlignment = ClampFinite(def_.chaseMinMoveAlignment, 0.38f, 0.0f, 1.0f);
    def_.playerDetectionDistance = ClampFinite(def_.playerDetectionDistance, 18.0f, 0.25f, 2000.0f);
    def_.visionHalfAngleRadians = ClampFinite(def_.visionHalfAngleRadians, 0.65f, 0.05f, 1.4f);
    def_.attackDistance = ClampFinite(def_.attackDistance, 1.35f, 0.05f, 50.0f);
    if (!std::isfinite(def_.attackCooldownSeconds) || def_.attackCooldownSeconds < 0.0) {
        def_.attackCooldownSeconds = 2.5;
    }
    def_.attackCooldownSeconds = std::min(def_.attackCooldownSeconds, 120.0);
    if (!std::isfinite(def_.attackWindupSeconds) || def_.attackWindupSeconds < 0.0) {
        def_.attackWindupSeconds = 0.45;
    }
    def_.attackWindupSeconds = std::min(def_.attackWindupSeconds, 30.0);
    if (!std::isfinite(def_.patrolWaitSeconds) || def_.patrolWaitSeconds < 0.0) {
        def_.patrolWaitSeconds = 0.0;
    }
    def_.patrolWaitSeconds = std::min(def_.patrolWaitSeconds, 600.0);
    if (!std::isfinite(def_.alertDurationSeconds) || def_.alertDurationSeconds < 0.0) {
        def_.alertDurationSeconds = 0.85;
    }
    def_.alertDurationSeconds = std::min(def_.alertDurationSeconds, 120.0);
    if (!std::isfinite(def_.chaseForgetSeconds) || def_.chaseForgetSeconds < 0.0) {
        def_.chaseForgetSeconds = 2.5;
    }
    def_.chaseForgetSeconds = std::min(def_.chaseForgetSeconds, 600.0);
    if (!std::isfinite(def_.leashDistance)) {
        def_.leashDistance = -1.0f;
    }
    def_.waypointArrivalRadius = ClampFinite(def_.waypointArrivalRadius, 0.55f, 0.05f, 20.0f);
    def_.homeArrivalRadius = ClampFinite(def_.homeArrivalRadius, 0.75f, 0.05f, 40.0f);
    if (!std::isfinite(def_.lightAversionStrongHoldSeconds) || def_.lightAversionStrongHoldSeconds < 0.0) {
        def_.lightAversionStrongHoldSeconds = 0.55;
    }
    if (!std::isfinite(def_.lightAversionWeakHoldSeconds) || def_.lightAversionWeakHoldSeconds < 0.0) {
        def_.lightAversionWeakHoldSeconds = 0.28;
    }
    if (!std::isfinite(def_.lightAversionDecayPerSecond) || def_.lightAversionDecayPerSecond < 0.0) {
        def_.lightAversionDecayPerSecond = 1.2;
    }
    if (!std::isfinite(def_.safeZoneAttackCooldownSeconds) || def_.safeZoneAttackCooldownSeconds < 0.0) {
        def_.safeZoneAttackCooldownSeconds = 1.1;
    }
    if (def_.homePosition.x == 0.0f && def_.homePosition.y == 0.0f && def_.homePosition.z == 0.0f
        && !def_.patrolPath.empty()) {
        def_.homePosition = def_.patrolPath.front();
    }
    const int phaseOrdinal = static_cast<int>(def_.initialPhase);
    if (phaseOrdinal < 0 || phaseOrdinal > static_cast<int>(HostileCharacterAiPhase::AttackCommit)) {
        def_.initialPhase = HostileCharacterAiPhase::Patrol;
    }
    if (def_.yawTurnMath != ri::math::YawShortestDeltaMode::IeeeRemainder
        && def_.yawTurnMath != ri::math::YawShortestDeltaMode::LegacyIterativePi) {
        def_.yawTurnMath = ri::math::YawShortestDeltaMode::IeeeRemainder;
    }
    if (def_.animIdle.empty()) {
        def_.animIdle = "idle";
    }
    if (def_.animWalk.empty()) {
        def_.animWalk = "walk";
    }
    if (def_.animRun.empty()) {
        def_.animRun = "run";
    }
    if (def_.animAlert.empty()) {
        def_.animAlert = "alert";
    }
    if (def_.animAttack.empty()) {
        def_.animAttack = "attack";
    }
    if (def_.animAttackCommit.empty()) {
        def_.animAttackCommit = def_.animAttack;
    }
}

void HostileCharacterAi::ResetRuntimeState() {
    phase_ = def_.initialPhase;
    forcedPhaseActive_ = false;
    forceAlertSilent_ = false;
    lightAversionTimer_ = 0.0;
    alertEnteredAt_ = -1.0;
    patrolPausedUntil_ = -1.0;
    nextAttackAllowedAt_ = -1.0;
    attackWindupEndsAt_ = -1.0;
    lastKnownPlayer_ = {};
    lastSeenPlayerTime_ = -1.0;
    patrolIndex_ = 0U;
    patrolDirection_ = 1;
    issuedAwarenessMessage_ = false;
}

void HostileCharacterAi::ForceAlert(ri::math::Vec3 lastKnownPlayerPosition, bool silent) {
    phase_ = HostileCharacterAiPhase::Alert;
    alertEnteredAt_ = -1.0;
    lastKnownPlayer_ = lastKnownPlayerPosition;
    lastSeenPlayerTime_ = -1.0;
    forceAlertSilent_ = silent;
    forcedPhaseActive_ = false;
}

void HostileCharacterAi::ForcePhase(HostileCharacterAiPhase phase) {
    forcedPhaseActive_ = true;
    forcedPhase_ = phase;
    phase_ = phase;
}

void HostileCharacterAi::ClearForcedPhase() {
    forcedPhaseActive_ = false;
}

void HostileCharacterAi::ApplyFalseSignal(ri::math::Vec3 worldPoint, float /*radiusHint*/) {
    ForceAlert(worldPoint, true);
}

HostileCharacterAiPhase HostileCharacterAi::CurrentPhase() const {
    return phase_;
}

bool HostileCharacterAi::HasForcedPhase() const {
    return forcedPhaseActive_;
}

ri::math::Vec3 HostileCharacterAi::LastKnownPlayerPosition() const {
    return lastKnownPlayer_;
}

void HostileCharacterAi::UpdateLightAversion(const HostileCharacterAiFrameInput& input) {
    const double dt = static_cast<double>(std::max(0.0f, input.deltaSeconds));
    double targetHold = 0.0;
    if (input.playerFlashlightActive && input.flashlightIlluminatesCharacter) {
        targetHold = std::max(targetHold, def_.lightAversionStrongHoldSeconds);
    }
    if (input.playerInSafeLight || input.characterInSafeLight) {
        targetHold = std::max(targetHold, def_.lightAversionWeakHoldSeconds);
    }
    if (targetHold > 0.0) {
        lightAversionTimer_ = std::max(lightAversionTimer_, targetHold);
    } else if (dt > 0.0) {
        lightAversionTimer_ = std::max(0.0, lightAversionTimer_ - def_.lightAversionDecayPerSecond * dt);
    }
}

bool HostileCharacterAi::IsRepelledByLight() const {
    return lightAversionTimer_ > 0.0001;
}

bool HostileCharacterAi::LeashExceeded(const ri::math::Vec3& characterPosition) const {
    if (!std::isfinite(def_.leashDistance) || def_.leashDistance < 0.0f) {
        return false;
    }
    return HorizontalDistance(characterPosition, def_.homePosition) > def_.leashDistance;
}

void HostileCharacterAi::HandlePlayerSafeZone(double nowSeconds) {
    attackWindupEndsAt_ = -1.0;
    if (nextAttackAllowedAt_ < nowSeconds + def_.safeZoneAttackCooldownSeconds) {
        nextAttackAllowedAt_ = nowSeconds + def_.safeZoneAttackCooldownSeconds;
    }
    phase_ = HostileCharacterAiPhase::Return;
}

void HostileCharacterAi::RunStateMachine(const HostileCharacterAiFrameInput& input, HostileCharacterAiFrameOutput& out) {
    const double now = input.timeSeconds;
    const bool repelled = IsRepelledByLight();
    const bool visible = input.playerVisible && def_.enabled;
    const bool leash = LeashExceeded(input.characterPosition);

    if (forcedPhaseActive_) {
        phase_ = forcedPhase_;
    } else if (input.playerInSafeZone) {
        HandlePlayerSafeZone(now);
    }

    if (visible) {
        lastKnownPlayer_ = input.playerPosition;
        lastSeenPlayerTime_ = now;
    }

    if (phase_ == HostileCharacterAiPhase::AttackCommit) {
        if (input.playerInSafeZone || repelled) {
            attackWindupEndsAt_ = -1.0;
            phase_ = HostileCharacterAiPhase::Return;
        } else if (now >= attackWindupEndsAt_ && attackWindupEndsAt_ > 0.0) {
            out.hostMeleeHitResolved = true;
            attackWindupEndsAt_ = -1.0;
            nextAttackAllowedAt_ = now + def_.attackCooldownSeconds;
            phase_ = HostileCharacterAiPhase::Return;
        }
        return;
    }

    if (forcedPhaseActive_) {
        return;
    }

    switch (phase_) {
    case HostileCharacterAiPhase::Patrol: {
        if (visible && !repelled) {
            phase_ = HostileCharacterAiPhase::Alert;
            alertEnteredAt_ = -1.0;
            if (!forceAlertSilent_ && !issuedAwarenessMessage_) {
                out.showFirstAwarenessMessage = true;
                issuedAwarenessMessage_ = true;
            }
            forceAlertSilent_ = false;
        }
        break;
    }
    case HostileCharacterAiPhase::Alert: {
        if (input.playerInSafeZone || repelled || leash) {
            phase_ = HostileCharacterAiPhase::Return;
            break;
        }
        if (!visible && alertEnteredAt_ >= 0.0 && (now - alertEnteredAt_) > def_.alertDurationSeconds) {
            phase_ = HostileCharacterAiPhase::Return;
            break;
        }
        if (alertEnteredAt_ >= 0.0 && (now - alertEnteredAt_) >= def_.alertDurationSeconds) {
            if (visible && !repelled) {
                phase_ = HostileCharacterAiPhase::Chase;
            } else {
                phase_ = HostileCharacterAiPhase::Return;
            }
        }
        break;
    }
    case HostileCharacterAiPhase::Chase: {
        if (input.playerInSafeZone || repelled || leash) {
            phase_ = HostileCharacterAiPhase::Return;
            break;
        }
        const bool lostTooLong =
            !visible && lastSeenPlayerTime_ >= 0.0 && (now - lastSeenPlayerTime_) > def_.chaseForgetSeconds;
        if (lostTooLong) {
            phase_ = HostileCharacterAiPhase::Return;
            break;
        }
        const float dist = HorizontalDistance(input.characterPosition, input.playerPosition);
        const bool attackReady = nextAttackAllowedAt_ < 0.0 || now >= nextAttackAllowedAt_;
        if (visible && !repelled && dist <= def_.attackDistance && attackReady) {
            phase_ = HostileCharacterAiPhase::AttackCommit;
            attackWindupEndsAt_ = now + def_.attackWindupSeconds;
        }
        break;
    }
    case HostileCharacterAiPhase::Return: {
        if (visible && !repelled && !input.playerInSafeZone) {
            phase_ = HostileCharacterAiPhase::Alert;
            alertEnteredAt_ = -1.0;
        } else if (!input.playerInSafeZone
                   && HorizontalDistance(input.characterPosition, def_.homePosition) <= def_.homeArrivalRadius) {
            phase_ = HostileCharacterAiPhase::Patrol;
            patrolPausedUntil_ = now + def_.patrolWaitSeconds;
        }
        break;
    }
    default:
        break;
    }

    if (phase_ == HostileCharacterAiPhase::Alert && alertEnteredAt_ < 0.0) {
        alertEnteredAt_ = now;
    }
}

void HostileCharacterAi::ComputeLocomotion(const HostileCharacterAiFrameInput& input,
                                           HostileCharacterAiFrameOutput& out,
                                           const ri::math::Vec3& moveTarget,
                                           float moveSpeed,
                                           float turnSpeed,
                                           float minAlignment,
                                           bool useRunAnim) {
    const ri::math::Vec3 toTarget = HorizontalDirection(input.characterPosition, moveTarget);
    const float dist = HorizontalDistance(input.characterPosition, moveTarget);
    if (dist <= kEpsilon) {
        out.animationAction = useRunAnim ? def_.animRun : def_.animWalk;
        return;
    }

    const float desiredYaw = std::atan2(toTarget.x, toTarget.z);
    const float currentYaw = input.characterYawRadians;
    const float maxTurn = turnSpeed * std::max(0.0f, input.deltaSeconds);
    const ri::math::YawStepResult yawStep =
        ri::math::StepYawToward(currentYaw, desiredYaw, maxTurn, def_.yawTurnMath);
    out.yawDeltaThisFrame = yawStep.newYaw - currentYaw;

    const float align = std::clamp(yawStep.alignment, minAlignment, 1.0f);
    const float step = moveSpeed * std::max(0.0f, input.deltaSeconds) * align;
    out.horizontalDisplacementRequest = toTarget * std::min(step, dist);
    out.animationAction = useRunAnim ? def_.animRun : def_.animWalk;
}

HostileCharacterAiFrameOutput HostileCharacterAi::Advance(const HostileCharacterAiFrameInput& input) {
    HostileCharacterAiFrameOutput out{};
    out.phase = phase_;

    if (!def_.enabled) {
        out.animationAction = def_.animIdle;
        return out;
    }

    UpdateLightAversion(input);
    RunStateMachine(input, out);
    out.phase = phase_;

    out.chaseLayerActive =
        phase_ == HostileCharacterAiPhase::Alert || phase_ == HostileCharacterAiPhase::Chase;

    const bool repelled = IsRepelledByLight();

    if (phase_ == HostileCharacterAiPhase::AttackCommit) {
        const ri::math::Vec3 toPlayer = HorizontalDirection(input.characterPosition, input.playerPosition);
        const float desiredYaw = std::atan2(toPlayer.x, toPlayer.z);
        const float currentYaw = input.characterYawRadians;
        const float maxTurn =
            def_.turnSpeedRadiansPerSecond * def_.attackTurnSpeedMultiplier * std::max(0.0f, input.deltaSeconds);
        const ri::math::YawStepResult yawStep =
            ri::math::StepYawToward(currentYaw, desiredYaw, maxTurn, def_.yawTurnMath);
        out.yawDeltaThisFrame = yawStep.newYaw - currentYaw;
        out.horizontalDisplacementRequest = {};
        if (!def_.animAttackCommit.empty()) {
            out.animationAction = def_.animAttackCommit;
        } else if (!def_.animAttack.empty()) {
            out.animationAction = def_.animAttack;
        } else {
            out.animationAction = def_.animRun;
        }
        return out;
    }

    if (phase_ == HostileCharacterAiPhase::Patrol) {
        if (patrolPausedUntil_ > input.timeSeconds) {
            out.animationAction = def_.animIdle;
            return out;
        }
        if (!def_.patrolPath.empty()) {
            std::size_t hops = 0U;
            while (hops < def_.patrolPath.size()) {
                const ri::math::Vec3 wpTarget = def_.patrolPath[patrolIndex_];
                const float wpDist = HorizontalDistance(input.characterPosition, wpTarget);
                if (wpDist > def_.waypointArrivalRadius) {
                    break;
                }
                ++hops;
                patrolPausedUntil_ = input.timeSeconds + def_.patrolWaitSeconds;
                if (def_.patrolPath.size() <= 1U) {
                    out.animationAction = def_.animIdle;
                    return out;
                }
                if (def_.patrolWrap == HostilePatrolWrapMode::Loop) {
                    patrolIndex_ = (patrolIndex_ + 1U) % def_.patrolPath.size();
                } else {
                    const std::size_t last = def_.patrolPath.size() - 1U;
                    if (patrolIndex_ == last && patrolDirection_ > 0) {
                        patrolDirection_ = -1;
                    } else if (patrolIndex_ == 0U && patrolDirection_ < 0) {
                        patrolDirection_ = 1;
                    }
                    patrolIndex_ =
                        static_cast<std::size_t>(static_cast<int>(patrolIndex_) + patrolDirection_);
                }
                if (def_.patrolWaitSeconds > 0.0) {
                    out.animationAction = def_.animIdle;
                    return out;
                }
            }
        }
        ri::math::Vec3 target = def_.homePosition;
        if (!def_.patrolPath.empty()) {
            target = def_.patrolPath[patrolIndex_];
        }
        const float dist = HorizontalDistance(input.characterPosition, target);
        if (dist <= def_.waypointArrivalRadius) {
            out.animationAction = def_.animIdle;
            return out;
        }
        ComputeLocomotion(
            input,
            out,
            target,
            def_.patrolSpeed,
            def_.patrolTurnSpeedRadiansPerSecond,
            def_.patrolMinMoveAlignment,
            false);
        return out;
    }

    if (phase_ == HostileCharacterAiPhase::Alert) {
        ComputeLocomotion(
            input,
            out,
            lastKnownPlayer_,
            def_.alertMoveSpeed,
            def_.turnSpeedRadiansPerSecond,
            def_.chaseMinMoveAlignment,
            false);
        out.animationAction = def_.animAlert;
        return out;
    }

    if (phase_ == HostileCharacterAiPhase::Chase) {
        ri::math::Vec3 focus = lastKnownPlayer_;
        if (repelled) {
            ri::math::Vec3 away = input.characterPosition - input.playerPosition;
            away.y = 0.0f;
            focus = input.characterPosition
                    + ri::math::Normalize(away) * def_.lightRetreatStandoffDistance;
        }
        ComputeLocomotion(input,
                          out,
                          focus,
                          repelled ? def_.lightRetreatSpeed : def_.chaseSpeed,
                          def_.turnSpeedRadiansPerSecond,
                          def_.chaseMinMoveAlignment,
                          !repelled);
        return out;
    }

    if (phase_ == HostileCharacterAiPhase::Return) {
        ComputeLocomotion(input,
                          out,
                          def_.homePosition,
                          def_.returnSpeed,
                          def_.turnSpeedRadiansPerSecond,
                          def_.patrolMinMoveAlignment,
                          false);
        return out;
    }

    out.animationAction = def_.animIdle;
    return out;
}

} // namespace ri::world
