# Authoring Catalog Thumbnail Cache Plan

## Current State

The editor already has a unified persistent thumbnail cache for structural, volume, and logic catalog presets.

- Cache artifacts are stored under `Saved/Editor/CatalogThumbnailCache` as BMP images plus `.meta` sidecars.
- Entries are invalidated by a cache version, preset fingerprint, texture fingerprint, missing files, and a seven-day age limit.
- Structural presets render with catalog-specific software-preview settings; volume and logic presets use their dedicated wireframe render path.
- Catalog drawing blits cached images, and cache invalidation is available through `StructuralThumbnailCache::ClearPersistent()`.
- `PrecacheAll(..., force)` exists, but normal editor startup currently uses `PrewarmVisible(...)` during catalog paint instead of a full readiness pass.

The original persistence, fingerprinting, staleness, and higher-quality rendering work is complete. This document now tracks only the remaining behavior gap: icons can still be generated while a user is browsing the catalog.

## Active Backlog

1. Invoke `PrecacheAll` during editor startup after the texture root is known, and show concise progress/failure status without blocking the window indefinitely.
2. Add a user-facing forced-recache command that calls `PrecacheAll(..., true)` and refreshes the catalog once it completes.
3. Make catalog paint image-only: use a stable fallback for entries not ready yet rather than calling `PrewarmVisible` from the paint path.
4. Extend sidecar metadata only where it is useful for diagnosis (generator version, source identity, and generation timestamp); current fingerprint and file-age checks remain the validity authority.
5. Add automated cache tests for empty-cache generation, valid-cache reuse, one-entry invalidation, stale-entry refresh, and forced recache.

## Acceptance Criteria

- After startup readiness, paging and scrolling never construct preview scenes or render thumbnails.
- Deleting or changing one cached entry regenerates only that entry on the next precache pass.
- A forced recache rebuilds all entries while preserving an operable editor.
- Cache failures use a visible fallback and an actionable status message instead of blocking catalog interaction.

## Source Map

- `Apps/RawIron.Editor/src/EditorStructuralPicker.h`: cache API.
- `Apps/RawIron.Editor/src/EditorStructuralPicker.cpp`: persistent cache, fingerprints, rendering, and invalidation.
- `Apps/RawIron.Editor/src/main.cpp`: editor startup and catalog paint integration.
