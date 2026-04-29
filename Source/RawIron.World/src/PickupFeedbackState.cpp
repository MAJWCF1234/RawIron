#include "RawIron/World/PickupFeedbackState.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ri::world {
namespace {

double SanitizeDuration(double durationMs, double fallbackMs) {
    if (!std::isfinite(durationMs)) {
        return fallbackMs;
    }
    return std::clamp(durationMs, 0.0, 180000.0);
}

void AdvanceTimedEntry(std::optional<TimedPresentationEntry>& entry, double elapsedMs) {
    if (!entry.has_value()) {
        return;
    }
    if (!std::isfinite(elapsedMs) || elapsedMs <= 0.0) {
        return;
    }
    entry->remainingMs = std::max(0.0, entry->remainingMs - elapsedMs);
    if (entry->remainingMs <= 0.0) {
        entry.reset();
    }
}

} // namespace

PickupFeedbackState::PickupFeedbackState(const PickupFeedbackPolicy& policy) {
    SetPolicy(policy);
}

void PickupFeedbackState::SetPolicy(const PickupFeedbackPolicy& policy) {
    policy_ = policy;
    policy_.historyLimit = std::max<std::size_t>(1U, policy_.historyLimit);
    if (policy_.mode == PickupFeedbackMode::Disabled) {
        ClearTransientState();
        history_.clear();
    }
}

const PickupFeedbackPolicy& PickupFeedbackState::Policy() const {
    return policy_;
}

void PickupFeedbackState::RecordPickup(const PickupFeedbackRequest& request) {
    if (policy_.mode == PickupFeedbackMode::Disabled) {
        return;
    }

    const std::string label = NormalizeLabel(request.itemLabel.empty() ? request.itemId : request.itemLabel);
    const std::string message = policy_.mode == PickupFeedbackMode::Verbose && !request.pickupMessage.empty()
        ? request.pickupMessage
        : "Picked up " + label;
    ActivateMessage(message, request.messageDurationMs);

    const std::string objectiveText = policy_.allowObjectiveUpdates ? request.objectiveText : std::string{};
    const std::string hintText =
        policy_.mode == PickupFeedbackMode::Verbose && policy_.allowHints ? request.hintText : std::string{};
    if (!objectiveText.empty()) {
        pendingObjective_ = objectiveText;
    }
    if (!hintText.empty()) {
        ActivateHint(hintText, request.hintDurationMs);
    }
    PushHistory(PickupFeedbackKind::PickedUp, message, objectiveText, hintText);
}

void PickupFeedbackState::RecordAlreadyCarrying(std::string itemLabel, double durationMs) {
    if (policy_.mode == PickupFeedbackMode::Disabled) {
        return;
    }
    const std::string message = "Already carrying " + NormalizeLabel(std::move(itemLabel));
    ActivateMessage(message, durationMs);
    PushHistory(PickupFeedbackKind::AlreadyCarrying, message, {}, {});
}

void PickupFeedbackState::RecordConsumed(std::string itemLabel, double durationMs) {
    if (policy_.mode == PickupFeedbackMode::Disabled) {
        return;
    }
    const std::string message = "Used " + NormalizeLabel(std::move(itemLabel));
    ActivateMessage(message, durationMs);
    PushHistory(PickupFeedbackKind::Consumed, message, {}, {});
}

void PickupFeedbackState::RecordUnavailable(std::string label, double durationMs) {
    if (policy_.mode == PickupFeedbackMode::Disabled) {
        return;
    }
    const std::string message = NormalizeLabel(std::move(label)) + " unavailable";
    ActivateMessage(message, durationMs);
    PushHistory(PickupFeedbackKind::Unavailable, message, {}, {});
}

void PickupFeedbackState::Advance(double elapsedMs) {
    AdvanceTimedEntry(activeMessage_, elapsedMs);
    AdvanceTimedEntry(activeHint_, elapsedMs);
}

void PickupFeedbackState::ClearTransientState() {
    activeMessage_.reset();
    activeHint_.reset();
    pendingObjective_.reset();
}

const std::optional<TimedPresentationEntry>& PickupFeedbackState::ActiveMessage() const {
    return activeMessage_;
}

const std::optional<TimedPresentationEntry>& PickupFeedbackState::ActiveHint() const {
    return activeHint_;
}

std::optional<std::string> PickupFeedbackState::ConsumePendingObjective() {
    std::optional<std::string> value = std::move(pendingObjective_);
    pendingObjective_.reset();
    return value;
}

const std::vector<PickupFeedbackHistoryEntry>& PickupFeedbackState::History() const {
    return history_;
}

void PickupFeedbackState::ActivateMessage(std::string message, double durationMs) {
    const double safeDuration = SanitizeDuration(durationMs, 3500.0);
    activeMessage_ = TimedPresentationEntry{
        .text = std::move(message),
        .durationMs = safeDuration,
        .remainingMs = safeDuration,
        .severity = PresentationSeverity::Normal,
    };
}

void PickupFeedbackState::ActivateHint(std::string hintText, double durationMs) {
    const double safeDuration = SanitizeDuration(durationMs, 6000.0);
    activeHint_ = TimedPresentationEntry{
        .text = std::move(hintText),
        .durationMs = safeDuration,
        .remainingMs = safeDuration,
        .severity = PresentationSeverity::Normal,
    };
}

void PickupFeedbackState::PushHistory(PickupFeedbackKind kind,
                                      std::string message,
                                      std::string objectiveText,
                                      std::string hintText) {
    history_.push_back(PickupFeedbackHistoryEntry{
        .kind = kind,
        .message = std::move(message),
        .objectiveText = std::move(objectiveText),
        .hintText = std::move(hintText),
        .revision = nextRevision_++,
    });
    if (history_.size() > policy_.historyLimit) {
        history_.erase(history_.begin(),
                       history_.begin() + static_cast<std::ptrdiff_t>(history_.size() - policy_.historyLimit));
    }
}

std::string PickupFeedbackState::NormalizeLabel(std::string label) const {
    return label.empty() ? "item" : std::move(label);
}

} // namespace ri::world
