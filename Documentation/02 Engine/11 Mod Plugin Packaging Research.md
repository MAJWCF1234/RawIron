# RawIron Public Mod and Plugin Packaging Research

This note records the public-engine packaging contract for mods, plugins, and ripacks. It is intentionally conservative: public RawIron should be stable, discoverable, and easy to validate, while internal RawIron can continue experimenting and export into this public contract.

## Real-world packaging patterns checked

- Unity packages use a JSON package manifest to describe a specific package version. Unity requires stable identity and version fields, recommends display metadata and descriptions, and supports dependency maps plus documentation/license/changelog URLs.
- Unreal Engine discovers plugins from descriptor files in known plugin roots. Plugins can contain code, content, modules, metadata, config, and enable/disable state. Unreal also separates project plugins from engine plugins and treats plugin descriptors as the discovery unit.
- Thunderstore-style mod packages center on a manifest plus sidecar presentation files such as README and icon, with versioned package identity and dependencies.
- Mod distribution systems generally benefit from official tooling, validation, and structured issue/reporting paths instead of ad-hoc user bug reports.

## RawIron public contract

RawIron Public should treat package metadata as a compatibility contract, not just loose JSON.

A public RawIron mod/plugin package should prefer this layout:

```text
<MyPackage>/
  package.ri_package.json
  README.md
  LICENSE.md
  icon.png                 optional
  assets/                  asset documents and source/cooked references
  scripts/                 riscript entry points or reconstructed stubs
  plugins/
    manifest.plugins       plugin registry rows
    registry.json          enable/disable and metadata state
    hooks.riplugin         executable hook aliases/bindings
    load_order.cfg         explicit ordering
```

A public `.ripak` should unpack to either the contract root above or a single top-level folder containing that contract root. Tooling should prefer root manifests but may support one-folder-wrapped archives as a compatibility affordance.

## Stable identity fields

Use stable ids for lookup and user-friendly labels for UI.

- Package id: machine-stable, lowercase, safe characters, no spaces.
- Display name: human-facing name.
- Version: semantic triplet, such as `1.2.3`.
- Package kind: `asset-pack`, `resource-pack`, `script-pack`, or `mixed-pack`.
- Install scope: `mounted`, `project`, or `either`.
- Dependencies: package id plus version requirement, with optional dependencies marked explicitly.
- Conflicts: package ids that must not be active with this package.

## Plugin/mod behavior fields

RawIron should preserve the existing split:

- `manifest.plugins`: what plugin entries exist.
- `registry.json`: whether they are enabled and how they are presented.
- `hooks.riplugin`: where they attach to runtime/editor/game hooks.
- `load_order.cfg`: deterministic ordering when multiple plugins hook the same phase.

Current public loader key/value hook aliases:

- `on_startup=pluginId` maps to the `startup` hook phase and `bootstrap` event.
- `on_runtime=pluginId` maps to the `runtime` hook phase and `frame_sample` event.

Descriptor-style hook groups such as `runtime.mod` should remain in registry/package metadata until the public loader grows direct descriptor hook support. Internal RawIron exporters should emit currently executable aliases for public builds.

This mirrors the useful parts of real-world systems: descriptor discovery, project-level enable/disable, dependency/ordering metadata, and content/code separation.

## Compatibility policy for internal RawIron

Internal RawIron may have richer metadata, but public export should normalize into the public contract:

- Strip private-only fields or move them into a namespaced payload object.
- Emit canonical public fields even if internal uses aliases.
- Keep public package signatures and sizes accurate after export.
- Avoid absolute paths and `..` path traversal.
- Prefer `.ri_asset.json` entries for package manifest assets.
- Put experimental fields under an `extensions` or `metadata` object so old public builds can ignore them.
- Emit `on_startup`/`on_runtime` hook aliases for current public loader compatibility while preserving richer hook group metadata separately.

## Public parser policy

Public RawIron should be strict about security and structure, but forgiving about harmless naming drift.

Strict:

- relative paths only
- no `..`
- deterministic install locations
- matching file signatures and sizes
- no duplicate asset ids, paths, or install paths
- clear validation errors

Forgiving:

- harmless field aliases in `.ri_asset.json`
- extra unknown metadata fields
- optional README/icon/license files
- one-folder-wrapped ripacks

## Improvements already made on this branch

- Editor RGBA GDI blit now guards allocation/selection failures.
- Plugin install now creates required folders before metadata writes to avoid partial installs.
- Asset document parsing accepts common aliases such as `assetId`, `assetType`, `name`, `source`, and `metadata` while still serializing canonical public RawIron fields.
- Public plugin package template now uses hook aliases that the current public loader actually resolves.

## Next recommended improvements

1. Add a `--plugin-package-build` command that packages a plugin folder into a `.ripak` with README/LICENSE/icon plus plugin metadata.
2. Add a `--plugin-package-validate` command that verifies manifest, registry, hooks, load order, safe paths, and dependencies.
3. Teach `.ripak` extraction/import to find `package.ri_package.json` inside a single top-level folder wrapper.
4. Add a mod load report that explains why each plugin was loaded, skipped, blocked by policy, or disabled.
5. Add direct descriptor hook support so `runtime.mod=pluginId` and similar hook groups can map into engine-owned hook phases intentionally instead of being treated as loose script authority.
6. Add a compatibility export preset in internal RawIron: `Export Public Ripack v1`.

## Design line

RawIron Public should not chase every internal metadata experiment. It should define a stable public shape. Internal RawIron should export into that shape when forward-porting content, plugins, and mods.
