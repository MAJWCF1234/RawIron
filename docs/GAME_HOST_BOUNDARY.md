# Engine host, Cube Test and Liminal Hall

`RawIron.GameHost` is an engine-owned library under `Source/`. It contains the shared
host implementation formerly under `Games/Common`. Existing `RawIron/Games/*`
generic helper includes and `RawIron::Game.RuntimeDiagnosticsDraw` aliases remain
compatible; they now resolve to this engine library.

The Cube Test game explicitly links `RawIron::GameHost`. Its app owns command-line
choices; its runtime owns gallery content, calibration fixtures, world assembly,
interaction bindings and the simulation callback. Engine host support owns:

1. Resolving the requested project manifest.
2. Rejecting a manifest whose runtime module differs from the executable's module.
3. Validating format and tuning before constructing the world or opening the game window.
4. Preparing manifest, authored support data and script services.
5. Constructing the shared runtime with the manifest's module identity.

`PrepareGameProject` returns a prepared project or a diagnosed failure. Cube Test's
play loop receives its prepared scripts rather than rereading those files. Runtime
startup registers the same script object as a service; shutdown restores the registry.
Other games keep using the compatible engine host helpers and can adopt the prepared
project API incrementally.

Liminal Hall now explicitly links `RawIron::GameHost`. Desktop and headless launch
use `PrepareGameProject` and `PreparedGameProject.CreateRuntime`; its software
benchmark also uses shared project preflight. Module/config validation completes
before audio, UI or world construction. Game initialization consumes the prepared
script bundle, including audio tuning, and headless launch forwards the original
arguments to runtime startup. Checkpoint save-path overrides pass through shared
boot options. See [Liminal Hall boundary validation](LIMINAL_HOST_BOUNDARY_VALIDATION.md).

This is a shared boot boundary, not dynamic loading of arbitrary game code into
`RawIron.Player`. The generic Player remains a separate host to integrate later.
Cube Test still owns its native presentation/input bindings and authored gallery layout.

`RawIron.GameHost.ProjectBootSmoke` verifies actual runtime startup, service ownership,
frame/shutdown, missing manifests, and rejection of the wrong game module.
