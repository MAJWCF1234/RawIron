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
        : DefaultGrantedMessage(request.context);
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
    pendingUiPulse_ = !request.uiPulseCue.empty();
    pendingHapticPulse_ = !request.hapticCue.empty();
    PushHistory(AccessFeedbackResult::Granted, message, objectiveText, hintText, request, false);
}

void AccessFeedbackState::RecordDenied(const AccessFeedbackRequest& request) {
    if (policy_.mode == AccessFeedbackMode::Disabled) {
        return;
    }
    if (lastDeniedEventMs_ >= 0.0 && (elapsedMs_ - lastDeniedEventMs_) < std::max(0.0, policy_.deniedCooldownMs)) {
        PushHistory(AccessFeedbackResult::Denied, {}, {}, {}, request, true);
        return;
    }
    lastDeniedEventMs_ = elapsedMs_;
    const std::string requiredItem = NormalizeRequiredItem(request.requiredItemLabel);
    const std::string defaultMessage = DefaultDeniedMessage(request.context, requiredItem);
    const std::string message = policy_.mode == AccessFeedbackMode::Verbose && !request.deniedMessage.empty()
        ? request.deniedMessage
        : defaultMessage;
    ActivateMessage(message, request.deniedDurationMs, SeverityForDeniedTier(request.deniedSeverity));

    const std::string hintText =
        policy_.mode == AccessFeedbackMode::Verbose && policy_.allowHints ? request.lockedHint : std::string{};
    if (!hintText.empty()) {
        ActivateHint(hintText, 5000.0);
    }
    pendingUiPulse_ = !request.uiPulseCue.empty();
    pendingHapticPulse_ = !request.hapticCue.empty();
    PushHistory(AccessFeedbackResult::Denied, message, {}, hintText, request, false);
}

void AccessFeedbackState::Advance(double elapsedMs) {
    if (std::isfinite(elapsedMs) && elapsedMs > 0.0) {
        elapsedMs_ += elapsedMs;
    }
    AdvanceTimedEntry(activeMessage_, elapsedMs);
    AdvanceTimedEntry(activeHint_, elapsedMs);
}

void AccessFeedbackState::ClearTransientState() {
    activeMessage_.reset();
    activeHint_.reset();
    pendingObjective_.reset();
    pendingUiPulse_ = false;
    pendingHapticPulse_ = false;
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

bool AccessFeedbackState::ConsumePendingUiPulse() {
    const bool value = pendingUiPulse_;
    pendingUiPulse_ = false;
    return value;
}

bool AccessFeedbackState::ConsumePendingHapticPulse() {
    const bool value = pendingHapticPulse_;
    pendingHapticPulse_ = false;
    return value;
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
                                      std::string hintText,
                                      const AccessFeedbackRequest& request,
                                      const bool suppressedByCooldown) {
    history_.push_back(AccessFeedbackHistoryEntry{
        .result = result,
        .message = std::move(message),
        .objectiveText = std::move(objectiveText),
        .hintText = std::move(hintText),
        .audioCue = result == AccessFeedbackResult::Granted ? request.grantedAudioCue : request.deniedAudioCue,
        .uiPulseCue = request.uiPulseCue,
        .hapticCue = request.hapticCue,
        .context = request.context,
        .deniedSeverity = request.deniedSeverity,
        .suppressedByCooldown = suppressedByCooldown,
        .revision = nextRevision_++,
    });
    if (history_.size() > policy_.historyLimit) {
        history_.erase(history_.begin(),
                       history_.begin() + static_cast<std::ptrdiff_t>(history_.size() - policy_.historyLimit));
    }
}

std::string AccessFeedbackState::DefaultGrantedMessage(const AccessFeedbackContext context) const {
    switch (context) {
    case AccessFeedbackContext::Terminal:
        return "Terminal access granted.";
    case AccessFeedbackContext::Door:
        return "Door unlocked.";
    case AccessFeedbackContext::Objective:
        return "Objective advanced.";
    case AccessFeedbackContext::Generic:
    default:
        return "Access granted.";
    }
}

std::string AccessFeedbackState::DefaultDeniedMessage(const AccessFeedbackContext context,
                                                      const std::string_view requiredItem) const {
    std::string prefix = "Access denied";
    switch (context) {
    case AccessFeedbackContext::Terminal:
        prefix = "Terminal access denied";
        break;
    case AccessFeedbackContext::Door:
        prefix = "Door locked";
        break;
    case AccessFeedbackContext::Objective:
        prefix = "Objective locked";
        break;
    case AccessFeedbackContext::Generic:
    default:
        break;
    }
    if (requiredItem.empty()) {
        return prefix + ".";
    }
    return prefix + ". " + std::string(requiredItem) + " required.";
}

PresentationSeverity AccessFeedbackState::SeverityForDeniedTier(const AccessDeniedSeverity severity) const {
    switch (severity) {
    case AccessDeniedSeverity::Low:
    case AccessDeniedSeverity::Medium:
        return PresentationSeverity::Normal;
    case AccessDeniedSeverity::High:
    case AccessDeniedSeverity::Critical:
    default:
        return PresentationSeverity::Critical;
    }
}

std::string AccessFeedbackState::NormalizeRequiredItem(std::string label) const {
    return label;
}

} // namespace ri::world
