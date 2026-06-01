# RawIron Multiplayer Sandbox

Format contract: `rawiron-game-v1.3.7` with mandatory RuntimeCore binding.

Manifest identity:

- `id`: `rawiron-multiplayer-sandbox`
- `entry`: `RawIron.MultiplayerSandboxGame`
- `runtimeModule`: `RawIron.Game.MultiplayerSandbox`
- `editorProjectArg`: `--game=rawiron-multiplayer-sandbox`

Purpose:

- full-engine multiplayer testbed for dedicated/listen/hybrid/client modes
- join-by-code workflows (`eos` + `direct` fallback)
- bot swarm load testing and latency simulation
- RuntimeCore lifecycle/event/service validation under sustained net load

Play examples:

- `RawIron.MultiplayerSandboxGame --net-mode=listen --issue-join-code --bots=32 --frames=3600`
- `RawIron.MultiplayerSandboxGame --net-mode=dedicated --port=27015 --max-peers=256 --server-tick=125`
- `RawIron.MultiplayerSandboxGame --net-mode=hybrid --bots=64 --sim-net --sim-delay-ms=80 --sim-jitter-ms=20 --sim-loss-pct=3`

Editor open:

- `RawIron.Editor --game=rawiron-multiplayer-sandbox`
