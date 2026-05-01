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

## Current Package And Asset Formats

RawIron packages should ship as a renamed ZIP archive:

- `.ripak`

`*.ripak` is the user-facing package file, similar in spirit to Minecraft `.mcworld`: a normal ZIP container with a RawIron extension. It should be safe to rename to `.zip` for inspection, but tools and projects should treat `.ripak` as the canonical package artifact.

Inside a `.ripak`, RawIron uses structured manifest files and engine-owned asset documents:

- `package.ri_package.json`
- `.ri_asset.json`

`package.ri_package.json` is not the package by itself. It is the manifest inside the `.ripak` container or inside the exploded package work directory.

`.ri_asset.json` is the current standardized asset descriptor used during import/cook. It can point at standard media payloads such as images, audio, and material data, or at RawIron-owned formats such as `.riscript` and future custom model/scene files.

Current tooling writes the exploded package directory and manifest first, validates it, then creates the `.ripak`. The exploded folder remains useful for debugging and editor staging; the `.ripak` is the thing users share, mount, import, and install.

## Standard Media Payloads

These should remain ordinary files inside packages unless there is a clear runtime reason to cook them:

- Images/textures: `.png`, `.jpg`, `.jpeg`, `.tga`, `.bmp`, `.hdr`, `.tif`, `.tiff`
- Audio: `.wav`, `.ogg`, `.mp3`, `.flac`
- Material source data: standard RawIron material documents or imported material metadata

The package manifest tracks these through asset entries, but the payloads themselves do not need fake RawIron extensions just to be inside a package.

## RawIron-Owned Authored Formats

These are engine-owned formats and should be valid package contents:

- `.riscript` — RawIron scripting language, intended as our Lua-like authored behavior/config/test language.
- `.rimodel` or `.ri_model` — editable RawIron model definition.
- `.riscene` or `.ri_scene` — editable RawIron scene definition.
- `.ri_meshc` / `.rimeshbin` — cooked runtime mesh payload.
- `.ri_asset.json` — standardized import/cook descriptor.
- `package.ri_package.json` — package manifest inside `.ripak`.

## Legacy / Experimental Extension Family

These still exist as useful vocabulary and legacy/experimental aliases:

- `.ri_model`
- `.ri_mesh`
- `.ri_scene`
- `.ri_mat`
- `.ri_tex`
- `.ri_audio`
- `.ri_meshc`

Compact names remain preferred when they read well as real file types: `.rimodel`, `.riscene`, `.ritex`, and `.riaudio`.

## Pipeline bookkeeping (non-runtime)

These are development and interchange shapes, not replacements for cooked runtime payloads:

- [[Asset Extraction Inventory]] — extracted-archive inventory manifest for tooling and validation workflows.
- [[RawIron Package Format]] — package manifest contract for validated converted content.
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
