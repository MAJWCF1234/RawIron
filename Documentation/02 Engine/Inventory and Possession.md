---
tags:
  - rawiron
  - engine
  - world
  - inventory
  - gameplay
---

# Inventory and Possession

## Scope

`ri::world::InventoryLoadout` (`RawIron/World/InventoryState.h`) models **player-facing storage**: hotbar, backpack, optional **off-hand**, selection index, presentation policy, and **stack quantities** for stackable definitions.

It is intentionally **agnostic** of rendering and physics; hosts bind it to HUD, pickups, and animation (“what is in each hand?”).

## Possession vocabulary

| Concept | API | Notes |
|--------|-----|-------|
| **Primary / main hand** | `PrimaryHandSlot()`, `PrimaryHandItem()` | Uses the **selected hotbar index** — the slot used by `QuickUseSelectedHotbar` and the usual action binding. |
| **Off-hand** | `OffHand()` | Torch, shield, sidearm — `preferredEquipSlot == InventoryEquipSlot::OffHand`, `SwapHands()`, quick-use equip path. |
| **Held in hands** | `HoldsItemInHands(itemId)` | True if `itemId` is in **either** the selected hotbar slot **or** the off-hand (world interaction / “you are holding the key”). |
| **Owned anywhere** | `ContainsItemId`, `CountItemId` | Counts **total units** summed across stacks in hotbar, backpack, and off-hand. |

Selection changes which item counts as “main hand”; gameplay should call `SetSelectedHotbarSlot` when the player rotates the bar.

## Stacks

`InventoryItemDefinition::stackCount` defaults to **1**. For **`unique == false`** items (ammo, bandages that stack):

- **`AddItem`** merges into an existing matching slot when possible (off-hand first for same id, then hotbar, then backpack).
- **`CountItemId`** returns **total units**, not slot count.
- **`CaptureSnapshot`** / **`RestoreSnapshot`** persist parallel `hotbarCounts`, `backpackCounts`, and `offHandCount`.

Consumables with **heal** decrement one unit per successful quick-use while stack size &gt; 1.

## Limits

- Per-slot stack counts use signed `int` and merge with overflow protection: totals **clamp at `INT_MAX`** (`MergeStackTotals` in `InventoryState.cpp`).
- `TryInventoryGate` re-validates counts after a consuming remove so callers do not get a false success if invariants drift.

## Logic inventory gates

`ri::logic::LogicGraph` nodes of kind `logic_inventory_gate` call `LogicGraphSpec::inventoryQuery(instigatorId, itemId, quantity, consume)` (see `LogicTypes.h`).

Bind a loadout with **`BindInventoryGateQuery(boundInstigatorId)`**:

- Pass **`""`** (empty bound id) to accept **any** instigator (single-player convenience).
- Otherwise the handler requires `instigatorId == boundInstigatorId`.

**`TryInventoryGate(itemId, quantity, consume)`** implements checks and optional removal:

- Removal order when **`consume == true`**: **backpack** slots first, then **hotbar**, then **off-hand** (deep storage consumed before equipped items).

Store the returned `InventoryQuery` on your graph spec **before** evaluation. The loadout instance **must outlive** the graph if the closure captures `this`.

## Related Notes

- [[Library Layers]] — where `InventoryLoadout` sits in the World layer
- [[Entity IO and Logic Graph]] — wiring logic outputs after gates pass
- [[02 World Systems]]
