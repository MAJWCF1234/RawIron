# Epic Online Services (EOS) SDK placement

RawIron expects the optional EOS SDK here:

- `ThirdParty/EOS/SDK/Include/eos_sdk.h`
- `ThirdParty/EOS/SDK/Bin/Win64` (or `Bin/Win32`) for platform binaries

## Build toggle

Enable EOS hooks with:

```powershell
cmake -S . -B build -DRAWIRON_USE_EOS=ON
```

If SDK files are missing, build still succeeds with provider stubs and runtime falls back to `DirectToken` join codes.

## Notes

- EOS account setup and platform approvals are handled outside this repository.
- Do not commit proprietary SDK binaries unless your distribution policy allows it.
