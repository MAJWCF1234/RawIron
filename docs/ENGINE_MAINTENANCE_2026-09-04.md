# Engine maintenance review — 2026-09-04

Scope: evaluate the configured Windows engine workspace, roadmap accuracy and
older implementation paths; fix reproduced defects through existing modules.
This is an incremental review of the current working tree, not a clean-checkout
release certification or a claim that every engine path has been audited.

## Findings and work

| Area | Finding | Implemented correction |
|---|---|---|
| Core / structural / software / XR | Normals transformed as vectors become incorrect under nonuniform scaling | Shared inverse-transpose normal math, used by existing consumers |
| Structural deferred operations | Spline copies and retained terrain triangles lose authored UVs | Preserve corresponding UV corners through each operation |
| Structural transforms | Already-local input is transformed again; reflected triangle winding is not corrected | Respect `compiledWorldSpace` and preserve outward winding on reflections |
| Structural mesh composition | `MergeCompiledMeshes` omits the UV stream | Preserve complete UV streams; use existing projection fallback for incomplete input |
| Dependency ownership | miniaudio and stb remain under engine source folders | Relocate unchanged vendor files to `ThirdParty`, update CMake and provenance |
| Roadmaps | Package manifest/mount work is described as unimplemented despite current APIs and tests | Reconcile completed foundations and keep unvalidated lifecycle features open |

## Regression evidence

Before the production fixes, `ConvexClipperSmoke` failed on spline UV retention,
merge UV retention, already-local spline input and retained terrain UVs. Both
`OpenXR.RuntimeSmoke` and `ScenePreviewRayTraceSafetySmoke` also failed with the
nonuniform-scale fixture. Logs are under `Saved/engine-review-20260904-` with
suffixes `structural-reproduction.log` and `normal-reproduction.log`.

After correction, all five focused tests passed: Core utilities, convex clipper,
structural brush metadata, software ray-trace safety and OpenXR runtime. Tests
include reflected winding/UV corner correspondence, inverse-transpose normals,
shear, extreme finite scales, singular/non-finite fallback and mixed-UV merging.
The common math stays in Core; structural transforms stay in Structural. The
experience has no additional primitive or normal-transform implementation.

The local-space bug was flag handling, not loss of negative scale in
`GetSafeScale`; the latter already preserved ordinary negative values and was
left unchanged. Terrain cutout still retains/discards whole triangles by its
existing rule; this change does not introduce geometric clipping.

## Dependency provenance

Library bytes were moved, not upgraded. Adapter implementation remains in Audio,
Render.Software and ri_tool; CMake now uses the single copies in `ThirdParty`.

| Header | SHA-256 before and after relocation |
|---|---|
| miniaudio/miniaudio.h | `C0DD363F340CC30444DDCE6E7514A1C24C456D137516CDBF49A528EDECD33840` |
| stb/stb_image.h | `1F8C1B6B408F26E3B20CBFBBD4758AFB3DC9B837FF1E17C258928F406148A87C` |

The miniaudio version is recorded as 0.11.21. stb_image identifies v2.30; its
original upstream commit was not recorded, so the relocation notes identify
exact bytes without inventing a commit. Old active build/include references were
searched; only historical provenance retains the former path.

## Roadmap reconciliation

The current Content implementation already provides manifest v2, dependency
resolution, mount transactions and compatibility fingerprints. Existing
`PackageResolverSmoke`, `PackageMountRegistrySmoke`, `GamePackageRequirementsSmoke`,
`SessionExtensionPackageSmoke` and `RuntimeNetcodeSmoke` cover those foundations.
The tracker now distinguishes them from uncompleted end-to-end native extension
lifecycle and showcase network adoption. Content fingerprints are not publisher
signatures. The existing 589-row r185 inventory is acknowledged, without claiming
this pass re-audited the upstream source or implemented all examples.

## Verification

The complete configured RelWithDebInfo workspace rebuild passed, including editor,
CLI tools, Cube Test and VR showcase. Log:
`Saved/engine-review-20260904-final-build.log`.

All **159/159 registered CTests passed**, serially, in 64.29 seconds:

```powershell
& 'C:/Program Files/CMake/bin/cmake.exe' --build --preset build-dev-msvc -j 8
& 'C:/Program Files/CMake/bin/ctest.exe' --test-dir build/dev-msvc -C RelWithDebInfo --output-on-failure --timeout 60
```

Full-suite log: `Saved/engine-review-20260904-all-tests.log`. This includes the
updated Core extreme-scale test, software/XR regressions, structural/preset
contracts, package security/resolution/mount tests, runtime netcode, architecture
policy and Cube Test content/world checks.

Desktop GPU calibration: **10/10 checks passed** with
`Scripts/Test-MaterialCalibration.ps1 -IncludeGallery`. Evidence:
[GPU report](../Saved/visual_checks/engine-review-20260904/20260904-215443-896/report.json).
The tested Cube Test executable SHA-256 is
`0A1030DD0495B44337F376D863CAAB0A6AAB5397C09838272F53248755EFE97B`.

Checks cover dimensions, flat receiver stability, finite rough-floor shading,
RGB ordering, retained shadows, repeatability, direct/hybrid black-level handling,
and normal-room sky/floor bounds. These fixed regions are regression probes, not
whole-image quality scores or direct coverage of the CPU normal fix.

The calibration and normal-room captures were also visually inspected:
[calibration](../Saved/visual_checks/engine-review-20260904/20260904-215443-896/frame-1.png),
[normal room](../Saved/visual_checks/engine-review-20260904/20260904-215443-896/normal-room.png).
Coarse shadow edges and very dark unlit sides remain clearly visible. Final
lighting/material parity is therefore still open despite the numerical passes.
`git diff --check` passed; existing CRLF normalization warnings are not test failures.

## Remaining gates

- This was the existing working tree and configured preset, not a clean checkout,
  alternate toolchain/platform matrix, sanitizer run or complete engine audit.
- Physical headset stereo, input, haptics and comfort require an actual HMD run.
  XR conversion unit tests cannot certify those behaviours.
- Coarse cast shadows, dark gallery sides, final normal-map convention review,
  KTX2/BasisU support and complete cooked asset membership remain open.
- Native extension migration/unloading, package acquisition and release-grade
  desktop/PCVR session compatibility still need end-to-end validation.
