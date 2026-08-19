#include "RawIron/Core/JobSystem.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace ri::core {
namespace detail {

struct JobFenceState {
    mutable std::mutex mutex;
    std::condition_variable completed;
    std::size_t pendingJobs = 0;
    bool sealed = false;
    std::exception_ptr firstFailure;
};

} // namespace detail
namespace {

thread_local JobSystem* currentJobSystem = nullptr;

[[nodiscard]] std::size_t ResolveWorkerCount(JobSystemConfig config) noexcept {
    const std::size_t maximum = config.maxWorkerCount == 0U ? 32U : config.maxWorkerCount;
    if (config.workerCount != 0U) {
        return std::clamp(config.workerCount, std::size_t{1U}, maximum);
    }
    const unsigned int hardware = std::thread::hardware_concurrency();
    const std::size_t automatic = hardware > 1U ? static_cast<std::size_t>(hardware - 1U) : 1U;
    return std::min(automatic, maximum);
}

} // namespace

JobCancelledError::JobCancelledError()
    : std::runtime_error("RawIron job was cancelled during shutdown.") {}

JobFence::JobFence()
    : state_(std::make_shared<detail::JobFenceState>()) {}

JobFence::JobFence(std::shared_ptr<detail::JobFenceState> state)
    : state_(std::move(state)) {}

bool JobFence::Ready() const noexcept {
    if (state_ == nullptr) {
        return true;
    }
    const std::scoped_lock lock(state_->mutex);
    return state_->pendingJobs == 0U;
}

std::size_t JobFence::PendingJobs() const noexcept {
    if (state_ == nullptr) {
        return 0U;
    }
    const std::scoped_lock lock(state_->mutex);
    return state_->pendingJobs;
}

JobSystem::JobSystem(JobSystemConfig config) {
    const std::size_t workerCount = ResolveWorkerCount(config);
    workers_.reserve(workerCount);
    try {
        for (std::size_t index = 0U; index < workerCount; ++index) {
            workers_.emplace_back([this] { WorkerMain(); });
        }
    } catch (...) {
        {
            const std::scoped_lock lock(queueMutex_);
            accepting_ = false;
            stopping_ = true;
        }
        queueWake_.notify_all();
        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        throw;
    }
}

JobSystem::~JobSystem() noexcept {
    try {
        (void)Shutdown(JobShutdownMode::Drain);
    } catch (...) {
        // Drain does not allocate in the normal path. Destruction remains a hard no-throw boundary.
    }
}

bool JobSystem::Submit(JobFence& fence, Job job) {
    if (!job || fence.state_ == nullptr) {
        return false;
    }

    std::unique_lock queueLock(queueMutex_);
    if (!accepting_) {
        return false;
    }
    std::unique_lock fenceLock(fence.state_->mutex);
    if (fence.state_->sealed
        || fence.state_->pendingJobs == std::numeric_limits<std::size_t>::max()) {
        return false;
    }

    // The worker cannot observe this item until queueLock is released. Push first so allocation
    // failure leaves both queue and fence counts unchanged, then publish the matching count.
    queue_.push_back(WorkItem{.job = std::move(job), .fence = fence.state_});
    ++fence.state_->pendingJobs;
    submittedJobs_.fetch_add(1U, std::memory_order_relaxed);
    fenceLock.unlock();
    queueLock.unlock();
    queueWake_.notify_one();
    return true;
}

JobFence JobSystem::DispatchRange(const std::size_t itemCount,
                                  const std::size_t grainSize,
                                  RangeJob job) {
    if (!job) {
        throw std::invalid_argument("JobSystem::DispatchRange requires a callback.");
    }
    JobFence fence;
    if (itemCount == 0U) {
        return fence;
    }

    const std::size_t grain = std::max<std::size_t>(1U, grainSize);
    const auto sharedJob = std::make_shared<RangeJob>(std::move(job));
    try {
        for (std::size_t begin = 0U; begin < itemCount;) {
            const std::size_t end = begin + std::min(grain, itemCount - begin);
            if (!Submit(fence, [sharedJob, begin, end] { (*sharedJob)(begin, end); })) {
                throw std::runtime_error("JobSystem stopped while dispatching a range.");
            }
            begin = end;
        }
    } catch (...) {
        const std::exception_ptr submissionFailure = std::current_exception();
        try {
            Wait(fence);
        } catch (...) {
            // The submission/allocation failure remains primary, but every published callback is
            // terminal before captured caller data can leave scope.
        }
        std::rethrow_exception(submissionFailure);
    }
    return fence;
}

void JobSystem::SealFence(const std::shared_ptr<detail::JobFenceState>& fence) noexcept {
    if (fence == nullptr) {
        return;
    }
    const std::scoped_lock lock(fence->mutex);
    fence->sealed = true;
}

void JobSystem::RethrowFenceFailure(const std::shared_ptr<detail::JobFenceState>& fence) {
    std::exception_ptr failure;
    {
        const std::scoped_lock lock(fence->mutex);
        failure = fence->firstFailure;
    }
    if (failure != nullptr) {
        std::rethrow_exception(failure);
    }
}

void JobSystem::Wait(JobFence& fence) {
    if (fence.state_ == nullptr) {
        return;
    }
    SealFence(fence.state_);

    if (IsWorkerThread()) {
        for (;;) {
            {
                const std::scoped_lock lock(fence.state_->mutex);
                if (fence.state_->pendingJobs == 0U) {
                    break;
                }
            }
            if (!TryExecuteOne()) {
                std::unique_lock lock(fence.state_->mutex);
                fence.state_->completed.wait_for(lock, std::chrono::milliseconds(1), [&] {
                    return fence.state_->pendingJobs == 0U;
                });
            }
        }
    } else {
        std::unique_lock lock(fence.state_->mutex);
        fence.state_->completed.wait(lock, [&] { return fence.state_->pendingJobs == 0U; });
    }
    RethrowFenceFailure(fence.state_);
}

