# RawIron

Native **C++20** game engine and tooling: Vulkan runtime, scene graph, logic/events, and two in-repo games. **Windows-first**; Linux presets exist for library work.

**On GitHub:** this **README** on the default branch is the **source-of-truth** for how to build and run. The **Releases** tab may ship **optional** large binary bundles (split ZIPs + installer); there are **no** checked-in engine binaries on `main`. For **bugs / features**, use **[Issues](issues)** (templates under [`.github/ISSUE_TEMPLATE/`](.github/ISSUE_TEMPLATE/)). For **contribution workflow**, see [**CONTRIBUTING.md**](CONTRIBUTING.md). For **security**, see [**SECURITY.md**](SECURITY.md).

[![CI](https://github.com/MAJWCF1234/RawIron/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/MAJWCF1234/RawIron/actions/workflows/ci.yml) — **Windows MSVC** slim build + `RawIron.UiMenu --headless` on every push/PR to `main`. Maintainer guide: [**Documentation/04 Build/GitHub Push and Publish.md**](Documentation/04%20Build/GitHub%20Push%20and%20Publish.md).

---

## What you get on `main` (default CMake)

By default the root `CMakeLists.txt` builds a **slim** set of **runnable targets**:

| Output | Role |
|--------|------|
| **`RawIron.UiMenu`** | Windows JSON + Dear ImGui **UI / screen-flow** harness (`--demo-vn`, `--headless`). |
| **`RawIron.ParticleShowcase`** | CPU/GPU **particle** exercise host. |
| **`RawIron.LiminalGame`** | **Liminal Hall** game. |
| **`RawIron.ForestRuinsGame`** | **Wilderness Ruins** game. |

Everything else (generic **Player**, **Editor**, **Visual Shell**, **`ri_tool`**, **DevInspector**, large native **CTest** trees) is **off** unless you pass **`-D RAWIRON_BUILD_…=ON`** at configure time. See [Optional targets](#optional-targets).

---

## Quick start (clone → build → run)

**Prerequisites:** Visual Studio 2022 (C++ desktop), **CMake ≥ 3.24**, **Vulkan SDK** (for game/particle paths).

```powershell
git clone <your-fork-or-upstream-url> RawIron
cd RawIron
cmake --preset dev-msvc
cmake --build build/dev-msvc --config RelWithDebInfo --target RawIron.UiMenu RawIron.ParticleShowcase RawIron.LiminalGame RawIron.ForestRuinsGame
```

**Typical outputs** (paths use `RelWithDebInfo`; adjust if you use another VS configuration):

```text
build\dev-msvc\Apps\RawIron.UiMenu\RelWithDebInfo\RawIron.UiMenu.exe
build\dev-msvc\Apps\RawIron.ParticleShowcase\RelWithDebInfo\RawIron.ParticleShowcase.exe
build\dev-msvc\Games\LiminalHall\App\RelWithDebInfo\RawIron.LiminalGame.exe
build\dev-msvc\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe
```

**UI harness smoke (no window):**

```powershell
.\build\dev-msvc\Apps\RawIron.UiMenu\RelWithDebInfo\RawIron.UiMenu.exe --workspace=$PWD --headless
```

**VN demo (interactive, branching JSON UI):** double-click **`Launch UiMenu VN Demo.cmd`** in the repo root, or run the same `RawIron.UiMenu.exe` with **`--demo-vn`**. In-game copy may use **`${variableId}`** in `text` / `label` / `speaker` / choice labels / **`portrait`** / **`image`** / **`background.image`** paths. Press **`B`** for the **backlog** (opens scrolled to the end). **`H`** toggles the small music / missing-background dev strip. **`1`–`9`** activate visible choice buttons in screen order. Screen **`advance`** supports **`onSpace`**, **`onClick`**, **`onEnter`**, **`onMouseWheel`**, and **`delaySeconds`** (hold **Ctrl** to shorten the timer). **`say`** blocks may set **`voice`** (cue string; UI + backlog until playback is wired).

**Games** (from repo root, after build):

- `.\play-liminal.cmd`
- `.\play-forest-ruins.cmd`

**If MSVC fails on a removable / odd filesystem:** use `cmake --preset dev-msvc-localappdata` and `cmake --build --preset build-dev-msvc-localappdata`, then optionally `.\Scripts\Sync-ProfileBuildToRepo.ps1` to mirror binaries under `.\build\dev-msvc`.

---

## Releases (what users expect on GitHub)

Visitors typically look for **(1)** how to run something without compiling, **(2)** a clear **version / tag**, **(3)** **checksums** for large downloads, and **(4)** whether the default branch is **ahead** of the last release. This repo is optimized around **build-from-source** on `main`; releases are an **optional** acceleration path.

| Expectation | What we publish |
|-------------|-----------------|
| **Source always works** | Default branch **`main`** should match the **Quick start** in this README (slim CMake). CI / local builds validate that expectation. |
| **Releases ≠ nightly `main`** | A **GitHub Release** is a **snapshot**: tag + attached assets. New commits on `main` may land **after** the newest release; clone **`main`** for the latest sources. |
| **Prebuilt “full workspace”** | Maintainers may attach **split ZIP parts** (`RawIron_full_release_with_builds.zip.part01` … `.part03`) to a release, produced with **`Scripts/Publish-FullWorkspaceSplitZip.ps1`**. **SHA256** of the **reassembled** ZIP belongs in release notes and in **`Installer/RawIron.FullWorkspace.Installer.ps1`** (`ExpectedSha256`, `ReleaseTag`). |
| **Installer entry point** | **`Installer/RawIron.FullWorkspace.Installer.cmd`** (or **`.ps1 -NoGui`**) downloads those parts from **`/releases/download/<tag>/...`**, verifies the hash, and extracts. |
| **No release?** | Users should **build from source** per **Quick start**. Do not assume game or menu binaries exist in the git tree. |
| **Pre-release checkbox** | GitHub’s **“Set as a pre-release”** should be used when a bundle is experimental or not yet smoke-tested on a clean machine. |

**Maintainer checklist when publishing a new full-workspace release**

1. Create a **tag** (example pattern: `full-workspace-msvc-YYYY-MM-DD` — keep in sync with installer defaults).  
2. Attach **all** split parts; confirm **`part03`** padding rule in `Scripts/Publish-FullWorkspaceSplitZip.ps1` / `ReleaseArtifacts/release-notes.md` if used.  
3. Paste **SHA256** and short **“what’s inside”** into the release description (you can start from **`ReleaseArtifacts/release-notes.md`**).  
4. Update **`Installer/RawIron.FullWorkspace.Installer.ps1`** `ReleaseTag` and `ExpectedSha256` on `main` in the **same** PR or immediately after, so `main` always points at a real asset set.  
5. If you **move, delete, or retag** a release, expect broken installer downloads until URLs and hashes match again.

**License:** there is **no** single repository-wide `LICENSE` file yet; **`ThirdParty/`** contains per-library notices. See **CONTRIBUTING.md**.

---

## Optional targets

Reconfigure with any of:

- `-D RAWIRON_BUILD_PLAYER=ON`
- `-D RAWIRON_BUILD_EDITOR=ON`
- `-D RAWIRON_BUILD_TOOLS=ON` (builds `Tools/ri_tool`)
- `-D RAWIRON_BUILD_VISUAL_SHELL=ON`
- `-D RAWIRON_BUILD_TESTS=ON` (enables **CTest**; small app smokes such as `RawIron.UiMenu.HeadlessParse` when the UiMenu target is built)
- `-D RAWIRON_BUILD_DEV_INSPECTOR=ON`

`CMakeLists.slim.txt` in the repo root is a **full drop-in copy** of the slim root `CMakeLists.txt` (useful if your IDE locks `CMakeLists.txt` on Windows).

---

## Repository layout (short)

- **`Source/`** — engine libraries (`RawIron.Core`, `RawIron.Runtime`, `RawIron.Render.Vulkan`, `RawIron.SceneUtilities`, …).
- **`Games/`** — **LiminalHall** and **WildernessRuins** runtimes + game apps.
- **`Apps/`** — **`RawIron.UiMenu`**, **`RawIron.ParticleShowcase`** (other apps optional via CMake).
- **`Assets/`** — cooked/source content; **`Assets/UI/`** — JSON UI manifests + schema.
- **`Documentation/`** — Obsidian-style engine docs (`Documentation/00 Home.md`).
- **`Scripts/`** — build hygiene, publish, sync profile builds.
- **`Installer/`** — full-workspace release installer.
- **`Tests/`** — CMake stub; heavy historical native suites were removed in favor of **game `*.riscript`** flows and optional **CTest** from apps.

---

## Testing

- Default **`RAWIRON_BUILD_TESTS=OFF`**: no expectation of a monolithic `RawIron.Core.Tests` binary on `main`.
- Set **`RAWIRON_BUILD_TESTS=ON`** and configure/build **RawIron.UiMenu** to register lightweight **CTest** entries where defined in app `CMakeLists.txt`.
- Engine and gameplay validation increasingly live in **per-game scripts** under `Games/*/scripts/`.

---

## Documentation

Start here:

- `Documentation/00 Home.md`
- `Documentation/02 Engine/Current Engine Review.md`
- `Documentation/02 Engine/Repository Layout.md`
- `Documentation/04 Build/Testing.md` (may still mention removed native suites — treat as historical where it conflicts with this README)
- `Documentation/04 Build/GitHub Push and Publish.md` — **CI**, bundle push, and **GitHub Releases** workflow for maintainers

---

## Contributing & issues

- **Contributing guide:** [**CONTRIBUTING.md**](CONTRIBUTING.md) (build matrix, UI smoke, release notes for maintainers).
- **GitHub Issues** (templates under **`.github/ISSUE_TEMPLATE/`**) for bugs and feature requests — the issue chooser also links back to this README and **Documentation/**.
- **Pull requests:** fill out **`.github/pull_request_template.md`**. User-facing behavior (README, manifests, installer defaults) should be updated in the same PR when it changes.
- Large refactors: open an issue first so `main` stays buildable for the **slim default** above.

Other scripts (push bundles, clean build trees): see **`Scripts/`** and comments in **`Installer/`**.
