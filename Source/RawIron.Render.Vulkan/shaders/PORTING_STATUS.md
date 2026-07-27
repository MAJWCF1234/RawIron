# Reference shader migration status

The migration source initially contained 781 files: 419 `.fx` effects, 222 `.fxh` includes, 131 PNG textures,
and nine other assets. All 139 texture assets are now centralized under `shaders/NativeTextures`; 617 reference source/checklist files remain. A reference source is removed only after its native Raw Iron replacement builds in both
applicable shader paths, has a `.rishader` asset, preserves required notices, and has regression coverage.

## Completed

| Reference family | Native asset | Implementation | Textures | Verification |
| --- | --- | --- | --- | --- |
| CropResize / `Resizer.fx` | `NativeEffects/crop_resize.rishader` | Fast + extended composite | None required | `ReferenceShaderMigrationSmoke` |
| Barbatos / `uFakeHDR.fx` v3.2 | `NativeEffects/barbatos_fake_hdr.rishader` | Fast + extended composite | `Barbatos_LUT_Atlas.png` | Asset parse + SHA-256 + shader wiring |
| Shared Layer/SMAA/LUT runtime resources | Native shader bundle | Native descriptor sets | `NativeTextures/{Layer,AreaTex,SearchTex,lut}.png` | SHA-256 + staging checks |
| Complete reference texture tree | Recursive native texture bundle | `native://` asset resolver + recursive Vulkan staging | 139 files / preserved hierarchy | Full-tree staging regression |
| PD80 LUT/noise/color-space core | `NativeEffects/pd80_cinetools_lut.rishader` | Extended composite + native six-binding resource bundle | Blue-noise RGBA, permutation, 31-row Cinetools LUT | Asset paths + linear upload + real shader sampling |

## Raw Iron-owned capability replacements

These are engine-native designs. The old sources supplied only a capability checklist; Raw Iron owns the math,
resource use, parameter model, naming, fast-path behavior, and extended-path integration.

| Retired checklist sources | Raw Iron capabilities | Design |
| --- | --- | --- |
| `BaBa_Deband.fx` | `NativeEffects/ri_adaptive_deband.rishader` | Bounded scene-linear guide, rotated multi-radius pairs, gradient rejection |
| `JaSharpen.fx` | `NativeEffects/ri_local_sharpen.rishader` | Scene-linear cross guide, soft detail clamp, edge limiter |
| `S_Outline.fx` | `NativeEffects/ri_ink_outline.rishader` | Combined depth/color mask, four combination modes, wobble and debug output |
| root `Glitch.fx` | `NativeEffects/ri_signal_glitch.rishader` | Bounded row events, source-relative chroma displacement, burst quantization |
| root `NightVision.fx` | `NativeEffects/ri_night_vision.rishader` | Luminance phosphor response, stable grain, scan modulation, aspect-correct falloff |
| root `HighPassSharpen.fx` | `NativeEffects/ri_high_pass_sharpen.rishader` | Existing scene-bounded high-pass guide, soft clamp, large-edge rejection |
| root `HQ4X.fx` | `NativeEffects/ri_hq4x.rishader` | Bounded eight-neighbor edge reconstruction with finite low-luma weighting |
| root `HSLShift.fx` | `NativeEffects/ri_hsl_shift.rishader` | Eight editable circular hue anchors with chroma-aware saturation/lightness shaping |
| root `LevelsPlus.fx` | `NativeEffects/ri_levels_plus.rishader` | Per-channel in/out/gamma mapping, range shift, clipping diagnostics, three ACES options |
| root `LightDoF.fx` | `NativeEffects/ri_light_dof.rishader` | Depth circle-of-confusion, focus point/manual focus, disk blur, near/far chroma fringe |
| root `MagicBloom.fx` | `NativeEffects/ri_magic_bloom.rishader` | Bounded multi-radius bloom, local adaptation, native dirt texture, screen blend |
| root `UIMask.fx` | `NativeEffects/ri_ui_mask.rishader` | RGB-channel union mask restoring the pre-post presentation image |
| `Blending.fxh` | `NativeEffects/ri_blending.rishader` | Thirty-one finite-safe blend modes exposed by `RiBlendLayer` |
| `DrawText.fxh` | `NativeEffects/ri_text_overlay.rishader` | Native 14x7 FontAtlas binding and glyph coverage lookup |
| `Macros.fxh` | `NativeEffects/ri_shader_macros.rishader` | Typed asset parameters plus finite math/luma/UV helpers |
| `ReShade.fxh` | `NativeEffects/ri_shader_contract.rishader` | Native scene color, scene depth, viewport, time, and frame-coordinate contract |
| `ReShadeUI.fxh` | `NativeEffects/ri_shader_ui_contract.rishader` | `.rishader` typed parameters, ranges, defaults, colors, and toggles |
| `CShade/cThreshold.fx` | `NativeEffects/ri_luminance_threshold.rishader` | Finite-safe luminance isolation with a creator-controlled soft knee |
| `CShade/cQuantize.fx` | `NativeEffects/ri_color_quantize.rishader` | Bounded RGB posterization, source-grid pixelation, and stable dither choices |
| `CShade/kMirror.fx` | `NativeEffects/ri_kaleidoscope.rishader` | Bounded polar folding with configurable symmetry, segments, rotation, and zoom |

## Migration constraints

- MIT, BSD, CC0, and similarly permissive sources can be ported while preserving their notices.
- GPL sources require an explicit repository licensing decision before derivative code is merged.
- No-Derivatives and All-Rights-Reserved sources must be independently reimplemented from behavior, not copied.
- Texture assets require their own provenance check; a shader's code license does not automatically cover images.
- Multi-pass, temporal, compute, depth, motion-vector, and history-buffer effects must receive native render-graph
  resources rather than being flattened into the monolithic composite shader.
