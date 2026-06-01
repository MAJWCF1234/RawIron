# Plugins and Mods

Plugins and mod-governance are built into the project format.

## Required plugin surfaces

Every current game contract includes:

- `scripts/plugins.riscript`
- `config/plugins.policy`
- `plugins/manifest.plugins`
- `plugins/load_order.cfg`
- `plugins/registry.json`
- `plugins/hooks.riplugin`

## Related security surface

Games also carry `config/security.policy`, which sits alongside plugin policy as part of project governance.

## What these files do

- `scripts/plugins.riscript`: authored plugin-related tuning and runtime declarations
- `config/plugins.policy`: permission and policy baseline for plugin loading
- `plugins/manifest.plugins`: plugin inventory
- `plugins/load_order.cfg`: plugin ordering
- `plugins/registry.json`: plugin registry metadata
- `plugins/hooks.riplugin`: hook declarations consumed by runtime and editor surfaces

## Project ownership

Plugins and mods are not hidden internal features. They are first-class authored project content and are included in the shipped workspace.

## Typical policy posture

Project policies can control things like script allowance and unsigned plugin allowance. Those policies belong in project config, not scattered through game code.
