---
tags:
  - rawiron
  - assets
  - packages
  - pipeline
---

# RawIron Package Format

## Purpose

RawIron packages are the engine-owned handoff point between foreign source drops and runtime-ready content.

Unity packages, Unreal asset folders, loose FBX/OBJ/glTF/Blender drops, texture packs, and audio packs should be treated as import sources. After conversion, RawIron tooling should produce a `.ripak` file: a ZIP-compatible archive with a RawIron extension, similar in spirit to Minecraft `.mcworld`.

This gives the project a clean rule:

- source formats are allowed at the import boundary
- RawIron `.ripak` files are the canonical portable handoff
- runtime systems should depend on RawIron-owned outputs, not Unity project state
- foreign engine containers are conversion inputs, not distributable package payloads

## Package Container

The user-facing package format is:

```text
<package-id>.ripak
```

`.ripak` is a renamed ZIP archive. It should be possible to rename it to `.zip` and inspect it with normal ZIP tools, but RawIron tools should prefer the `.ripak` extension so projects can distinguish engine packages from arbitrary archives.

The internal manifest is:

```text
package.ri_package.json
```

That JSON file is not the package by itself. It is the package manifest inside the `.ripak`, or inside an exploded staging directory while tools are converting, validating, or debugging a package.

## Two Use Modes

RawIron packages support two project workflows.

Mounted package:

- The `.ripak` is copied into a project `Packages/` folder, or exploded under `Packages/<package-id>/` while editing/debugging.
- Files remain package-owned.
- The project can enable, disable, upgrade, or remove the package as a third-party resource pack.
- This is the right mode for marketplace packs, shared art libraries, test packs, and anything the project should not silently mutate.

Installed package:

- The package's standardized assets are copied into the project's normal folders.
- The project treats the installed outputs as if they were authored there.
- Scripts can land under `scripts/packages/<package-id>/`.
- Asset documents can land under `assets/packages/<package-id>/` or explicit `installPath` destinations.
- A package receipt is kept under `assets/package_receipts/`.

Installed mode is for committing imported content into a specific game. Mounted mode is for portable third-party packs.

## Archive Shape

The current `.ripak` internal shape is:

```text
<package-id>.ripak
  package.ri_package.json
  assets/
    <relative-source-layout>.<source-ext>.ri_asset.json
  scripts/
    <converted RawIron scripts>.riscript
  media/
    <standard images/audio when preserved directly>
```

The `assets/` tree preserves the source-relative layout where practical. Each file is a standardized `.ri_asset.json` descriptor. Images, sounds, and other standard media payloads can remain standard files inside the archive when the runtime/toolchain can consume them directly. RawIron-owned authored behavior uses `.riscript`, and future custom model/scene/runtime formats should live alongside the descriptors.

Foreign scripts must not remain the runtime authority. Unity `.cs`, UnityScript `.js`, Boo `.boo`, or Lua-like source from third-party packs can be retained as provenance, but the package should also contain a reconstructed RawIron script under `scripts/`. Early reconstruction may be a review-required `.riscript` stub that records lifecycle hooks and source identity; complete reconstruction should translate behavior into RawIron's own scripting vocabulary.

Foreign engine containers must not ship inside a finished `.ripak`. Files such as Unreal `.uasset` and Unity-only cache state can be cracked open during conversion to recover names, object paths, material settings, graph intent, and import references, but the package output should be RawIron-owned descriptors plus portable payloads such as FBX, glTF, PNG, WAV, and OGG. When a container cannot be reconstructed from local metadata, the importer can escalate to that engine's command-line/export tools, then package only the exported portable data and reconstructed RawIron files.

Tools may also use the same shape as an exploded staging directory:

```text
Assets/Cooked/Packages/<package-id>/
  package.ri_package.json
  assets/
```

The staging directory is not the distribution format. The `.ripak` is.

## Manifest Shape

`package.ri_package.json` is UTF-8 JSON inside the `.ripak`:

