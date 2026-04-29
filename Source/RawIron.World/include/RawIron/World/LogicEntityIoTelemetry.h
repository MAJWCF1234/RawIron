#pragma once

#include "RawIron/Logic/LogicGraph.h"
#include "RawIron/Logic/LogicTypes.h"
#include "RawIron/Runtime/RuntimeEventBus.h"
#include "RawIron/World/Instrumentation.h"

namespace ri::world {

/// Emit one `entityIo` event for a logic node output (maps to \ref EntityIoTracker::IncrementOutputsFired when `tracker` is non-null).
void EmitEntityIoForLogicOutput(ri::runtime::RuntimeEventBus& bus,
                                EntityIoTracker* tracker,
                                const ri::logic::LogicOutputEvent& ev);

/// Emit one `entityIo` event for a logic input dispatch (maps to \ref EntityIoTracker::IncrementInputsDispatched when `tracker` is non-null).
void EmitEntityIoForLogicInput(ri::runtime::RuntimeEventBus& bus,
                               EntityIoTracker* tracker,
                               std::string_view targetId,
                               std::string_view inputNameNormalized,
                               const ri::logic::LogicContext& ctx);

/// Chain \ref ri::logic::LogicGraph output events and per-input dispatches onto the runtime bus using the shared field contract
/// (\ref ri::runtime::entity_io). Call once after constructing the graph; pass your existing output handler so gameplay wiring stays intact.
void AttachLogicGraphEntityIoTelemetry(ri::logic::LogicGraph& graph,
                                       ri::runtime::RuntimeEventBus& eventBus,
                                       EntityIoTracker* tracker,
                                       ri::logic::LogicGraph::OutputHandler userOutputHandler);

} // namespace ri::world
