# Structural Query Purpose Gates Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make structural primitive Q-mesh participation precise enough for raycast, trace/ballistics, placement, and interaction systems to avoid unnecessary candidates.

**Architecture:** Core scene metadata owns the query-purpose vocabulary and helper predicates. SceneUtilities consumers call those helpers when building trace collider sets so runtime systems get filtered data without duplicating structural-brush rules.

**Tech Stack:** C++20, RawIron.Core scene components, RawIron.SceneUtilities subtree collider generation, CTest smoke targets.

---

### Task 1: Add Core Structural Query Purpose Helpers

**Files:**
- Modify: `Source/RawIron.Core/include/RawIron/Scene/Components.h`
- Modify: `Source/RawIron.Core/src/Scene.cpp`
- Modify: `Tests/StructuralBrushMetadataSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-20-structural-query-purpose-gates.md`

- [x] **Step 1: Write the failing metadata test**

Add checks in `Tests/StructuralBrushMetadataSmoke.cpp` that call:

```cpp
ri::scene::ToString(ri::scene::StructuralBrushQueryPurpose::Raycast)
ri::scene::StructuralBrushSupportsQueryPurpose(metadata, ri::scene::StructuralBrushQueryPurpose::Raycast)
```

Expected names: `raycast`, `trace`, `placement`, and `interaction`.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target StructuralBrushMetadataSmoke
```

Expected: build fails because `StructuralBrushQueryPurpose` and `StructuralBrushSupportsQueryPurpose` do not exist.

- [x] **Step 3: Add the core enum and helper**

Add this enum near `StructuralBrushChannel`:

```cpp
enum class StructuralBrushQueryPurpose {
    Raycast,
    Trace,
    Placement,
    Interaction,
};
```

Add declarations:

```cpp
std::string ToString(StructuralBrushQueryPurpose purpose);
bool StructuralBrushSupportsQueryPurpose(const StructuralBrushMetadata& metadata,
                                         StructuralBrushQueryPurpose purpose);
```

Implement the helper so it maps to `queryMesh.raycastable`, `traceable`, `placeable`, and `interactable`.

- [x] **Step 4: Run the metadata smoke target**

Run:

```powershell
cmake --build build\semantic-metadata --target StructuralBrushMetadataSmoke
```

Expected: target builds successfully.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.Core/include/RawIron/Scene/Components.h Source/RawIron.Core/src/Scene.cpp Tests/StructuralBrushMetadataSmoke.cpp docs/superpowers/plans/2026-06-20-structural-query-purpose-gates.md
git commit -m "feat: expose structural query purpose helpers"
```

### Task 2: Gate Trace Collider Emission by Trace Purpose

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneSubtreeColliders.h`
- Modify: `Source/RawIron.SceneUtilities/src/SceneSubtreeColliders.cpp`
- Modify: `Tests/SceneSubtreeCollidersSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-20-structural-query-purpose-gates.md`

- [x] **Step 1: Write the failing subtree collider test**

Add a brush whose Q-mesh is still enabled for placement/interaction but disabled for trace, then call:

```cpp
ri::scene::AppendTraceCollidersForSubtree(
    scene,
    root,
    {.requiredStructuralBrushQueryPurpose = ri::scene::StructuralBrushQueryPurpose::Trace},
    colliders);
```

Expected: the non-traceable brush is omitted from emitted trace colliders.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SceneSubtreeCollidersSmoke
```

Expected: build fails because `SubtreeColliderBuildOptions::requiredStructuralBrushQueryPurpose` does not exist.

- [x] **Step 3: Add the option and filter**

Add:

```cpp
std::optional<StructuralBrushQueryPurpose> requiredStructuralBrushQueryPurpose{};
```

Use `StructuralBrushSupportsQueryPurpose` to skip structural brush nodes that do not support the requested purpose.

- [x] **Step 4: Run focused verification**

Run:

```powershell
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke)" --output-on-failure
```

Expected: all three tests pass.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneSubtreeColliders.h Source/RawIron.SceneUtilities/src/SceneSubtreeColliders.cpp Tests/SceneSubtreeCollidersSmoke.cpp docs/superpowers/plans/2026-06-20-structural-query-purpose-gates.md
git commit -m "feat: gate trace colliders by query purpose"
```

### Task 3: Push Verified Work

**Files:**
- No source files.

- [x] **Step 1: Verify the focused smoke suite**

Run:

```powershell
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke)" --output-on-failure
```

Expected: all three tests pass.

- [x] **Step 2: Push main**

Run:

```powershell
git push origin main
```

Expected: push completes successfully.
