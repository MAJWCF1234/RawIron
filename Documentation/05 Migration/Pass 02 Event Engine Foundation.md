---
tags:
  - rawiron
  - migration
  - pass-02
  - events
---

# Pass 02 Event Engine Foundation

## Goal

Extract the prototype event engine out of app code and give RawIron a native, reusable hook/action runtime that can survive beyond the web prototype.

## What Landed

### `RawIron.Events`

- normalized event definitions with stable runtime IDs
- world flag storage
- world numeric value storage
- condition evaluation for:
  - `requiredFlags`
  - `missingFlags`
  - `valuesAtLeast`
  - `valuesAtMost`
  - `valuesEqual`
- hook execution with:
  - `sourceId` filtering
  - `once`
  - `cooldownMs`
  - `maxRuns`
  - `consumeInteraction`
  - `stopAfterMatch`
- built-in action runtime for:
  - `set_flag`
  - `set_value`
  - `add_value`
  - `run_group`
  - `run_sequence`
  - `if`
  - `delay`
  - `cancel_sequence`
  - `cancel_timer`
- target-group resolution
- named timer scheduling and cancellation
- sequence step scheduling
- executor callbacks for game/editor-specific action effects that do not belong in the generic engine layer yet

## Why This Shape Is Right

The prototype event system was doing two jobs at once:

- generic event-engine control flow
- game-specific action execution

RawIron now owns the generic control flow in a dedicated library.

That means future runtime systems can plug in their own action handlers without forcing the event engine itself to know about enemies, alarms, UI, or one specific game.

## Verification

The migration suite now verifies:

- stable event normalization
- hook execution order
- world-state mutation between events on the same hook
- cooldown behavior
- max-run limits
- target-group deduplication
- action-group execution
- named delay timers
- named timer cancellation
- sequence scheduling and cancellation
- conditional action branching

Toolchain status:

- `MSVC`: pass
- `Clang`: pass

## Next Steps From Here

1. connect `RawIron.Events` to future world/runtime services as they land
2. port more prototype action handlers once their target systems have native RawIron homes
3. port runtime schemas so authored event/sequence/action data can validate against engine-owned types
