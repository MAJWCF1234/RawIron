# Liminal Hall Material Modernization Design

## Goal

Modernize `Games/LiminalHall` so it consumes the current RawIron engine renderer through materially richer authored content, not through game-private rendering code. The pass replaces remaining low-information legacy surface requests with exact or near-exact LRT package materials, then uses the result as the calibration scene for the next `RawIron.Render.Vulkan` renderer upgrade.

## Engine Boundary

- `RawIron.Render.Vulkan` remains engine-owned.
- `LiminalHall` remains a consumer of engine rendering and engine-shipped asset packages.
- Game-side work is limited to authored data and game runtime material selection/remapping where the scene needs exact package resolution.

## Current State

- `Games/LiminalHall/levels/assembly.structural.csv` still references many `ri_psx_official_*` base textures even where RT normal/detail maps are already present.
- `Games/LiminalHall/levels/assembly.primitives.csv` contains upgraded emissive/window cards and duplicate `_import1` rows that still point at prototype textures.
- `Games/LiminalHall/Runtime/src/LiminalHallWorld.cpp` already contains `ApplyLiminalRendererShowcaseMaterials(...)`, but the remap is broad and does not fully encode exact surface-family replacements or stronger gameplay/demo material cues.

## Design

### 1. Structural Surface Classes

`LiminalHall` surfaces will be normalized into a small set of material families from `Assets/Packages/LRT - Texture Pack - RT28.8 - 128x`:

- Concrete slabs and walkable floors: `RT_all_concrete_1`
- Monumental stone / towers / drums / stairs / arches: `RT_tuff` or `RT_tuff_bricks`
- Metal walkways / braces / decks: `RT_stainless_steel`, `RT_iron_block`, or grate-like copper/iron families where silhouette read benefits
- Emissive apertures / glow cards: `RT_white_stained_glass`, `RT_shroomlight`, `RT_sea_lantern`

Each chosen family should carry its companion normal/spec-style maps where available so the current Vulkan material path sees better roughness/specular response, not just new albedo.

### 2. Authoring Honesty

The level CSV data should ask for appropriate modern material families directly instead of relying on PSX placeholder albedo names while only swapping some channels later. Runtime remapping stays in place to resolve package-relative paths and to cover imported primitive duplicates or special gameplay/demo surfaces.

### 3. Gameplay Surface Preservation

Logic demo objects can remain readable and color-coded, but should stop looking like flat prototype blocks where practical. The pass should preserve gameplay clarity while lifting the material response into the same scene language as the rest of the hall.

## Validation

Success means:

- `assembly.structural.csv` no longer contains legacy `ri_psx_official_*` surface requests.
- Imported emissive/window card primitives no longer rely on placeholder white prototype textures for the visible scene.
- `LiminalHall` still builds as a normal game module.
- The scene presents more believable concrete, stone, metal, and emissive response under the existing engine-owned Vulkan renderer.

## Deferred Work

This pass does not attempt the full pseudo-raytraced renderer redesign. It creates the benchmark scene that will expose where `RawIron.Render.Vulkan` needs stronger reflections, GI, contact shadowing, roughness handling, and temporal stability next.
