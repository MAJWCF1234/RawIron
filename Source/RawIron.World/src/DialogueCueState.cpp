#include "RawIron/World/DialogueCueState.h"

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

DialogueCueState::DialogueCueState(const DialogueCuePolicy& policy) {
    SetPolicy(policy);
}

void DialogueCueState::SetPolicy(const DialogueCuePolicy& policy) {
    policy_ = policy;
    policy_.historyLimit = std::max<std::size_t>(1U, policy_.historyLimit);
    if (policy_.mode == DialogueCueMode::Disabled) {
        ClearTransientState();
        history_.clear();
    }
}

const DialogueCuePolicy& DialogueCueState::Policy() const {
    return policy_;
}

void DialogueCueState::Present(const DialogueCueRequest& request) {
    if (policy_.mode == DialogueCueMode::Disabled) {
        return;
    }

    const bool repeated = request.speakOnce
        && !request.sourceId.empty()
        && spokenSources_.contains(request.sourceId);

    if (repeated) {
        const std::string repeatText = policy_.allowRepeatHints
            ? (request.repeatHint.empty() ? "Nothing new." : request.repeatHint)
            : "Nothing new.";
        ActivateDialogue(repeatText, request.repeatDurationMs);
        PushHistory(request, repeatText, {}, true);
        return;
    }

    if (request.speakOnce && !request.sourceId.empty()) {
        spokenSources_.insert(request.sourceId);
    }

    const std::string message = policy_.mode == DialogueCueMode::Verbose
        ? MakeVerboseDialogue(request)
        : MakeMinimalDialogue(request);
    ActivateDialogue(message, request.dialogueDurationMs);

    const std::string objectiveText = policy_.allowObjectiveUpdates ? request.objectiveText : std::string{};
    const std::string guidanceHint = policy_.mode == DialogueCueMode::Verbose && policy_.allowGuidanceHints
        ? request.guidanceHint
        : std::string{};
    if (!objectiveText.empty()) {
        pendingObjective_ = objectiveText;
    }
    if (!guidanceHint.empty()) {
        ActivateGuidanceHint(guidanceHint, request.dialogueDurationMs);
    }
    PushHistory(request, message, guidanceHint, false);
}

void DialogueCueState::Advance(double elapsedMs) {
    AdvanceTimedEntry(activeDialogue_, elapsedMs);
    AdvanceTimedEntry(activeGuidanceHint_, elapsedMs);
}

void DialogueCueState::ClearTransientState() {
    activeDialogue_.reset();
    activeGuidanceHint_.reset();
    pendingObjective_.reset();
}

void DialogueCueState::ResetConversationFlags() {
    spokenSources_.clear();
}

const std::optional<TimedPresentationEntry>& DialogueCueState::ActiveDialogue() const {
    return activeDialogue_;
}

const std::optional<TimedPresentationEntry>& DialogueCueState::ActiveGuidanceHint() const {
    return activeGuidanceHint_;
}

std::optional<std::string> DialogueCueState::ConsumePendingObjective() {
    std::optional<std::string> value = std::move(pendingObjective_);
    pendingObjective_.reset();
    return value;
}

const std::vector<DialogueCueHistoryEntry>& DialogueCueState::History() const {
    return history_;
}

void DialogueCueState::ActivateDialogue(std::string message, double durationMs) {
    const double safeDuration = SanitizeDuration(durationMs, 5000.0);
    activeDialogue_ = TimedPresentationEntry{
        .text = std::move(message),
        .durationMs = safeDuration,
        .remainingMs = safeDuration,
        .severity = PresentationSeverity::Normal,
    };
}

void DialogueCueState::ActivateGuidanceHint(std::string text, double durationMs) {
    const double safeDuration = SanitizeDuration(durationMs, 5000.0);
    activeGuidanceHint_ = TimedPresentationEntry{
        .text = std::move(text),
        .durationMs = safeDuration,
        .remainingMs = safeDuration,
        .severity = PresentationSeverity::Normal,
    };
}

void DialogueCueState::PushHistory(const DialogueCueRequest& request,
                                   std::string message,
                                   std::string guidanceHint,
                                   bool repeated) {
    history_.push_back(DialogueCueHistoryEntry{
        .sourceId = request.sourceId,
        .message = std::move(message),
        .objectiveText = policy_.allowObjectiveUpdates ? request.objectiveText : std::string{},
        .guidanceHint = std::move(guidanceHint),
        .repeated = repeated,
        .revision = nextRevision_++,
    });
    if (history_.size() > policy_.historyLimit) {
        history_.erase(history_.begin(),
                       history_.begin() + static_cast<std::ptrdiff_t>(history_.size() - policy_.historyLimit));
    }
}

std::string DialogueCueState::MakeVerboseDialogue(const DialogueCueRequest& request) const {
    if (!request.dialogueText.empty()) {
        return request.dialogueText;
    }
    return NormalizeSpeakerLabel(request.speakerLabel) + ": STANDING BY.";
}

std::string DialogueCueState::MakeMinimalDialogue(const DialogueCueRequest& request) const {
    return NormalizeSpeakerLabel(request.speakerLabel) + " interaction logged.";
}

std::string DialogueCueState::NormalizeSpeakerLabel(std::string label) const {
    return label.empty() ? "NPC" : std::move(label);
}

} // namespace ri::world
