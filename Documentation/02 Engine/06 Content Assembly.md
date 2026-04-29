---
tags:
  - rawiron
  - engine
  - content
  - prefabs
  - templates
---

# Content Assembly

This note records how RawIron wants to think about authored content now that template and prefab expansion are moving into native `C++`.

## Purpose

The engine should not treat authored world assembly as app glue.

It should own the rules for:

- expanding entity templates
- instantiating prefabs
- normalizing authored transforms
- applying creator-facing runtime policies like inventory visibility
- rejecting invalid recursive content graphs

That is engine behavior, not temporary tooling behavior.

## Creator Runtime Policy

RawIron should also own small authored gameplay-policy seams that creators need to toggle without forking engine code.

One concrete example is inventory presence and presentation.

The engine now has a native inventory-policy concept with three intended authored modes:

- `disabled`
- `hidden_data_only`
- `visible`

That means the future editor menu-maker flow can expose inventory as a creator choice instead of a hardcoded game rule:

- some projects want a visible hotbar/backpack UI
- some only want silent item storage for things like keycards, keys, and quest tokens
- some want no inventory system at all

That policy belongs in engine-owned content/runtime configuration, not in app-side UI glue.

## RawIron.Content

The current native landing zone is:

- `Source/RawIron.Content`

Current responsibilities:

- generic content-value storage
- authoring sanitizers for vec2, vec3, quaternion, and scale data
- finite number and integer clamps
- template merge and inheritance behavior
- prefab transform resolution
- nested prefab expansion
- authored world-volume translation into typed native runtime descriptors
- asset extraction inventory / manifest serialization for pipeline bookkeeping ([[Asset Extraction Inventory]])
- declarative model definition format for structured model composition ([[Declarative Model Definition]])

## Template Rule

Templates should let authored content inherit stable defaults without forcing copy-paste.

Current native behavior:

- inherit from a parent template
- merge nested object fields
- let arrays replace, not deep-merge
- remove `template` after expansion
- fail fast on recursive template chains

That matches the prototype's useful behavior while keeping the runtime honest about invalid content.

## Prefab Rule

Prefabs should express reusable authored chunks, not just repeated geometry blobs.

That means a prefab instance can carry:

- position
- rotation
- scale
- ID prefixing
- nested prefab instances

And the expansion layer must correctly propagate transforms through child authored nodes such as:

- geometry
- model instances
- spawners
- lights

## Sanitization Rule

Authored content arrives dirty sometimes.

The engine should sanitize it before it becomes runtime state.

Current examples:

- bad vec components fall back to safe values
- zero-length quaternions fall back to identity
- unusable scale values get clamped back to sane bounds
- invalid numbers do not silently poison later transform work

## Why This Matters

This is one of the bridges from “prototype app” to “real engine.”

If RawIron owns content assembly natively:

- the editor can depend on it
- the runtime can depend on it
- the asset pipeline can depend on it

That makes prefabs and templates first-class engine concepts instead of lingering prototype tricks.

The same rule now applies to several authored world-volume families.

RawIron.Content can now translate authored content objects into native runtime descriptors for:

- filtered collision
- camera blockers
- AI blockers
- damage and kill volumes
- camera modifiers
- safe zones
- custom gravity volumes
- directional wind volumes
- buoyancy volumes
- surface velocity primitives
- radial force volumes
- physics constraint volumes
- traversal link volumes
- ladder and climb helper volumes
- local grid snap volumes
- hint and skip guidance brushes
- camera confinement volumes
- lod override volumes
- navmesh modifier volumes
- ambient audio volumes and spline helpers
- streaming level volumes
- checkpoint spawn volumes
- teleport volumes
- launch volumes
- analytics heatmap volumes
- generic trigger volumes
- spatial query volumes
- visibility primitives for `portal` and `anti_portal`
- reflection probe volumes
- light importance volumes
- light portals
- occlusion portals
- post-process volumes
- audio reverb volumes
- audio occlusion volumes
- localized fog
- volumetric fog blockers
- fluid simulation volumes

The newer fidelity pass also makes sure those authored environment volumes preserve more of the behavior that matters:

- tint color
- fluid flow
- non-zero environment extents

The newer native physics-helper bridge extends that same rule to movement-affecting authored helpers:

- gravity and jump modifiers
- drag and buoyancy
- conveyor and wind flow
- radial push and pull fields

That is another real engine seam, not a temporary app concern.

## Current Boundary

RawIron.Content currently stops at expansion and normalization.

It does not yet fully own:

- `.ri_scene` persistence
- authored asset import
- final scene/runtime instantiation beyond typed world-volume authoring bridges

Those are natural next layers above this foundation.

## Related Notes

- [[04 Level Design Patterns]]
- [[03 Event Engine]]
- [[02 World Systems]]
- [[05 Migration/Pass 10 Content Templates and Prefab Expansion Foundation]]
- [[05 Migration/Pass 12 Authored Volume Content Bridge]]
