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
- explicit engine-owned hook group metadata
- executable hook bindings in `plugins/hooks.riplugin`
- deterministic load order
- project-relative paths only
- no private internal-engine assumptions
- local entries must resolve inside the game root and exist before the plugin becomes active

Current public hook bindings:

- `startup=pluginId` or `on_startup=pluginId` maps to `startup` / `bootstrap`
- `runtime=pluginId` or `on_runtime=pluginId` maps to `runtime` / `frame_sample`
- descriptor keys such as `runtime.mod=pluginId` are preserved as engine-owned hook phases with a default event

## Export checklist

Before an internal RawIron plugin/mod is exported for public RawIron:

- Confirm package identity fields are stable: `id`, `name`, `version`, `author`, `category`.
- Confirm `version` is a semantic triplet such as `1.0.0`.
- Confirm every public path is project-relative.
- Confirm hooks use current public bindings (`on_startup` / `on_runtime`).
- Confirm richer internal hook groups stay in descriptor/registry metadata until engine mapping lands.
- Confirm load order is deterministic.
- Confirm the package can be disabled from `plugins/registry.json`.
- Confirm scripts use public `.riscript` markers or stubs.
- Run `ri_tool --plugin-package-validate <package-root>` and `ri_tool --plugin-package-build <package-root> --output <id.ripak>`.
- Run `ri_tool --extension-validate <package.riplugin.json>` and `ri_tool --plugins-doctor --game <id>` after installation.

Internal RawIron exporters should target this shape when producing public-compatible plugin/mod packages.
