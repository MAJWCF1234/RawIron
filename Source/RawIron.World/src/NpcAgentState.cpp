#include "RawIron/World/NpcAgentState.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ri::world {
namespace {

void FinalizePatrolTelemetry(NpcPatrolUpdate& update, NpcPatrolPhaseCode phase) {
    update.phaseCode = phase;
    update.animationIntentOrdinal = NpcAnimationIntentOrdinal(update.animationIntent);
}

double SanitizeDuration(double durationMs, double fallbackMs) {
    if (!std::isfinite(durationMs)) {
        return fallbackMs;
    }
    return std::clamp(durationMs, 0.0, 180000.0);
}

} // namespace

NpcAgentState::NpcAgentState(const NpcAgentPolicy& policy) {
    SetPolicy(policy);
}

void NpcAgentState::SetPolicy(const NpcAgentPolicy& policy) {
    policy_ = policy;
    policy_.historyLimit = std::max<std::size_t>(1U, policy_.historyLimit);
    if (policy_.mode == NpcAgentMode::Disabled) {
        ResetRuntimeState();
        history_.clear();
    }
}

const NpcAgentPolicy& NpcAgentState::Policy() const {
    return policy_;
}

void NpcAgentState::Configure(const NpcAgentDefinition& definition) {
    definition_ = definition;
    ResetRuntimeState();
}

const NpcAgentDefinition& NpcAgentState::Definition() const {
    return definition_;
}

NpcInteractionResult NpcAgentState::Interact(double,
                                             double dialogueDurationMs,
                                             double repeatDurationMs,
                                             std::string dialogueText,
                                             std::string repeatHint) {
    if (policy_.mode == NpcAgentMode::Disabled) {
        NpcInteractionResult rejected{};
        rejected.outcomeCode = NpcInteractionOutcomeCode::RejectedAgentDisabled;
        return rejected;
    }
    if (!policy_.allowInteraction) {
        NpcInteractionResult rejected{};
        rejected.outcomeCode = NpcInteractionOutcomeCode::RejectedInteractionDisabled;
        return rejected;
    }

    const bool coolingDown = interactionCooldownUntilSeconds_ > 0.0;
    if (coolingDown) {
        return NpcInteractionResult{
            .accepted = false,
            .repeated = false,
            .coolingDown = true,
            .outcomeCode = NpcInteractionOutcomeCode::RejectedCooldownActive,
            .displayText = "Interaction cooling down.",
            .animationAction = definition_.resumeAnimation.empty() ? definition_.defaultAnimation : definition_.resumeAnimation,
            .resumeAction = definition_.resumeAnimation.empty() ? definition_.defaultAnimation : definition_.resumeAnimation,
            .durationMs = 0.0,
        };
    }

    const bool repeated = policy_.allowSpeakOnce && definition_.speakOnce && hasSpoken_;
    if (repeated) {
        const std::string text = repeatHint.empty() ? "Nothing new." : std::move(repeatHint);
        PushHistory("repeat", text);
        interactionCooldownUntilSeconds_ = SanitizeDuration(repeatDurationMs, 5000.0) / 1000.0;
        return NpcInteractionResult{
            .accepted = true,
            .repeated = true,
            .coolingDown = false,
            .outcomeCode = NpcInteractionOutcomeCode::AcceptedRepeatedSpeakOnce,
            .displayText = text,
            .animationAction = definition_.resumeAnimation.empty() ? definition_.defaultAnimation : definition_.resumeAnimation,
            .resumeAction = definition_.resumeAnimation.empty() ? definition_.defaultAnimation : definition_.resumeAnimation,
            .durationMs = SanitizeDuration(repeatDurationMs, 5000.0),
        };
    }

    hasSpoken_ = true;
    const std::string text = dialogueText.empty()
        ? (definition_.displayName.empty() ? "NPC: STANDING BY." : definition_.displayName + ": STANDING BY.")
        : std::move(dialogueText);
    PushHistory("dialogue", text);
    interactionCooldownUntilSeconds_ = definition_.interactionCooldownMs > 0.0
        ? (definition_.interactionCooldownMs / 1000.0)
        : 0.0;
    return NpcInteractionResult{
        .accepted = true,
        .repeated = false,
        .coolingDown = false,
        .outcomeCode = NpcInteractionOutcomeCode::AcceptedPrimaryDialogue,
        .displayText = text,
        .animationAction = definition_.interactionAnimation.empty() ? "talk" : definition_.interactionAnimation,
        .resumeAction = definition_.resumeAnimation.empty() ? definition_.defaultAnimation : definition_.resumeAnimation,
        .durationMs = SanitizeDuration(dialogueDurationMs, 7000.0),
    };
}

