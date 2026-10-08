# Mesh feature roster validation, 2026-10-08

Four example families now have native Cube Test rooms, bringing the gallery to
22 rooms and 42 bidirectional portal routes. This is capability evidence for these
families, not a claim that all 589 upstream examples or all their controls pass.

| Room | Three.js r185 reference | RawIron owner and behavior |
|---|---|---|
| vertex-colors | [geometry/colors](https://github.com/mrdoob/three.js/blob/r185/examples/webgl_geometry_colors.html) | Core Mesh.colors; scene-linear RGB interpolated in Vulkan, software rasterization and XR vertex streams |
| uv-transform | [materials/texture_rotation](https://github.com/mrdoob/three.js/blob/r185/examples/webgl_materials_texture_rotation.html) | SceneUtilities TransformMeshUvs; centered rotation, offsets, repeat and mirroring |
| morph-targets | [morphtargets](https://github.com/mrdoob/three.js/blob/r185/examples/webgl_morphtargets.html) | SceneUtilities BlendMeshMorphTargets; absolute weighted targets, optional authored normal blending or recomputed normals |
| clipping | [clipping](https://github.com/mrdoob/three.js/blob/r185/examples/webgl_clipping.html) | SceneUtilities ClipMeshPlanes; intersect local half-spaces and interpolate UV, color and normal streams |

Engine operators preserve the source and validate stream sizes, finite parameters,
indices and bounded output. Games supply layout, assets, materials and cached animation
samples. Vertex RGB round-trips through glTF COLOR_0.

## Reproduction and evidence

Build targets: MeshFeatureOpsSmoke, RawIron.CubeTestGame, RawIron.CubeTest.WorldSmoke,
VulkanFrameTuningSmoke and RawIron.VRShowcase, RelWithDebInfo in build/dev-msvc.

```
ctest --test-dir build/dev-msvc -C RelWithDebInfo -R "MeshFeatureOpsSmoke|CubeTest.|SceneUtilities.GltfExporterSmoke|Vulkan.NativeScenePreviewDetailNormalSmoke|Vulkan.FrameTuningSmoke|GameHost.ProjectBootSmoke" --output-on-failure
```

11 targeted tests passed. They exercise analytic UV/morph/clip results, invalid input,
software pixels, XR RGB vertex streams, actual glTF round-trip, boot contracts,
22-room/portal layout, platform geometry and saved glTF files. No physical-headset
visual or comfort validation was performed.

Run Scripts/Test-MeshFeatureRoster.ps1 for eight direct native Vulkan captures
(two fixed phases for each room). Latest captures and executable/image hashes:
Saved/visual_checks/mesh-features/20261008-003606-979/report.json.
The adjacent pixel-comparison.json verifies an unchanged static vertex-color control
and changed UV/morph/clipping pixels; pixel differences are not a quality score.

The pre-lighting benchmark at Saved/benchmarks/cube-test/20261008-001827-624/report.md
measured about 23.5–23.7 ms mean CPU present intervals across baseline and four new
rooms on RTX 5060 Ti, hidden 1280x720, one run with 30 warmup/120 measured intervals.
This includes present waits, is not GPU timing or physical-display FPS, and does
not establish the 200 FPS target. It predates the shading corrections.

A broader workspace build rebuilt the renderer and desktop applications but failed
in existing Tools/ri_tool/src/main.cpp edits (invalid Mat4 indexing and a missing
function boundary). Targeted renderer/game builds succeeded. No release was published.

The subsequent [general readiness slice](GAME_READINESS_SLICE_2026_10_08.md)
repairs that tool blocker and records the newer full-workspace validation.

## Chunky shading investigation

Before image: Saved/visual_checks/mesh-features/20261008-001738-501/morph-targets-16.bmp.
After image: Saved/visual_checks/mesh-features/20261008-003606-979/morph-targets-16.bmp.

The directional map previously covered 180 metres: 4.4 cm per texel at the highest
4096 resolution before PCF filtering. VulkanNativeSceneFrame.shadowCoverageRadius
now provides a finite, bounded game request (8..256 metre half-width, compatibility
default 90). Cube Test binds native_shadow_radius=16 in its postprocess script:
32-metre coverage, 5.625 times finer spacing, same map allocation. Coverage follows
the camera and fades at its boundary; distant directional shadows leave the region.
Cascades are still needed for simultaneous near/far quality in large worlds.

Authored morph normal targets remove rounded-target shading seams without welding
UV topology or erasing intentionally hard edges. PCF derives the receiver plane from
unoffset geometry rather than a surface bent by interpolated normal bias. Point-light
contributions no longer incorrectly consume the directional light's shadow visibility.
Cast shadows remain enabled. GPU inspection confirms finer cast-shadow edges and
smoother highlights; residual curved-surface terminator faceting/speckling remains.

## Limits

- UV transformation currently operates on mesh UVs rather than independent per-map
  texture matrices.
- Gallery animation uses 32 cached samples at eight samples/second. The reusable
  operator accepts arbitrary weights; GPU morph attributes and continuous GPU blends
  are not implemented by this work.
- Clipping is geometric and mesh-local, with intentional open boundaries. Fragment
  clipping, section caps and separately configurable shadow clipping remain unimplemented.
- Mesh vertex colors are RGB; glTF VEC4 alpha is not represented in that stream.
  The separate ray-traced preview does not yet consume vertex RGB.
- XR stream tests establish upload data, not headset rendering/comfort or parity.
- Source 2 quality superiority and complete Three.js parity remain unproven.
