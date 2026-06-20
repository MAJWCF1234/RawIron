# Semantic Partition Query Purpose Metrics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add semantic partition metrics that report how many structural primitives support each Q-mesh purpose: raycast, trace/ballistics, placement, and interaction.

**Architecture:** `RawIron.Core` already exposes `StructuralBrushSupportsQueryPurpose`. `RawIron.SceneUtilities` should reuse that helper while rebuilding semantic partition side tables, storing counts alongside existing role, operation, rebuild-scope, and M/P/Q/I channel metrics.

**Tech Stack:** C++20, RawIron.SceneUtilities semantic structural partition, RawIron.Core structural brush metadata, CTest smoke tests.

---

### Task 1: Add Query Purpose Counts to Semantic Partition Metrics

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h`
- Modify: `Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp`
- Modify: `Tests/SemanticStructuralPartitionSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-20-semantic-partition-query-purpose-metrics.md`

- [x] **Step 1: Write the failing metrics test**

Extend `Tests/SemanticStructuralPartitionSmoke.cpp` to assert:

```cpp
metrics.queryPurposeCounts.raycast == 2
metrics.queryPurposeCounts.trace == 1
metrics.queryPurposeCounts.placement == 1
metrics.queryPurposeCounts.interaction == 1
```

The existing fixture has `wall_a` trace/raycast enabled, `floor_a` raycast/placement/interaction enabled, and `wall_b` query disabled.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
```

Expected: build fails because `SemanticStructuralPartitionMetrics::queryPurposeCounts` does not exist.

- [x] **Step 3: Add metrics storage and side-table counting**

Add this struct in `SemanticStructuralPartition.h`:

```cpp
struct SemanticStructuralPartitionQueryPurposeCounts {
    std::size_t raycast = 0;
    std::size_t trace = 0;
    std::size_t placement = 0;
    std::size_t interaction = 0;
};
```

Add `SemanticStructuralPartitionQueryPurposeCounts queryPurposeCounts{};` to `SemanticStructuralPartitionMetrics`.

In `SemanticStructuralPartition.cpp`, add an `IncrementQueryPurposeCounts` helper that calls `StructuralBrushSupportsQueryPurpose` for each purpose, then call it from `RebuildSideTables`.

- [x] **Step 4: Run focused verification**

Run:

```powershell
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke)" --output-on-failure
```

Expected: all three smoke tests pass.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp Tests/SemanticStructuralPartitionSmoke.cpp docs/superpowers/plans/2026-06-20-semantic-partition-query-purpose-metrics.md
git commit -m "feat: expose semantic partition query purpose metrics"
```

### Task 2: Push Verified Work

**Files:**
- No source files.

- [ ] **Step 1: Verify focused smoke suite**

Run:

```powershell
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke)" --output-on-failure
```

Expected: all three smoke tests pass.

- [ ] **Step 2: Push main**

Run:

```powershell
git push origin main
```

Expected: push completes successfully.
