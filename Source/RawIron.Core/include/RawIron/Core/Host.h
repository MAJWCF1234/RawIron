#pragma once

#include <string_view>

namespace ri::core {

class CommandLine;

/// Per-frame timing passed to `Host::OnFrame`.
struct FrameContext {
    int frameIndex = 0;
    /// Logical timestep from `MainLoopOptions::fixedDeltaSeconds` (deterministic simulation clock).
    double deltaSeconds = 0.0;
    /// `frameIndex * deltaSeconds` — fixed-step timeline (matches legacy behavior).
    double elapsedSeconds = 0.0;
    /// Monotonic wall time since `OnStartup` returned (steady clock), in seconds.
    double realtimeSeconds = 0.0;
    /// Wall-clock seconds since the previous `OnFrame` (0 on the first frame).
    double realDeltaSeconds = 0.0;
};

class Host {
public:
    virtual ~Host() = default;

    [[nodiscard]] virtual std::string_view GetName() const noexcept = 0;
    [[nodiscard]] virtual std::string_view GetMode() const noexcept = 0;

    virtual void OnStartup(const CommandLine& commandLine) = 0;
    [[nodiscard]] virtual bool OnFrame(const FrameContext& frame) = 0;
    virtual void OnShutdown() = 0;
};

} // namespace ri::core
