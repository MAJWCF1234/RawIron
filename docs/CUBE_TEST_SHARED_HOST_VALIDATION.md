# CubeTest shared desktop/XR validation — 2026-08-27

This pass completes five scoped tracker items: shared engine ownership, qualified haptics,
desktop/XR authority tests, repeatable room benchmarks, and desktop control validation.
It does not certify physical-headset comfort, full XR material parity or a release build.

## Implementation ownership

| Engine owner | Reusable implementation | Experience responsibility |
|---|---|---|
| `RawIron.Runtime` | `DesktopMouseLook`: raw input, focus loss, capture/release, angle limits | Supply the native window, authored sensitivity and camera angles |
| `RawIron.Trace` | Sampled keyboard input, jump edges, movement and collision | Supply bindings, movement tuning and authored colliders |
| `RawIron.World` | `InteractivePropGrab`: ray acquisition, ownership, movement and bounded throws | Supply the prop pool, hand/player owner and input ray |
| `RawIron.World` | `InteractivePropAuthorityBridge`: bounded commands, serialization and atomic two-pool updates | Bind the two pools and authored projectile emitter; declare session identity |
| `RawIron.XR.OpenXR` | `HardwareSceneBuilder`: atlas packing and native mesh conversion; `HapticPolicy`: output safety | Supply the shared scene and qualified interaction events |
| `RawIron.Render.Vulkan` | Successful-present interval callback | Select benchmark rooms and write reports |

The desktop and VR hosts no longer carry independent ray-grab/throw implementations. The game
authority adapter no longer contains a packet codec, and the VR app no longer contains its own
texture-atlas/mesh converter. No external library was added. Third-party source belongs under
`ThirdParty`; the original comparison images/models remain under CubeTest's assets tree.

## Behavior and regression coverage

**Desktop controls.** Existing focus-gated `HostInputService` feeds the engine keyboard adapter.
Deterministic tests cover directional axes, opposing keys, jump edges, focus suppression, walk vs.
sprint displacement, jump/landing and collision at sprint speed. Mouse capture releases only its
own confinement, discards pending deltas on focus loss and bounds pitch. Focus loss cancels pending
primary actions and releases carried props without inventing a throw. Home now resets position,
velocity, portal state and the authored start-room view, rather than just changing camera angles.
F2 logs feet/yaw/pitch/grab state for live checks.

**Grab/throw.** The shared engine tests exercise desktop and hand ownership, refusal to steal a
grab, first/moving samples, a 12 m/s throw cap, non-finite tracking and zero-velocity release on
tracking/focus loss. Remote authority clients cannot grab locally. Implementing networked grab
requests remains separate work; these clients can request projectiles through the authority plane.

**Haptics.** Hover no longer requests vibration. Explicit selection of a reachable prop and a
successful grab provide typed events. The engine gate rejects missing/invalid events, unfocused
or untracked input, invalid hand indexes and non-finite values. It caps amplitude at 0.35, pulse
duration at 50 ms, and cadence at one pulse per hand per 100 ms. Tests exercise the cap, cooldown,
independent hands and inert hover-like requests. Contact is an available qualified engine event;
this experience does not emit unqualified global collision pulses. No physical vibration was tested.

**Authority.** `RawIron.CubeTest.SharedAuthority` builds independent desktop/XR worlds and checks
all serialized bytes, immutable remote presentation, corrupt second-pool rollback, truncation,
trailing data, invalid channels/origins/directions, non-finite/overflow data, detached worlds,
four accepted commands per snapshot tick, same-tick rate preservation, and the 32-peer memory
bound with retirement on the next tick. Generic snapshots reject more than 1 MiB before copying.
The existing Runtime netcode suite additionally checks session agreement before commands/snapshots
and rejection of incompatible contracts. This is deterministic integration coverage, not a live
desktop-to-headset multiplayer session. Both hosts now avoid independently stepping remote prop
physics after authoritative snapshots arrive.

**XR scene conversion.** All static rooms are now resident. A startup-only 42-meter filter formerly
left distant rooms absent after portal travel. The engine builder test checks distant geometry,
dynamic-node exclusion, authored metalness/roughness/normal sign, and explicit missing-texture
diagnostics. The VR host aborts on atlas errors instead of silently showing untextured materials.

## Reproduce

From the repository root:

