---
tags:
  - rawiron
  - formats
  - assets
---

# File Format Decisions

## Guiding Principle

RawIron should own its runtime asset formats.

External source formats are for import.
Internal RawIron formats are for editing, cooking, and runtime use.

## Rejected

### `.rim`

Avoid `.rim`.

Reason:

- already used in other software/tooling contexts
- likely to create confusion and search collisions

## Current Standard Document

The active tooling standard is:

- `.ri_asset.json`

`ri_tool --formats` reports this as the single unified standard asset document for mesh, material, texture, audio, scene, and behavior metadata. `ri_tool --asset-standardize` and `ri_tool --asset-standardize-dir` write this format into `Assets/Cooked/Standardized`.

## Legacy / Experimental Extension Family

These still exist as useful vocabulary and legacy/experimental aliases, but they are not the current standardized output:

- `.ri_model`
- `.ri_mesh`
- `.ri_scene`
- `.ri_mat`
- `.ri_tex`
- `.ri_audio`
- `.ri_meshc`

## Compact Alternatives

These are also liked and may be used where a shorter extension is better:

- `.rimodel`
- `.riscene`
- `.ritex`
- `.riaudio`

## Earlier Favorites

- editable model: `.rimodel`
- cooked runtime mesh: `.rimeshbin` or `.ri_meshc`

## Current Interpretation

One likely direction is:

- explicit readable extensions for editor-side assets
- tighter binary-oriented extensions for cooked runtime assets

Example split:

- editor/import asset: `.ri_model`
- canonical editable model asset: `.rimodel`
- cooked mesh payload: `.rimeshbin`

This remains useful design context, but the current implemented path is `.ri_asset.json` first.

## Pipeline bookkeeping (non-runtime)

These are development and interchange shapes, not replacements for cooked runtime payloads:

- [[Asset Extraction Inventory]] — extracted-archive inventory manifest for tooling and validation workflows.
- [[Declarative Model Definition]] — declarative model composition data for a generic builder or loader path.

## Format Families To Define

- model
- mesh
- scene
- prefab
- material
- texture
- audio
- shader
- package/archive

## Open Questions

- whether to keep both explicit and compact families
- where the line sits between editable and cooked assets
- whether package/archive assets get their own top-level extension family
