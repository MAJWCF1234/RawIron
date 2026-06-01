---
tags:
  - rawiron
  - networking
  - eos
  - bots
---

# EOS Integration

## Goal

RawIron supports provider-based online rendezvous for host/join flows:

- preferred provider: `EpicOnlineServices`
- fallback provider: `DirectToken` (no backend required)

This keeps engine-level networking flexible for game creators while avoiding mandatory self-hosted master infrastructure.

## Build Flags

- `RAWIRON_USE_EOS=ON` enables EOS compile hooks (`RAWIRON_HAS_EOS`).
- `RAWIRON_USE_ENET=ON` enables ENet UDP transport backend.
- `RAWIRON_BUILD_BOT_CLIENT=ON` builds `RawIron.BotClient`.
- `EOS_SDK_ROOT` defaults to `ThirdParty/EOS/SDK`.

Example:

```powershell
cmake -S . -B build -DRAWIRON_USE_ENET=ON -DRAWIRON_USE_EOS=ON -DRAWIRON_BUILD_BOT_CLIENT=ON
cmake --build build --target RawIron.Runtime RawIron.BotClient
```

## ThirdParty Layout

EOS is integrated as an optional provider with expected SDK placement in:

- `ThirdParty/EOS/SDK/Include/eos_sdk.h`
- `ThirdParty/EOS/SDK/Bin/Win64` (or `Bin/Win32`)

See `ThirdParty/EOS/README.md` for exact layout details.

## Runtime Net Modes

`AuthoritativeNetModule` supports:

- `Dedicated`
- `ListenHost`
- `HybridP2P`
- `ClientOnly`

Traffic planes:

- `Authority` channel for gameplay-critical state.
- `P2P` channel for non-critical side traffic (voice/cosmetics/social).

## Join-by-Code Behavior

- Host/listen modes can issue code on startup (`issueJoinCodeOnStartup`).
- Client can resolve and connect by code (`joinCodeToResolve`).
- If EOS provider is unavailable, runtime falls back to `DirectToken`.

## Bot Client

`RawIron.BotClient` is a headless swarm/load harness for multiplayer testing.

Common runs:

```powershell
.\build\Apps\RawIron.BotClient\RawIron.BotClient.exe --frames=240 --net-mode=listen --rendezvous=direct --issue-join-code --bots=32 --server-tick=125 --max-peers=128
.\build\Apps\RawIron.BotClient\RawIron.BotClient.exe --frames=240 --net-mode=client --rendezvous=direct --join-code=RI1:127.0.0.1:27015:1 --bots=16
.\build\Apps\RawIron.BotClient\RawIron.BotClient.exe --frames=240 --net-mode=hybrid --bots=64 --sim-net --sim-delay-ms=80 --sim-jitter-ms=20 --sim-loss-pct=3
.\build\Apps\RawIron.DedicatedServer\RawIron.DedicatedServer.exe --frames=0 --port=27015 --server-tick=125 --max-peers=256 --rendezvous=eos --issue-join-code
```

## Competitive Runtime Contracts (current)

- Dedicated-server-first policy flag is enabled by default (`dedicatedServerFirst=true`).
- Snapshot replication now tracks per-peer baselines and prefers delta packets when smaller than full snapshots.
- Net metrics emit delta/full snapshot byte counts plus netgraph-style RTT/jitter/loss/prediction error fields.
- Demo stream records deterministic per-frame checksums and enforces monotonic tick playback order.

## Notes

- EOS usage terms and account/platform approvals are separate from engine code.
- Dedicated server hosting costs are external to EOS (compute/bandwidth provider costs).
- `DirectToken` is convenient for local/manual testing but is not a managed global code service.
