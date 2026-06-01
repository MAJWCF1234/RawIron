# Content and Game Format

Game projects are validated content bundles, not loose folders.

## Manifest and format contract

`RawIron.Content` validates project manifests and required on-disk surfaces. Supported game format strings include `rawiron-game-v1.3.7`, `rawiron-game-v1.3.6`, and `rawiron-game-v1.3.5`.

Every game binds to the shared runtime contract through manifest fields such as:

- `runtimeContract`
- `runtimeModule`
- `runtimeHost`
- `runtimeServices`

## Required project surfaces

Games are expected to carry, at minimum:

- `manifest.json`
- `README.md`
- `scripts/*.riscript` for gameplay, rendering, logic, UI, audio, streaming, localization, physics, postprocess, init, state, network, persistence, AI, plugins, animation, and VFX
- `config/*.cfg`, `*.policy`, and related config baselines
- `levels/assembly.*` authored level and runtime support files
- `assets/*` manifests, palettes, materials, shaders, banks, and fonts
- `data/*` schema, registry, lookup, telemetry, save, and achievement data
- `plugins/*` manifest, load order, registry, and hooks
- `ai/*` behavior, blackboard, factions, perception, and squad tactics
- `ui/*` layout and styling
- `tests/*.riscript`

## Config ownership

Shared config enforcement lives in `Games/Common/src/GameConfigContracts.cpp` and is applied through `ri::games::EnforceGameConfigContracts(...)`.

Available enforcement modes:

- `Strict`
- `Balanced`
- `Permissive`

The default runtime posture is `Balanced`, which still blocks missing core surfaces and reports schema problems without forcing every iteration to hard-fail.

## Why this matters

This keeps projects consistent across games and prevents runtime tuning from drifting into hardcoded per-game behavior that bypasses the engine.

## Reference

- `Games/GAME_FORMAT.md`
- `Source/RawIron.Content/src/GameManifest.cpp`
