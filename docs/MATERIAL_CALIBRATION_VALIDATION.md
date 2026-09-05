# Material calibration validation

The native fixture is owned by `RawIron.SceneUtilities::BuildMaterialCalibrationScene`.
Cube Test supplies paths from **its own** `assets/reference/threejs-r185` directory. No external
Three.js checkout, JavaScript runtime, LRT textures, or RAWIRONX32 pack is needed for this lane.
Asset identity and licenses are recorded in that directory's README and `asset-manifest.json`.

## Reproduce

Run from the repository root with the installed CMake that configured the build. On this machine
that is `C:/Program Files/CMake/bin/cmake.exe`; the Strawberry CMake on PATH did not regenerate
the existing MSVC cache correctly until the installed CMake configured it again.

```powershell
& 'C:/Program Files/CMake/bin/cmake.exe' --preset dev-msvc
& 'C:/Program Files/CMake/bin/cmake.exe' --build --preset build-dev-msvc --target RawIron.CubeTestGame AuthoringHelpersSafetySmoke RawIron.CubeTest.WorldSmoke --parallel 6
& 'C:/Program Files/CMake/bin/ctest.exe' --test-dir build/dev-msvc -C RelWithDebInfo -R 'AuthoringHelpersSafetySmoke|RawIron.CubeTest.' --output-on-failure
$exe = 'build/dev-msvc/Games/CubeTest/App/RelWithDebInfo/RawIron.CubeTestGame.exe'
& $exe --workspace-root=. --material-calibration --background --benchmark-frames=8
& $exe --workspace-root=. --material-calibration --save-preview --output=Saved/visual_checks/material-calibration-software.bmp
& $exe --workspace-root=. --material-calibration --background --capture-native=Saved/visual_checks/material-calibration-gpu.bmp
& ./Scripts/Test-MaterialCalibration.ps1
& $exe --workspace-root=. --material-calibration
```

The hidden eight-frame run exercises native Vulkan initialization/submission/presentation, but
does not certify visible pixels or performance. `--save-preview` is a separate software layout check.
`--capture-native=<path.bmp>` reads the first rendered swapchain image before presentation and exits
after one frame unless `--benchmark-frames` specifies another count. It preserves the GPU's encoded
RGB bytes, works with a hidden/occluded window, and rejects unsupported swapchain formats or missing
transfer-source support. It cannot be combined with software preview or glTF export. Capture waits
for GPU readback, so it is a diagnostic operation, not a performance benchmark.

`Scripts/Test-MaterialCalibration.ps1` launches the exact RelWithDebInfo executable twice at
1280×720, checks both images, and writes a timestamped directory under `Saved/visual_checks/calibration`.
Its `report.json` records executable/shader SHA-256 hashes, launch arguments, display drivers, image
hashes, and check results; adjacent logs identify the selected GPU and swapchain format. Six checks
cover dimensions, the flat receiver's self-shadow pattern, the rough floor's black region, RGB channel
order, retained cast shadows, and repeatability across the two launches. Pixel regions are specific to
this fixed camera. This optional Windows hardware lane is separate from CTest and is not a general
image-quality score or a cross-driver golden-image comparison.

Build `VulkanNativeValidationProbe` and run `Scripts/Test-MaterialCalibration.ps1 -IncludeGallery`
to add four checks: a black/colored-control luma-curve probe in direct and lightweight hybrid modes,
plus normal-room sky and floor bounds. `-IncludeGallery -IncludeExtendedPost` adds the extended
shader probe, but driver compilation can require substantial time and memory (see evidence below).
These captures are recorded separately in `galleryCaptures`; the small shader probes do not
establish full hybrid/extended visual parity. The default invocation retains the six calibration
checks without this additional hardware work.

Use `--hybrid-hdr` only as a separately labeled experimental comparison. Never mix its evidence
with the default direct native Vulkan path. No real-headset validation is implied by either lane.

## What to inspect

- Top row: linear unlit black, 18% gray, 50% gray, white, red, green, blue. Presentation still applies
  the engine tonemapper; input values are not assertions of identical display byte values.
- Second row: original `hardwood2_diffuse.jpg`, a linear gray control, an untextured lit normal
  control, and the original OpenGL/DirectX normal maps. Both mapped panels use the same tint and
  XY strength; only DirectX's Y sign is inverted. Compare the PNGs/reference example, not a generated substitute.
- Lower left: metalness 0/1 rows and roughness 0.1/0.5/0.9 columns under one white directional light.
- Lower right: red in front of blue for depth; a 50% red transparent panel over blue for blend.
- Floor and right post: shadow receiver and caster. Light intensity is 1, ambient is neutral 0.12,
  fog is zero, exposure/contrast/saturation are 1, and project shader grading is not loaded.

This is a calibration fixture, not a finished showcase zone. Hardware material fidelity, the
Lee Perry-Smith visual comparison, PCVR, frame-time budgets, and cooked packaging remain separate gates.

## Initial evidence — 2026-08-27

