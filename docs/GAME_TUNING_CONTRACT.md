# Game tuning contract

This contract pass uses the existing Liminal Hall, Wilderness Ruins, Multiplayer Sandbox,
and Cube Test projects. It does not introduce a separate showcase.

## Movement requests

The four desktop hosts resolve `scripts/physics.riscript` through the shared
`RawIron.GameHost` physics adapter after their gameplay defaults are applied:

| Setting | Applied behavior |
| --- | --- |
| `movement_gravity` | Base player gravity; takes precedence over legacy gameplay gravity |
| `movement_fall_gravity_multiplier` | Descending gravity multiplier |
| `global_gravity_scale` | Multiplies base player gravity |
| `global_jump_scale` | Multiplies the authored player jump speed |
| `global_drag_scale` | Multiplies player ground friction |
| `global_air_control_scale` | Multiplies player air control, capped at 1 |

The `global_*` names apply to the player movement binding in this pass. They do not
yet scale every prop/rigid body or XR locomotion. The resolved baseline is retained
before local volume modifiers are applied, preventing repeated scale multiplication.
Startup reports the effective movement values.

## Network requests

Multiplayer Sandbox's interactive and headless hosts and Cube Test's desktop/VR
authority configuration apply these settings from `scripts/network.riscript`:

| Setting | Applied behavior | Explicit CLI override |
| --- | --- | --- |
| `network_tick_hz` | Fixed authority command dispatch per second | `--net-tick` |
| `network_snapshot_rate` | Authority snapshot broadcasts per second | `--server-tick` |
| `network_max_clients` | Transport peer capacity | `--max-peers` |

The existing net module calls its snapshot cadence `serverTickRate`. This does not
set the game's simulation clock. Explicit CLI options take precedence over scripts;
absent script keys retain host defaults. Offline mode remains a host choice.

`network_tick_hz` now schedules host commands delivered to a mounted simulation
bridge. It is independent of snapshot cadence and does not step game physics.
Domain bridges receive `OnCommandTick` before dispatch, so command budgets no longer
reset when a snapshot is captured. Runtime queues and catch-up work are bounded;
see [command cadence validation](COMMAND_CADENCE_VALIDATION.md).
Timeout, prediction-window and interpolation requests remain unbound.

## Validation and unsupported requests

Balanced and Strict hosts reject malformed numeric assignments and duplicate keys
with a path, key, and source line. Permissive hosts report them as warnings.
Schema validation rejects non-finite values even for maps constructed in memory.
Numeric getters use their fallback for a non-finite value.

File validation and capability binding are reported separately. Current showcase
hosts explicitly report unbound streaming-budget, automatic checkpoint saving,
persistence-policy, and unsupported network requests. Checkpoint resume support
does not imply an automatic checkpoint writer is mounted.

Editor scaffolding now emits numeric, schema-compatible tuning defaults. Project
identity remains in `manifest.json`. Existing authored files are not overwritten.

## Verification

`RawIron.Games.TuningContractSmoke` covers parser diagnostics, non-finite maps,
actual movement simulation under changed gravity/jump scales, network precedence,
all four project profiles, and the Balanced malformed-file gate.
`RawIron.Editor.ScaffoldContractSmoke` checks a generated project against Strict
validation and checks preservation of existing authored settings.

These checks establish configuration behavior, not GPU visual quality or 200 FPS.
