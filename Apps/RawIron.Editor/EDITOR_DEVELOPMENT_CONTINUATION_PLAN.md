# RawIron Editor Development Continuation Plan

## Purpose

This document is an unpushed handoff and continuation plan for `RawIron.Editor`.

It is meant to answer four questions clearly:

1. What the editor is today
2. What was already split out of the old monolith
3. What still needs to be built or refactored
4. What order the next work should happen in so the editor becomes a true end-to-end game authoring tool

This plan assumes the editor should evolve into a Hammer/Slade-style native authoring app for RawIron projects rather than remain a scene preview shell with partial editing tools.

## Current Reality

The editor is no longer a single giant file plus a few helpers, but it is also not finished as a proper app architecture yet.

Current strengths:

- boots against a mounted RawIron workspace and mounted game
- loads the authoring scene and primary preview scene
- exposes scene graph editing and resource browsing
- supports project scaffolding
- supports playtest launching
- supports embedded text editing for supported project resources
- supports structural primitive authoring
- supports gameplay authoring controls for some runtime policy and trigger workflows
- has a headless smoke-testable startup path

Current weaknesses:

- too much orchestration still lives in `main.cpp`
- the renderer/painter is only partially extracted
- left-panel scene/resources rendering is still inline
- input handling is still concentrated in one class
- the app still lacks full authoring workflows for UI, menus, materials, lights, entities, colliders, triggers, gameplay data, and multi-file roundtrip editing
- the editor does not yet feel like a complete engine-owned game creation environment

## Current Source Layout

Current editor app module list:

- [CMakeLists.txt](D:/RawIron/Apps/RawIron.Editor/CMakeLists.txt)
- [main.cpp](D:/RawIron/Apps/RawIron.Editor/src/main.cpp)
- [EditorWorkspace.h](D:/RawIron/Apps/RawIron.Editor/src/EditorWorkspace.h)
- [EditorWorkspace.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorWorkspace.cpp)
- [EditorProjectScaffolding.h](D:/RawIron/Apps/RawIron.Editor/src/EditorProjectScaffolding.h)
- [EditorProjectScaffolding.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorProjectScaffolding.cpp)
- [EditorPlaytestLauncher.h](D:/RawIron/Apps/RawIron.Editor/src/EditorPlaytestLauncher.h)
- [EditorPlaytestLauncher.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorPlaytestLauncher.cpp)
- [EditorResourceBrowser.h](D:/RawIron/Apps/RawIron.Editor/src/EditorResourceBrowser.h)
- [EditorResourceBrowser.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorResourceBrowser.cpp)
- [EditorResourceDocument.h](D:/RawIron/Apps/RawIron.Editor/src/EditorResourceDocument.h)
- [EditorResourceDocument.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorResourceDocument.cpp)
- [EditorResourceTextEditor.h](D:/RawIron/Apps/RawIron.Editor/src/EditorResourceTextEditor.h)
- [EditorResourceTextEditor.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorResourceTextEditor.cpp)
- [EditorHierarchy.h](D:/RawIron/Apps/RawIron.Editor/src/EditorHierarchy.h)
- [EditorHierarchy.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorHierarchy.cpp)
- [EditorFilesInspector.h](D:/RawIron/Apps/RawIron.Editor/src/EditorFilesInspector.h)
- [EditorFilesInspector.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorFilesInspector.cpp)
- [EditorInspectorPanels.h](D:/RawIron/Apps/RawIron.Editor/src/EditorInspectorPanels.h)
- [EditorInspectorPanels.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorInspectorPanels.cpp)
- [EditorRenderer.h](D:/RawIron/Apps/RawIron.Editor/src/EditorRenderer.h)
- [EditorRenderer.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorRenderer.cpp)

Related bundled preview registration that was also introduced:

- [RegisterBundledGamePreviews.cpp](D:/RawIron/Source/RawIron.Editor.BundledGames/src/RegisterBundledGamePreviews.cpp)
- [MultiplayerSandboxEditorPreview.h](D:/RawIron/Games/RawIronMultiplayerSandbox/Runtime/include/RawIron/Games/MultiplayerSandbox/MultiplayerSandboxEditorPreview.h)
- [MultiplayerSandboxEditorPreview.cpp](D:/RawIron/Games/RawIronMultiplayerSandbox/Runtime/src/MultiplayerSandboxEditorPreview.cpp)

