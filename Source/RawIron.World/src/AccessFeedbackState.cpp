#include "RawIron/World/AccessFeedbackState.h"

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

AccessFeedbackState::AccessFeedbackState(const AccessFeedbackPolicy& policy) {
    SetPolicy(policy);
}

void AccessFeedbackState::SetPolicy(const AccessFeedbackPolicy& policy) {
    policy_ = policy;
    policy_.historyLimit = std::max<std::size_t>(1U, policy_.historyLimit);
    if (policy_.mode == AccessFeedbackMode::Disabled) {
        ClearTransientState();
        history_.clear();
    }
}

const AccessFeedbackPolicy& AccessFeedbackState::Policy() const {
    return policy_;
}

void AccessFeedbackState::RecordGranted(const AccessFeedbackRequest& request) {
    if (policy_.mode == AccessFeedbackMode::Disabled) {
        return;
    }
    const std::string message = policy_.mode == AccessFeedbackMode::Verbose && !request.grantedMessage.empty()
        ? request.grantedMessage
        : "Access granted.";
    ActivateMessage(message, request.grantedDurationMs, PresentationSeverity::Normal);

    const std::string objectiveText = policy_.allowObjectiveUpdates ? request.unlockObjective : std::string{};
    const std::string hintText =
        policy_.mode == AccessFeedbackMode::Verbose && policy_.allowHints ? request.unlockHint : std::string{};
    if (!objectiveText.empty()) {
        pendingObjective_ = objectiveText;
    }
    if (!hintText.empty()) {
        ActivateHint(hintText, 5000.0);
    }
    PushHistory(AccessFeedbackResult::Granted, message, objectiveText, hintText);
}

void AccessFeedbackState::RecordDenied(const AccessFeedbackRequest& request) {
    if (policy_.mode == AccessFeedbackMode::Disabled) {
        return;
    }
    const std::string requiredItem = NormalizeRequiredItem(request.requiredItemLabel);
    const std::string defaultMessage = requiredItem.empty()
        ? "Access denied."
        : "Access denied. " + requiredItem + " required.";
    const std::string message = policy_.mode == AccessFeedbackMode::Verbose && !request.deniedMessage.empty()
        ? request.deniedMessage
        : defaultMessage;
    ActivateMessage(message, request.deniedDurationMs, PresentationSeverity::Critical);

    const std::string hintText =
        policy_.mode == AccessFeedbackMode::Verbose && policy_.allowHints ? request.lockedHint : std::string{};
    if (!hintText.empty()) {
        ActivateHint(hintText, 5000.0);
    }
    PushHistory(AccessFeedbackResult::Denied, message, {}, hintText);
}

void AccessFeedbackState::Advance(double elapsedMs) {
    AdvanceTimedEntry(activeMessage_, elapsedMs);
    AdvanceTimedEntry(activeHint_, elapsedMs);
}

void AccessFeedbackState::ClearTransientState() {
    activeMessage_.reset();
    activeHint_.reset();
    pendingObjective_.reset();
}

const std::optional<TimedPresentationEntry>& AccessFeedbackState::ActiveMessage() const {
    return activeMessage_;
}

const std::optional<TimedPresentationEntry>& AccessFeedbackState::ActiveHint() const {
    return activeHint_;
}

std::optional<std::string> AccessFeedbackState::ConsumePendingObjective() {
    std::optional<std::string> value = std::move(pendingObjective_);
    pendingObjective_.reset();
    return value;
}

const std::vector<AccessFeedbackHistoryEntry>& AccessFeedbackState::History() const {
    return history_;
}

void AccessFeedbackState::ActivateMessage(std::string message, double durationMs, PresentationSeverity severity) {
    const double safeDuration = SanitizeDuration(durationMs, severity == PresentationSeverity::Critical ? 4000.0 : 2000.0);
    activeMessage_ = TimedPresentationEntry{
        .text = std::move(message),
        .durationMs = safeDuration,
        .remainingMs = safeDuration,
        .severity = severity,
    };
}

void AccessFeedbackState::ActivateHint(std::string hintText, double durationMs) {
    const double safeDuration = SanitizeDuration(durationMs, 5000.0);
    activeHint_ = TimedPresentationEntry{
        .text = std::move(hintText),
        .durationMs = safeDuration,
        .remainingMs = safeDuration,
        .severity = PresentationSeverity::Normal,
    };
}

void AccessFeedbackState::PushHistory(AccessFeedbackResult result,
                                      std::string message,
                                      std::string objectiveText,
                                      std::string hintText) {
    history_.push_back(AccessFeedbackHistoryEntry{
        .result = result,
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

std::string AccessFeedbackState::NormalizeRequiredItem(std::string label) const {
    return label;
}

} // namespace ri::world
