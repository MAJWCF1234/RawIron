# Three.js reference assets

These assets come from the Three.js `0.185.1` example distribution and are retained only as
cross-engine compatibility fixtures. Raw Iron does not include or execute Three.js source code.

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

Behavior-only references with no copied JavaScript or required content assets:

- `webxr_xr_cubes.html`: bounded dynamic cube field
- `webxr_xr_dragging.html`: ray selection and exclusive two-hand grab ownership
- `webxr_xr_haptics.html`: collision-strength haptic feedback
- `webxr_xr_ballshooter.html`: bounded projectile pooling and impulse-driven targets
- `webxr_vr_teleport.html`: parabolic targeting, slope rejection, and standing-volume clearance

These behaviors are implemented through `RawIron.World` simulation and the generic
`RawIron.XR.OpenXR` interaction-frame contract. They are not ports of Three.js internals.

The accompanying `LICENSE.txt` is the Three.js MIT license from that distribution.
