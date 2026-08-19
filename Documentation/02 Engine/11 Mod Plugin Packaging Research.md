# RawIron Public Mod and Plugin Packaging Contract

This note records the public-engine packaging contract for mods, plugins, and ripacks. It is intentionally conservative: public RawIron should be stable, discoverable, and easy to validate, while internal RawIron can continue experimenting and export into this public contract.

## Real-world packaging patterns checked

- Unity packages use a JSON package manifest to describe a specific package version. Unity requires stable identity and version fields, recommends display metadata and descriptions, and supports dependency maps plus documentation/license/changelog URLs.
- Unreal Engine discovers plugins from descriptor files in known plugin roots. Plugins can contain code, content, modules, metadata, config, and enable/disable state. Unreal also separates project plugins from engine plugins and treats plugin descriptors as the discovery unit.
- Thunderstore-style mod packages center on a manifest plus sidecar presentation files such as README and icon, with versioned package identity and dependencies.
- Mod distribution systems generally benefit from official tooling, validation, and structured issue/reporting paths instead of ad-hoc user bug reports.

## Current Shipped Contract

RawIron treats asset packages and editor-discoverable plugin packages as different surfaces today. They should not be presented as one finished public mod archive format.

### Asset and mixed packages

`ri_tool` ships `--asset-package-build`, `--asset-package-validate`, `--asset-package-import`, and `--asset-package-install`. These commands operate on `package.ri_package.json` and can create or consume ZIP-compatible `.ripak` archives.

The manifest contract includes stable identity, version, kind, install scope, dependencies, conflicts, asset records, size/signature validation, and safe package-relative paths. The current archive reader expects `package.ri_package.json` at the archive root; one-folder-wrapped archives are not supported yet.

### Editor plugin-store packages

The editor discovers store packages as folders containing `package.riplugin.json`, then installs their manifest, load order, registry, and hook lines into a selected game project. The template under `Plugins/Templates/PublicPluginPackage` is the current public starting point for this flow.

A project-side plugin package uses these control-plane files:

```text
<PluginPackage>/
  package.riplugin.json
  README.md
  LICENSE.md                optional
  icon.png                  optional
  plugins/
    manifest.plugins       plugin registry rows
    registry.json          enable/disable and metadata state
    hooks.riplugin         executable hook aliases/bindings
    load_order.cfg         explicit ordering
  scripts/
    plugins.riscript       optional plugin tuning markers
```

## Stable identity fields

Use stable ids for lookup and user-friendly labels for UI. Asset-package manifests enforce these fields today; plugin-store descriptors carry the same intent but are not yet validated or archived by the asset-package commands.

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

Descriptor-style hook groups such as `runtime.mod` are parsed into project data, but currently resolve to the `default` event and are reported when no handler exists. They are metadata, not an executable public hook path yet. Public packages should use `on_startup` or `on_runtime` until direct descriptor hooks are implemented.

This mirrors the useful parts of real-world systems: descriptor discovery, project-level enable/disable, dependency/ordering metadata, and content/code separation.

## Active Packaging Backlog

- [x] Add `--plugin-package-build` to package a plugin folder into a `.ripak` with its descriptor, README/LICENSE/icon, and project control-plane files (`ri_tool --plugin-package-build`, `PlanPluginPackageArchive` / `StagePluginPackageArchive`).
- [x] Add `--plugin-package-validate` to verify a plugin descriptor, registry, hooks, load order, and safe paths before install (`ri_tool --plugin-package-validate`, `ValidatePluginPackage`; accepts package folders and built `.ripak` archives). Package-root resolution refuses symlink/reparse directories and a directory decoy named `package.riplugin.json`.
- [ ] Resolve `package.ri_package.json` from a single top-level directory after `.ripak` extraction, without weakening path validation.
- [ ] Add a mod load report that explains each plugin's loaded, skipped, policy-blocked, disabled, and handler-missing state.
- [ ] Map descriptor hook groups such as `runtime.mod` to deliberate engine phases/events instead of the current unhandled `default` event.
- [ ] Add an internal `Export Public Ripack v1` preset that normalizes richer internal metadata into the supported public asset and plugin contracts.

`--plugin-package-validate` and `--plugin-package-build` are shipped. Next packaging steps are archive install/import for plugin packages into a game project, then the remaining backlog items above.

## Design line

RawIron Public should not chase every internal metadata experiment. It should define a stable public shape. Internal RawIron should export into that shape when forward-porting content, plugins, and mods.
