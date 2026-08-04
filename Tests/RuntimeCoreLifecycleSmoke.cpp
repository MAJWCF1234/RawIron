#include "RawIron/Core/CommandLine.h"
#include "RawIron/Runtime/RuntimeCore.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                           \
            std::cerr << "RuntimeCoreLifecycleSmoke failed at line " << __LINE__ << ": " #condition << '\n';       \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)

struct TestService {
    int value = 42;
};

struct ModulePlan {
    std::string name;
    bool startupResult = true;
    bool failContext = false;
    bool throwOnStartup = false;
    bool throwOnShutdown = false;
    bool registerService = false;
    bool requestStopOnStartup = false;
    bool failOnFrame = false;
    bool failOnPause = false;
    bool failOnResume = false;
    int startupCalls = 0;
    int frameCalls = 0;
    int pauseCalls = 0;
    int resumeCalls = 0;
    int shutdownCalls = 0;
    ri::runtime::RuntimePhase lastFramePhase = ri::runtime::RuntimePhase::Uninitialized;
    std::vector<std::string>* trace = nullptr;
};

class ProbeModule final : public ri::runtime::RuntimeModule {
public:
    explicit ProbeModule(ModulePlan& plan) : plan_(plan) {}

    [[nodiscard]] std::string_view Name() const noexcept override {
        return plan_.name;
    }

    bool OnRuntimeStartup(ri::runtime::RuntimeContext& context,
                          const ri::core::CommandLine&) override {
        ++plan_.startupCalls;
        Record("start:");
        if (plan_.registerService) {
            (void)context.Services().Register<TestService>(std::make_shared<TestService>());
        }
        if (plan_.failContext) {
            context.Fail("deliberate startup failure");
        }
        if (plan_.requestStopOnStartup) {
            context.RequestStop("deliberate startup cancellation");
        }
        if (plan_.throwOnStartup) {
            throw std::runtime_error("deliberate startup exception");
        }
        return plan_.startupResult;
    }

    bool OnRuntimeFrame(ri::runtime::RuntimeContext& context,
                        const ri::core::FrameContext&) override {
        ++plan_.frameCalls;
        plan_.lastFramePhase = context.Phase();
        if (plan_.failOnFrame) {
            context.Fail("deliberate frame failure");
        }
        return true;
    }

    void OnRuntimePause(ri::runtime::RuntimeContext& context) override {
        ++plan_.pauseCalls;
        if (plan_.failOnPause) {
            context.Fail("deliberate pause failure");
        }
    }

    void OnRuntimeResume(ri::runtime::RuntimeContext& context) override {
        ++plan_.resumeCalls;
        if (plan_.failOnResume) {
            context.Fail("deliberate resume failure");
        }
    }

    void OnRuntimeShutdown(ri::runtime::RuntimeContext&) override {
        ++plan_.shutdownCalls;
        Record("stop:");
        if (plan_.throwOnShutdown) {
            throw std::runtime_error("deliberate shutdown exception");
        }
    }

private:
    void Record(const std::string_view prefix) const {
        if (plan_.trace != nullptr) {
            plan_.trace->push_back(std::string(prefix) + plan_.name);
        }
    }

    ModulePlan& plan_;
};

ri::core::CommandLine MakeCommandLine() {
    static char executable[] = "RuntimeCoreLifecycleSmoke";
    static char* argv[] = {executable};
    return ri::core::CommandLine(1, argv);
}

ri::runtime::RuntimeCore MakeRuntime() {
    return ri::runtime::RuntimeCore(
        ri::runtime::RuntimeIdentity{.id = "lifecycle-smoke", .displayName = "Lifecycle Smoke"});
}

