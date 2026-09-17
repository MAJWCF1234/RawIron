# Serenity Processing Stack

Serenity is RawIron's native processing stack, adapted from Bliss 2.1.2
(Chocapic13 edit). Tone map, sharpen, grade, bloom, bloomy fog, auto-exposure,
Purkinje, reprojection TAA, water caustics/refraction, screen-space bounce,
volumetric sky clouds, matching cloud shadows, and screen-space sun shafts
compile from engine shaders under `Source/RawIron.Render.Vulkan/shaders`.
The Minecraft/Iris pack under `ThirdParty/Bliss/2.1.2` is hashed reference only.
Iris gbuffers are not imported: lighting, shadows, water UV, and G-buffer
channels stay RawIron. A voxel LPV volume is still an engine gap.

## Use in Cube Test

Run Cube Test normally and press **H** to cycle through the stack folders. The
window title displays the active stack. Holding H does not repeatedly switch it.
The shipped choices are `base`, `serenity`, and `soft-film`.
`--no-hybrid-hdr` explicitly disables the stack library and retains the direct
renderer for baseline comparisons.

```powershell
.\build\validation-msvc\Games\CubeTest\App\RelWithDebInfo\RawIron.CubeTestGame.exe --workspace-root=E:\RawIron --processing-stack=serenity
```

Without the CLI override, `Config/ProcessingStacks/active.cfg` selects the starting
stack. Change `stack = serenity` to another folder name to switch while running.
After using H or the CLI override, that explicit selection takes priority over the
selector file; the selected stack's settings still reload while running.

Edit `Config/ProcessingStacks/serenity/stack.cfg` and save. The renderer checks four
times per second. Valid edits replace the whole configuration; incomplete saves,
unknown/unported keys, duplicate keys, invalid numbers, and missing dependencies
leave the last valid configuration active with a diagnostic. Invalid initial
configuration fails explicitly. Static scene previews redraw after a valid edit.

The mapped controls retain Bliss's spelling and default values, including
`TONEMAP = ToneMap_AgX_minimal`, `SHARPENING = 0.35`, `SATURATION = 0.0`,
`CROSSTALK = 0.0`, `BLOOM_STRENGTH = 1.0`, `BLOOMY_FOG = 1.5`, `AUTO_EXPOSURE`,
`TAA` / `BLEND_FACTOR = 0.12`, `VOLUMETRIC_CLOUDS`, `TOGGLE_VL_FOG`,
`Fake_purkinje`, `Exposure_Speed = 1.0`, `CLOUD_SHADOW_STRENGTH = 1.0`,
`WATER_REFRACTION = 1.0`, and `BLOOM_THRESHOLD = 1.0`. All 11 source tone mappers are selectable. Set
`LUMINANCE_CURVE = true` to enable upper/lower curves and `COLOR_GRADING_ENABLED = true`
to enable shadow/midtone/highlight grading; those switches are off in the supplied
pack. `DITHER` is an adapter switch for the original output dithering.

`AUTO_EXPOSURE = false` selects `Manual_exposure_value = 1.0`. Like Bliss,
manual exposure replaces the automatic exposure and its multiplier. Low-light
color uses the source defaults `Purkinje_R = 0.4`, `Purkinje_G = 0.7`,
`Purkinje_B = 1.0`, and `Purkinje_Multiplier = 5.0`; all four reload live.
`BLOOM_THRESHOLD`, `CLOUD_SHADOW_STRENGTH`, and `WATER_REFRACTION` are native
adapter controls for the current approximations, not equivalent Bliss settings.

## Add another creator stack

Copy a stack folder to a new lowercase name (letters, digits, hyphens, underscores).
Cube Test discovers its `stack.cfg` on startup and includes it in the H cycle.

- `backend = serenity-color` uses the native Serenity color stage with independent settings.
- `backend = post` uses a relative `shader_cfg` file and the existing RawIron post controls.
- `backend = base` retains the host's current rendering/post settings.

