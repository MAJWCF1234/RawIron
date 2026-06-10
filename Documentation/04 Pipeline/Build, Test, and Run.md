# Build, Test, and Run

## Configure and build

Typical local Windows flow:

```powershell
cmake --preset dev-msvc
cmake --build build/dev-msvc --config RelWithDebInfo --target RawIron.Player RawIron.Preview RawIron.Editor RawIron.VisualShell RawIron.UiMenu RawIron.ParticleShowcase RawIron.LiminalGame RawIron.ForestRuinsGame RawIron.MultiplayerSandboxGame RawIron.BotClient RawIron.DedicatedServer
```

A LocalAppData-based preset (`dev-msvc-localappdata`) exists for environments where the repo path is not a good build location. After building there, run `Scripts/Sync-ProfileBuildToRepo.ps1` to mirror binaries into `build/dev-msvc`.

**Canonical layout:** all Windows launchers resolve binaries under `build/dev-msvc/<Apps|Games|Tools>/.../RelWithDebInfo/`. Legacy flat `build/` ninja trees and extra preset subdirs (`build/dev-clang`, `build/dev-mingw`) are stale — remove them with `Scripts/Clean-StaleBuildRoots.ps1` (keeps `build/dev-msvc`).

## Launch surfaces

- root launchers such as `Launch RawIron Editor.cmd`, `Launch RawIron Visual Shell.cmd`, and project play scripts (they call `Scripts/Resolve-RawIronBinary.cmd`)
- game-local launchers in each `Games/<Project>` folder
- direct executable invocation from `build/dev-msvc/...`

## Tests

- `RAWIRON_BUILD_TESTS=ON` enables optional lightweight CTest registration where defined
- `RawIron.UiMenu --headless` is the baseline no-window smoke path
- game projects also carry `tests/*.riscript` surfaces for gameplay, rendering, network, and UI validation

## Multiplayer runs

Typical native multiplayer test pieces are:

- `RawIron.DedicatedServer`
- `RawIron.BotClient`
- `RawIron.MultiplayerSandboxGame`

## Tools

- `ri_tool` is available when `RAWIRON_BUILD_TOOLS=ON`
- `Scripts/` contains build helpers, release helpers, and sync helpers