bool FailedStartupIsTransactional() {
    std::vector<std::string> trace;
    ModulePlan first{.name = "first", .registerService = true, .trace = &trace};
    ModulePlan failing{
        .name = "failing",
        .startupResult = false,
        .failContext = true,
        .trace = &trace,
    };
    ModulePlan neverStarted{.name = "never-started", .trace = &trace};

    ri::runtime::RuntimeCore core = MakeRuntime();
    const auto hostService = std::make_shared<TestService>();
    hostService->value = 7;
    CHECK(core.Context().Services().Register<TestService>(hostService));
    core.AddModule(std::make_unique<ProbeModule>(first));
    core.AddModule(std::make_unique<ProbeModule>(failing));
    core.AddModule(std::make_unique<ProbeModule>(neverStarted));

    const ri::core::CommandLine commandLine = MakeCommandLine();
    CHECK(!core.Startup(commandLine));
    CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Failed);
    CHECK(core.Context().FailureReason() == "deliberate startup failure");
    CHECK(core.Context().Services().Count() == 1U);
    CHECK(core.Context().Services().Resolve<TestService>() == hostService);
    CHECK(first.startupCalls == 1 && first.shutdownCalls == 1);
    CHECK(failing.startupCalls == 1 && failing.shutdownCalls == 1);
    CHECK(neverStarted.startupCalls == 0 && neverStarted.shutdownCalls == 0);
    CHECK(trace == std::vector<std::string>({"start:first", "start:failing", "stop:failing", "stop:first"}));

    core.Shutdown();
    CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Stopped);
    CHECK(core.Context().Services().Resolve<TestService>() == hostService);
    CHECK(first.shutdownCalls == 1 && failing.shutdownCalls == 1 && neverStarted.shutdownCalls == 0);
    return true;
}

bool SuccessfulShutdownAndRestartAreExactOnce() {
    std::vector<std::string> trace;
    ModulePlan first{.name = "first", .registerService = true, .trace = &trace};
    ModulePlan second{.name = "second", .trace = &trace};
    ModulePlan addedAfterStop{.name = "added-after-stop", .trace = &trace};
    ModulePlan rejectedWhileRunning{.name = "rejected", .trace = &trace};

    ri::runtime::RuntimeCore core = MakeRuntime();
    core.AddModule(std::make_unique<ProbeModule>(first));
    core.AddModule(std::make_unique<ProbeModule>(second));
    const ri::core::CommandLine commandLine = MakeCommandLine();

    CHECK(core.Startup(commandLine));
    CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Running);
    CHECK(core.Context().Services().Contains<TestService>());
    CHECK(!core.TryAddModule(std::make_unique<ProbeModule>(rejectedWhileRunning)));
    CHECK(core.ModuleCount() == 2U);
    CHECK(core.Frame({.frameIndex = 1, .deltaSeconds = 1.0 / 60.0, .elapsedSeconds = 1.0 / 60.0}));
    CHECK(first.frameCalls == 1 && second.frameCalls == 1);

    core.Shutdown();
    CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Stopped);
    CHECK(core.Context().Services().Count() == 0U);
    CHECK(first.shutdownCalls == 1 && second.shutdownCalls == 1);
    CHECK(trace == std::vector<std::string>({"start:first", "start:second", "stop:second", "stop:first"}));

    core.Shutdown();
    CHECK(first.shutdownCalls == 1 && second.shutdownCalls == 1);
    CHECK(core.TryAddModule(std::make_unique<ProbeModule>(addedAfterStop)));
    CHECK(core.ModuleCount() == 3U);

    trace.clear();
    CHECK(core.Startup(commandLine));
    CHECK(first.startupCalls == 2 && second.startupCalls == 2 && addedAfterStop.startupCalls == 1);
    core.Shutdown();
    CHECK(first.shutdownCalls == 2 && second.shutdownCalls == 2 && addedAfterStop.shutdownCalls == 1);
    CHECK(trace == std::vector<std::string>({
        "start:first", "start:second", "start:added-after-stop",
        "stop:added-after-stop", "stop:second", "stop:first",
    }));
    return true;
}

bool StartupExceptionCleansAttemptedModule() {
    std::vector<std::string> trace;
    ModulePlan first{.name = "first", .trace = &trace};
    ModulePlan throwing{.name = "throwing-startup", .throwOnStartup = true, .trace = &trace};

    ri::runtime::RuntimeCore core = MakeRuntime();
    core.AddModule(std::make_unique<ProbeModule>(first));
    core.AddModule(std::make_unique<ProbeModule>(throwing));
    const ri::core::CommandLine commandLine = MakeCommandLine();

    CHECK(!core.Startup(commandLine));
    CHECK(core.Context().FailureReason().find("deliberate startup exception") != std::string_view::npos);
    CHECK(first.shutdownCalls == 1 && throwing.shutdownCalls == 1);
    CHECK(trace == std::vector<std::string>({
        "start:first", "start:throwing-startup", "stop:throwing-startup", "stop:first",
    }));
    core.Shutdown();
    CHECK(first.shutdownCalls == 1 && throwing.shutdownCalls == 1);
    return true;
}

