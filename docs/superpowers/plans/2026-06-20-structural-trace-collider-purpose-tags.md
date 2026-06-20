# Structural Trace Collider Purpose Tags Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stamp structural trace colliders with active M/P/Q/I channels and Q-mesh purposes so downstream systems can inspect and filter collider intent cheaply.

**Architecture:** `SceneSubtreeColliders.cpp` already centralizes structural semantic tag emission. Extend that tagger to call the shared `StructuralBrushParticipatesInChannel` and `StructuralBrushSupportsQueryPurpose` helpers, using existing `ToString(...)` names for stable tag payloads.

**Tech Stack:** C++20, RawIron.SceneUtilities subtree collider generation, RawIron.Core structural brush metadata helpers, CTest smoke tests.

---

### Task 1: Add Channel and Query-Purpose Tags to Structural Trace Colliders

**Files:**
- Modify: `Source/RawIron.SceneUtilities/src/SceneSubtreeColliders.cpp`
- Modify: `Tests/SceneSubtreeCollidersSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-20-structural-trace-collider-purpose-tags.md`

- [x] **Step 1: Write the failing semantic tag test**

Extend `Tests/SceneSubtreeCollidersSmoke.cpp` so the existing `PlayerBrush` semantic tag assertions also require:

```cpp
ContainsTag(*playerCollider, "structural.channel:visual_mesh")
ContainsTag(*playerCollider, "structural.channel:physics_mesh")
ContainsTag(*playerCollider, "structural.channel:query_mesh")
ContainsTag(*playerCollider, "structural.channel:information_layer")
ContainsTag(*playerCollider, "structural.query_purpose:raycast")
ContainsTag(*playerCollider, "structural.query_purpose:trace")
ContainsTag(*playerCollider, "structural.query_purpose:placement")
ContainsTag(*playerCollider, "structural.query_purpose:interaction")
```

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SceneSubtreeCollidersSmoke
```

Expected: the target builds, but the smoke executable fails under CTest because those tags are not emitted yet.

- [x] **Step 3: Emit active channel and purpose tags**

Update `AppendStructuralBrushSemanticTags(...)` to append `structural.channel:<name>` for each active M/P/Q/I channel and `structural.query_purpose:<name>` for each supported Q-mesh purpose.

- [x] **Step 4: Run focused verification**

Run:

```powershell
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke)" --output-on-failure
```

Expected: all three smoke tests pass.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/src/SceneSubtreeColliders.cpp Tests/SceneSubtreeCollidersSmoke.cpp docs/superpowers/plans/2026-06-20-structural-trace-collider-purpose-tags.md
git commit -m "feat: tag structural trace colliders by query intent"
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
