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

enum class AccessFeedbackContext {
    Generic,
    Terminal,
    Door,
    Objective,
};

enum class AccessDeniedSeverity {
    Low,
    Medium,
    High,
    Critical,
};

struct AccessFeedbackPolicy {
    AccessFeedbackMode mode = AccessFeedbackMode::Verbose;
    bool allowObjectiveUpdates = true;
    bool allowHints = true;
    double deniedCooldownMs = 350.0;
    std::size_t historyLimit = 16U;
};

struct AccessFeedbackRequest {
    std::string requiredItemLabel;
    std::string grantedMessage;
    std::string deniedMessage;
    std::string unlockObjective;
    std::string unlockHint;
    std::string lockedHint;
    std::string grantedAudioCue;
    std::string deniedAudioCue;
    std::string uiPulseCue;
    std::string hapticCue;
    AccessFeedbackContext context = AccessFeedbackContext::Generic;
    AccessDeniedSeverity deniedSeverity = AccessDeniedSeverity::Medium;
    double grantedDurationMs = 2000.0;
    double deniedDurationMs = 4000.0;
};

struct AccessFeedbackHistoryEntry {
    AccessFeedbackResult result = AccessFeedbackResult::Granted;
    std::string message;
    std::string objectiveText;
    std::string hintText;
    std::string audioCue;
    std::string uiPulseCue;
    std::string hapticCue;
    AccessFeedbackContext context = AccessFeedbackContext::Generic;
    AccessDeniedSeverity deniedSeverity = AccessDeniedSeverity::Medium;
    bool suppressedByCooldown = false;
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
    [[nodiscard]] bool ConsumePendingUiPulse();
    [[nodiscard]] bool ConsumePendingHapticPulse();

private:
    void ActivateMessage(std::string message, double durationMs, PresentationSeverity severity);
    void ActivateHint(std::string hintText, double durationMs);
    void PushHistory(AccessFeedbackResult result,
                     std::string message,
                     std::string objectiveText,
                     std::string hintText,
                     const AccessFeedbackRequest& request,
                     bool suppressedByCooldown);
    [[nodiscard]] std::string DefaultGrantedMessage(AccessFeedbackContext context) const;
    [[nodiscard]] std::string DefaultDeniedMessage(AccessFeedbackContext context, std::string_view requiredItem) const;
    [[nodiscard]] PresentationSeverity SeverityForDeniedTier(AccessDeniedSeverity severity) const;
    [[nodiscard]] std::string NormalizeRequiredItem(std::string label) const;

    AccessFeedbackPolicy policy_{};
    std::optional<TimedPresentationEntry> activeMessage_{};
    std::optional<TimedPresentationEntry> activeHint_{};
    std::optional<std::string> pendingObjective_{};
    bool pendingUiPulse_ = false;
    bool pendingHapticPulse_ = false;
    std::vector<AccessFeedbackHistoryEntry> history_{};
    double elapsedMs_ = 0.0;
    double lastDeniedEventMs_ = -1.0;
    std::uint64_t nextRevision_ = 1U;
};

} // namespace ri::world
