#include "RawIron/World/SignalBroadcastState.h"

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

SignalBroadcastState::SignalBroadcastState(const SignalBroadcastPolicy& policy) {
    SetPolicy(policy);
}

void SignalBroadcastState::SetPolicy(const SignalBroadcastPolicy& policy) {
    policy_ = policy;
    policy_.historyLimit = std::max<std::size_t>(1U, policy_.historyLimit);
    if (policy_.mode == SignalBroadcastMode::Disabled) {
        ClearTransientState();
        history_.clear();
    }
}

const SignalBroadcastPolicy& SignalBroadcastState::Policy() const {
    return policy_;
}

void SignalBroadcastState::Record(const SignalBroadcastRequest& request) {
    if (policy_.mode == SignalBroadcastMode::Disabled) {
        return;
    }

    const double safeDuration = SanitizeDuration(request.durationMs, 4000.0);
    ActivateTimed(activeMessage_, request.message.empty() ? "UNKNOWN SIGNAL" : request.message, safeDuration);

    const std::string subtitleText =
        policy_.mode == SignalBroadcastMode::Verbose ? request.subtitle : std::string{};
    const std::string guidanceText =
        policy_.mode == SignalBroadcastMode::Verbose && policy_.allowGuidanceHints ? request.guidanceHint : std::string{};
    if (!subtitleText.empty()) {
        ActivateTimed(activeSubtitle_, subtitleText, std::min(safeDuration, 4800.0));
    }
    if (!guidanceText.empty()) {
        ActivateTimed(activeGuidanceHint_, guidanceText, safeDuration);
    }
    PushHistory(activeMessage_->text, subtitleText, guidanceText);
}

void SignalBroadcastState::Advance(double elapsedMs) {
    AdvanceTimedEntry(activeMessage_, elapsedMs);
    AdvanceTimedEntry(activeSubtitle_, elapsedMs);
    AdvanceTimedEntry(activeGuidanceHint_, elapsedMs);
}

void SignalBroadcastState::ClearTransientState() {
    activeMessage_.reset();
    activeSubtitle_.reset();
    activeGuidanceHint_.reset();
}

const std::optional<TimedPresentationEntry>& SignalBroadcastState::ActiveMessage() const {
    return activeMessage_;
}

const std::optional<TimedPresentationEntry>& SignalBroadcastState::ActiveSubtitle() const {
    return activeSubtitle_;
}

const std::optional<TimedPresentationEntry>& SignalBroadcastState::ActiveGuidanceHint() const {
    return activeGuidanceHint_;
}

const std::vector<SignalBroadcastHistoryEntry>& SignalBroadcastState::History() const {
    return history_;
}

void SignalBroadcastState::ActivateTimed(std::optional<TimedPresentationEntry>& target,
                                         std::string text,
                                         double durationMs) {
    const double safeDuration = SanitizeDuration(durationMs, 4000.0);
    target = TimedPresentationEntry{
        .text = std::move(text),
        .durationMs = safeDuration,
        .remainingMs = safeDuration,
        .severity = PresentationSeverity::Normal,
    };
}

void SignalBroadcastState::PushHistory(std::string message, std::string subtitle, std::string guidanceHint) {
    history_.push_back(SignalBroadcastHistoryEntry{
        .message = std::move(message),
        .subtitle = std::move(subtitle),
        .guidanceHint = std::move(guidanceHint),
        .revision = nextRevision_++,
    });
    if (history_.size() > policy_.historyLimit) {
        history_.erase(history_.begin(),
                       history_.begin() + static_cast<std::ptrdiff_t>(history_.size() - policy_.historyLimit));
    }
}

} // namespace ri::world
