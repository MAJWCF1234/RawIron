---
tags:
  - rawiron
  - engine
  - apps
  - layout
---

# Application Package Format

RawIron applications should follow the same ownership model as RawIron games:

- the engine lives in `Source/`
- the application lives in `Apps/<AppName>/`
- the application mounts engine services instead of scattering its code across the workspace

This keeps editor shells, preview tools, players, and utility programs self-contained and easy to delete, rename, package, or test.

## Rule

An application package owns everything specific to that application except:

- shared engine code in `Source/`
- shared workspace/build glue in top-level `cmake/`, `Scripts/`, and root launch shortcuts
- truly shared assets/config that are intentionally workspace-wide

If code or assets are used by only one app, they should stay inside that app package. Promote them into shared engine modules only after they are proven reusable.

## Standard Shape

Minimum package:

- `Apps/<AppName>/CMakeLists.txt`
- `Apps/<AppName>/src/`

Recommended shape:

- `Apps/<AppName>/include/`
- `Apps/<AppName>/Assets/`
- `Apps/<AppName>/Config/`
- `Apps/<AppName>/Tests/`
- `Apps/<AppName>/README.md`

All of these subfolders are optional, but they should be preferred over placing app-specific material elsewhere in the repository.

## Build Contract

RawIron apps should register through `cmake/RawIronApp.cmake` using `rawiron_add_app(...)`.

That helper standardizes:

- executable creation
- engine/library linkage
- Windows platform libraries
- runtime DLL staging
- optional engine texture bundling
- app smoke-test registration

Each app `CMakeLists.txt` should describe only what the app owns:

- sources
- linked engine modules
- app-specific compile definitions
- optional smoke-test expectations

## Ownership Boundaries

Good app-local examples:

- launcher UI
- preview-shell orchestration
- app-specific startup wiring
- app-specific assets and config defaults
- package-local helper classes used by one app

Good shared-engine examples:

- scene graph/runtime services
- generalized rendering backends
- reusable runtime tuning or event infrastructure
- common preview/math/trace/world helpers

## Goal

Games and apps should be parallel concepts in RawIron:

- `Games/<GameName>/` owns game-specific runtime/content/application code
- `Apps/<AppName>/` owns app-specific host/composition/UI/tooling code

Both mount the engine. Neither should splatter their identity across `Source/`.