## What Was Already Successfully Split

The following responsibilities are already extracted from the old monolithic editor body:

### Workspace and project discovery

- workspace game enumeration
- resource catalog collection
- category classification
- text-resource detection
- project dev-config ensure path

Owned by:

- [EditorWorkspace.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorWorkspace.cpp)

### Project scaffolding

- creating missing authoring files
- mounted game bootstrap support

Owned by:

- [EditorProjectScaffolding.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorProjectScaffolding.cpp)

### Playtest launching

- runtime executable resolution
- playtest launch routing

Owned by:

- [EditorPlaytestLauncher.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorPlaytestLauncher.cpp)

### Resource browser filtering/navigation

- visible row filtering
- resource path lookup
- visible scroll calculations

Owned by:

- [EditorResourceBrowser.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorResourceBrowser.cpp)

### Resource document logic

- file metadata validation
- manifest validation summary
- text/binary editability classification
- save path for resource text

Owned by:

- [EditorResourceDocument.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorResourceDocument.cpp)

### Embedded resource text editor control

- creating and destroying the Win32 edit control
- layout and visibility logic
- saving from the embedded control
- opening the selected file in Explorer
- dirty-resource prompt flow

Owned by:

- [EditorResourceTextEditor.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorResourceTextEditor.cpp)

### Hierarchy helpers

- hierarchy draw-order construction
- hierarchy search filtering
- visible selection scroll calculations

Owned by:

- [EditorHierarchy.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorHierarchy.cpp)

### Files inspector model

- project shortcut layout
- Files panel content model
- manifest summary model
- footer and aux-state summary

Owned by:

- [EditorFilesInspector.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorFilesInspector.cpp)

### Inspector panel rendering helpers

- Node panel body rendering
- Brush panel body rendering
- Gameplay panel body rendering
- gameplay row layout

Owned by:

- [EditorInspectorPanels.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorInspectorPanels.cpp)

### Shared renderer primitives

- UTF-8 / wide conversion helpers
- fill / frame / inset frame painting
- text painting
- toolbar button painting
- panel header painting

Owned by:

- [EditorRenderer.cpp](D:/RawIron/Apps/RawIron.Editor/src/EditorRenderer.cpp)

## Current Main Problem

Even after the successful refactor passes, the editor window class in [main.cpp](D:/RawIron/Apps/RawIron.Editor/src/main.cpp) still does too much.

The main remaining architectural issue is that `RawIronEditorWindow` still mixes:

- Win32 app shell and lifetime
- input dispatch
- layout calculations
- scene edit orchestration
- left panel rendering
- viewport rendering orchestration
- status bar rendering
- part of inspector tab routing
- some transform-editing and authoring state logic

The code is healthier than before, but still too centralized for the editor to scale safely.

## High-Level Development Goal

The correct destination is:

`RawIron.Editor` becomes a true engine-owned game creation app where RawIron projects can be created, opened, edited, previewed, validated, and playtested without depending on per-game ad hoc tooling.

That means the editor should eventually own:

- project creation
- project scaffolding
- level authoring
- geometry/primitive authoring
- gameplay entity authoring
- materials and lighting
- triggers and colliders
- UI layout and menu authoring
- runtime config authoring
- asset and dependency inspection
- launch/playtest/debug workflows
- export/roundtrip of engine-owned data formats

## Recommended Development Phases

The next work should happen in phases. Do not try to do all of this at once.

### Phase 1: Finish the architectural split

Goal:

Move remaining structural responsibilities out of `main.cpp` until it becomes mostly app shell and coordination code.

#### Phase 1A: Extract left-panel rendering

Create:

- `EditorLeftPanel.h`
- `EditorLeftPanel.cpp`

Move out:

- scene graph list drawing
- resources list drawing
- search UI drawing
- category chip drawing
- visible row rendering
- focused game strip rendering
- left-panel tab strip rendering

Recommended outputs:

- left-panel draw helpers
- left-panel view models
- hit-test helpers for rows/chips/search buttons

Why:

The left panel is still one of the highest-churn UI surfaces and should not remain embedded in the main paint pass.

#### Phase 1B: Extract viewport frame renderer

Create:

- `EditorViewportRenderer.h`
- `EditorViewportRenderer.cpp`

Move out:

