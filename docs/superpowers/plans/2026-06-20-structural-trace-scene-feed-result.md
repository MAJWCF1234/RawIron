# Structural Trace Scene Feed Result Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Return a ready structural `ri::trace::TraceScene` together with feed metrics so callers can verify how much structural data entered the trace acceleration structure.

**Architecture:** `SceneStructuralTraceFeed` already builds filtered structural trace colliders and can build a `TraceScene`. Add a result wrapper that preserves build-time counts while still returning the ready-to-query trace scene.

**Tech Stack:** C++20, RawIron.SceneUtilities, RawIron.Trace `TraceScene`, CTest smoke tests.

---

### Task 1: Add Structural Trace Scene Feed Result

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneStructuralTraceFeed.h`
- Modify: `Source/RawIron.SceneUtilities/src/SceneStructuralTraceFeed.cpp`
- Modify: `Tests/SceneStructuralTraceFeedSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-20-structural-trace-scene-feed-result.md`

- [x] **Step 1: Write the failing smoke test**

Extend `Tests/SceneStructuralTraceFeedSmoke.cpp` to call:

```cpp
ri::scene::StructuralTraceSceneFeedResult feedResult =
    ri::scene::BuildStructuralTraceSceneFeedForSubtree(scene, root);
```

Assert `feedResult.metrics.colliderCount == 1`, `feedResult.metrics.structuralStaticColliderCount == 1`, and `feedResult.traceScene.TraceRay(...)` hits `"TraceableWall"`.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SceneStructuralTraceFeedSmoke
```

Expected: build fails because `StructuralTraceSceneFeedResult` and `BuildStructuralTraceSceneFeedForSubtree` do not exist.

- [x] **Step 3: Add result types and overloads**

Declare and implement:

```cpp
struct StructuralTraceSceneFeedMetrics {
    std::size_t colliderCount = 0;
    std::size_t staticColliderCount = 0;
    std::size_t structuralStaticColliderCount = 0;
    std::size_t dynamicColliderCount = 0;
};

struct StructuralTraceSceneFeedResult {
    ri::trace::TraceScene traceScene{};
    StructuralTraceSceneFeedMetrics metrics{};
};
```

Add two overloads named `BuildStructuralTraceSceneFeedForSubtree`, one using default structural trace feed options and one accepting `SubtreeColliderBuildOptions`.

- [x] **Step 4: Run focused verification**

Run:

```powershell
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke|SceneStructuralTraceFeedSmoke)" --output-on-failure
```

Expected: all four smoke tests pass.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneStructuralTraceFeed.h Source/RawIron.SceneUtilities/src/SceneStructuralTraceFeed.cpp Tests/SceneStructuralTraceFeedSmoke.cpp docs/superpowers/plans/2026-06-20-structural-trace-scene-feed-result.md
git commit -m "feat: report structural trace scene feed metrics"
```

### Task 2: Push Verified Work

**Files:**
- No source files.

- [ ] **Step 1: Verify focused smoke suite**

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
