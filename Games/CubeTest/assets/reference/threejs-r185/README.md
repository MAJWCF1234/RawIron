# Three.js reference assets

These assets come from the Three.js `0.185.1` example distribution and are retained only as
cross-engine compatibility fixtures. Raw Iron does not include or execute Three.js source code.

These are **physical copies inside the experience's assets**, not links into an external Three.js
checkout. The build, tests, desktop host, and shared PCVR world do not need the original checkout.
`asset-manifest.json` records the upstream-relative filename, size, and SHA-256 of all 13 copied
assets/license files. Each was checked against the local 0.185.1 distribution on 2026-08-27.
`RawIron.CubeTest.ReferenceAssets` verifies the copies without accessing that distribution.

The baseline now uses `hardwood2_diffuse.jpg` and `uv_grid_opengl.jpg`; the isolated calibration
uses the hardwood image and both original normal-convention PNGs. PBR tint/roughness variations
are Raw Iron-authored comparisons, not claims to reproduce a reference example pixel-for-pixel.
Generated gray/color swatches are material constants only, not replacement asset images.
The original Lee Perry-Smith geometry, color/specular/normal maps and its own license remain
beside one another below `models/gltf/LeePerrySmith`.

These copies are current authoring/runtime inputs. Cooked project-package declaration is still
tracked separately; this manifest proves identity and ownership, not `.ripak` packaging.

Source: <https://github.com/mrdoob/three.js>

Reference examples:

- `css3d_sprites.html`: `textures/sprite.png`
- `misc_exporter_gltf_normals.html`: `textures/NormalMapOpenGL.png`,
  `textures/NormalMapDirectX.png`
- `misc_exporter_gltf.html`: `textures/uv_grid_opengl.jpg`,
  `textures/hardwood2_diffuse.jpg`, `models/gltf/ShaderBall.glb`,
  `models/gltf/coffeemat.glb`
- `webgl_materials_normalmap.html`: `models/gltf/LeePerrySmith/LeePerrySmith.glb`,
  `Map-COL.jpg`, `Map-SPEC.jpg`, and `Infinite-Level_02_Tangent_SmoothUV.jpg`
- `webgl_geometries.html`: the existing `textures/uv_grid_opengl.jpg` is reused
  for native structural revolve, torus and Mobius comparisons. The revolve's
  mathematical profile is converted to meters; no JavaScript implementation is copied.
- `webgl_geometry_extrude_splines.html`: open/closed spline extrusion is a behavior
  reference for the enhanced structural `spline_sweep`; its gallery reuses the local
  UV grid and authors native control points. No new external assets are needed.

Behavior-only references with no copied JavaScript or required content assets:

- `webxr_xr_cubes.html`: bounded dynamic cube field
- `webxr_xr_dragging.html`: ray selection and exclusive two-hand grab ownership
- `webxr_xr_haptics.html`: collision-strength haptic feedback
- `webxr_xr_ballshooter.html`: bounded projectile pooling and impulse-driven targets
- `webxr_vr_teleport.html`: parabolic targeting, slope rejection, and standing-volume clearance

These behaviors are implemented through `RawIron.World` simulation and the generic
`RawIron.XR.OpenXR` interaction-frame contract. They are not ports of Three.js internals.

The accompanying `LICENSE.txt` is the Three.js MIT license from that distribution.

`coffeemat.glb` also contains five embedded KTX2 image payloads; they remain inside this copied GLB.
The current native glTF importer decodes its meshopt geometry but not those images. Diagnostics report
their byte ranges and unsupported status, and the independent glTF test records payload hashes.
Magenta material markers are deliberate missing-format indicators, not replacement comparison assets.

The native `--normal-comparison` fixture reuses the existing normal PNGs without
modifying their bytes. Native image-row orientation and green-channel conversion
are composed through the engine material contract; matched extrusion regions and
deliberately unconverted controls are GPU-tested. See
[normal mapping validation](../../../../../docs/NORMAL_MAPPING_COMPARISON_VALIDATION.md).
