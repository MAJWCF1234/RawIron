# Contributing to RawIron

Thanks for helping improve the engine, games, or tooling.

## Continuous integration

Pull requests and pushes to **`main`** / **`master`** run **[`.github/workflows/ci.yml`](.github/workflows/ci.yml)** (Windows, Vulkan SDK, slim targets, **`RawIron.UiMenu --headless`**). Fix CI before merge when possible.

Maintainer playbook (push when push fails, full-workspace releases, `gh` tips): **[Documentation/04 Build/GitHub Push and Publish.md](Documentation/04%20Build/GitHub%20Push%20and%20Publish.md)**.

## Before you open a PR

1. **Build the default (slim) matrix** from the root [README.md](README.md) — `cmake --preset dev-msvc` then build **`RawIron.UiMenu`**, **`RawIron.ParticleShowcase`**, **`RawIron.LiminalGame`**, and **`RawIron.ForestRuinsGame`**. If your change needs optional CMake flags, say so in the PR. (Same matrix as CI.)
2. **UI / JSON flows:** if you touch manifests or `RawIron.UiMenu`, run at least:
   - `RawIron.UiMenu.exe --workspace=<repo> --headless`
   - and, when relevant, `--demo-vn` for a quick interactive pass.
3. Use the [pull request template](.github/pull_request_template.md) (checklist + configure line).

## Issues

- **Bugs:** [Bug report](.github/ISSUE_TEMPLATE/bug_report.yml) — include commit hash, configure command, and the failing build or runtime command.
- **Features:** [Feature request](.github/ISSUE_TEMPLATE/feature_request.yml) — pick an **Area** so maintainers can route it.

## Releases (maintainers)

Publishing split full-workspace ZIPs, checksums, and installer defaults is documented in the README **Releases** section and in `Scripts/Publish-FullWorkspaceSplitZip.ps1`. Update `Installer/RawIron.FullWorkspace.Installer.ps1` **`ReleaseTag`** / **`ExpectedSha256`** when you attach new assets to a GitHub Release.

## License

There is **no single root `LICENSE` file** yet; third-party trees under `ThirdParty/` include their own license files. Do not remove or obscure those notices when vendoring or upgrading dependencies.

## Forks

If you use this tree as a long-lived fork, consider updating **`.github/ISSUE_TEMPLATE/config.yml`** `contact_links` URLs so “README” / “Documentation” point at your GitHub namespace instead of the upstream slug baked in for convenience.
