# PsxPack assembly

Builds `Games/WildernessRuins/assets/PsxPack` for Wilderness Ruins free-demo / engine use.

```powershell
pwsh Tools/PsxPack/Assemble-PsxPack.ps1 -SourceRoot <private-purchase-root>
```

Or set `RAWIRON_PSX_SOURCE` for the session. Do not commit that path. The script never writes under the purchase root and never embeds the root folder name in pack metadata.

Licensing for packed content: `Games/WildernessRuins/assets/PsxPack/LICENSING.md`.
