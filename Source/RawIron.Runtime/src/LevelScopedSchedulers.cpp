#include "RawIron/Runtime/LevelScopedSchedulers.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ri::runtime {
namespace {

[[nodiscard]] bool IsFinitePositive(double value) noexcept {
    return std::isfinite(value) && value > 0.0;
}

template <typename TEntry>
TEntry* FindEntryById(std::vector<TEntry>& entries, const std::uint64_t id) {
    const auto found = std::find_if(entries.begin(), entries.end(), [&](const TEntry& entry) {
        return entry.id == id;
    });
    return found != entries.end() ? &(*found) : nullptr;
}

} // namespace

std::uint64_t LevelScopedTimeoutScheduler::ScheduleAfter(const double delaySeconds,
                                                         LevelScheduledCallback callback,
                                                         const double nowSeconds) {
    if (callback == nullptr || !std::isfinite(nowSeconds) || !std::isfinite(delaySeconds) || delaySeconds < 0.0) {
        return 0;
    }
    const std::uint64_t id = nextId_++;
    entries_.push_back(Entry{
        .id = id,
        .fireAt = nowSeconds + delaySeconds,
        .callback = std::move(callback),
    });
    return id;
}

void LevelScopedTimeoutScheduler::Cancel(const std::uint64_t token) {
    if (token == 0) {
        return;
    }
    const auto remove = std::remove_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.id == token;
    });
    entries_.erase(remove, entries_.end());
}

void LevelScopedTimeoutScheduler::Tick(const double nowSeconds) {
    if (!std::isfinite(nowSeconds) || entries_.empty()) {
        return;
    }

    std::vector<Entry> pending;
    pending.reserve(entries_.size());
    for (Entry& entry : entries_) {
        if (nowSeconds + 1e-12 >= entry.fireAt) {
            pending.push_back(std::move(entry));
        }
    }
    const auto remove = std::remove_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return nowSeconds + 1e-12 >= entry.fireAt;
    });
    entries_.erase(remove, entries_.end());

    for (Entry& entry : pending) {
        if (entry.callback) {
            entry.callback();
        }
    }
}

void LevelScopedTimeoutScheduler::Clear() noexcept {
    entries_.clear();
}

std::size_t LevelScopedTimeoutScheduler::PendingCount() const noexcept {
    return entries_.size();
}

std::uint64_t LevelScopedIntervalScheduler::ScheduleEvery(const double periodSeconds,
                                                            LevelScheduledCallback callback,
                                                            const double nowSeconds,
                                                            const DriftPolicy drift) {
    if (callback == nullptr || !std::isfinite(nowSeconds) || !IsFinitePositive(periodSeconds)) {
        return 0;
    }
    const std::uint64_t id = nextId_++;
    entries_.push_back(Entry{
        .id = id,
        .period = periodSeconds,
        .nextFire = nowSeconds + periodSeconds,
        .callback = std::move(callback),
        .drift = drift,
    });
    return id;
}

void LevelScopedIntervalScheduler::Cancel(const std::uint64_t token) {
    if (token == 0) {
        return;
    }
    const auto remove = std::remove_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.id == token;
    });
    entries_.erase(remove, entries_.end());
}

void LevelScopedIntervalScheduler::Tick(const double nowSeconds, const double deltaSeconds) {
    (void)deltaSeconds;
    if (paused_ || entries_.empty() || !std::isfinite(nowSeconds)) {
        return;
    }

    struct PendingFire {
        std::uint64_t id = 0;
        LevelScheduledCallback callback{};
    };

    std::vector<PendingFire> pending;
    for (const Entry& entry : entries_) {
        if (!IsFinitePositive(entry.period)) {
            continue;
        }

        double nextFire = entry.nextFire;
        while (nowSeconds + 1e-12 >= nextFire) {
            pending.push_back(PendingFire{
                .id = entry.id,
                .callback = entry.callback,
            });
            if (entry.drift == DriftPolicy::Compensate) {
                nextFire += entry.period;
            } else {
                nextFire = nowSeconds + entry.period;
                break;
            }
        }

        if (Entry* liveEntry = FindEntryById(entries_, entry.id); liveEntry != nullptr) {
            liveEntry->nextFire = nextFire;
        }
    }

    for (PendingFire& fire : pending) {
        if (FindEntryById(entries_, fire.id) == nullptr) {
            continue;
        }
        if (fire.callback) {
            fire.callback();
        }
    }
}

void LevelScopedIntervalScheduler::Clear() noexcept {
    entries_.clear();
}

std::size_t LevelScopedIntervalScheduler::ActiveCount() const noexcept {
    return entries_.size();
}

} // namespace ri::runtime
