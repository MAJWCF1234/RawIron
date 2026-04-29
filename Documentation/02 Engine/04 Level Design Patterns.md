---
tags:
  - rawiron
  - engine
  - level-design
  - authoring
---

# Level Design Patterns

This note preserves the authoring patterns the prototype proved out so the native engine keeps the same level-design intelligence while the implementation moves into C++.

## Purpose

RawIron should not just port systems.
It should port good authoring habits.

That means documenting the kinds of patterns the engine is meant to support cleanly, even while some of the surrounding editor/runtime layers are still being rebuilt.

## Approach Triggers

Approach-trigger patterns are still one of the best ways to author contextual beats without hardcoding room logic.

The pattern is:

1. Place a trigger or query volume.
2. Fire a hook when the player or actor enters it.
3. Gate the response on flags or values.
4. Let the event or local-logic layer decide what to do.

This keeps authored spaces responsive without requiring bespoke code for every doorway, hallway, or threshold.

## Verified Safe Islands

The prototype was right to lean on readable safe spaces.

RawIron should preserve that design taste through native authoring concepts such as:

- trusted light islands
- authored safe zones
- local environment changes that tell the player what a space means

The point is not to turn every map into a corridor.
The point is to let spaces communicate safety, pressure, and route intention through authored runtime systems.

## Local Graphs Over Global Spaghetti

A good authoring rule:

- use events for global scenario logic
- use local logic graphs for room-local behavior

That means a room should be able to express things like:

- trigger to relay
- relay to timer
- timer to counter
- counter to threshold check
- threshold check to lights, doors, or messages

without promoting every small interaction chain into one giant global event list.

## Reusable Room Chunks

The engine should support reusable authored chunks instead of forcing copy-paste world building.

The prototype explored that through prefab and template thinking.
RawIron should preserve the same instinct in native content tooling:

- reusable room pieces
- reusable logic setups
- reusable lighting or helper rigs
- reusable debug/demo stations

The new native landing zone for that behavior is `RawIron.Content`, which now owns template and prefab expansion in `C++` instead of leaving those authoring rules inside the prototype shell.

## Operator And Helper Bays

The prototype benefited from having dedicated places where new systems had to prove themselves in-world.

RawIron should keep that habit.

A good native dev map should have deliberate rows or bays for:

- structural operators
- query volumes
- traversal helpers
- audio and fog helpers
- instrumentation checks
- rendering tricks and edge cases

If a system only exists in docs and tests, it is easy to lie to ourselves about how usable it really is.

## In-World Readouts

The prototype made good use of readable in-world debug surfaces.

RawIron should preserve that idea in native form:

- info panels
- hierarchy or inspector-facing summaries
- room-local status readouts
- helper bays that make state visible without opening a terminal first

That is a good engine habit, not a temporary prototype trick.

## Current Rule Of Thumb

When designing RawIron authoring features, prefer systems that let designers express:

- context
- state
- transition
- feedback

without needing to edit engine code.

## Related Notes

- [[06 Content Assembly]]
- [[03 Event Engine]]
- [[02 World Systems]]
- [[05 Debugging and Instrumentation]]
