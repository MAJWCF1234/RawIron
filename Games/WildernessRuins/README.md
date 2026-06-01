# Wilderness Ruins

Format contract: `rawiron-game-v1.3.7`

## Identity

- `id`: `wilderness-ruins`
- `name`: `Wilderness Ruins`
- `entry`: `RawIron.ForestRuinsGame`
- `runtimeModule`: `RawIron.Game.ForestRuins`
- `editorProjectArg`: `--game=wilderness-ruins`
- `primaryLevel`: `levels/assembly.primitives.csv`
- `editorPreviewScene`: `wilderness-ruins`

## Purpose

Wilderness Ruins is the grounded outdoor counterpart to Liminal Hall. It uses the same shared engine and project contract while demonstrating a different authored atmosphere, movement space, and world presentation profile.

## Open and run

- `Games\WildernessRuins\Open Wilderness Ruins In Editor.cmd`
- `Games\WildernessRuins\Play Wilderness Ruins.cmd`
- `RawIron.Editor --game=wilderness-ruins`

## Controls

- `WASD` move
- `Mouse` look
- `Space` jump
- `Shift` sprint
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

Wilderness Ruins exercises the same shared runtime contract as the rest of the workspace while keeping its own authored gameplay and presentation profile inside project data.

## Reference

- `Games/GAME_FORMAT.md`
- `Documentation/03 Projects/Wilderness Ruins.md`
