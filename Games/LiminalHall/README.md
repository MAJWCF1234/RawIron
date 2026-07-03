# Liminal Hall

Format contract: `rawiron-game-v1.3.7`

## Identity

- `id`: `liminal-hall`
- `name`: `Liminal Hall`
- `entry`: `RawIron.LiminalGame`
- `runtimeModule`: `RawIron.Game.LiminalHall`
- `editorProjectArg`: `--game=liminal-hall`
- `primaryLevel`: `levels/assembly.primitives.csv`
- `editorPreviewScene`: `liminal-hall`

## Purpose

Liminal Hall is the surreal in-repo exploration and runtime showcase project. It demonstrates authored structural primitives at monument scale, semantic CSV assembly, a compact structural primitive gallery, surreal mood landmarks, basin water presentation, runtime-owned Vulkan presentation, interaction hooks, project config ownership, and the shared game contract working inside a shippable game folder.

## Showcase highlights

- **Structural chamber** — `levels/assembly.structural.csv` drives the main hall with full semantic metadata (roles, regions, operations, M/P/Q/I channels).
- **Primitive gallery** — west-side `LiminalStructuralGallery` spawned from engine structural presets with LRT material rows.
- **Mood landmarks** — animated FractalGate, checker obelisks, brain sphere, neon sun, and glitch pyramid with matching point lights.
- **Basin water** — translucent outer-basin plane under the structural floor for void/water presentation.
- **Logic demo chain** — pressure plate, door, portal with LogicKit GLB visuals.

## Open and run

- `Games\LiminalHall\Open Liminal Hall In Editor.cmd`
- `Games\LiminalHall\Play Liminal Hall.cmd`
- `RawIron.Editor --game=liminal-hall`

## Controls

- `WASD` move
- `Mouse` look (click the game window first so it has focus)
- `Space` jump
- `Shift` sprint
- `E` interact
- `Esc` pause menu during gameplay (Esc again or Resume to continue; Esc on title menu quits)
- `F1` toggle game-local main menu flow (console/log only in `RawIron.LiminalGame`; does not block movement)
- `F2` toggle game-local VN flow (blocks movement while active)
- `Tab` cycle runtime UI options
- `1-9` choose a runtime UI option directly
- `Enter` / `Space` activate or advance runtime UI
- `Backspace` go back or leave the active runtime UI flow

## Main project surfaces

### Runtime scripts

- `scripts/gameplay.riscript`
- `scripts/rendering.riscript`
- `scripts/logic.riscript`
- `scripts/ui.riscript`
- `scripts/audio.riscript`
- `scripts/streaming.riscript`
- `scripts/localization.riscript`
- `scripts/physics.riscript`
- `scripts/postprocess.riscript`
- `scripts/init.riscript`
- `scripts/state.riscript`
- `scripts/network.riscript`
- `scripts/persistence.riscript`
- `scripts/ai.riscript`
- `scripts/plugins.riscript`
- `scripts/animation.riscript`
- `scripts/vfx.riscript`

### Config and policy

- `config/game.cfg`
- `config/input.map`
- `config/project.dev`
- `config/network.cfg`
- `config/build.profile`
- `config/security.policy`
- `config/plugins.policy`

### Level assembly

- `levels/assembly.primitives.csv`
- `levels/assembly.colliders.csv`
- `levels/assembly.navmesh`
- `levels/assembly.zones.csv`
- `levels/assembly.triggers.csv`
- `levels/assembly.occlusion.csv`
- `levels/assembly.audio.zones`
- `levels/assembly.lods.csv`
- `levels/assembly.ai.nodes`
- `levels/assembly.lighting.csv`
- `levels/assembly.cinematics.csv`

### Assets, data, plugins, and tests

- `assets/*`
- `data/*`
- `plugins/*`
- `ai/*`
- `ui/*`
- `tests/*.riscript`

### UI authoring contract

- `ui/main.ui.json` is the primary game-local UI flow manifest.
- `ui/vn_intro.ui.json` is the primary game-local VN/dialogue flow manifest.
- `ui/layout.xml` and `ui/styling.css` are optional support assets for HUD/chrome, not the main flow source.
- `scripts/ui.riscript` remains scalar-only runtime tuning for diagnostics, HUD behavior, and runtime UI boot policy.
- `runtime_ui_boot_flow` uses `0=gameplay`, `1=menu`, `2=vn`.
- `runtime_ui_hotkeys_enabled` controls whether `F1` / `F2` can switch runtime flows interactively.

## Runtime focus

Liminal Hall exercises:

- shared runtime lifecycle through `RuntimeCore`
- config-driven tuning instead of per-game render invention
- authored triggers, zones, audio spaces, and interaction hooks
- plugin and policy surfaces as part of a normal project

## Reference

- `Games/GAME_FORMAT.md`
- `Documentation/03 Projects/Liminal Hall.md`
