# Authority command cadence, 2026-10-08

Authority bridge commands now execute at fixed engine command boundaries instead of
immediately on packet receipt. This clock is independent of snapshot cadence and
does not step game physics. The game continues owning simulation integration.

## Engine behavior

- `AuthoritativeNetConfig.tickRate` controls command dispatch, clamped to 1..240 Hz.
- Host scripts bind `network_tick_hz`; `--net-tick` takes precedence.
- `network_snapshot_rate` / `--server-tick` retain separate snapshot scheduling.
- The host queue permits at most 128 commands, 16 per peer and 1 MiB of payload.
- Each command boundary dispatches at most 32 requests, with at most four catch-up
  boundaries per frame. Nonfinite/negative deltas do not advance the clock.
- Departed or no-longer-agreed peers lose queued commands; protocol cooldown is
  checked again before dispatch. Control/session/snapshot traffic keeps its existing
  routing rather than being delayed behind gameplay requests.
- `IAuthoritativeSimulationBridge.OnCommandTick` runs before command dispatch.
  `InteractivePropAuthorityBridge` resets domain budgets at these boundaries;
  `CaptureSnapshot` no longer changes command budgets.
- Accepted/rejected/received command events carry the command tick. Network metrics
  report tick count, dispatched requests, queued packets/bytes and resource/stale drops.
- Shutdown clears queued work. Legacy unbridged diagnostic command events retain
  their immediate route; gameplay should use the authority bridge contract.

Direct users of the prop bridge must invoke `OnCommandTick` to advance its command
budget. Capturing snapshots is now a read-only operation for that budget.

## Demo experience verification

`RawIron.CubeTest.CommandCadenceSmoke` uses the actual Cube Test world, projectile
emitter and shared prop authority bridge through `AuthoritativeNetModule`, with an
injected transport. It verifies:

1. At 16 Hz commands / 64 Hz snapshots, snapshots continue before the first command
   boundary and queued projectile requests do not mutate the world early.
2. Four projectile requests succeed at a boundary; the fifth is rejected. Additional
   snapshot captures cannot replenish the budget before the next command boundary.
3. Queued requests from a disconnected peer are retired without spawning projectiles.
4. A 64-request burst retains 16 requests and drops 48; dispatch still respects the
   domain's four-per-tick limit.
5. A long hitch permits at most four command boundaries; NaN/negative deltas add none.
6. One second of regular frames produces 16 command ticks and 64 snapshot broadcasts.
7. The 1 MiB queued-byte ceiling rejects an overflowing request. Malformed queued
   data does not mutate the demo, and shutdown removes pending valid work.

The existing `RawIron.CubeTest.SharedAuthority` check also verifies that changing
snapshot ticks cannot reset a command budget. `RawIron.Games.TuningContractSmoke`
checks script values and CLI precedence for both rates.

## Results and reproduction

```
cmake --build build/dev-msvc --config RelWithDebInfo --parallel 8
ctest --test-dir build/dev-msvc -C RelWithDebInfo --output-on-failure
```

The full build succeeded; all 172 registered tests passed in 50.24 seconds.
Logs: `build/command-cadence-full-build.log` and `build/command-cadence-full-tests.log`.
The expanded byte-budget/shutdown demo assertions were then built and passed in an
additional targeted run; build log: `build/command-cadence-byte-check-build.log`.
Whitespace validation passes. Validation ran on the local working tree before GitHub synchronization; no release was published.

This demonstrates engine scheduling and real demo state changes through an injected
transport. It does not establish remote-session performance, headset comfort,
player input prediction/reconciliation, reliable command acknowledgements, reconnect
recovery, ownership leases, or durable world state. No Aeroworld code was modified.

The next shared slice should validate action ownership and reconnect/resynchronization
using the existing native demo experiences before starting a game rebuild.

The subsequent [foundations audit](LEGACY_FOUNDATIONS_AUDIT_2026_10_08.md)
reproduces and fixes delayed work surviving a module restart, clears session
approval/baseline state, and validates scene hierarchy safety.
