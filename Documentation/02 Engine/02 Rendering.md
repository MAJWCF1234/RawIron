# Rendering

RawIron exposes both Vulkan and software rendering paths.

## Rendering libraries

- `RawIron.Render.Vulkan`
- `RawIron.Render.Software`
- shared scene and post-process support from `RawIron.Core`, `RawIron.World`, and `RawIron.SceneUtilities`

## Rendering ownership

Rendering policy is engine-owned.

- Post-process state and parameter conversion live in shared engine/runtime structures.
- Games feed authored values through config and script surfaces.
- Runtime code should not invent private presentation stacks outside engine contracts.

## Authoring inputs commonly consumed by games

- `scripts/rendering.riscript`
- `scripts/postprocess.riscript`
- `assets/shaders.manifest`
- `assets/materials.manifest`
- `assets/palette.ripalette`
- `levels/assembly.lighting.csv`
- `levels/assembly.occlusion.csv`
- `levels/assembly.lods.csv`
- `levels/assembly.audio.zones`

## Preview and runtime surfaces

- `RawIron.Player` is the generic runtime host.
- `RawIron.Preview` is the snapshot and preview host.
- `RawIron.Editor` and `RawIron.EditorPreview` provide authoring-time visualization.
- `RawIron.ParticleShowcase` isolates particle-focused rendering work.

## Engine goal

The engine owns the render path. Game projects author the data and choose values through validated config and script contracts.
