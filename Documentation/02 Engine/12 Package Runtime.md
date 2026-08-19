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

## Shared session extensions

RawIron treats a **session extension** as the network-visible form of a package, mod,
plugin, data pack, or external integration. It is an engine capability: any game can
adopt it and decide its own policy; sample projects merely exercise it.

`SessionExtensionContract` is a bounded, canonical list of extension id, version,
package-content fingerprint, provided capabilities, kind, and reload policy. Content
builds descriptors from validated installed packages and rechecks every declared asset
before producing the package fingerprint. The fingerprint proves that peers selected the
same bytes under the current integrity model; it is not publisher authentication.

An authoritative host may set `AuthoritativeNetConfig::requireSessionExtensionAgreement`.
Before a client can send authority gameplay traffic or receive simulation snapshots, the
host sends its contract and the client must acknowledge the exact fingerprint. A mismatch
leaves that peer outside gameplay and emits `net.session_extensions.rejected`; an exact
match emits `net.session_extensions.accepted` on both sides. This applies equally to a
dedicated server, listen host, editor playtest, or any custom game runtime.

Reload policy expresses the minimum safe boundary rather than forcing one game style:

- `frame-boundary`: data/assets and visual integrations
- `simulation-boundary`: gameplay systems whose state/schema can be synchronized
- `session-restart`: native modules until an explicit state-migration and isolated-code host exists

`SessionExtensionCoordinator` is the generic host-side state machine for a live proposal:
stage a normalized contract, require acknowledgement from the selected peers, and activate
only at or after its declared simulation tick. It intentionally does not mount code itself;
the host uses the accepted boundary to run its own package staging, state migration, and
rollback actions.

The first protocol intentionally performs preflight only. Package acquisition, host
approval UI, replicated schema registration, state migration, and coordinated live
activation are separate layers; RawIron will not hot-swap arbitrary native code in an
active multiplayer simulation.

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
decoding. The pinned stb decoder verifies actual output size and CRC; because it does not
expose a consumed-input cursor, extraction also rejects entries whose declared compressed
region still fully expands after dropping the final compressed byte (trailing garbage after
a valid DEFLATE end marker). `--asset-package-install` stages every asset and the install
receipt under an exclusive system-temporary root, then promotes into the project with
per-file backup and rollback so a mid-install failure restores prior project contents.
Existing files are replaced from an exclusive sibling temp rather than truncated in place.
Publisher authentication remains a separate trust layer. Windows adversarial extraction is
covered in CI; POSIX code paths compile by design but require their platform CI lane for
runtime proof.

The current `fnv1a64:` value is a deterministic integrity checksum over canonical LF text.
It detects package corruption and makes Windows/Git line-ending conversion stable. It is
not a cryptographic publisher signature. Trust stores, public-key signing, revocation,
and isolated executable-package hosts are later security layers.

## Compatibility

Manifest version 1 remains readable. Its legacy `asset-pack`, `world-pack`, and
`plugin-pack` kinds continue to validate, with an implicit unrestricted engine API range
and data-only runtime.
