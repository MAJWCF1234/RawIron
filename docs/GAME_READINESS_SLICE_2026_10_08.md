# General engine readiness slice, 2026-10-08

The full configured workspace builds and all 171 registered tests pass.
Aeroworld was inspected read-only for requirements; its rebuild has not started.
No game-specific engine feature or Aeroworld integration was introduced.

## Changes

- Restore `ri_tool` compilation: reinstate `BuildPartFromCommandLine` and use
  `ExtractTranslation` for the engine's 4x4 rig transforms.
- Preserve authored primitive length through structural graph construction and
  assembly spawning. Previously a 2.4-metre helix reverted to default length.
- Verify structural assemblies in world space under translation, rotation and
  nonuniform scale, preserving UVs within a 1e-6 tolerance. Local mesh storage
  must not be mistaken for a missing world transform.
- Decode CLI test output as UTF-8 and update stale help expectations to the
  current documented verbs and authoring folders.
- Add `RawIron.Tool.BlockCharacterAuthoringSmoke`: create a model, add a shaped
  part, reload the saved dimensions/bone/shape, reject missing bones, duplicate
  IDs and invalid extents, and verify rejected edits preserve the saved bytes.
  Its output is confined to the build test workspace.

## Validation

```
cmake --build build/dev-msvc --config RelWithDebInfo --parallel 8
ctest --test-dir build/dev-msvc -C RelWithDebInfo --output-on-failure
git diff --check
```

The build succeeded. All 171 tests passed in 74.47 seconds. Whitespace checks pass.
Build evidence: `build/game-readiness-slice-final-build.log`.
Suite evidence: `build/game-readiness-slice-final-tests.log`.
These are local working-tree checks, not a clean-checkout release or published build.
Rendering artifacts and the 200 FPS target remain separate unresolved work.

## Requirements guiding subsequent shared-engine work

Inspected sources: `N:/Aeroworld/package.json` and selected multiplayer handlers
in `N:/Aeroworld/server.js`. No game code was changed, launched or converted.
This is a preliminary requirements sample, not a complete game audit.

Observed interactions include joining worlds, movement updates, combat requests,
prop-state batches, dimension changes, portal edits, save/load and chat. Its movement
handler accepts clamped client positions and broadcasts them; a native rebuild
should use validated inputs and server-owned simulation instead.

RawIron already has `IAuthoritativeSimulationBridge`, snapshot delta/baseline
handling, package agreement, transport budgets and authoritative prop replication.
Those foundations do not establish complete multiplayer-game readiness.

Next general multiplayer slices should verify:

1. Fixed command/simulation cadence distinct from snapshot cadence;
   `network_tick_hz` currently remains explicitly unbound.

   Subsequent [command cadence work](COMMAND_CADENCE_VALIDATION.md) binds this key
   to authority command dispatch. Game simulation stepping remains separate.
2. Server-owned entity/prop authority and validated action requests, including
   ownership rejection and disconnect cleanup.
3. Late join, reconnect and full resynchronization under loss/reordering, with
   package compatibility checked before gameplay.
4. Shared player prediction/reconciliation and interpolation integration rather
   than only the legacy demonstration position stream.
5. Replicated world transitions and durable state boundaries. Domain rules and
   content remain game-owned; reusable scheduling/replication belong to the engine.

Use existing native showcases and fault-injection tests for these capabilities
before considering the Aeroworld rebuild.
