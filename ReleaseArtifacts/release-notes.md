# Release `full-workspace-msvc-2026-05-05`

Full workspace drop (sources + `build\dev-msvc` + assets) split for GitHub’s asset size limits.

## Download

Concatenate in order:

- `RawIron_full_release_with_builds.zip.part01`
- `RawIron_full_release_with_builds.zip.part02`
- `RawIron_full_release_with_builds.zip.part03`

→ single file `RawIron_full_release_with_builds.zip`

## SHA256 (joined file, before extract)

```
b309613536dbdc0706ea4d67e271a6c3039dd0823838fb4d3ed0c6d29cdebe8e
```

## Installer

Use **`RawIron_Installer.zip`** from this release, or run **`Installer/RawIron.FullWorkspace.Installer.cmd`** from a **git clone** at commit **`main`** after pull (defaults match this tag + SHA in `Installer/RawIron.FullWorkspace.Installer.ps1`).

## Notes

- **`Installer/`** is excluded from the big ZIP; ship the small installer bundle separately.
- If **`part03`** is 1 byte, see `Scripts/Publish-FullWorkspaceSplitZip.ps1` (GitHub rejects empty assets; joined hash unchanged).

## Build from source

Prefer cloning **`main`** and the root **README** Quick start unless you need this offline bundle.
