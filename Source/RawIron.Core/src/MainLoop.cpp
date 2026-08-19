#include "RawIron/Core/MainLoop.h"

#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Host.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Core/Version.h"

#include <chrono>
#include <cmath>
#include <exception>
#include <string>
#include <thread>

namespace ri::core {
namespace {

void LogShutdownFailureNoThrow(const std::string_view reason) noexcept {
    try {
        LogInfo(std::string("MainLoop: host shutdown failed during exception cleanup: ")
                + std::string(reason));
    } catch (...) {
        // Cleanup must never replace the exception that caused the main loop to unwind.
    }
}

class HostShutdownGuard final {
public:
    explicit HostShutdownGuard(Host& host) noexcept
        : host_(host) {}

    HostShutdownGuard(const HostShutdownGuard&) = delete;
    HostShutdownGuard& operator=(const HostShutdownGuard&) = delete;

    ~HostShutdownGuard() noexcept {
        if (!active_) {
            return;
        }
        active_ = false;
        try {
            host_.OnShutdown();
        } catch (const std::exception& exception) {
            LogShutdownFailureNoThrow(exception.what());
        } catch (...) {
            LogShutdownFailureNoThrow("non-standard exception");
        }
    }

    void ShutdownNow() {
        if (!active_) {
            return;
        }
        active_ = false;
        host_.OnShutdown();
    }

private:
    Host& host_;
    bool active_ = true;
};

} // namespace

int RunMainLoop(Host& host, const CommandLine& commandLine, const MainLoopOptions& options) {
    LogSection("RawIron");
    LogInfo(std::string(kEngineName) + " - " + std::string(kTagline));
    LogInfo(std::string("Host: ") + std::string(host.GetName()));
    LogInfo(std::string("Mode: ") + std::string(host.GetMode()));
    LogInfo(std::string("Args: ") + std::to_string(commandLine.Args().size()));

    // Once startup is entered, shutdown is paired with it exactly once—even if startup or a frame
    // callback throws. During unwinding, a secondary shutdown failure is diagnosed and suppressed
    // so the original failure remains observable to the caller.
    HostShutdownGuard shutdownGuard(host);
    host.OnStartup(commandLine);

    using SteadyClock = std::chrono::steady_clock;
    const auto loopStart = SteadyClock::now();
    const bool fixedDeltaOk =
        std::isfinite(options.fixedDeltaSeconds) && options.fixedDeltaSeconds > 0.0;
    const double fixedDeltaSeconds = fixedDeltaOk ? options.fixedDeltaSeconds : (1.0 / 60.0);
    if (!fixedDeltaOk) {
        LogInfo("MainLoop: fixedDeltaSeconds invalid or non-finite; using 1/60 fallback");
    }
    const auto frameDuration = std::chrono::duration<double>(fixedDeltaSeconds);
    auto nextFrameStart = loopStart;
    auto lastFrameBoundary = loopStart;

    int frameIndex = 0;
    while (options.maxFrames <= 0 || frameIndex < options.maxFrames) {
        if (options.paceToFixedDelta && frameIndex > 0) {
            nextFrameStart += std::chrono::duration_cast<SteadyClock::duration>(frameDuration);
            std::this_thread::sleep_until(nextFrameStart);
        }

        const auto now = SteadyClock::now();
        const double realtimeSeconds = std::chrono::duration<double>(now - loopStart).count();
        double realDeltaSeconds = 0.0;
        if (frameIndex > 0) {
            realDeltaSeconds = std::chrono::duration<double>(now - lastFrameBoundary).count();
        }
        if (!std::isfinite(realDeltaSeconds) || realDeltaSeconds < 0.0) {
            realDeltaSeconds = 0.0;
        }
        lastFrameBoundary = now;

        FrameContext context{};
        context.frameIndex = frameIndex;
        context.deltaSeconds = fixedDeltaSeconds;
        context.elapsedSeconds = fixedDeltaSeconds * static_cast<double>(frameIndex);
        context.realtimeSeconds = realtimeSeconds;
        context.realDeltaSeconds = realDeltaSeconds;

        if (options.verboseFrames) {
            LogInfo(std::string("Frame ") + std::to_string(frameIndex) +
                    " simDt=" + std::to_string(context.deltaSeconds) +
                    " simT=" + std::to_string(context.elapsedSeconds) +
                    " realDt=" + std::to_string(context.realDeltaSeconds) +
                    " realT=" + std::to_string(context.realtimeSeconds));
        }

        if (!host.OnFrame(context)) {
            break;
        }

        ++frameIndex;
    }

    shutdownGuard.ShutdownNow();
    return 0;
}

} // namespace ri::core