- top chrome paint orchestration
- tool strip paint orchestration
- viewport block paint orchestration
- status bar paint orchestration
- final buffer composition helpers
- clip exclusion logic around the resource edit control

Why:

This is the most important remaining step if the editor is to become a proper app instead of a giant paint function.

#### Phase 1C: Extract input routing helpers

Create:

- `EditorInput.h`
- `EditorInput.cpp`

Move out:

- toolbar hit handling
- left-panel row selection routing
- resource shortcut click handling
- gameplay panel button hit handling
- scene graph interaction routing
- wheel/keyboard navigation helper logic

Why:

Right now input behavior is still tightly fused to the window class. That makes future features harder and riskier to add.

#### Phase 1D: Extract scene editing controller

Create:

- `EditorSceneController.h`
- `EditorSceneController.cpp`

Move out:

- selected node edit application
- transform nudge logic
- authored node operations
- duplication/grouping/ungrouping
- protected-node behavior
- export/import command helpers
- trigger spawn helpers

Why:

Scene-edit behavior is application logic, not window-shell logic.

### Phase 2: Build real editor-owned workflows

Goal:

Stop thinking of the editor as “preview + some editing” and turn it into a complete project authoring tool.

#### Phase 2A: New project creation wizard

Create:

- `EditorNewProjectWizard.h`
- `EditorNewProjectWizard.cpp`

Must support:

- create new RawIron game from editor
- create manifest
- create levels/scripts/config/ui/menus/tests structure
- choose template or genre starter
- optionally register bundled preview

Desired starter templates:

- empty 3D sandbox
- liminal exploration
- combat/test arena
- multiplayer sandbox

#### Phase 2B: Visual UI and menu authoring

Create:

- `EditorUiDesigner.h`
- `EditorUiDesigner.cpp`
- `EditorMenuDesigner.h`
- `EditorMenuDesigner.cpp`

Must support:

- screen tree
- box/text/image/button layout editing
- menu route linking
- style preview
- XML/CSS/menu roundtrip

Why:

Right now the editor can jump to UI files, but not visually author them. This is one of the biggest blockers to true end-to-end creation.

#### Phase 2C: Materials, lights, triggers, colliders inspectors

Create:

- `EditorMaterialInspector.*`
- `EditorLightInspector.*`
- `EditorTriggerInspector.*`
- `EditorColliderInspector.*`

Must support:

- editing engine-owned component data directly
- validation feedback
- common presets
- preview and reset affordances

Why:

These are core engine-owned responsibilities that game projects should not need to reinvent in custom scripts.

#### Phase 2D: Entity and gameplay authoring

Create:

- `EditorEntityAuthoring.*`
- `EditorGameplayAuthoring.*`

Must support:

- basic entity archetypes
- spawn points
- trigger target assignment
- gameplay script and config references
- simple stateful gameplay components

Why:

Without this, game creation still falls back to hand-editing files and per-project workarounds.

### Phase 3: Strengthen engine ownership and data roundtripping

Goal:

Ensure game teams trust editor-owned and engine-owned formats instead of writing one-off systems in game projects.

#### Phase 3A: Strong roundtrip guarantees

Must guarantee:

- editor loads and saves engine-owned project files without silent loss
- level exports preserve authored intent
- resource edits do not strip metadata
- inspector edits are serialized cleanly

Targets:

- scene state
- primitives CSV
- gameplay config
- rendering config
- UI XML/CSS
- menu config
- trigger data
- material references

#### Phase 3B: Validation surfaces

Add:

- project validation panel
- missing dependency checks
- bad path detection
- schema issues summary
- actionable fix suggestions

Why:

The editor should catch broken project state before the user has to discover it at runtime.

#### Phase 3C: Asset ownership view

Add:

- mounted assets browser
- package reference inspector
- asset dependency preview
- texture/material resolution display
- missing asset diagnostics

Why:

If the editor cannot explain what assets a project depends on, teams will keep building their own side tools.

### Phase 4: Make the editor feel like a complete product

Goal:

Polish the app into something people want to stay inside for hours.

Needs:

- command palette
- multi-document tabs
- persistent panel layouts
- better hotkey discoverability
- session restore
- consistent status feedback
- cleaner top chrome
- better error dialogs
- autosave visibility
- undo/redo history inspection

## Concrete Next Coding Sequence

