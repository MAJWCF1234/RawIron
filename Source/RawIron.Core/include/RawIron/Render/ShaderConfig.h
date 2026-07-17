#pragma once

#include "RawIron/Render/PostProcessProfiles.h"

#include <filesystem>
#include <optional>
#include <string>

namespace ri::render {

/// Parsed `shader.cfg` (UTF-8 JSON). Optional `//` full-line comments allowed after a UTF-8 BOM.
///
/// Schema (summary):
/// - `replace` (bool): if true, `ApplyShaderConfig` replaces driven post values (time is preserved).
/// - `blendWeight` (number 0–1): weight when `replace` is false (`ApplyShaderConfig` toward file parameters).
/// - `post` / `presentation` (object): optional snake_case fields matching `PostProcessParameters`
///   (e.g. `cas_sharpen_amount`, `bloom_intensity`, `tint_color` object with r/g/b).
/// - `effects` (array, order = stack): each element has `id` or `type` (string), optional `enabled` (bool),
///   optional `blend` (0–1), optional `params` (object), optional `math` (string) for reference-math labels
///   (logged; use when two entries share a family but target different source formulas or packs).
///   Effect ids: `cas`, `bloom`, `deband`, `vignette`, `grade`, `film`, `lift_gamma_gain` / `lgg` /
///   `sweetfx_lift_gamma_gain` / `reshade_lift_gamma_gain` / `rgb_lift_gamma_gain`, `vibrance` /
///   `sweetfx_vibrance` / `reshade_vibrance`, `technicolor` (SweetFX v1.1),
///   `technicolor2` (Prod80 / SweetFX Technicolor2 — standalone 3-strip),
///   `pd80_technicolor` / `prod80_technicolor` / `pd80_04_technicolor` (PD80_04_Technicolor.fx — 2-strip + hue + optional 3-strip),
///   `pd80_color_temperature` / `prod80_color_temperature` / `pd80_04_color_temperature` (PD80_04_Color_Temperature.fx — Kelvin RGB + HSL luma),
///   `pd80_saturation_limit` / `prod80_saturation_limiter` / `pd80_04_saturation_limiter` (PD80_04_Saturation_Limit.fx — cap HSL saturation),
///   `pd80_color_balance` / `prod80_color_balance` / `pd80_04_color_balance` (PD80_04_Color_Balance.fx — shadow/mid/highlight RGB split),
///   `pd80_color_isolation` / `prod80_color_isolation` / `pd80_04_color_isolation` (PD80_04_Color_Isolation.fx — hue band isolate),
///   `pd80_levels` / `prod80_levels` / `pd80_03_levels` (PD80_03_Levels.fx — in/out levels + gamma + optional dither),
///   `pd80_black_white` / `prod80_black_white` / `pd80_04_black_white` (PD80_04_BlacknWhite.fx — ProcessBW + iq curve),
///   `pd80_contrast_brightness_saturation` / `prod80_04_ContrastBrightnessSaturation` / `pd80_04_cbs`
///   (PD80_04_Contrast_Brightness_Saturation.fx — prod80 exposure/con/bri/sat/vib + selective sat + depth),
///   `pd80_chromatic_aberration` / `prod80_06_chromaticaberration` / `pd80_06_ca`
///   (PD80_06_Chromatic_Aberration.fx — spectral ring taps; inner taps omit PD80 CA — uses graded-core chain only),
///   `sepia` / `sweetfx_tint` (Sepia.fx `Tint`: col*Tint*2.55), `monochrome` (Monochrome v1.1 presets + custom coeffs),
///   `dpx` / `sweetfx_dpx` (DPX.fx Loadus — per-channel sigmoid + XYZ/RGB matrices),
///   `color_matrix` / `sweetfx_color_matrix` (ColorMatrix.fx v1.0 — 3×3 row controls + strength),
///   `fake_hdr` / `sweetfx_fake_hdr` (FakeHDR.fx — neighbor-ring mimic HDR; distinct from engine bloom),
///   `levels` / `sweetfx_levels` (Levels.fx v1.2 — 0–255 black/white stretch; not tone curve / DPX),
///   `luma_sharpen` / `sweetfx_luma_sharpen` (LumaSharpen.fx v1.5 — luma unsharp; not CAS),
///   `sweetfx_curves` (Curves.fx — 11 contrast formulas; separate from `grade` / `curves` tone line),
///   `sweetfx_chromatic_aberration` (ChromaticAberration.fx — per-channel shift in pixels; separate from radial `chromatic_aberration`),
///   `sweetfx_border` / `sweet_fx_border` (Border.fx — aspect ratio mask or pixel border widths + color),
///   `sweetfx_cartoon` / `sweet_fx_cartoon` (Cartoon.fx — Power / EdgeSlope edge darkening),
///   `sweetfx_splitscreen` / `sweet_fx_splitscreen` (Splitscreen.fx — mode 0..6 pre/post region compare),
///   `sweetfx_nostalgia` / `sweet_fx_nostalgia` (Nostalgia.fx — palette quantization + optional dither/scanlines),
///   `sweetfx_compare` / `sweet_fx_compare` (Compare.fx — mode 0..8 compare and difference blend),
///   `sweetfx_layer` / `sweet_fx_layer` (Layer.fx v0.2 — layer UV scale/position + alpha blend; procedural stand-in for Layer.png),
///   `sweetfx_fxaa` / `sweet_fx_fxaa` (FXAA.fx 3.11 — subpix + edge thresholds; distinct from sharpening / CAS),
///   `sweetfx_crt` / `sweet_fx_crt` (CRT.fx — beam profile + curvature/corner geometry + dot-mask),
///   `sweetfx_ascii` / `sweet_fx_ascii` (ASCII.fx — luminance quantization + bitfield glyph raster + dither),
///   `sweetfx_smaa` / `sweet_fx_smaa` (SMAA.fx — morphological edge detection + directional neighborhood blend),
///   `reshade_daltonize` / `daltonize` (Daltonize.fx — LMS simulate+compensate color-vision deficiency modes),
///   `reshade_display_depth` / `display_depth` (DisplayDepth.fx — depth/normal/split debug depth visualization),
///   `reshade_lut` / `lut` (LUT.fx — strip LUT color remap with independent chroma/luma amount),
///   `colourfulness` / `colorfulness` / `sweetfx_colourfulness` (Colourfulness.fx bacondither — perceptual
///     saturation with near-clip soft limit; `colourfulness` 0 neutral, negative desaturates, positive enriches),
///   `filmic_pass` / `filmicpass` / `sweetfx_filmic_pass` (FilmicPass.fx — cinematic tone/colour curve;
///     params `strength` (0 = skip), `fade`, `bleach`, `saturation`),
///   `film_grain2` / `filmgrain2` / `sweetfx_film_grain2` (FilmGrain2.fx martinsh — animated 3D-Perlin grain,
///     distinct from the simpler SweetFX `film` grain; params `amount` (0 = skip), `color_amount`,
///     `luminance_amount`, `size`),
///   `denoise` / `sweetfx_denoise` / `nvidia_denoise` (Denoise.fx KNN — spatial noise reduction on graded LDR;
///     params `strength` (0 = skip), `noise_level`, `lerp_coefficient`, `weight_threshold`, `counter_threshold`,
///     `gaussian_sigma`),
///   `adaptive_sharpen` / `adaptivesharpen` / `reshade_adaptive_sharpen` (AdaptiveSharpen.fx bacondither —
///     edge-aware sharpening; params `strength`/`curve_height` (0 = skip), `curve_slope`, overshoot/compression
///     knobs, `scale_lim`, `scale_cs`, `pm_p`),
///   `gaussian_blur` / `gaussianblur` / `reshade_gaussian_blur` (GaussianBlur.fx Ioxa — fused separable blur on
///     graded LDR; params `strength` (0 = skip), `offset`, `radius` 0–4),
///   `fine_sharp` / `finesharp` / `reshade_fine_sharp` (FineSharp.fx Didée — fused YUV sharpen chain; params
///     `strength`/`sstr` (0 = skip), `equalization`/`cstr`, `x_strength`/`xstr`, `x_repair`/`xrep`, `l_strength`/`lstr`,
///     `p_strength`/`pstr`, `mode` 0–2),
///   `marty_bloom` / `reshade_marty_bloom` / `bloom_and_lens` (Bloom.fx Marty McFly — fused pyramid bloom;
///     params `amount`/`fBloomAmount` (0 = skip), `threshold`, `saturation`, `mix_mode` 0–3, `tint` rgb object),
///   `ring_dof` / `marty_dof` / `reshade_dof` (DOF.fx Marty McFly ring DOF — depth-aware bokeh; params `strength`
///     (0 = skip), `auto_focus`, `manual_focus`, `focus_point`, `blur_radius`, ring knobs),
///   `ambient_light` / `reshade_ambient_light` (AmbientLight.fx Ganossa — adaptive light bleed; params `intensity`
///    /`alInt` (0 = skip), `threshold`, `adapt`, dirt/adaptive modes),
///   `fake_motion_blur` / `reshade_fake_motion_blur` (FakeMotionBlur.fx — temporal smear; params `recall`/`mbRecall`
///     (0 = skip), `softness`/`mbSoftness`; uses previous-frame history texture),
///   `reflective_bump_mapping` / `rbm` / `reshade_reflective_bump_mapping` (ReflectiveBumpMapping.fx Marty McFly —
///     screen-space glossy relief reflections; params `strength` (0 = skip), `blur_width`, `sample_count`,
///     `relief_height`, fresnel/threshold/color-mask knobs),
///   `crop_resize` / `crop_scale` / `resizer` (MIT CropResize/Resizer.fx — centered content crop and final
///     presentation scaling; params `content_width`, `content_height`, `intermediate_width`,
///     `intermediate_height`, `final_width`, `final_height`, `filter` 0 point / 1 linear, `strength`),
///   `preset` (uses `preset`/`name` slug + `blend`), plus `output`.
///
/// Resolution order: see `ResolveShaderCfgPath`.
struct ShaderPresentationConfig {
    PostProcessParameters parameters{};
    bool loaded = false;
    /// When `replace` is false, blends from current `PostProcessParameters` toward `parameters`
    /// (same weighting as `BlendPostProcessParameters`).
    float blendWeight = 1.0f;
    /// If true, `ApplyShaderConfig` assigns `parameters` (still preserving `timeSeconds` on the target).
    bool replace = false;
};

/// Search order (first match):
/// 1. `<executableDirectory>/shader.cfg` when `executableDirectory` is non-empty
/// 2. `<workspace>/Assets/shader.cfg`
/// 3. `<workspace>/shader.cfg`
/// 4. `<workspace>/Content/shader.cfg`
[[nodiscard]] std::optional<std::filesystem::path> ResolveShaderCfgPath(
    const std::filesystem::path& workspaceOrGameRoot,
    const std::filesystem::path& executableDirectory = {});

/// Future world/runtime hook: apply the same `ShaderPresentationConfig` to native Vulkan frames by resolving
/// `ResolveShaderCfgPath(worldContentRoot, exeDir)` and calling `ApplyShaderConfig(frame.postProcess, cfg)`.

/// Reads and parses JSON. Returns `true` on success. On failure or missing file, returns `false`
/// and leaves `*out` unchanged (except `error` message when provided).
[[nodiscard]] bool LoadShaderCfg(const std::filesystem::path& path,
                                 ShaderPresentationConfig* out,
                                 std::string* error = nullptr);

/// Convenience: resolve under `root` and load when present.
[[nodiscard]] bool TryLoadShaderCfgFromRoot(const std::filesystem::path& workspaceOrGameRoot,
                                            ShaderPresentationConfig* out,
                                            std::string* error = nullptr);

void ApplyShaderConfig(PostProcessParameters& io, const ShaderPresentationConfig& cfg);

} // namespace ri::render
