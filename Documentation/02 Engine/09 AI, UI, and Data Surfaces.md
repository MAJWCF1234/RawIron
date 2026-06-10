# AI, UI, and Data Surfaces

RawIron projects treat AI, UI, persistence, and data files as authored project surfaces, not buried engine internals.

## AI surfaces

Games provide AI content through:

- `scripts/ai.riscript`
- `levels/assembly.ai.nodes`
- `ai/behavior.tree`
- `ai/blackboard.json`
- `ai/factions.cfg`
- `ai/perception.cfg`
- `ai/squad.tactics`

These files define behavior tuning, node graphs, default blackboard values, perception ranges, faction relationships, and squad patterns.

## UI surfaces

Games provide UI content through:

- `scripts/ui.riscript`
- `ui/layout.xml`
- `ui/styling.css`

The current editor and runtime path already consume UI script scalars for diagnostics, objective panel state, crosshair mode, crosshair scale, and style variants.

### Runtime text overlay (engine)

`RawIron.World` exposes semantic HUD channels through `text_overlay_events` on `RuntimeEventBus` (`message`, `subtitle`, `levelToast`, `objectiveChanged`, loading/voice hooks). `TextOverlayEventBridge` updates `TextOverlayState`; standalone games use `GameTextOverlayHost` (`Games/Common`) to advance timers and draw GDI captions over the Vulkan client area.

Liminal Hall wires this path today — showcase beats, objectives, subtitles, and interaction toasts render on-screen when emitted through the runtime bus.

## Data and persistence surfaces

Games provide data through:

- `data/schema.db`
- `data/lookup.index`
- `data/entity.registry`
- `data/telemetry.db`
- `data/save.schema`
- `data/achievements.registry`
- `scripts/persistence.riscript`
- `scripts/network.riscript`

These surfaces support lookup acceleration, entity archetype registration, telemetry, save validation, achievement mapping, and network/persistence tuning.

## Why this structure matters

The engine can validate and inspect these systems because they are explicit project files. That makes them easier to ship, mod, audit, and reuse across projects.
