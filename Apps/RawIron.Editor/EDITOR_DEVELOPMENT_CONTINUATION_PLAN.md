# RawIron Editor Development Continuation Plan

## Purpose

This is the current continuation plan for `RawIron.Editor`. Remove completed work instead of accumulating historical phases here. The editor is Raw Iron's engine-owned game creation app; Visual Shell is the workdesk and Forge is the companion model/rig authoring app.

## Current State

Shipped editor workflows now include:

- mounted workspace/game discovery and project health reporting
- new-game templates and project scaffolding
- scene hierarchy, resource browser, embedded text editing, and save prompts
- native viewport rendering with Vulkan presentation and performance controls
- authored-node transform, grouping, duplication, deletion, and placement operations
- structural primitive catalog, thumbnails, placement, exact Q-mesh picking, CSV import/export, and M/P/Q/I diagnostics
- material roughness/metallic/opacity adjustment and light intensity adjustment
- trigger creation and collider/lighting/trigger CSV roundtrip helpers
- Logic Kit authoring/runtime preview surfaces
- UI workbench screen/block creation and preview
- plugin store/project policy surfaces
- project playtest launching and headless smoke boots for bundled games

The extracted modules are the source of truth for architecture. Important owners include:

- `EditorWorkspace.*`, `EditorProjectHealth.*`, `EditorProjectScaffolding.*`, `EditorNewGame.*`
- `EditorResourceBrowser.*`, `EditorResourceDocument.*`, `EditorResourceTextEditor.*`
- `EditorHierarchy.*`, `EditorLeftPanel.*`, `EditorInspectorPanels.*`
- `EditorInput.*`, `EditorSceneController.*`, `EditorViewportRenderer.*`, `EditorVulkanViewport.*`
- `EditorStructuralPicker.*`, `EditorAuthoringCatalog.*`, `EditorLevelExport.*`
- `EditorLogicLayer.*`, `EditorPluginManager.*`, `EditorPlaytestLauncher.*`

## Main Architectural Weakness

`src/main.cpp` remains roughly nine thousand lines. The earlier extraction work exists, but `RawIronEditorWindow` still owns too much state, message routing, panel coordination, frame composition, and feature-specific command handling. New features should move logic into an owning module rather than extending the window class with another subsystem.

## Active Backlog

### 1. Split the window coordinator

- move `RawIronEditorWindow` declaration/lifetime into `EditorWindow.*`
- move remaining paint/frame coordination into a small frame-composition module
- move command routing into typed commands instead of expanding message-handler switches
- retain state/model/layout/render/input separation for every panel

Done means `main.cpp` contains startup/CLI selection and high-level application boot rather than the editor implementation.

### 2. Complete structural authoring

M/P/Q/I validation is visible in the Brush inspector. Next add direct controls for:

- semantic role, region, operation, and rebuild scope
- collision, visibility, and navigation policies
- M/P/Q/I participation and Q-mesh purposes
- generated-fragment-to-authored-owner selection
- overlays for regions, query/collision participation, and dirty rebuild bounds

All edits must invalidate the semantic partition cache and roundtrip without dropping metadata.

### 3. Add an editor transaction model

- command-backed undo/redo for transforms, hierarchy changes, primitives, materials, lights, triggers, and UI blocks
- dirty-state ownership per edited document/scene
- autosave/recovery visibility
- history inspection suitable for diagnosing destructive authoring actions

This comes before substantially expanding direct-edit controls.

### 4. Connect Forge assets to the editor

- open a selected Forge asset directly in an editor preview/import context
- show importer validation and dependency status in the resource inspector
- bind `.ri_rig.json` assets to imported skinned meshes
- preserve source, generated asset, rig, material, and animation ownership links

### 5. Finish first-class component inspectors

Extract and deepen dedicated material, light, trigger, collider, and entity inspectors. Each needs typed validation, presets, reset, serialization, and preview behavior rather than ad hoc nudge rows.

### 6. Strengthen project validation and asset ownership

- missing/bad dependency paths and schema issues
- mounted package and asset dependency graph
- source-versus-runtime asset status
- actionable repair suggestions
- build-time validation of editor-generated structural and UI data

### 7. Complete UI/menu roundtrip

The current UI workbench can create and preview screens/blocks. It still needs complete layout/property editing, route linking, style editing, lossless save/reload, and menu/runtime parity tests.

### 8. Product polish

- command palette and searchable actions
- multi-document tabs and session restore
- persistent panel layouts
- hotkey discovery
- clearer long-operation progress and error dialogs

## Next Recommended Sprint

1. Introduce an editor transaction/command interface for transforms and structural metadata.
2. Add editable structural role/region/policy/channel controls using that transaction path.
3. Serialize and reload the edits in a roundtrip smoke test.
4. Add region/Q-mesh overlays driven by the semantic partition cache.
5. Start the `EditorWindow.*` split only after those commands no longer depend on private window internals.

## Validation Loop

```powershell
cmake --build build/dev-msvc --config RelWithDebInfo --target RawIron.Editor
build/dev-msvc/Apps/RawIron.Editor/RelWithDebInfo/RawIron.Editor.exe --headless --frames 1 --no-scene-dump --workspace-root=O:/RawIron --game=rawiron-multiplayer-sandbox
ctest --test-dir build/dev-msvc -C RelWithDebInfo --output-on-failure -R "RawIron.Editor|SemanticStructuralPartition|StructuralBrushMetadata"
```

Before merging editor work, also run the complete configured CTest suite.

## Guardrails

- engine-owned formats and validation stay ahead of project-specific workarounds
- no feature may silently replace unknown/invalid data without reporting it
- editor changes must remain headless-smoke-testable
- visual controls require a model, validation path, serialization path, and regression test
- completed backlog items are removed from this document
