#pragma once

#include "RawIron/World/PresentationState.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ri::world {

enum class SignalBroadcastMode {
    Disabled,
    Minimal,
    Verbose,
};

struct SignalBroadcastPolicy {
    SignalBroadcastMode mode = SignalBroadcastMode::Verbose;
    bool allowGuidanceHints = true;
    std::size_t historyLimit = 16U;
};

struct SignalBroadcastRequest {
    std::string message;
    std::string subtitle;
    std::string guidanceHint;
    double durationMs = 4000.0;
};

struct SignalBroadcastHistoryEntry {
    std::string message;
    std::string subtitle;
    std::string guidanceHint;
    std::uint64_t revision = 0;
};

class SignalBroadcastState {
public:
    explicit SignalBroadcastState(const SignalBroadcastPolicy& policy = {});

    void SetPolicy(const SignalBroadcastPolicy& policy);
    [[nodiscard]] const SignalBroadcastPolicy& Policy() const;

    void Record(const SignalBroadcastRequest& request);
    void Advance(double elapsedMs);
    void ClearTransientState();

    [[nodiscard]] const std::optional<TimedPresentationEntry>& ActiveMessage() const;
    [[nodiscard]] const std::optional<TimedPresentationEntry>& ActiveSubtitle() const;
    [[nodiscard]] const std::optional<TimedPresentationEntry>& ActiveGuidanceHint() const;
    [[nodiscard]] const std::vector<SignalBroadcastHistoryEntry>& History() const;

private:
    void ActivateTimed(std::optional<TimedPresentationEntry>& target, std::string text, double durationMs);
    void PushHistory(std::string message, std::string subtitle, std::string guidanceHint);

    SignalBroadcastPolicy policy_{};
    std::optional<TimedPresentationEntry> activeMessage_{};
    std::optional<TimedPresentationEntry> activeSubtitle_{};
    std::optional<TimedPresentationEntry> activeGuidanceHint_{};
    std::vector<SignalBroadcastHistoryEntry> history_{};
    std::uint64_t nextRevision_ = 1U;
};

} // namespace ri::world
