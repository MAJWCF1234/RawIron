# Cube Test gallery validation — 2026-08-27

Five tracker items are complete: texture diagnostics, material/default failure tests, authored portal
routes, independent glTF file checks, and gallery help. This is not a declaration of showcase visual
quality, cooked-package completeness, or headset validation.

## Reproduce

From the workspace root, using the CMake installation that configured this build:

```powershell
& 'C:/Program Files/CMake/bin/cmake.exe' --build --preset build-dev-msvc --target RawIron.CubeTestGame RawIron.CubeTest.WorldSmoke GltfExporterSmoke AuthoringHelpersSafetySmoke PortalTravelSmoke --parallel 6
& 'C:/Program Files/CMake/bin/ctest.exe' --test-dir build/dev-msvc -C RelWithDebInfo -R 'AuthoringHelpersSafetySmoke|RawIron.CubeTest.|GltfExporterSmoke|NativeScenePreviewDetailNormalSmoke|ExtendedCompositeWiringSmoke|ReferenceShaderMigrationSmoke|FrameTuningSmoke|HybridPostProcessSafetySmoke|PortalTravelSmoke' --output-on-failure
$exe = 'build/dev-msvc/Games/CubeTest/App/RelWithDebInfo/RawIron.CubeTestGame.exe'
& $exe --gallery-help
& $exe --workspace-root=. --start-room=normals --background --capture-native=Saved/visual_checks/normals.bmp
& ./Scripts/Test-MaterialCalibration.ps1
```

The glTF file check is a Windows PowerShell CTest and launches the batch exporter without a window.
It retains artifacts under `build/dev-msvc/Testing/CubeTestGltf/<timestamp>`. Other tests in the
command must have been built locally; this was a targeted run, not a clean full-workspace build.

## Texture and material checks

`RawIron.Render.Software::AuditSceneTextures` is a reusable, strict preflight utility. It inspects
every authored material slot and animation frame, records requested/resolved source, decoded RGBA8
dimensions and color/data interpretation, and rejects missing/corrupt data without extension
substitution. Intentional empty slots retain their scalar/default material values. Optional cooked
entries identify the archive and blob; a corrupt selected package entry never falls through to a
loose replacement. Cube Test's current comparison lane uses its own loose reference assets, not a
new cooked package.

Cube Test checks reference images before model import, then audits the constructed scene before
opening a native window or exporting. Native Vulkan logs the **actual** upload source, dimensions,
VkFormat, color space, sampler policy and fallback reason, including alternate-extension use in
other hosts. Sampler anisotropy is recorded separately when the shared sampler is created.

`RawIron.CubeTest.GalleryContracts` verifies:

- Scalar material defaults: lit, opaque, nonmetallic, rough, neutral normal scale, no accidental maps.
- Missing and corrupt files in all nine image slots, plus a missing animated albedo frame.
- One file used as albedo and normal retains distinct sRGB/linear interpretations.
- Every gallery-bound supported image decodes, and missing required gallery fixtures stop the host.
- Unsupported embedded base-color textures produce a named magenta marker, not an ordinary white material.
- Invalid room IDs fail with a diagnostic rather than silently starting at baseline.

**Newly exposed limitation:** `coffeemat.glb` has five embedded KTX2 images. The URI-only glTF importer
does not decode them. It now records each container/image index, byte range, MIME type, authored
sampler and slot color space, with an explicit unsupported-format reason. Dimensions are reported
as unavailable because no decode occurred. The independent report preserves each payload's hash.
The coffee geometry remains inspectable, with magenta base-color markers and scalar data-map defaults.
KTX2/BasisU material support remains a separate open TODO; supported loose-image uploads are not
evidence that these embedded maps rendered.

## Portal routes and help

All six links are bidirectional:

| Link | Forward arrival X / yaw | Return arrival X / yaw |
|---|---|---|
| baseline ↔ sprites | 19.65 / +90° | 6.25 / −90° |
| sprites ↔ normals | 45.65 / +90° | 32.25 / −90° |
| normals ↔ exporter | 71.65 / +90° | 58.25 / −90° |
| exporter ↔ interaction | 97.65 / +90° | 84.25 / −90° |
| interaction ↔ projectile | 123.65 / +90° | 110.25 / −90° |
| projectile ↔ teleport | 149.65 / +90° | 136.25 / −90° |

Arrival feet are Y=0.20, Z=0. Tests use the desktop player's 0.5×1.8×0.5 standing volume to verify
clearance and nearby walkable ground. Every trigger is exercised through `UpdatePortalTraveler`;
tests verify the exact destination/yaw, cleared velocity policy, no arrival bounce before or after
cooldown, unique IDs, destination room identity, labels, and return routes. Physical-headset comfort
and full live movement/input coverage remain open.

`CubeTestGallery` supplies seven room guides with subsystem, original source filenames/reference
examples, controls, and expected observations. `--gallery-help` prints them without loading assets.
F1 displays the current guide and outgoing portal labels; the window title shows the current room
and nearby portal destinations. F1 releases mouse capture/grabs before opening the guide. The shared
guide contents and room lookup are tested; the modal F1 interaction was not manually exercised in
this run. Assets stay under the experience's `assets/reference/threejs-r185` directory.

## Independent glTF checks

`Tests/VerifyCubeTestGltf.ps1` reads raw GLB headers, JSON and BIN chunks independently of the engine
importer. It proves both sources actually use integer POSITION quantization and that coffee has six
meshopt-compressed views with valid source byte ranges. It then exports the gallery and independently
checks accessor/view bounds, finite vertex streams, matching normal/UV counts, index ranges,
portable local URIs and image hashes. Imported subtrees retain the source primitive/vertex counts:

| Fixture | Primitives | Vertices |
|---|---:|---:|
| ShaderBall | 2 | 45,368 |
| Coffee | 2 | 177,247 |

The resulting gallery has 166 meshes and all eight supported reference images, each exported once
with original bytes. The unsupported embedded coffee maps are explicitly excluded from material
parity; their source payload records remain in the report rather than being represented by substitutes.

## Recorded results

- RelWithDebInfo build passed; **13 targeted CTests passed**. Logs:
  `Saved/five-todos-final-build.log` and `Saved/five-todos-final-tests.log`.
- Independent glTF report: `build/dev-msvc/Testing/CubeTestGltf/20260827-132013-382/report.json`,
  with adjacent exported glTF, binary, images and import diagnostics.
- Seven native room launches/captures succeeded. Each recorded all eight supported texture uploads
  with no Vulkan texture fallback. `Saved/visual_checks/five-todos-rooms/report.json` records executable
  and image hashes, with per-room BMPs and logs. GPU: NVIDIA GeForce RTX 5060 Ti; format 50 sRGB.
- Executable SHA-256: `62172F641AA684961BA80DFD034EB1A61B040711C983B079D7863AE2E65308ED`.
- All **six calibration GPU checks** still pass, including repeatability and retained shadows:
  `Saved/visual_checks/calibration/20260827-132031-359/report.json`.
- **Initial ordinary gallery visual sign-off failed:** the inspected normal-room capture had severe color
  clipping (white head/floor and saturated surroundings). Correct texture resolution does not prove
  correct shading or presentation. Investigate this separately from the passing isolated calibration
  fixture; the capture is diagnostic evidence only. Coarse shadow edges and PCVR remain open too.

## Normal-room clipping follow-up

Two separate causes were fixed:

- Contrast can produce negative RGB before the luma curve. Dividing negative luma by an epsilon
  caused negative colors to reverse into amplified white. The direct, lightweight hybrid and
  extended presentation shaders now bound their LDR curve input before the ratio. Disabled curves
  retain their original no-op behavior.
- The coffee fixture is about 209 source units tall. Its 1.1 exhibit scale made it about 230 meters
  tall, enclosing and shadowing adjacent 16-meter rooms. The experience now scales it to about
  three meters and places it on the floor; the generic importer retains the source geometry.
  `GalleryContracts` checks its world-space bounds stay inside the exporter room.

RelWithDebInfo builds passed (`Saved/gallery-clip-final-build.log`,
`Saved/gallery-probe-final-build.log`), and all **13 targeted CTests passed**
(`Saved/gallery-clipping-tests.log`). Build `VulkanNativeValidationProbe`, then run:

```powershell
& ./Scripts/Test-MaterialCalibration.ps1 -IncludeGallery
```

All **10 GPU checks passed** on NVIDIA GeForce RTX 5060 Ti, sRGB format 50. Report:
`Saved/visual_checks/calibration/20260827-135502-630/report.json`.
Executable SHA-256: `53E571E28FE72F43BC59BB9061D3C823C4F5A34255FD4479A49E7CCDFE3C64F6`.
The six original calibration checks still pass. Direct/lightweight-hybrid luma probes both keep
the black ROI at byte zero while their blue controls remain visible (122/133). The ordinary room's
sampled sky is RGB 170/184/191 and its lit floor is 145–146, with no white saturation. The adjacent
`normal-room.bmp` was visually inspected; the giant coffee enclosure and white head/floor are gone.

**Extended GPU validation is incomplete.** Its probe was stopped during driver pipeline compilation
after roughly four minutes with about 32 GB resident memory and no rendered frame. The partial log
is `Saved/visual_checks/calibration/20260827-135029-636/luma-extended.log`. The shader builds and
source guard passes, but neither proves rendered output. `-IncludeGallery -IncludeExtendedPost`
is a separate opt-in reproduction, not part of the ten passing checks.

This closes the clipping investigation, not normal-map fidelity or a showcase golden image.
The head remains too close/side-on and very dark, normal convention/panel orientation still needs
review, and cast shadows have coarse edges. Coffee's embedded KTX2 materials remain unsupported.
Third-party code belongs under `ThirdParty`; comparison assets remain owned by this experience.

Logs, captures and build artifacts are local evidence, excluded from Git. Source tests and commands
are checked in so the checks can be repeated without the disposable Three.js checkout.

## Motion investigation — 2026-08-27

A reported motion-rendering artifact is not yet characterized. Inspection found a concrete engine
shadow-stabilization defect: the camera-following center was snapped after projection, but that
moving point already maps to the shadow texture's center. Consequently the world-space texel grid
still slid continuously as the camera moved. The native renderer now uses
`StabilizeOrthographicShadowMatrix` to snap the projection of fixed world origin instead.

The engine regression in `VulkanFrameTuningSmoke` checks sub-texel stability, whole-texel boundary
crossings, unchanged scale and unchanged depth at 1024/2048/4096 shadow resolutions. Three targeted
tests passed (`Saved/motion-shadow-tests.log`), and CubeTest rebuilt successfully
(`Saved/motion-shadow-build.log`). All ten existing GPU checks passed in
`Saved/visual_checks/calibration/20260827-142137-870/report.json`; this report separately identifies
the CubeTest candidate and the existing luma-probe executable. CubeTest SHA-256:
`7DA41E55E1B7917F9E6EB459B42792245C17EA4FB081280399591C62F1CA3AF7`.

A hidden native `--jiggle-test --benchmark-frames=240` run also exited successfully
(`Saved/motion-jiggle-native.log`), exercising animated transforms. That run is runtime stability
evidence, not visual motion-quality approval. The user's exact artifact still needs a description
or recording to distinguish shadow crawling from aliasing, ghosting, geometry corruption or pacing.
Do not treat this correction as proof that the reported issue is fully resolved.
