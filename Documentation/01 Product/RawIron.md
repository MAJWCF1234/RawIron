# RawIron

RawIron is a workspace-oriented engine and game stack.

## What it includes

- Shared engine libraries in `Source/`
- Runnable native apps in `Apps/`
- Game projects in `Games/`
- Shared art, data, and authored content in `Assets/`
- Local launchers, installers, and release scripts at the workspace root

## Engine profile

RawIron is built around:

- `RawIron.Core` for host, loop, logging, diagnostics, and command line handling
- `RawIron.Runtime` for lifecycle, services, events, and multiplayer runtime modules
- `RawIron.World`, `RawIron.Logic`, `RawIron.Events`, `RawIron.Trace`, and `RawIron.Spatial` for simulation, world state, and interaction
- `RawIron.Render.Vulkan` and `RawIron.Render.Software` for presentation paths
- `RawIron.Content` for manifests, project validation, and authored data loading

## Runnable workspace targets

Default local builds can include:

- `RawIron.Player`
- `RawIron.Preview`
- `RawIron.Editor`
- `RawIron.VisualShell`
- `RawIron.UiMenu`
- `RawIron.ParticleShowcase`
- `RawIron.LiminalGame`
- `RawIron.ForestRuinsGame`
- `RawIron.MultiplayerSandboxGame`
- `RawIron.BotClient`
- `RawIron.DedicatedServer`

## Project model

RawIron games are format-driven projects with a required manifest, script/config surfaces, plugin policy files, asset manifests, level assemblies, AI data, UI data, and tests. The shared runtime core mounts those projects and enforces the contract before gameplay starts.

## Navigation

- [[02 Engine/00 Engine Home|Engine overview]]
- [[03 Projects/00 Projects Home|Games and project layout]]
- [[04 Pipeline/00 Pipeline Home|Pipeline and release workflow]]
