# Structural showcase expansion — 2026-09-08

## Scope and ownership

Eight new Cube Test rooms add sixteen exhibits to the existing nine structural exhibits.
The gallery now has eighteen rooms and 34 directed portal routes. Every mesh is spawned through
`SpawnStructuralPrimitiveBundle`; the game owns parameters, scene placement and materials only.

| Room ID | Exhibits | Engine owner / reference relationship |
|---|---|---|
| `knots` | (2,3) trefoil; (3,5) knot | New `RawIron.Structural::torus_knot`; `webgl_geometries.html` |
| `helices` | Three and five turns | New `helix` atop the existing transported tube; native extension of spline sweeps |
| `lathe-poles` | Spindle; pointed vessel | Enhanced `revolve`, nondegenerate axis fans; `webgl_geometries.html` |
| `extrusion` | Concave L; concave cross | Fixed `extrude_along_normal_primitive`, validated ear clipping; `webgl_geometry_extrude_shapes.html` |
| `rounded` | Sharp and soft rounding | Existing `rounded_box` superellipsoid approximation; native geometry extension |
| `superellipsoids` | Box-like; anisotropic pinched | Existing three-axis exponent primitive; native geometry extension |
| `hulls` | Octahedron; asymmetric ten-point hull | Existing convex-solid compiler; `webgl_geometry_convex.html` |
| `heightfields` | 8 and 48 cells per axis | Existing native ridge heightfield; tessellation comparison related to `webgl_geometry_terrain.html` |

All use the original `uv_grid_opengl.jpg` already copied into
`Games/CubeTest/assets/reference/threejs-r185/textures`, covered by the existing licence and SHA-256
manifest. No JavaScript runtime, borrowed implementation or new third-party library was added.
`O:/three.js-master/three.js-master` was used only to inspect example references; runtime does not depend on it.
These are eight demo rooms, not eight newly completed upstream examples.

## Geometry fixes

- Knots use bounded coprime windings, deterministic samples, transported section frames and the existing
  closed-tube seam correction. Helices expose radius, turns, axial length and separate cap normals.
- Smooth lathe profiles can now touch the axis at either endpoint. Each pole band omits its collapsed
  triangle, while preserving the UV seam. Interior axis crossings and non-increasing heights still fail.
- Extrusion previously fanned both caps around the world origin, filling space outside translated or
  concave profiles. Simple planar polygons now receive checked ear-clipped caps, consistent winding,
  normalized cap UVs and arc-length side UVs. Clockwise input and an explicit closing point are accepted.
- New shape parameters pass through presets, overrides, structural graph adapters, deferred compilation
  and compile signatures. No second primitive dispatcher was introduced in the experience.

## Verification

The initial failing test recorded missing curves, rejected axis endpoints, invalid extrusion bounds
and missing extrusion UVs in `Saved/eight-demos-reproduction.log`. The expanded geometry regression
then passed against current source, including cap area 3 and signed volume 1.5 for a translated L,
nondegenerate triangles, unit normals, winding, seam closure, invalid inputs, memory bounds and determinism.

The old `build/dev-msvc` cache references `E:/RawIron` and an unavailable Build Tools compiler.
A separate build was configured with Visual Studio 2022 Community:

```powershell
cmake --preset dev-msvc -B build/demo-expansion-msvc "-DCMAKE_GENERATOR_INSTANCE=C:/Program Files/Microsoft Visual Studio/2022/Community"
cmake --build build/demo-expansion-msvc --config RelWithDebInfo -j 8
ctest --test-dir build/demo-expansion-msvc -C RelWithDebInfo --output-on-failure
Scripts/Test-StructuralShowcase.ps1 -BuildDirectory build/demo-expansion-msvc
```

Full-build, integration and hardware results are pending in this working record.

## Remaining gates

Visual/performance superiority to Three.js has **not** been established. No matched upstream GPU
benchmark or physical-headset run is included. Desktop and VR share geometry and portal definitions,
but a desktop capture does not certify stereo comfort, haptics or motion performance.

Rounded boxes remain approximations, not exact fillets. Extrusion does not support holes or bevels.
Knots and helices do not automatically solve self-intersection clearance. The heightfield uses the
engine's ridge, not the reference terrain-noise function. Hulls use hard face normals; legacy rounded,
superellipsoid and heightfield primitives retain their existing shading/UV-generation policy.
Exhibit collision is conservative AABB collision, not exact concave triangle collision.
