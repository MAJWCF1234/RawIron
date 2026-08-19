#include "RawIron/Runtime/JobSystemService.h"

#include "RawIron/Core/CommandLine.h"

#include <stdexcept>

namespace ri::runtime {

ri::core::JobSystem* TryGetJobSystem(RuntimeContext& context) noexcept {
    return context.Services().Resolve<ri::core::JobSystem>().get();
}

const ri::core::JobSystem* TryGetJobSystem(const RuntimeContext& context) noexcept {
    return context.Services().Resolve<ri::core::JobSystem>().get();
}

ri::core::JobSystem& GetJobSystem(RuntimeContext& context) {
    ri::core::JobSystem* jobs = TryGetJobSystem(context);
    if (jobs == nullptr) {
        throw std::logic_error("RawIron JobSystem service is not mounted.");
    }
    return *jobs;
}

bool JobSystemRuntimeModule::OnRuntimeStartup(RuntimeContext& context,
                                              const ri::core::CommandLine& commandLine) {
    if (context.Services().Contains<ri::core::JobSystem>()) {
        return true;
    }

    ri::core::JobSystemConfig config{};
    const int requestedWorkers = commandLine.GetIntOr("--job-workers", 0);
    if (requestedWorkers > 0) {
        config.workerCount = static_cast<std::size_t>(requestedWorkers);
    }
    // Avoid accidental thread explosions from malformed automation or config while retaining a
    // useful ceiling for high-core workstations.
    config.maxWorkerCount = 64U;
    ownedService_ = std::make_shared<ri::core::JobSystem>(config);
    if (!context.Services().Register<ri::core::JobSystem>(ownedService_)) {
        ownedService_.reset();
        return false;
    }
    return true;
}

void JobSystemRuntimeModule::OnRuntimeShutdown(RuntimeContext& context) {
    if (ownedService_ == nullptr) {
        return;
    }
    if (!ownedService_->Shutdown(ri::core::JobShutdownMode::Drain)) {
        // Keep the owning reference alive. RuntimeCore records this callback failure, and later
        // destruction/restart on the lifecycle thread can safely join the pool.
        throw std::logic_error("JobSystem runtime shutdown cannot join from one of its own workers.");
    }
    context.Services().Unregister<ri::core::JobSystem>();
    ownedService_.reset();
}

std::unique_ptr<RuntimeModule> MakeJobSystemRuntimeModule() {
    return std::make_unique<JobSystemRuntimeModule>();
}

} // namespace ri::runtime
