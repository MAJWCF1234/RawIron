# Editor UI Smoke

Editor UI Smoke is the content-only Raw Iron editor regression experience. It deliberately has no
game-local C++ renderer, physics loop, or replacement runtime. When the manifest is mounted, the
engine-owned editor preview fallback imports `levels/assembly.primitives.csv` through
`RawIron.SceneUtilities`, keeps the shared editor camera/grid/lighting, and applies this project's
rendering and post-process scripts.

The scene is a compact visual checklist: a selected center subject, RGB transform axes, warm/cool
material swatches, and a large modal-frame landmark. Together with the authored UI flows and full
project contract, it exercises project mounting, viewport import, selection, inspectors, files,
Create/UI/Logic workbenches, keyboard focus, modal navigation, and playtest handoff.

Open it with:

```powershell
RawIron.Editor.exe --game=editor-ui-smoke
```

For automated validation, `RawIron.Games.BundledExperiencesSmoke` loads the manifest and runtime
support data, imports the primary assembly, verifies its transforms, and confirms the generic editor
preview contains the authored nodes.