bool JobSystem::WaitFor(JobFence& fence, const std::chrono::milliseconds timeout) {
    if (fence.state_ == nullptr) {
        return true;
    }
    SealFence(fence.state_);
    const auto deadline = std::chrono::steady_clock::now() + std::max(timeout, std::chrono::milliseconds::zero());

    for (;;) {
        bool ready = false;
        {
            const std::scoped_lock lock(fence.state_->mutex);
            ready = fence.state_->pendingJobs == 0U;
        }
        if (ready) {
            RethrowFenceFailure(fence.state_);
            return true;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        if (IsWorkerThread() && TryExecuteOne()) {
            continue;
        }
        std::unique_lock lock(fence.state_->mutex);
        if (!fence.state_->completed.wait_until(lock, deadline, [&] {
                return fence.state_->pendingJobs == 0U;
            })) {
            return false;
        }
    }
}

void JobSystem::WaitIdle() {
    const std::size_t selfAllowance = IsWorkerThread() ? 1U : 0U;
    for (;;) {
        {
            const std::scoped_lock lock(queueMutex_);
            if (queue_.empty()
                && activeJobs_.load(std::memory_order_acquire) <= selfAllowance) {
                return;
            }
        }
        if (IsWorkerThread() && TryExecuteOne()) {
            continue;
        }
        std::unique_lock lock(queueMutex_);
        idleWake_.wait_for(lock, std::chrono::milliseconds(1), [&] {
            return queue_.empty()
                && activeJobs_.load(std::memory_order_acquire) <= selfAllowance;
        });
    }
}

bool JobSystem::Shutdown(const JobShutdownMode mode) {
    if (IsWorkerThread()) {
        return false;
    }
    const std::scoped_lock shutdownLock(shutdownMutex_);

    std::deque<WorkItem> cancelled;
    const std::exception_ptr cancellation = mode == JobShutdownMode::CancelPending
        ? std::make_exception_ptr(JobCancelledError{})
        : std::exception_ptr{};
    {
        const std::scoped_lock lock(queueMutex_);
        if (workers_.empty()) {
            accepting_ = false;
            stopping_ = true;
            return true;
        }
        accepting_ = false;
        stopping_ = true;
        if (mode == JobShutdownMode::CancelPending) {
            cancelled.swap(queue_);
        }
    }

    if (!cancelled.empty()) {
        for (WorkItem& item : cancelled) {
            Complete(item.fence, cancellation, true);
        }
    }
    queueWake_.notify_all();
    idleWake_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    {
        const std::scoped_lock lock(queueMutex_);
        workers_.clear();
    }
    return true;
}

bool JobSystem::AcceptingJobs() const noexcept {
    const std::scoped_lock lock(queueMutex_);
    return accepting_;
}

bool JobSystem::IsWorkerThread() const noexcept {
    return currentJobSystem == this;
}

JobSystemMetrics JobSystem::Metrics() const noexcept {
    JobSystemMetrics metrics{};
    {
        const std::scoped_lock lock(queueMutex_);
        metrics.workerCount = workers_.size();
        metrics.queuedJobs = queue_.size();
    }
    metrics.activeJobs = activeJobs_.load(std::memory_order_relaxed);
    metrics.submittedJobs = submittedJobs_.load(std::memory_order_relaxed);
    metrics.executedJobs = executedJobs_.load(std::memory_order_relaxed);
    metrics.failedJobs = failedJobs_.load(std::memory_order_relaxed);
    metrics.cancelledJobs = cancelledJobs_.load(std::memory_order_relaxed);
    return metrics;
}

bool JobSystem::TryExecuteOne() {
    WorkItem item;
    {
        const std::scoped_lock lock(queueMutex_);
        if (queue_.empty()) {
            return false;
        }
        item = std::move(queue_.front());
        queue_.pop_front();
    }
    Execute(item);
    return true;
}

void JobSystem::WorkerMain() {
    currentJobSystem = this;
    for (;;) {
        WorkItem item;
        {
            std::unique_lock lock(queueMutex_);
            queueWake_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
            if (queue_.empty()) {
                if (stopping_) {
                    break;
                }
                continue;
            }
            item = std::move(queue_.front());
            queue_.pop_front();
        }
        Execute(item);
    }
    currentJobSystem = nullptr;
}

void JobSystem::Execute(WorkItem& item) noexcept {
    activeJobs_.fetch_add(1U, std::memory_order_acq_rel);
    std::exception_ptr failure;
    try {
        item.job();
    } catch (...) {
        failure = std::current_exception();
        failedJobs_.fetch_add(1U, std::memory_order_relaxed);
    }
    executedJobs_.fetch_add(1U, std::memory_order_relaxed);
    activeJobs_.fetch_sub(1U, std::memory_order_acq_rel);
    Complete(item.fence, failure, false);
}

void JobSystem::Complete(const std::shared_ptr<detail::JobFenceState>& fence,
                         const std::exception_ptr& failure,
                         const bool cancelled) noexcept {
    if (cancelled) {
        cancelledJobs_.fetch_add(1U, std::memory_order_relaxed);
    }
    if (fence != nullptr) {
        bool becameReady = false;
        {
            const std::scoped_lock lock(fence->mutex);
            if (failure != nullptr && fence->firstFailure == nullptr) {
                fence->firstFailure = failure;
            }
            if (fence->pendingJobs > 0U) {
                --fence->pendingJobs;
                becameReady = fence->pendingJobs == 0U;
            }
        }
        if (becameReady) {
            fence->completed.notify_all();
        }
    }
    idleWake_.notify_all();
}

} // namespace ri::core
