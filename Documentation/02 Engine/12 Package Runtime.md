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
when the package mounts so lookup does not re-scan manifests, but every resolve
re-opens the cached path reparse-safe and re-checks that the final path still lies
under the package root. That blocks post-mount symlink/junction swaps from escaping
the package; it is intentionally not a pure in-memory path table.

Inspect a graph from the command line:

```text
ri_tool --asset-package-resolve <package-id> --project <root>
ri_tool --asset-package-mount-check <package-id> --project <root>
```

Useful options include `--package-version`, `--engine-api`, `--platform`,
`--capabilities`, `--grant-permissions`, and `--include-optional-packages`.

## Game package requirements

Games declare their package roots in `assets/dependencies.json`. The game runtime mounts
all required roots as one transaction before it starts plugin hooks; optional roots are
attempted independently and leave a diagnostic if unavailable. Game-local `Packages/`
entries take precedence over a matching shared workspace package while developing.

```json
{
  "engineApiVersion": "1.0.0",
  "capabilities": ["game.runtime"],
  "permissions": ["world.spawn"],
  "packages": [
    { "id": "studio.base", "version": "^1.0.0" },
    { "id": "studio.photo-mode", "version": "^1.0.0", "optional": true }
  ]
}
```

An absent `dependencies.json`, or one without `packages`, remains a valid legacy game
configuration. Validate a game's mount graph without launching it with:

```text
ri_tool --game-package-mount-check --game <id>
```

## Security boundary

Capabilities describe what a package supplies or needs. Permissions describe privileged
operations the host may grant. Resolution rejects ungranted permissions, but it is not
an execution sandbox by itself. Native packages still require the native plugin host;
WASM and Lua execution hosts remain separate runtime work.

Manifest file, mount, runtime-entry, and project `installPath` values use portable
forward-slash-relative syntax. Validation rejects absolute/rooted paths, Windows drive
and device forms, backslashes, `.`/`..`, empty components, reserved device names, and
other cross-platform ambiguous components. Project installation canonicalizes every
asset and receipt destination and verifies existing symlink/reparse components remain
inside the canonical project root before the first project write. Planned destinations
are collision-checked with ordinal Unicode case-insensitive comparison on Windows and a
portable ASCII case-insensitive policy elsewhere. Existing multi-link files are rejected
so overwrite cannot mutate a hard-link alias outside the project. Paths are checked again
immediately before each copy.

Archive extraction is an explicit boundary before manifest parsing. `ri_tool` parses
ZIP central and local records itself and only accepts classic, single-disk stored or
DEFLATE entries. It rejects ZIP64, encryption, masked headers, links/reparse entries,
special Unix file types, unsupported flags/methods, inconsistent headers/descriptors,
overlapping regions, duplicate/platform-colliding names, file/directory prefix
collisions, and every path form rejected by the package-relative path contract. Nothing
is extracted until every entry has passed metadata and containment preflight.

The fixed extraction ceilings are 512 MiB of archive input, 16384 entries, 256 MiB
expanded bytes per file, 1 GiB total expanded bytes, and a 200:1 per-file compression
ratio. Output is counted and CRC-checked after stored streaming or bounded DEFLATE decode;
metadata alone is not treated as proof of the produced size. Extraction uses an
exclusive system-temporary root and exclusive final-file creation, rechecks containment
and link/reparse components before writes, and owns recursive cleanup on both success and
failure. PowerShell `Expand-Archive` is no longer used. Windows package creation still
uses `Compress-Archive`; extraction is native and does not fetch a backend at runtime.

Current format limits are intentional: no ZIP64, split, encrypted, linked, sparse, or
non-stored/non-DEFLATE package entries. One bounded DEFLATE file is held in memory while
decoding. The pinned stb decoder verifies actual output size and CRC but does not expose
its final compressed-input cursor, so harmless bytes following a valid DEFLATE end marker
inside an entry cannot yet be distinguished from fully consumed input. Publisher
authentication and transactional multi-file project installation remain separate trust
layers. Windows adversarial extraction is covered in CI; POSIX code paths compile by
design but require their platform CI lane for runtime proof.

The current `fnv1a64:` value is a deterministic integrity checksum over canonical LF text.
It detects package corruption and makes Windows/Git line-ending conversion stable. It is
not a cryptographic publisher signature. Trust stores, public-key signing, revocation,
and isolated executable-package hosts are later security layers.

## Compatibility

Manifest version 1 remains readable. Its legacy `asset-pack`, `world-pack`, and
`plugin-pack` kinds continue to validate, with an implicit unrestricted engine API range
and data-only runtime.