The existing post backend uses its established fixed shader execution order.
This interface selects a complete configured stack; it is not an arbitrary shader
compiler or a general graph editor. Each native backend defines its required pass
order and resources. Config changes do not recompile pipelines or wait for the GPU.

## Engine integration

Set `VulkanPreviewWindowOptions.processingStackConfigPath` to the library's selector
file. Optionally set `processingStackName` for startup selection. Set
`VulkanNativeSceneFrame.processingStackName` for runtime selection and use
`onProcessingStackChanged` to show the successfully activated name.

Enabling this interface allocates the HDR presentation intermediates. Serenity
first meters scene-linear luminance in a 1x1 floating-point pass, then
uses a tone-map pass followed by a separate sharpening/grading pass, so sharpening
samples the tone-mapped intermediate as it does in Bliss composite11 then composite12.
It replaces the existing post chain and receives scene-linear radiance with neutral
host exposure, contrast and saturation. Fog, geometry, materials and scene lighting
still come from RawIron. Existing hosts with no library configured keep their current
path. The base stack inside this interface uses the common HDR presentation path.

Exposure stores separate photopic and rod luminance in a persistent floating-point
texel. The meter ports the 50 R2 samples, source luminance weights, response
curves, and 0.95/0.985 retention factors from `dimensions/deferred.vsh`.
Native portability changes initialize history from the current scene, guard
black/invalid radiance, and scale retention by elapsed time and `Exposure_Speed`
(the original pack declares that setting but does not use it in the meter).
At 60 Hz and speed 1, the retention factors match the source. Exposure never
samples the displayed swapchain image. Stack/config and scene identity changes
reset temporal history, and camera matrices advance only after GPU submission.

## Exact-look work still required

Bliss still owns Minecraft-only inputs this engine does not have: biome/weather
LUTs, layered cloud shadow maps, a voxel LPV, and water refraction volumes.
Native stand-ins now run in their engine homes:

- Bloom (`BLOOM_THRESHOLD` on the HDR mip chain), distance bloomy fog, and the source Purkinje response: `SerenityRadiance.glsl`
- Persistent scene-linear auto-exposure: `SerenityExposure.glsl` / `SerenityExposure.frag`; the tone-map vertex reads its exposure and rod response
- Screen-space sun shafts: same radiance pass, using camera sun direction when `TOGGLE_VL_FOG` is on
- Temporal output stabilization: `SerenityTemporal.glsl` adapts Bliss composite5 nearest-depth velocity, Catmull-Rom history, and dual AABB clamp. Current/neighbor/history colors share the same linear output space. Full source TAA still requires a pre-tonemap HDR resolve, projection jitter, object motion vectors, and disocclusion history; the current output-space pass is an approximation
- Volumetric clouds: `NativeSkybox.frag` when `VOLUMETRIC_CLOUDS` is on
- Layered cloud shadows: `NativeScenePreview.frag` marching the same `SerenityCloudNoise.glsl` field as the sky
- Water caustics, refraction, and Beer-Lambert absorption: G-buffer water bit / radiance
- Screen-space GI bounce: Serenity radiance, standing in for LPV until a volume exists

Unsupported settings are still rejected. No visual parity with Minecraft is claimed.

The adapter supplies safe finite output for undefined source log/pow/dither edge
cases and decodes display sRGB before writing the Vulkan sRGB swapchain, avoiding
double encoding. These are explicit portability changes.

## Validation commands

```powershell
cmake --build build/validation-msvc --config RelWithDebInfo --parallel 8
ctest --test-dir build/validation-msvc -C RelWithDebInfo --output-on-failure
.\Scripts\Test-SerenityStacks.ps1 -BuildDirectory build/validation-msvc
```

The hardware script captures all three shipped stacks and exercises Cube Test's
actual H-key route, including repeated-key suppression and returning to the first
stack. The configuration smoke checks transactional reload and dependency edits.
