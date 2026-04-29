#include "RawIron/World/PresentationState.h"

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

PresentationState::PresentationState(std::size_t historyLimit)
    : historyLimit_(std::max<std::size_t>(1U, historyLimit)) {}

void PresentationState::ShowMessage(std::string text,
                                    double durationMs,
                                    PresentationSeverity severity) {
    const double safeDuration = SanitizeDuration(durationMs, 4000.0);
    activeMessage_ = TimedPresentationEntry{
        .text = std::move(text),
        .durationMs = safeDuration,
        .remainingMs = safeDuration,
        .severity = severity,
    };
    PushHistory(PresentationChannel::Message, activeMessage_->text, severity, safeDuration);
}

void PresentationState::ShowSubtitle(std::string text, double durationMs) {
    const double safeDuration = SanitizeDuration(durationMs, 5000.0);
    activeSubtitle_ = TimedPresentationEntry{
        .text = std::move(text),
        .durationMs = safeDuration,
        .remainingMs = safeDuration,
        .severity = PresentationSeverity::Normal,
    };
    PushHistory(PresentationChannel::Subtitle, activeSubtitle_->text, PresentationSeverity::Normal, safeDuration);
}

void PresentationState::UpdateObjective(std::string text, const ObjectiveUpdateOptions& options) {
    objective_.text = std::move(text);
    objective_.hint = options.hint;
    objective_.flashRemainingMs = options.flash ? 1500.0 : 0.0;
    objective_.revision = nextRevision_++;
    PushHistory(PresentationChannel::Objective, objective_.text, PresentationSeverity::Normal, objective_.flashRemainingMs);

    if (options.announce && !objective_.text.empty()) {
        ShowMessage("NEW OBJECTIVE: " + objective_.text, 4000.0, PresentationSeverity::Normal);
    } else if (!options.hint.empty()) {
        ShowMessage(options.hint, options.hintDurationMs, PresentationSeverity::Normal);
    }
}

void PresentationState::PushNarrativeMessage(std::string text, double durationMs, bool urgent) {
    ShowMessage(std::move(text), durationMs, urgent ? PresentationSeverity::Critical : PresentationSeverity::Normal);
    if (urgent) {
        TriggerUrgencyFlash(std::min(1200.0, durationMs));
    }
}

void PresentationState::TriggerUrgencyFlash(double durationMs) {
    urgencyFlashRemainingMs_ = SanitizeDuration(durationMs, 600.0);
}

void PresentationState::Advance(double elapsedMs) {
    AdvanceTimedEntry(activeMessage_, elapsedMs);
    AdvanceTimedEntry(activeSubtitle_, elapsedMs);

    if (std::isfinite(elapsedMs) && elapsedMs > 0.0) {
        objective_.flashRemainingMs = std::max(0.0, objective_.flashRemainingMs - elapsedMs);
        urgencyFlashRemainingMs_ = std::max(0.0, urgencyFlashRemainingMs_ - elapsedMs);
    }
}

void PresentationState::ClearTransientState() {
    activeMessage_.reset();
    activeSubtitle_.reset();
    objective_.flashRemainingMs = 0.0;
    urgencyFlashRemainingMs_ = 0.0;
}

const std::optional<TimedPresentationEntry>& PresentationState::ActiveMessage() const {
    return activeMessage_;
}

const std::optional<TimedPresentationEntry>& PresentationState::ActiveSubtitle() const {
    return activeSubtitle_;
}

const ObjectivePresentationState& PresentationState::Objective() const {
    return objective_;
}

const std::vector<PresentationHistoryEntry>& PresentationState::History() const {
    return history_;
}

double PresentationState::UrgencyFlashRemainingMs() const {
    return urgencyFlashRemainingMs_;
}

void PresentationState::PushHistory(PresentationChannel channel,
                                    std::string text,
                                    PresentationSeverity severity,
                                    double durationMs) {
    history_.push_back(PresentationHistoryEntry{
        .channel = channel,
        .text = std::move(text),
        .severity = severity,
        .durationMs = durationMs,
        .revision = nextRevision_++,
    });
    if (history_.size() > historyLimit_) {
        history_.erase(history_.begin(), history_.begin() + static_cast<std::ptrdiff_t>(history_.size() - historyLimit_));
    }
}

} // namespace ri::world
