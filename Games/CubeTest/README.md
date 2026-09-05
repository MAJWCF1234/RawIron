# Cube Test

Cube Test is Raw Iron's walkable native-capability gallery. It validates structural movement and portals,
camera-facing sprite batches, normal-map conventions, compressed glTF import, textured glTF export, and
native Vulkan hybrid HDR through the same engine APIs available to games.

Comparison assets are physical copies inside `assets/reference/threejs-r185`, with licenses and a
SHA-256 manifest. Cube Test does not need the original Three.js checkout. Its baseline cube uses
the original hardwood image; the four PBR samples use the original UV grid with authored tint,
metalness, roughness, and opacity variations. They no longer use the unrelated LRT texture pack.

`--cooked-texture-demo` explicitly opts into the separate `RAWIRONX32.ripak` streaming test.
Only that option searches the configured workspace/adjacent package locations and replaces the
center cube's reference texture. Ordinary comparison runs never mount it automatically.
The project-owned reference files still need cooked project-package declarations.

Missing or corrupt required reference images now stop Cube Test with the exact path. A shared scene
audit and native upload diagnostics report source, dimensions, color space and sampler/fallback policy.
Coffee's embedded KTX2 images are currently unsupported: its geometry imports, but explicit magenta
material markers and logs identify the missing image support. Do not treat it as a material-parity fixture yet.

The cube, platforms, colliders, and portal thresholds are authored through the structural/trace systems with:

- M-mesh: render geometry, material slots, UV tiling, visible shape.
- P-mesh: static simulation shape and physical material metadata.
- Q-mesh: raycast, trace, placement, and interaction metadata.
- I-layer: semantic role, relations, reporting ID, and SSG-style links.

The main cube uses the Three.js hardwood albedo. Smaller gold-, copper-, iron-, and glass-tinted UV-grid
samples give the renderer a quick specular/roughness/transparency
comparison. A **subtract portal brush** validates structural semantic roles in the render path. Four ring point lights
stress-test hybrid HDR lighting. `levels/assembly.primitives.csv` mirrors the runtime layout for editor round-trip;
`levels/cube-test.primitives.csv` remains a legacy structural reference fixture. The editor registers `cube-test` as a
bundled preview scene.

Cube Test carries the complete Raw Iron project contract and mounts `RuntimeCore`, runtime support data, config
validation, events/services, and the shared plugin bridge before entering the native scene loop. It is intentionally
small, but it is no longer a special-case mini runtime.

The project is the first connected zone set in the [native Three.js-reference showcase roadmap](../../docs/THREEJS_NATIVE_SHOWCASE_ROADMAP.md).
That roadmap and its [execution tracker](../../docs/THREEJS_NATIVE_SHOWCASE_TODOS.md) require native C++ engine
features, recorded asset provenance, hardware visual validation, and shared desktop/PCVR proof; these zones are not
considered finished merely because a reference scene opens.

Six bidirectional portal links connect seven areas: baseline materials, the 512-sprite native billboard
batch, OpenGL/DirectX normal conventions, the glTF import/export room, and a bounded interactive-prop
field, a pooled projectile and knock-down target room, and a trace-validated parabolic teleport room.
The gallery uses the exact
Three.js distribution assets documented under `assets/reference/threejs-r185`, but no Three.js JavaScript
or implementation code. The compressed coffee fixture exercises `KHR_mesh_quantization` and
`EXT_meshopt_compression`; exported folders copy and reference their external textures. The interaction
rooms are a native Raw Iron synthesis of the controller-cubes, dragging, haptics, ball-shooter, and teleport example behaviors;
those source examples do not require content assets to copy.

Desktop controls follow the Half-Life 2 convention: `WASD` moves, the mouse always looks while the
window is focused, `Shift` sprints, `Space` jumps, and holding `E` on an interaction-room prop carries it;
release `E` to throw. Left/right mouse are reserved for primary/secondary tool actions. `Home` resets the
view; `T` executes the currently valid parabolic target inside the teleport room, and `Esc` quits the current
harness. Movement, gravity, grounding, and jump forgiveness use the engine
`TraceScene`/`MovementController` path against the authored platform and query collider.

Run from the build output with:

```powershell
RawIron.CubeTestGame.exe --game-root=Games\CubeTest
```

Useful verification:

