# Raw Iron Native Showcase Roadmap

## Purpose

Cube Test and the PCVR Showcase will become Raw Iron's living proof that the engine can meet and exceed the useful capabilities demonstrated by the Three.js example collection.  The reference collection is an input to an engine-validation programme, not a codebase to port.

The finished experience is a connected, walkable showcase: authors travel through portal-linked zones, inspect a feature, interact with it, and can run the same authored world on desktop or in PCVR.  Every zone is built through reusable Raw Iron systems so a game can use the capability without depending on Cube Test.

## Non-negotiable rules

- No JavaScript, WebGL renderer, Three.js runtime, or copied Three.js implementation code ships in Raw Iron.
- Implement the capability in native C++ in the engine subsystem that owns it.  A game zone only declares and composes that feature.
- Use the corresponding Three.js example assets when their licence and distribution permit it.  Preserve source, version, file list, licence, and attribution beside the fixture.
- Game-ready reference content is project-scoped and cooked into declared packages; it is not a permanent loose engine asset library.
- Desktop and PCVR share world, material, physics, interaction, portal, and replication contracts.  VR may have a specialized presentation/input host, but not a separate game simulation.
- A demo is not complete because it opens.  It needs deterministic smoke coverage, hardware visual review, an interaction check where applicable, and a documented performance result.

## What “better than the reference” means

Raw Iron does not need to imitate browser UI or duplicate a rendering demo pixel-for-pixel.  It must provide a stronger authoring result:

| Reference quality | Raw Iron completion standard |
|---|---|
| A standalone web example | A reusable C++ engine feature with game-facing data and documentation |
| Browser asset request | Validated project package, provenance, cooker, and explicit fallback behaviour |
| Mouse-only interaction | Shared desktop and tracked-controller interaction where the feature benefits from it |
| One rendered frame | Automated smoke plus captured hardware review on supported GPU and headset lanes |
| Isolated scene | Portal-linked world zone with navigation, collision, reset, and readable fixture labels |
| Demo-specific state | Deterministic, bounded runtime state that can participate in replication when relevant |

“Better” never means adding visual effects to hide a wrong renderer.  Correct material binding, color handling, depth, normals, transforms, and stable frame pacing come first.

## Delivery model

Each example is evaluated before it is accepted into the showcase.  The inventory covers the entire Three.js `examples/` collection, but examples are not all copied one-for-one: variants which prove the same engine capability are grouped into one well-authored Raw Iron zone.  Every source example still receives an inventory decision: implemented, grouped under another zone, deferred with a reason, or not applicable to a native engine.

The inventory source must be an identified local Three.js checkout or an explicitly identified, licensed archive.
The pinned r185 audit list is maintained in [the generated example inventory](THREEJS_EXAMPLES_INVENTORY_R185.md).
The Raw Iron-owned fixture directory and `asset-manifest.json` prove asset provenance for selected fixtures, but they
do not by themselves prove that every upstream example has been audited. If the checkout is unavailable, the
coverage claim stays open and the missing source is recorded as a blocker.

### Example intake record

For each reference, record:

1. Example path, Three.js revision, goal, and source asset list/licence.
2. The Raw Iron engine owners: renderer, content pipeline, world, physics, input/XR, networking, audio, or tools.
3. The native capability to create or extend.  If it is missing, build it in that owner rather than adding a Cube Test-only workaround.
4. The authoring surface: primitive/component/material/package declaration and editor exposure.
5. Desktop, PCVR, headless, and hardware-validation requirements.
6. Portal-zone placement, player-facing explanation, reset route, and performance budget.
7. Final result: pass, grouped, deferred, or rejected with evidence.

### Completion states

