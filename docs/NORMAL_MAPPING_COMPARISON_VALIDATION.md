# Native normal-mapping comparison — 2026-09-04

## Engine work

This increment extends the existing C++ material/scene/renderer paths. No Three.js
runtime or JavaScript was added. Structural geometry continues to use the existing
structural primitive collection; the comparison fixture uses the engine's existing
quad helper, not a second primitive system.

| Defect | Correction |
|---|---|
| Native tangent reconstruction drops UV determinant sign and bitangent handedness | Preserve both signs; compare against independently authored analytic normals |
| Native tangent fallback depends on absolute screen derivatives | Relative UV degeneracy test and finite frame fallback |
| Software tangent reconstruction overwrites mirrored bitangent | Preserve the UV bitangent orientation after orthogonalization |
| Software normal maps ignore authored XY scale / DirectX conversion | Apply `Material::normalScale` before normalization |
| Missing maps and degenerate UVs invent a lighting direction | Retain geometric normals; invalid strengths also fall back safely |
| Native quality tiers alter bump amplitude and standard maps implicitly add micro-occlusion | Honour authored normal scale; keep artificial relief limited to explicit layered/mixed-media styles |

`RawIron.SceneUtilities::AddNormalMappingComparisonPanels` is the reusable scene
builder. `BuildNormalMappingComparisonScene` adds an isolated neutral camera/light
fixture. Cube Test supplies copied asset paths and room placement only. Both desktop
and VR hosts consume the same gallery scene; this is not physical-headset approval.

## Comparison controls

```powershell
build/dev-msvc/Games/CubeTest/App/RelWithDebInfo/RawIron.CubeTestGame.exe --workspace-root=O:/RawIron --normal-comparison
```

Columns, left to right: **OpenGL**, **DirectX with Y conversion**, **DirectX without
conversion** (intentional negative control). The lower row mirrors the U coordinate.
The first two columns should agree on bump orientation; the third should disagree.
All use the same neutral color and roughness, without emissive tint. Native image
rows are top-first, whereas these source normals use a bottom-left tangent basis:
the fixture flips V to keep labels upright and composes a common Y-strength sign
correction with the DirectX conversion. The resulting GL/DX scales are -0.5/+0.5.
The final capture checks also require the marked extrusion to be brighter on top
under the upward-facing light direction; pair agreement alone can hide a shared
orientation error. The labels in
the original maps differ, so whole-image byte equality is not a valid comparison.

The regular `--start-room=normals` gallery also contains these six panels. The
Lee Perry-Smith head is smaller and offset so it no longer obscures the central
panel comparison. F1 describes the controls. No additional platform or portal
system was introduced.

## Reference provenance

Read-only reference reviewed locally:
`O:/three.js-master/three.js-master/examples/misc_exporter_gltf_normals.html`;
its checkout package version is 0.185.1. No code from it was copied or executed.
The two existing experience-owned texture files were compared with that checkout:

| File | Matching SHA-256 |
|---|---|
| NormalMapOpenGL.png | `03856DC8FD1ED6391553A86ACC40C579E886AD772084D0EE58106EDE4D33F5A6` |
| NormalMapDirectX.png | `BD11394B4C5BBF887A7DBA3658890ED505CB6AC0304E0C3BEDFB5B2220253CD9` |

Runtime files remain in `Games/CubeTest/assets/reference/threejs-r185/textures`,
with the existing license and asset manifest. The upstream folder is disposable.
No new third-party dependency was required.

## Verification and limits

The final analytic fixtures use identity model scale so their control normals are
not accidentally inverse-scaled. An early draft fixture did have that error and
was corrected before the isolated reproduction below.

With only the old tangent/ignored-strength behaviour reintroduced into the final
fixtures, the software regression failed four modes and **8/16 GPU cases failed**.
The corrected GPU matrix passed **16/16**, with zero mean absolute pixel error in
all sampled-versus-analytic regions: ordinary, U-mirrored, V-mirrored and degenerate
UVs, at 320/1280 widths and quality tiers 0/2. The CPU test additionally covers
DirectX conversion, zero strength, tiny UVs, missing textures and non-finite strength.

Reproduction logs:
`Saved/normal-frame-isolated-software-before.log` and
`Saved/normal-frame-isolated-gpu-before.log`. The test restores production source
before the final rebuild. Regression command: `Scripts/Test-NormalMapping.ps1`.
Its synthetic one-pixel TGA is a test fixture under Saved, not a substituted demo
asset. The actual demo retains the original 512×512 Three.js maps.

## Final results

- Full configured RelWithDebInfo workspace build: passed
  (`Saved/normal-comparison-final-build.log`).
- Registered CTests: **159/159 passed**, 55.71 seconds
  (`Saved/normal-comparison-all-tests.log`).
- Native normal matrix plus original-asset demo: **18/18 passed** with
  `Scripts/Test-NormalMapping.ps1 -IncludeDemo`.
- Existing material/gallery GPU regressions: **10/10 passed** with
  `Scripts/Test-MaterialCalibration.ps1 -IncludeGallery`.
- `git diff --check`: passed.

[Final normal GPU report](../Saved/visual_checks/normal-mapping/20260904-222853-435/report.json)
and [reviewed demo capture](../Saved/visual_checks/normal-mapping/20260904-222853-435/comparison.png).
[Material regression report](../Saved/visual_checks/normal-comparison-material-regression/20260904-222934-340/report.json).

The actual source-map converted-pair error was 1.132/0.616 red-channel levels in
matched interior regions (standard/mirrored rows). The deliberately unconverted
controls differed by 9.325/8.928 levels. Extrusion top/bottom samples were 148/127
and 147/126 under the known light direction. These are narrowly defined comparison
checks, not a universal material-quality score.

Tested Cube Test executable SHA-256:
`54E55CB2E0B635E5E00FE3D475D6848EF98A1EC460AAA56304023C6FDC372260`.
The report includes shader/executable hashes, capture arguments and driver inventory.
The isolated demo was visually reviewed after its final origin correction. The
regular gallery still has dark sides and coarse shadows; it is not visually signed off.

Remaining: physical HMD validation, Lee Perry-Smith material/lighting approval,
coarse cast shadows, full glTF export round-trip of negative normalScale, KTX2/BasisU
support, complete cooking, and clean-machine release/performance evidence. Completing
example coverage alone will not certify that a game is ready to ship.
