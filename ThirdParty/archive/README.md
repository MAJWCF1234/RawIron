# Archive Backend Slot

RawIron package files use `.ripak`, a ZIP-compatible container with a RawIron extension.

Current Windows tooling can create and extract `.ripak` files through PowerShell archive commands so the format can move immediately. This directory is reserved for the embedded third-party archive backend that should replace shell-based ZIP handling for cross-platform tools and editor/runtime integration.

Expected backend coverage:

- ZIP read/write for `.ripak` packages.
- ZIP read/extract for third-party `.zip` resource packs.
- Safe extraction with path traversal rejection.
- Optional adapters for other import archives such as `.unitypackage`, `.tar`, `.tar.gz`, and `.7z`.
- Third-party authoring files such as Blender `.blend` are import sources; archive/package tooling should preserve them at the source boundary only and emit RawIron-owned package outputs.

The archive backend must treat `package.ri_package.json` as metadata inside the package, not as the package itself.
