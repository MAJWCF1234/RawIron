---
tags:
  - rawiron
  - engine
  - helpers
  - scenekit
---

# Built-In Helpers

## Direction

RawIron is treating common scene-helper patterns as **built-in engine utilities** instead.

Current identity:

- internal library name: `RawIron.SceneUtilities`
- CMake/public alias: `RawIron::SceneKit`
- plain-English name: `RawIron Scene Kit`

Current built-ins:

- `GridHelper`-style scene helper
- `AxesHelper`-style scene helper
- orbit-camera utility similar in purpose to `OrbitControls`
- scene query utilities for finding and describing nodes
- scene traversal utilities for walking subtrees and typed node collections
- scene raycast utilities for picking and interaction-style examples
- camera-to-ray helpers for viewport-style picking
- Wavefront OBJ model loading into Scene Kit mesh nodes
- five milestone scenario checks covering lit scene setup, orbit navigation, lighting, model loading, and click-style picking

## Why This Is Built In

These helpers show up constantly in prototypes and editor work.

For RawIron, they are important enough to be first-party engine code instead of disposable sample code.

They are not part of `RawIron.Core`.

Current home:

- `Source/RawIron.SceneUtilities`
- `Source/RawIron.SceneSamples`

## Current API Surface

### Helper Builders

- `AddPrimitiveNode`
- `AddLightNode`
- `AddGridHelper`
- `AddAxesHelper`
- `AddOrbitCamera`
- `SetOrbitCameraState`
- `ComputeOrbitCameraStateFromPosition`
- `ComputeFramedOrbitCameraState`
- `FrameNodesWithOrbitCamera`

### Scene Utilities

- `FindNodeByName`
- `FindAncestorByName`
- `CollectRootNodes`
- `CollectNodeSubtree`
- `CollectDescendantNodes`
- `CollectRenderableNodes`
- `CollectCameraNodes`
- `CollectLightNodes`
- `CollectCameraConfinementVolumeNodes`
- `IsWorldPointInsideCameraConfinementVolume`
- `ResolveDominantCameraConfinementVolumeNodeAtWorldPoint`

Note: scene-owned **camera confinement volumes** are authored as `CameraConfinementVolume` assets attached to nodes (`RawIron.Core`); the helpers above evaluate oriented boxes in world space and resolve overlaps by `priority`.

- `DescribeNodePath`
- `ComputeNodeWorldBounds`
- `ComputeCombinedWorldBounds`
- `GetBoundsCenter`
- `GetBoundsSize`
- `RaycastNode`
- `RaycastSceneNearest`
- `RaycastSceneAll`
- `BuildPerspectiveCameraRay`
- `LoadWavefrontObjMesh`
- `AddWavefrontObjNode`
- `BuildLitCubeSceneKitPreview`
- `RunSceneKitMilestoneChecks`
- `AllSceneKitMilestonesPassed`

## Current Use

The starter sample scene is now built from these helpers instead of hand-assembling every mesh, camera, and helper node manually.

That means the demo path is also acting as a first integration test for the library itself.

## Next Likely Growth

- scene clone/merge helpers
- transform utility helpers such as fuller look-at and align-to-plane support
- editor-facing selection and hierarchy utilities
- deeper asset import coverage beyond OBJ
