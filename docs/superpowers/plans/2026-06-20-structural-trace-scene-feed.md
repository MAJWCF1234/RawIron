# Structural Trace Scene Feed Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a ready-to-query `ri::trace::TraceScene` directly from structural primitives using the semantic trace collider feed defaults.

**Architecture:** `SceneStructuralTraceFeed` already builds filtered structural trace colliders. Add thin helper overloads that construct `ri::trace::TraceScene` from those colliders, optionally accepting `ri::spatial::SpatialIndexOptions` for BSP tuning.

**Tech Stack:** C++20, RawIron.SceneUtilities, RawIron.Trace `TraceScene`, CTest smoke tests.

---

### Task 1: Add Structural Trace Scene Builder

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneStructuralTraceFeed.h`
- Modify: `Source/RawIron.SceneUtilities/src/SceneStructuralTraceFeed.cpp`
- Modify: `Tests/SceneStructuralTraceFeedSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-20-structural-trace-scene-feed.md`

- [x] **Step 1: Write the failing smoke test**

Extend `Tests/SceneStructuralTraceFeedSmoke.cpp` to call:

```cpp
ri::trace::TraceScene traceScene =
    ri::scene::BuildStructuralTraceSceneForSubtree(scene, root);
```

Assert `traceScene.Metrics().colliderCount == 1`, `structuralStaticColliderCount == 1`, and a structural-only ray from `{0, 0, -3}` toward `{0, 0, 1}` hits `"TraceableWall"`.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SceneStructuralTraceFeedSmoke
```

Expected: build fails because `BuildStructuralTraceSceneForSubtree` does not exist.

- [x] **Step 3: Add trace scene builder overloads**

Declare and implement:

```cpp
[[nodiscard]] ri::trace::TraceScene BuildStructuralTraceSceneForSubtree(
    const Scene& scene,
    int rootNodeHandle,
    ri::spatial::SpatialIndexOptions indexOptions = {});
[[nodiscard]] ri::trace::TraceScene BuildStructuralTraceSceneForSubtree(
    const Scene& scene,
    int rootNodeHandle,
    const SubtreeColliderBuildOptions& options,
    ri::spatial::SpatialIndexOptions indexOptions = {});
```

Implementation should build colliders with `BuildStructuralTraceCollidersForSubtree(...)` and pass them into `ri::trace::TraceScene`.

- [x] **Step 4: Run focused verification**

Run:

```powershell
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke|SceneStructuralTraceFeedSmoke)" --output-on-failure
```

Expected: all four smoke tests pass.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneStructuralTraceFeed.h Source/RawIron.SceneUtilities/src/SceneStructuralTraceFeed.cpp Tests/SceneStructuralTraceFeedSmoke.cpp docs/superpowers/plans/2026-06-20-structural-trace-scene-feed.md
git commit -m "feat: build trace scene from structural feed"
```

### Task 2: Push Verified Work

**Files:**
- No source files.

- [x] **Step 1: Verify focused smoke suite**

Run:

```powershell
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke|SceneStructuralTraceFeedSmoke)" --output-on-failure
```

Expected: all four smoke tests pass.

- [ ] **Step 2: Push main**

Run:

```powershell
git push origin main
```

Expected: push completes successfully.
