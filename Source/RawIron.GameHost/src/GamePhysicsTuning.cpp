#include "RawIron/Games/GamePhysicsTuning.h"

#include "RawIron/Core/Log.h"

#include <algorithm>

namespace ri::games {

ri::trace::MovementControllerOptions ResolveGamePhysicsTuning(
    ri::trace::MovementControllerOptions authored,
    const ri::content::ScriptScalarMap& physics) {
    using ri::content::ScriptScalarOrClamped;
    authored.gravity = ScriptScalarOrClamped(physics, "movement_gravity", authored.gravity, 1.0f, 64.0f)
        * ScriptScalarOrClamped(physics, "global_gravity_scale", 1.0f, 0.1f, 4.0f);
    authored.fallGravityMultiplier = ScriptScalarOrClamped(
        physics, "movement_fall_gravity_multiplier", authored.fallGravityMultiplier, 0.5f, 4.0f);
    authored.groundFriction = std::max(0.0f, authored.groundFriction)
        * ScriptScalarOrClamped(physics, "global_drag_scale", 1.0f, 0.1f, 4.0f);
    authored.jumpSpeed = std::max(0.0f, authored.jumpSpeed)
        * ScriptScalarOrClamped(physics, "global_jump_scale", 1.0f, 0.65f, 1.35f);
    authored.airControl = std::clamp(authored.airControl
        * ScriptScalarOrClamped(physics, "global_air_control_scale", 1.0f, 0.75f, 1.35f), 0.0f, 1.0f);
    ri::core::LogInfo("Applied movement contract: gravity=" + std::to_string(authored.gravity)
        + " jump=" + std::to_string(authored.jumpSpeed)
        + " groundFriction=" + std::to_string(authored.groundFriction)
        + " airControl=" + std::to_string(authored.airControl));
    return authored;
}

} // namespace ri::games