- **Candidate** — catalogued, not yet audited.
- **Audited** — scope, assets, licence, and Raw Iron owner decided.
- **Foundation** — reusable engine API exists; a scene may not yet prove it.
- **Zone prototype** — native gallery scene exercises the API.
- **Validated desktop** — exact RelWithDebInfo executable, smoke checks, and hardware capture agree.
- **Validated PCVR** — same world is comfortable and correct on a real headset path.
- **Showcase ready** — provenance, controls, performance, failure cases, and docs are complete.

## Showcase layout

The experience remains one cohesive world rather than a launcher full of unrelated samples.

```text
Arrival / renderer confidence room
  ├─ Materials, lights, geometry, textures, normal conventions
  ├─ Sprites, text, animation, particles, and environment zones
  ├─ Content corridor: import, compressed assets, export, packages
  ├─ Interaction arena: traces, grabbing, projectiles, physics, audio
  ├─ Rendering lab: post-processing, shadows, probes, volumetrics
  └─ PCVR wing: shared multiplayer-ready world, locomotion, hands, comfort
```

Portals are engine world links with an authored destination, safe arrival volume, visual/readability treatment, and a testable traversal contract.  They are not bespoke level-script teleports.

## Current starting fixtures

These are foundations already represented in Cube Test.  They still require the validation status stated in the TODO document; they are not all visual-quality sign-offs.

| Three.js reference | Raw Iron native proof | Current state |
|---|---|---|
| `css3d_sprites.html` | Camera-facing sprite batch | Zone prototype; desktop/PCVR visual validation required |
| `misc_exporter_gltf_normals.html` | OpenGL/DirectX normal convention fixture and native export checks | Zone prototype; hardware material validation required |
| `misc_exporter_gltf.html` | Compressed glTF import, textured native export, packaged reference fixtures | Zone prototype; package and visual validation required |
| `webgl_materials_normalmap.html` | Tangent-space normal-mapped Lee Perry-Smith fixture | Zone prototype; hardware material validation required |
| `webxr_xr_cubes.html`, `webxr_xr_dragging.html` | Bounded props, ray selection, exclusive grab ownership | Foundation/zone prototype |
| `webxr_xr_haptics.html`, `webxr_xr_ballshooter.html` | Contact-qualified haptics, projectile pool, impulse targets | Foundation/zone prototype |
| `webxr_vr_teleport.html` | Parabolic target, slope and standing-volume validation | Foundation/zone prototype |

The exact assets and their provenance are recorded in `Games/CubeTest/assets/reference/threejs-r185/README.md`.

## Work phases

### Phase 0 — establish a renderer truth baseline

Before expanding the gallery, prove the native Vulkan presentation path with a small controlled fixture set: unlit color, sRGB albedo, normal map, metallic/roughness, transparency, shadow/depth, and a known glTF fixture.  Capture the output from the exact RelWithDebInfo binary on real hardware, preserve logs/settings, and compare it to expected swatches and geometry.  Headless tests are necessary but cannot certify GPU pixels.

### Phase 1 — foundation gallery

Finish the baseline materials, primitives, portals, movement, sprite, normal-map, and import/export rooms.  Give every zone an authored data manifest and a clean reset path.  Move repeated fixture construction into engine/content APIs.

### Phase 2 — content, geometry, and animation

Audit loader, exporter, geometry, skinning, morph target, animation, instancing, and LOD examples.  Add only standards-backed native import/export features that fit Raw Iron's content architecture.  Validate source assets and cooked packages separately.

### Phase 3 — rendering and environments

Cover lights, shadows, PBR materials, texture encodings, render targets, post-processing, sky/environment, reflection/probe, particles, and volumetric techniques.  Each rendering feature needs a disabled/baseline control and a known-good fixture so quality regressions are visible rather than subjective.

### Phase 4 — world interaction and game systems

Cover ray queries, collision, character movement, audio, UI/text, physics constraints, navigation, AI-facing world queries, and bounded dynamic simulation.  These zones should demonstrate author workflows rather than only renderer tricks.

### Phase 5 — shared desktop and PCVR

