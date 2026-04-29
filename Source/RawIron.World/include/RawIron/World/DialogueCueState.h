#pragma once

#include "RawIron/World/PresentationState.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace ri::world {

enum class DialogueCueMode {
    Disabled,
    Minimal,
    Verbose,
};

struct DialogueCuePolicy {
    DialogueCueMode mode = DialogueCueMode::Verbose;
    bool allowObjectiveUpdates = true;
    bool allowGuidanceHints = true;
    bool allowRepeatHints = true;
    std::size_t historyLimit = 16U;
};

struct DialogueCueRequest {
    std::string sourceId;
    std::string speakerLabel;
    std::string dialogueText;
    std::string repeatHint;
    std::string objectiveText;
    std::string guidanceHint;
    bool speakOnce = false;
    double dialogueDurationMs = 5000.0;
    double repeatDurationMs = 5000.0;
};

struct DialogueCueHistoryEntry {
    std::string sourceId;
    std::string message;
    std::string objectiveText;
    std::string guidanceHint;
    bool repeated = false;
    std::uint64_t revision = 0;
};

class DialogueCueState {
public:
    explicit DialogueCueState(const DialogueCuePolicy& policy = {});

    void SetPolicy(const DialogueCuePolicy& policy);
    [[nodiscard]] const DialogueCuePolicy& Policy() const;

    void Present(const DialogueCueRequest& request);
    void Advance(double elapsedMs);
    void ClearTransientState();
    void ResetConversationFlags();

    [[nodiscard]] const std::optional<TimedPresentationEntry>& ActiveDialogue() const;
    [[nodiscard]] const std::optional<TimedPresentationEntry>& ActiveGuidanceHint() const;
    [[nodiscard]] std::optional<std::string> ConsumePendingObjective();
    [[nodiscard]] const std::vector<DialogueCueHistoryEntry>& History() const;

private:
    void ActivateDialogue(std::string message, double durationMs);
    void ActivateGuidanceHint(std::string text, double durationMs);
    void PushHistory(const DialogueCueRequest& request, std::string message, std::string guidanceHint, bool repeated);
    [[nodiscard]] std::string MakeVerboseDialogue(const DialogueCueRequest& request) const;
    [[nodiscard]] std::string MakeMinimalDialogue(const DialogueCueRequest& request) const;
    [[nodiscard]] std::string NormalizeSpeakerLabel(std::string label) const;

    DialogueCuePolicy policy_{};
    std::optional<TimedPresentationEntry> activeDialogue_{};
    std::optional<TimedPresentationEntry> activeGuidanceHint_{};
    std::optional<std::string> pendingObjective_{};
    std::unordered_set<std::string> spokenSources_{};
    std::vector<DialogueCueHistoryEntry> history_{};
    std::uint64_t nextRevision_ = 1U;
};

} // namespace ri::world
