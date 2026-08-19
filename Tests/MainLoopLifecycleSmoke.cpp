#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Host.h"
#include "RawIron/Core/MainLoop.h"

#include <cstdlib>
#include <stdexcept>
#include <string_view>

namespace {

struct HostPlan {
    bool throwOnStartup = false;
    int throwOnFrame = -1;
    bool throwOnShutdown = false;
    bool stopOnFirstFrame = false;
};

class RecordingHost final : public ri::core::Host {
public:
    explicit RecordingHost(HostPlan plan)
        : plan_(plan) {}

    std::string_view GetName() const noexcept override { return "MainLoopLifecycleSmoke"; }
    std::string_view GetMode() const noexcept override { return "test"; }

    void OnStartup(const ri::core::CommandLine&) override {
        ++startupCalls;
        if (plan_.throwOnStartup) {
            throw std::runtime_error("startup failure");
        }
    }

    bool OnFrame(const ri::core::FrameContext&) override {
        const int currentFrame = frameCalls++;
        if (currentFrame == plan_.throwOnFrame) {
            throw std::runtime_error("frame failure");
        }
        return !plan_.stopOnFirstFrame;
    }

    void OnShutdown() override {
        ++shutdownCalls;
        if (plan_.throwOnShutdown) {
            throw std::runtime_error("shutdown failure");
        }
    }

    int startupCalls = 0;
    int frameCalls = 0;
    int shutdownCalls = 0;

private:
    HostPlan plan_{};
};

ri::core::MainLoopOptions FastOptions() {
    ri::core::MainLoopOptions options{};
    options.maxFrames = 3;
    options.paceToFixedDelta = false;
    return options;
}

bool RunThrows(RecordingHost& host, const ri::core::CommandLine& commandLine,
               std::string_view expectedMessage) {
    try {
        (void)ri::core::RunMainLoop(host, commandLine, FastOptions());
    } catch (const std::runtime_error& exception) {
        return exception.what() == expectedMessage;
    }
    return false;
}

} // namespace

int main() {
    const ri::core::CommandLine commandLine(0, nullptr);

    RecordingHost normal({});
    if (ri::core::RunMainLoop(normal, commandLine, FastOptions()) != 0
        || normal.startupCalls != 1 || normal.frameCalls != 3 || normal.shutdownCalls != 1) {
        return EXIT_FAILURE;
    }

    RecordingHost earlyStop(HostPlan{.stopOnFirstFrame = true});
    if (ri::core::RunMainLoop(earlyStop, commandLine, FastOptions()) != 0
        || earlyStop.frameCalls != 1 || earlyStop.shutdownCalls != 1) {
        return EXIT_FAILURE;
    }

    RecordingHost startupFailure(HostPlan{.throwOnStartup = true});
    if (!RunThrows(startupFailure, commandLine, "startup failure")
        || startupFailure.startupCalls != 1 || startupFailure.frameCalls != 0
        || startupFailure.shutdownCalls != 1) {
        return EXIT_FAILURE;
    }

    RecordingHost frameFailure(HostPlan{.throwOnFrame = 1});
    if (!RunThrows(frameFailure, commandLine, "frame failure")
        || frameFailure.frameCalls != 2 || frameFailure.shutdownCalls != 1) {
        return EXIT_FAILURE;
    }

    // A cleanup failure must not hide the primary frame failure or trigger shutdown twice.
    RecordingHost doubleFailure(HostPlan{.throwOnFrame = 0, .throwOnShutdown = true});
    if (!RunThrows(doubleFailure, commandLine, "frame failure")
        || doubleFailure.frameCalls != 1 || doubleFailure.shutdownCalls != 1) {
        return EXIT_FAILURE;
    }

    // With no earlier failure, shutdown's own exception remains visible.
    RecordingHost shutdownFailure(HostPlan{.throwOnShutdown = true});
    if (!RunThrows(shutdownFailure, commandLine, "shutdown failure")
        || shutdownFailure.frameCalls != 3 || shutdownFailure.shutdownCalls != 1) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
