# Project Authoring

Project authoring in RawIron happens inside the shipped workspace.

## Open a game in the editor

Use either:

- `RawIron.Editor --game=liminal-hall`
- `RawIron.Editor --game=wilderness-ruins`
- `RawIron.Editor --game=rawiron-multiplayer-sandbox`

or the matching game-local helper scripts.

## Authoring areas

A normal game project contains:

- manifest and identity data
- runtime script surfaces
- cfg and policy surfaces
- level assembly files
- asset manifests and registries
- plugin and hook declarations
- AI graphs and config
- UI layout and styling
- test scripts

## Local project state

Project-local development state is stored in:

- `config/project.dev`
- `Saved/Editor/<game-id>/`

## Validation path

Projects are checked through:

- manifest format validation in `RawIron.Content`
- runtime support loading
- shared config contract enforcement in `Games/Common`
- editor asset-presence and support summaries

## Good authoring posture

- put tuning in cfg and riscript surfaces
- keep gameplay/runtime modules mounted through `RuntimeCore`
- keep plugin permissions and security posture in policy files
- keep tests close to the game in `tests/`