bool ThrowingShutdownStillCleansEveryModuleOnce() {
    std::vector<std::string> trace;
    ModulePlan first{.name = "first", .registerService = true, .trace = &trace};
    ModulePlan throwingA{.name = "throwing-shutdown-a", .throwOnShutdown = true, .trace = &trace};
    ModulePlan throwingB{.name = "throwing-shutdown-b", .throwOnShutdown = true, .trace = &trace};
    ModulePlan last{.name = "last", .trace = &trace};

    ri::runtime::RuntimeCore core = MakeRuntime();
    core.AddModule(std::make_unique<ProbeModule>(first));
    core.AddModule(std::make_unique<ProbeModule>(throwingA));
    core.AddModule(std::make_unique<ProbeModule>(throwingB));
    core.AddModule(std::make_unique<ProbeModule>(last));
    const ri::core::CommandLine commandLine = MakeCommandLine();

    CHECK(core.Startup(commandLine));
    trace.clear();
    core.Shutdown();
    CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Stopped);
    CHECK(core.Context().Services().Count() == 0U);
    CHECK(first.shutdownCalls == 1 && throwingA.shutdownCalls == 1
          && throwingB.shutdownCalls == 1 && last.shutdownCalls == 1);
    CHECK(core.Context().FailureReason().find("throwing-shutdown-a") != std::string_view::npos);
    CHECK(core.Context().FailureReason().find("throwing-shutdown-b") != std::string_view::npos);
    CHECK(core.Context().FailureReason().find("deliberate shutdown exception") != std::string_view::npos);
    CHECK(trace == std::vector<std::string>({
        "stop:last", "stop:throwing-shutdown-b", "stop:throwing-shutdown-a", "stop:first",
    }));

    core.Shutdown();
    CHECK(first.shutdownCalls == 1 && throwingA.shutdownCalls == 1
          && throwingB.shutdownCalls == 1 && last.shutdownCalls == 1);
    return true;
}

bool GracefulStartupCancellationIsNotAFailure() {
    ModulePlan cancelling{
        .name = "cancelling",
        .requestStopOnStartup = true,
    };
    ModulePlan neverStarted{.name = "never-started"};
    ri::runtime::RuntimeCore core = MakeRuntime();
    core.AddModule(std::make_unique<ProbeModule>(cancelling));
    core.AddModule(std::make_unique<ProbeModule>(neverStarted));
    const ri::core::CommandLine commandLine = MakeCommandLine();

    CHECK(!core.Startup(commandLine));
    CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Stopped);
    CHECK(core.Context().FailureReason().empty());
    CHECK(core.Context().StopReason() == "deliberate startup cancellation");
    CHECK(cancelling.startupCalls == 1 && cancelling.shutdownCalls == 1);
    CHECK(neverStarted.startupCalls == 0 && neverStarted.shutdownCalls == 0);
    return true;
}

bool ReentrantLifecycleCallsCannotCorruptTheOuterOperation() {
    ModulePlan module{.name = "reentrant-probe"};
    ri::runtime::RuntimeCore core = MakeRuntime();
    core.AddModule(std::make_unique<ProbeModule>(module));
    const ri::core::CommandLine commandLine = MakeCommandLine();

    bool attemptedRestart = false;
    bool restartResult = true;
    const auto startedListener = core.Context().Events().On(
        "runtime.started",
        [&core](const ri::runtime::RuntimeEvent&) { core.Shutdown(); });
    const auto frameListener = core.Context().Events().On(
        "runtime.frame",
        [&core](const ri::runtime::RuntimeEvent&) { core.Shutdown(); });
    const auto stoppedListener = core.Context().Events().On(
        "runtime.phase",
        [&core, &commandLine, &attemptedRestart, &restartResult](const ri::runtime::RuntimeEvent& event) {
            const auto found = event.fields.find("to");
            if (found != event.fields.end() && found->second == "stopped") {
                attemptedRestart = true;
                restartResult = core.Startup(commandLine);
            }
        });
    CHECK(startedListener != ri::runtime::RuntimeEventBus::kInvalidListenerId);
    CHECK(frameListener != ri::runtime::RuntimeEventBus::kInvalidListenerId);
    CHECK(stoppedListener != ri::runtime::RuntimeEventBus::kInvalidListenerId);

    CHECK(core.Startup(commandLine));
    CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Running);
    CHECK(module.startupCalls == 1 && module.shutdownCalls == 0);
    CHECK(core.Frame({.frameIndex = 1, .deltaSeconds = 1.0 / 60.0, .elapsedSeconds = 1.0 / 60.0}));
    CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Running);
    CHECK(module.frameCalls == 1 && module.shutdownCalls == 0);

    core.Shutdown();
    CHECK(attemptedRestart && !restartResult);
    CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Stopped);
    CHECK(module.shutdownCalls == 1);

    // Once the outer shutdown has returned, a new activation is valid and independently cleaned.
    CHECK(core.Startup(commandLine));
    CHECK(module.startupCalls == 2 && module.shutdownCalls == 1);
    core.Shutdown();
    CHECK(module.shutdownCalls == 2);
    return true;
}

