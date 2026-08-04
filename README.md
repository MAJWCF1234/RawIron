# RawIron

Native **C++20** game engine and full-workspace game stack: shared runtime core, Vulkan rendering, editor tooling, multiplayer support, plugin/mod policy surfaces, and three in-repo games. **Windows-first**; Linux presets exist for library work.

**On GitHub:** this **README** on the default branch is the **source-of-truth** for how to build and run. The **Releases** tab may ship **optional** large binary bundles (split ZIPs + installer); there are **no** checked-in engine binaries on `main`. For **bugs / features**, use **[Issues](issues)** (templates under [`.github/ISSUE_TEMPLATE/`](.github/ISSUE_TEMPLATE/)). For **contribution workflow**, see [**CONTRIBUTING.md**](CONTRIBUTING.md). For **security**, see [**SECURITY.md**](SECURITY.md).

[![CI](https://github.com/MAJWCF1234/RawIron/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/MAJWCF1234/RawIron/actions/workflows/ci.yml) — **Windows MSVC** CI builds the complete configured workspace, runs **`RawIron.UiMenu --headless`** and every registered CTest, verifies the real ENet lane, and runs a focused AddressSanitizer lane. Native GPU presentation remains a separate hardware-lane concern. Maintainer guide: [**Documentation/04 Pipeline/Releases.md**](Documentation/04%20Pipeline/Releases.md).

---

## What you get on `main` (default CMake)

At a glance:

- shared engine libraries in `Source/`
- native apps and tools in `Apps/` and optional `Tools/`
- game projects in `Games/`
- full workspace shipping through split release parts plus installer
- project-owned config, plugin, AI, UI, and test surfaces that ship with the workspace

By default the root `CMakeLists.txt` builds these **runnable targets**:

| Output | Role |
|--------|------|
| **`RawIron.Player`** | Generic **runtime host** + Vulkan bootstrap (`Apps/RawIron.Player`). **`Launch RawIron Player.cmd`**. |
| **`RawIron.Preview`** | Scene Kit **snapshot / preview** host, software + optional Vulkan (`Apps/RawIron.Preview`). **`Launch RawIron Preview.cmd`**. |
| **`RawIron.Editor`** | Native **editor** host (`Apps/RawIron.Editor`). **`Launch RawIron Editor.cmd`**. Game folders pass **`--game=…`**. |
| **`RawIron.Forge`** | Native **model-source and rigging workbench** (`Apps/RawIron.Forge`), launched from the existing **Forge** icon in Visual Shell. |
| **`RawIron.VisualShell`** | Keyboard-first **visual shell** (`Apps/RawIron.VisualShell`). **`Launch RawIron Visual Shell.cmd`**. |
| **`RawIron.UiMenu`** | JSON + Dear ImGui **UI / screen-flow** harness (`--demo-vn`, `--headless`). |
| **`RawIron.ParticleShowcase`** | CPU/GPU **particle** exercise host. |
| **`RawIron.LiminalGame`** | **Liminal Hall** game. |
| **`RawIron.ForestRuinsGame`** | **Wilderness Ruins** game. |
| **`RawIron.MultiplayerSandboxGame`** | **RawIron Multiplayer Sandbox** game. |
| **`RawIron.BotClient`** | Headless multiplayer bot client for session/load testing. |
| **`RawIron.DedicatedServer`** | Dedicated multiplayer server host. |

Optional **`RAWIRON_BUILD_*`** switches turn off **`RawIron.Player`** / **`RawIron.Preview`**, enable **`ri_tool`**, **CTest** smokes, **DevInspector**, etc. See [Optional targets](#optional-targets). **`CMakeLists.ide-mirror.txt`** is a copy of the same defaults for workflows where an IDE cannot edit **`CMakeLists.txt`** in place.

---

## Quick start (clone → build → run)

**Prerequisites:** Visual Studio 2022 (C++ desktop), **CMake ≥ 3.24**, **Vulkan SDK** (for game/particle paths).

```powershell
git clone <your-fork-or-upstream-url> RawIron
cd RawIron
cmake --preset dev-msvc
cmake --build build/dev-msvc --config RelWithDebInfo --target RawIron.Player RawIron.Preview RawIron.Editor RawIron.Forge RawIron.VisualShell RawIron.UiMenu RawIron.ParticleShowcase RawIron.LiminalGame RawIron.ForestRuinsGame RawIron.MultiplayerSandboxGame RawIron.BotClient RawIron.DedicatedServer
```

**Typical outputs** (paths use `RelWithDebInfo`; adjust if you use another VS configuration):

```text
build\dev-msvc\Apps\RawIron.Player\RelWithDebInfo\RawIron.Player.exe
build\dev-msvc\Apps\RawIron.Preview\RelWithDebInfo\RawIron.Preview.exe
build\dev-msvc\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe
build\dev-msvc\Apps\RawIron.Forge\RelWithDebInfo\RawIron.Forge.exe
build\dev-msvc\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe
build\dev-msvc\Apps\RawIron.UiMenu\RelWithDebInfo\RawIron.UiMenu.exe
build\dev-msvc\Apps\RawIron.ParticleShowcase\RelWithDebInfo\RawIron.ParticleShowcase.exe
build\dev-msvc\Games\LiminalHall\App\RelWithDebInfo\RawIron.LiminalGame.exe
build\dev-msvc\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe
build\dev-msvc\Games\RawIronMultiplayerSandbox\App\RelWithDebInfo\RawIron.MultiplayerSandboxGame.exe
build\dev-msvc\Apps\RawIron.BotClient\RelWithDebInfo\RawIron.BotClient.exe
build\dev-msvc\Apps\RawIron.DedicatedServer\RelWithDebInfo\RawIron.DedicatedServer.exe
```

**UI harness smoke (no window):**

```powershell
.\build\dev-msvc\Apps\RawIron.UiMenu\RelWithDebInfo\RawIron.UiMenu.exe --workspace=$PWD --headless
```

**Editor, Forge & visual shell (from repo root, after build):** double-click **`Launch RawIron Editor.cmd`** or **`Launch RawIron Visual Shell.cmd`**. Visual Shell is the Raw Iron workdesk; its existing **Forge** icon opens the model-source and rigging workbench. The editor is invoked with **`--workspace=<repo root>`**; add **`--game=liminal-hall`** or **`--game=wilderness-ruins`** to open a registered project (same as **`Games\LiminalHall\Open Liminal Hall In Editor.cmd`**).

**VN demo (interactive, branching JSON UI):** double-click **`Launch UiMenu VN Demo.cmd`** in the repo root, or run the same `RawIron.UiMenu.exe` with **`--demo-vn`**. In-game copy may use **`${variableId}`** in `text` / `label` / `speaker` / choice labels / **`portrait`** / **`image`** / **`background.image`** paths. Press **`B`** for the **backlog** (opens scrolled to the end). **`H`** toggles the small music / missing-background dev strip. **`1`–`9`** activate visible choice buttons in screen order. Screen **`advance`** supports **`onSpace`**, **`onClick`**, **`onEnter`**, **`onMouseWheel`**, and **`delaySeconds`** (hold **Ctrl** to shorten the timer). **`say`** blocks may set **`voice`** (cue string; UI + backlog until playback is wired).

**Games** (from each game folder under `Games\`, after build — see **`Play Liminal Hall.cmd`**, **`Play Wilderness Ruins.cmd`**, and **`Play RawIron Multiplayer Sandbox.cmd`**).

**One build tree on Windows:** use `build\dev-msvc` only. Launchers resolve binaries there via `Scripts\Resolve-RawIronBinary.cmd`. To drop legacy flat `build\` ninja output or old `build\dev-clang` / `build\dev-mingw` trees while keeping MSVC output: `.\Scripts\Clean-StaleBuildRoots.ps1`.

**If MSVC fails on a removable / odd filesystem:** use `cmake --preset dev-msvc-localappdata` and `cmake --build --preset build-dev-msvc-localappdata`, then `.\Scripts\Sync-ProfileBuildToRepo.ps1` to mirror binaries under `.\build\dev-msvc`.

## Engine shape

- `RawIron.Runtime` owns shared lifecycle, services, events, and module mounting
- `RawIron.Content` owns game manifest validation and project format enforcement
- `RawIron.Render.Vulkan` and `RawIron.Render.Software` provide the main presentation paths
- `RawIron.World`, `RawIron.Logic`, `RawIron.Events`, `RawIron.Trace`, and `RawIron.Spatial` support world simulation and authored interactions
- `Games/Common` enforces shared config contract behavior across projects

## Multiplayer and mods

- multiplayer is a core engine surface with dedicated, listen, hybrid, and client flows
- `RawIron.DedicatedServer`, `RawIron.BotClient`, and `RawIron.MultiplayerSandboxGame` are the main multiplayer integration targets
- every game project carries plugin and policy surfaces such as `scripts/plugins.riscript`, `config/plugins.policy`, `plugins/manifest.plugins`, and `plugins/hooks.riplugin`
- game tuning belongs in project cfg and riscript files under engine-owned validation

---

## Releases (what users expect on GitHub)

Visitors typically look for **(1)** how to run something without compiling, **(2)** a clear **version / tag**, **(3)** **checksums** for large downloads, and **(4)** whether the default branch is **ahead** of the last release. This repo is optimized around **build-from-source** on `main`; releases are an **optional** acceleration path.

| Expectation | What we publish |
|-------------|-----------------|
| **Source always works** | Default branch **`main`** should match the **Quick start**. CI configures the development workspace, builds all configured targets, and runs the registered headless suite; native GPU and interactive UX validation still require appropriate hardware lanes and local review. |
| **Releases ≠ nightly `main`** | A **GitHub Release** is a **snapshot**: tag + attached assets. New commits on `main` may land **after** the newest release; clone **`main`** for the latest sources. |
| **Prebuilt “full workspace”** | Maintainers publish a split full-workspace archive generated by **`Scripts/Publish-FullWorkspaceSplitZip.ps1`**. The package includes the full `D:\RawIron` workspace content except `ReleaseArtifacts`, split as `RawIron_full_release_with_builds.zip.part01` … `.part04` when required by asset size. **SHA256** of the reassembled ZIP belongs in release notes and in **`Installer/RawIron.FullWorkspace.Installer.ps1`** (`ExpectedSha256`, `ReleaseTag`). |
| **Installer entry point** | **`Installer/RawIron.FullWorkspace.Installer.cmd`** (or **`.ps1 -NoGui`**) downloads those parts from **`/releases/download/<tag>/...`**, verifies the hash, and extracts. |
| **No release?** | Users should **build from source** per **Quick start**. Do not assume game or menu binaries exist in the git tree. |
| **Pre-release checkbox** | GitHub’s **“Set as a pre-release”** should be used when a bundle is experimental or not yet smoke-tested on a clean machine. |

**Maintainer checklist when publishing a new full-workspace release**

1. Create a **tag** (example pattern: `full-workspace-msvc-YYYY-MM-DD` — keep in sync with installer defaults).  
2. Attach **all** split parts produced by `Scripts/Publish-FullWorkspaceSplitZip.ps1` (commonly `part01` … `part04` for full-workspace drops).  
3. Paste **SHA256** and short **“what’s inside”** into the release description (you can start from **`ReleaseArtifacts/release-notes.md`**).  
4. Update **`Installer/RawIron.FullWorkspace.Installer.ps1`** `ReleaseTag` and `ExpectedSha256` on `main` in the **same** PR or immediately after, so `main` always points at a real asset set.  
5. If you **move, delete, or retag** a release, expect broken installer downloads until URLs and hashes match again.

**License:** there is **no** single repository-wide `LICENSE` file yet; **`ThirdParty/`** contains per-library notices. See **CONTRIBUTING.md**.

---

## Optional targets

**Player**, **Preview**, **Editor**, **Forge**, and **Visual Shell** are **ON** by default. Turn any **OFF** with `-D RAWIRON_BUILD_PLAYER=OFF`, `-D RAWIRON_BUILD_PREVIEW=OFF`, `-D RAWIRON_BUILD_EDITOR=OFF`, `-D RAWIRON_BUILD_FORGE=OFF`, `-D RAWIRON_BUILD_VISUAL_SHELL=OFF` for a faster configure.

Other switches:

- `-D RAWIRON_BUILD_TOOLS=ON` (builds `Tools/ri_tool`)
- `-D RAWIRON_BUILD_TESTS=ON` (enables the engine, content, runtime, editor-support, game, CLI, and app **CTest** targets; the development presets already set this to `ON`)
- `-D RAWIRON_BUILD_DEV_INSPECTOR=ON`

`CMakeLists.ide-mirror.txt` tracks the same **`RAWIRON_BUILD_*`** defaults as **`CMakeLists.txt`** for IDE workflows that cannot edit the primary file.

---

## Repository layout (short)

- **`Source/`** — engine libraries (`RawIron.Core`, `RawIron.Runtime`, `RawIron.Render.Vulkan`, `RawIron.SceneUtilities`, …).
- **`Games/`** — **LiminalHall**, **WildernessRuins**, and **RawIronMultiplayerSandbox** runtimes + game apps.
- **`Apps/`** — **`RawIron.Player`**, **`RawIron.Preview`**, **`RawIron.Editor`**, **`RawIron.Forge`**, **`RawIron.VisualShell`**, **`RawIron.UiMenu`**, **`RawIron.ParticleShowcase`**, **`RawIron.BotClient`**, **`RawIron.DedicatedServer`**.
- **`Assets/`** — cooked/source content; **`Assets/UI/`** — JSON UI manifests + schema.
- **`Documentation/`** — Obsidian-style engine docs (`Documentation/00 Home.md`).
- **`Scripts/`** — build hygiene, publish, sync profile builds.
- **`Installer/`** — full-workspace release installer.
- **`Tests/`** — native unit, contract, regression, integration, CLI, and smoke executables registered by subsystem CMake files.

---

## Testing

- The raw CMake option defaults to **`RAWIRON_BUILD_TESTS=OFF`**, while supported development presets such as `dev-msvc` enable it.
- Build the configured workspace, then run `ctest --test-dir build/dev-msvc -C RelWithDebInfo --output-on-failure` from the repository root.
- Tests are registered close to their owning engine/app/game targets; game-authored validation also lives under `Games/*/tests/` and `Games/*/scripts/`.
- CI runs the complete registered Windows suite plus focused ENet and AddressSanitizer lanes. Vulkan presentation and visual correctness are not implied by headless CTest success.

---

## Documentation

Start here:

- `Documentation/00 Home.md`
- `Documentation/01 Product/RawIron.md`
- `Documentation/02 Engine/00 Engine Home.md`
- `Documentation/02 Engine/01 Runtime Core.md`
- `Documentation/02 Engine/04 Multiplayer.md`
- `Documentation/02 Engine/06 Plugins and Mods.md`
- `Documentation/02 Engine/10 Mod and Plugin Authoring.md`
- `Documentation/03 Projects/00 Projects Home.md`
- `Documentation/04 Pipeline/Build, Test, and Run.md`
- `Documentation/04 Pipeline/Releases.md`

---

## Contributing & issues

- **Contributing guide:** [**CONTRIBUTING.md**](CONTRIBUTING.md) (build matrix, UI smoke, release notes for maintainers).
- **GitHub Issues** (templates under **`.github/ISSUE_TEMPLATE/`**) for bugs and feature requests — the issue chooser also links back to this README and **Documentation/**.
- **Pull requests:** fill out **`.github/pull_request_template.md`**. User-facing behavior (README, manifests, installer defaults) should be updated in the same PR when it changes.
- Large refactors: open an issue first so `main` stays buildable with the **default CMake** and **CI smoke** expectations above.

Other scripts (push bundles, clean build trees): see **`Scripts/`** and comments in **`Installer/`**.
