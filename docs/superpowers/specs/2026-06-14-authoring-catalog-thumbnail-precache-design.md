# Authoring Catalog Thumbnail Precache Design

## Goal

Improve RawIron Editor responsiveness and catalog visual quality by replacing live or on-demand catalog thumbnail generation with a boot-time precache pass that produces high-quality clickable image icons for all authoring presets.

After boot:

- the authoring catalog should only blit cached images
- scrolling and page changes should not trigger live preview scene rendering
- temporary preview scenes and model data used to generate thumbnails should be disposed immediately
- thumbnails should automatically refresh when missing, stale, or source inputs change

## Problem Statement

The current authoring catalog relies on thumbnail generation logic that is too close to the live rendering path. Even with an existing `StructuralThumbnailCache`, the editor still pays too much cost around thumbnail readiness and preview generation behavior. This contributes to:

- sluggish catalog browsing
- extra redraw/render pressure during editor interaction
- lower-than-desired icon quality because thumbnails are treated as runtime previews instead of prebaked UI assets
- unnecessary retention of preview-generation state beyond the moment it is useful

The user wants the catalog to behave more like a ready-to-go icon atlas:

- thumbnails prepared on boot
- better quality than the current runtime-feel previews
- no repeated catalog rendering work during normal editing
- periodic or forced refresh only when needed

## Desired User Experience

When the editor boots:

1. It performs a thumbnail precache verification pass.
2. Missing or invalid thumbnails are rebuilt before normal catalog use.
3. The authoring catalog opens with fully ready image icons.
4. Clicking, paging, and scrolling the catalog only manipulates UI state and blits cached images.

When assets or preset definitions change:

- the editor detects that cached thumbnails are stale
- stale entries are rebuilt during the next boot precache pass
- users may also trigger a forced recache

When no relevant content changed:

- the editor reuses the existing cached icons
- no thumbnail regeneration work occurs

## Scope

This design covers:

- structural, volume, and logic catalog thumbnails
- boot-time thumbnail precache
- cache invalidation and refresh rules
- higher-quality thumbnail rendering settings for catalog assets
- disposal of preview-generation scene/model state after icon creation

This design does not cover:

- solving the full software viewport performance architecture
- replacing the main editor viewport with a GPU renderer
- changing the semantics of spawn/preset placement

## Recommended Approach

Use a boot-time unified thumbnail precache system backed by persisted image files and metadata.

The editor should treat catalog thumbnails as durable UI assets, not live mini-previews. A single catalog thumbnail cache should own all icon generation and reuse rules for every catalog section.

## Architecture

### 1. Unified Catalog Thumbnail Cache

Introduce or expand a cache abstraction that owns:

- thumbnail image pixels for each preset
- persistent image file path for each preset
- metadata file path for each preset
- preset fingerprint
- generation timestamp
- cache version
- stale/missing state

This cache should become the single source of truth for all catalog icon retrieval.

### 2. Boot-Time Precache Pass

At editor startup, before catalog interaction begins:

1. Enumerate all presets in all authoring catalog sections.
2. For each preset:
   - compute a fingerprint
   - load cached metadata if present
   - decide whether the icon is valid or must be rebuilt
3. Generate any missing or stale thumbnails.
4. Load final image data into the in-memory thumbnail cache used by the UI.

This pass should be considered part of editor readiness for the catalog.

### 3. Persistent Cache Artifacts

Persist generated thumbnail images and metadata under an editor-owned cache directory so boot does not have to rerender every icon every time.

Each cached preset should have:

- a high-quality image file, likely PNG
- a metadata sidecar describing:
  - fingerprint
  - generated-at timestamp
  - generator version
  - source section/preset identity

### 4. Generation Pipeline

For each preset requiring regeneration:

1. Build a temporary preview scene or wireframe representation.
2. Render a higher-quality thumbnail using catalog-specific preview settings.
3. Save the resulting image to the persistent cache location.
4. Save metadata sidecar.
5. Dispose temporary scene/model/render-generation state immediately.

Only the final image and metadata remain after generation.

### 5. Catalog Draw Path

Catalog drawing should become strictly image-backed:

1. Get cached image for visible cell.
2. Blit image.
3. Draw label and selection/hover chrome.

No live thumbnail scene construction or thumbnail rendering should happen during ordinary catalog browsing.

## Cache Refresh Rules

A cached thumbnail must be regenerated when any of the following is true:

- image file is missing
- metadata file is missing
- cache version changed
- preset fingerprint changed
- dependent texture/material timestamp changed
- cached image age exceeds 7 days
- user explicitly requests forced recache

If none of those conditions is true, the cached icon is reused.

## Fingerprinting Strategy

Each preset thumbnail fingerprint should include:

- catalog section
- preset identity or type id
- structural preset shape/config data, if structural
- wireframe/render mode relevant to the preset
- material or texture dependency identity and timestamps where applicable
- thumbnail-generator version number

The fingerprint should be deterministic and stable across boots.

## Quality Targets

Because thumbnails are precached rather than rendered in the hot UI path, they can use better settings than the current runtime-feel icons.

Recommended quality profile:

- larger internal render resolution than current thumbnails
- stable camera framing per preset type
- cleaner texture sampling than the current low-latency path
- optional mild sharpen/postprocess only if it improves legibility
- consistent lighting/background treatment across catalog sections

The objective is readable, attractive thumbnails that feel like authored UI assets rather than rough live previews.

## Performance Targets

After this system lands:

- catalog scroll and paging should avoid live scene preview generation
- catalog redraw cost should primarily be GDI image blits and text
- boot may spend time validating/regenerating icons, but normal browsing should be materially cheaper
- temporary preview models/scenes should not remain resident once thumbnails are generated

## Persistence and Paths

Use a dedicated editor cache location under the RawIron saved/editor data area so the cache is writable and isolated from source assets.

Suggested contents:

- thumbnail image files
- metadata sidecars
- optional manifest/index file for bulk verification

The cache should be safe to delete; boot regeneration rebuilds it.

## Forced Recache

Provide an explicit force-recache path so the user can rebuild the catalog icons on demand.

Expected behavior:

- delete or invalidate thumbnail metadata/images
- rerun generation pass
- repopulate in-memory cache from fresh outputs

## Failure Handling

If a thumbnail fails to generate:

- write an obvious fallback icon
- mark the entry as failed in metadata
- keep the editor usable
- surface a concise status message

Generation failure for one preset must not block the rest of the catalog.

## File Impact

Primary likely implementation files:

- `Apps/RawIron.Editor/src/EditorStructuralPicker.h`
- `Apps/RawIron.Editor/src/EditorStructuralPicker.cpp`
- `Apps/RawIron.Editor/src/EditorAuthoringCatalog.cpp`
- `Apps/RawIron.Editor/src/main.cpp`

Potential secondary impact:

- renderer helpers used for thumbnail image blitting
- saved/editor cache path utilities

## Testing

### Functional

- boot with empty cache generates all thumbnails
- boot with valid cache reuses all thumbnails
- deleting one thumbnail regenerates only that thumbnail
- changing a preset fingerprint regenerates the affected thumbnail
- forced recache rebuilds all thumbnails
- 7-day stale threshold triggers regeneration

### Performance

- measure catalog browsing without live thumbnail generation
- verify paging/scrolling no longer spikes preview-render work
- verify temporary preview scenes/models are disposed after generation

### Visual

- verify thumbnails are sharper and more readable than current icons
- verify consistent framing and lighting
- verify structural, logic, and volume sections all render correctly

## Rollout Order

1. Expand current thumbnail cache into unified persistent cache with metadata.
2. Add fingerprint/staleness checks.
3. Add boot precache pass.
4. Switch catalog draw path to image-only retrieval.
5. Add forced recache command and status reporting.
6. Tune quality settings once the performance path is stable.

## Recommendation

Implement the boot-time persistent precache system first, then tune thumbnail quality upward once the catalog is fully detached from live preview generation.

That sequence delivers the best outcome:

- immediate catalog responsiveness gains
- stable clickable icons at boot
- higher quality thumbnails without paying runtime interaction cost
- predictable refresh behavior when content changes
