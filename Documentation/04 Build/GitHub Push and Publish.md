---
tags:
  - rawiron
  - build
  - github
---

# GitHub — push and publish

How maintainers get changes onto **GitHub**, how **CI** validates **`main`**, and how **Releases** (full-workspace ZIPs) are produced.

## Day-to-day push

1. **Branch** off `main`, commit, open a **pull request** (fills [`.github/pull_request_template.md`](../../.github/pull_request_template.md)).
2. Wait for **CI** (`.github/workflows/ci.yml`) to go green on the PR.
3. **Merge** to `main`. Default branch stays the source of truth for clone + build ([README](../../README.md) Quick start).

### When `git push` fails (locked `.git` / removable drive)

From repo root:

```powershell
.\Scripts\Git-PushViaBundle.ps1 -Confirm
```

Creates a **bundle** under `%TEMP%`, clones it, pushes to `origin`, then deletes temp files. See script header for safety notes.

## Continuous integration

- Workflow: **[`.github/workflows/ci.yml`](../../.github/workflows/ci.yml)**  
- **Triggers:** `push` / `pull_request` to `main` or `master`, plus **Run workflow** (manual).  
- **Does:** Vulkan SDK → MSVC → `cmake --preset dev-msvc` → build **UiMenu**, **ParticleShowcase**, **LiminalGame**, **ForestRuinsGame** → **`RawIron.UiMenu --headless`**.

If CI fails on Vulkan or shader tools, see [[04 Build/Vulkan Environment|Vulkan Environment]] and confirm `glslangValidator` is on `PATH` after the install action (the workflow uses the same CMake rules as a local machine).

## Full-workspace release (split ZIP + installer)

High level (details also in root **README** → *Releases*):

1. On a machine with a **full** configured build tree if you include `build\` in the drop, run:

   ```powershell
   .\Scripts\Publish-FullWorkspaceSplitZip.ps1 -OutputDir D:\YourDropFolder
   ```

2. Compute **SHA256** of the **reassembled** joined ZIP (before splitting is one file; after join, hash that file).
3. On GitHub: **Releases** → **Draft a new release** → choose **tag** (e.g. `full-workspace-msvc-YYYY-MM-DD`) → attach **`part01` … `part03`** (+ optional `RawIron_Installer.zip`).
4. Paste release body starting from **`ReleaseArtifacts/release-notes.md`** (placeholders + checksum).
5. On **`main`**, update **`Installer/RawIron.FullWorkspace.Installer.ps1`** `ReleaseTag` and `ExpectedSha256` to match (same PR as release notes tweak, or immediate follow-up).

### GitHub CLI (optional)

If you use `gh` and already have assets on disk:

```powershell
gh release create <tag> .\path\part01 .\path\part02 .\path\part03 --title "..." --notes-file .\ReleaseArtifacts\release-notes.md
```

Adjust flags for **pre-release** (`--prerelease`) as needed.

## Permissions and security

- **Workflow token:** CI uses `permissions: contents: read` only.  
- **Vulnerability reports:** [**SECURITY.md**](../../SECURITY.md) (private reporting; do not use public issues for undisclosed security bugs).

## Forks

Replace hard-coded `MAJWCF1234/RawIron` URLs in **`.github/ISSUE_TEMPLATE/config.yml`** (and README badges, if you add them) with your fork or org so **Issues → contact links** and badges resolve correctly.
