# PsxPack licensing status

## Authority

The licensee holds a **signed agreement** with creator **Pizza Doggy** covering that creator's own catalog
(past, present, and future public releases across itch.io / Steam / related channels), including rights to
copy, modify, convert, package, redistribute, publish, and use commercially for models, textures, audio,
skyboxes, props, environments, and related original works.

Purchase evidence: Pizza Doggy **ALL IN ONE - The Humble Bundle** (retro PSX-style catalogue; related PSX Humble Bundle promotion).

**Intended distribution of PsxPack:** content for the RawIron engine / Wilderness Ruins **free demo** and
related project use. This is **game/engine content packaging**, not a marketplace resale of a standalone
asset pack, template, or bundle of source assets "on their own."

Creator-owned Pizza Doggy assets covered by the signed agreement are **authorized** for PsxPack.

## Classification of selected content

| Class | Meaning | PsxPack handling |
|-------|---------|------------------|
| **1. Creator-owned and covered** | Pizza Doggy original work under the signed agreement | Approved for packing and demo distribution |
| **2. Third-party, separately licensed** | Non-Pizza Doggy author with its own license that still permits this use | Include only when that license allows game/demo use |
| **3. Third-party, insufficient rights** | License forbids the intended use | Exclude |
| **4. Authorship unclear** | Investigate; do not blanket-uncertain the whole pack | Resolve per asset group |

### Class 1 - Creator-owned and covered (approved)

- `World/` landmarks and Modular Survival-style props
- `Textures/` loose bark/grass used by ForestRuins
- `Skies/` equirect skyboxes
- `LRT/tile/` curated `RT_*` tiles
- `Nature/Forest/` ground textures
- License/receipt copies under `licenses/`

### Class 2 - Third-party, separately licensed (included for game/demo use)

**DanglingBat** (pack `Nature/Trees`, `Nature/Rocks`, `Nature/Plants`):

- License on file: *DanglingBat's Game Assets - CC License Agreement* (under `licenses/`).
- Permits: use in any game (commercial or not); edit/modify.
- Forbids: reselling or sharing the assets **on their own**; including them in **marketplace** asset packs / templates / bundles; AI training use.
- **Our use:** embedded ForestRuins / engine demo content (not a standalone DanglingBat resale pack).

If PsxPack were ever published as a **standalone asset marketplace pack**, DanglingBat content would need to be stripped. That is not the current intent.

### Class 3 - Excluded

Unused third-party bulk and unused LRT / landmark extras are not selected into the curated pack.

### Class 4 - Authorship unclear

None of the currently required runtime paths are in a blocking unknown state.

## Attribution

- Pizza Doggy - primary catalog under signed agreement + Humble Bundle purchase.
- DanglingBat - trees / rocks / plants per Class 2 license (attribution appreciated).

## Assembly

```text
Tools/PsxPack/Assemble-PsxPack.ps1
```

Rebuilds this pack via a private session `-SourceRoot` (not documented or committed).
