# Logic entities — engine-agnostic specification (sheet)



**Realized form:** A small **directed event graph** over **named logic nodes**. The world (triggers, UI, scripts) sends **inputs** to node ids; nodes emit **outputs** as events with optional payloads. No renderer, audio, or physics is implied — only state transitions and fan-out.

---

## Authoring vision: logic as 3D structures on an invisible layer

The same graph can be authored as **physical 3D objects in the level editor**, not as code or flat files. Think of a **dedicated logic layer** that sits in the same world space as the map but is **invisible at runtime** (or only visible in debug). Designers work inside the real geometry—doorways, halls, props—while placing and connecting logic **in situ**.

**Blocks.** Each logic node (relay, timer, counter, compare, and later custom kinds) is a **small volumetric block** placed in the world. Each block carries a **hovering label** (name + kind) so you can read the circuit from a distance. Blocks can snap to grids or surfaces; their position is for **human readability and teamwork**, not for simulation—the runtime still keys everything by stable ids.

**Wires.** Connections are **stretchy, draggable cables** between **ports** on those blocks (one port per input or output family, or a unified “socket” UI that expands). You drag from an output port to an input port; the editor stores the edge. Wires can **route through space** (spline or polyline) so you can trace power flow along corridors, under floors, or across voids—like laying conduit in the actual level. Delays and simple parameters can be **inspector fields** on the wire or the target port.

**Bridging into the playable map.** Triggers, doors, lights, spawners, and UI panels expose **the same port vocabulary** on the **visible** layer. A pressure pad in the world is not a separate scripting language; it is a **mesh or volume that owns output ports**. You drag a wire from `OnStepped` on the pad to `Trigger` on a relay, then from the relay to `Open` on a door. The **physical map** and the **logic layer** share one coordinate system: you see where cause and effect live relative to the architecture.

**Audience.** The goal is **3D scripting for people who do not program**: if you can arrange blocks and plug cables in the real space of the level, you can build **gated progression, timed sequences, counters for objectives, and comparison checks** (e.g. “when this value is in range, enable the exit”). The formal tables below remain the **contract** those blocks compile into; the editor is the **spatial, tactile** front end.

---

## 1. Core concepts

| Concept | Definition |
|--------|------------|
| **Node** | Stable string `id`, a `kind`, internal state, and a ruleset for inputs → state change → outputs. |
| **Edge** | `(sourceId, outputName) → list of { targetId, inputName, delayMs?, parameter? }`. |
| **Context** | Opaque bag passed along an activation (e.g. `sourceId`, player id, cause). Implementations may merge or filter. |
| **Clock** | Monotonic time source for delays; max delay should be capped (e.g. 24h) for sanity. |

---

## 2. Node kinds (behavioral contract)

### 2.1 `logic_relay`

| Field | Type | Notes |
|-------|------|--------|
| `id` | string | Required. |
| `startEnabled` | boolean | Default true. Disabled relays ignore `Trigger`. |

**Inputs** (names case-insensitive):

| Input | Effect |
|-------|--------|
| `Trigger` | If enabled, emit outputs on `OnTrigger`. |
| `Enable` / `TurnOn` | Set enabled. |
| `Disable` / `TurnOff` | Set disabled. |
| `Toggle` | Flip enabled. |

**Outputs:**

| Output | When |
|--------|------|
| `OnTrigger` | After successful trigger. |

---

### 2.2 `logic_timer`

| Field | Type | Notes |
|-------|------|--------|
| `id` | string | Required. |
| `intervalMs` | number | ≥ 0, capped (e.g. ≤ 86_400_000). Repeating timers clamp to ≥ 1 ms. |
| `repeating` | boolean | If true, reschedule after each tick. |
| `autoStart` | boolean | Start on spawn if enabled. |
| `startEnabled` | boolean | Default true. |

**Inputs:**

| Input | Effect |
|-------|--------|
| `Trigger` / `Start` | Start (replace in-flight run). |
| `Reset` | Cancel then start fresh. |
| `Stop` / `Cancel` / `CancelTimer` | Cancel; no `OnFinished` from cancel path. |
| `Enable` / `TurnOn` | Enable; if `autoStart`, start. |
| `Disable` / `TurnOff` | Disable and cancel. |
| `Toggle` | If active → cancel; else → start. |

