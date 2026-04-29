#pragma once

// Hostile phase machine + frame input/output contract — developer map: Documentation/02 Engine/NPC Behavior Support.md

#include "RawIron/Math/Angles.h"
#include "RawIron/Math/Vec3.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace ri::world {

/// High-level locomotion / awareness phase for an opponent / hostile-style character controller.
/// Visibility and LOS are supplied by the host; this module only consumes \ref HostileCharacterAiFrameInput flags.
enum class HostileCharacterAiPhase : std::uint8_t {
    Patrol = 0,
    Alert = 1,
    Chase = 2,
    Return = 3,
    /// Melee / strike commit: windup then host resolves hit reaction (\ref HostileCharacterAiFrameOutput::hostMeleeHitResolved).
    AttackCommit = 4,
};

/// Patrol path wrap when multiple waypoints are authored.
enum class HostilePatrolWrapMode : std::uint8_t {
    Loop = 0,
    PingPong = 1,
};

/// Authoring + tuning for one hostile instance. Host maps animation names to its playback controller.
struct HostileCharacterAiDefinition {
    std::string id;
    std::vector<ri::math::Vec3> patrolPath{};
    ri::math::Vec3 homePosition{};
    float patrolSpeed = 1.4f;
    float chaseSpeed = 3.2f;
    float alertMoveSpeed = 2.0f;
    float returnSpeed = 1.6f;
    float lightRetreatSpeed = 2.4f;
    /// When light-averse retreat runs in chase, aim for a point this far beyond the character along the away-from-player axis.
    float lightRetreatStandoffDistance = 2.0f;
    float turnSpeedRadiansPerSecond = 5.0f;
    float patrolTurnSpeedRadiansPerSecond = 2.5f;
    float attackTurnSpeedMultiplier = 2.5f;
    float patrolMinMoveAlignment = 0.22f;
    float chaseMinMoveAlignment = 0.38f;
    float playerDetectionDistance = 18.0f;
    float visionHalfAngleRadians = 0.65f;
    float attackDistance = 1.35f;
    double attackCooldownSeconds = 2.5;
    double attackWindupSeconds = 0.45;
    double patrolWaitSeconds = 0.0;
    double alertDurationSeconds = 0.85;
    double chaseForgetSeconds = 2.5;
    /// Negative or non-finite: leash disabled.
    float leashDistance = 24.0f;
    float waypointArrivalRadius = 0.55f;
    float homeArrivalRadius = 0.75f;
    double lightAversionStrongHoldSeconds = 0.55;
    double lightAversionWeakHoldSeconds = 0.28;
    double lightAversionDecayPerSecond = 1.2;
    double safeZoneAttackCooldownSeconds = 1.1;
    HostileCharacterAiPhase initialPhase = HostileCharacterAiPhase::Patrol;
    HostilePatrolWrapMode patrolWrap = HostilePatrolWrapMode::Loop;
    /// Shortest horizontal turn math (IEEE default; legacy loop optional for sandbox parity).
    ri::math::YawShortestDeltaMode yawTurnMath = ri::math::YawShortestDeltaMode::IeeeRemainder;
    std::string animIdle = "idle";
    std::string animWalk = "walk";
    std::string animRun = "run";
    std::string animAlert = "alert";
    std::string animAttack = "attack";
    /// Preferred commit clip (empty → sanitized to \ref animAttack in \ref Configure).
    std::string animAttackCommit{};
    bool enabled = true;
};

/// Per-frame facts computed by the host (traces, volumes, flashlight cone + LOS, etc.).
struct HostileCharacterAiFrameInput {
    double timeSeconds = 0.0;
    float deltaSeconds = 0.0f;
    ri::math::Vec3 characterPosition{};
    float characterYawRadians = 0.0f;
    ri::math::Vec3 playerPosition{};
    /// Combined gate: in detection range, inside vision cone, clear LOS to player.
    bool playerVisible = false;
    bool playerFlashlightActive = false;
    /// Host already combined cone + LOS from player light to this character.
    bool flashlightIlluminatesCharacter = false;
    bool playerInSafeZone = false;
    bool playerInSafeLight = false;
    bool characterInSafeLight = false;
};

/// Locomotion + presentation intent for the host to apply (sweep move, ground snap, anim cross-fade).
struct HostileCharacterAiFrameOutput {
    HostileCharacterAiPhase phase = HostileCharacterAiPhase::Patrol;
    ri::math::Vec3 horizontalDisplacementRequest{};
    float yawDeltaThisFrame = 0.0f;
    std::string animationAction;
    bool showFirstAwarenessMessage = false;
    bool chaseLayerActive = false;
    /// Fire host hit reaction (shake, VFX, invuln window, despawn policy, …) once when true.
    bool hostMeleeHitResolved = false;
};

class HostileCharacterAi {
public:
    void Configure(const HostileCharacterAiDefinition& definition);
    [[nodiscard]] const HostileCharacterAiDefinition& Definition() const;

    void ResetRuntimeState();
    [[nodiscard]] HostileCharacterAiFrameOutput Advance(const HostileCharacterAiFrameInput& input);

    void ForceAlert(ri::math::Vec3 lastKnownPlayerPosition, bool silent);
    void ForcePhase(HostileCharacterAiPhase phase);
    void ClearForcedPhase();

    /// Scripted “false signal”: jump to alert chasing a fixed world point (radius reserved for host policy).
    void ApplyFalseSignal(ri::math::Vec3 worldPoint, float /*radiusHint*/);

    [[nodiscard]] HostileCharacterAiPhase CurrentPhase() const;
    [[nodiscard]] bool HasForcedPhase() const;

    /// Last player position passed through visibility updates (may be stale if \ref HostileCharacterAiFrameInput::playerVisible is false).
    [[nodiscard]] ri::math::Vec3 LastKnownPlayerPosition() const;

private:
    void SanitizeDefinition();
    void UpdateLightAversion(const HostileCharacterAiFrameInput& input);
    [[nodiscard]] bool IsRepelledByLight() const;
    [[nodiscard]] bool LeashExceeded(const ri::math::Vec3& characterPosition) const;
    void HandlePlayerSafeZone(double nowSeconds);
    void RunStateMachine(const HostileCharacterAiFrameInput& input, HostileCharacterAiFrameOutput& out);
    void ComputeLocomotion(const HostileCharacterAiFrameInput& input,
                           HostileCharacterAiFrameOutput& out,
                           const ri::math::Vec3& moveTarget,
                           float moveSpeed,
                           float turnSpeed,
                           float minAlignment,
                           bool useRunAnim);

    HostileCharacterAiDefinition def_{};
    HostileCharacterAiPhase phase_ = HostileCharacterAiPhase::Patrol;
    bool forcedPhaseActive_ = false;
    HostileCharacterAiPhase forcedPhase_ = HostileCharacterAiPhase::Patrol;
    bool forceAlertSilent_ = false;

    double lightAversionTimer_ = 0.0;
    double alertEnteredAt_ = -1.0;
    double patrolPausedUntil_ = -1.0;
    double nextAttackAllowedAt_ = -1.0;
    double attackWindupEndsAt_ = -1.0;

    ri::math::Vec3 lastKnownPlayer_{};
    double lastSeenPlayerTime_ = -1.0;

    std::size_t patrolIndex_ = 0U;
    int patrolDirection_ = 1;

    bool issuedAwarenessMessage_ = false;
};

} // namespace ri::world
