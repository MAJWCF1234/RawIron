# Three.js Native Showcase TODOs

This is the execution tracker for [the native showcase roadmap](THREEJS_NATIVE_SHOWCASE_ROADMAP.md).  Check an item only after evidence is recorded; a successful headless run does not prove hardware rendering or PCVR comfort.

## P0 — renderer truth and safety

- [x] Create a minimal native material calibration scene: unlit swatches, original Three.js sRGB albedo/normal maps, metallic/roughness, transparency, depth, and shadow. Shared builder, `--material-calibration`, and test evidence: [validation lane](MATERIAL_CALIBRATION_VALIDATION.md).
- [x] Capture the exact `RelWithDebInfo` Cube Test executable on the target GPU; preserve launch arguments, renderer diagnostics, driver/device information, and image output. `--capture-native` and `Scripts/Test-MaterialCalibration.ps1` produce GPU BMPs plus a hashed evidence report; [results](MATERIAL_CALIBRATION_VALIDATION.md#shadow-and-roughness-regression-evidence--2026-08-27).
- [x] Verify every fixture texture resolves to the intended source/package blob. Strict scene audit and native upload logs cover all eight supported images; embedded coffee image ranges/hashes and unsupported-format fallbacks are explicit. [Texture evidence](CUBE_TEST_GALLERY_VALIDATION.md).
- [ ] Fix any color-space, material-binding, UV/tangent, light, depth, or compositor defect exposed by the calibration scene before adding visual-complexity work.
- [x] Investigate the triangular self-shadow patterns and excessive black floor regions in the 2026-08-27 calibration GPU capture. Fixed PCF receiver-plane depth and non-finite pseudo-reflection roughness; targeted CTests and six hardware checks pass. [Evidence and remaining quality limits](MATERIAL_CALIBRATION_VALIDATION.md#shadow-and-roughness-regression-evidence--2026-08-27).
- [ ] Review cast-shadow edge quality and resolution after the receiver-plane correction; retain real shadows while improving the coarse PCF edges visible in the calibration capture.
- [x] Add regression tests for material defaults and missing/invalid asset behaviour. Nine map slots and animated frames are audited; missing/corrupt loose assets stop Cube Test, and unsupported embedded base color gets an explicit magenta marker. [Regression evidence](CUBE_TEST_GALLERY_VALIDATION.md).
- [ ] Establish golden, reviewable hardware captures for baseline materials and the Lee Perry-Smith normal-map fixture.
- [x] Document the exact supported GPU presentation validation lane; distinguish it from software preview and headless CTest. See [validation lane](MATERIAL_CALIBRATION_VALIDATION.md); visual quality sign-off remains open.

## P0 — make the current Cube Test zones trustworthy

- [x] Keep actual comparison assets inside the experience so the upstream checkout can be removed: 13 copied files verified against Three.js 0.185.1, with licenses and SHA-256 manifest; `RawIron.CubeTest.ReferenceAssets` checks identity and rejects external links. Baseline/calibration use these copies; RAWIRONX32 is opt-in.
- [x] Replace identified Cube Test/VR host implementations with reusable engine APIs: raw mouse capture, ray grab/throw, bounded prop authority codec, XR atlas/mesh conversion and present timing. The experience retains asset/layout/tuning bindings. [Ownership and tests](CUBE_TEST_SHARED_HOST_VALIDATION.md).
- [x] Give each portal an authored ID, destination, safe arrival pose, label, and automated traversal check. All 18 directions pass standing clearance, ground support, exact pose, cooldown/no-bounce, unique-ID and return-route checks. [Portal evidence](CUBE_TEST_GALLERY_VALIDATION.md).
- [x] Add neutral native OpenGL/converted-DirectX/unconverted controls and mirrored-UV rows; correct engine tangent frames and authored normal strength. See [normal comparison evidence](NORMAL_MAPPING_COMPARISON_VALIDATION.md).
- [ ] Confirm the normal-map room renders `NormalMapOpenGL.png`, `NormalMapDirectX.png`, and Lee Perry-Smith maps with verified convention handling.
- [x] Verify compressed glTF import (`KHR_mesh_quantization` / `EXT_meshopt_compression`) and exported glTF texture references with independent file checks. Raw GLB/JSON/binary inspection verifies geometry counts, streams/indices and eight exported image hashes; this does not certify unsupported KTX2 material parity. [File-check evidence](CUBE_TEST_GALLERY_VALIDATION.md).
- [ ] Support coffee's five embedded KTX2/BasisU images through the reusable glTF/texture pipeline; remove the explicit diagnostic markers only after decoded material fidelity is verified.
- [x] Investigate severe color clipping in the ordinary normal-room GPU capture. Fixed negative-luma sign reversal in all three presentation shaders and the oversized coffee exhibit enclosing/shadowing neighboring rooms. Ten GPU regression checks and 13 targeted tests pass; full gallery material parity remains open. [Follow-up evidence](CUBE_TEST_GALLERY_VALIDATION.md#normal-room-clipping-follow-up).
- [ ] Declare all Cube Test reference assets in cooked project packages; retain loose assets only where authoring requires them.
- [x] Add a repeatable room-by-room benchmark and a readable performance report. Ten rooms × two launches, warmup exclusion, raw CSV, CPU present cadence percentiles and hashed JSON/Markdown evidence. [Results and limits](CUBE_TEST_SHARED_HOST_VALIDATION.md).
- [x] Update gallery signage/help so players can identify the native subsystem, source fixture, controls, and expected observation in each zone. Ten shared room guides power `--gallery-help` and in-game F1; window titles show the current room and nearby portal destination. [Help and checks](CUBE_TEST_GALLERY_VALIDATION.md).

## P1 — desktop and PCVR shared experience

- [x] Validate Half-Life-2-style desktop controls: focus-safe mouse capture, WASD/jump/sprint/collision engine tests, corrected full spawn reset, shared grab/throw tests and twelve portal routes; visible native launch and live traversal observed. [Exact validation scope](CUBE_TEST_SHARED_HOST_VALIDATION.md).
- [ ] Validate PCVR smooth turning, optional snap turning, locomotion, selection, grab/throw, teleport, collision, and portal traversal on a physical headset.
- [x] Ensure haptics require qualified selection/grab/contact events, focus and tracking. Removed hover pulses; engine caps amplitude/duration and per-hand cadence with regression tests. Physical haptic feel remains unverified. [Policy evidence](CUBE_TEST_SHARED_HOST_VALIDATION.md).
- [x] Add desktop↔PCVR authority tests: independent shared worlds, byte-identical snapshots, atomic failure, finite/bounded commands and peer budgets; both remote hosts stop advancing prop physics locally. [Coverage and network limits](CUBE_TEST_SHARED_HOST_VALIDATION.md).
- [ ] Add frame-time and comfort metrics for PCVR; fail safely when OpenXR, Vulkan requirements, or headset tracking are unavailable.
- [ ] Add authored controller/hand representation only after shared world correctness and material fidelity are proven.

## P1 — showcase coverage records

The pinned r185 Git-tree inventory is [tracked here](THREEJS_EXAMPLES_INVENTORY_R185.md): 589 HTML examples are catalogued. This is an audit list, not a claim that all rows are implemented. The disposable upstream checkout is not required at runtime; final coverage sign-off still needs source review against the identified revision.

- [x] Record the target revision’s example inventory: the existing r185 Git-tree list contains 589 numbered rows. This maintenance pass checked the recorded list, not upstream completeness; per-example decisions and source verification remain open below.
- [ ] For every example, create a decision record: implement, group with another zone, defer, or not applicable; include the reason and the Raw Iron owner.
- [ ] Record licence/provenance and exact asset filenames for every accepted fixture.
- [ ] Maintain a public coverage matrix with current completion state and links to tests, zone, and engine documentation.

## Initial reference matrix

| Reference example(s) | Native Raw Iron capability | State | Exit evidence |
|---|---|---|---|
| `css3d_sprites.html` | Camera-facing sprite batch | Prototype | Desktop + PCVR capture, correct blend/depth policy, batch benchmark |
| `misc_exporter_gltf_normals.html` | Normal conventions and native export validation | Prototype | Verified OpenGL/DirectX maps and exported artifact inspection |
| `misc_exporter_gltf.html` | Compressed glTF import and textured export | Prototype | Independent import/export and cooked-package tests |
| `webgl_materials_normalmap.html` | Tangent-space normal mapping | Prototype | Hardware material calibration and fixture capture |
| `webxr_xr_cubes.html`, `webxr_xr_dragging.html` | Dynamic props, ray selection, ownership | Prototype | Desktop/PCVR interaction and replication tests |
| `webxr_xr_haptics.html`, `webxr_xr_ballshooter.html` | Qualified haptics, projectile pool, impulse targets | Prototype | Live headset haptic log and bounded-simulation tests |
| `webxr_vr_teleport.html` | Validated parabolic teleport | Prototype | Slope/clearance edge-case tests and headset run |

The matrix above is the current accepted seed set, not the complete Three.js inventory. The complete matrix must
be generated from the locally available upstream checkout (or an explicitly identified, licensed revision) before
claiming all-example coverage.

## P2 — native engine capability tracks

- [x] Enhance the **existing structural primitive collection** for geometry examples:
  smooth/UV-preserving `revolve`, `spline_sweep`, full `torus`, and cataloged
  `mobius`/`parametric_patch`; keep tessellation private to `RawIron.Structural`.
  Nine exhibits on three new platforms use `SpawnStructuralPrimitiveBundle`.
  Sixteen targeted tests pass, including structural graph/preset integration,
  XR upload, eighteen portal routes, and exported topology. Native captures and
  limits are in [structural surface validation](STRUCTURAL_SURFACE_SHOWCASE_VALIDATION.md).
- [ ] Improve shadow-side material readability on the new geometry platforms;
  the current GPU captures prove geometry presentation, not Three.js lighting parity.

- [ ] **Geometry and animation:** audit primitives, instancing, LOD, skinning, morphs, skeletal animation, and procedural geometry examples; define their Raw Iron data contracts.
- [ ] **Loaders and exporters:** audit format-specific examples; prioritize broadly useful, standards-backed content paths and explicit unsupported-format diagnostics.
- [ ] **Materials and lighting:** cover PBR maps, texture transforms/encodings, clearcoat/transmission where supported, light types, shadows, and reflection/environment data.
- [ ] **Render architecture:** cover render targets, post-processing, anti-aliasing, GPU particles, compute-style work where it suits Raw Iron, and debug views through native render graph/system boundaries.
- [ ] **Environment:** cover sky, fog, environment probes, terrain/water/volumetric ideas only after baseline shading is correct and measurable.
- [ ] **World systems:** cover tracing, physics constraints, character movement, audio, text/UI, navigation, and interaction examples as authorable systems.
- [ ] **Networking:** turn relevant interactive zones into deterministic desktop↔PCVR multiplayer validation, with authority and package compatibility checks.

## P2 — extension and package readiness

- [x] Implement generic package metadata, dependency resolution, entry-point validation, permissions, and content compatibility fingerprints. These existing foundations live in `RawIron.Content`; see [package runtime](../Documentation/02%20Engine/12%20Package%20Runtime.md) and [maintenance verification](ENGINE_MAINTENANCE_2026-09-04.md). Integrity fingerprints are not publisher authentication.
- [x] Implement atomic package mounting, shared dependency reference counts, and collision rejection in `PackageMountRegistry`; retain the existing engine API rather than introducing showcase-owned mounting.
- [ ] Complete end-to-end extension hot-load/hot-unload, state migration, rollback and asset-residency validation. Mount transactions and a reload-policy coordinator exist; native code remains session-restart only.
- [ ] Enable and validate package-set preflight in every shared showcase host. The opt-in authoritative-network contract and mismatch rejection already exist; package acquisition and physical desktop/PCVR session evidence remain separate work.
- [ ] Build a generic Shadertoy-compatible extension proof only after the generic extension lifecycle is real.  It must use Raw Iron renderer/package APIs and the site's terms/licensing; it is not copied Three.js or Shadertoy runtime code.

## 2026-09-04 engine maintenance

- [x] Preserve authored UVs through structural spline copies, terrain retention and mesh merging.
- [x] Respect already-local structural input and correct reflected triangle winding.
- [x] Share inverse-transpose normal handling across structural, software preview and XR mesh conversion.
- [x] Move legacy miniaudio/stb vendor sources into `ThirdParty` without changing library bytes.

Regression reproduction, configured-workspace verification and remaining limits are recorded in the [maintenance report](ENGINE_MAINTENANCE_2026-09-04.md). These checks do not replace the release gates below.

## P3 — final release evidence

- [ ] Build the full workspace from a clean checkout using the documented CMake preset.
- [ ] Run the registered test suite plus showcase-specific headless/smoke tests.
- [ ] Run desktop hardware and physical-headset validation using the exact release candidates.
- [ ] Package only declared game/workspace assets; verify no loose global authoring textures are required at runtime.
- [ ] Update README, Cube Test guide, asset provenance, controls, capability matrix, and known limitations in the same change.
- [ ] Publish the results as a versioned showcase report with clear pass/fail/deferred status.

## Documentation to maintain with every zone

- [ ] Reference example path, upstream revision, licence, and asset list.
- [ ] Raw Iron engine subsystem/API and authoring surface.
- [ ] Desktop and PCVR controls, accessibility/comfort notes, and reset route.
- [ ] Test commands, hardware capture location, benchmark target, and known limitations.
- [ ] Package membership, cooker input, and runtime mount contract.
