#include "RawIron/Core/JobSystem.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace {

bool BasicDispatchAndFailureIsolation() {
    ri::core::JobSystem jobs({.workerCount = 2U, .maxWorkerCount = 2U});
    ri::core::JobFence fence;
    std::atomic<std::uint64_t> sum{0U};
    constexpr std::uint64_t kCount = 20'000U;
    for (std::uint64_t value = 1U; value <= kCount; ++value) {
        if (!jobs.Submit(fence, [&sum, value] {
                sum.fetch_add(value, std::memory_order_relaxed);
            })) {
            return false;
        }
    }
    jobs.Wait(fence);
    if (sum.load(std::memory_order_relaxed) != (kCount * (kCount + 1U)) / 2U
        || !fence.Ready() || fence.PendingJobs() != 0U
        || jobs.Submit(fence, [] {})) {
        return false;
    }

    ri::core::JobFence failing;
    std::atomic<int> survivors{0};
    if (!jobs.Submit(failing, [] { throw std::runtime_error("deliberate job failure"); })
        || !jobs.Submit(failing, [&] { survivors.fetch_add(1, std::memory_order_relaxed); })
        || !jobs.Submit(failing, [&] { survivors.fetch_add(1, std::memory_order_relaxed); })) {
        return false;
    }
    bool sawFailure = false;
    try {
        jobs.Wait(failing);
    } catch (const std::runtime_error& exception) {
        sawFailure = std::string_view(exception.what()) == "deliberate job failure";
    }
    const ri::core::JobSystemMetrics metrics = jobs.Metrics();
    return sawFailure && survivors.load(std::memory_order_relaxed) == 2
        && metrics.workerCount == 2U && metrics.submittedJobs == kCount + 3U
        && metrics.executedJobs == kCount + 3U && metrics.failedJobs == 1U
        && metrics.cancelledJobs == 0U && metrics.queuedJobs == 0U && metrics.activeJobs == 0U;
}

bool NestedWaitAndRangeDispatch() {
    // One worker is intentional: a conventional blocking pool deadlocks here. RawIron workers help
    // execute their own queue while waiting on a child fence.
    ri::core::JobSystem jobs({.workerCount = 1U, .maxWorkerCount = 1U});
    ri::core::JobFence parent;
    std::atomic<int> children{0};
    std::atomic<bool> nestedWaitCompleted{false};
    if (!jobs.Submit(parent, [&] {
            ri::core::JobFence child;
            for (int index = 0; index < 1'000; ++index) {
                if (!jobs.Submit(child, [&] { children.fetch_add(1, std::memory_order_relaxed); })) {
                    throw std::runtime_error("nested submit failed");
                }
            }
            jobs.Wait(child);
            nestedWaitCompleted.store(true, std::memory_order_release);
        })) {
        return false;
    }
    jobs.Wait(parent);
    if (!nestedWaitCompleted.load(std::memory_order_acquire)
        || children.load(std::memory_order_relaxed) != 1'000) {
        return false;
    }

    std::atomic<std::uint64_t> rangeSum{0U};
    ri::core::JobFence range = jobs.DispatchRange(50'003U, 127U, [&](const std::size_t begin,
                                                                    const std::size_t end) {
        std::uint64_t local = 0U;
        for (std::size_t index = begin; index < end; ++index) {
            local += static_cast<std::uint64_t>(index);
        }
        rangeSum.fetch_add(local, std::memory_order_relaxed);
    });
    jobs.Wait(range);
    constexpr std::uint64_t kItems = 50'003U;
    return rangeSum.load(std::memory_order_relaxed) == (kItems * (kItems - 1U)) / 2U;
}

bool TimeoutAndCancelAreTerminal() {
    ri::core::JobSystem jobs({.workerCount = 1U, .maxWorkerCount = 1U});
    std::mutex gateMutex;
    std::condition_variable gateChanged;
    bool blockerStarted = false;
    bool releaseBlocker = false;

    ri::core::JobFence blocker;
    if (!jobs.Submit(blocker, [&] {
            std::unique_lock lock(gateMutex);
            blockerStarted = true;
            gateChanged.notify_all();
            gateChanged.wait(lock, [&] { return releaseBlocker; });
        })) {
        return false;
    }
    {
        std::unique_lock lock(gateMutex);
        gateChanged.wait(lock, [&] { return blockerStarted; });
    }
    if (jobs.WaitFor(blocker, std::chrono::milliseconds(1))) {
        return false;
    }

    ri::core::JobFence cancelled;
    std::atomic<int> unexpectedlyRan{0};
    constexpr int kCancelledJobs = 512;
    for (int index = 0; index < kCancelledJobs; ++index) {
        if (!jobs.Submit(cancelled, [&] { unexpectedlyRan.fetch_add(1, std::memory_order_relaxed); })) {
            return false;
        }
    }

    bool shutdownResult = false;
    std::thread shutdownThread([&] {
        shutdownResult = jobs.Shutdown(ri::core::JobShutdownMode::CancelPending);
    });
    while (jobs.AcceptingJobs()) {
        std::this_thread::yield();
    }
    {
        const std::scoped_lock lock(gateMutex);
        releaseBlocker = true;
    }
    gateChanged.notify_all();
    shutdownThread.join();
    jobs.Wait(blocker);

    bool sawCancellation = false;
    try {
        jobs.Wait(cancelled);
    } catch (const ri::core::JobCancelledError&) {
        sawCancellation = true;
    }
    const ri::core::JobSystemMetrics metrics = jobs.Metrics();
    return shutdownResult && sawCancellation && unexpectedlyRan.load(std::memory_order_relaxed) == 0
        && metrics.cancelledJobs == static_cast<std::uint64_t>(kCancelledJobs)
        && metrics.executedJobs == 1U && !jobs.AcceptingJobs()
        && !jobs.Submit(cancelled, [] {});
}

bool WorkerCannotSelfJoin() {
    ri::core::JobSystem jobs({.workerCount = 1U, .maxWorkerCount = 1U});
    ri::core::JobFence fence;
    std::atomic<bool> rejected{false};
    if (!jobs.Submit(fence, [&] {
            rejected.store(!jobs.Shutdown(), std::memory_order_release);
        })) {
        return false;
    }
    jobs.Wait(fence);
    return rejected.load(std::memory_order_acquire) && jobs.Shutdown();
}

} // namespace

int main() {
    return BasicDispatchAndFailureIsolation()
            && NestedWaitAndRangeDispatch()
            && TimeoutAndCancelAreTerminal()
            && WorkerCannotSelfJoin()
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
