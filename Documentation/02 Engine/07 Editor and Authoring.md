# Editor and Authoring

`RawIron.Editor` is the native project authoring host for workspace games.

## Project discovery

The editor resolves projects from the workspace `Games/` folder by scanning for `manifest.json` and loading `GameManifest` data. When no project is explicitly requested, it can auto-open a default game from the registered workspace set.

## What the editor reads from a game

The editor surfaces authored data from:

- `manifest.json`
- `scripts/`
- `config/`
- `levels/`
- `assets/`
- `data/`
- `ai/`
- `ui/`
- `tests/`
- `plugins/`

It also reads runtime support summaries and lookup/index data so project resources can be prioritized and presented meaningfully.

## Editor boot summary

The editor loads and reports state from:

- `scripts/ui.riscript`
- `scripts/audio.riscript`
- `scripts/streaming.riscript`
- `scripts/localization.riscript`
- `scripts/physics.riscript`
- `scripts/postprocess.riscript`
- `scripts/init.riscript`
- `scripts/network.riscript`
- `scripts/persistence.riscript`
- `scripts/ai.riscript`
- `scripts/plugins.riscript`
- `scripts/animation.riscript`
- `scripts/vfx.riscript`
- `config/game.cfg`
- `config/network.cfg`
- `config/build.profile`
- `config/security.policy`
- `config/plugins.policy`

## Resource categories

The editor classifies project resources into categories such as:

- manifest
- levels
- scripts
- tests
- UI/screens
- menus
- assets
- other

That classification is used to keep project browsing and diagnostics readable across large games.

## Project-local authoring baseline

The editor ensures `config/project.dev` exists for a game project and stores per-project authoring state under `Saved/Editor/<game-id>/`.

## Preview scene binding

Each game manifest can declare `editorPreviewScene`, which lets the editor open into a project-appropriate scene view instead of a generic placeholder.
