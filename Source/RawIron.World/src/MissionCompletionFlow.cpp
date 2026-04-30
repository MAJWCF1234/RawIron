#include "RawIron/World/MissionCompletionFlow.h"

#include "RawIron/Runtime/RuntimeId.h"

#include <cmath>
#include <sstream>

namespace ri::world {
namespace {

void Emit(ri::runtime::RuntimeEventBus* bus, std::string_view type, ri::runtime::RuntimeEvent event) {
    if (bus == nullptr) {
        return;
    }
    bus->Emit(type, std::move(event));
}

} // namespace

void ApplyAuthoritativeMissionCompletionTransition(
    ri::runtime::RuntimeEventBus* eventBus,
    const MissionCompletionTelemetry& telemetry,
    const MissionCompletionTransitionOptions& options) {
    if (!options.emitCompletionEvent && !options.emitFreezeHints) {
        return;
    }
    const std::string id = ri::runtime::CreateRuntimeId("mission");

    if (options.emitFreezeHints) {
        ri::runtime::RuntimeEvent freeze{};
        freeze.id = id + ":freeze";
        freeze.type = "simulationFreezeRequested";
        freeze.fields["reason"] = "mission_completion";
        freeze.fields["missionId"] = telemetry.missionId;
        Emit(eventBus, "simulationFreezeRequested", std::move(freeze));
    }

    if (options.emitCompletionEvent) {
        ri::runtime::RuntimeEvent complete{};
        complete.id = id;
        complete.type = "missionCompletion";
        complete.fields["missionId"] = telemetry.missionId;
        complete.fields["outcome"] = telemetry.outcome;
        std::ostringstream elapsed;
        if (std::isfinite(telemetry.missionElapsedSeconds)) {
            elapsed << telemetry.missionElapsedSeconds;
        } else {
            elapsed << '0';
        }
        complete.fields["missionElapsedSeconds"] = elapsed.str();
        if (!telemetry.detail.empty()) {
            complete.fields["detail"] = telemetry.detail;
        }
        complete.fields["audioPolicy"] = "stop_loops";
        complete.fields["uiPolicy"] = "finalize_hud";
        Emit(eventBus, "missionCompletion", std::move(complete));
    }
}

} // namespace ri::world
