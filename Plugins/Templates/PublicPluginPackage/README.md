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
- explicit hook group
- deterministic load order
- project-relative paths only
- no private internal-engine assumptions

Internal RawIron exporters should target this shape when producing public-compatible plugin/mod packages.
