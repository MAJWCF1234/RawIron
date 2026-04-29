---
tags:
  - rawiron
  - engine
  - scene
  - scenekit
---

# Scene Primitives

## Intent

This note tracks the first batch of scene/runtime concepts used in the native engine.

RawIron now has native starter versions of:

- scene graph
- node hierarchy
- local transforms
- world transform composition
- camera descriptors
- mesh descriptors
- material descriptors
- light descriptors
- integrated helper builders
- scene query utilities

## Current Shape

### `Scene`

Owns:

- nodes
- materials
- meshes
- cameras
- lights

### `Node`

Each node currently has:

- name
- parent
- children
- local transform
- optional mesh/material attachment
- optional camera attachment
- optional light attachment

### `Transform`

Current transform state:

- position
- Euler rotation in degrees
- scale

World transforms are composed from parent to child in the core runtime.

## Current Purpose

This is not yet a renderer.
It is the engine-side scene representation that a renderer, editor hierarchy, prefab system, and importer pipeline can grow around.

## Next Likely Steps

- bounds and visibility data
- prefab/scene serialization
- runtime scene loading from `.ri_scene` or successor formats
- real render submission built from scene contents
- editor hierarchy and inspector views driven by the same data
- more built-in utilities that prototypes usually depend on immediately
