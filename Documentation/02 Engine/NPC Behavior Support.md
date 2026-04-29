---
tags:
  - rawiron
  - engine
  - world
  - npc
  - ai
  - developers
---

# NPC Behavior Support (Developer Map)

## Why this note exists

Native NPC-related behavior in Raw Iron is **split across two first-class state machines** in `RawIron.World`—one for **non-hostile** characters (patrol, talk, “speak once”) and one for **hostile** characters (patrol, alert, chase, attack, return). Without a single map, it is easy to assume the engine “has no NPC support” or to wire the wrong type to a character.

This page is the **developer-facing index**: what to use, what you must provide, and what is still out of scope.

## The two building blocks

| Use case | Type / class | Header | Role |
|----------|--------------|--------|------|
| **Friendly / quest / ambient NPCs** | `ri::world::NpcAgentState` | `RawIron/World/NpcAgentState.h` | Path following, interaction gating, dialogue presentation hints, capped **history** for tooling. |
| **Enemies / predators / opponents** | `ri::world::HostileCharacterAi` | `RawIron/World/HostileCharacterAi.h` | Phase machine (patrol → alert → chase → attack → return), flashlight / safe-zone hooks, melee commit signal. |

They are **intentionally separate**: merging “every NPC” into one mega-class made the prototype harder to maintain. Games may use **both** side by side (e.g. guard that switches modes is still authored as two layers or two instances at the glue layer).

## `NpcAgentState` — patrol and interaction

### Responsibilities

- **`NpcAgentDefinition`** holds authored identity, **path points**, patrol mode (**loop / ping-pong / once**), speeds, waits, interaction cooldown, **speak-once** behavior, and animation **name hints** (`pathAnimation`, `interactionAnimation`, …).
- **`AdvancePatrol`** consumes real **world positions** each frame and returns **`NpcPatrolUpdate`**: waypoint index, suggested speed, **`NpcAnimationIntent`**, and a stable **`NpcPatrolPhaseCode`** for tests and telemetry (`NpcPatrolPhaseLabel`).
- **`Interact`** returns **`NpcInteractionResult`** with **`NpcInteractionOutcomeCode`** (`NpcInteractionOutcomeLabel`) so headless runners and editors can reason about rejects (disabled, cooldown, …) vs accepted lines.

### What you still implement elsewhere

- **Physics / locomotion**: the engine emits *intent* (target point, speed, animation name); your character controller applies moves, collision, and ground alignment.
- **Line-of-sight or “can talk”**: policy flags (`NpcAgentPolicy`) enable/disable patrol or interaction; stricter rules (facing player, distance, quest flags) belong in game code before calling `Interact`.

### Reference tests

- `Tests/RawIron.EngineImport.Tests/src/TestNpcAgentState.cpp` — cooldowns, patrol pause at waypoints, speak-once repeat path, outcome codes.

## `HostileCharacterAi` — hostile phases

### Responsibilities

- **`HostileCharacterAiDefinition`** holds patrol path, speeds per phase, detection radii, vision cone (half-angle **radians**), attack windup/cooldown, leash, light-aversion timers, **yaw turn mode** (`YawShortestDeltaMode`), and animation name hints.
- **`Advance`** takes **`HostileCharacterAiFrameInput`** each tick: time, **positions**, **player visibility** (already combined: range + cone + LOS if your host did that), flashlight/safe-zone booleans.
- **`HostileCharacterAiFrameOutput`** returns phase, **horizontal displacement request**, **yaw delta**, animation hint, awareness signal, chase layer flag, and **melee hit resolved** for the host to react once.

### Critical contract

**Visibility and LOS are not computed inside `HostileCharacterAi`.** The host must fold traces, volumes, and lighting into `playerVisible`, `flashlightIlluminatesCharacter`, and safe-zone flags. The class stays **testable** and **deterministic** given the input.

### Reference tests

- `Tests/RawIron.EngineImport.Tests/src/TestHostileCharacterAi.cpp` — patrol, alert, chase, attack commit, flashlight gating, leash, forced phases, yaw math.

## Wiring NPC intent into the engine (frame loop)

Both NPC types are **pure**: they expect you to drive them once per simulation step with authoritative transforms and environment facts.

**Friendly (`NpcAgentState`)**

1. Resolve the NPC root position each tick (physics / attachment / animation root).
2. Call **`AdvancePatrol`** with `nowSeconds`, `deltaSeconds`, and current **world-space** origin; apply returned speed, facing, and **`NpcAnimationIntent`** in your mover/anim graph.
3. When the player attempts use / talk, gate with your gameplay rules (distance, quest state), then call **`Interact`**. Feed primary and repeat lines from content; branch on **`NpcInteractionOutcomeCode`** for UI and audio.

**Hostile (`HostileCharacterAi`)**

1. Trace or query visibility: fold range, FOV cone, occlusion, lighting, and safe zones into **`HostileCharacterAiFrameInput::playerVisible`** and the flashlight / repel flags.
2. Call **`Advance`**; translate **`HostileCharacterAiFrameOutput`** (displacement hint, yaw delta, phase) into controller motion and combat reactions. On **`hostMeleeHitResolved`**, apply damage once on the host side.

**Connecting to logic / triggers**

NPC code does **not** auto-fire `RuntimeEventBus` signals. When an interaction succeeds or a phase crosses a threshold your design cares about, call **`LogicGraph::DispatchInput`** / **`EmitWorldOutput`** yourself, or rely on volumes that already map into the graph (see [[Entity IO and Logic Graph]] for the `entityIo` bus contract and **`AttachLogicGraphEntityIoTelemetry`**).

## Related presentation / dialogue surfaces

Not a third “NPC brain,” but commonly paired:

- **`DialogueCueState`**, **`InteractionPromptState`**, **`PresentationState`** (see [[Library Layers]] under `RawIron.World`) — subtitle/objective-style presentation and prompts that **games** connect to interaction outcomes.

Authoring flows through **`RawIron.Content`** and runtime volumes are described in [[06 Content Assembly]] and [[02 World Systems]].

## What is **not** here yet (honest boundaries)

Raw Iron does **not** currently ship a full **navmesh follower**, **behavior tree**, **squad coordinator**, or **animation graph** inside these types. Those belong in higher-level game modules or future engine layers. The current NPC types are **state + intent** so your game layer stays thin and testable.

## Summary

- Use **`NpcAgentState`** for authored patrol + dialogue-style interaction with stable outcome codes.
- Use **`HostileCharacterAi`** for opponent phases; **feed** visibility and environment facts from your trace/volume stack.
- Treat **`RawIron.World`** as the **library home** for these APIs; see [[Library Layers]] for the full World bullet list.

## Related Notes

- [[Entity IO and Logic Graph]]
- [[02 World Systems]]
- [[Library Layers]]
- [[06 Content Assembly]]
- [[00 Engine Home]]
