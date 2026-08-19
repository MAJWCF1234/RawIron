#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/JobSystem.h"
#include "RawIron/Runtime/JobSystemService.h"
#include "RawIron/Runtime/RuntimeCore.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

ri::core::CommandLine MakeCommandLine() {
    static char arg0[] = "JobSystemServiceSmoke";
    static char arg1[] = "--job-workers";
    static char arg2[] = "1";
    static char* argv[] = {arg0, arg1, arg2};
    return ri::core::CommandLine(3, argv);
}

ri::runtime::RuntimeCore MakeRuntime() {
    return ri::runtime::RuntimeCore({
        .id = "job-service-smoke",
        .displayName = "Job Service Smoke",
        .mode = "test",
        .instanceId = "job-service-smoke-1",
    });
}

} // namespace

int main() {
    const ri::core::CommandLine commandLine = MakeCommandLine();
    ri::runtime::RuntimeCore runtime = MakeRuntime();
    runtime.AddDefaultModules();
    const std::vector<std::string> names = runtime.ModuleNames();
    if (names.size() != 3U || names.front() != "Jobs"
        || std::find(names.begin(), names.end(), "HostInput") == names.end()
        || !runtime.Startup(commandLine)) {
        return EXIT_FAILURE;
    }

    ri::core::JobSystem* jobs = ri::runtime::TryGetJobSystem(runtime.Context());
    if (jobs == nullptr || jobs->Metrics().workerCount != 1U
        || &ri::runtime::GetJobSystem(runtime.Context()) != jobs) {
        return EXIT_FAILURE;
    }
    std::atomic<int> calls{0};
    ri::core::JobFence fence = jobs->DispatchRange(1'000U, 31U,
        [&](const std::size_t begin, const std::size_t end) {
            calls.fetch_add(static_cast<int>(end - begin), std::memory_order_relaxed);
        });
    jobs->Wait(fence);
    if (calls.load(std::memory_order_relaxed) != 1'000) {
        return EXIT_FAILURE;
    }
    runtime.Shutdown();
    if (ri::runtime::TryGetJobSystem(runtime.Context()) != nullptr) {
        return EXIT_FAILURE;
    }

    // A host-injected pool is borrowed, not shut down or removed by the default module.
    ri::runtime::RuntimeCore injectedRuntime = MakeRuntime();
    const auto injected = std::make_shared<ri::core::JobSystem>(
        ri::core::JobSystemConfig{.workerCount = 1U, .maxWorkerCount = 1U});
    if (!injectedRuntime.Context().Services().Register<ri::core::JobSystem>(injected)) {
        return EXIT_FAILURE;
    }
    injectedRuntime.AddDefaultModules();
    if (!injectedRuntime.Startup(commandLine)
        || ri::runtime::TryGetJobSystem(injectedRuntime.Context()) != injected.get()) {
        return EXIT_FAILURE;
    }
    injectedRuntime.Shutdown();
    if (ri::runtime::TryGetJobSystem(injectedRuntime.Context()) != injected.get()
        || !injected->AcceptingJobs()) {
        return EXIT_FAILURE;
    }
    return injected->Shutdown() ? EXIT_SUCCESS : EXIT_FAILURE;
}
