# Build, Test, and Run

## Configure and build

Typical local Windows flow:

```powershell
cmake --preset dev-msvc
cmake --build build/dev-msvc --config RelWithDebInfo --target RawIron.Player RawIron.Preview RawIron.Editor RawIron.VisualShell RawIron.UiMenu RawIron.ParticleShowcase RawIron.LiminalGame RawIron.ForestRuinsGame RawIron.MultiplayerSandboxGame RawIron.BotClient RawIron.DedicatedServer
```

A LocalAppData-based preset also exists for environments where the repo path is not a good build location.

## Launch surfaces

- root launchers such as `Launch RawIron Editor.cmd`, `Launch RawIron Visual Shell.cmd`, and project play scripts
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