```json
{
  "formatVersion": 1,
  "packageId": "post_apocalypse",
  "displayName": "Post apocalypse",
  "packageKind": "resource-pack",
  "packageVersion": "1.0.0",
  "installScope": "either",
  "mountPoint": "Packages/post_apocalypse",
  "sourceRoot": "Assets/Source/Post apocalypse",
  "generatedAtUtc": "2026-04-30T21:00:00Z",
  "tags": ["environment", "props"],
  "dependencies": [
    {
      "packageId": "base_materials",
      "versionRequirement": ">=1.0.0",
      "optional": true
    }
  ],
  "conflicts": ["legacy_post_apocalypse"],
  "assets": [
    {
      "id": "ms_crate",
      "type": "mesh",
      "path": "assets/Crate/MS_Crate.fbx.ri_asset.json",
      "installPath": "assets/props/post_apocalypse/Crate/MS_Crate.fbx.ri_asset.json",
      "sourcePath": "Assets/Source/Post apocalypse/Crate/MS_Crate.fbx",
      "sizeBytes": 512,
      "signature": "fnv1a64:0123456789abcdef"
    }
  ]
}
```

## Required Fields

- `formatVersion`: package manifest schema version. Current value is `1`.
- `packageId`: stable machine ID for the package.
- `displayName`: human-facing package name.
- `packageKind`: package category. Current values are `asset-pack`, `resource-pack`, `script-pack`, and `mixed-pack`.
- `packageVersion`: semantic package version.
- `installScope`: `mounted`, `project`, or `either`.
- `mountPoint`: recommended project-relative mount location, usually `Packages/<package-id>`.
- `sourceRoot`: original source folder or archive root used for conversion.
- `generatedAtUtc`: UTC timestamp when the package manifest was generated.
- `tags`: optional search/filter labels.
- `dependencies`: optional package dependencies.
- `conflicts`: package IDs that cannot be active with this package.
- `assets`: complete list of package-local standardized asset documents.

Each asset entry must include:

- `id`: standardized asset ID from the `.ri_asset.json` document.
- `type`: standardized asset type such as `mesh`, `texture`, `material`, `audio`, or `scene`.
- `path`: package-relative path to a `.ri_asset.json` document.
- `installPath`: optional project-relative destination used by install-on-project mode.
- `sourcePath`: original source path recorded by the standardized asset document.
- `sizeBytes`: byte size of the `.ri_asset.json` document.
- `signature`: content signature for the `.ri_asset.json` document.

## Validation Rules

The package validator must fail the package when:

- `formatVersion` is unsupported.
- `packageId`, `displayName`, or `generatedAtUtc` is empty.
- `packageKind` is not one of the supported package kinds.
- `packageVersion` is empty or not a semantic triplet.
- `installScope` is not `mounted`, `project`, or `either`.
- `mountPoint` is absolute or contains `..`.
- `assets` is empty.
- any asset has an empty ID, type, path, source path, size, or signature.
- two assets share the same ID.
- two assets share the same package-relative path.
- two assets share the same explicit install path.
- an asset path is absolute or contains `..`.
- an asset install path is absolute or contains `..`.
- an asset path does not point to a `.ri_asset.json` document.
- tags, dependencies, or conflicts contain empty entries.
- dependencies or conflicts contain duplicate package IDs.
- the package depends on or conflicts with itself.
- a listed asset file is missing or is not a regular file.
- recorded size or signature differs from the package-local file.
- the asset document cannot be parsed.
- the document ID, type, or source path differs from the manifest entry.
- the package contains a `.ri_asset.json` document that is not listed in the manifest.

These checks make the package manifest a real completeness gate instead of a loose index.

## Current Tooling

Build a RawIron package from a source folder:

```text
ri_tool --asset-package-build <source-dir> [--output-dir <package-dir-or-file.ripak>] [--package <file.ripak-or-manifest>]
```

Validate an existing package:

```text
ri_tool --asset-package-validate <file.ripak-or-package-dir-or-manifest>
```

Import a package as a mounted third-party resource pack:

```text
ri_tool --asset-package-import <file.ripak-or-package-dir-or-manifest> [--project <project-root>] [--output-dir <mounted-package-dir>]
```

