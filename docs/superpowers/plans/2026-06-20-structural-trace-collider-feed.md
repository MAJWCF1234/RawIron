# Structural Trace Collider Feed Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Provide a scene-level structural trace collider feed with safe defaults for trace/ballistics systems.

**Architecture:** `SceneSubtreeColliders` already performs the actual collider extraction and filtering. `SceneStructuralTraceFeed` should expose a higher-level helper that returns default options for structural trace feeds and builds a vector of colliders for a subtree using those options.

**Tech Stack:** C++20, RawIron.SceneUtilities, RawIron.Trace, CTest smoke tests.

---

### Task 1: Add Default Structural Trace Collider Feed Helper

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneStructuralTraceFeed.h`
- Modify: `Source/RawIron.SceneUtilities/src/SceneStructuralTraceFeed.cpp`
- Modify: `Source/RawIron.SceneUtilities/CMakeLists.txt`
- Create: `Tests/SceneStructuralTraceFeedSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-20-structural-trace-collider-feed.md`

- [x] **Step 1: Write the failing smoke test**

Create `Tests/SceneStructuralTraceFeedSmoke.cpp` that builds a scene with a traceable solid brush, a placement-only solid brush, and a query-collision brush. Call:

```cpp
const std::vector<ri::trace::TraceCollider> colliders =
    ri::scene::BuildStructuralTraceCollidersForSubtree(scene, root);
```

Expected: only the traceable solid brush emits a collider, and that collider has `structural.query_purpose:trace`.

- [x] **Step 2: Register and run the test to verify it fails**

Add the test target to `Source/RawIron.SceneUtilities/CMakeLists.txt`, then run:

```powershell
cmake --build build\semantic-metadata --target SceneStructuralTraceFeedSmoke
```

Expected: build fails because `BuildStructuralTraceCollidersForSubtree` does not exist.

- [x] **Step 3: Add feed-level helper API**

Declare in `SceneStructuralTraceFeed.h`:

```cpp
[[nodiscard]] SubtreeColliderBuildOptions MakeDefaultStructuralTraceColliderBuildOptions();
[[nodiscard]] std::vector<ri::trace::TraceCollider> BuildStructuralTraceCollidersForSubtree(
    const Scene& scene,
    int rootNodeHandle);
[[nodiscard]] std::vector<ri::trace::TraceCollider> BuildStructuralTraceCollidersForSubtree(
    const Scene& scene,
    int rootNodeHandle,
    const SubtreeColliderBuildOptions& options);
```

Implement the default options as:

```cpp
{
    .structural = true,
    .dynamic = false,
    .respectStructuralBrushCollisionPolicy = true,
    .requireStructuralBrushQueryMeshChannel = true,
    .requiredStructuralBrushQueryPurpose = StructuralBrushQueryPurpose::Trace,
    .appendStructuralBrushSemanticTags = true,
}
```

- [x] **Step 4: Run focused verification**

Run:

```powershell
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke|SceneStructuralTraceFeedSmoke)" --output-on-failure
```

Expected: all four smoke tests pass.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneStructuralTraceFeed.h Source/RawIron.SceneUtilities/src/SceneStructuralTraceFeed.cpp Source/RawIron.SceneUtilities/CMakeLists.txt Tests/SceneStructuralTraceFeedSmoke.cpp docs/superpowers/plans/2026-06-20-structural-trace-collider-feed.md
git commit -m "feat: add structural trace collider feed"
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
