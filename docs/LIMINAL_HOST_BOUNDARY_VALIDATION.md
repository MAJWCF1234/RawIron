# Liminal Hall engine/game boundary, 2026-10-08

Liminal Hall now uses the same engine-owned prepared-project boot path as Cube Test.
Its runtime target links `RawIron::GameHost` directly instead of the compatibility
diagnostics target name.

## Ownership

| Responsibility | Owner |
|---|---|
| Resolve manifest, validate project/config, check executable module identity | RawIron.GameHost |
| Prepare manifest, runtime support and script snapshot | RawIron.GameHost |
| Construct RuntimeCore and register prepared services | RawIron.GameHost |
| Resolve runtime/save paths, including checkpoint location override | RawIron.GameHost |
| Parse audio settings from prepared scalars | RawIron.Content |
| Hall world, authored content, gameplay, UI flows, plugin choices and controls | Games/LiminalHall |
| Native presentation/input bindings and per-frame game callback | Games/LiminalHall, using shared renderer/runtime APIs |

This is shared engine boot and services, not dynamic game-module loading through
the generic Player. Liminal Hall's gameplay and authored layout stay game-owned.

## Corrected paths

Desktop and headless launch previously duplicated manifest lookup, project-format
validation and runtime construction. Game initialization created audio before
validating tuning, loaded scripts again, and returned a failure value callers ignored.

Both now prepare through GameHost before game setup. Initialization consumes the
prepared script snapshot; audio parsing gained an in-memory overload so it uses
that snapshot too. Runtime service registration retains the same scripts object.
The software benchmark also uses shared preflight.

`GameProjectBootOptions.checkpointStorageRoot` preserves save-location overrides.
The app now forwards actual launch arguments in headless mode as well as desktop
mode. Simulation attachment/frame failures propagate as launch failure; a failed
headless frame cannot be reported as a successful capture.

Game event bridges detach during state destruction, so their RuntimeCore/event bus
outlives game state. The headless regression caught an incorrect destruction order
during the refactor; the final order is verified by rendering, shutdown and process exit.

## Validation

- `RawIron.GameHost.ProjectBootSmoke`: Liminal module preparation, custom save path
  and prepared-script service identity, alongside existing Cube Test lifecycle checks.
- `RawIron.LiminalHall.ProjectBootSmoke`: actual desktop/headless entry points reject
  another game's module and malformed numeric tuning; a valid headless launch renders
  a saved frame and exits. Invalid fixtures are copied into a temporary test directory;
  author-owned game files are never changed.
- Existing semantic structural, material-audit and rendering-script checks pass.
- Full configured workspace build succeeds; all 174 tests pass in 53.45 seconds.
  Whitespace validation passes.

Logs: `build/liminal-host-full-build.log`, `build/liminal-host-full-tests.log`, and
`build/liminal-host-boundary-target-tests.log`.

An additional actual CLI run succeeded with `--headless --headless-frames=2
--width=256 --height=144 --no-autoplay --offline`. Its software capture and log are
`Saved/Validation/liminal-host-boundary/headless.bmp` and the adjacent `headless.log`.
The software preview scaling produces a 128x72 image. This verifies startup/render/
shutdown, not native GPU visual quality or performance.

Validation ran on the local working tree before GitHub synchronization; no release was published. Editor preview continues using the game-owned
world builder; native input/presentation bindings were not generalized by this slice.