NpcPatrolUpdate NpcAgentState::AdvancePatrol(double nowSeconds,
                                             float deltaSeconds,
                                             const ri::math::Vec3& currentPosition) {
    NpcPatrolUpdate update{};
    if (interactionCooldownUntilSeconds_ > 0.0) {
        interactionCooldownUntilSeconds_ = std::max(0.0, interactionCooldownUntilSeconds_ - static_cast<double>(std::max(0.0f, deltaSeconds)));
    }
    if (policy_.mode == NpcAgentMode::Disabled) {
        update.animationAction = definition_.pathIdleAnimation.empty() ? definition_.defaultAnimation : definition_.pathIdleAnimation;
        FinalizePatrolTelemetry(update, NpcPatrolPhaseCode::InactiveAgentDisabled);
        return update;
    }
    if (!policy_.allowPatrol) {
        update.animationAction = definition_.pathIdleAnimation.empty() ? definition_.defaultAnimation : definition_.pathIdleAnimation;
        FinalizePatrolTelemetry(update, NpcPatrolPhaseCode::InactivePatrolDisabled);
        return update;
    }
    if (definition_.pathPoints.empty()) {
        update.animationAction = definition_.pathIdleAnimation.empty() ? definition_.defaultAnimation : definition_.pathIdleAnimation;
        FinalizePatrolTelemetry(update, NpcPatrolPhaseCode::InactiveNoPath);
        return update;
    }

    update.active = true;
    if (pausedUntilSeconds_ > nowSeconds) {
        update.paused = true;
        update.animationIntent = NpcAnimationIntent::Idle;
        update.animationAction = definition_.pathIdleAnimation.empty() ? definition_.defaultAnimation : definition_.pathIdleAnimation;
        update.waypointIndex = pathIndex_;
        FinalizePatrolTelemetry(update, NpcPatrolPhaseCode::PausedWaitAtWaypoint);
        return update;
    }

    pathIndex_ = std::min(pathIndex_, definition_.pathPoints.size() - 1U);
    const ri::math::Vec3& target = definition_.pathPoints[pathIndex_];
    const float distance = ri::math::Distance(currentPosition, target);
    const float epsilon = std::max(0.05f, definition_.pathEpsilon);
    if (distance <= epsilon) {
        std::size_t nextIndex = pathIndex_;
        if (definition_.pathPoints.size() > 1U) {
            const std::size_t lastIndex = definition_.pathPoints.size() - 1U;
            if (pathDirection_ > 0) {
                nextIndex = std::min(lastIndex, pathIndex_ + 1U);
            } else if (pathIndex_ > 0U) {
                nextIndex = pathIndex_ - 1U;
            }
            if (nextIndex == pathIndex_) {
                if (definition_.patrolMode == NpcPatrolMode::Once) {
                    update.paused = true;
                    update.animationIntent = NpcAnimationIntent::Idle;
                    update.animationAction = definition_.pathIdleAnimation.empty() ? definition_.defaultAnimation : definition_.pathIdleAnimation;
                    update.waypointIndex = pathIndex_;
                    FinalizePatrolTelemetry(update, NpcPatrolPhaseCode::PausedPatrolCompletedOnce);
                    return update;
                }
                if (definition_.patrolMode == NpcPatrolMode::Loop || definition_.patrolLoop) {
                    nextIndex = 0U;
                } else {
                    pathDirection_ = -pathDirection_;
                    if (pathDirection_ > 0) {
                        nextIndex = std::min(lastIndex, pathIndex_ + 1U);
                    } else if (pathIndex_ > 0U) {
                        nextIndex = pathIndex_ - 1U;
                    }
                }
            }
        }
        pathIndex_ = nextIndex;
        if (definition_.pathWaitMs > 0.0) {
            pausedUntilSeconds_ = nowSeconds + (definition_.pathWaitMs / 1000.0);
            update.paused = true;
            update.animationIntent = NpcAnimationIntent::Idle;
            update.animationAction = definition_.pathIdleAnimation.empty() ? definition_.defaultAnimation : definition_.pathIdleAnimation;
            update.waypointIndex = pathIndex_;
            FinalizePatrolTelemetry(update, NpcPatrolPhaseCode::PausedWaitAtWaypoint);
            return update;
        }
    }

    const ri::math::Vec3& nextTarget = definition_.pathPoints[pathIndex_];
    update.targetPosition = nextTarget;
    update.desiredSpeed = std::max(0.0f, definition_.patrolSpeed);
    const bool shouldRun = update.desiredSpeed >= std::max(0.1f, definition_.pathRunThreshold);
    update.animationIntent = shouldRun ? NpcAnimationIntent::Run : NpcAnimationIntent::Walk;
    update.animationAction = shouldRun
        ? (definition_.pathRunAnimation.empty() ? definition_.pathAnimation : definition_.pathRunAnimation)
        : (definition_.pathAnimation.empty() ? definition_.defaultAnimation : definition_.pathAnimation);
    if (update.animationAction.empty()) {
        update.animationAction = definition_.defaultAnimation;
    }
    update.waypointIndex = pathIndex_;
    FinalizePatrolTelemetry(update, shouldRun ? NpcPatrolPhaseCode::AdvancingRun : NpcPatrolPhaseCode::AdvancingWalk);
    return update;
}

void NpcAgentState::ResetRuntimeState() {
    pathIndex_ = 0U;
    pathDirection_ = 1;
    pausedUntilSeconds_ = 0.0;
    interactionCooldownUntilSeconds_ = 0.0;
    hasSpoken_ = false;
}

const std::vector<NpcAgentHistoryEntry>& NpcAgentState::History() const {
    return history_;
}

void NpcAgentState::PushHistory(std::string category, std::string text) {
    history_.push_back(NpcAgentHistoryEntry{
        .category = std::move(category),
        .text = std::move(text),
        .revision = nextRevision_++,
    });
    if (history_.size() > policy_.historyLimit) {
        history_.erase(history_.begin(),
                       history_.begin() + static_cast<std::ptrdiff_t>(history_.size() - policy_.historyLimit));
    }
}

} // namespace ri::world
