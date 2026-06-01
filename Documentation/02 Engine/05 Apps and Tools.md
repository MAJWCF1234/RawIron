# Apps and Tools

RawIron ships multiple native hosts and workspace tools.

## Primary apps

- `RawIron.Player`: generic runtime host
- `RawIron.Preview`: preview and snapshot host
- `RawIron.Editor`: native editor host
- `RawIron.VisualShell`: keyboard-first shell
- `RawIron.UiMenu`: JSON and Dear ImGui UI harness
- `RawIron.ParticleShowcase`: particle-focused app
- `RawIron.BotClient`: headless bot swarm client
- `RawIron.DedicatedServer`: headless authoritative server

## Game apps

- `RawIron.LiminalGame`
- `RawIron.ForestRuinsGame`
- `RawIron.MultiplayerSandboxGame`

## `ri_tool`

`Tools/ri_tool` is the optional command-line workspace utility target. Its source shows that it understands workspace layout, content folders, saved folders, package and asset document support, scene utilities, preview helpers, and Vulkan tooling discovery.

## Editor-facing game hooks

The editor inspects game projects for:

- `config/security.policy`
- `config/plugins.policy`
- `plugins/manifest.plugins`
- `plugins/load_order.cfg`
- `plugins/hooks.riplugin`

That makes plugins, policies, and hook surfaces part of normal project authoring rather than an external add-on.

## Build switches

Apps and tools are enabled through root `RAWIRON_BUILD_*` options in `CMakeLists.txt`.
