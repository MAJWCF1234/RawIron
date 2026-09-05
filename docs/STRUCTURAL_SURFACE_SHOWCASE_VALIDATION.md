# Structural surface showcase

## Ownership and scope

This expansion enhances the existing structural primitive collection. CubeTest
authors `StructuralPrimitiveOptions` and calls `SpawnStructuralPrimitiveBundle`;
it does not create a second primitive registry or run JavaScript.

The shared path is:

`StructuralPrimitivePresets -> StructuralPrimitiveBundle -> StructuralBrush ->
RawIron.Structural::BuildPrimitiveMesh -> CompiledMesh -> scene Mesh`

`ProceduralSurfaceMesh.*` is a **private implementation** inside
`RawIron.Structural`. There is no public SceneUtilities procedural primitive API.
Smooth normals and authored UVs now survive structural mesh conversion and graph
transforms. Existing CSG output without UVs retains planar projection.

## Collection changes

| Structural type | Change |
|---|---|
| `revolve` | `closedProfile=false` adds an open profile with increasing heights, positive radii, smooth normals, arc-length V and exact full-revolution seam. Default closed-profile behavior is retained. |
| `spline_sweep` | Replaces independent box beams with a continuous circular Catmull-Rom sweep, parallel-transport frames, arc-length U, optional hard-normal caps and closed-loop twist correction. |
| `torus` | Full revolutions now have smooth normals and two periodic UV seams. Existing partial-arc behavior is retained. |
| `mobius` | New non-orientable ribbon in the same structural dispatcher; bundle materials are double sided. |
| `parametric_patch` | New sampled surface lattice, indexed by `u*(cellsY+1)+v` in `vertices`; empty input selects the unit saddle preset. |

New catalog entries: `revolve_open`, `spline_loop`, `mobius`,
`parametric_patch`. These are available to the existing structural catalog gallery,
not only CubeTest. Graph nodes preserve closure/cap settings and path resolution;
incremental signatures include the new settings.

Surface tessellation rejects degenerate or oversized geometry. The private indexed
working mesh is bounded to 1,048,576 vertices before conversion into the compiler's
existing triangle soup. Invalid structural surface generation returns empty output
and `ValidateStructuralPrimitive` reports failure; it does not substitute a cube.
Sweeps do not solve self-intersection. Open profiles, ribbons and patches are
mesh primitives, not convex CSG solids. Existing assembly CSV supports catalog
preset selection; it is not a serialization format for arbitrary sampled lattices.

## Three.js references and platforms

Sources inspected locally from Three.js 0.185.1:
`webgl_geometries.html` and `webgl_geometry_extrude_splines.html`.
The revolve comparison uses the geometry example's mathematical profile converted
to meters. Tubes compare the example's open/closed spline extrusion behavior using
authored native control points; this does not reproduce its animated ride camera.
The saddle demonstrates the new sampled surface capability.

| Room ID | Center X | Exhibits |
|---|---:|---|
| `lathe` | 182 | 20- and 96-segment vessels; partial revolve |
| `tubes` | 208 | Capped open spline; 6- and 24-sided closed loops |
| `surfaces` | 234 | Torus, Mobius ribbon, saddle |

All nine exhibits use the already-local
`Games/CubeTest/assets/reference/threejs-r185/textures/uv_grid_opengl.jpg`.
No additional dependency, JavaScript implementation, or upstream path is required.
No new third-party code was introduced. The upstream checkout remains disposable.

The shared room guide now authors ten rooms. Platforms, bidirectional gates,
eighteen portal routes and room-start lookup derive from that list. Desktop and VR
use the same arrival positions. Teleport input is restricted to the teleport room,
so adding later rooms does not accidentally enable it there.

## Verification

Validated 2026-08-27 with the MSVC `RelWithDebInfo` build. Desktop executable SHA-256:
`D517A634BE0DB1023DB7E6C686A4156E679F40E544354E46DD1A472D6F101253`.

- Build: `Saved/structural-showcase-final-build.log`, followed by the CLI help
  refresh in `Saved/structural-showcase-help-build.log`. Tests and captures were
  repeated after the help refresh.
- **16/16 targeted CTests passed**, recorded in `Saved/structural-showcase-tests.log`.
  These cover private tessellation math (winding, finite/unit normals, seams, caps,
  Catmull-Rom interpolation, bounds/budgets and invalid inputs), public structural
  dispatch, sampled lattice validation, preset/graph/brush integration and UV
  preservation, XR upload streams, gallery contracts, authority, calibration,
  compressed import/export and existing CSG/model-bake regressions.
- Independent glTF checks now require exact index counts and double-sided
  materials for all nine exhibits, plus all eight supported local image hashes.
- Direct native Vulkan 1280x720 captures, exact arguments, image/executable hashes
  and driver information: `Saved/visual_checks/structural-surfaces/report.json`.
  Images: `lathe.bmp`, `tubes.bmp`, `surfaces.bmp` in that directory.
- **10/10 existing GPU calibration checks passed**:
  `Saved/visual_checks/calibration/20260827-150728-767/report.json`.
- OpenXR surface upload tests passed without requiring a headset. The actual
  `--start-room=surfaces --probe-only` reports an active OpenXR runtime but no HMD:
  `Saved/structural-showcase-vr-probe.log`. Physical stereo rendering, movement and
  comfort remain unvalidated.
- **20/20 benchmark runs completed** (ten rooms, two launches each, 30 warmup
  and 120 measured intervals):
  `Saved/benchmarks/cube-test/20260827-150459-697/report.json` and `report.md`.
  The new rooms measured mean CPU present intervals of 16.36–16.65 ms and
  P95 intervals of 32.96–33.42 ms. This is not GPU execution time, display FPS,
  headset performance or a Three.js comparison. This benchmark used executable
  `E497712E80A22D91B8BDA03D6DCED5C2984740551AB02E7BD7593D35CAC1F3B6`, before
  the final CLI help-only refresh; engine, world and shaders were unchanged.

Visual inspection confirms all nine exhibits, UV mapping and distinct coarse/fine
silhouettes. Shadow-side material readability is too dark and shadow edges remain
coarse; lighting fidelity is **open**. Physical headset validation and any claim of
visual/performance superiority to Three.js require separate measurements.
