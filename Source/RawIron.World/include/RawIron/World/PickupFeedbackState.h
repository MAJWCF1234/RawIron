#pragma once

#include "RawIron/World/PresentationState.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ri::world {

enum class PickupFeedbackMode {
    Disabled,
    Minimal,
    Verbose,
};

enum class PickupFeedbackKind {
    PickedUp,
    AlreadyCarrying,
    Consumed,
    Unavailable,
};

struct PickupFeedbackPolicy {
    PickupFeedbackMode mode = PickupFeedbackMode::Verbose;
    bool allowObjectiveUpdates = true;
    bool allowHints = true;
    std::size_t historyLimit = 16U;
};

struct PickupFeedbackRequest {
    std::string itemId;
    std::string itemLabel;
    std::string pickupMessage;
    std::string objectiveText;
    std::string hintText;
    double messageDurationMs = 3500.0;
    double hintDurationMs = 6000.0;
};

struct PickupFeedbackHistoryEntry {
    PickupFeedbackKind kind = PickupFeedbackKind::PickedUp;
    std::string message;
    std::string objectiveText;
    std::string hintText;
    std::uint64_t revision = 0;
};

class PickupFeedbackState {
public:
    explicit PickupFeedbackState(const PickupFeedbackPolicy& policy = {});

    void SetPolicy(const PickupFeedbackPolicy& policy);
    [[nodiscard]] const PickupFeedbackPolicy& Policy() const;

    void RecordPickup(const PickupFeedbackRequest& request);
    void RecordAlreadyCarrying(std::string itemLabel, double durationMs = 2500.0);
    void RecordConsumed(std::string itemLabel, double durationMs = 2500.0);
    void RecordUnavailable(std::string label, double durationMs = 2500.0);
    void Advance(double elapsedMs);
    void ClearTransientState();

    [[nodiscard]] const std::optional<TimedPresentationEntry>& ActiveMessage() const;
    [[nodiscard]] const std::optional<TimedPresentationEntry>& ActiveHint() const;
    [[nodiscard]] std::optional<std::string> ConsumePendingObjective();
    [[nodiscard]] const std::vector<PickupFeedbackHistoryEntry>& History() const;

private:
    void ActivateMessage(std::string message, double durationMs);
    void ActivateHint(std::string hintText, double durationMs);
    void PushHistory(PickupFeedbackKind kind, std::string message, std::string objectiveText, std::string hintText);
    [[nodiscard]] std::string NormalizeLabel(std::string label) const;

    PickupFeedbackPolicy policy_{};
    std::optional<TimedPresentationEntry> activeMessage_{};
    std::optional<TimedPresentationEntry> activeHint_{};
    std::optional<std::string> pendingObjective_{};
    std::vector<PickupFeedbackHistoryEntry> history_{};
    std::uint64_t nextRevision_ = 1U;
};

} // namespace ri::world
