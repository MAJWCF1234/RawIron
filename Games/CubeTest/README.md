# Cube Test

Cube Test is a tiny RawIron game used to validate walking around fully mapped material samples on a structural platform, with **native Vulkan hybrid HDR** as the default interactive renderer.

The cube and platform are authored as structural primitives with:

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

Controls are `WASD` to move, `Shift` to sprint, `Space` to jump, right-mouse drag or arrow keys to
look, and `Esc` to quit. Movement, gravity, grounding, and jump forgiveness use the engine
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
```
