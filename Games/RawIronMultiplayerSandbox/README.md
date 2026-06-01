# RawIron Multiplayer Sandbox

Format contract: `rawiron-game-v1.3.7`

## Identity

- `id`: `rawiron-multiplayer-sandbox`
- `name`: `RawIron Multiplayer Sandbox`
- `entry`: `RawIron.MultiplayerSandboxGame`
- `runtimeModule`: `RawIron.Game.MultiplayerSandbox`
- `editorProjectArg`: `--game=rawiron-multiplayer-sandbox`
- `primaryLevel`: `levels/assembly.primitives.csv`
- `editorPreviewScene`: `rawiron-multiplayer-sandbox`

## Purpose

RawIron Multiplayer Sandbox is the in-repo multiplayer proving ground for dedicated, listen, hybrid, and client flows. It is also the clearest project for validating join-code workflows, simulated bad network conditions, bot swarm testing, and runtime diagnostics under load.

## Open and run

- `RawIron.Editor --game=rawiron-multiplayer-sandbox`
- `RawIron.MultiplayerSandboxGame --net-mode=listen --issue-join-code --bots=32 --frames=3600`
- `RawIron.MultiplayerSandboxGame --net-mode=dedicated --port=27015 --max-peers=256 --server-tick=125`
- `RawIron.MultiplayerSandboxGame --net-mode=hybrid --bots=64 --sim-net --sim-delay-ms=80 --sim-jitter-ms=20 --sim-loss-pct=3`

## Controls

- `WASD` move
- `Mouse` look
- `Space` jump
- `Shift` sprint
- `E` interact
- `Esc` quit

## Multiplayer features exercised here

- authoritative dedicated and listen-host paths
- hybrid P2P side-plane support
- EOS and direct-token rendezvous
- join-code issuance and resolution
- latency simulation and packet loss simulation
- bot swarm module integration
- runtime service and lifecycle validation during network sessions

## Main project surfaces

### Runtime scripts

- `scripts/gameplay.riscript`
- `scripts/rendering.riscript`
- `scripts/logic.riscript`
- `scripts/ui.riscript`
- `scripts/audio.riscript`
- `scripts/streaming.riscript`
- `scripts/localization.riscript`
- `scripts/physics.riscript`
- `scripts/postprocess.riscript`
- `scripts/init.riscript`
- `scripts/state.riscript`
- `scripts/network.riscript`
- `scripts/persistence.riscript`
- `scripts/ai.riscript`
- `scripts/plugins.riscript`
- `scripts/animation.riscript`
- `scripts/vfx.riscript`

### Config and policy

- `config/game.cfg`
- `config/input.map`
- `config/project.dev`
- `config/network.cfg`
- `config/build.profile`
- `config/security.policy`
- `config/plugins.policy`

### Level assembly

- `levels/assembly.*`

### Assets, data, plugins, and tests

- `assets/*`
- `data/*`
- `plugins/*`
- `ai/*`
- `ui/*`
- `tests/*.riscript`

## Related apps

- `RawIron.DedicatedServer`
- `RawIron.BotClient`

## Reference

- `Games/GAME_FORMAT.md`
- `Documentation/03 Projects/RawIron Multiplayer Sandbox.md`
- `Documentation/02 Engine/04 Multiplayer.md`