bool DestructorCleansAnActiveRuntime() {
    ModulePlan module{.name = "destructor-probe"};
    const ri::core::CommandLine commandLine = MakeCommandLine();
    {
        ri::runtime::RuntimeCore core = MakeRuntime();
        core.AddModule(std::make_unique<ProbeModule>(module));
        CHECK(core.Startup(commandLine));
        CHECK(module.shutdownCalls == 0);
    }
    CHECK(module.shutdownCalls == 1);
    return true;
}

bool CallbackFailuresEnterFailedPhaseAndStopFanout() {
    const ri::core::CommandLine commandLine = MakeCommandLine();

    {
        ModulePlan failing{.name = "frame-failure", .failOnFrame = true};
        ModulePlan skipped{.name = "frame-skipped"};
        ri::runtime::RuntimeCore core = MakeRuntime();
        core.AddModule(std::make_unique<ProbeModule>(failing));
        core.AddModule(std::make_unique<ProbeModule>(skipped));
        CHECK(core.Startup(commandLine));
        CHECK(!core.Frame({.frameIndex = 1, .deltaSeconds = 1.0 / 60.0}));
        CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Failed);
        CHECK(core.Context().FailureReason() == "deliberate frame failure");
        CHECK(failing.frameCalls == 1 && skipped.frameCalls == 0);
        core.Shutdown();
        CHECK(failing.shutdownCalls == 1 && skipped.shutdownCalls == 1);
    }

    {
        ModulePlan failing{.name = "pause-failure", .failOnPause = true};
        ModulePlan skipped{.name = "pause-skipped"};
        ri::runtime::RuntimeCore core = MakeRuntime();
        core.AddModule(std::make_unique<ProbeModule>(failing));
        core.AddModule(std::make_unique<ProbeModule>(skipped));
        CHECK(core.Startup(commandLine));
        CHECK(!core.Pause("failure probe"));
        CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Failed);
        CHECK(core.Context().FailureReason() == "deliberate pause failure");
        CHECK(failing.pauseCalls == 1 && skipped.pauseCalls == 0);
        core.Shutdown();
        CHECK(failing.shutdownCalls == 1 && skipped.shutdownCalls == 1);
    }

    {
        ModulePlan failing{.name = "resume-failure", .failOnResume = true};
        ModulePlan skipped{.name = "resume-skipped"};
        ri::runtime::RuntimeCore core = MakeRuntime();
        core.AddModule(std::make_unique<ProbeModule>(failing));
        core.AddModule(std::make_unique<ProbeModule>(skipped));
        CHECK(core.Startup(commandLine));
        CHECK(core.Pause("resume failure probe"));
        CHECK(!core.Resume());
        CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Failed);
        CHECK(core.Context().FailureReason() == "deliberate resume failure");
        CHECK(failing.resumeCalls == 1 && skipped.resumeCalls == 0);
        core.Shutdown();
        CHECK(failing.shutdownCalls == 1 && skipped.shutdownCalls == 1);
    }

    return true;
}

