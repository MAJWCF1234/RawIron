---
tags:
  - rawiron
  - engine
  - tooling
  - camera
  - development
  - testing
---

# Automated Review / Scripted Camera Sequence

## Overview

The Automated Review / Scripted Camera Sequence is an optional development feature that drives a controlled, non-player traversal through a scene for inspection, review, and regression checking. Rather than representing real gameplay, it provides a predictable automated tour of the runtime that can be used to validate presentation, traversal, timing, and subsystem behavior.

This feature is intended for development and testing workflows only. It is not part of shipping gameplay and should be treated as tooling-oriented runtime support.

## Purpose

As Raw Iron scenes and systems grow in complexity, it becomes increasingly useful to have a repeatable way to move through a level or environment without requiring live manual input every time. A scripted review path allows developers to verify that a scene still loads, renders, animates, and behaves as expected under a known sequence of camera movements and timed actions.

The purpose of this feature is to support fast visual review and regression detection by providing a deterministic or semi-deterministic automated pass through a scene.

## Core Function

When enabled, the Automated Review / Scripted Camera Sequence takes control of the camera, and optionally a lightweight stand-in player or agent, and executes a predefined sequence of actions. Depending on the implementation, these actions may include:

- moving the camera through predefined waypoints
- pausing at review points
- rotating or framing important scene elements
- advancing through timed steps
- triggering simple interactions
- exercising loading boundaries or transition zones
- following a repeatable inspection route

This creates an automated review pass that can be used for development validation, scene demonstrations, capture workflows, or regression testing.

## Role in Raw Iron

Within Raw Iron, this feature should be treated as a development review and test-driving mechanism rather than gameplay logic. Its role is to provide a lightweight automation path for inspecting content and validating runtime behavior under a repeatable script.

It is especially useful for:

- visual scene review
- regression checking after engine changes
- traversal validation
- performance capture under controlled motion
- verifying culling, streaming, and animation behavior
- repeatable demo or showcase passes during development

This makes it a practical bridge between ad hoc manual testing and more formal automated validation systems.

## What It Is Not

This feature is not:

- real gameplay AI
- a shipping non-player character behavior system
- the main player controller
- a substitute for proper replay support
- a substitute for headless test automation
- a long-term foundation for gameplay scripting

It is a development automation aid for scene review and test traversal.

## Benefits

The Automated Review / Scripted Camera Sequence provides several important benefits:

### Repeatable Visual Inspection

It allows developers to revisit the same path through a scene consistently, making it easier to notice rendering regressions, lighting changes, missing assets, or broken traversal points.

### Faster Regression Checks

It reduces the need for repeated manual walkthroughs after every engine or content change.

### Controlled Demonstration Path

It provides a stable way to showcase a scene or subsystem during development without requiring live camera operation.

### Validation of Runtime Boundaries

Because the sequence can cross triggers, streaming regions, and visibility boundaries, it can help expose issues in culling, loading, collision, and transition logic.

## Relationship to Future Raw Iron Systems

In the long term, Raw Iron may replace or expand this kind of feature with more formal systems such as:

- headless runtime test runners
- replay playback systems
- integration test drivers
- scripted validation harnesses
- capture-oriented camera sequencing tools

For that reason, the Automated Review / Scripted Camera Sequence should be viewed as a useful development feature with clear boundaries, not as a permanent substitute for broader automation infrastructure.

## Design Considerations

Because this feature is development-oriented, it should remain:

- optional
- isolated from shipping gameplay code
- easy to enable or disable
- clearly separated from normal input and player control paths
- structured so it can later be replaced by more formal review or test systems

Keeping this boundary clean will prevent it from turning into engine-level “fake player” logic that becomes hard to maintain over time.

## Summary

The Automated Review / Scripted Camera Sequence is an optional development feature that performs a scripted traversal or inspection pass through a Raw Iron scene. Its purpose is to support repeatable visual review, regression checking, and controlled development demonstrations. It should be treated as tooling-oriented automation rather than gameplay functionality, with clear separation from the player controller and future formal test systems.

## Native Implementation

Implemented in **`RawIron.SceneUtilities`** as **orbit-rig scripting only** (no gameplay AI, no player controller takeover beyond camera framing).

### API (`RawIron/Scene/ScriptedCameraReview.h`)

- **`ScriptedCameraSequence`** — build steps with `AddWait`, `AddSnapOrbit`, `AddMoveOrbit`, `AddFrameNodes`, or load JSON via `TryParseScriptedCameraSequenceFromJson` / `TryLoadScriptedCameraSequenceFromJsonFile`. Optional `SetLoopPlayback` / `LoopPlayback` for repeating the whole sequence after the last step.
- **`ScriptedReviewEase`** — `moveOrbit` interpolation style: **`linear`**, **`smoothstep`** (default), **`easeInOut`** (quadratic). **Yaw** uses shortest-path blending so large spins do not take the long way around.
- **`ScriptedCameraReviewPlayer`** — `Start`, `Stop`, `Tick(Scene&, OrbitCameraHandles&, deltaSeconds)`; optional completion callback; **`SetLoopPlayback`** (and **`CompletedLoopCount`** when looping); **`SetStepBeganCallback`** for lightweight step tracing without pulling in engine logging.
- **`BuildDefaultStarterSandboxReview()`** — built-in tour aligned with `BuildStarterScene` node names (`Crate`, etc.).
- **`FindFirstNodeNamed`** — resolves `frameNodes` steps by `Node::name`.

### JSON shape (`formatVersion`: **1**)

Top-level optional **`"loop": true`** mirrors `ScriptedCameraSequence::SetLoopPlayback`. **`"steps"`** array; each object has **`"kind"`**: `"wait"` | `"snapOrbit"` | `"moveOrbit"` | `"frameNodes"`. Orbit payloads live under **`"orbit"`** with **`"target": { "x","y","z" }`**, **`"distance"`**, **`"yawDegrees"`**, **`"pitchDegrees"`**. **`moveOrbit`** accepts optional **`"ease"`** (`"linear"` \| `"smoothstep"` \| `"easeInOut"`). **`frameNodes`** uses **`"nodeNames": ["..."]`** and optional **`"padding"`**.

### Player integration

**`RawIron.Player`** accepts **`--scripted-camera`** (built-in sequence) or **`--scripted-camera=<path/to.json>`** — loads the JSON when valid, otherwise falls back to the built-in starter sequence and logs the error. Additional flags:

- **`--scripted-camera-loop`** — forces loop playback (in addition to JSON `"loop": true`, which is applied on load).
- **`--scripted-camera-verbose`** — logs each step entry (kind + index) to the standard log.

While the sequence **`IsActive()`**, **`AnimateStarterSceneOrbitPreview`** is skipped; crate/beacon animation still runs via **`AnimateStarterSceneProps`**. With loop playback enabled, **`Completed()`** stays false until **`Stop()`**; orbit idle motion resumes once playback stops and the scripted player is inactive.

This stays **outside** formal replay, headless CI drivers, and shipping gameplay logic—replaceable later by richer automation stacks.

## Related Notes

- [[Development Inspector]]
- [[05 Debugging and Instrumentation]]
- [[Testing]]
