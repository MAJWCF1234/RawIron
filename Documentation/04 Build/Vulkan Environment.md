---
tags:
  - rawiron
  - vulkan
  - build
---

# Vulkan Environment

## Current Machine Status

As of April 6, 2026, the current RawIron machine has a working Vulkan SDK and runtime.

Confirmed paths:

- Vulkan SDK root: `C:\VulkanSDK\1.4.341.1`
- Vulkan runtime loader: `C:\Windows\System32\vulkan-1.dll`
- `VULKAN_SDK` environment variable: `C:\VulkanSDK\1.4.341.1`
- RawIron app-local Vulkan loader copies are staged into each executable output directory during build

Confirmed tools:

- `C:\VulkanSDK\1.4.341.1\Bin\vkcube.exe`
- `C:\VulkanSDK\1.4.341.1\Bin\glslc.exe`
- `C:\VulkanSDK\1.4.341.1\Bin\vkconfig.exe`
- `C:\Windows\System32\vulkaninfo.exe`

## Verification

The machine was checked in three ways:

1. `vulkaninfo --summary` reported Vulkan instance version `1.4.341`
2. `vkcube.exe` launched successfully
3. the running `vkcube.exe` process was inspected and confirmed to have loaded:
   - `C:\Windows\System32\vulkan-1.dll`

That means this machine is not only carrying the SDK.
It is also resolving the Windows Vulkan runtime loader correctly.

## RawIron Command

`ri_tool` now exposes:

```powershell
ri_tool --vulkan-diagnostics
```

On Windows, this reports:

- Vulkan SDK root
- resolved Vulkan runtime library path
- Vulkan instance API version
- surface initialization status
- validation-layer availability
- enumerated instance extensions
- enumerated instance layers
- selected physical device summary
- queue-family support details
- discovered SDK tools in the SDK `Bin` directory

Because RawIron now stages `vulkan-1.dll` into the executable output directories on Windows, the diagnostics command should resolve the local copy when run from a built RawIron executable folder.

Linux direction:

- the Vulkan runtime should resolve through the system loader
- Linux should not depend on app-local DLL staging
- full Linux window/surface integration is still part of the future shared desktop platform layer

## Notes

- `vulkaninfo --summary` showed several third-party layers on this machine, including OBS, Overwolf, Steam, and ReShade-related layers
- one ReShade-related loader error appeared during summary output:
  - `Failed to open dynamic library "C:\ProgramData\ReShade\.\ReShade64.dll"`

This does not prevent Vulkan from working, but it is worth remembering if future debugging gets noisy.
