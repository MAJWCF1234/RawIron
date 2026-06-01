---
tags:
  - rawiron
  - testing
  - build
---

# Testing

## Current Default On `main`

RawIron currently defaults to:

- `RAWIRON_BUILD_TESTS=OFF`

That means a default local configure/build does **not** guarantee registered CTest suites or monolithic native test binaries.

## What CI Validates

The GitHub Actions Windows pipeline validates a fast smoke subset to keep turnaround time practical:

- selected runtime/app build targets
- `RawIron.UiMenu --headless` execution smoke

Treat CI as a fast gate, not full subsystem exhaustiveness.

## Local Validation Flow

For local verification, use layered checks:

1. Full target builds for apps and games from root `CMakeLists.txt`.
2. Runtime smoke runs (UiMenu headless, editor/player/preview/game launches as needed).
3. Per-game scripted validation in `Games/*/scripts` and `Games/*/tests`.
4. Optional explicit test registration by enabling test flags and building targets that define CTest entries.

## Commands

Build default workspace:

```powershell
cmake --preset dev-msvc
cmake --build build/dev-msvc --config RelWithDebInfo
```

UI smoke (headless):

```powershell
.\build\dev-msvc\Apps\RawIron.UiMenu\RelWithDebInfo\RawIron.UiMenu.exe --workspace=$PWD --headless
```

List registered CTests (if any are configured):

```powershell
ctest --test-dir .\build\dev-msvc -N
```

Run registered CTests:

```powershell
ctest --test-dir .\build\dev-msvc --output-on-failure -V
```

## Historical Note

Older documentation referenced a fixed 17-entry generated CTest inventory and larger always-present native suites. Keep that as historical context only; current behavior is opt-in testing plus runtime/scripted validation.

## Related Notes

- [[Current Engine Review]]
- [[Repository Layout]]
- [[GitHub Push and Publish]]
