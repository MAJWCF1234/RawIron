# RawIron Forge Development Plan

Forge is Raw Iron's in-house modeler and sculptor: the Blender + ZBrush companion launched from the Visual Shell Forge tile. Native `.ri_model.json` primitives and `.ri_sculpt.json` clay are the source of truth. FBX, glTF, OBJ, and `.blend` remain import/export bridges, not the authoring format.

It is not another workdesk and should not duplicate the editor's level/project responsibilities.

## Current State

- indexes native primitive models, native sculpt clay, rigs, and imported OBJ/glTF/GLB/FBX/Blender sources under `Assets/Source`
- creates grouped primitive documents, dense sphere/cube sculpt cages, humanoid rigs, and rest-pose motion clips from Forge
- stamps per-part albedo/roughness/metal/texture on native stock models (LOOK bay) and keys bone-name animation clips on the bound rig (MOTION bay)
- sculpts selected `.ri_sculpt.json` clay in the embedded Vulkan viewport by calling engine APIs: LMB clay, Shift+LMB smooth, Ctrl+LMB inflate, Ctrl+Shift flatten, E+LMB extrude, Alt subtract, `[` `]` radius, `-` `=` strength, 9/0 density, X/Y/Z-mirror, W wireframe, N normals, C collision AABB, Ctrl+Z/Y stroke undo/redo; strokes save on mouse-up or Ctrl+S
- shows engine-built brush cursor, wireframe, vertex-normal ticks, and collision AABB overlays on that clay
- renders selected models, primitive documents, sculpt meshes, and rig skeletons in that viewport
- supports orbit, pan, zoom, frame-selected, and grid/axes overlay toggles on that preview
- runs the real Raw Iron importer on selected OBJ/glTF/GLB/FBX files
- distinguishes Blender authoring containers from runtime-importable exports
- creates collision-safe baseline humanoid rig files
- opens selected source assets in their associated authoring application
- opens a selected model, sculpt, or rig directly in the editor through the shared validated handoff API
- has headless catalog/import/sculpt regression coverage

## Active Backlog

### 1. Native DCC workspace

- clay/smooth/inflate/flatten, X/Y/Z-mirror, stroke undo, face extrude, density remesh, brush cursor, wireframe, normals, and collision AABB overlays are in
- stock models stamp albedo color/roughness/metal/texture through engine instantiate; motion clips are native `.ri_anim.json` bone tracks with play/key
- still missing: remesh that keeps extruded box topology, weight paint, clip timeline scrubber with multi-clip list
- cancelable import progress UI for large foreign files

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

- bind a rig to a native sculpt or primitive bake
- weight inspection, normalization, prune, mirror, and limited paint tools
- maximum-influence and missing-weight validation
- portable storage that the editor/runtime can consume

### 5. Animation workflow

- clip list and timeline
- trim, loop, root-motion, event markers, and retarget preview
- skeleton compatibility diagnostics
- animation compression/build settings

### 6. Editor handoff

- selected Forge sources now open in the editor Files inspector with shared rig/model/sculpt validation
- next, import prepared model output directly into the active scene
- preserve source/model/sculpt/rig/material/animation ownership links
- surface the same validation results in both apps

## Next Recommended Sprint

Keep growing the native loop: remesh that preserves extruded box topology, then a real timeline/weight paint pass. Treat FBX/glTF import as a one-way bridge into `.ri_sculpt.json` / `.ri_model.json` / `.ri_anim.json`, not as the place artists keep working.

## Guardrails

- `.ri_sculpt.json` and `.ri_model.json` are native Forge sources; `.blend`/FBX/glTF are interchange
- import/validation work runs off the UI thread once previews become interactive
- model, sculpt, and rig edits require undo/redo before destructive controls ship
- Forge outputs must load in the editor and runtime through shared engine APIs
- every supported format needs a valid and malformed fixture test
