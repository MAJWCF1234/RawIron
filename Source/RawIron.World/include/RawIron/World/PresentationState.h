#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ri::world {

enum class PresentationChannel {
    Message,
    Subtitle,
    Objective,
};

enum class PresentationSeverity {
    Normal,
    Critical,
};

struct TimedPresentationEntry {
    std::string text;
    double durationMs = 0.0;
    double remainingMs = 0.0;
    PresentationSeverity severity = PresentationSeverity::Normal;
};

struct ObjectivePresentationState {
    std::string text;
    std::string hint;
    double flashRemainingMs = 0.0;
    std::uint64_t revision = 0;
};

struct PresentationHistoryEntry {
    PresentationChannel channel = PresentationChannel::Message;
    std::string text;
    PresentationSeverity severity = PresentationSeverity::Normal;
    double durationMs = 0.0;
    std::uint64_t revision = 0;
};

struct ObjectiveUpdateOptions {
    bool announce = true;
    bool flash = true;
    std::string hint;
    double hintDurationMs = 7000.0;
};

class PresentationState {
public:
    explicit PresentationState(std::size_t historyLimit = 32U);

    void ShowMessage(std::string text,
                     double durationMs = 4000.0,
                     PresentationSeverity severity = PresentationSeverity::Normal);
    void ShowSubtitle(std::string text, double durationMs = 5000.0);
    void UpdateObjective(std::string text, const ObjectiveUpdateOptions& options = {});
    void PushNarrativeMessage(std::string text, double durationMs = 4000.0, bool urgent = false);
    void TriggerUrgencyFlash(double durationMs = 600.0);
    void Advance(double elapsedMs);
    void ClearTransientState();

    [[nodiscard]] const std::optional<TimedPresentationEntry>& ActiveMessage() const;
    [[nodiscard]] const std::optional<TimedPresentationEntry>& ActiveSubtitle() const;
    [[nodiscard]] const ObjectivePresentationState& Objective() const;
    [[nodiscard]] const std::vector<PresentationHistoryEntry>& History() const;
    [[nodiscard]] double UrgencyFlashRemainingMs() const;

private:
    void PushHistory(PresentationChannel channel,
                     std::string text,
                     PresentationSeverity severity,
                     double durationMs);

    std::optional<TimedPresentationEntry> activeMessage_{};
    std::optional<TimedPresentationEntry> activeSubtitle_{};
    ObjectivePresentationState objective_{};
    double urgencyFlashRemainingMs_ = 0.0;
    std::vector<PresentationHistoryEntry> history_{};
    std::size_t historyLimit_ = 32U;
    std::uint64_t nextRevision_ = 1U;
};

} // namespace ri::world
