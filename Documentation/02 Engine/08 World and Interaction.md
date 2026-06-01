# World and Interaction

RawIron world simulation is built from authored level data plus shared runtime services.

## Level assembly surfaces

Current games carry a family of authored files under `levels/` including:

- `assembly.primitives.csv`
- `assembly.colliders.csv`
- `assembly.navmesh`
- `assembly.zones.csv`
- `assembly.triggers.csv`
- `assembly.occlusion.csv`
- `assembly.audio.zones`
- `assembly.lods.csv`
- `assembly.ai.nodes`
- `assembly.lighting.csv`
- `assembly.cinematics.csv`

## Shared world behavior

The engine and game runtime support stack provide:

- collision and trace scenes
- navmesh and zone awareness
- trigger dispatch
- occlusion and LOD data ingestion
- authored audio zones and environmental routing
- AI node graph support
- interaction target resolution for world actors

## Interaction model

The world runtime can resolve interactables from camera- or player-driven queries and route those interactions into world actor logic, trigger flow, or game-authored handlers.

## Environmental context

Games author zones for streaming, audio, triggers, and visibility. Runtime services evaluate those authored volumes and feed the active environment back into rendering, audio, and gameplay systems.

## Diagnostics

Both the editor and game runtimes surface asset presence, lookup counts, trigger counts, occlusion counts, audio zone counts, LOD counts, and related runtime support summaries so authored worlds can be checked quickly.
