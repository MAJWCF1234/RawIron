# RawIron Multiplayer Sandbox

`RawIron Multiplayer Sandbox` is the workspace multiplayer proving ground.

## Identity

- game id: `rawiron-multiplayer-sandbox`
- editor arg: `--game=rawiron-multiplayer-sandbox`
- app target: `RawIron.MultiplayerSandboxGame`
- runtime module: `RawIron.Game.MultiplayerSandbox`

## Purpose

- exercise dedicated, listen, hybrid, and client net modes
- test join-by-code flows through EOS and direct token rendezvous
- run bot swarm load testing and simulated latency/loss conditions
- validate `RuntimeCore` behavior under sustained network load

## Runtime behavior

The sandbox validates the game project format, enforces shared config contracts, mounts shared runtime support services, configures `AuthoritativeNetModule`, optionally adds `BotSwarmModule`, and then runs a fixed-step frame loop.

## Example flows

- `RawIron.MultiplayerSandboxGame --net-mode=listen --issue-join-code --bots=32 --frames=3600`
- `RawIron.MultiplayerSandboxGame --net-mode=dedicated --port=27015 --max-peers=256 --server-tick=125`
- `RawIron.MultiplayerSandboxGame --net-mode=hybrid --bots=64 --sim-net --sim-delay-ms=80 --sim-jitter-ms=20 --sim-loss-pct=3`

## Related tools

- `RawIron.DedicatedServer`
- `RawIron.BotClient`
