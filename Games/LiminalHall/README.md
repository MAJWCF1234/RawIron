# Liminal Hall

Format contract: `rawiron-game-v1.3.7` with v1.3.7 showcase files populated (see [`Games/GAME_FORMAT.md`](../GAME_FORMAT.md)).

Manifest v1.3.7 identity:

- `id`: `liminal-hall`
- `editorProjectArg`: `--game=liminal-hall`
- `primaryLevel`: `levels/assembly.primitives.csv`
- `editorPreviewScene`: `liminal-hall`

Level layout (**v2 — fractured void**): large lower basin, reworked plaza spawn, eastern shard ascent + gallery + spire beacon, western suspended bridge into annex tower, northern monolith court with red disk, rebuilt portal ring pocket, floating plates to the south, and new zone IDs (`void_plaza`, `shard_east`, `bridge_west`, `monolith_north`, `portal_ring`, `south_basin`). Spawn sits on the cyan pad at negative Z facing the plaza.

**Surreal parkour course (“void ladder”):** from the cyan **CourseEntryStripe** south of spawn, drop onto the **south honeycomb** hub and run **`PkHub01–04` → `PkDesc05–08`** (depth descent). **`PkZig09–12`** zig-zags through uncanny spacing; **`PkHx01–08`** is a color-shifting helix; **`PkLinkA/B`** bridge to knife-edge **`PkGn01–03`** ribbons; **`PkStressA/B/C`** is a same-height sprint-jump lane (~2.8 m); **`PkVault01–02`** + **`PkPreFinish`** climb to **`PkFinishRing`** / crown / overhead glyph. Tunables live in **`scripts/gameplay.riscript`** and **`scripts/physics.riscript`**.

Standalone launch from repo root:

- `play-liminal.cmd`
- `pwsh ./play-liminal.ps1`
- extra args are forwarded, e.g.:
  - `play-liminal.cmd --width=1600 --height=900 --window-title="Liminal Hall Test"`
  - `play-liminal.cmd --mouse-sensitivity=0.16`
  - `play-liminal.cmd --no-mouse-capture` (useful for debugging / capture software)
  - `play-liminal.cmd --headless --frames=300 --output=Saved/Previews/liminal_demo.bmp`

Editor load:

- `RawIron.Editor --game=liminal-hall`
- or `Launch RawIron Editor.cmd --game=liminal-hall`

Controls in standalone:

- `WASD` move
- `Mouse` look (pointer clip when capture is on)
- `Space` jump
- `Shift` sprint
- `E` interact (doors / info panels when authored into the runtime environment)
- `Esc` exit

Game-owned content consumed by engine:

- `scripts/gameplay.riscript` movement/spawn tuning
- `scripts/rendering.riscript` fog/ambient/clear tuning
- `scripts/logic.riscript` directed event-graph logic authoring (text + spatial)
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
- `assets/palette.ripalette` authored color set
- `assets/layers.config` render/authoring layer routing baseline
- `assets/manifest.assets` required asset key-to-path manifest
- `assets/metadata.json` required project metadata constants
- `assets/dependencies.json` required runtime/editor dependency groups
- `assets/streaming.manifest` prioritized runtime streaming asset list
- `assets/shaders.manifest` required shader-pack mapping manifest
- `assets/materials.manifest` PBR material definitions and bindings
- `assets/audio.banks` pre-compiled audio bank references
- `assets/fonts.manifest` typography and font atlas definitions
- `levels/assembly.primitives.csv` primitive placements, with optional `texture,tileX,tileY`
- `levels/assembly.colliders.csv` collision assembly for trace/movement
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

Runtime tuning notes:

- `scripts/gameplay.riscript` can define `mouse_sensitivity` (degrees per pixel).
- command line `--mouse-sensitivity=<float>` overrides script/default at runtime.
- gameplay script also exposes movement/view feel knobs:
  - `ground_acceleration`, `air_acceleration`, `ground_friction`, `air_control`
  - `coyote_time`, `jump_buffer_time`, `low_jump_gravity_multiplier`, `max_fall_speed`
  - `camera_height`, `head_bob_amplitude`, `head_bob_frequency`, `head_bob_sprint_scale`
  - `spawn_x`, `spawn_y`, `spawn_z`, `spawn_yaw`, `spawn_pitch`
- rendering script exposes sprint FOV behavior:
  - `fov_base`, `fov_sprint_add`, `fov_lerp_per_second`

Headless demo capture:

- `--headless` runs a deterministic fixed-step simulation and saves the final frame BMP.
- `--frames=<n>` and `--dt=<seconds>` control duration and timestep.
- `--output=<path>` chooses output location (default: `Saved/Previews/liminal_headless.bmp`).
- `--no-autoplay` disables the staged attract-mode walk in headless mode.

UI script notes (`scripts/ui.riscript`):

- scalar keys currently wired by runtime/editor: `show_runtime_diagnostics`, `show_objective_panel`, `crosshair_mode`, `crosshair_scale`, `hud_style_variant`

## Feature Showcase Map

- AI data path: `scripts/ai.riscript` + `levels/assembly.ai.nodes` + `ai/behavior.tree` + `ai/blackboard.json` + `ai/factions.cfg`
  - Demonstrates AI cadence/mode data and faction defaults consumed into startup/runtime diagnostics.
- Plugin path: `scripts/plugins.riscript` + `config/plugins.policy` + `plugins/*`
  - Demonstrates hook registration order and plugin policy toggles consumed by runtime/editor status logs and render-priority boost gating.
- Animation and VFX path: `scripts/animation.riscript` + `scripts/vfx.riscript` + `assets/animation.graph` + `assets/vfx.manifest`
  - Demonstrates bob/fog/render tuning derived from authored manifests and script scalars.
- Lighting and cinematic path: `levels/assembly.lighting.csv` + `levels/assembly.cinematics.csv`
  - Demonstrates authored row-count driven startup influences for exposure/FOV blend behavior and diagnostics.
- Data and telemetry path: `data/entity.registry` + `data/lookup.index` + `data/telemetry.db`
  - Demonstrates entity and lookup coverage plus telemetry SQLite-header validation for deterministic startup reporting.

