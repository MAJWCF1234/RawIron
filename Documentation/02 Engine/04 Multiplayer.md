# Multiplayer

Multiplayer is a shared engine feature, not a sidecar experiment.

## Core net runtime

`AuthoritativeNetConfig` and `AuthoritativeNetModule` in `RawIron.Runtime` provide the main engine surface.

## Supported modes

- `Dedicated`
- `ListenHost`
- `HybridP2P`
- `ClientOnly`

## Supported rendezvous providers

- `EpicOnlineServices`
- `DirectToken`
- `None`

## Engine net features

- authoritative server/client flow
- snapshot replication
- lag compensation and rewind buffers
- latency simulation
- optional side-channel P2P plane
- host migration state tracking
- join code issuance and resolution
- prediction and correction telemetry

## Multiplayer apps

- `RawIron.DedicatedServer`: authoritative headless host
- `RawIron.BotClient`: headless load and swarm client
- `RawIron.MultiplayerSandboxGame`: game-facing multiplayer testbed

## Typical command line surfaces

Common options across server, bot, and sandbox flows include:

- `--net-mode`
- `--host` or `--host-port`
- `--port`
- `--connect-host`
- `--connect-port`
- `--p2p-port`
- `--rendezvous=direct|eos`
- `--issue-join-code`
- `--join-code=<code>`
- `--net-tick`
- `--server-tick`
- `--max-peers`
- `--sim-net`
- `--sim-delay-ms`
- `--sim-jitter-ms`
- `--sim-loss-pct`

## Project-side data

Games also carry network and persistence authoring surfaces such as:

- `scripts/network.riscript`
- `scripts/persistence.riscript`
- `config/network.cfg`

## Best place to explore the stack

[[03 Projects/RawIron Multiplayer Sandbox|RawIron Multiplayer Sandbox]] is the most complete in-repo exercise bed for multiplayer support.
