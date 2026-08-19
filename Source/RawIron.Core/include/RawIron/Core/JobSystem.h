#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace ri::core {
namespace detail {
struct JobFenceState;
}

struct JobSystemConfig {
    /// Zero selects hardware_concurrency - 1, leaving the calling/game thread available.
    std::size_t workerCount = 0;
    /// Safety ceiling for automatic and explicit worker counts. Zero uses 32.
    std::size_t maxWorkerCount = 32;
};

struct JobSystemMetrics {
    std::size_t workerCount = 0;
    std::size_t queuedJobs = 0;
    std::size_t activeJobs = 0;
    std::uint64_t submittedJobs = 0;
    std::uint64_t executedJobs = 0;
    std::uint64_t failedJobs = 0;
    std::uint64_t cancelledJobs = 0;
};

class JobCancelledError final : public std::runtime_error {
public:
    JobCancelledError();
};

/// Completion and failure state for one submitted batch. A fence accepts jobs until its first
/// Wait/WaitFor call seals it. Copies refer to the same batch state.
class JobFence {
public:
    JobFence();

    [[nodiscard]] bool Ready() const noexcept;
    [[nodiscard]] std::size_t PendingJobs() const noexcept;

private:
    friend class JobSystem;
    explicit JobFence(std::shared_ptr<detail::JobFenceState> state);

    std::shared_ptr<detail::JobFenceState> state_;
};

enum class JobShutdownMode : std::uint8_t {
    /// Stop accepting jobs and execute everything already submitted before workers exit.
    Drain,
    /// Stop accepting jobs, cancel queued work, and wait only for work already executing.
    CancelPending,
};

/// Engine-owned general worker pool for bounded, frame-independent CPU work.
///
/// Waiting from one of this system's workers executes queued jobs while blocked. This permits
/// nested fan-out even with one worker and avoids the common worker-waits-for-worker deadlock.
class JobSystem final {
public:
    using Job = std::function<void()>;
    using RangeJob = std::function<void(std::size_t begin, std::size_t end)>;

    explicit JobSystem(JobSystemConfig config = {});
    ~JobSystem() noexcept;

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;
    JobSystem(JobSystem&&) = delete;
    JobSystem& operator=(JobSystem&&) = delete;

    /// Adds a job to `fence`. Returns false after shutdown, for an empty job, or once the fence is
    /// sealed by a wait. Allocation failures retain the strong guarantee and may throw.
    [[nodiscard]] bool Submit(JobFence& fence, Job job);

    /// Splits [0, itemCount) into cache-friendly contiguous chunks. The returned fence must be
    /// waited before data captured by `job` is released.
    [[nodiscard]] JobFence DispatchRange(std::size_t itemCount,
                                         std::size_t grainSize,
                                         RangeJob job);

    /// Seals the fence, helps execute work when called by a worker, and rethrows the first job
    /// exception after every job in the fence has reached a terminal state.
    void Wait(JobFence& fence);
    [[nodiscard]] bool WaitFor(JobFence& fence, std::chrono::milliseconds timeout);

    /// Waits for all currently submitted work. A worker caller excludes its own active callback.
    void WaitIdle();

    /// Idempotent. Returns false if invoked from one of this system's jobs because joining the
    /// current worker would deadlock; the owner must perform shutdown from its lifecycle thread.
    [[nodiscard]] bool Shutdown(JobShutdownMode mode = JobShutdownMode::Drain);

    [[nodiscard]] bool AcceptingJobs() const noexcept;
    [[nodiscard]] bool IsWorkerThread() const noexcept;
    [[nodiscard]] JobSystemMetrics Metrics() const noexcept;

private:
    struct WorkItem {
        Job job;
        std::shared_ptr<detail::JobFenceState> fence;
    };

    [[nodiscard]] bool TryExecuteOne();
    void WorkerMain();
    void Execute(WorkItem& item) noexcept;
    void Complete(const std::shared_ptr<detail::JobFenceState>& fence,
                  const std::exception_ptr& failure,
                  bool cancelled) noexcept;
    static void SealFence(const std::shared_ptr<detail::JobFenceState>& fence) noexcept;
    static void RethrowFenceFailure(const std::shared_ptr<detail::JobFenceState>& fence);

    mutable std::mutex queueMutex_;
    std::condition_variable queueWake_;
    std::condition_variable idleWake_;
    std::deque<WorkItem> queue_;
    std::vector<std::thread> workers_;
    std::mutex shutdownMutex_;
    bool accepting_ = true;
    bool stopping_ = false;

    std::atomic<std::size_t> activeJobs_{0};
    std::atomic<std::uint64_t> submittedJobs_{0};
    std::atomic<std::uint64_t> executedJobs_{0};
    std::atomic<std::uint64_t> failedJobs_{0};
    std::atomic<std::uint64_t> cancelledJobs_{0};
};

} // namespace ri::core
