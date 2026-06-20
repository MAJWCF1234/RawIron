# Semantic Partition Query Purpose Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the semantic structural partition filter candidates by exact Q-mesh purpose: raycast, trace/ballistics, placement, or interaction.

**Architecture:** `RawIron.Core` already owns `StructuralBrushQueryPurpose` and `StructuralBrushSupportsQueryPurpose`. `RawIron.SceneUtilities` should expose an optional query-purpose field on `SemanticStructuralPartitionQuery` and use the shared core helper inside `MatchesQuery`, keeping the spatial index fast and semantic filtering centralized.

**Tech Stack:** C++20, RawIron.SceneUtilities semantic structural partition, RawIron.Core structural brush metadata, CTest smoke tests.

---

### Task 1: Add Query Purpose Filtering to Semantic Structural Partition

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h`
- Modify: `Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp`
- Modify: `Tests/SemanticStructuralPartitionSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-20-semantic-partition-query-purpose.md`

- [x] **Step 1: Write the failing semantic partition test**

Add test coverage to `Tests/SemanticStructuralPartitionSmoke.cpp` that makes `floor_a` query-enabled for placement/interaction but not trace, then queries:

```cpp
partition.QueryBox(
    {{-2.0f, -0.2f, -2.0f}, {2.0f, 0.2f, 2.0f}},
    {.queryPurpose = ri::scene::StructuralBrushQueryPurpose::Trace});
```

Expected: the trace-purpose query omits `floor_a` while placement-purpose queries can still find it.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
```

Expected: build fails because `SemanticStructuralPartitionQuery::queryPurpose` does not exist.

- [x] **Step 3: Add the query-purpose filter**

Add this field to `SemanticStructuralPartitionQuery`:

```cpp
std::optional<StructuralBrushQueryPurpose> queryPurpose{};
```

Then add this check to `SemanticStructuralPartition::MatchesQuery` after the broad channel check:

```cpp
if (query.queryPurpose.has_value()
    && !StructuralBrushSupportsQueryPurpose(entry.metadata, *query.queryPurpose)) {
    return false;
}
```

- [x] **Step 4: Run focused verification**

Run:

```powershell
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke)" --output-on-failure
```

Expected: all three smoke tests pass.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp Tests/SemanticStructuralPartitionSmoke.cpp docs/superpowers/plans/2026-06-20-semantic-partition-query-purpose.md
git commit -m "feat: filter semantic partition by query purpose"
```

### Task 2: Push Verified Work

**Files:**
- No source files.

- [x] **Step 1: Verify focused smoke suite**

Run:

```powershell
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke)" --output-on-failure
```

Expected: all three smoke tests pass.

- [x] **Step 2: Push main**

Run:

```powershell
git push origin main
```

Expected: push completes successfully.
