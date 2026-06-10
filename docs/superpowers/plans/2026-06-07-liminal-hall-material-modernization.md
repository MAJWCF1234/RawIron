# Liminal Hall Material Modernization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Liminal Hall's remaining legacy placeholder-style material requests with exact LRT package-driven surface families while keeping rendering engine-owned.

**Architecture:** Keep `RawIron.Render.Vulkan` unchanged for this slice. Modernize the game's authored structural/primitives data and strengthen `LiminalHallWorld.cpp`'s runtime material remap so package textures are resolved consistently and special-case surfaces still read correctly.

**Tech Stack:** C++20, CMake, CSV-authored level data, RawIron scene/material runtime, PowerShell/CTest verification

---

### Task 1: Add a failing material regression check

**Files:**
- Create: `Tests/VerifyLiminalHallMaterials.cmake`
- Modify: `Games/LiminalHall/Runtime/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

```cmake
file(READ "${RAWIRON_WORKSPACE}/Games/LiminalHall/levels/assembly.structural.csv" structural)
if (structural MATCHES "ri_psx_official_")
  message(FATAL_ERROR "Liminal Hall structural assembly still references legacy ri_psx_official textures.")
endif()
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake -DRAWIRON_WORKSPACE=O:/RawIron -P Tests/VerifyLiminalHallMaterials.cmake`
Expected: FAIL with a message about `ri_psx_official` textures still being present.

- [ ] **Step 3: Register the check in the Liminal Hall runtime target**

```cmake
if (RAWIRON_BUILD_TESTS)
  add_test(
    NAME RawIron.LiminalHall.MaterialAudit
    COMMAND ${CMAKE_COMMAND}
      -DRAWIRON_WORKSPACE=${CMAKE_SOURCE_DIR}
      -P ${CMAKE_SOURCE_DIR}/Tests/VerifyLiminalHallMaterials.cmake
  )
endif()
```

- [ ] **Step 4: Re-run the script to keep the red state visible**

Run: `cmake -DRAWIRON_WORKSPACE=O:/RawIron -P Tests/VerifyLiminalHallMaterials.cmake`
Expected: FAIL until the authored data is modernized.

- [ ] **Step 5: Commit**

```bash
git add Tests/VerifyLiminalHallMaterials.cmake Games/LiminalHall/Runtime/CMakeLists.txt
git commit -m "test: add liminal hall material audit"
```

### Task 2: Modernize authored structural and visible primitive textures

**Files:**
- Modify: `Games/LiminalHall/levels/assembly.structural.csv`
- Modify: `Games/LiminalHall/levels/assembly.primitives.csv`

- [ ] **Step 1: Write the failing authored-data expectation**

```text
Expectation: structural concrete/stone/metal rows no longer use `ri_psx_official_*` in the `texture` column.
Expectation: visible glow/window-card primitive rows no longer use placeholder `ri_prototype_white.png`.
```

- [ ] **Step 2: Update structural surface families**

```csv
OuterBasinFloor,...,ctm/RT_all_concrete_1.png,...,ctm/RT_all_concrete_1_n.png,ctm/RT_all_concrete_1_s.png,...
SouthEntryWallWest,...,tile/RT_tuff_bricks.png,...,tile/RT_tuff_bricks_n.png,tile/RT_tuff_bricks_s.png,...
WestCatwalk,...,tile/RT_stainless_steel.png,...,tile/RT_iron_block_n.png,tile/RT_stainless_steel_s.png,...
```

- [ ] **Step 3: Update visible primitive glow/window textures**

```csv
SouthCorridorFluorescentGlow_import1,...,O:/RawIron/Assets/Packages/LRT - Texture Pack - RT28.8 - 128x/tile/RT_shroomlight.png,...
NorthApseDoorGlow_import1,...,O:/RawIron/Assets/Packages/LRT - Texture Pack - RT28.8 - 128x/tile/RT_sea_lantern.png,...
TowerWindowCardW1_import1,...,-,...
```

- [ ] **Step 4: Run the audit to verify authored data is green**

Run: `cmake -DRAWIRON_WORKSPACE=O:/RawIron -P Tests/VerifyLiminalHallMaterials.cmake`
Expected: PASS with no fatal error.

- [ ] **Step 5: Commit**

```bash
git add Games/LiminalHall/levels/assembly.structural.csv Games/LiminalHall/levels/assembly.primitives.csv
git commit -m "feat: modernize liminal hall authored materials"
```

### Task 3: Strengthen runtime material remapping for package-exact visuals

**Files:**
- Modify: `Games/LiminalHall/Runtime/src/LiminalHallWorld.cpp`

- [ ] **Step 1: Write the failing runtime expectation**

```text
Expectation: concrete gets full albedo/normal/spec triplets, stone masses get tuff/tuff-brick triplets, metal spans get stronger metal families, and key logic/showcase surfaces stop looking like flat prototype cubes where practical.
```

- [ ] **Step 2: Implement exact remap helpers**

```cpp
auto forcePackageTriplet = [&](ri::scene::Material& material,
                               std::string_view albedoTail,
                               std::string_view normalTail,
                               std::string_view specTail) { /* ... */ };
```

- [ ] **Step 3: Expand the per-material and per-node classification**

```cpp
if (ContainsAny(key, {"pressureplate"})) { /* iron/trapdoor family */ }
if (ContainsAny(key, {"portal"})) { /* emissive glass family */ }
if (ContainsAny(nodeName, {"tower", "monolith", "drum"})) { /* tuff/tuff-brick family */ }
if (ContainsAny(nodeName, {"catwalk", "brace", "bridge"})) { /* metal family */ }
```

- [ ] **Step 4: Build the Liminal Hall runtime target**

Run: `cmake --build build --target RawIron.Game.LiminalHall -j 8`
Expected: build succeeds.

- [ ] **Step 5: Commit**

```bash
git add Games/LiminalHall/Runtime/src/LiminalHallWorld.cpp
git commit -m "feat: strengthen liminal hall runtime material remap"
```
