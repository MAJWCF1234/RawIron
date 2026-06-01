# Releases

RawIron ships user-facing GitHub releases as a split full-workspace archive plus installer.

## Packaging rule

Package the full `D:\RawIron` workspace and exclude only `D:\RawIron\ReleaseArtifacts`.

## Release shape

A published release contains:

- split archive parts such as `RawIron_full_release_with_builds.zip.part01`, `part02`, `part03`, and additional parts when required by size
- `RawIron_Installer.zip`
- release notes with the SHA256 for the reassembled main archive

## Installer role

The installer downloads the split parts from the GitHub release, reassembles the archive, verifies the expected hash, and extracts the workspace to the user-chosen destination.

## Publishing pieces

- `Scripts/Publish-FullWorkspaceSplitZip.ps1`
- `Installer/RawIron.FullWorkspace.Installer.ps1`
- `Installer/RawIron.FullWorkspace.Installer.cmd`
- `Installer/README_REASSEMBLY.txt`

## Maintainer expectations

- release tag and installer defaults stay aligned
- release notes include the archive checksum
- attached assets include every split part required for the full workspace bundle
- the shipped bundle includes project assets, game folders, build outputs, scripts, and installer content as part of the workspace release model
