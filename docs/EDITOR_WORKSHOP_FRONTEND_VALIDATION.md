# Editor workshop frontend pass, 2026-10-08

The existing native workshop style remains: dark steel panels, copper/amber active
states, Hammer-style viewport tools, Bryce orbit/pan/zoom controls and a dense
SLADE-like resource workflow. This pass improves the existing editor rather than
replacing it with a different frontend.

## Changes

- Wider hierarchy with more vertical room; compact node-kind markers preserve
  space for names. Name overflow uses ellipsis rather than painting into type labels.
  Both existing hierarchy rendering paths receive the same treatment.
- Panel headers measure title width before placing metadata, avoiding overlapping
  labels. Collapsed panels use rotated dock labels instead of wrapping paragraphs
  into a 36-pixel rail. Clicking anywhere on a collapsed rail expands its panel.
- Catalog cards have 64-pixel thumbnails inside 104-pixel cards, readable two-line
  labels, space-separated display names and a clear `Place selected` action.
  Full cards and labels share their hit area. Columns adapt down to one on narrow views.
- Thumbnail previews use texture-independent clay materials and stronger ambient
  illumination. Cache revision 3 regenerates previews instead of reusing dark old
  images. Authored scene materials and geometry are unaffected by thumbnail styling.
- `--editor-state-root=<directory>` can isolate preview/test scene state, camera
  sidecars and logic authoring state. Normal launches retain the existing save paths.
  `--editor-preview` labels an isolated preview window. The alternate executable used
  during development exists only in the ignored build tree.

## Validation

The canonical `RawIron.Editor` executable rebuilt successfully. All 12 editor tests
passed, including the new `RawIron.Editor.CatalogLayoutSmoke`. That check covers
160/640/1040/1680-pixel viewport widths, all three catalog sections, final-page
clamping, card containment and clicks at label edges outside the thumbnail.
Existing viewport, plugin, scene-controller, workspace, game and scaffold checks pass.
Whitespace validation passes.

Logs: `build/editor-ui-final-build.log` and `build/editor-ui-tests.log`.
The whole engine suite was not rerun for this editor-only change.

Visual verification used the user-provided starting image and an inspected native
editor preview. The final canonical editor window was captured through Windows
PrintWindow after a screen-based capture was rejected because another window
occluded it. Accepted final image: `Saved/Validation/editor-ui/after.png`.
This validates the visible frontend, not rendering quality or full accessibility.

The updated editor was launched on Liminal Hall using the normal project state location.
Changes are local and uncommitted; no new GitHub synchronization was performed.

## Build palette follow-up

The catalog now provides a bounded text search across meshes, volumes and LogicKit
nodes. Matching is case-insensitive, treats underscores/hyphens as spaces and matches
all entered words. Results retain their original preset indices for spawning.
A clicked search owns keyboard input, preventing scene shortcuts from firing while
typing. Enter chooses the first result and arms Create mode; Escape ends typing.
Clicking outside ends typing. Clear restores the full current tab. Search updates
selection to a matching piece when necessary. Empty results block the Place button.
The footer identifies the selected piece and explains the active keyboard workflow.
Arch thumbnails now use a near-frontal camera so the opening remains visible;
geometry, proportions and scene cameras remain unchanged.

The canonical editor and catalog test rebuilt successfully. All 12 editor checks
passed after this follow-up. The expanded catalog check exercises every preset in
all three tabs with capitalized, space-separated queries, stable filtered indices,
blank searches, no matches, final-page clamping and the empty-placement guard.
The palette was rendered with the actual native GDI renderer and inspected at
`Saved/Validation/editor-ui/palette-search.png`; this is an offscreen palette capture,
not a capture of a complete running editor window. The attempted live preview was
closed before a usable capture was obtained. Keyboard routing was reviewed in code;
an end-to-end interactive keyboard test was not completed.

Artifact generation is optional: run `EditorCatalogLayoutSmoke.exe <output.bmp>`.
Latest logs: `build/editor-palette-build.log`, `build/editor-palette-tests.log`.