**Outputs:**

| Output | When |
|--------|------|
| `OnTimer` | Each time the interval elapses (including each repeat). |
| `OnFinished` | After a **non-repeating** timer fires once (after `OnTimer` for that run). |

**Concurrency:** Starting again while active should invalidate the previous scheduled token so only one logical run is live per id (unless the product explicitly supports multi-arm timers).

---

### 2.3 `logic_counter`

| Field | Type | Notes |
|-------|------|--------|
| `id` | string | Required. |
| `startValue` | number | Initial `value`. |
| `minValue` / `maxValue` | number or unbounded | Clamp `value`; if min > max, swap. |
| `step` | number | Default 1; clamp to sensible range (e.g. 1 … 1_000_000). |
| `startEnabled` | boolean | Default true. |

**Inputs:**

| Input | Effect |
|-------|--------|
| `Trigger` / `Increment` / `Add` | Add `parameter ?? step` to value (clamped). |
| `Decrement` / `Subtract` | Subtract `parameter ?? step`. |
| `SetValue` / `Set` | Set to `parameter` (clamped). |
| `Reset` | Set to `startValue`; **must** emit change outputs even if value unchanged (authoring “pulse”). |
| `Enable` / `Disable` / `Toggle` | As usual. |

**Outputs** (payload should include `counterValue`, `counterDelta` where relevant):

| Output | When |
|--------|------|
| `OnChanged` | Value changed (or forced on reset). |
| `OnIncrement` | Delta &gt; 0. |
| `OnDecrement` | Delta &lt; 0. |
| `OnZero` | Value became 0 from non-zero. |
| `OnHitMin` | Value reached finite `minValue`. |
| `OnHitMax` | Value reached finite `maxValue`. |

---

### 2.4 `logic_compare`

| Field | Type | Notes |
|-------|------|--------|
| `id` | string | Required. |
| `sourceLogicEntityId` | string? | Read `sourceProperty` from that logic node (e.g. counter’s `value`). |
| `sourceWorldValue` | string? | Alternative: read from a global key/value store. |
| `sourceProperty` | string | Default `value`. |
| `equalsValue` | number? | Equality test if set. |
| `minValue` / `maxValue` | number? | Inclusive range if both; else one-sided bound. |
| `value` | any? | Constant observed value if no external source. |
| `evaluateOnSpawn` | boolean | Run once when created. |
| `startEnabled` | boolean | Default true. |

**Observation:** Coerce observed value to number when possible; boolean coercion `!!observed` only if no numeric bounds apply.

**Inputs:**

| Input | Effect |
|-------|--------|
| `Trigger` / `Evaluate` / `Compare` | Recompute result. |
| `Enable` / `Disable` / `Toggle` | As usual. |

**Outputs:**

| Output | When |
|--------|------|
| `OnTrue` / `OnFalse` | Every evaluation (current result). |
| `OnBecomeTrue` / `OnBecomeFalse` | Only when result **changes** from previous evaluation. |

Payload should include `compareValue`, `compareResult`.

---

### 2.5 `logic_sequencer`

Cycles or steps through a fixed list of **output slots** each time it receives an advance pulse.

| Field | Type | Notes |
|-------|------|--------|
| `id` | string | Required. |
| `stepCount` | number | How many outputs (or explicit `steps[]` with per-step delayMs). |
| `wrap` | boolean | After last step, return to first vs stop. |
| `startEnabled` | boolean | Default true. |

**Inputs:** `Trigger` / `Advance` (next step), `Reset` (index → 0), `Enable` / `Disable` / `Toggle`.

**Outputs:** `OnStep` (every step, payload `stepIndex`), `OnStep0` … `OnStepN` (optional named fires), `OnComplete` (if `wrap` false and finished).

---

### 2.6 `logic_pulse` (one-shot / pulse extender)

Turns an edge into a **held** output for a duration, or stretches a short pulse.

| Field | Type | Notes |
|-------|------|--------|
| `id` | string | Required. |
| `holdMs` | number | How long `OnActive` stays true after a trigger. |
| `retrigger` | enum | `extend` (reset timer on new pulse), `ignore`, `restart`. |
| `startEnabled` | boolean | Default true. |

