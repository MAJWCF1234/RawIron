# Structural Trace Feed Filter Reasons Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Break structural trace feed filtering metrics down by reason: collision policy, disabled Q-mesh channel, and missing query purpose.

**Architecture:** `SceneStructuralTraceFeed` already counts source and emitted structural brushes. Extend that feed metrics pass to apply the same semantic filter order as `AppendTraceCollidersForSubtree`, producing reason-specific counts without requiring callers to re-run the filtering logic.

**Tech Stack:** C++20, RawIron.SceneUtilities, RawIron.Trace `TraceScene`, CTest smoke tests.

---

### Task 1: Add Reason-Specific Structural Trace Feed Filter Metrics

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneStructuralTraceFeed.h`
- Modify: `Source/RawIron.SceneUtilities/src/SceneStructuralTraceFeed.cpp`
- Modify: `Tests/SceneStructuralTraceFeedSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-20-structural-trace-feed-filter-reasons.md`

- [x] **Step 1: Write the failing smoke test**

Extend `Tests/SceneStructuralTraceFeedSmoke.cpp` with four source structural brushes:

```cpp
TraceableWall          // emitted
PlacementOnlyWall      // filtered by missing Trace purpose
QueryCollisionWall     // filtered by collision policy
DisabledQueryWall      // filtered by disabled Q-mesh channel
```

Assert:

```cpp
feedResult.metrics.sourceStructuralBrushCount == 4
feedResult.metrics.filteredStructuralBrushCount == 3
feedResult.metrics.collisionPolicyFilteredCount == 1
feedResult.metrics.queryChannelFilteredCount == 1
feedResult.metrics.queryPurposeFilteredCount == 1
```

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SceneStructuralTraceFeedSmoke
```

Expected: build fails because the new reason-specific metric fields do not exist.

- [x] **Step 3: Add reason metrics**

Add `collisionPolicyFilteredCount`, `queryChannelFilteredCount`, and `queryPurposeFilteredCount` to `StructuralTraceSceneFeedMetrics`.

In `SceneStructuralTraceFeed.cpp`, count source structural brushes in the subtree and classify filtered brushes using this order:

1. `respectStructuralBrushCollisionPolicy` with `None`, `Query`, or `Detail`
2. `requireStructuralBrushQueryMeshChannel` with disabled `StructuralBrushChannel::QueryMesh`
3. `requiredStructuralBrushQueryPurpose` with unsupported purpose

- [x] **Step 4: Run focused verification**

Run:

```powershell
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke|SceneStructuralTraceFeedSmoke)" --output-on-failure
```

Expected: all four smoke tests pass.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneStructuralTraceFeed.h Source/RawIron.SceneUtilities/src/SceneStructuralTraceFeed.cpp Tests/SceneStructuralTraceFeedSmoke.cpp docs/superpowers/plans/2026-06-20-structural-trace-feed-filter-reasons.md
git commit -m "feat: report structural trace feed filter reasons"
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
