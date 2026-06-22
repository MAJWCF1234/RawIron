# Public RawIron Plugin Package Template

This folder is a reference package shape for public RawIron plugins and mods.

Copy this folder, rename the package id, then update:

- `package.riplugin.json`
- `plugins/manifest.plugins`
- `plugins/registry.json`
- `plugins/hooks.riplugin`
- `plugins/load_order.cfg`
- `scripts/plugins.riscript`

The public contract is intentionally simple:

- stable plugin id
- semantic version triplet
- explicit hook group metadata
- executable hook aliases in `plugins/hooks.riplugin`
- deterministic load order
- project-relative paths only
- no private internal-engine assumptions

Current public loader hook aliases:

- `on_startup=pluginId` maps to `startup` / `bootstrap`
- `on_runtime=pluginId` maps to `runtime` / `frame_sample`

The descriptor `hookGroup` remains useful metadata for the editor/store/exporter, but `plugins/hooks.riplugin` should use an executable alias the public loader can currently resolve.

Internal RawIron exporters should target this shape when producing public-compatible plugin/mod packages.
