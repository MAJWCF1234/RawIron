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
  Normalized engine-owned representation
- **Cooker layer**
  Produces runtime-ready binaries per platform and build target
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
