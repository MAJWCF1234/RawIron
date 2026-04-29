# RawIron game format (reference)

Sample games use a versioned contract such as `rawiron-game-v1.3.7`; see each game’s `manifest.json` and README for the exact bundle.

## Core engine pillars (FPS test hall backbone)

These systems are implemented in RawIron C++ and exercised by standalone games such as **Liminal Hall** and **Wilderness Ruins**.

### World collision + traces for movement

- `**ri::trace::TraceScene`** holds axis-aligned colliders with a `**ri::spatial::SpatialIndex**` broad phase.
- Queries include **ray**, **box**, **swept box**, **slide**, and ground probes; metrics are tracked for diagnostics.
- `**ri::trace::SimulateMovementControllerStep`** drives player motion against the active `TraceScene` (grounding, step/slide, air control, optional traversal hooks).

### Level ingest → meshes + materials

- Level payload (JSON and related game files) is validated, expanded (prefabs/templates), and compiled into **structural nodes** and **scene instances**.
- `**BuildWorld`** (per game) produces a `**ri::scene::Scene**`, **player rig / camera** handles, and `**std::vector<ri::trace::TraceCollider>`** for movement.
- Materials and UV/tile data are bound through the content and rendering paths described in each game’s README (e.g. `levels/assembly.primitives.csv` with optional `texture,tileX,tileY` in Liminal Hall).

### Pointer-lock FPS controller + stance / movement

- Standalone uses **Win32 pointer clip + raw mouse** for look; **WASD** + **sprint** + **jump** map to `**ri::trace::MovementInput`**.
- `**ri::trace::MovementControllerOptions**` / `**MovementStance**` support **stand / crouch / prone** speed caps, sprint, stamina (optional), coyote time, jump buffer, and parkour-oriented options.
- **Fixed timestep**: simulation uses a clamped `dt` per step; presentation (bob, FOV) uses the same frame’s `dt` for smooth blending.

### Interactables + ray-based use

- `**RuntimeEnvironmentService::ResolveInteractionTarget`** combines **ray** and **near-radius** selection for **procedural doors** and **dynamic info panels** (distance order, stable tie-breaks).
- Standalone games call this from the **camera** with full **yaw + pitch** forward; **E** performs a **press-edge** interact and dispatches:
  - **Doors**: `TryInteractWithProceduralDoor` (access / lock / transition metadata).
  - **Info panels** (Liminal Hall + logic): `ApplyWorldActorLogicInput` when an `interactionHook` is authored.

### Environmental volumes + audio routing (area context)

- **Post-process, fog, audio reverb/occlusion, fluid volumes**: evaluated per foot position; combined state drives rendering and audio.
- `**UpdateEnvironmentalVolumesAt`** tracks **enter/exit** for those volume families for transition semantics.
- `**GetAudioRoutingStateAt`** exposes **ambient / chase / ending** layer scalars plus environment label for soundscape logic.

### Trigger volume dispatch (lightweight scripting bridge)

- **Generic** and **spatial** trigger volumes can author `**onEnterEvent` / `onStayEvent` / `onExitEvent`** (and aliases) for **named** dispatch into logic / runtime events.
- `**UpdateTriggerVolumesAt`** produces **transitions** and **requests** (teleport, launch, damage, streaming, analytics, etc.).

### Spatial query + trace helpers

- `**ri::trace::TraceScene`** is the shared collision front end; `**ri::trace::SpatialQueryHelpers**` (ray/box/sweep blocking) builds on the same types for movement and tooling.
- **Instrumentation** records query counts and candidate scans for profiling.

### Mechanics / automation harness

- `--benchmark-frames=<n>` on standalone: run **n** presented frames, log **FPS** from the same path as manual play, then exit.
- `**--bench`** / `**--bench-frames**`: CPU/software render benchmark (see `--help` per app).
- `**ri::debug::ExportRuntimeDebugSnapshot**` (and related APIs) support dumping structured runtime snapshots for tooling (see `Source/RawIron.Debug`).

---

For per-game file lists and controls, see `**Games/LiminalHall/README.md**`, `**Games/WildernessRuins/README.md**`, and each `**manifest.json**`.