Install a package into a project as project-owned content:

```text
ri_tool --asset-package-install <file.ripak-or-package-dir-or-manifest> [--project <project-root>]
```

The build command standardizes supported source files into package-local `.ri_asset.json` documents, writes `package.ri_package.json`, validates the staging directory, then writes the `.ripak` archive. Current Windows tooling uses PowerShell ZIP support for archive create/extract; a vendored third-party archive backend should replace that for cross-platform and Unity `.unitypackage` extraction.

## Project Discovery

Projects should accept packages from:

- `<project-root>/<package-id>.ripak`
- `<project-root>/Packages/<package-id>.ripak`
- `<project-root>/Packages/<package-id>/package.ri_package.json`
- `<project-root>/packages/<package-id>/package.ri_package.json`

Discovery loads every manifest it can find and validates each package in place. A package with invalid validation data can still be surfaced to tooling as broken or unavailable, but it must not be mounted into runtime content without passing validation.

## Install Path Rules

If an asset entry has `installPath`, install mode copies that package-local `.ri_asset.json` to the exact project-relative destination.

If `installPath` is empty, tooling derives the destination:

- `script` or `behavior`: `scripts/packages/<package-id>/...`
- `scene`: `levels/packages/<package-id>/...`
- all other asset types: `assets/packages/<package-id>/...`

The project keeps a receipt at:

```text
assets/package_receipts/<package-id>.ri_package.json
```

Receipts make later audits, updates, and uninstall tools possible.

## Unity Conversion Role

Unity should stay at the importer boundary.

For Unity-derived content, RawIron package conversion should preserve enough information to reproduce engine behavior without requiring Unity at runtime:

- source file identity and stable asset IDs
- mesh/material/texture/audio/scene type classification
- Unity GUID and `.meta` relationships
- prefab hierarchy and component bindings
- script/component conversion into `.riscript` or explicit unsupported-script placeholders
- animation clips/controllers and shader references
- Unity mesh summary data where available
- source-relative layout
- deterministic package-local standardized documents
- validation that every converted output is present and accounted for

Unity `Library/`, `.vs/`, generated project files, and other tool caches are not package inputs. They should stay ignored or be removed before package build.

## Unreal Conversion Role

Unreal should also stay at the importer boundary.

For Unreal-derived content, RawIron conversion should:

- detect Unreal package files by extension and package tag
- extract object names, import paths, class names, material parameter labels, texture settings, and engine version tokens where possible
- use portable companion payloads such as FBX meshes and PNG textures as the package media
- reconstruct RawIron material documents, scene/entity descriptors, and `.riscript` behavior from the recovered metadata
- reconstruct native model surfaces when imported meshes lack the UVs or runtime shape needed by the original material
- map material graph concepts such as flipbook atlas animation, scalar phase parameters, nearest sampling, unlit shading, translucency, emissive output, and opacity into RawIron material/script fields
- mark partially inferred outputs as `generated-review-required`
- exclude `.uasset`, `.umap`, `DerivedDataCache/`, `Intermediate/`, `Saved/`, and Unreal project/build residue from the finished `.ripak`
- escalate to Unreal commandlets or headless editor export only when local token/name-table extraction is not enough

The goal is not to disguise provenance. The goal is that a RawIron package looks and behaves like RawIron content, with foreign engine files used only as licensed source material during conversion.

## Blender Conversion Role

Blender `.blend` files are third-party authoring sources. RawIron tooling should classify them as mesh/model import inputs, preserve them only at the source/import boundary, and convert them into RawIron-owned model/material/texture descriptors or standard interchange payloads during packaging. Runtime packages should not require Blender to be installed.

## Cleanup Rule

`Assets/Source` can be cleaned only after packages pass the full end-to-end gate:

1. source import or extraction is reproducible
2. package build succeeds
3. package validation succeeds
4. runtime/editor tools can load the package outputs needed by the game
5. tests cover the specific package families being cleaned

Until those gates exist for a source drop, the source folder remains the recovery copy.
