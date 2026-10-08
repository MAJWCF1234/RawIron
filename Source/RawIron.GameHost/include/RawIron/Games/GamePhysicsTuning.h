#pragma once

#include "RawIron/Content/ScriptScalars.h"
#include "RawIron/Trace/MovementController.h"

namespace ri::games {

/// Resolve once at startup, before retaining the authored movement baseline.
/// Local volume modifiers compose with this baseline during simulation.
[[nodiscard]] ri::trace::MovementControllerOptions ResolveGamePhysicsTuning(
    ri::trace::MovementControllerOptions authored,
    const ri::content::ScriptScalarMap& physics);

} // namespace ri::games
