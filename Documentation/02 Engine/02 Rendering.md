# Rendering

RawIron exposes both Vulkan and software rendering paths.

## Rendering libraries

- `RawIron.Render.Vulkan`
- `RawIron.Render.Software`
- shared scene and post-process support from `RawIron.Core`, `RawIron.World`, and `RawIron.SceneUtilities`

## Rendering ownership

Rendering policy is engine-owned.

- Post-process state and parameter conversion live in shared engine/runtime structures.
- Games feed authored values through config and script surfaces.
- Runtime code should not invent private presentation stacks outside engine contracts.

## Authoring inputs commonly consumed by games

- `scripts/rendering.riscript`
- `scripts/postprocess.riscript`
- `assets/shaders.manifest`
- `assets/materials.manifest`
- `assets/palette.ripalette`
- `levels/assembly.lighting.csv`
- `levels/assembly.occlusion.csv`
- `levels/assembly.lods.csv`
- `levels/assembly.audio.zones`

## Preview and runtime surfaces

- `RawIron.Player` is the generic runtime host.
- `RawIron.Preview` is the snapshot and preview host. Pass `--ray-trace` for software path-traced stills (`ScenePreviewRenderer::RayTrace`); optional `--ray-scale` tunes internal trace resolution.
- `RawIron.Editor` and `RawIron.EditorPreview` provide authoring-time visualization.
- `RawIron.ParticleShowcase` isolates particle-focused rendering work.

## Native Vulkan frame publication

`VulkanNativeSceneFrame::suppressUnchangedFrames` makes the producer's update contract explicit:

- Static and editor-driven producers leave suppression enabled and change `frameSequence` whenever they publish new renderable state.
- Continuously animated runtime producers set suppression to `false`, so time-based camera, animation, simulation, and shader changes render every host frame.
- Sequence values are opaque identifiers. Zero is valid, ordering is ignored, and only exact equality with the last successfully presented identifier suppresses a frame.
- A sequence becomes the last-presented value only after a successful or suboptimal Vulkan present. Acquire or present failures remain retryable and cannot accidentally suppress the replacement frame.

Once native initialization reaches frame processing, callback, scene-build, acquire, submit, and present failures pass through the same device-resource teardown path before the loop returns. Engine-owned Win32 windows are destroyed after Vulkan surface teardown; embedded editor/client windows remain host-owned. Host message-hook exceptions are contained inside the Win32 callback boundary and reported after teardown.

Camera and sky uniform storage is owned per frame-in-flight slot. The CPU waits only for the fence protecting the slot it is about to rewrite, so the other slot may remain in flight without observing partially updated camera or atmosphere data. Descriptor sets bind the matching slot-local buffers when each swapchain command buffer is recorded.

Native device creation queries `VkPhysicalDeviceFeatures` before enabling optional features. Sampler anisotropy is disabled with a 1x sampler fallback when unsupported. When `independentBlend` is unavailable, every attachment in a multi-target pipeline uses a common, spec-valid blend/write state; this may reduce G-buffer fidelity for transparent/additive draws, but it preserves rendering on feature-limited hardware instead of requesting an unsupported feature or constructing an invalid pipeline.

The shadow pass binds the compatible white material descriptor at set 1 before the shadow draw sequence, with authored alpha-cutout materials overriding it as needed. Ordinary shadow draws do not sample that set, while the fallback keeps the declared pipeline layout valid. Scene-generation changes wait every frame-slot fence before retiring cached mesh buffers. Camera and sky uniform updates remain slot-local: cache retirement does not collapse the accepted per-frame UBO ownership protocol into device-wide idling.

Use `RawIron.Render.Vulkan.FrameSchedulingSmoke` and `RawIron.Render.Vulkan.DeviceFeaturePolicySmoke` for the CPU-side publication and optional-feature contracts. They do not replace native device, resize, presentation, or visual validation. On Windows, build `VulkanNativeValidationProbe` and run it with `VK_LAYER_KHRONOS_validation`; point `VK_LAYER_SETTINGS_PATH` at `Tests/VulkanValidation` to enable core and synchronization validation. The probe owns a hidden window, renders eight frames, changes scene cache identity while submissions exist, and exits without human input. It is intentionally not a default CTest because CI hosts are not guaranteed to expose a real Vulkan presentation device.

## Engine goal

The engine owns the render path. Game projects author the data and choose values through validated config and script contracts.
