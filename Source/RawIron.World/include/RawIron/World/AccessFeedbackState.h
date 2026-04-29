#pragma once

#include "RawIron/World/PresentationState.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ri::world {

enum class AccessFeedbackMode {
    Disabled,
    Minimal,
    Verbose,
};

enum class AccessFeedbackResult {
    Granted,
    Denied,
};

struct AccessFeedbackPolicy {
    AccessFeedbackMode mode = AccessFeedbackMode::Verbose;
    bool allowObjectiveUpdates = true;
    bool allowHints = true;
    std::size_t historyLimit = 16U;
};

struct AccessFeedbackRequest {
    std::string requiredItemLabel;
    std::string grantedMessage;
    std::string deniedMessage;
    std::string unlockObjective;
    std::string unlockHint;
    std::string lockedHint;
    double grantedDurationMs = 2000.0;
    double deniedDurationMs = 4000.0;
};

struct AccessFeedbackHistoryEntry {
    AccessFeedbackResult result = AccessFeedbackResult::Granted;
    std::string message;
    std::string objectiveText;
    std::string hintText;
    std::uint64_t revision = 0;
};

class AccessFeedbackState {
public:
    explicit AccessFeedbackState(const AccessFeedbackPolicy& policy = {});

    void SetPolicy(const AccessFeedbackPolicy& policy);
    [[nodiscard]] const AccessFeedbackPolicy& Policy() const;

    void RecordGranted(const AccessFeedbackRequest& request);
    void RecordDenied(const AccessFeedbackRequest& request);
    void Advance(double elapsedMs);
    void ClearTransientState();

    [[nodiscard]] const std::optional<TimedPresentationEntry>& ActiveMessage() const;
    [[nodiscard]] const std::optional<TimedPresentationEntry>& ActiveHint() const;
    [[nodiscard]] std::optional<std::string> ConsumePendingObjective();
    [[nodiscard]] const std::vector<AccessFeedbackHistoryEntry>& History() const;

private:
    void ActivateMessage(std::string message, double durationMs, PresentationSeverity severity);
    void ActivateHint(std::string hintText, double durationMs);
    void PushHistory(AccessFeedbackResult result, std::string message, std::string objectiveText, std::string hintText);
    [[nodiscard]] std::string NormalizeRequiredItem(std::string label) const;

    AccessFeedbackPolicy policy_{};
    std::optional<TimedPresentationEntry> activeMessage_{};
    std::optional<TimedPresentationEntry> activeHint_{};
    std::optional<std::string> pendingObjective_{};
    std::vector<AccessFeedbackHistoryEntry> history_{};
    std::uint64_t nextRevision_ = 1U;
};

} // namespace ri::world