If someone resumes work immediately, this is the exact order recommended:

1. Extract left-panel rendering and hit helpers
2. Extract viewport/top chrome/status bar orchestration into a true frame renderer
3. Extract scene edit/controller logic
4. Move `RawIronEditorWindow` declaration into its own header and split implementation across multiple `.cpp` files
5. Add project creation wizard
6. Add visual UI/menu editor
7. Add material/light/trigger/collider inspectors
8. Add entity/gameplay authoring
9. Add project validation panel
10. Add asset ownership browser

This order is important.

If the architecture is not finished first, feature work will keep inflating `main.cpp` again.

## Suggested File Targets For The Next Refactor

The next likely file set:

- `D:\RawIron\Apps\RawIron.Editor\src\EditorLeftPanel.h`
- `D:\RawIron\Apps\RawIron.Editor\src\EditorLeftPanel.cpp`
- `D:\RawIron\Apps\RawIron.Editor\src\EditorViewportRenderer.h`
- `D:\RawIron\Apps\RawIron.Editor\src\EditorViewportRenderer.cpp`
- `D:\RawIron\Apps\RawIron.Editor\src\EditorSceneController.h`
- `D:\RawIron\Apps\RawIron.Editor\src\EditorSceneController.cpp`
- `D:\RawIron\Apps\RawIron.Editor\src\EditorWindow.h`
- `D:\RawIron\Apps\RawIron.Editor\src\EditorWindow.cpp`

## Current Validation Loop

The current safe validation loop is:

1. Build:

```powershell
cmake --build D:\RawIron\build --target RawIron.Editor -j 6
```

2. Headless smoke boot:

```powershell
D:\RawIron\build\Apps\RawIron.Editor\RawIron.Editor.exe --headless --frames 1 --no-scene-dump --workspace-root=D:\RawIron --game=rawiron-multiplayer-sandbox
```

3. Optional interactive boot:

```powershell
D:\RawIron\build\Apps\RawIron.Editor\RawIron.Editor.exe --workspace-root=D:\RawIron --game=rawiron-multiplayer-sandbox
```

Minimum success criteria:

- editor process starts
- mounted game opens correctly
- scene state resolves
- no startup exception
- resource panel still functions
- playtest still resolves runtime target

## Known Remaining Rough Spots

These are not necessarily broken, but they should be tracked:

- `main.cpp` still contains too much top-level orchestration
- some window/helper wrappers remain in `main.cpp`
- the left panel is still inline
- viewport and status bar orchestration are still inline
- scene edit behavior is still mixed into the window class
- visual UI/menu authoring does not exist yet
- materials/lights/colliders/triggers are not first-class editor inspectors yet
- the gameplay panel is still fairly light compared to what a full game editor needs

## Recommended Design Principles Going Forward

### Keep engine ownership strong

Whenever possible, prefer engine-owned formats, inspectors, and workflows over project-specific hacks.

The editor should become the trusted place where teams:

- create projects
- inspect config
- validate assets
- author levels
- wire gameplay
- preview runtime behavior

### Separate model from rendering

Every panel should eventually have:

- a state/model structure
- layout helpers
- render helpers
- input helpers

Do not keep inventing panel content ad hoc inside paint handlers.

### Separate shell from logic

The window class should eventually only own:

- native lifetime
- message routing
- high-level orchestration
- module coordination

It should not own all detailed editor behaviors directly.

### Prefer small testable passes

The safest way to keep momentum is:

1. split one subsystem
2. rebuild
3. smoke boot
4. then continue

Do not do giant all-at-once rewrites of the editor shell.

## If Resuming After A Break

Recommended first resume actions:

1. open [CMakeLists.txt](D:/RawIron/Apps/RawIron.Editor/CMakeLists.txt)
2. inspect [main.cpp](D:/RawIron/Apps/RawIron.Editor/src/main.cpp)
3. inspect the extracted modules listed above
4. search for:

```text
Paint(
OnLeftButtonDown(
OnKeyDown(
DrawPanelHeader(
DrawToolbarButton(
```

5. choose the next subsystem to extract rather than adding features directly to `main.cpp`

## Short Version

If there is only time for one sentence:

Finish the editor architectural split first, then build visual UI/menu authoring and real gameplay/material/light/trigger inspectors on top of that cleaner structure.

