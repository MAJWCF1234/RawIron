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
- `assets/dependencies.json` optional game package roots, runtime capabilities, and granted permissions
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

## Audio tuning contract

`scripts/audio.riscript` carries two required scalars. Loading and apply are engine-owned:

- `ri::content::LoadGameAudioTuningScalars(gameRoot)` loads the file
- `ri::audio::ApplyAudioMasterGain` pushes master gain (attenuation-only `[0,1]`; values above `1.0` clamp with a message)
- `ri::audio::BlendAudioEnvironmentProfile` applies `audio_environment_blend` (`0` dry, `1` authored, `2` exaggerated)

Games that mount an `AudioManager` call those helpers; they do not re-parse or re-clamp the contract themselves. Games without an audio manager still need the file present to satisfy the format contract.

## Why this matters

Every game mounts the same engine features. What makes a project feel different is its authored configuration and content — riscript scalars, cfg policies, UI manifests, levels, assets — or a custom package it brings. That keeps projects consistent and stops runtime tuning from drifting into hardcoded per-game forks of engine systems.

## Reference

- `Games/GAME_FORMAT.md`
- `Source/RawIron.Content/src/GameManifest.cpp`
