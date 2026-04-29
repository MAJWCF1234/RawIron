---
tags:
  - rawiron
  - migration
  - content
  - prefabs
  - templates
---

# Pass 10 Content Templates and Prefab Expansion Foundation

## Goal

Port the prototype's authored-world expansion logic out of `Q:\anomalous-echo\index.js` and into a native `C++` engine library that RawIron can own long-term.

This pass focuses on the engine-facing parts of authored content assembly:

- transform sanitization
- numeric clamp helpers
- entity-template inheritance
- prefab-instance transform resolution
- nested prefab expansion

The goal is to keep the prototype's level-authoring intelligence without dragging the app shell or game code into RawIron runtime libraries.

## Prototype Source

Primary source seam:

- `Q:\anomalous-echo\index.js`

Prototype functions mirrored in this pass:

- `finiteVec3Components`
- `finiteVec2Components`
- `finiteQuatComponents`
- `finiteScaleComponents`
- `clampFiniteNumber`
- `clampFiniteInteger`
- `clampPickupMotion`
- `mergeLevelTemplateData`
- `resolveLevelEntityTemplate`
- `applyLevelEntityTemplates`
- `getPrefabTransform`
- `transformPrefabVector`
- `transformPrefabNode`
- `instantiateLevelPrefab`
- `expandLevelPrefabs`

## Native Landing Zone

New library:

- `Source/RawIron.Content`

Current native files:

- `Source/RawIron.Content/include/RawIron/Content/Value.h`
- `Source/RawIron.Content/include/RawIron/Content/PrefabExpansion.h`
- `Source/RawIron.Content/src/PrefabExpansion.cpp`

## What RawIron.Content Owns

### Data Node

`Value` is a lightweight engine-owned content node that can hold:

- null
- booleans
- numbers
- strings
- arrays
- objects

This gives RawIron a neutral intermediate structure for authored content expansion without forcing the runtime to depend on browser-era data objects.

### Authoring Sanitizers

The native layer now normalizes prototype-style authoring data for:

- vec3 values
- vec2 values
- quaternions
- scales
- clamped finite numbers
- clamped finite integers
- pickup motion values

This keeps dirty authored input from leaking directly into runtime transforms.

### Entity Templates

RawIron now supports native template expansion for authored entities.

Current behavior:

- template inheritance
- recursive template resolution
- nested object merging
- array replacement semantics
- removal of final `template` keys after expansion
- recursion detection for invalid template graphs

### Prefab Expansion

RawIron now supports native prefab expansion over authoring data.

Current behavior:

- instance transform extraction
- authored ID prefixing
- position, rotation, scale, look-at, and path transformation
- nested prefab instancing
- recursive prefab detection
- root-level append into:
  - `geometry`
  - `modelInstances`
  - `spawners`
  - `lights`

## Why This Matters

This is one of the first passes where RawIron starts to own authored-world assembly rather than just runtime helpers.

That matters because a real engine needs more than:

- rendering
- math
- events

It also needs a trustworthy way to take authored content patterns and turn them into runtime-ready world data.

The prototype already proved the authoring model.
This pass gives RawIron a native landing zone for that model.

## Testing

Coverage lives in:

- `Tests/RawIron.EngineImport.Tests/src/TestContentPrefabExpansion.cpp`

The pass is currently verified for:

- sanitization helpers
- template inheritance and merging
- prefab expansion into root collections
- nested prefab transform propagation
- path and look-at transformation
- recursive template rejection
- recursive prefab rejection

One practical build note from this pass:

- the original giant import test body pushed MSVC into compiler heap exhaustion
- the content test was split into its own translation unit so the port stays healthy under both MSVC and Clang

## Result

RawIron now has a native authored-content foundation instead of leaving template and prefab expansion trapped inside the prototype app shell.

That moves the engine closer to a real editor/runtime pipeline:

- authored data enters RawIron
- templates resolve natively
- prefabs expand natively
- the runtime no longer has to depend on JavaScript-era content assembly

## Next Good Passes

1. Port the next engine-worthy authored-content helpers from `index.js` into `RawIron.Content` or a neighboring native library.
2. Connect `RawIron.Content` to future `.ri_scene` load/save paths so authored content stops being test-only.
3. Feed expanded content into scene/runtime assembly so prefabs and templates stop at the engine boundary instead of the test boundary.
