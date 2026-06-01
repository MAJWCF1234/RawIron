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

Liminal Hall is the surreal in-repo exploration and runtime showcase project. It demonstrates authored world assembly, runtime-owned presentation, interaction hooks, project config ownership, and the shared game contract working inside a shippable game folder.

## Open and run

- `Games\LiminalHall\Open Liminal Hall In Editor.cmd`
- `Games\LiminalHall\Play Liminal Hall.cmd`
- `RawIron.Editor --game=liminal-hall`

## Controls

- `WASD` move
- `Mouse` look
- `Space` jump
- `Shift` sprint
- `E` interact
- `Esc` quit

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

## Runtime focus

Liminal Hall exercises:

- shared runtime lifecycle through `RuntimeCore`
- config-driven tuning instead of per-game render invention
- authored triggers, zones, audio spaces, and interaction hooks
- plugin and policy surfaces as part of a normal project

## Reference

- `Games/GAME_FORMAT.md`
- `Documentation/03 Projects/Liminal Hall.md`
