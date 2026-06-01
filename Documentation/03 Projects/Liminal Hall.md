# Liminal Hall

`Liminal Hall` is the in-repo surreal environment and runtime showcase project.

## Identity

- game id: `liminal-hall`
- editor arg: `--game=liminal-hall`
- app target: `RawIron.LiminalGame`
- primary level: `levels/assembly.primitives.csv`

## What it demonstrates

- authored world assembly and collision
- runtime-owned rendering and post-process flow
- triggers, zones, info panels, and interactive world actors
- project config ownership through script and cfg surfaces
- plugin and hook surfaces as part of the normal game contract

## Main authored surfaces

- `scripts/gameplay.riscript`
- `scripts/rendering.riscript`
- `scripts/postprocess.riscript`
- `scripts/logic.riscript`
- `scripts/ui.riscript`
- `config/game.cfg`
- `config/security.policy`
- `config/plugins.policy`
- `levels/assembly.*`
- `plugins/*`

## Open and run

- `Games\LiminalHall\Open Liminal Hall In Editor.cmd`
- `Games\LiminalHall\Play Liminal Hall.cmd`
- `RawIron.Editor --game=liminal-hall`