bool ActiveMovesAreRejectedWithoutMutation() {
    ModulePlan active{.name = "active-move-probe"};
    ModulePlan inactiveSource{.name = "inactive-move-source"};
    ri::runtime::RuntimeCore core = MakeRuntime();
    ri::runtime::RuntimeCore source = MakeRuntime();
    core.AddModule(std::make_unique<ProbeModule>(active));
    source.AddModule(std::make_unique<ProbeModule>(inactiveSource));
    const ri::core::CommandLine commandLine = MakeCommandLine();

    bool constructorRejected = false;
    bool assignmentRejected = false;
    const auto listener = core.Context().Events().On(
        "runtime.frame",
        [&core, &source, &constructorRejected, &assignmentRejected](const ri::runtime::RuntimeEvent&) {
            try {
                ri::runtime::RuntimeCore stolen(std::move(core));
            } catch (const std::logic_error&) {
                constructorRejected = true;
            }
            try {
                core = std::move(source);
            } catch (const std::logic_error&) {
                assignmentRejected = true;
            }
        });
    CHECK(listener != ri::runtime::RuntimeEventBus::kInvalidListenerId);

    CHECK(core.Startup(commandLine));
    CHECK(core.Frame({.frameIndex = 1, .deltaSeconds = 1.0 / 60.0}));
    CHECK(constructorRejected && assignmentRejected);
    CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Running);
    CHECK(core.ModuleCount() == 1U);
    CHECK(active.frameCalls == 1);
    CHECK(active.lastFramePhase == ri::runtime::RuntimePhase::Running);
    CHECK(source.Context().Phase() == ri::runtime::RuntimePhase::Uninitialized);
    CHECK(source.ModuleCount() == 1U);
    CHECK(inactiveSource.startupCalls == 0 && inactiveSource.shutdownCalls == 0);

    core.Shutdown();
    CHECK(active.shutdownCalls == 1);

    // Once both sides are inactive, assignment is allowed and transfers the unstarted module set.
    core = std::move(source);
    CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Uninitialized);
    CHECK(core.ModuleCount() == 1U);
    CHECK(source.Context().Phase() == ri::runtime::RuntimePhase::Stopped);
    CHECK(source.ModuleCount() == 0U);
    CHECK(core.Startup(commandLine));
    core.Shutdown();
    CHECK(inactiveSource.startupCalls == 1 && inactiveSource.shutdownCalls == 1);
    return true;
}

bool PausedSignalsAndStopEventFailuresArePreserved() {
    const ri::core::CommandLine commandLine = MakeCommandLine();

    {
        ModulePlan module{.name = "paused-failure-probe"};
        ri::runtime::RuntimeCore core = MakeRuntime();
        core.AddModule(std::make_unique<ProbeModule>(module));
        CHECK(core.Startup(commandLine));
        CHECK(core.Pause("paused failure"));
        core.Context().Fail("failure recorded while paused");
        CHECK(!core.Resume());
        CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Failed);
        CHECK(core.Context().StopRequested());
        CHECK(core.Context().FailureReason() == "failure recorded while paused");
        core.Shutdown();
        CHECK(module.shutdownCalls == 1);
    }

    {
        ModulePlan module{.name = "paused-stop-probe"};
        ri::runtime::RuntimeCore core = MakeRuntime();
        core.AddModule(std::make_unique<ProbeModule>(module));
        CHECK(core.Startup(commandLine));
        CHECK(core.Pause("paused stop"));
        core.Context().RequestStop("stop recorded while paused");
        CHECK(!core.Resume());
        CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Paused);
        CHECK(core.Context().StopRequested());
        CHECK(core.Context().StopReason() == "stop recorded while paused");
        core.Shutdown();
        CHECK(module.shutdownCalls == 1);
    }

    {
        ModulePlan module{.name = "stop-event-failure-probe"};
        ri::runtime::RuntimeCore core = MakeRuntime();
        core.AddModule(std::make_unique<ProbeModule>(module));
        CHECK(core.Context().Events().On(
            "runtime.frame",
            [&core](const ri::runtime::RuntimeEvent&) { core.Context().RequestStop("graceful frame stop"); })
            != ri::runtime::RuntimeEventBus::kInvalidListenerId);
        CHECK(core.Context().Events().On(
            "runtime.stop_requested",
            [&core](const ri::runtime::RuntimeEvent&) { core.Context().Fail("stop event failure"); })
            != ri::runtime::RuntimeEventBus::kInvalidListenerId);
        CHECK(core.Startup(commandLine));
        CHECK(!core.Frame({.frameIndex = 1, .deltaSeconds = 1.0 / 60.0}));
        CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Failed);
        CHECK(core.Context().FailureReason() == "stop event failure");
        CHECK(module.frameCalls == 0);
        core.Shutdown();
        CHECK(module.shutdownCalls == 1);
    }

    return true;
}