```powershell
RawIron.CubeTestGame.exe --gallery-help
RawIron.CubeTestGame.exe --game-root=Games\CubeTest --save-preview --output=Saved\visual_checks\cube_test_preview.bmp
RawIron.CubeTestGame.exe --game-root=Games\CubeTest --save-jiggle-preview --jiggle-frames=8 --output=Saved\visual_checks\cube_test_jiggle.bmp
RawIron.CubeTestGame.exe --game-root=Games\CubeTest --benchmark-frames=3
RawIron.CubeTestGame.exe --game-root=Games\CubeTest --jiggle-test
RawIron.CubeTestGame.exe --workspace-root=. --material-calibration --benchmark-frames=8 --background
RawIron.CubeTestGame.exe --workspace-root=. --material-calibration --save-preview --output=Saved\visual_checks\material-calibration-software.bmp
RawIron.CubeTestGame.exe --workspace-root=. --material-calibration --background --capture-native=Saved\visual_checks\material-calibration-gpu.bmp
RawIron.CubeTestGame.exe --workspace-root=. --start-room=sprites
RawIron.CubeTestGame.exe --workspace-root=. --start-room=interaction
RawIron.CubeTestGame.exe --workspace-root=. --start-room=projectile
RawIron.CubeTestGame.exe --workspace-root=. --start-room=teleport
RawIron.CubeTestGame.exe --workspace-root=. --export-gltf=Saved\CubeCapabilityGallery.gltf
```

Press **F1** in play for the current room's subsystem, source fixtures, controls, expected observation,
and outgoing portal labels. The window title identifies the room and nearby portal destination.
`--gallery-help` prints all seven guides without opening a window; unknown `--start-room` IDs are rejected.
See [gallery validation](../../docs/CUBE_TEST_GALLERY_VALIDATION.md) for portal clearance/traversal tests,
independent glTF byte checks, texture diagnostics and current rendering limitations.

`--export-gltf` is a batch command: it exports, reports warnings, and exits without opening a window.

`--material-calibration` uses a fixed camera, neutral presentation settings, the copied Three.js
hardwood and OpenGL/DirectX normal textures, unlit color controls, PBR variations, overlapping
depth/blend panels, and a shadow caster/receiver. `Esc` exits; movement and animation are disabled.
Missing or undecodable reference textures stop this mode with their project paths in the error.
It rejects jiggle, cooked-streaming, nonbaseline start rooms, extended post effects, and network modes.
See [material calibration validation](../../docs/MATERIAL_CALIBRATION_VALIDATION.md) for exact build,
test, and hardware review instructions. A `--save-preview` image is software output, not GPU evidence.

`--capture-native=<path.bmp>` saves actual first-frame GPU pixels, including when `--background`
hides the window. It defaults to exiting after one frame, or follows an explicit `--benchmark-frames`
count. It rejects software-preview/export combinations and unsupported swapchain readback formats.
The readback stalls the GPU and should not be used for performance measurements. From the repository
root, `Scripts/Test-MaterialCalibration.ps1` captures two independent launches, checks the calibration
regressions, and saves logs, executable/shader/image hashes, driver details, and results in JSON.
Build `VulkanNativeValidationProbe` and add `-IncludeGallery` to that script for dark-color luma-curve
checks in the direct/lightweight hybrid paths and normal-room sky/floor checks. The separate
`-IncludeExtendedPost` switch also exercises the extended shader, whose driver compilation can
consume substantial time and memory. These checks are not a full gallery visual sign-off.

## OpenXR / SteamVR host

`RawIron.VRShowcase.exe` is a separate host over this same world graph. The engine uses the official pinned
Khronos OpenXR 1.1 loader while requesting the compatible OpenXR 1.0 core, requires
`XR_KHR_vulkan_enable2`, queries the runtime's Vulkan API limits, and creates Raw Iron's two-hand action
schema with Valve Index, Oculus Touch, and Khronos simple-controller bindings.

The live bridge creates Vulkan through OpenXR, selects the runtime's physical device, starts a session,
allocates a two-layer stereo swapchain, locates both eye views, synchronizes actions, and renders the shared
Cube Test scene as hardware geometry with depth and a GPU-resident shared material atlas. The stereo forward pass
uses the authored world-space normals, base color, metalness, roughness, and tangent-space normal maps; it also
uses each eye's real camera position for specular response. The normals room contains the original Lee Perry-Smith
geometry and maps from `webgl_materials_normalmap.html` as a cross-engine fidelity fixture. Cyan and orange
tracked proxies expose the left and right controller aim poses. The default layout follows VRChat's Oculus Touch
roles: left stick locomotes, right stick turns smoothly, either grip picks up props, trigger selects/interacts,
and the right `A` button jumps. Locomotion uses Cube Test's existing
`TraceScene` / movement-controller collision path and portal traveler; the right stick performs latched 30-degree
comfort turns only when a host explicitly selects snap mode. Either controller grip selects the nearest
interaction-room prop along its tracked aim ray; hold to move it and
release to throw it. Prop transforms stream through a bounded coherent dynamic vertex buffer, while acquisition
and explicit trigger selection request short runtime haptic pulses. Generic hover, rendering,
projectile fire without a selected prop, and unqualified physics impacts do not vibrate controllers.
The XR engine requires focus and tracked input, caps amplitude at 0.35 and duration at 50 ms,
and allows at most one pulse per hand every 100 ms.