```powershell
& 'C:/Program Files/CMake/bin/cmake.exe' --build --preset build-dev-msvc --target RawIron.CubeTestGame RawIron.VRShowcase RawIron.CubeTest.WorldSmoke InteractivePropFieldSmoke OpenXrRuntimeSmoke MovementControllerDurationSmoke KeyboardFocusSmoke RuntimeNetcodeSmoke --parallel 6
& 'C:/Program Files/CMake/bin/ctest.exe' --test-dir build/dev-msvc -C RelWithDebInfo -R 'RawIron.CubeTest.|InteractivePropFieldSmoke|OpenXR.RuntimeSmoke|MovementControllerDurationSmoke|KeyboardFocusSmoke|Runtime.NetcodeSmoke|PortalTravelSmoke|AuthoringHelpersSafetySmoke|GltfExporterSmoke|NativeScenePreviewDetailNormalSmoke|ExtendedCompositeWiringSmoke|ReferenceShaderMigrationSmoke|FrameTuningSmoke|HybridPostProcessSafetySmoke' --output-on-failure
& ./Scripts/Benchmark-CubeTestRooms.ps1
& ./Scripts/Test-MaterialCalibration.ps1 -IncludeGallery
& ./build/dev-msvc/Apps/RawIron.VRShowcase/RelWithDebInfo/RawIron.VRShowcase.exe --probe-only --offline
& ./build/dev-msvc/Games/CubeTest/App/RelWithDebInfo/RawIron.CubeTestGame.exe --workspace-root=. --start-room=baseline --offline
```

Other targets in the test expression must already be built. This was an incremental targeted build,
not a clean full-workspace build.

## Recorded results

- Both hosts built in RelWithDebInfo. Build logs: `Saved/five-final-build.log` and
  `Saved/five-release-build.log`. **19/19 targeted tests pass**: `Saved/five-final-tests.log`.
- **14/14 benchmark launches completed**, covering seven rooms twice, with 30 warmup intervals
  and 120 measured intervals each. Raw CSVs, executable/shader hashes, arguments, driver details,
  JSON and Markdown: `Saved/benchmarks/cube-test/20260827-141230-691/report.md` and `report.json`.
  Mean CPU present intervals ranged from 14.23 to 14.86 ms; P95 ranged from 28.85 to 29.86 ms.
  The metric includes CPU work/present waiting and is **not GPU execution time or display FPS**.
  These were hidden 1280×720 direct Vulkan runs with normal animation/physics and no readback,
  on a desktop with other applications running. No performance threshold or VR budget is certified.
- **10/10 GPU regression checks pass**:
  `Saved/visual_checks/calibration/20260827-141323-968/report.json`. Existing calibration, direct/
  lightweight-hybrid luma controls and normal-room sky/floor checks remain green.
- Desktop executable SHA-256: `1C4259C4BCEF9391DB97AD7DE66C9F6EFFFDBE65316F1245B7C379E81886AA34`.
  VR executable SHA-256: `034FE7215D09D18AA7F3F0725472B37F6D9DD0DA6C62000C3B739B276AA2B932`.
- OpenXR discovery found an active runtime but **no available HMD**. The host exited with code 2
  and the expected diagnostic after constructing the shared 186-node world. Log:
  `Saved/five-vr-probe.log`. This is safe unavailability handling, not a passing headset session.
- Desktop launched visibly, with a rendered native scene and responsive window. Live F2 state
  recorded movement/view changes; the user's subsequent play traversed the gallery and returned
  through all twelve portal directions (`Saved/five-desktop-live.log`). Automated UI input stopped
  when user input was detected. The session subsequently exited through normal shutdown, saving
  its pipeline cache and final camera state with no failure diagnostic. It was not reopened over
  the user's interaction. Jump/sprint/collision/grab edge cases above were
  checked by deterministic tests; a fully scripted manual walkthrough of every control is not claimed.

## Remaining limits

Physical headset input, haptic feel, stereo quality and comfort remain unverified. XR camera-facing
sprites, general scene-animation streaming, embedded KTX2/Basis materials and packed ORM sampling
remain open. Dark/poorly framed normal fixtures, shadow-edge quality, cooked packages and approved
golden captures also remain open. Extended-post GPU validation is still incomplete as documented in
[the gallery report](CUBE_TEST_GALLERY_VALIDATION.md#normal-room-clipping-follow-up).

Logs, captures and generated reports under `Saved` are local evidence excluded from Git. Source
tests, benchmark scripts and these reproduction commands are checked in with the implementation.
