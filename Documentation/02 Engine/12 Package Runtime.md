# Package Runtime

RawIron uses `.ripak` as one package protocol for content, worlds, avatars, systems,
plugins, tools, and editor extensions. Package manifest version 2 adds the metadata
needed to inspect and resolve those package types without breaking version 1 asset packs.

## Manifest contract

A version 2 `package.ri_package.json` can declare:

- package identity, semantic version, kind, author, and description
- compatible RawIron engine API range and supported platforms
- required packages, optional packages, and conflicts
- provided and required capabilities
- requested permissions
- a `data`, `native`, `wasm`, or `lua` runtime entry point and ABI version
- content asset paths, canonical sizes, and integrity signatures

Data-only packages need no executable entry point. Executable packages must provide a
safe package-relative entry path that exists inside the package root.

## Resolution

`RawIron.Content` resolves the complete graph before activation. It selects compatible
versions deterministically, backtracks when the newest candidate makes the graph
unsatisfiable, rejects cycles and conflicts, enforces platform and engine API constraints,
checks permissions, and emits dependencies before dependents.

`PackageMountRegistry` turns that resolved graph into an atomic live mount set. Each
activation retains every dependency it uses. Releasing an activation walks its graph in
reverse order and unloads packages only when their reference count reaches zero, so two
worlds or avatars can safely share one system package. Version/root mismatches and virtual
mount-point collisions are rejected before registry state changes.

Runtime lookup is intentionally narrow: callers can resolve declared asset IDs, declared
virtual asset paths, or a validated executable entry point. Arbitrary files inside a
package are not exposed through the mount API. Asset IDs and virtual paths are indexed
when the package mounts, so steady-state lookup does not scan manifests or touch the
filesystem.

Inspect a graph from the command line:

```text
ri_tool --asset-package-resolve <package-id> --project <root>
ri_tool --asset-package-mount-check <package-id> --project <root>
```

Useful options include `--package-version`, `--engine-api`, `--platform`,
`--capabilities`, `--grant-permissions`, and `--include-optional-packages`.

## Security boundary

Capabilities describe what a package supplies or needs. Permissions describe privileged
operations the host may grant. Resolution rejects ungranted permissions, but it is not
an execution sandbox by itself. Native packages still require the native plugin host;
WASM and Lua execution hosts remain separate runtime work.

The current `fnv1a64:` value is a deterministic integrity checksum over canonical LF text.
It detects package corruption and makes Windows/Git line-ending conversion stable. It is
not a cryptographic publisher signature. Trust stores, public-key signing, revocation,
and isolated executable-package hosts are later security layers.

## Compatibility

Manifest version 1 remains readable. Its legacy `asset-pack`, `world-pack`, and
`plugin-pack` kinds continue to validate, with an implicit unrestricted engine API range
and data-only runtime.
