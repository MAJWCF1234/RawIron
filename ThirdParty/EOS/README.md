# Epic Online Services (EOS) SDK placement

RawIron expects the optional EOS SDK here:

- `ThirdParty/EOS/SDK/Include/eos_sdk.h`
- `ThirdParty/EOS/SDK/Lib/EOSSDK-Win64-Shipping.lib`
- `ThirdParty/EOS/SDK/Bin/EOSSDK-Win64-Shipping.dll`

## Credentials

Copy `eos_config.example.json` to either:

- `O:/RawIron/eos_config.json` (gitignored), or
- `config/eos_config.json`

Fill Product / Sandbox / Deployment / Client ID + Secret from the Epic Dev Portal.
Environment overrides: `EOS_PRODUCT_ID`, `EOS_SANDBOX_ID`, `EOS_DEPLOYMENT_ID`, `EOS_CLIENT_ID`, `EOS_CLIENT_SECRET`.

## Build toggle

```powershell
cmake --preset dev-msvc-network
# or explicitly:
cmake -S . -B build/dev-msvc-network -DRAWIRON_USE_ENET=ON -DRAWIRON_USE_EOS=ON
```

`rawiron_stage_runtime_dlls` copies `EOSSDK-Win64-Shipping.dll` next to DedicatedServer / BotClient / MultiplayerSandboxGame.

## Runtime model

- **EOS** = rendezvous only (issue/resolve join codes via Lobby + `RI_TOKEN` attribute)
- **ENet** = authoritative gameplay transport (UDP)

Join codes look like `EOS:<lobbyId>`. Direct LAN codes `RI1:host:port:mode` still work with `--rendezvous=direct`.

## Notes

- Do not commit proprietary SDK binaries or live `eos_config.json` unless your distribution policy allows it.
- EOS account / product setup is outside this repository.
