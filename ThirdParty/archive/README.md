# Archive Backend Slot

RawIron package files use `.ripak`, a ZIP-compatible container with a RawIron extension.

`ri_tool` now reads and extracts `.ripak` itself. Its bounded classic-ZIP parser is in
`Tools/ri_tool/src/SecureRipakArchive.cpp`; raw DEFLATE decoding reuses the pinned
public-domain/MIT `stb_image` implementation already vendored under
`Source/RawIron.Render.Software/third_party/stb`. No extractor or dependency is fetched
at runtime. Windows package creation still invokes PowerShell `Compress-Archive`; archive
creation is not an untrusted-input boundary, but it remains a portability item.

Current extraction coverage:

- classic single-disk ZIP with stored or raw-DEFLATE file entries
- signed and unsigned classic data descriptors
- central/local header, size, CRC, range, overlap, and descriptor agreement checks
- portable relative entry-name validation and host-filesystem collision detection
- rejection of ZIP64, encryption, masked headers, links, reparse entries, special Unix
  types, unsupported methods/flags, traversal, rooted/device paths, ambiguous names,
  duplicates, and file/directory prefix collisions
- fixed limits of 512 MiB archive bytes, 16384 entries, 256 MiB expanded bytes per file,
  1 GiB total expanded bytes, and 200:1 per-file expansion
- exclusive temporary roots, exclusive no-follow final file creation, actual expanded-size
  and CRC checks, and owned cleanup after success or failure

ZIP64, split archives, encrypted archives, symbolic links, sparse files, and non-stored/
non-DEFLATE methods are intentionally unsupported and rejected before payload writes.
The decoder holds one bounded DEFLATE entry in memory. It validates the actual decoded
size and CRC, but the current stb API does not report whether a valid DEFLATE end marker
consumed every declared compressed byte; trailing compressed-stream bytes are therefore a
documented residual ambiguity, not a path or output-budget bypass.

Future backend coverage:

- native/cross-platform ZIP writing for `.ripak` packages
- optional adapters for other import archives such as `.unitypackage`, `.tar`, `.tar.gz`,
  and `.7z`, each behind the same containment and resource-budget contract
- Third-party authoring files such as Blender `.blend` are import sources; archive/package tooling should preserve them at the source boundary only and emit RawIron-owned package outputs.

The archive backend must treat `package.ri_package.json` as metadata inside the package, not as the package itself.
