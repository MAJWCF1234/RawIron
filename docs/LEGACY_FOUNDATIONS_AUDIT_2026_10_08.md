# Engine foundations audit, 2026-10-08

This slice continued multiplayer lifecycle work and inspected older shared engine
foundations. It is a focused audit, not a complete assessment of every subsystem.
All edits are general engine fixes; Aeroworld remains untouched.

## Network lifecycle: reproduced stale work after restart

Cube Test reproduced a delayed projectile command from a stopped session executing
after the same network module restarted. The new command queue was cleared, but
the pre-existing latency queue survived enabled reconfiguration. Peer approvals,
snapshot baselines, clocks and other session state also lacked a complete reset.

Session boundaries now retire delayed packets, queued commands, snapshot baselines,
peer approvals/cooldowns, prediction history, rewind history, join codes and clocks.
The runtime notifies its authority bridge of command tick zero as a new session;
the shared prop bridge resets its command budget even if the preceding session never
advanced. Last-session counters remain readable after shutdown and reset at startup.

Stopped modules no longer route packets. Shutdown attempts each network service and
session cleanup even if one service throws, then reports the first cleanup failure.

Evidence:

- Before: `build/legacy-session-before-tests.log` reports
  `old delayed command leaked into the new demo session`.
- After: Cube Test's command cadence test verifies no stale emission, no inherited
  snapshot remainder, stopped-send rejection, and failure-closed cleanup using an
  injected transport shutdown exception.
- Runtime netcode tests verify both server and client return to pending package
  agreement after restart, and the client cannot send gameplay before agreement.

This verifies local module reuse and authority state boundaries. Real-network
reconnect, per-entity ownership and persistent world recovery remain separate work.

## Scene graph: remove recursion from world transforms

The older world-transform cache recursively evaluated parent chains. Deep valid
hierarchies could exhaust the call stack; mutable node edits could create cyclic
chains. Reparent validation could also loop through an already-corrupt ancestor chain.

World transforms now collect unresolved ancestors iteratively and publish their cached
matrices from root to leaf. Traversal is bounded by scene size; invalid parents and
cycles produce `logic_error` diagnostics before publishing partial results. Root and
already-cached-parent paths remain allocation-free. Reparent checks terminate on
malformed chains, and repairing a parent does not duplicate existing child references.

`RawIron.Core.SceneHierarchySafetySmoke` checks a 12,000-node chain, cache hits and
invalidation, cycle diagnosis, invalid ancestors, reparent rejection and repair.
The cycle/invalid-parent cases are deliberately injected through mutable node access.
This does not replace a complete scene invariant validator or audit every child-list
consumer and recursive authoring traversal.

## Other inspected foundations

The source pass also sampled `RuntimeCore.cpp`, `JobSystem.cpp`, `FrameArena.cpp`,
`GameSimulationClock.cpp`, `JsonScan.cpp`, `LatencyTools.cpp` and `SnapshotReplication.cpp`.
Existing protections include attempted-module rollback and exception isolation,
job-fence failure propagation and cancellation, arena bounds/alignment checks,
finite frame-delta handling, latency byte ceilings and transactional baseline recording.
These observations are not proof that those files are defect-free. No speculative
rewrite or removal was performed.

Remaining audit priorities include child-list traversal/depth limits, extreme clock
values at network timestamp conversion, long-running worker/lifecycle stress, and
malformed asset inputs across loaders. Rendering quality remains separate work.

## Validation

```
cmake --build build/dev-msvc --config RelWithDebInfo --parallel 8
ctest --test-dir build/dev-msvc -C RelWithDebInfo --output-on-failure
git diff --check
```

Full build succeeded; all 173 tests passed in 52.73 seconds; whitespace checks pass.
Logs: `build/legacy-foundations-final-build.log` and
`build/legacy-foundations-full-tests.log`.

Actual native GPU capture: `Saved/visual_checks/legacy-foundations/morph-targets-16.bmp`.
The adjacent `comparison.json` records zero changed pixels out of 921,600 against
the prior fixed-pose capture. That establishes unchanged pixels for one demo frame,
not whole-engine rendering parity, performance or headset validation.

These checks ran on the local working tree before GitHub synchronization; they are
not a published release or a clean-checkout qualification.