Finish the shared authority contract and validate cross-play: desktop movement and HL2-style controls; tracked-controller locomotion, smooth turning, snap-turn option, selection/grab, haptics only on qualified contact, and comfort-safe portal travel.  PCVR-specific visualization belongs in `RawIron.XR.OpenXR`; world semantics remain engine-wide.

### Phase 6 — packages and extensions

Demonstrate generic, signed/validated package mounting and hot extension lifecycle without making the feature sandbox-specific.  A future Shadertoy-compatible extension is a good generic stress case: it must be an engine extension with clear security, determinism, rendering, asset, and multiplayer policy—not an engine-defined game mode.

Current foundation (2026-09-04): manifest v2, dependency resolution, atomic mount/reference-count management, package fingerprints, and opt-in network preflight already exist in the engine. Continue from those APIs. Native-module state migration/unloading, publisher authentication, package acquisition and end-to-end showcase session validation remain uncompleted gates. See [package runtime](../Documentation/02%20Engine/12%20Package%20Runtime.md) and the [maintenance report](ENGINE_MAINTENANCE_2026-09-04.md).

### Phase 7 — final showcase and regression programme

Publish a coverage matrix for every Three.js example, package the complete playable gallery, capture desktop and PCVR evidence, run clean-machine build/test lanes, and keep every completed zone under regression coverage.

## Quality gates

No zone advances to **Showcase ready** without all of the following:

- Native C++ engine feature is located in the appropriate reusable subsystem.
- No Three.js source implementation is compiled, embedded, or executed.
- Asset origin, licence, version, and cooked package membership are documented.
- Exact launch command, build configuration, and scene start point are documented.
- Unit/contract tests cover data and simulation; a smoke test covers loading and traversal.
- Hardware capture verifies the actual GPU output, including correct color and material response.
- Where relevant, a real-headset run verifies stereo, input mapping, haptics policy, collision, and comfort.
- Failure behaviour is deliberate: missing asset, unsupported format, unavailable headset, and malformed package produce actionable diagnostics rather than corrupted imagery or silent fallbacks.
- Performance budget is recorded for the intended target class and regression-tested where practical.

## Asset policy

Vendored libraries and third-party code belong under `ThirdParty/<dependency>`, with their licenses
and version provenance; see [ThirdParty policy](../ThirdParty/README.md). Experience comparison
images and models belong in the experience's assets tree, not beside library source.

The upstream checkout is disposable. Copy every accepted asset and its required sidecars/licenses
into the owning experience's `assets` tree before using it. Never author runtime paths, symlinks,
or build/test dependencies back into the checkout. Record upstream-relative paths and hashes for
comparison; these are provenance, not runtime locations. Cube Test's current physical copies live
under `Games/CubeTest/assets/reference/threejs-r185` and are verified by `RawIron.CubeTest.ReferenceAssets`.

Reference assets remain isolated under the Cube Test project with their upstream licence.  Source assets are kept only when needed for authoring/validation; distributable builds declare cooked `.ripak` packages and load the required ranges directly.  The engine does not accumulate a global “example texture folder.”  Any new fixture must state whether it is: upstream reference content, Raw Iron-authored content, generated test data, or a third-party dependency.

## Outcome

The goal is not a claim that Raw Iron “has every Three.js demo.”  The goal is a testable, native, game-authoring-quality engine whose public showcase makes feature coverage, limitations, performance, and next work obvious.  If a Three.js example reveals a missing useful capability, that capability is added to Raw Iron correctly—or the inventory records why it is intentionally out of scope.

### Normal mapping comparison increment — 2026-09-04

The normal-map example now exercises a shared native six-panel comparison builder,
mirrored UVs, DirectX conversion and an intentional wrong-convention control.
Renderer fixes and analytic GPU/CPU tests are documented in
[normal mapping validation](NORMAL_MAPPING_COMPARISON_VALIDATION.md). Full upstream
visual parity and release readiness remain separate gates.
