# Cube Test

Cube Test is a tiny RawIron game used to validate walking around a single fully mapped cube on a structural platform.

The cube and platform are authored as structural primitives with:

- M-mesh: render geometry, material slots, UV tiling, visible shape.
- P-mesh: static simulation shape and physical material metadata.
- Q-mesh: raycast, trace, placement, and interaction metadata.
- I-layer: semantic role, relations, reporting ID, and SSG-style links.

Run from the build output with:

```powershell
RawIron.CubeTestGame.exe --game-root=Games\CubeTest
```

Useful verification:

```powershell
RawIron.CubeTestGame.exe --game-root=Games\CubeTest --save-preview --output=Saved\visual_checks\cube_test_preview.bmp
RawIron.CubeTestGame.exe --game-root=Games\CubeTest --benchmark-frames=3
```
