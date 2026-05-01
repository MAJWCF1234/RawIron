---
tags:
  - rawiron
  - assets
  - pipeline
---

# Asset Pipeline

## Requirement

RawIron is expected to support roughly:

- about **30** model and texture source formats
- about **12** audio source formats

## Core Rule

Support **many input formats**, but only **a small number of internal engine formats**.

The runtime should not directly depend on dozens of third-party source formats.

## Pipeline Shape

1. Source assets are imported from external formats
2. Importers normalize them into RawIron canonical structures
3. Cookers convert canonical assets into runtime-ready engine assets
4. The runtime loads only RawIron-owned formats

## Current Implemented Tooling

The current native tooling starts with standard asset documents rather than final packed runtime binaries.

Implemented today:

- `ri_tool --formats` reports `.ri_asset.json` as the current standard asset document.
- `ri_tool --asset-standardize <source-path>` writes one standardized document.
- `ri_tool --asset-standardize-dir <source-dir>` batch-writes standardized documents.
- `ri_tool --asset-package-build <source-dir>` converts a source tree into an exploded staging folder and writes a ZIP-compatible `.ripak` package.
- `ri_tool --asset-package-validate <file.ripak-or-package-dir-or-manifest>` verifies the internal package manifest against package-local asset documents.
- `ri_tool --asset-package-import <file.ripak-or-package-dir-or-manifest>` mounts a third-party package under a project `Packages/` folder.
- `ri_tool --asset-package-install <file.ripak-or-package-dir-or-manifest>` installs package contents into project-owned standard folders.
- Outputs default under `Assets/Cooked/Standardized`.
- Supported standardization inputs currently include Unity-style `.asset`, `.spm`, `.fbx`, `.obj`, `.gltf`, `.glb`, common image formats, common audio formats, `.mat`, and `.unity`.

This is the pipeline's metadata, normalization, and package-manifest stage, not the final packed binary runtime payload.

## Why This Matters

- faster load times
- cleaner runtime code
- less platform-specific parser baggage
- easier validation and versioning
- more consistent materials, meshes, textures, and audio behavior
- better long-term tooling

## Layers

- **Importer layer**
  Reads foreign file formats
- **Canonical asset layer**
  Normalized engine-owned representation; `.ri_asset.json` is the current implemented document shape
- **Package layer**
  ZIP-compatible `.ripak` package containing `package.ri_package.json`, package-local asset documents, standard media payloads, and RawIron-owned authored files such as `.riscript`; packages can be mounted like resource packs or installed into project-owned content folders
- **Cooker layer**
  Produces runtime-ready binaries per platform and build target; still a next layer beyond current standardization output
- **Runtime loader**
  Loads only cooked RawIron assets

## Important Consequence

The hard problem is not merely "support 42 formats."

The real challenge is:

- mesh semantics
- coordinate system conversion
- tangents, normals, and skinning
- material translation
- color space and compression decisions
- animation data consistency

That is why the pipeline must be a first-class subsystem from the start.

## Related Notes

- [[Asset Extraction Inventory]] — generated manifest for archive discovery, extraction status, and unpacked outputs (tooling and validation, not runtime gameplay).
- [[Declarative Model Definition]] — data-driven model composition interchange for parts, transforms, materials, and tags.
- [[RawIron Package Format]] — package manifest contract for validated converted content.
