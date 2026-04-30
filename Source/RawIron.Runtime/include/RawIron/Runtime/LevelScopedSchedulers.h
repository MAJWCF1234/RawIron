#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace ri::runtime {

using LevelScheduledCallback = std::function<void()>;

/// One-shot delayed callbacks in simulation/wall time. Call \ref Clear on level unload or session reset.
class LevelScopedTimeoutScheduler {
public:
    [[nodiscard]] std::uint64_t ScheduleAfter(double delaySeconds,
                                              LevelScheduledCallback callback,
                                              double nowSeconds);

    void Cancel(std::uint64_t token);

    /// Fires due timers (absolute schedule using \p nowSeconds).
    void Tick(double nowSeconds);

    void Clear() noexcept;

    [[nodiscard]] std::size_t PendingCount() const noexcept;

private:
    struct Entry {
        std::uint64_t id = 0;
        double fireAt = 0.0;
        LevelScheduledCallback callback{};
    };

    std::vector<Entry> entries_;
    std::uint64_t nextId_ = 1;
};

/// Repeating callbacks with pause/resume and optional drift compensation (catch-up vs anchor-to-clock).
class LevelScopedIntervalScheduler {
public:
    enum class DriftPolicy {
        Compensate,
        ResetToNow,
    };

    [[nodiscard]] std::uint64_t ScheduleEvery(double periodSeconds,
                                              LevelScheduledCallback callback,
                                              double nowSeconds,
                                              DriftPolicy drift = DriftPolicy::Compensate);

    void Cancel(std::uint64_t token);

    void SetPaused(bool paused) noexcept {
        paused_ = paused;
    }

    [[nodiscard]] bool Paused() const noexcept {
        return paused_;
    }

    void Tick(double nowSeconds, double deltaSeconds);

    void Clear() noexcept;

    [[nodiscard]] std::size_t ActiveCount() const noexcept;

private:
    struct Entry {
        std::uint64_t id = 0;
        double period = 0.0;
        double nextFire = 0.0;
        LevelScheduledCallback callback{};
        DriftPolicy drift = DriftPolicy::Compensate;
    };

    std::vector<Entry> entries_;
    std::uint64_t nextId_ = 1;
    bool paused_ = false;
};

} // namespace ri::runtime
