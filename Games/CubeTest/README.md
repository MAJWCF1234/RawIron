# Cube Test

Cube Test is Raw Iron's walkable native-capability gallery. It validates structural movement and portals,
camera-facing sprite batches, normal-map conventions, compressed glTF import, textured glTF export, and
native Vulkan hybrid HDR through the same engine APIs available to games.

When `RAWIRONX32.ripak` is available at a configured package location, the small center cube mounts it directly and
cycles cooked textures while spinning. In the local RawIron workspace, Cube Test discovers the reference pack at
`O:\Assets\RAWIRONX32.ripak` after the repository's generic `Assets/Textures` library was removed. Software preview
and native Vulkan range-read only this cube's selected package blobs; the test does not extract a loose runtime copy.
Other sample materials may still use optional loose authoring paths until their project package declarations are added.

The cube, platforms, colliders, and portal thresholds are authored through the structural/trace systems with:

- M-mesh: render geometry, material slots, UV tiling, visible shape.
- P-mesh: static simulation shape and physical material metadata.
- Q-mesh: raycast, trace, placement, and interaction metadata.
- I-layer: semantic role, relations, reporting ID, and SSG-style links.

The main cube uses an LRT chiseled-quartz albedo/normal/spec set for normal and shadow readability. Smaller gold,
copper, iron, and **crystal/glass (diamond block)** samples give the renderer a quick specular/roughness/transparency
comparison. A **subtract portal brush** validates structural semantic roles in the render path. Four ring point lights
stress-test hybrid HDR lighting. `levels/assembly.primitives.csv` mirrors the runtime layout for editor round-trip;
`levels/cube-test.primitives.csv` remains a legacy structural reference fixture. The editor registers `cube-test` as a
bundled preview scene.

Cube Test carries the complete Raw Iron project contract and mounts `RuntimeCore`, runtime support data, config
validation, events/services, and the shared plugin bridge before entering the native scene loop. It is intentionally
small, but it is no longer a special-case mini runtime.

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
RawIron.CubeTestGame.exe --game-root=Games\CubeTest --save-preview --output=Saved\visual_checks\cube_test_preview.bmp
RawIron.CubeTestGame.exe --game-root=Games\CubeTest --save-jiggle-preview --jiggle-frames=8 --output=Saved\visual_checks\cube_test_jiggle.bmp
RawIron.CubeTestGame.exe --game-root=Games\CubeTest --benchmark-frames=3
RawIron.CubeTestGame.exe --game-root=Games\CubeTest --jiggle-test
RawIron.CubeTestGame.exe --workspace-root=. --start-room=sprites
RawIron.CubeTestGame.exe --workspace-root=. --start-room=interaction
RawIron.CubeTestGame.exe --workspace-root=. --start-room=projectile
RawIron.CubeTestGame.exe --workspace-root=. --start-room=teleport
RawIron.CubeTestGame.exe --workspace-root=. --export-gltf=Saved\CubeCapabilityGallery.gltf
```

`--export-gltf` is a batch command: it exports, reports warnings, and exits without opening a window.

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
and strong boundary impacts request short runtime haptic pulses.

In the teleport room, hold either thumbstick click to trace and display a parabolic target. Green means the
slope and complete standing volume are valid; red means blocked or too steep. Releasing the click commits a
valid target through the same movement body used by smooth locomotion.

Current XR limitations are explicit: camera-facing sprite expansion, embedded KTX2/Basis textures, packed ORM
texture sampling, general scene-animation streaming outside the interaction field, and authored controller/hand
models are not yet bridged. The
fallback pulsing clear remains available internally only when a headset host supplies no hardware scene.

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
