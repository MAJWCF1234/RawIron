#pragma once

#include "RawIron/Runtime/RuntimeEventBus.h"

#include <string>

namespace ri::world {

struct MissionCompletionTelemetry {
    std::string missionId{};
    std::string outcome{"success"};
    double missionElapsedSeconds = 0.0;
    std::string detail{};
};

struct MissionCompletionTransitionOptions {
    bool emitCompletionEvent = true;
    bool emitFreezeHints = true;
};

/// Terminal gameflow hook: single entry point for freezing simulation intent, UI/audio bookkeeping hints,
/// and analytics/replay-oriented telemetry (via \ref ri::runtime::RuntimeEventBus).
void ApplyAuthoritativeMissionCompletionTransition(
    ri::runtime::RuntimeEventBus* eventBus,
    const MissionCompletionTelemetry& telemetry,
    const MissionCompletionTransitionOptions& options = {});

} // namespace ri::world