**Inputs:** `Trigger` / `Pulse`, `Cancel`, `Enable` / `Disable` / `Toggle`.

**Outputs:** `OnActive` (while held), `OnRise`, `OnFall` (edges).

---

### 2.7 `logic_latch`

Stable **boolean memory** (set / reset / toggle).

| Field | Type | Notes |
|-------|------|--------|
| `id` | string | Required. |
| `startValue` | boolean | Initial state. |
| `mode` | enum | `sr` (separate Set/Reset), `toggle` (one input flips), `priority` (dominant Set or Reset if both same tick—define policy). |

**Inputs (SR mode):** `Set`, `Reset`, `Toggle` (optional), `Enable` / `Disable`.

**Outputs:** `OnTrue`, `OnFalse`, `OnChanged` (payload `latchedValue`).

---

### 2.8 `logic_channel` (wireless / long-distance bus)

**Publish–subscribe** by name so graphs do not need a visible wire across the whole map.

| Field | Type | Notes |
|-------|------|--------|
| `id` | string | Publisher or subscriber node id. |
| `channelName` | string | Shared key. |
| `role` | enum | `publish` / `subscribe`. |

**Publisher inputs:** `Send` (parameter becomes payload), `Trigger` (empty payload).

**Subscriber outputs:** `OnMessage` (payload from publisher), optional filter on payload type.

---

### 2.9 `logic_merge` / `logic_split` (fan-in / fan-out with weights)

**Merge:** several inputs OR together into one `OnTrigger` (optional `mode`: `any`, `all` for a frame). **Split:** one input duplicated to N outputs with optional **per-branch scale** (numeric factor on scalar buses) or **delayMs** per branch—Create-like gearbox flavor without simulating physics.

---

### 2.10 `logic_predicate`

Passes or blocks events based on **context** (instigator kind, tag, team, flag).

| Field | Type | Notes |
|-------|------|--------|
| `rules` | list | e.g. `{ "instigatorMustBe": "player" }`, `{ "hasFlag": "area_a_clear" }`. |

**Inputs:** `Trigger` (forwards if pass).

**Outputs:** `OnPass`, `OnFail` (optional), or only forward on pass.

---

### 2.11 `logic_inventory_gate` (item check on instigator)

Specialized comparator for **“does this actor carry item X?”** without scripting.

| Field | Type | Notes |
|-------|------|--------|
| `itemId` | string | Required key / item id. |
| `consumeOnPass` | boolean | Remove one charge on success (keycard consumed vs permanent key—product policy). |
| `quantity` | number | Default 1. |

**Inputs:** `Evaluate` / `Trigger` (reads `context.instigator` inventory).

**Outputs:** `OnPass`, `OnFail`, `OnBecomeTrue`, `OnBecomeFalse` (same semantics as `logic_compare` if you want latched door state).

---

### 2.12 `logic_trigger_detector`

Sits **between** a raw volume/button pulse and the rest of the graph: filters noise, enforces **one-shot per actor**, **cooldown**, or **instigator type** (player vs NPC).

| Field | Type | Notes |
|-------|------|--------|
| `oncePerInstigator` | boolean | Remember who already fired; ignore repeat touches until reset or leave. |
| `cooldownMs` | number | Minimum gap between accepted pulses. |
| `instigatorFilter` | enum | `any`, `player`, `npc`, `tag:…` (product-defined). |
| `requireExitBeforeRetrigger` | boolean | Must get `OnEndTouch` before next `OnPass` for same instigator. |

**Inputs:** `Trigger` (from volume—payload carries instigator), `Reset` (clear memory), `Enable` / `Disable` / `Toggle`.

**Outputs:** `OnPass` (forwards context), `OnReject` (optional, for feedback).

---

## 3. World actors with ports (map layer)

These are **visible or collision-based** entities that participate in the **same graph** as logic blocks. In the editor they show **ports** on the mesh or on a sibling gizmo; wires leave the invisible logic layer and **dock** here.