bool StartingPhaseSignalsDoNotAdvanceToLoading() {
    const ri::core::CommandLine commandLine = MakeCommandLine();

    for (const bool fail : {false, true}) {
        ModulePlan module{.name = fail ? "starting-failure-skipped" : "starting-stop-skipped"};
        ri::runtime::RuntimeCore core = MakeRuntime();
        core.AddModule(std::make_unique<ProbeModule>(module));
        std::vector<std::string> phases;
        CHECK(core.Context().Events().On(
            "runtime.phase",
            [&core, &phases, fail](const ri::runtime::RuntimeEvent& event) {
                const auto found = event.fields.find("to");
                if (found == event.fields.end()) {
                    return;
                }
                phases.push_back(found->second);
                if (found->second == "starting") {
                    if (fail) {
                        core.Context().Fail("starting phase failure");
                    } else {
                        core.Context().RequestStop("starting phase cancellation");
                    }
                }
            }) != ri::runtime::RuntimeEventBus::kInvalidListenerId);

        CHECK(!core.Startup(commandLine));
        CHECK(std::find(phases.begin(), phases.end(), "loading") == phases.end());
        CHECK(module.startupCalls == 0 && module.shutdownCalls == 0);
        if (fail) {
            CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Failed);
            CHECK(core.Context().FailureReason() == "starting phase failure");
        } else {
            CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Stopped);
            CHECK(core.Context().FailureReason().empty());
            CHECK(core.Context().StopReason() == "starting phase cancellation");
        }
    }
    return true;
}

bool PendingFailureStopsBeforeNextFrameFanout() {
    ModulePlan module{.name = "pending-frame-failure-probe"};
    ri::runtime::RuntimeCore core = MakeRuntime();
    core.AddModule(std::make_unique<ProbeModule>(module));
    const ri::core::CommandLine commandLine = MakeCommandLine();
    int frameEventCalls = 0;
    int stopEventCalls = 0;
    CHECK(core.Context().Events().On(
        "runtime.frame",
        [&frameEventCalls](const ri::runtime::RuntimeEvent&) { ++frameEventCalls; })
        != ri::runtime::RuntimeEventBus::kInvalidListenerId);
    CHECK(core.Context().Events().On(
        "runtime.stop_requested",
        [&stopEventCalls](const ri::runtime::RuntimeEvent&) { ++stopEventCalls; })
        != ri::runtime::RuntimeEventBus::kInvalidListenerId);

    CHECK(core.Startup(commandLine));
    core.Context().Fail("failure latched between frames");
    CHECK(!core.Frame({.frameIndex = 1, .deltaSeconds = 1.0 / 60.0}));
    CHECK(core.Context().Phase() == ri::runtime::RuntimePhase::Failed);
    CHECK(core.Context().FailureReason() == "failure latched between frames");
    CHECK(frameEventCalls == 0);
    CHECK(stopEventCalls == 1);
    CHECK(module.frameCalls == 0);
    core.Shutdown();
    CHECK(module.shutdownCalls == 1);
    return true;
}

} // namespace

int main() {
    if (!FailedStartupIsTransactional()
        || !SuccessfulShutdownAndRestartAreExactOnce()
        || !StartupExceptionCleansAttemptedModule()
        || !ThrowingShutdownStillCleansEveryModuleOnce()
        || !GracefulStartupCancellationIsNotAFailure()
        || !ReentrantLifecycleCallsCannotCorruptTheOuterOperation()
        || !DestructorCleansAnActiveRuntime()
        || !CallbackFailuresEnterFailedPhaseAndStopFanout()
        || !ActiveMovesAreRejectedWithoutMutation()
        || !PausedSignalsAndStopEventFailuresArePreserved()
        || !StartingPhaseSignalsDoNotAdvanceToLoading()
        || !PendingFailureStopsBeforeNextFrameFanout()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
