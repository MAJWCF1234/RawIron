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

## Export checklist

Before an internal RawIron plugin/mod is exported for public RawIron:

- Confirm package identity fields are stable: `id`, `name`, `version`, `author`, `category`.
- Confirm `version` is a semantic triplet such as `1.0.0`.
- Confirm every public path is project-relative.
- Confirm hooks use current public aliases: `on_startup` or `on_runtime`.
- Confirm richer internal hook groups are preserved as metadata, not as required execution keys.
- Confirm load order is deterministic.
- Confirm the package can be disabled from `plugins/registry.json`.
- Confirm scripts use public `.riscript` markers or stubs.

Internal RawIron exporters should target this shape when producing public-compatible plugin/mod packages.
