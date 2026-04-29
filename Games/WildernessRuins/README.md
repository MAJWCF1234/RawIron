# Wilderness Ruins

This game is intended to be edited from this folder.

Format contract: `rawiron-game-v1.3.7` (see [`Games/GAME_FORMAT.md`](../GAME_FORMAT.md)).

Manifest v1.3.7 identity:

- `id`: `wilderness-ruins`
- `editorProjectArg`: `--game=wilderness-ruins`
- `primaryLevel`: `levels/assembly.primitives.csv`
- `editorPreviewScene`: `wilderness-ruins`

## Edit Here

- `D:\RawIron\Games\WildernessRuins\Open Wilderness Ruins In Editor.cmd`
- or from repo root: `D:\RawIron\edit-forest-ruins.cmd`

Editor opens directly to `--game=wilderness-ruins`.
Authoring export writes to:

- `levels/assembly.primitives.csv`

## Play Here

- `D:\RawIron\Games\WildernessRuins\Play Wilderness Ruins.cmd`
- or from repo root: `D:\RawIron\play-forest-ruins.cmd`

## Game-Owned Content

- `manifest.json` game metadata and editor/game binding
- `scripts/gameplay.riscript` movement/spawn/gameplay tuning
- `scripts/rendering.riscript` fog/ambient/fov/render tuning
- `scripts/logic.riscript` directed event-graph logic authoring (text + spatial compile target)
- `scripts/ui.riscript` HUD/crosshair/objective-panel tuning
- `scripts/audio.riscript` game-owned audio environment and mix tuning scalars
- `scripts/streaming.riscript` game-owned streaming/checkpoint tuning scalars
- `scripts/localization.riscript` locale/fallback scalar tuning
- `scripts/physics.riscript` global physics scalar tuning
- `scripts/postprocess.riscript` post-FX scalar tuning
- `scripts/init.riscript` startup warmup/precache scalar tuning
- `scripts/state.riscript` save/checkpoint/stateflow scalar tuning
- `scripts/network.riscript` network tick/session/reliability scalar tuning
- `scripts/persistence.riscript` save-slot/flush/journal scalar tuning
- `scripts/ai.riscript` AI cadence/awareness scalar tuning
- `config/game.cfg` game-owned cfg scalars for runtime/editor profile toggles
- `config/input.map` required input-action map baseline
- `config/project.dev` required local/project workspace cfg baseline
- `config/network.cfg` required runtime network transport and budget baseline
- `config/build.profile` required profile/channel/content budget baseline
- `config/security.policy` required gameplay/security policy scalar baseline
- `assets/palette.ripalette` authored palette
- `assets/layers.config` render/authoring layer routing baseline
- `assets/manifest.assets` required asset key-to-path manifest
- `assets/metadata.json` required project metadata constants
- `assets/dependencies.json` required runtime/editor dependency groups
- `assets/streaming.manifest` prioritized runtime streaming asset list
- `assets/shaders.manifest` required shader-pack mapping manifest
- `assets/materials.manifest` PBR material definitions and bindings
- `assets/audio.banks` pre-compiled audio bank references
- `assets/fonts.manifest` typography and font atlas definitions
- `levels/assembly.primitives.csv` authored primitive placements
- `levels/assembly.colliders.csv` authored collider layout
- `levels/assembly.navmesh` navmesh region/link descriptor baseline
- `levels/assembly.zones.csv` sector-based streaming zone definitions
- `levels/assembly.triggers.csv` level event triggers and hitboxes
- `levels/assembly.occlusion.csv` precomputed culling and occlusion volumes
- `levels/assembly.audio.zones` spatial audio volumes, reverb nodes, and occlusion dampening
- `levels/assembly.lods.csv` level-of-detail culling distances and scaling ranges
- `levels/assembly.ai.nodes` authored AI node graph definitions
- `data/schema.db` text-based schema descriptor baseline for persistent records
- `data/lookup.index` baseline index for accelerated data lookup keys
- `data/entity.registry` baseline entity/archetype registry data
- `data/save.schema` validation schema for safe serialization and saves
- `data/achievements.registry` cross-platform achievement and trophy mappings
- `ai/behavior.tree` baseline behavior-tree graph definitions
- `ai/blackboard.json` baseline blackboard key schema and defaults
- `ai/factions.cfg` baseline faction and relation matrix values
- `ai/perception.cfg` sight, hearing, and sensing parameters
- `ai/squad.tactics` group behavior and formation logic
- `ui/layout.xml` base UI layout definitions
- `ui/styling.css` global styling and theming for UI nodes
- `tests/gameplay.test.riscript` unit-style checks for core game logic contracts
- `tests/rendering.test.riscript` automated visual regression test definitions
- `tests/network.test.riscript` latency simulation and state-sync validation
- `tests/ui.test.riscript` UI functional state and navigation testing

UI script notes (`scripts/ui.riscript`):

- scalar keys currently wired by runtime/editor: `show_runtime_diagnostics`, `show_objective_panel`, `crosshair_mode`, `crosshair_scale`, `hud_style_variant`

