#pragma once

// NPC patrol / interaction intent and telemetry — developer map: Documentation/02 Engine/NPC Behavior Support.md

#include "RawIron/Math/Vec3.h"
#include "RawIron/World/HeadlessVerification.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ri::world {

enum class NpcAgentMode {
    Disabled,
    Ambient,
    Patrol,
};

enum class NpcPatrolMode {
    Loop,
    PingPong,
    Once,
};

enum class NpcAnimationIntent {
    Idle,
    Walk,
    Run,
    Alert,
    Interact,
};

[[nodiscard]] constexpr std::uint8_t NpcAnimationIntentOrdinal(NpcAnimationIntent intent) noexcept {
    return static_cast<std::uint8_t>(intent);
}

// Serial chart (band 0x01xx, see headless::kNpcInteractionOutcomeBand). Stable for tests/telemetry.
enum class NpcInteractionOutcomeCode : std::uint16_t {
    Unspecified = 0,
    RejectedAgentDisabled = 0x0101,
    RejectedInteractionDisabled = 0x0102,
    RejectedCooldownActive = 0x0103,
    AcceptedRepeatedSpeakOnce = 0x0104,
    AcceptedPrimaryDialogue = 0x0105,
};

// Serial chart (band 0x02xx, see headless::kNpcPatrolPhaseBand). Stable for tests/telemetry.
enum class NpcPatrolPhaseCode : std::uint16_t {
    Unspecified = 0,
    InactiveAgentDisabled = 0x0201,
    InactivePatrolDisabled = 0x0202,
    InactiveNoPath = 0x0203,
    PausedWaitAtWaypoint = 0x0210,
    PausedPatrolCompletedOnce = 0x0211,
    AdvancingWalk = 0x0220,
    AdvancingRun = 0x0221,
};

[[nodiscard]] constexpr std::string_view NpcInteractionOutcomeLabel(NpcInteractionOutcomeCode code) noexcept {
    switch (code) {
    case NpcInteractionOutcomeCode::Unspecified:
        return "unspecified";
    case NpcInteractionOutcomeCode::RejectedAgentDisabled:
        return "rejected_agent_disabled";
    case NpcInteractionOutcomeCode::RejectedInteractionDisabled:
        return "rejected_interaction_disabled";
    case NpcInteractionOutcomeCode::RejectedCooldownActive:
        return "rejected_cooldown_active";
    case NpcInteractionOutcomeCode::AcceptedRepeatedSpeakOnce:
        return "accepted_repeated_speak_once";
    case NpcInteractionOutcomeCode::AcceptedPrimaryDialogue:
        return "accepted_primary_dialogue";
    default:
        return "unknown";
    }
}

[[nodiscard]] constexpr std::string_view NpcPatrolPhaseLabel(NpcPatrolPhaseCode code) noexcept {
    switch (code) {
    case NpcPatrolPhaseCode::Unspecified:
        return "unspecified";
    case NpcPatrolPhaseCode::InactiveAgentDisabled:
        return "inactive_agent_disabled";
    case NpcPatrolPhaseCode::InactivePatrolDisabled:
        return "inactive_patrol_disabled";
    case NpcPatrolPhaseCode::InactiveNoPath:
        return "inactive_no_path";
    case NpcPatrolPhaseCode::PausedWaitAtWaypoint:
        return "paused_wait_at_waypoint";
    case NpcPatrolPhaseCode::PausedPatrolCompletedOnce:
        return "paused_patrol_completed_once";
    case NpcPatrolPhaseCode::AdvancingWalk:
        return "advancing_walk";
    case NpcPatrolPhaseCode::AdvancingRun:
        return "advancing_run";
    default:
        return "unknown";
    }
}

struct NpcAgentPolicy {
    NpcAgentMode mode = NpcAgentMode::Patrol;
    bool allowInteraction = true;
    bool allowPatrol = true;
    bool allowSpeakOnce = true;
    std::size_t historyLimit = 16U;
};

struct NpcAgentDefinition {
    std::string id;
    std::string displayName;
    std::string defaultAnimation = "idle";
    std::string interactionAnimation = "talk";
    std::string resumeAnimation = "idle";
    std::string pathAnimation = "walk";
    std::string pathRunAnimation = "run";
    std::string pathTurnAnimation = "alert";
    std::string pathIdleAnimation = "idle";
    float patrolSpeed = 1.1f;
    float pathRunThreshold = 1.55f;
    float pathEpsilon = 0.35f;
    double pathWaitMs = 0.0;
    double interactionCooldownMs = 0.0;
    NpcPatrolMode patrolMode = NpcPatrolMode::Loop;
    bool patrolLoop = true;
    bool lookAtPath = true;
    bool speakOnce = false;
    std::vector<ri::math::Vec3> pathPoints{};
};

struct NpcInteractionResult {
    bool accepted = false;
    bool repeated = false;
    bool coolingDown = false;
    NpcInteractionOutcomeCode outcomeCode = NpcInteractionOutcomeCode::Unspecified;
    std::string displayText;
    std::string animationAction;
    std::string resumeAction;
    double durationMs = 0.0;
};

struct NpcPatrolUpdate {
    bool active = false;
    bool paused = false;
    NpcPatrolPhaseCode phaseCode = NpcPatrolPhaseCode::Unspecified;
    std::uint8_t animationIntentOrdinal = NpcAnimationIntentOrdinal(NpcAnimationIntent::Idle);
    ri::math::Vec3 targetPosition{};
    float desiredSpeed = 0.0f;
    NpcAnimationIntent animationIntent = NpcAnimationIntent::Idle;
    std::string animationAction;
    std::size_t waypointIndex = 0U;
};

struct NpcAgentHistoryEntry {
    std::string category;
    std::string text;
    std::uint64_t revision = 0;
};

class NpcAgentState {
public:
    explicit NpcAgentState(const NpcAgentPolicy& policy = {});

    void SetPolicy(const NpcAgentPolicy& policy);
    [[nodiscard]] const NpcAgentPolicy& Policy() const;

    void Configure(const NpcAgentDefinition& definition);
    [[nodiscard]] const NpcAgentDefinition& Definition() const;

    [[nodiscard]] NpcInteractionResult Interact(double nowSeconds,
                                                double dialogueDurationMs,
                                                double repeatDurationMs,
                                                std::string dialogueText,
                                                std::string repeatHint);
    [[nodiscard]] NpcPatrolUpdate AdvancePatrol(double nowSeconds,
                                                float deltaSeconds,
                                                const ri::math::Vec3& currentPosition);
    void ResetRuntimeState();

    [[nodiscard]] const std::vector<NpcAgentHistoryEntry>& History() const;

private:
    void PushHistory(std::string category, std::string text);

    NpcAgentPolicy policy_{};
    NpcAgentDefinition definition_{};
    std::vector<NpcAgentHistoryEntry> history_{};
    std::uint64_t nextRevision_ = 1U;
    std::size_t pathIndex_ = 0U;
    int pathDirection_ = 1;
    double pausedUntilSeconds_ = 0.0;
    double interactionCooldownUntilSeconds_ = 0.0;
    bool hasSpoken_ = false;
};

} // namespace ri::world
