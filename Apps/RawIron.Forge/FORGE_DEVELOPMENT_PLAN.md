# RawIron Forge Development Plan

Forge is the model, rig, and animation companion app launched from the existing Forge icon in Visual Shell. It is not another workdesk and should not duplicate the editor's level/project responsibilities.

## Current State

- indexes OBJ, glTF/GLB, FBX, and Blender authoring sources under `Assets/Source`
- indexes and validates `.ri_rig.json` skeleton assets
- runs the real Raw Iron importer on selected OBJ/glTF/GLB/FBX files
- reports imported node, mesh, and material counts
- distinguishes Blender authoring containers from runtime-importable exports
- creates collision-safe baseline humanoid rig files
- opens selected source assets in their associated authoring application
- has headless catalog/import regression coverage

## Active Backlog

### 1. Native model preview

- render the selected imported scene in an embedded Raw Iron viewport
- orbit/pan/frame controls, bounds, axes, wireframe, normals, skeleton, and collision overlays
- cancelable background imports with visible progress and error details

### 2. Asset preparation document

- source units/up-axis/scale and transform normalization
- mesh/material/texture dependency list
- LOD and collision-generation settings
- deterministic output into `.ri_asset.json` plus generated runtime data
- reimport status and source-change detection

### 3. Rig hierarchy editor

- bone tree selection, add/delete/reparent/rename
- rest-pose transform editing and mirror tools
- humanoid mapping and coverage visualization
- hierarchy validation without hand-editing JSON

### 4. Skinning workflow

- bind a rig to an imported mesh
- weight inspection, normalization, prune, mirror, and limited paint tools
- maximum-influence and missing-weight validation
- portable storage that the editor/runtime can consume

### 5. Animation workflow

- clip list and timeline
- trim, loop, root-motion, event markers, and retarget preview
- skeleton compatibility diagnostics
- animation compression/build settings

### 6. Editor handoff

- open/import selected Forge output in `RawIron.Editor`
- preserve source/model/rig/material/animation ownership links
- surface the same validation results in both apps

## Next Recommended Sprint

Build an embedded preview around the existing importer and scene renderer, then create a typed asset-preparation document. Do not begin weight painting until preview, reimport, deterministic output, and undo/redo foundations exist.

## Guardrails

- `.blend` remains an authoring container; runtime data comes from deterministic exported inputs
- import/validation work runs off the UI thread once previews become interactive
- model and rig edits require undo/redo before destructive controls ship
- Forge outputs must load in the editor and runtime through shared engine APIs
- every supported format needs a valid and malformed fixture test
