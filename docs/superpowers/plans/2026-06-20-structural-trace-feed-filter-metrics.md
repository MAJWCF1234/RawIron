# Structural Trace Feed Filter Metrics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Report how many structural brushes entered the trace feed and how many were filtered out before becoming trace colliders.

**Architecture:** `SceneStructuralTraceFeed` already receives the scene and root node while building filtered trace colliders. Count structural brush nodes in the source subtree, then combine that source count with emitted collider counts in `StructuralTraceSceneFeedMetrics`.

**Tech Stack:** C++20, RawIron.SceneUtilities, RawIron.Trace `TraceScene`, CTest smoke tests.

---

### Task 1: Add Source and Filtered Structural Brush Metrics

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneStructuralTraceFeed.h`
- Modify: `Source/RawIron.SceneUtilities/src/SceneStructuralTraceFeed.cpp`
- Modify: `Tests/SceneStructuralTraceFeedSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-20-structural-trace-feed-filter-metrics.md`

- [x] **Step 1: Write the failing smoke test**

Extend `Tests/SceneStructuralTraceFeedSmoke.cpp` to assert:

```cpp
feedResult.metrics.sourceStructuralBrushCount == 3
feedResult.metrics.filteredStructuralBrushCount == 2
```

The fixture has three structural brushes and the default trace feed emits only one traceable blocking collider.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SceneStructuralTraceFeedSmoke
```

Expected: build fails because the new metric fields do not exist.

- [x] **Step 3: Add source/filter metrics**

Add `sourceStructuralBrushCount` and `filteredStructuralBrushCount` to `StructuralTraceSceneFeedMetrics`.

In `SceneStructuralTraceFeed.cpp`, count structural brush nodes in `CollectNodeSubtree(scene, rootNodeHandle, true)` where `node.structuralBrush.brushId` is non-empty. Set `filteredStructuralBrushCount` to `sourceStructuralBrushCount - colliderCount` when the source count is larger.

- [x] **Step 4: Run focused verification**

Run:

```powershell
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke|SceneStructuralTraceFeedSmoke)" --output-on-failure
```

Expected: all four smoke tests pass.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneStructuralTraceFeed.h Source/RawIron.SceneUtilities/src/SceneStructuralTraceFeed.cpp Tests/SceneStructuralTraceFeedSmoke.cpp docs/superpowers/plans/2026-06-20-structural-trace-feed-filter-metrics.md
git commit -m "feat: report structural trace feed filtering"
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

- [x] **Step 2: Push main**

Run:

```powershell
git push origin main
```

Expected: push completes successfully.