In the teleport room, hold either thumbstick click to trace and display a parabolic target. Green means the
slope and complete standing volume are valid; red means blocked or too steep. Releasing the click commits a
valid target through the same movement body used by smooth locomotion.

Current XR limitations are explicit: camera-facing sprite expansion, embedded KTX2/Basis textures, packed ORM
texture sampling, general scene-animation streaming outside the interaction field, and authored controller/hand
models are not yet bridged. The
fallback pulsing clear remains available internally only when a headset host supplies no hardware scene.

Shared implementation ownership: `RawIron.World` owns ray grab/throw and bounded two-pool authority
serialization; `RawIron.Runtime` owns raw desktop mouse capture; `RawIron.Trace` owns keyboard
movement and collision; `RawIron.XR.OpenXR` owns material-atlas/mesh conversion and the haptic gate.
The experience supplies assets, room layout, pool authoring and host bindings. Both network clients
display received prop state without independently advancing its physics; remote grabbing remains
disabled until an authoritative grab protocol is implemented. All static rooms are resident in XR,
so portal travel cannot leave the startup radius and enter missing geometry.

## Repeatable room benchmark

The gallery now has ten rooms. `--start-room=lathe`, `--start-room=tubes`, and
`--start-room=surfaces` select the new structural geometry platforms in either
desktop or VR. Their nine exhibits use the existing `StructuralPrimitiveBundle`
path: enhanced `revolve`, `spline_sweep`, and `torus`, plus cataloged `mobius` and
`parametric_patch`. F1 describes each reference and expected observation; the
eighteen portal routes connect all rooms in both directions. See
[structural surface scope and validation](../../docs/STRUCTURAL_SURFACE_SHOWCASE_VALIDATION.md).

From the repository root run `Scripts/Benchmark-CubeTestRooms.ps1`. It launches every room twice,
discards 30 warmup present intervals, measures 120 intervals, and writes raw CSV, JSON provenance
and a readable Markdown table under `Saved/benchmarks/cube-test`. `--frame-times=<csv>` exposes the
engine present-timing callback for other bounded runs. This measures CPU wall time between successful
Vulkan presents, not GPU execution time or physical-display FPS. Hidden 1280×720 direct rendering,
no readback, normal room animation/physics; no physical-headset performance claim.

Home resets position, velocity and view to the selected start room; F2 writes a control-state
diagnostic to the log. See [shared-host validation](../../docs/CUBE_TEST_SHARED_HOST_VALIDATION.md).

Use the system-wide active OpenXR runtime by default. To test SteamVR without replacing an Oculus system
default, use the per-process override:

```powershell
RawIron.VRShowcase.exe --steamvr --frames=300
RawIron.VRShowcase.exe --steamvr --start-room=interaction --frames=300
RawIron.VRShowcase.exe --steamvr --start-room=projectile --frames=300
RawIron.VRShowcase.exe --steamvr --start-room=teleport --frames=300
RawIron.VRShowcase.exe --steamvr --probe-only
```

On a machine without a connected HMD the executable exits with a specific diagnostic. The report distinguishes
session focus, bound input sources, active pose actions, valid tracked poses, select edges, dynamic updates,
and accepted haptic pulses.

## Neutral normal-map comparison

Run with `--normal-comparison` for six native panels using the experience-owned
Three.js normal maps. Columns are OpenGL, converted DirectX, and deliberately
unconverted DirectX. The lower row mirrors U. The same engine fixture is present
in the normal gallery room; F1 describes it. The first two columns should agree
on bump orientation, while the third is a negative control.

Use `--background --capture-native=<absolute.bmp>` for a repeatable GPU capture.
See [normal mapping validation](../../docs/NORMAL_MAPPING_COMPARISON_VALIDATION.md)
for engine ownership, regression evidence and remaining release gates.