| Actor kind | Typical outputs | Typical inputs |
|------------|-----------------|----------------|
| **Trigger volume** | `OnStartTouch`, `OnEndTouch`, optional `OnStay` | `Enable`, `Disable` (volume armed) |
| **Trigger detector block** (logic-layer helper) | Forwards `OnPass` after rules | `Trigger` (from volume or ray) — adds **once per visitor**, **cooldown**, **debounce**, or **filter** so volumes stay dumb and reusable |
| **Spawner** | `OnSpawned`, `OnDespawned`, `OnFailed` | `Spawn`, `Despawn`, `Enable`, `Disable` |
| **Door / mover** | `OnOpened`, `OnClosed`, `OnLocked` | `Open`, `Close`, `Lock`, `Unlock`, `Toggle` |
| **Keycard reader** (interactable) | `OnInteract`, `OnScan` (payload: instigator) | `Enable`, `Disable` |
| **Light / FX / audio** | — | `Enable`, `Disable`, `SetIntensity`, `Play` |

**Context payload:** Any output that involves a player or AI should include `instigatorId` / `instigator` handle so **inventory gates** and **predicates** can run without extra wiring.

---

## 4. Example circuits (authoring stories)

### 4.1 Spawn a creature when someone enters a volume

1. Place a **trigger volume** in the doorway; arm it (`startEnabled` true).
2. Optional: place a **trigger detector** logic block (invisible layer) next to it—wire `OnStartTouch` → detector `Trigger`. Configure detector **once per instigator** and **players only** if you do not want NPCs or repeat spawns.
3. Wire detector `OnPass` → **`logic_relay`** or directly → **spawner** `Spawn`.
4. On the **spawner**, set template id (monster archetype), transform offset, and whether spawn is **once** (spawner internal flag) or **every** entry.

Graph: `TriggerVolume.OnStartTouch` → `TriggerDetector.Trigger` → `OnPass` → `Spawner.Spawn`.

### 4.2 Keycard reader → item check → door

1. Place **keycard reader** at the door; wire its `OnInteract` (or `OnScan`) → **`logic_inventory_gate`** `Evaluate` (or chain: reader → relay → gate). Gate `itemId` = `level_b_key` (example).
2. Wire gate **`OnPass`** → **door** `Unlock` then `Open` (or a single `GrantAccess` input if the door combines both).
3. Wire gate **`OnFail`** → optional **feedback** (light pulse, sound, UI message) via another relay.
4. If the door must **stay** open only while the condition was ever satisfied, use a **`logic_latch`**: `OnPass` → `Set` on latch; door listens to latch `OnTrue` for `Open`. If the door should **re-lock** when they walk away, that is a separate policy (not automatic).

Graph: `KeycardReader.OnInteract` → `logic_inventory_gate.Evaluate` → `OnPass` → `Door.Unlock` + `Door.Open`; `OnFail` → `Relay` → `Feedback`.

---

## 5. Wiring (level-agnostic)

Levels or tools express:

```jsonc
{
  "entities": { "my_counter": { "kind": "logic_counter", "startValue": 0, "maxValue": 3 } },
  "routes": [
    { "from": ["panel_a", "OnInteract"], "to": [{ "target": "my_counter", "input": "Increment" }] },
    { "from": ["my_counter", "OnHitMax"], "to": [{ "target": "light_b", "input": "Enable", "delayMs": 0 }] }
  ]
}
```

ProtoEngine today uses parallel `outputs` maps on spawners and `fireEntityOutput`; the **agnostic** form is the same graph, stored however Raw Iron prefers (JSON, ECS systems, visual graph).

---

## 6. Non-goals (explicit)


- **3D editor placement** is for Raw Iron Editor  nodes may still be **data-only** in headless tools.
- UI panels, subtitles, and lights are **example consumers**; the contract is the port names and payloads.
- No standard for debugging history; optional `entityIo`-style telemetry is product-specific.

---

## 7. Migration hint (ProtoEngine)

**ProtoEngine (done):** The four spawner cases, `logicEntityStates`, timer/counter/compare helpers, and related `dispatchBuiltinEntityInput` branches are removed; the dev hall counter/compare demo is replaced by a static spec panel and direct trigger-volume `outputs` (no logic nodes). **Raw Iron (to implement):** one scheduler module + one graph executor + trigger/action bus + **world-actor port adapters** for volumes, spawners, doors, and interactables.
