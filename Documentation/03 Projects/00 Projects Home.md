# Projects

RawIron currently carries three primary in-repo games.

- [[03 Projects/Liminal Hall|Liminal Hall]]
- [[03 Projects/Wilderness Ruins|Wilderness Ruins]]
- [[03 Projects/RawIron Multiplayer Sandbox|RawIron Multiplayer Sandbox]]

## Shared project rules

All games:

- declare a manifest and supported format string
- boot through the shared runtime contract
- carry the required script, config, asset, AI, plugin, UI, data, and test surfaces
- can be opened through `RawIron.Editor --game=<id>`
- ship as part of the full workspace release

## Shared support code

`Games/Common` contains shared game-facing support such as config contract enforcement and runtime boot helpers used across multiple projects.

## Shared authored families

Every game format includes structured work under:

- `scripts/`
- `config/`
- `levels/`
- `assets/`
- `data/`
- `plugins/`
- `ai/`
- `ui/`
- `tests/`

## Project launch patterns

- direct game-local play scripts
- direct game-local editor scripts
- `RawIron.Editor --game=<id>`
- app target execution from the build tree
