# Reference shader migration status

The migration source initially contained 781 files: 419 `.fx` effects, 222 `.fxh` includes, 131 PNG textures,
and nine other assets. Ten files have moved out of the reference tree, leaving 771. A reference source is removed only after its native Raw Iron replacement builds in both
applicable shader paths, has a `.rishader` asset, preserves required notices, and has regression coverage.

## Completed

| Reference family | Native asset | Implementation | Textures | Verification |
| --- | --- | --- | --- | --- |
| CropResize / `Resizer.fx` | `NativeEffects/crop_resize.rishader` | Fast + extended composite | None required | `ReferenceShaderMigrationSmoke` |
| Barbatos / `uFakeHDR.fx` v3.2 | `NativeEffects/barbatos_fake_hdr.rishader` | Fast + extended composite | `Barbatos_LUT_Atlas.png` | Asset parse + SHA-256 + shader wiring |
| Shared Layer/SMAA/LUT runtime resources | Native shader bundle | Native descriptor sets | `NativeTextures/{Layer,AreaTex,SearchTex,lut}.png` | SHA-256 + staging checks |

## Raw Iron-owned capability replacements

These are engine-native designs. The old sources supplied only a capability checklist; Raw Iron owns the math,
resource use, parameter model, naming, fast-path behavior, and extended-path integration.

| Retired checklist sources | Raw Iron capabilities | Design |
| --- | --- | --- |
| `BaBa_Deband.fx` | `NativeEffects/ri_adaptive_deband.rishader` | Bounded scene-linear guide, rotated multi-radius pairs, gradient rejection |
| `JaSharpen.fx` | `NativeEffects/ri_local_sharpen.rishader` | Scene-linear cross guide, soft detail clamp, edge limiter |
| `S_Outline.fx` | `NativeEffects/ri_ink_outline.rishader` | Combined depth/color mask, four combination modes, wobble and debug output |

## Migration constraints

- MIT, BSD, CC0, and similarly permissive sources can be ported while preserving their notices.
- GPL sources require an explicit repository licensing decision before derivative code is merged.
- No-Derivatives and All-Rights-Reserved sources must be independently reimplemented from behavior, not copied.
- Texture assets require their own provenance check; a shader's code license does not automatically cover images.
- Multi-pass, temporal, compute, depth, motion-vector, and history-buffer effects must receive native render-graph
  resources rather than being flattened into the monolithic composite shader.