- RelWithDebInfo build passed for `RawIron.CubeTestGame`, `AuthoringHelpersSafetySmoke`, and
  `RawIron.CubeTest.WorldSmoke`. Eight targeted tests passed: authoring helpers, glTF exporter,
  Cube Test world, calibration, reference checksums, native detail-normal wiring, extended
  composite wiring, and reference shader migration. Logs: `Saved/calibration-build.log` and
  `Saved/calibration-tests.log`. This is not a clean full-workspace test result; an earlier broader
  run could not run 21 tests whose executables were absent from this local build.
- The 13 project-owned asset/license files match the original 0.185.1 distribution byte for byte.
  Checksum validation uses only the copied files. No runtime/build path into the external checkout
  was found in Source, Apps, Games, Tools, Scripts, Tests, or CMake sources.
- Native eight-frame calibration boot passed on NVIDIA GeForce RTX 5060 Ti, Windows driver
  `32.0.16.1088`, sRGB swapchain format 50, mailbox presentation. Log: `Saved/calibration-native.log`.
- Visible GPU capture: `Saved/visual_checks/material-calibration-native.png`, client 1280×720
  (window border included), direct native Vulkan with `--workspace-root=O:/RawIron --material-calibration`.
  Executable SHA-256: `593E60335DB43E76ABCC7C3C58A8486C59A2FC0860F1A8CAD2836EFDD7663B69`.
  Log: `Saved/calibration-native-visible.log`. The separate software capture is
  `Saved/visual_checks/material-calibration-software.bmp`.
- Inspection caught the sun emitting upward in both fixture/gallery authoring. Positive pitch
  now emits downward; the shared fixture test guards that direction. Lit samples became visible
  and the normal panels' main lobes became comparable. That initial capture still showed triangular
  self-shadow patterns and excessive black floor regions; the follow-up below fixes their causes.

## Shadow and roughness regression evidence — 2026-08-27

- Fixed two independent native shader defects. Pseudo-reflection roughness could reach 1.18,
  producing a negative highlight exponent and non-finite output even at zero reflection weight.
  The shader now clamps roughness before evaluating the lobe. PCF formerly compared every tap to
  the center receiver depth; it now projects depth to each nearest texel's receiver-plane position.
  Cast shadows and the existing bounded bias remain enabled.
- RelWithDebInfo builds passed for Cube Test, its world smoke test, the Vulkan validation probe,
  frame tuning, and hybrid post-process safety. Ten targeted CTests passed (the initial eight plus
  `RawIron.Render.Vulkan.FrameTuningSmoke` and `RawIron.Render.Vulkan.HybridPostProcessSafetySmoke`).
  Logs: `Saved/calibration-final-build.log`, `Saved/calibration-fixes-build.log`,
  `Saved/calibration-final-tests.log`, and `Saved/calibration-render-tests.log`.
- Direct GPU evidence: `Saved/visual_checks/calibration/20260827-130039-776/report.json` and
  `frame-1.bmp` / `frame-2.bmp`. All six hardware checks passed on NVIDIA GeForce RTX 5060 Ti,
  driver `32.0.16.1088`, sRGB format 50. The flat receiver interior is uniform at byte 173;
  the sampled rough floor is uniform at 155; the cast-shadow control is 16. Both BMP hashes are
  `3C6762E55E13E181460527AFB5BA23C59B51F3490F1CC8995CBD4134800D1A11`.
  Executable SHA-256: `38796927040780CFCA0B874204301EE62EC115FF1D92E7C0A3A86FC53C89FD50`.
- The generic hidden `VulkanNativeValidationProbe` also captured 320×200 and completed eight frames,
  including scene-cache retirement (`Saved/native-probe-capture.log`). A separate experimental
  hybrid capture completed (`Saved/calibration-hybrid-capture.log`); it is not included in the six
  direct-path pixel checks and does not establish hybrid visual parity.
- These specific defects are resolved; broad visual sign-off remains open. Coarse/jagged cast-shadow
  edges still need quality review, as do the Lee Perry-Smith fixture, all material defaults, other
  GPU/driver combinations, and physical-headset output. These captures are regression evidence,
  not an approved showcase golden image. The full workspace suite was not rerun.

## Gallery luma-curve follow-up — 2026-08-27

The ordinary gallery exposed another defect absent from neutral calibration: contrast-produced
negative RGB reversed into white in the luma ratio. All three presentation shaders now clamp the
active LDR curve input; a separate oversized coffee exhibit was also enclosing/shadowing neighboring
rooms. [Gallery evidence](CUBE_TEST_GALLERY_VALIDATION.md#normal-room-clipping-follow-up) records both
fixes, the bounds regression and remaining visual limitations.

`Scripts/Test-MaterialCalibration.ps1 -IncludeGallery` passed all **10 GPU checks**, including the
original six, two direct/lightweight-hybrid black/colored-control probes and two ordinary-room
checks. Report: `Saved/visual_checks/calibration/20260827-135502-630/report.json`.
Thirteen targeted CTests passed. The optional extended GPU probe was stopped during driver
compilation at roughly four minutes and 32 GB resident memory, before producing a frame; its source
and build checks pass, but extended rendered output is unverified. Use `-IncludeExtendedPost` only
when prepared for that additional compilation cost. No full gallery or hybrid parity is claimed.

Logs/captures under `Saved` are local evidence, intentionally excluded from Git. The source, asset
manifest, tests, and these commands allow another machine to reproduce them.
