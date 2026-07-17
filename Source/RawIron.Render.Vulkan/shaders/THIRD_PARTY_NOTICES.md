# Native shader-port notices

Raw Iron's native shader sources contain clean integrations or permitted ports of the effects listed below.
The original ReShade-format sources are removed only after the replacement is built and covered by regression tests.

## Crop and Resize

The centered crop/resize mapping in `NativeComposite.frag` and `NativeHybridComposite.frag` is based on
`CropResize/Resizer.fx` by Edward Jeffrey and its VirtualResolution-derived portions by Lucas Melo.

Copyright (C) 2025 Edward Jeffrey
Copyright (c) 2017 Lucas Melo

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
documentation files (the "Software"), to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and
to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

# Barbatos uFakeHDR

`NativeEffects/barbatos_fake_hdr.rishader`, its native composite implementation, and
`NativeTextures/Barbatos/Barbatos_LUT_Atlas.png` are derived from uFakeHDR version 3.2 by Barbatos.
The source declares the work CC0 (public-domain dedication).

## PD80 Cinetools LUT core

`NativeEffects/pd80_cinetools_lut.rishader` and `ApplyPd80CinetoolsLut` preserve the behavior of the
PD80 LUT-v2 workflow by prod80, based on earlier LUT work credited in that source to Ganossa,
Marty McFly, and Otis Inf. Raw Iron supplies its own Vulkan descriptor model, shader entry point,
parameter packing, atlas binding, and blue-noise integration. The migrated reference helper files
`PD80_LUT_v2.fxh`, `PD80_00_Noise_Samplers.fxh`, and `PD80_00_Color_Spaces.fxh` have been removed.

## Raw Iron signal, night-display, and high-pass operators

`NativeEffects/ri_signal_glitch.rishader`, `ri_night_vision.rishader`, and
`ri_high_pass_sharpen.rishader` are independent Raw Iron implementations. The former root
`Glitch.fx`, `NightVision.fx`, and Ioxa's `HighPassSharpen.fx` were used only to identify desired
engine capabilities; their sampling math, parameter model, timing behavior, resource access, and
fast/extended integration were not copied. Those checklist sources have been removed.

## Native root shader and helper tranche

The Raw Iron assets `ri_hq4x.rishader`, `ri_hsl_shift.rishader`, `ri_levels_plus.rishader`,
`ri_light_dof.rishader`, `ri_magic_bloom.rishader`, and `ri_ui_mask.rishader` are native Vulkan
implementations integrated with Raw Iron's bounded scene color, depth buffer, typed profile API, and
fast/extended composites. Capability references and credits: the Shadertoy HQ4X reference, HSLShift
and DrawText by kingeric1992, LevelsPlus by Christian Cann Schuldt Jensen and Kirill Yarovoy,
Light DoF by luluco250, Magic Bloom by luluco250, and UIMask by Lucas Melo. Light DoF was treated as
a behavior checklist rather than copied because its source is CC BY-SA 4.0. Magic Bloom and UIMask
carry MIT notices in their former sources; Raw Iron retains attribution while using its own render
resource model and independently structured math.

The native blend library covers the mode set credited by the former `Blending.fxh` to originalnicodr,
prod80, uchu suzume, and Marot Satil, using finite-safe Raw Iron formulas. `ri_shader_contract.rishader`
and `ri_shader_ui_contract.rishader` replace the CC0 ReShade compatibility headers with engine-native
scene/depth and typed-parameter contracts. `ri_shader_macros.rishader` replaces TreyM/dddfault UI and
resource macros with schema-validated assets. `NativeTextures/FontAtlas.png`,
`NativeTextures/MagicBloom_Dirt.png`, and `NativeTextures/brussell/UIDetectMaskRGB.png` are bound and
sampled by the native composites. All eleven former root reference files have been removed.
