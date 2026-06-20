# Structural Trace Feed Efficiency Ratios Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add compact feed efficiency ratios so callers can quickly see how much structural trace filtering helped.

**Architecture:** `StructuralTraceSceneFeedMetrics` already stores source, emitted, and filtered counts. Add derived ratios during feed metric computation, keeping them immutable snapshots of the feed build.

**Tech Stack:** C++20, RawIron.SceneUtilities, RawIron.Trace `TraceScene`, CTest smoke tests.

---

### Task 1: Add Emitted and Filtered Structural Brush Ratios

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneStructuralTraceFeed.h`
- Modify: `Source/RawIron.SceneUtilities/src/SceneStructuralTraceFeed.cpp`
- Modify: `Tests/SceneStructuralTraceFeedSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-20-structural-trace-feed-efficiency-ratios.md`

- [x] **Step 1: Write the failing smoke test**

Extend `Tests/SceneStructuralTraceFeedSmoke.cpp` to assert:

```cpp
feedResult.metrics.emittedStructuralBrushRatio == 0.25f
feedResult.metrics.filteredStructuralBrushRatio == 0.75f
```

The fixture has four source structural brushes and one emitted trace collider.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SceneStructuralTraceFeedSmoke
```

Expected: build fails because the new ratio fields do not exist.

- [x] **Step 3: Add ratio fields and computation**

Add `float emittedStructuralBrushRatio` and `float filteredStructuralBrushRatio` to `StructuralTraceSceneFeedMetrics`.

In `ComputeStructuralTraceSceneFeedMetrics`, when `sourceStructuralBrushCount > 0`, set:

```cpp
emittedStructuralBrushRatio = colliderCount / sourceStructuralBrushCount
filteredStructuralBrushRatio = filteredStructuralBrushCount / sourceStructuralBrushCount
```

- [x] **Step 4: Run focused verification**

Run:

```powershell
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke|SceneStructuralTraceFeedSmoke)" --output-on-failure
```

Expected: all four smoke tests pass.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneStructuralTraceFeed.h Source/RawIron.SceneUtilities/src/SceneStructuralTraceFeed.cpp Tests/SceneStructuralTraceFeedSmoke.cpp docs/superpowers/plans/2026-06-20-structural-trace-feed-efficiency-ratios.md
git commit -m "feat: report structural trace feed efficiency ratios"
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
