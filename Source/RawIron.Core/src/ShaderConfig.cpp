#include "RawIron/Render/ShaderConfig.h"

#include "RawIron/Core/Detail/JsonScan.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Math/Vec2.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#if defined(_MSC_VER)
// Alias fallback chains intentionally reuse short scoped binding names for equivalent
// legacy/current JSON keys. C4456 treats those disjoint if-initializer scopes as shadowing.
#pragma warning(disable : 4456)
#endif

namespace ri::render {
namespace {

namespace fs = std::filesystem;
namespace detail = ri::core::detail;

[[nodiscard]] std::string StripUtf8BomAndLineComments(std::string_view utf8) {
    if (utf8.size() >= 3U && static_cast<unsigned char>(utf8[0]) == 0xEFU
        && static_cast<unsigned char>(utf8[1]) == 0xBBU && static_cast<unsigned char>(utf8[2]) == 0xBFU) {
        utf8.remove_prefix(3U);
    }
    std::string out;
    out.reserve(utf8.size());
    std::size_t lineStart = 0;
    while (lineStart < utf8.size()) {
        const std::size_t newline = utf8.find('\n', lineStart);
        const std::size_t lineEnd = newline == std::string_view::npos ? utf8.size() : newline;
        const std::string_view line = utf8.substr(lineStart, lineEnd - lineStart);
        const std::size_t t = detail::SkipWhitespace(line, 0);
        const bool comment = t + 1U < line.size() && line[t] == '/' && line[t + 1U] == '/';
        if (!comment) {
            out.append(line.data(), line.size());
            out.push_back('\n');
        }
        lineStart = lineEnd == utf8.size() ? lineEnd : lineEnd + 1U;
    }
    return out;
}

[[nodiscard]] bool ScanJsonNumberToken(std::string_view text, std::size_t start, std::size_t* consumedOut, double* valueOut) {
    std::size_t index = start;
    if (index < text.size() && (text[index] == '-' || text[index] == '+')) {
        ++index;
    }
    while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
        ++index;
    }
    if (index < text.size() && text[index] == '.') {
        ++index;
        while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
            ++index;
        }
    }
    if (index < text.size() && (text[index] == 'e' || text[index] == 'E')) {
        ++index;
        if (index < text.size() && (text[index] == '-' || text[index] == '+')) {
            ++index;
        }
        while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
            ++index;
        }
    }
    if (index == start) {
        return false;
    }
    const std::string buffer(text.substr(start, index - start));
    char* parseEnd = nullptr;
    const double parsed = std::strtod(buffer.c_str(), &parseEnd);
    if (parseEnd == buffer.c_str()) {
        return false;
    }
    *valueOut = parsed;
    *consumedOut = index;
    return true;
}

[[nodiscard]] std::optional<ri::math::Vec3> TryParseJsonVec3(std::string_view text, std::string_view key) {
    const std::optional<std::size_t> valueIndex = detail::FindJsonKey(text, key);
    if (!valueIndex.has_value()) {
        return std::nullopt;
    }
    std::size_t i = detail::SkipWhitespace(text, *valueIndex);
    if (i >= text.size()) {
        return std::nullopt;
    }
    if (text[i] == '{') {
        const std::optional<std::string_view> obj = detail::ExtractJsonObject(text, key);
        if (!obj.has_value()) {
            return std::nullopt;
        }
        ri::math::Vec3 v{1.0f, 1.0f, 1.0f};
        if (const std::optional<double> r = detail::ExtractJsonDouble(*obj, "r")) {
            v.x = static_cast<float>(*r);
        }
        if (const std::optional<double> g = detail::ExtractJsonDouble(*obj, "g")) {
            v.y = static_cast<float>(*g);
        }
        if (const std::optional<double> b = detail::ExtractJsonDouble(*obj, "b")) {
            v.z = static_cast<float>(*b);
        }
        return v;
    }
    if (text[i] != '[') {
        return std::nullopt;
    }
    ++i;
    ri::math::Vec3 v{};
    for (int c = 0; c < 3; ++c) {
        i = detail::SkipWhitespace(text, i);
        std::size_t consumed = 0;
        double num = 0.0;
        if (!ScanJsonNumberToken(text, i, &consumed, &num)) {
            return std::nullopt;
        }
        (&v.x)[c] = static_cast<float>(num);
        i = detail::SkipWhitespace(text, consumed);
        if (c < 2) {
            if (i >= text.size() || text[i] != ',') {
                return std::nullopt;
            }
            ++i;
        }
    }
    i = detail::SkipWhitespace(text, i);
    if (i >= text.size() || text[i] != ']') {
        return std::nullopt;
    }
    return v;
}

[[nodiscard]] std::optional<ri::math::Vec2> TryParseJsonVec2(std::string_view text, std::string_view key) {
    const std::optional<std::size_t> valueIndex = detail::FindJsonKey(text, key);
    if (!valueIndex.has_value()) {
        return std::nullopt;
    }
    std::size_t i = detail::SkipWhitespace(text, *valueIndex);
    if (i >= text.size() || text[i] != '[') {
        return std::nullopt;
    }
    ++i;
    ri::math::Vec2 v{};
    for (int c = 0; c < 2; ++c) {
        i = detail::SkipWhitespace(text, i);
        std::size_t consumed = 0;
        double num = 0.0;
        if (!ScanJsonNumberToken(text, i, &consumed, &num)) {
            return std::nullopt;
        }
        (&v.x)[c] = static_cast<float>(num);
        i = detail::SkipWhitespace(text, consumed);
        if (c < 1) {
            if (i >= text.size() || text[i] != ',') {
                return std::nullopt;
            }
            ++i;
        }
    }
    i = detail::SkipWhitespace(text, i);
    if (i >= text.size() || text[i] != ']') {
        return std::nullopt;
    }
    return v;
}

std::string NormalizeToken(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    bool wroteSeparator = false;
    for (char ch : value) {
        const unsigned char code = static_cast<unsigned char>(ch);
        if (std::isalnum(code) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(code)));
            wroteSeparator = false;
            continue;
        }
        if (ch == '_' || ch == '-' || std::isspace(code) != 0) {
            if (!normalized.empty() && !wroteSeparator) {
                normalized.push_back('_');
                wroteSeparator = true;
            }
        }
    }
    while (!normalized.empty() && normalized.back() == '_') {
        normalized.pop_back();
    }
    return normalized;
}

void MergePresentationObject(std::string_view obj, PostProcessParameters& p) {
    const auto set = [&obj](const char* key, float& dest) {
        if (const std::optional<double> v = detail::ExtractJsonDouble(obj, key)) {
            dest = static_cast<float>(*v);
        }
    };
    set("noise_amount", p.noiseAmount);
    set("scanline_amount", p.scanlineAmount);
    set("barrel_distortion", p.barrelDistortion);
    set("chromatic_aberration", p.chromaticAberration);
    set("tint_strength", p.tintStrength);
    set("blur_amount", p.blurAmount);
    set("static_fade_amount", p.staticFadeAmount);
    set("cas_sharpen_amount", p.casSharpenAmount);
    set("cas_contrast_adaptation", p.casContrastAdaptation);
    set("bloom_intensity", p.bloomIntensity);
    set("bloom_threshold", p.bloomThreshold);
    set("deband_strength", p.debandStrength);
    set("tone_curve_strength", p.toneCurveStrength);
    set("output_dither_strength", p.outputDitherStrength);
    set("vignette_strength", p.vignetteStrength);
    set("film_grain_intensity", p.filmGrainIntensity);
    set("lift_gamma_gain_mix", p.liftGammaGainMix);
    set("vibrance", p.vibrance);
    set("technicolor_power", p.technicolorPower);
    set("technicolor_strength", p.technicolorStrength);
    set("technicolor2_brightness", p.technicolor2Brightness);
    set("technicolor2_saturation", p.technicolor2Saturation);
    set("technicolor2_strength", p.technicolor2Strength);
    set("sepia_strength", p.sepiaStrength);
    set("monochrome_color_saturation", p.monochromeColorSaturation);
    set("dpx_contrast", p.dpxContrast);
    set("dpx_saturation", p.dpxSaturation);
    set("dpx_colorfulness", p.dpxColorfulness);
    set("dpx_strength", p.dpxStrength);
    set("color_matrix_strength", p.colorMatrixStrength);
    set("fake_hdr_power", p.fakeHdrPower);
    set("fake_hdr_radius1", p.fakeHdrRadius1);
    set("fake_hdr_radius2", p.fakeHdrRadius2);
    set("fake_hdr_strength", p.fakeHdrStrength);
    set("levels_black_point", p.levelsBlackPoint);
    set("levels_white_point", p.levelsWhitePoint);
    set("levels_strength", p.levelsStrength);
    set("levels_clip_highlight", p.levelsClipHighlight);
    set("luma_sharpen_strength", p.lumaSharpenStrength);
    set("luma_sharpen_clamp", p.lumaSharpenClamp);
    set("luma_sharpen_offset_bias", p.lumaSharpenOffsetBias);
    set("luma_sharpen_show_pattern", p.lumaSharpenShowPattern);
    set("sweet_fx_curves_contrast", p.sweetFxCurvesContrast);
    set("sweet_fx_curves_strength", p.sweetFxCurvesStrength);
    set("sweet_fx_chromatic_aberration_shift_x", p.sweetFxChromaticAberrationShiftX);
    set("sweet_fx_chromatic_aberration_shift_y", p.sweetFxChromaticAberrationShiftY);
    set("sweet_fx_chromatic_aberration_strength", p.sweetFxChromaticAberrationStrength);
    set("sweet_fx_border_width_x", p.sweetFxBorderWidthX);
    set("sweet_fx_border_width_y", p.sweetFxBorderWidthY);
    set("sweet_fx_border_ratio", p.sweetFxBorderRatio);
    set("sweet_fx_border_strength", p.sweetFxBorderStrength);
    set("sweet_fx_cartoon_power", p.sweetFxCartoonPower);
    set("sweet_fx_cartoon_edge_slope", p.sweetFxCartoonEdgeSlope);
    set("sweet_fx_cartoon_strength", p.sweetFxCartoonStrength);
    set("sweet_fx_tonemap_gamma", p.sweetFxTonemapGamma);
    set("sweet_fx_tonemap_exposure", p.sweetFxTonemapExposure);
    set("sweet_fx_tonemap_saturation", p.sweetFxTonemapSaturation);
    set("sweet_fx_tonemap_bleach", p.sweetFxTonemapBleach);
    set("sweet_fx_tonemap_defog", p.sweetFxTonemapDefog);
    set("sweet_fx_tonemap_strength", p.sweetFxTonemapStrength);
    set("sweet_fx_splitscreen_strength", p.sweetFxSplitscreenStrength);
    set("sweet_fx_nostalgia_dither", p.sweetFxNostalgiaDither);
    set("sweet_fx_nostalgia_strength", p.sweetFxNostalgiaStrength);
    set("sweet_fx_compare_difference_scale", p.sweetFxCompareDifferenceScale);
    set("sweet_fx_compare_strength", p.sweetFxCompareStrength);
    set("sweet_fx_layer_scale", p.sweetFxLayerScale);
    set("sweet_fx_layer_blend", p.sweetFxLayerBlend);
    set("sweet_fx_layer_tex_width", p.sweetFxLayerTexWidth);
    set("sweet_fx_layer_tex_height", p.sweetFxLayerTexHeight);
    set("sweet_fx_fxaa_subpix", p.sweetFxFxaaSubpix);
    set("sweet_fx_fxaa_edge_threshold", p.sweetFxFxaaEdgeThreshold);
    set("sweet_fx_fxaa_edge_threshold_min", p.sweetFxFxaaEdgeThresholdMin);
    set("sweet_fx_fxaa_strength", p.sweetFxFxaaStrength);
    set("sweet_fx_crt_amount", p.sweetFxCrtAmount);
    set("sweet_fx_crt_resolution", p.sweetFxCrtResolution);
    set("sweet_fx_crt_gamma", p.sweetFxCrtGamma);
    set("sweet_fx_crt_monitor_gamma", p.sweetFxCrtMonitorGamma);
    set("sweet_fx_crt_brightness", p.sweetFxCrtBrightness);
    set("sweet_fx_crt_scanline_gaussian", p.sweetFxCrtScanlineGaussian);
    set("sweet_fx_crt_curvature", p.sweetFxCrtCurvature);
    set("sweet_fx_crt_curvature_radius", p.sweetFxCrtCurvatureRadius);
    set("sweet_fx_crt_corner_size", p.sweetFxCrtCornerSize);
    set("sweet_fx_crt_viewer_distance", p.sweetFxCrtViewerDistance);
    set("sweet_fx_crt_overscan", p.sweetFxCrtOverscan);
    set("sweet_fx_crt_oversample", p.sweetFxCrtOversample);
    set("sweet_fx_ascii_swap_colors", p.sweetFxAsciiSwapColors);
    set("sweet_fx_ascii_invert_brightness", p.sweetFxAsciiInvertBrightness);
    set("sweet_fx_ascii_dithering", p.sweetFxAsciiDithering);
    set("sweet_fx_ascii_dithering_intensity", p.sweetFxAsciiDitheringIntensity);
    set("sweet_fx_ascii_dithering_debug_gradient", p.sweetFxAsciiDitheringDebugGradient);
    set("sweet_fx_ascii_strength", p.sweetFxAsciiStrength);
    set("sweet_fx_smaa_edge_threshold", p.sweetFxSmaaEdgeThreshold);
    set("sweet_fx_smaa_depth_threshold", p.sweetFxSmaaDepthThreshold);
    set("sweet_fx_smaa_debug_output", p.sweetFxSmaaDebugOutput);
    set("sweet_fx_smaa_strength", p.sweetFxSmaaStrength);
    set("reshade_daltonize_strength", p.reshadeDaltonizeStrength);
    set("reshade_display_depth_strength", p.reshadeDisplayDepthStrength);
    if (const std::optional<double> compositeBypass = detail::ExtractJsonDouble(obj, "composite_bypass")) {
        p.reshadeDisplayDepthStrength = *compositeBypass >= 0.5 ? 2.0f : 0.0f;
    }
    set("reshade_lut_amount_chroma", p.reshadeLutAmountChroma);
    set("reshade_lut_amount_luma", p.reshadeLutAmountLuma);
    set("reshade_lut_strength", p.reshadeLutStrength);
    set("pd80_technicolor_strength", p.pd80TechnicolorStrength);
    set("pd80_technicolor_saturation_2", p.pd80TechnicolorSaturation2);
    set("pd80_technicolor_3_brightness", p.pd80Technicolor3Brightness);
    set("pd80_technicolor_3_saturation", p.pd80Technicolor3Saturation);
    set("pd80_technicolor_3_strength", p.pd80Technicolor3Strength);
    set("pd80_color_temperature_kelvin", p.pd80ColorTemperatureKelvin);
    set("pd80_color_temperature_luminance_preservation", p.pd80ColorTemperatureLuminancePreservation);
    set("pd80_color_temperature_mix", p.pd80ColorTemperatureMix);
    set("pd80_color_temperature_strength", p.pd80ColorTemperatureStrength);
    set("pd80_saturation_limit", p.pd80SaturationLimit);
    set("pd80_saturation_limit_strength", p.pd80SaturationLimitStrength);
    set("pd80_color_balance_preserve_luma", p.pd80ColorBalancePreserveLuma);
    set("pd80_color_balance_separation_mode", p.pd80ColorBalanceSeparationMode);
    set("pd80_color_balance_strength", p.pd80ColorBalanceStrength);
    set("pd80_color_isolation_hue_mid", p.pd80ColorIsolationHueMid);
    set("pd80_color_isolation_hue_range", p.pd80ColorIsolationHueRange);
    set("pd80_color_isolation_sat_limit", p.pd80ColorIsolationSatLimit);
    set("pd80_color_isolation_fx_mix", p.pd80ColorIsolationFxMix);
    set("pd80_color_isolation_strength", p.pd80ColorIsolationStrength);
    set("pd80_levels_gamma", p.pd80LevelsGamma);
    set("pd80_levels_dither_strength", p.pd80LevelsDitherStrength);
    set("pd80_levels_strength", p.pd80LevelsStrength);
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_levels_enable_dither")) {
        p.pd80LevelsEnableDither = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_levels_enable_dither", p.pd80LevelsEnableDither);
    }
    set("pd80_black_white_mode", p.pd80BlackWhiteMode);
    set("pd80_black_white_curve_str", p.pd80BlackWhiteCurveStr);
    set("pd80_black_white_dither_strength", p.pd80BlackWhiteDitherStrength);
    set("pd80_black_white_red_channel", p.pd80BlackWhiteRedChannel);
    set("pd80_black_white_yellow_channel", p.pd80BlackWhiteYellowChannel);
    set("pd80_black_white_green_channel", p.pd80BlackWhiteGreenChannel);
    set("pd80_black_white_cyan_channel", p.pd80BlackWhiteCyanChannel);
    set("pd80_black_white_blue_channel", p.pd80BlackWhiteBlueChannel);
    set("pd80_black_white_magenta_channel", p.pd80BlackWhiteMagentaChannel);
    set("pd80_black_white_tint_hue", p.pd80BlackWhiteTintHue);
    set("pd80_black_white_tint_sat", p.pd80BlackWhiteTintSat);
    set("pd80_black_white_strength", p.pd80BlackWhiteStrength);
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_black_white_enable_dither")) {
        p.pd80BlackWhiteEnableDither = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_black_white_enable_dither", p.pd80BlackWhiteEnableDither);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_black_white_use_tint")) {
        p.pd80BlackWhiteUseTint = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_black_white_use_tint", p.pd80BlackWhiteUseTint);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_black_white_show_clip")) {
        p.pd80BlackWhiteShowClip = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_black_white_show_clip", p.pd80BlackWhiteShowClip);
    }
    set("pd80_cbs_dither_strength", p.pd80CbsDitherStrength);
    set("pd80_cbs_tint", p.pd80CbsTint);
    set("pd80_cbs_exposure", p.pd80CbsExposure);
    set("pd80_cbs_contrast", p.pd80CbsContrast);
    set("pd80_cbs_brightness", p.pd80CbsBrightness);
    set("pd80_cbs_saturation", p.pd80CbsSaturation);
    set("pd80_cbs_vibrance", p.pd80CbsVibrance);
    set("pd80_cbs_hue_mid", p.pd80CbsHueMid);
    set("pd80_cbs_hue_range", p.pd80CbsHueRange);
    set("pd80_cbs_sat_custom", p.pd80CbsSatCustom);
    set("pd80_cbs_sat_r", p.pd80CbsSatR);
    set("pd80_cbs_sat_y", p.pd80CbsSatY);
    set("pd80_cbs_sat_g", p.pd80CbsSatG);
    set("pd80_cbs_sat_a", p.pd80CbsSatA);
    set("pd80_cbs_sat_b", p.pd80CbsSatB);
    set("pd80_cbs_sat_p", p.pd80CbsSatP);
    set("pd80_cbs_sat_m", p.pd80CbsSatM);
    set("pd80_cbs_depth_start", p.pd80CbsDepthStart);
    set("pd80_cbs_depth_end", p.pd80CbsDepthEnd);
    set("pd80_cbs_depth_curve", p.pd80CbsDepthCurve);
    set("pd80_cbs_exposure_far", p.pd80CbsExposureFar);
    set("pd80_cbs_contrast_far", p.pd80CbsContrastFar);
    set("pd80_cbs_brightness_far", p.pd80CbsBrightnessFar);
    set("pd80_cbs_saturation_far", p.pd80CbsSaturationFar);
    set("pd80_cbs_vibrance_far", p.pd80CbsVibranceFar);
    set("pd80_cbs_strength", p.pd80CbsStrength);
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_cbs_enable_dither")) {
        p.pd80CbsEnableDither = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_cbs_enable_dither", p.pd80CbsEnableDither);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_cbs_enable_depth")) {
        p.pd80CbsEnableDepth = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_cbs_enable_depth", p.pd80CbsEnableDepth);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_cbs_display_depth")) {
        p.pd80CbsDisplayDepth = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_cbs_display_depth", p.pd80CbsDisplayDepth);
    }
    set("pd80_ca_master_strength", p.pd80CaMasterStrength);
    set("pd80_ca_effect_strength", p.pd80CaEffectStrength);
    set("pd80_ca_global_width", p.pd80CaGlobalWidth);
    set("pd80_ca_sample_steps", p.pd80CaSampleSteps);
    set("pd80_ca_type", p.pd80CaType);
    set("pd80_ca_degrees", p.pd80CaDegrees);
    set("pd80_ca_width", p.pd80CaWidth);
    set("pd80_ca_curve", p.pd80CaCurve);
    set("pd80_ca_o_x", p.pd80CaOX);
    set("pd80_ca_o_y", p.pd80CaOY);
    set("pd80_ca_shape_x", p.pd80CaShapeX);
    set("pd80_ca_shape_y", p.pd80CaShapeY);
    set("pd80_ca_depth_start", p.pd80CaDepthStart);
    set("pd80_ca_depth_end", p.pd80CaDepthEnd);
    set("pd80_ca_depth_curve", p.pd80CaDepthCurve);
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_ca_show_ca")) {
        p.pd80CaShowCa = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_ca_show_ca", p.pd80CaShowCa);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_ca_enable_depth_int")) {
        p.pd80CaEnableDepthInt = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_ca_enable_depth_int", p.pd80CaEnableDepthInt);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_ca_enable_depth_width")) {
        p.pd80CaEnableDepthWidth = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_ca_enable_depth_width", p.pd80CaEnableDepthWidth);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_ca_display_depth")) {
        p.pd80CaDisplayDepth = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_ca_display_depth", p.pd80CaDisplayDepth);
    }
    if (const auto v = TryParseJsonVec3(obj, "pd80_ca_vignette_color")) {
        p.pd80CaVignetteColor = *v;
    }
    set("pd80_ls_master_strength", p.pd80LsMasterStrength);
    set("pd80_ls_blur_sigma", p.pd80LsBlurSigma);
    set("pd80_ls_sharpening", p.pd80LsSharpening);
    set("pd80_ls_threshold", p.pd80LsThreshold);
    set("pd80_ls_limiter", p.pd80LsLimiter);
    set("pd80_ls_depth_start", p.pd80LsDepthStart);
    set("pd80_ls_depth_end", p.pd80LsDepthEnd);
    set("pd80_ls_depth_curve", p.pd80LsDepthCurve);
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_ls_show_edges")) {
        p.pd80LsShowEdges = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_ls_show_edges", p.pd80LsShowEdges);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_ls_enable_depth")) {
        p.pd80LsEnableDepth = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_ls_enable_depth", p.pd80LsEnableDepth);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_ls_enable_reverse")) {
        p.pd80LsEnableReverse = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_ls_enable_reverse", p.pd80LsEnableReverse);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_ls_display_depth")) {
        p.pd80LsDisplayDepth = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_ls_display_depth", p.pd80LsDisplayDepth);
    }
    set("pd80_fg_master_strength", p.pd80FgMasterStrength);
    set("pd80_fg_grain_adjust", p.pd80FgGrainAdjust);
    set("pd80_fg_grain_size", p.pd80FgGrainSize);
    set("pd80_fg_grain_color", p.pd80FgGrainColor);
    set("pd80_fg_grain_amount", p.pd80FgGrainAmount);
    set("pd80_fg_grain_intensity", p.pd80FgGrainIntensity);
    set("pd80_fg_grain_density", p.pd80FgGrainDensity);
    set("pd80_fg_grain_int_high", p.pd80FgGrainIntHigh);
    set("pd80_fg_grain_int_low", p.pd80FgGrainIntLow);
    set("pd80_fg_depth_start", p.pd80FgDepthStart);
    set("pd80_fg_depth_end", p.pd80FgDepthEnd);
    set("pd80_fg_depth_curve", p.pd80FgDepthCurve);
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_fg_grain_motion")) {
        p.pd80FgGrainMotion = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_fg_grain_motion", p.pd80FgGrainMotion);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_fg_grain_orig_color")) {
        p.pd80FgGrainOrigColor = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_fg_grain_orig_color", p.pd80FgGrainOrigColor);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_fg_use_negnoise")) {
        p.pd80FgUseNegnoise = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_fg_use_negnoise", p.pd80FgUseNegnoise);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_fg_enable_test")) {
        p.pd80FgEnableTest = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_fg_enable_test", p.pd80FgEnableTest);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_fg_enable_depth")) {
        p.pd80FgEnableDepth = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_fg_enable_depth", p.pd80FgEnableDepth);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_fg_display_depth")) {
        p.pd80FgDisplayDepth = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_fg_display_depth", p.pd80FgDisplayDepth);
    }

    set("pd80_ds_master_strength", p.pd80DsMasterStrength);
    set("pd80_ds_depth_near", p.pd80DsDepthNear);
    set("pd80_ds_depth_pos", p.pd80DsDepthPos);
    set("pd80_ds_depth_far", p.pd80DsDepthFar);
    set("pd80_ds_depth_smoothing", p.pd80DsDepthSmoothing);
    set("pd80_ds_intensity", p.pd80DsIntensity);
    set("pd80_ds_hue", p.pd80DsHue);
    set("pd80_ds_saturation", p.pd80DsSaturation);
    set("pd80_ds_blend_mode", p.pd80DsBlendMode);
    set("pd80_ds_opacity", p.pd80DsOpacity);
    set("pd80_cg_master_strength", p.pd80CgMasterStrength);
    set("pd80_color_gamut", p.pd80ColorGamut);
    set("pd80_csc_master_strength", p.pd80CscMasterStrength);
    set("pd80_csc_dither_strength", p.pd80CscDitherStrength);
    set("pd80_csc_color_space", p.pd80CscColorSpace);
    set("pd80_csc_pos0_toe_grey", p.pd80CscPos0ToeGrey);
    set("pd80_csc_pos1_toe_grey", p.pd80CscPos1ToeGrey);
    set("pd80_csc_pos0_shoulder_grey", p.pd80CscPos0ShoulderGrey);
    set("pd80_csc_pos1_shoulder_grey", p.pd80CscPos1ShoulderGrey);
    set("pd80_csc_color_sat", p.pd80CscColorSat);
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_csc_enable_dither")) {
        p.pd80CscEnableDither = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_csc_enable_dither", p.pd80CscEnableDither);
    }

    set("pd80_smh_master_strength", p.pd80SmhMasterStrength);
    set("pd80_smh_luma_mode", p.pd80SmhLumaMode);
    set("pd80_smh_separation_mode", p.pd80SmhSeparationMode);
    set("pd80_smh_dither_strength", p.pd80SmhDitherStrength);
    set("pd80_smh_shadow_exposure", p.pd80SmhShadowExposure);
    set("pd80_smh_shadow_contrast", p.pd80SmhShadowContrast);
    set("pd80_smh_shadow_brightness", p.pd80SmhShadowBrightness);
    set("pd80_smh_shadow_blend_mode", p.pd80SmhShadowBlendMode);
    set("pd80_smh_shadow_opacity", p.pd80SmhShadowOpacity);
    set("pd80_smh_shadow_tint", p.pd80SmhShadowTint);
    set("pd80_smh_shadow_saturation", p.pd80SmhShadowSaturation);
    set("pd80_smh_shadow_vibrance", p.pd80SmhShadowVibrance);
    set("pd80_smh_mid_exposure", p.pd80SmhMidExposure);
    set("pd80_smh_mid_contrast", p.pd80SmhMidContrast);
    set("pd80_smh_mid_brightness", p.pd80SmhMidBrightness);
    set("pd80_smh_mid_blend_mode", p.pd80SmhMidBlendMode);
    set("pd80_smh_mid_opacity", p.pd80SmhMidOpacity);
    set("pd80_smh_mid_tint", p.pd80SmhMidTint);
    set("pd80_smh_mid_saturation", p.pd80SmhMidSaturation);
    set("pd80_smh_mid_vibrance", p.pd80SmhMidVibrance);
    set("pd80_smh_highlight_exposure", p.pd80SmhHighlightExposure);
    set("pd80_smh_highlight_contrast", p.pd80SmhHighlightContrast);
    set("pd80_smh_highlight_brightness", p.pd80SmhHighlightBrightness);
    set("pd80_smh_highlight_blend_mode", p.pd80SmhHighlightBlendMode);
    set("pd80_smh_highlight_opacity", p.pd80SmhHighlightOpacity);
    set("pd80_smh_highlight_tint", p.pd80SmhHighlightTint);
    set("pd80_smh_highlight_saturation", p.pd80SmhHighlightSaturation);
    set("pd80_smh_highlight_vibrance", p.pd80SmhHighlightVibrance);
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_smh_enable_dither")) {
        p.pd80SmhEnableDither = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_smh_enable_dither", p.pd80SmhEnableDither);
    }
    if (const auto v = TryParseJsonVec3(obj, "pd80_smh_shadow_blend_color")) {
        p.pd80SmhBlendColorShadow = *v;
    }
    if (const auto v = TryParseJsonVec3(obj, "pd80_smh_mid_blend_color")) {
        p.pd80SmhBlendColorMid = *v;
    }
    if (const auto v = TryParseJsonVec3(obj, "pd80_smh_highlight_blend_color")) {
        p.pd80SmhBlendColorHighlight = *v;
    }

    set("pd80_cl_master_strength", p.pd80ClMasterStrength);
    set("pd80_cl_dither_strength", p.pd80ClDitherStrength);
    set("pd80_cl_enable_rgb", p.pd80ClEnableRgb);
    set("pd80_cl_grey_black_in", p.pd80ClGreyBlackIn);
    set("pd80_cl_grey_white_in", p.pd80ClGreyWhiteIn);
    set("pd80_cl_grey_black_out", p.pd80ClGreyBlackOut);
    set("pd80_cl_grey_white_out", p.pd80ClGreyWhiteOut);
    set("pd80_cl_grey_pos0_shoulder", p.pd80ClGreyPos0Shoulder);
    set("pd80_cl_grey_pos1_shoulder", p.pd80ClGreyPos1Shoulder);
    set("pd80_cl_grey_pos0_toe", p.pd80ClGreyPos0Toe);
    set("pd80_cl_grey_pos1_toe", p.pd80ClGreyPos1Toe);
    set("pd80_cl_red_black_in", p.pd80ClRedBlackIn);
    set("pd80_cl_red_white_in", p.pd80ClRedWhiteIn);
    set("pd80_cl_red_black_out", p.pd80ClRedBlackOut);
    set("pd80_cl_red_white_out", p.pd80ClRedWhiteOut);
    set("pd80_cl_red_pos0_shoulder", p.pd80ClRedPos0Shoulder);
    set("pd80_cl_red_pos1_shoulder", p.pd80ClRedPos1Shoulder);
    set("pd80_cl_red_pos0_toe", p.pd80ClRedPos0Toe);
    set("pd80_cl_red_pos1_toe", p.pd80ClRedPos1Toe);
    set("pd80_cl_green_black_in", p.pd80ClGreenBlackIn);
    set("pd80_cl_green_white_in", p.pd80ClGreenWhiteIn);
    set("pd80_cl_green_black_out", p.pd80ClGreenBlackOut);
    set("pd80_cl_green_white_out", p.pd80ClGreenWhiteOut);
    set("pd80_cl_green_pos0_shoulder", p.pd80ClGreenPos0Shoulder);
    set("pd80_cl_green_pos1_shoulder", p.pd80ClGreenPos1Shoulder);
    set("pd80_cl_green_pos0_toe", p.pd80ClGreenPos0Toe);
    set("pd80_cl_green_pos1_toe", p.pd80ClGreenPos1Toe);
    set("pd80_cl_blue_black_in", p.pd80ClBlueBlackIn);
    set("pd80_cl_blue_white_in", p.pd80ClBlueWhiteIn);
    set("pd80_cl_blue_black_out", p.pd80ClBlueBlackOut);
    set("pd80_cl_blue_white_out", p.pd80ClBlueWhiteOut);
    set("pd80_cl_blue_pos0_shoulder", p.pd80ClBluePos0Shoulder);
    set("pd80_cl_blue_pos1_shoulder", p.pd80ClBluePos1Shoulder);
    set("pd80_cl_blue_pos0_toe", p.pd80ClBluePos0Toe);
    set("pd80_cl_blue_pos1_toe", p.pd80ClBluePos1Toe);
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_cl_enable_dither")) {
        p.pd80ClEnableDither = *v ? 1.0f : 0.0f;
    } else {
        set("pd80_cl_enable_dither", p.pd80ClEnableDither);
    }
    set("pd80_pp_master_strength", p.pd80PpMasterStrength);
    set("pd80_pp_number_of_levels", p.pd80PpNumberOfLevels);
    set("pd80_pp_pixel_size", p.pd80PpPixelSize);
    set("pd80_pp_border_strength", p.pd80PpBorderStrength);
    set("pd80_pp_enable_dither", p.pd80PpEnableDither);
    set("pd80_pp_dither_motion", p.pd80PpDitherMotion);
    set("pd80_pp_dither_strength", p.pd80PpDitherStrength);
    set("pd80_mr_shape", p.pd80MrShape);
    set("pd80_mr_invert_shape", p.pd80MrInvertShape);
    set("pd80_mr_rotation", p.pd80MrRotation);
    set("pd80_mr_center_x", p.pd80MrCenter.x);
    set("pd80_mr_center_y", p.pd80MrCenter.y);
    set("pd80_mr_size_x", p.pd80MrSizeX);
    set("pd80_mr_size_y", p.pd80MrSizeY);
    set("pd80_mr_depth_position", p.pd80MrDepthPosition);
    set("pd80_mr_smoothing", p.pd80MrSmoothing);
    set("pd80_mr_depth_smoothing", p.pd80MrDepthSmoothing);
    set("pd80_mr_dither_strength", p.pd80MrDitherStrength);
    set("pd80_mr_color_r", p.pd80MrColor.x);
    set("pd80_mr_color_g", p.pd80MrColor.y);
    set("pd80_mr_color_b", p.pd80MrColor.z);
    set("pd80_mr_exposure", p.pd80MrExposure);
    set("pd80_mr_contrast", p.pd80MrContrast);
    set("pd80_mr_brightness", p.pd80MrBrightness);
    set("pd80_mr_hue", p.pd80MrHue);
    set("pd80_mr_saturation", p.pd80MrSaturation);
    set("pd80_mr_vibrance", p.pd80MrVibrance);
    set("pd80_mr_enable_gradient", p.pd80MrEnableGradient);
    set("pd80_mr_gradient_type", p.pd80MrGradientType);
    set("pd80_mr_gradient_curve", p.pd80MrGradientCurve);
    set("pd80_mr_intensity_boost", p.pd80MrIntensityBoost);
    set("pd80_mr_blend_mode", p.pd80MrBlendMode);
    set("pd80_mr_opacity", p.pd80MrOpacity);
    set("pd80_blp_master_strength", p.pd80BlpMasterStrength);
    set("pd80_blp_enable_dither", p.pd80BlpEnableDither);
    set("pd80_blp_dither_strength", p.pd80BlpDitherStrength);
    set("pd80_blp_lut_selector", p.pd80BlpLutSelector);
    set("pd80_blp_mix_chroma", p.pd80BlpMixChroma);
    set("pd80_blp_mix_luma", p.pd80BlpMixLuma);
    set("pd80_blp_black_in_r", p.pd80BlpBlackIn.x);
    set("pd80_blp_black_in_g", p.pd80BlpBlackIn.y);
    set("pd80_blp_black_in_b", p.pd80BlpBlackIn.z);
    set("pd80_blp_white_in_r", p.pd80BlpWhiteIn.x);
    set("pd80_blp_white_in_g", p.pd80BlpWhiteIn.y);
    set("pd80_blp_white_in_b", p.pd80BlpWhiteIn.z);
    set("pd80_blp_black_out_r", p.pd80BlpBlackOut.x);
    set("pd80_blp_black_out_g", p.pd80BlpBlackOut.y);
    set("pd80_blp_black_out_b", p.pd80BlpBlackOut.z);
    set("pd80_blp_white_out_r", p.pd80BlpWhiteOut.x);
    set("pd80_blp_white_out_g", p.pd80BlpWhiteOut.y);
    set("pd80_blp_white_out_b", p.pd80BlpWhiteOut.z);
    set("pd80_blp_gamma", p.pd80BlpGamma);
    set("pd80_clt_master_strength", p.pd80CltMasterStrength);
    set("pd80_clt_enable_dither", p.pd80CltEnableDither);
    set("pd80_clt_dither_strength", p.pd80CltDitherStrength);
    set("pd80_clt_lut_selector", p.pd80CltLutSelector);
    set("pd80_clt_mix_chroma", p.pd80CltMixChroma);
    set("pd80_clt_mix_luma", p.pd80CltMixLuma);
    set("pd80_clt_black_in_r", p.pd80CltBlackIn.x);
    set("pd80_clt_black_in_g", p.pd80CltBlackIn.y);
    set("pd80_clt_black_in_b", p.pd80CltBlackIn.z);
    set("pd80_clt_white_in_r", p.pd80CltWhiteIn.x);
    set("pd80_clt_white_in_g", p.pd80CltWhiteIn.y);
    set("pd80_clt_white_in_b", p.pd80CltWhiteIn.z);
    set("pd80_clt_black_out_r", p.pd80CltBlackOut.x);
    set("pd80_clt_black_out_g", p.pd80CltBlackOut.y);
    set("pd80_clt_black_out_b", p.pd80CltBlackOut.z);
    set("pd80_clt_white_out_r", p.pd80CltWhiteOut.x);
    set("pd80_clt_white_out_g", p.pd80CltWhiteOut.y);
    set("pd80_clt_white_out_b", p.pd80CltWhiteOut.z);
    set("pd80_clt_gamma", p.pd80CltGamma);
    set("pd80_lc_master_strength", p.pd80LcMasterStrength);
    set("pd80_lc_texture_width", p.pd80LcTextureWidth);
    set("pd80_lc_texture_height", p.pd80LcTextureHeight);
    set("pd80_lf_master_strength", p.pd80LfMasterStrength);
    set("pd80_lf_transition_speed", p.pd80LfTransitionSpeed);
    set("pd80_lf_min_level", p.pd80LfMinLevel);
    set("pd80_lf_max_level", p.pd80LfMaxLevel);
    set("pd80_cg4_master_strength", p.pd80Cg4MasterStrength);
    set("pd80_cg4_luma_mode", p.pd80Cg4LumaMode);
    set("pd80_cg4_separation_mode", p.pd80Cg4SeparationMode);
    set("pd80_cg4_enable_dither", p.pd80Cg4EnableDither);
    set("pd80_cg4_dither_strength", p.pd80Cg4DitherStrength);
    set("pd80_cg4_desaturate_base", p.pd80Cg4DesaturateBase);
    set("pd80_cg4_final_mix", p.pd80Cg4FinalMix);
    set("pd80_cg4_ls_m_r", p.pd80Cg4LightSceneMidColor.x);
    set("pd80_cg4_ls_m_g", p.pd80Cg4LightSceneMidColor.y);
    set("pd80_cg4_ls_m_b", p.pd80Cg4LightSceneMidColor.z);
    set("pd80_cg4_ls_m_mode", p.pd80Cg4LightSceneMidBlendMode);
    set("pd80_cg4_ls_m_opacity", p.pd80Cg4LightSceneMidOpacity);
    set("pd80_cg4_ls_s_r", p.pd80Cg4LightSceneShadowColor.x);
    set("pd80_cg4_ls_s_g", p.pd80Cg4LightSceneShadowColor.y);
    set("pd80_cg4_ls_s_b", p.pd80Cg4LightSceneShadowColor.z);
    set("pd80_cg4_ls_s_mode", p.pd80Cg4LightSceneShadowBlendMode);
    set("pd80_cg4_ls_s_opacity", p.pd80Cg4LightSceneShadowOpacity);
    set("pd80_cg4_enable_ds", p.pd80Cg4EnableDarkScene);
    set("pd80_cg4_ds_m_r", p.pd80Cg4DarkSceneMidColor.x);
    set("pd80_cg4_ds_m_g", p.pd80Cg4DarkSceneMidColor.y);
    set("pd80_cg4_ds_m_b", p.pd80Cg4DarkSceneMidColor.z);
    set("pd80_cg4_ds_m_mode", p.pd80Cg4DarkSceneMidBlendMode);
    set("pd80_cg4_ds_m_opacity", p.pd80Cg4DarkSceneMidOpacity);
    set("pd80_cg4_ds_s_r", p.pd80Cg4DarkSceneShadowColor.x);
    set("pd80_cg4_ds_s_g", p.pd80Cg4DarkSceneShadowColor.y);
    set("pd80_cg4_ds_s_b", p.pd80Cg4DarkSceneShadowColor.z);
    set("pd80_cg4_ds_s_mode", p.pd80Cg4DarkSceneShadowBlendMode);
    set("pd80_cg4_ds_s_opacity", p.pd80Cg4DarkSceneShadowOpacity);
    set("pd80_cg4_min_level", p.pd80Cg4MinLevel);
    set("pd80_cg4_max_level", p.pd80Cg4MaxLevel);
    set("pd80_cc_master_strength", p.pd80CcMasterStrength);
    set("pd80_cc_enable_whitepoint", p.pd80CcEnableWhitepoint);
    set("pd80_cc_whitepoint_strength", p.pd80CcWhitepointStrength);
    set("pd80_cc_enable_blackpoint", p.pd80CcEnableBlackpoint);
    set("pd80_cc_blackpoint_strength", p.pd80CcBlackpointStrength);
    set("pd80_rcc_master_strength", p.pd80RccMasterStrength);
    set("pd80_rcc_enable_dither", p.pd80RccEnableDither);
    set("pd80_rcc_dither_strength", p.pd80RccDitherStrength);
    set("pd80_rcc_enable_whitepoint", p.pd80RccEnableWhitepoint);
    set("pd80_rcc_whitepoint_respect_luma", p.pd80RccWhitepointRespectLuma);
    set("pd80_rcc_whitepoint_method", p.pd80RccWhitepointMethod);
    set("pd80_rcc_whitepoint_strength", p.pd80RccWhitepointStrength);
    set("pd80_rcc_whitepoint_luma_strength", p.pd80RccWhitepointLumaStrength);
    set("pd80_rcc_enable_blackpoint", p.pd80RccEnableBlackpoint);
    set("pd80_rcc_blackpoint_respect_luma", p.pd80RccBlackpointRespectLuma);
    set("pd80_rcc_blackpoint_method", p.pd80RccBlackpointMethod);
    set("pd80_rcc_blackpoint_strength", p.pd80RccBlackpointStrength);
    set("pd80_rcc_blackpoint_luma_strength", p.pd80RccBlackpointLumaStrength);
    set("pd80_rcc_enable_midpoint", p.pd80RccEnableMidpoint);
    set("pd80_rcc_midpoint_respect_luma", p.pd80RccMidpointRespectLuma);
    set("pd80_rcc_mid_use_alt_method", p.pd80RccMidUseAltMethod);
    set("pd80_rcc_mid_scale", p.pd80RccMidScale);
    set("pd80_fa_master_strength", p.pd80FaMasterStrength);
    set("pd80_fa_adjust_shoulder", p.pd80FaAdjustShoulder);
    set("pd80_fa_adjust_linear", p.pd80FaAdjustLinear);
    set("pd80_fa_adjust_toe", p.pd80FaAdjustToe);
    set("pd80_hb_master_strength", p.pd80HbMasterStrength);
    set("pd80_hb_debug_bloom", p.pd80HbDebugBloom);
    set("pd80_hb_dither_strength", p.pd80HbDitherStrength);
    set("pd80_hb_mix", p.pd80HbMix);
    set("pd80_hb_threshold", p.pd80HbThreshold);
    set("pd80_hb_grey_value", p.pd80HbGreyValue);
    set("pd80_hb_exposure", p.pd80HbExposure);
    set("pd80_hb_blur_sigma", p.pd80HbBlurSigma);
    set("pd80_hb_saturation", p.pd80HbSaturation);
    set("pd80_sc2_master_strength", p.pd80Sc2MasterStrength);
    set("pd80_sc2_correction_method", p.pd80Sc2CorrectionMethod);
    set("pd80_sc2_saturation_scale", p.pd80Sc2SaturationScale);
    set("pd80_sc2_lightness_scale", p.pd80Sc2LightnessScale);
    set("colourfulness", p.colourfulness);
    set("colorfulness", p.colourfulness);
    set("colourfulness_limit_luma", p.colourfulnessLimitLuma);
    set("filmic_pass_strength", p.filmicPassStrength);
    set("filmic_pass_fade", p.filmicPassFade);
    set("filmic_pass_bleach", p.filmicPassBleach);
    set("filmic_pass_saturation", p.filmicPassSaturation);
    set("film_grain2_amount", p.filmGrain2Amount);
    set("film_grain2_color_amount", p.filmGrain2ColorAmount);
    set("film_grain2_luminance_amount", p.filmGrain2LuminanceAmount);
    set("film_grain2_size", p.filmGrain2Size);
    set("denoise_strength", p.denoiseStrength);
    set("denoise_noise_level", p.denoiseNoiseLevel);
    set("denoise_lerp_coefficient", p.denoiseLerpCoefficient);
    set("denoise_weight_threshold", p.denoiseWeightThreshold);
    set("denoise_counter_threshold", p.denoiseCounterThreshold);
    set("denoise_gaussian_sigma", p.denoiseGaussianSigma);
    set("adaptive_sharpen_strength", p.adaptiveSharpenStrength);
    set("adaptive_sharpen_curve_slope", p.adaptiveSharpenCurveSlope);
    set("adaptive_sharpen_light_overshoot", p.adaptiveSharpenLightOvershoot);
    set("adaptive_sharpen_dark_overshoot", p.adaptiveSharpenDarkOvershoot);
    set("adaptive_sharpen_light_compr_low", p.adaptiveSharpenLightComprLow);
    set("adaptive_sharpen_light_compr_high", p.adaptiveSharpenLightComprHigh);
    set("adaptive_sharpen_dark_compr_low", p.adaptiveSharpenDarkComprLow);
    set("adaptive_sharpen_dark_compr_high", p.adaptiveSharpenDarkComprHigh);
    set("adaptive_sharpen_scale_lim", p.adaptiveSharpenScaleLim);
    set("adaptive_sharpen_scale_cs", p.adaptiveSharpenScaleCs);
    set("adaptive_sharpen_pm_p", p.adaptiveSharpenPmP);
    set("gaussian_blur_strength", p.gaussianBlurStrength);
    set("gaussian_blur_offset", p.gaussianBlurOffset);
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "gaussian_blur_radius")) {
        p.gaussianBlurRadius = static_cast<int>(*v);
    }
    set("fine_sharp_strength", p.fineSharpStrength);
    set("fine_sharp_equalization", p.fineSharpEqualization);
    set("fine_sharp_x_strength", p.fineSharpXStrength);
    set("fine_sharp_x_repair", p.fineSharpXRepair);
    set("fine_sharp_l_strength", p.fineSharpLStrength);
    set("fine_sharp_p_strength", p.fineSharpPStrength);
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "fine_sharp_mode")) {
        p.fineSharpMode = static_cast<int>(*v);
    }
    set("marty_bloom_threshold", p.martyBloomThreshold);
    set("marty_bloom_amount", p.martyBloomAmount);
    set("marty_bloom_saturation", p.martyBloomSaturation);
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "marty_bloom_mix_mode")) {
        p.martyBloomMixMode = static_cast<int>(*v);
    }
    if (const auto tint = TryParseJsonVec3(obj, "marty_bloom_tint")) {
        p.martyBloomTint = *tint;
    }
    set("creator_dof_strength", p.creatorDofStrength);
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "creator_dof_auto_focus")) {
        p.creatorDofAutoFocus = *v;
    }
    set("creator_dof_manual_focus", p.creatorDofManualFocusDepth);
    set("creator_dof_infinite_focus", p.creatorDofInfiniteFocus);
    if (const auto fp = TryParseJsonVec2(obj, "creator_dof_focus_point")) {
        p.creatorDofFocusPoint = *fp;
    }
    set("creator_dof_focus_radius", p.creatorDofFocusRadius);
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "creator_dof_focus_samples")) {
        p.creatorDofFocusSamples = static_cast<int>(*v);
    }
    set("creator_dof_near_blur_curve", p.creatorDofNearBlurCurve);
    set("creator_dof_far_blur_curve", p.creatorDofFarBlurCurve);
    set("creator_dof_blur_radius", p.creatorDofBlurRadius);
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "creator_dof_ring_samples")) {
        p.creatorDofRingSamples = static_cast<int>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "creator_dof_ring_rings")) {
        p.creatorDofRingRings = static_cast<int>(*v);
    }
    set("creator_dof_ring_threshold", p.creatorDofRingThreshold);
    set("creator_dof_ring_gain", p.creatorDofRingGain);
    set("creator_dof_ring_bias", p.creatorDofRingBias);
    set("creator_dof_ring_fringe", p.creatorDofRingFringe);
    set("ambient_light_intensity", p.ambientLightIntensity);
    set("ambient_light_threshold", p.ambientLightThreshold);
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "ambient_light_adaptation")) {
        p.ambientLightAdaptation = *v;
    }
    set("ambient_light_adapt", p.ambientLightAdapt);
    set("ambient_light_adapt_base_mult", p.ambientLightAdaptBaseMult);
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "ambient_light_adapt_black_level")) {
        p.ambientLightAdaptBlackLevel = static_cast<int>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "ambient_light_dither")) {
        p.ambientLightDither = *v;
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "ambient_light_dirt")) {
        p.ambientLightDirt = *v;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "ambient_light_adaptive_mode")) {
        p.ambientLightAdaptiveMode = static_cast<int>(*v);
    }
    set("ambient_light_dirt_int", p.ambientLightDirtInt);
    set("ambient_light_dirt_ovr_int", p.ambientLightDirtOvrInt);
    set("fake_motion_blur_recall", p.fakeMotionBlurRecall);
    set("fake_motion_blur_softness", p.fakeMotionBlurSoftness);
    set("reflective_bump_mapping_strength", p.reflectiveBumpMappingStrength);
    set("reflective_bump_mapping_blur_width", p.reflectiveBumpMappingBlurWidthPixels);
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "reflective_bump_mapping_sample_count")) {
        p.reflectiveBumpMappingSampleCount = static_cast<int>(*v);
    }
    set("reflective_bump_mapping_relief_height", p.reflectiveBumpMappingReliefHeight);
    set("reflective_bump_mapping_fresnel_reflectance", p.reflectiveBumpMappingFresnelReflectance);
    set("reflective_bump_mapping_fresnel_mult", p.reflectiveBumpMappingFresnelMult);
    set("reflective_bump_mapping_lower_threshold", p.reflectiveBumpMappingLowerThreshold);
    set("reflective_bump_mapping_upper_threshold", p.reflectiveBumpMappingUpperThreshold);
    set("reflective_bump_mapping_color_mask_red", p.reflectiveBumpMappingColorMaskRed);
    set("reflective_bump_mapping_color_mask_orange", p.reflectiveBumpMappingColorMaskOrange);
    set("reflective_bump_mapping_color_mask_yellow", p.reflectiveBumpMappingColorMaskYellow);
    set("reflective_bump_mapping_color_mask_green", p.reflectiveBumpMappingColorMaskGreen);
    set("reflective_bump_mapping_color_mask_cyan", p.reflectiveBumpMappingColorMaskCyan);
    set("reflective_bump_mapping_color_mask_blue", p.reflectiveBumpMappingColorMaskBlue);
    set("reflective_bump_mapping_color_mask_magenta", p.reflectiveBumpMappingColorMaskMagenta);
    set("reflective_bump_mapping_depth_far_plane", p.reflectiveBumpMappingDepthFarPlane);
    set("crop_scale_content_width", p.cropScaleContentWidth);
    set("crop_scale_content_height", p.cropScaleContentHeight);
    set("crop_scale_intermediate_width", p.cropScaleIntermediateWidth);
    set("crop_scale_intermediate_height", p.cropScaleIntermediateHeight);
    set("crop_scale_final_width", p.cropScaleFinalWidth);
    set("crop_scale_final_height", p.cropScaleFinalHeight);
    set("crop_scale_strength", p.cropScaleStrength);
    if (const std::optional<std::int32_t> v = detail::ExtractJsonInt(obj, "crop_scale_filter")) {
        p.cropScaleFilter = *v;
    }
    set("barbatos_fake_hdr_strength", p.barbatosFakeHdrStrength);
    if (const std::optional<std::int32_t> v = detail::ExtractJsonInt(obj, "barbatos_fake_hdr_preset")) {
        p.barbatosFakeHdrPreset = *v;
    }
    set("ri_adaptive_deband_strength", p.riAdaptiveDebandStrength);
    set("ri_adaptive_deband_radius", p.riAdaptiveDebandRadius);
    set("ri_adaptive_deband_threshold", p.riAdaptiveDebandThreshold);
    if (const std::optional<std::int32_t> v = detail::ExtractJsonInt(obj, "ri_adaptive_deband_iterations")) {
        p.riAdaptiveDebandIterations = *v;
    }
    set("ri_local_sharpen_strength", p.riLocalSharpenStrength);
    set("ri_local_sharpen_radius", p.riLocalSharpenRadius);
    set("ri_local_sharpen_clamp", p.riLocalSharpenClamp);
    set("ri_local_sharpen_edge_limit", p.riLocalSharpenEdgeLimit);
    set("ri_outline_strength", p.riOutlineStrength);
    set("ri_outline_thickness", p.riOutlineThickness);
    set("ri_outline_depth_sensitivity", p.riOutlineDepthSensitivity);
    set("ri_outline_color_sensitivity", p.riOutlineColorSensitivity);
    set("ri_outline_wobble_amount", p.riOutlineWobbleAmount);
    set("ri_outline_wobble_speed", p.riOutlineWobbleSpeed);
    set("ri_outline_wobble_frequency", p.riOutlineWobbleFrequency);
    set("ri_outline_debug", p.riOutlineDebug);
    if (const std::optional<std::int32_t> v = detail::ExtractJsonInt(obj, "ri_outline_method")) {
        p.riOutlineMethod = *v;
    }
    set("ri_signal_glitch_strength", p.riSignalGlitchStrength);
    set("ri_signal_glitch_block_size", p.riSignalGlitchBlockSize);
    set("ri_signal_glitch_color_shift_pixels", p.riSignalGlitchColorShiftPixels);
    set("ri_signal_glitch_speed", p.riSignalGlitchSpeed);
    set("ri_night_vision_strength", p.riNightVisionStrength);
    set("ri_night_vision_gain", p.riNightVisionGain);
    set("ri_night_vision_noise", p.riNightVisionNoise);
    set("ri_night_vision_vignette", p.riNightVisionVignette);

    if (const auto lift = TryParseJsonVec3(obj, "lift_rgb")) {
        p.liftRgb = *lift;
    }
    if (const auto v = TryParseJsonVec3(obj, "ri_outline_color")) {
        p.riOutlineColor = *v;
    }
    if (const auto gamma = TryParseJsonVec3(obj, "gamma_rgb")) {
        p.gammaRgb = *gamma;
    }
    if (const auto gain = TryParseJsonVec3(obj, "gain_rgb")) {
        p.gainRgb = *gain;
    }
    if (const auto bal = TryParseJsonVec3(obj, "vibrance_rgb_balance")) {
        p.vibranceRgbBalance = *bal;
    }
    if (const auto tc = TryParseJsonVec3(obj, "technicolor_rgb_negative")) {
        p.technicolorRgbNegative = *tc;
    }
    if (const auto t2 = TryParseJsonVec3(obj, "technicolor2_color_strength")) {
        p.technicolor2ColorStrength = *t2;
    }
    if (const auto v = TryParseJsonVec3(obj, "pd80_technicolor_red_2strip")) {
        p.pd80TechnicolorRed2strip = *v;
    }
    if (const auto v = TryParseJsonVec3(obj, "pd80_technicolor_cyan_2strip")) {
        p.pd80TechnicolorCyan2strip = *v;
    }
    if (const auto v = TryParseJsonVec3(obj, "pd80_technicolor_color_key")) {
        p.pd80TechnicolorColorKey = *v;
    }
    if (const auto v = TryParseJsonVec3(obj, "pd80_technicolor_3_color_strength")) {
        p.pd80Technicolor3ColorStrength = *v;
    }
    if (const auto v = TryParseJsonVec3(obj, "pd80_color_balance_shadow")) {
        p.pd80ColorBalanceShadow = *v;
    }
    if (const auto v = TryParseJsonVec3(obj, "pd80_color_balance_mid")) {
        p.pd80ColorBalanceMid = *v;
    }
    if (const auto v = TryParseJsonVec3(obj, "pd80_color_balance_high")) {
        p.pd80ColorBalanceHigh = *v;
    }
    if (const auto v = TryParseJsonVec3(obj, "pd80_levels_black_in")) {
        p.pd80LevelsBlackIn = *v;
    }
    if (const auto v = TryParseJsonVec3(obj, "pd80_levels_white_in")) {
        p.pd80LevelsWhiteIn = *v;
    }
    if (const auto v = TryParseJsonVec3(obj, "pd80_levels_black_out")) {
        p.pd80LevelsBlackOut = *v;
    }
    if (const auto v = TryParseJsonVec3(obj, "pd80_levels_white_out")) {
        p.pd80LevelsWhiteOut = *v;
    }
    if (const auto v = TryParseJsonVec2(obj, "pd80_mr_center")) {
        p.pd80MrCenter = *v;
    }
    if (const auto v = TryParseJsonVec3(obj, "pd80_mr_color")) {
        p.pd80MrColor = *v;
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(obj, "pd80_technicolor_enable_3strip")) {
        p.pd80TechnicolorEnable3strip = *v ? 1.0f : 0.0f;
    }
    if (const auto st = TryParseJsonVec3(obj, "sepia_tint")) {
        p.sepiaTint = *st;
    } else if (const auto st = TryParseJsonVec3(obj, "sepiaTint")) {
        p.sepiaTint = *st;
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "monochrome_preset")) {
        p.monochromePreset = std::clamp(static_cast<int>(std::lround(*pr)), 0, 17);
    }
    if (const auto mc = TryParseJsonVec3(obj, "monochrome_custom_coeff")) {
        p.monochromeCustomCoeff = *mc;
    } else if (const auto mc = TryParseJsonVec3(obj, "monochrome_conversion_values")) {
        p.monochromeCustomCoeff = *mc;
    }
    if (const auto cv = TryParseJsonVec3(obj, "dpx_rgb_curve")) {
        p.dpxRgbCurve = *cv;
    }
    if (const auto cc = TryParseJsonVec3(obj, "dpx_rgb_c")) {
        p.dpxRgbC = *cc;
    }

    if (const std::optional<std::string_view> tintObj = detail::ExtractJsonObject(obj, "tint_color")) {
        if (const std::optional<double> r = detail::ExtractJsonDouble(*tintObj, "r")) {
            p.tintColor.x = static_cast<float>(*r);
        }
        if (const std::optional<double> g = detail::ExtractJsonDouble(*tintObj, "g")) {
            p.tintColor.y = static_cast<float>(*g);
        }
        if (const std::optional<double> b = detail::ExtractJsonDouble(*tintObj, "b")) {
            p.tintColor.z = static_cast<float>(*b);
        }
    }
    if (const std::optional<ri::math::Vec3> tint = TryParseJsonVec3(obj, "tint_color")) {
        p.tintColor = *tint;
    }
    if (const std::optional<ri::math::Vec3> tint = TryParseJsonVec3(obj, "tint")) {
        p.tintColor = *tint;
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "luma_sharpen_pattern")) {
        p.lumaSharpenPattern = std::clamp(static_cast<int>(std::lround(*pr)), 0, 3);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "sweet_fx_curves_mode")) {
        p.sweetFxCurvesMode = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "sweet_fx_curves_formula")) {
        p.sweetFxCurvesFormula = std::clamp(static_cast<int>(std::lround(*pr)), 0, 10);
    }
    if (const auto sh = TryParseJsonVec2(obj, "sweet_fx_chromatic_aberration_shift")) {
        p.sweetFxChromaticAberrationShiftX = sh->x;
        p.sweetFxChromaticAberrationShiftY = sh->y;
    }
    if (const auto bw = TryParseJsonVec2(obj, "sweet_fx_border_width")) {
        p.sweetFxBorderWidthX = bw->x;
        p.sweetFxBorderWidthY = bw->y;
    }
    if (const auto bc = TryParseJsonVec3(obj, "sweet_fx_border_color")) {
        p.sweetFxBorderColor = *bc;
    }
    if (const auto fc = TryParseJsonVec3(obj, "sweet_fx_tonemap_fog_color")) {
        p.sweetFxTonemapFogColor = *fc;
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "sweet_fx_splitscreen_mode")) {
        p.sweetFxSplitscreenMode = std::clamp(static_cast<int>(std::lround(*pr)), 0, 6);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_splitscreen_strength")) {
        p.sweetFxSplitscreenStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "sweet_fx_nostalgia_palette")) {
        p.sweetFxNostalgiaPalette = std::clamp(static_cast<int>(std::lround(*pr)), 0, 14);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "sweet_fx_nostalgia_scanlines")) {
        p.sweetFxNostalgiaScanlines = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_nostalgia_dither")) {
        p.sweetFxNostalgiaDither = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_nostalgia_strength")) {
        p.sweetFxNostalgiaStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "sweet_fx_compare_mode")) {
        p.sweetFxCompareMode = std::clamp(static_cast<int>(std::lround(*pr)), 0, 8);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_compare_difference_scale")) {
        p.sweetFxCompareDifferenceScale = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_compare_strength")) {
        p.sweetFxCompareStrength = static_cast<float>(*v);
    }
    if (const auto lp = TryParseJsonVec2(obj, "sweet_fx_layer_position")) {
        p.sweetFxLayerPosition.x = lp->x;
        p.sweetFxLayerPosition.y = lp->y;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_layer_scale")) {
        p.sweetFxLayerScale = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_layer_blend")) {
        p.sweetFxLayerBlend = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_layer_tex_width")) {
        p.sweetFxLayerTexWidth = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_layer_tex_height")) {
        p.sweetFxLayerTexHeight = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_fxaa_subpix")) {
        p.sweetFxFxaaSubpix = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_fxaa_edge_threshold")) {
        p.sweetFxFxaaEdgeThreshold = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_fxaa_edge_threshold_min")) {
        p.sweetFxFxaaEdgeThresholdMin = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_fxaa_strength")) {
        p.sweetFxFxaaStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_crt_amount")) {
        p.sweetFxCrtAmount = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_crt_resolution")) {
        p.sweetFxCrtResolution = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_crt_gamma")) {
        p.sweetFxCrtGamma = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_crt_monitor_gamma")) {
        p.sweetFxCrtMonitorGamma = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_crt_brightness")) {
        p.sweetFxCrtBrightness = static_cast<float>(*v);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "sweet_fx_crt_scanline_intensity")) {
        p.sweetFxCrtScanlineIntensity = std::clamp(static_cast<int>(std::lround(*pr)), 2, 4);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_crt_scanline_gaussian")) {
        p.sweetFxCrtScanlineGaussian = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_crt_curvature")) {
        p.sweetFxCrtCurvature = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_crt_curvature_radius")) {
        p.sweetFxCrtCurvatureRadius = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_crt_corner_size")) {
        p.sweetFxCrtCornerSize = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_crt_viewer_distance")) {
        p.sweetFxCrtViewerDistance = static_cast<float>(*v);
    }
    if (const auto a = TryParseJsonVec2(obj, "sweet_fx_crt_angle")) {
        p.sweetFxCrtAngle = *a;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_crt_overscan")) {
        p.sweetFxCrtOverscan = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_crt_oversample")) {
        p.sweetFxCrtOversample = static_cast<float>(*v);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "sweet_fx_ascii_spacing")) {
        p.sweetFxAsciiSpacing = std::clamp(static_cast<int>(std::lround(*pr)), 0, 5);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "sweet_fx_ascii_font")) {
        p.sweetFxAsciiFont = std::clamp(static_cast<int>(std::lround(*pr)), 0, 1);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "sweet_fx_ascii_font_color_mode")) {
        p.sweetFxAsciiFontColorMode = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    }
    if (const auto v = TryParseJsonVec3(obj, "sweet_fx_ascii_font_color")) {
        p.sweetFxAsciiFontColor = *v;
    }
    if (const auto v = TryParseJsonVec3(obj, "sweet_fx_ascii_background_color")) {
        p.sweetFxAsciiBackgroundColor = *v;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_ascii_swap_colors")) {
        p.sweetFxAsciiSwapColors = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_ascii_invert_brightness")) {
        p.sweetFxAsciiInvertBrightness = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_ascii_dithering")) {
        p.sweetFxAsciiDithering = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_ascii_dithering_intensity")) {
        p.sweetFxAsciiDitheringIntensity = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_ascii_dithering_debug_gradient")) {
        p.sweetFxAsciiDitheringDebugGradient = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_ascii_strength")) {
        p.sweetFxAsciiStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "sweet_fx_smaa_edge_detection_type")) {
        p.sweetFxSmaaEdgeDetectionType = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_smaa_edge_threshold")) {
        p.sweetFxSmaaEdgeThreshold = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_smaa_depth_threshold")) {
        p.sweetFxSmaaDepthThreshold = static_cast<float>(*v);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "sweet_fx_smaa_max_search_steps")) {
        p.sweetFxSmaaMaxSearchSteps = std::clamp(static_cast<int>(std::lround(*pr)), 0, 112);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "sweet_fx_smaa_max_search_steps_diagonal")) {
        p.sweetFxSmaaMaxSearchStepsDiagonal = std::clamp(static_cast<int>(std::lround(*pr)), 0, 20);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "sweet_fx_smaa_corner_rounding")) {
        p.sweetFxSmaaCornerRounding = std::clamp(static_cast<int>(std::lround(*pr)), 0, 100);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_smaa_debug_output")) {
        p.sweetFxSmaaDebugOutput = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "sweet_fx_smaa_strength")) {
        p.sweetFxSmaaStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "reshade_daltonize_type")) {
        p.reshadeDaltonizeType = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "reshade_daltonize_strength")) {
        p.reshadeDaltonizeStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(obj, "reshade_display_depth_present_type")) {
        p.reshadeDisplayDepthPresentType = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "reshade_display_depth_strength")) {
        p.reshadeDisplayDepthStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "reshade_lut_amount_chroma")) {
        p.reshadeLutAmountChroma = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "reshade_lut_amount_luma")) {
        p.reshadeLutAmountLuma = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(obj, "reshade_lut_strength")) {
        p.reshadeLutStrength = static_cast<float>(*v);
    }
}

[[nodiscard]] std::string_view ParamsOrEffect(std::string_view effect) {
    if (const std::optional<std::string_view> params = detail::ExtractJsonObject(effect, "params")) {
        return *params;
    }
    return effect;
}

void MergeCas(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sharpen")) {
        layer.casSharpenAmount = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sharpen_amount")) {
        layer.casSharpenAmount = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "amount")) {
        layer.casSharpenAmount = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "contrast")) {
        layer.casContrastAdaptation = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "contrast_adaptation")) {
        layer.casContrastAdaptation = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeBloom(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> intensity = detail::ExtractJsonDouble(p, "intensity")) {
        layer.bloomIntensity = static_cast<float>(*intensity);
    }
    if (const std::optional<double> threshold = detail::ExtractJsonDouble(p, "threshold")) {
        layer.bloomThreshold = static_cast<float>(*threshold);
    }
    MergePresentationObject(p, layer);
}

void MergeDeband(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> strength = detail::ExtractJsonDouble(p, "strength")) {
        layer.debandStrength = static_cast<float>(*strength);
    }
    MergePresentationObject(p, layer);
}

void MergeVignette(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> vigStrength = detail::ExtractJsonDouble(p, "strength")) {
        layer.vignetteStrength = static_cast<float>(*vigStrength);
    }
    if (const std::optional<double> blur = detail::ExtractJsonDouble(p, "blur")) {
        layer.blurAmount = static_cast<float>(*blur);
    }
    MergePresentationObject(p, layer);
}

void MergeLiftGammaGain(std::string_view p, PostProcessParameters& layer) {
    if (const auto v = TryParseJsonVec3(p, "lift_rgb")) {
        layer.liftRgb = *v;
    } else if (const auto v = TryParseJsonVec3(p, "rgb_lift")) {
        layer.liftRgb = *v;
    }
    if (const auto v = TryParseJsonVec3(p, "gamma_rgb")) {
        layer.gammaRgb = *v;
    } else if (const auto v = TryParseJsonVec3(p, "rgb_gamma")) {
        layer.gammaRgb = *v;
    }
    if (const auto v = TryParseJsonVec3(p, "gain_rgb")) {
        layer.gainRgb = *v;
    } else if (const auto v = TryParseJsonVec3(p, "rgb_gain")) {
        layer.gainRgb = *v;
    }
    if (const std::optional<double> m = detail::ExtractJsonDouble(p, "lift_gamma_gain_mix")) {
        layer.liftGammaGainMix = static_cast<float>(*m);
    } else if (const std::optional<double> m = detail::ExtractJsonDouble(p, "mix")) {
        layer.liftGammaGainMix = static_cast<float>(*m);
    }
    MergePresentationObject(p, layer);
}

void MergeVibrance(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "vibrance")) {
        layer.vibrance = static_cast<float>(*v);
    }
    if (const auto bal = TryParseJsonVec3(p, "vibrance_rgb_balance")) {
        layer.vibranceRgbBalance = *bal;
    } else if (const auto bal = TryParseJsonVec3(p, "rgb_balance")) {
        layer.vibranceRgbBalance = *bal;
    }
    MergePresentationObject(p, layer);
}

void MergeColourfulness(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "colourfulness")) {
        layer.colourfulness = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "colorfulness")) {
        layer.colourfulness = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "amount")) {
        layer.colourfulness = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "limit_luma")) {
        layer.colourfulnessLimitLuma = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeFilmicPass(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "strength")) {
        layer.filmicPassStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fade")) {
        layer.filmicPassFade = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "bleach")) {
        layer.filmicPassBleach = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "saturation")) {
        layer.filmicPassSaturation = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeFilmGrain2(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "amount")) {
        layer.filmGrain2Amount = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "color_amount")) {
        layer.filmGrain2ColorAmount = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "luminance_amount")) {
        layer.filmGrain2LuminanceAmount = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "size")) {
        layer.filmGrain2Size = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeDenoise(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "strength")) {
        layer.denoiseStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "noise_level")) {
        layer.denoiseNoiseLevel = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "lerp_coefficient")) {
        layer.denoiseLerpCoefficient = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "weight_threshold")) {
        layer.denoiseWeightThreshold = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "counter_threshold")) {
        layer.denoiseCounterThreshold = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "gaussian_sigma")) {
        layer.denoiseGaussianSigma = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeAdaptiveSharpen(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "strength")) {
        layer.adaptiveSharpenStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "curve_height")) {
        layer.adaptiveSharpenStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "curve_slope")) {
        layer.adaptiveSharpenCurveSlope = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "light_overshoot")) {
        layer.adaptiveSharpenLightOvershoot = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "dark_overshoot")) {
        layer.adaptiveSharpenDarkOvershoot = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "light_compr_low")) {
        layer.adaptiveSharpenLightComprLow = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "light_compr_high")) {
        layer.adaptiveSharpenLightComprHigh = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "dark_compr_low")) {
        layer.adaptiveSharpenDarkComprLow = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "dark_compr_high")) {
        layer.adaptiveSharpenDarkComprHigh = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "scale_lim")) {
        layer.adaptiveSharpenScaleLim = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "scale_cs")) {
        layer.adaptiveSharpenScaleCs = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pm_p")) {
        layer.adaptiveSharpenPmP = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeGaussianBlur(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "strength")) {
        layer.gaussianBlurStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "offset")) {
        layer.gaussianBlurOffset = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "radius")) {
        layer.gaussianBlurRadius = static_cast<int>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeFineSharp(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "strength")) {
        layer.fineSharpStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sstr")) {
        layer.fineSharpStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "equalization")) {
        layer.fineSharpEqualization = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "cstr")) {
        layer.fineSharpEqualization = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "x_strength")) {
        layer.fineSharpXStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "xstr")) {
        layer.fineSharpXStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "x_repair")) {
        layer.fineSharpXRepair = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "xrep")) {
        layer.fineSharpXRepair = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "l_strength")) {
        layer.fineSharpLStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "lstr")) {
        layer.fineSharpLStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "p_strength")) {
        layer.fineSharpPStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pstr")) {
        layer.fineSharpPStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "mode")) {
        layer.fineSharpMode = static_cast<int>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeMartyBloom(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "amount")) {
        layer.martyBloomAmount = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fBloomAmount")) {
        layer.martyBloomAmount = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "threshold")) {
        layer.martyBloomThreshold = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fBloomThreshold")) {
        layer.martyBloomThreshold = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "saturation")) {
        layer.martyBloomSaturation = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fBloomSaturation")) {
        layer.martyBloomSaturation = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "mix_mode")) {
        layer.martyBloomMixMode = static_cast<int>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "iBloomMixmode")) {
        layer.martyBloomMixMode = static_cast<int>(*v);
    }
    if (const auto tint = TryParseJsonVec3(p, "tint")) {
        layer.martyBloomTint = *tint;
    }
    if (const auto tint = TryParseJsonVec3(p, "fBloomTint")) {
        layer.martyBloomTint = *tint;
    }
    MergePresentationObject(p, layer);
}

void MergeRingDof(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "strength")) {
        layer.creatorDofStrength = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "auto_focus")) {
        layer.creatorDofAutoFocus = *v;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "DOF_AUTOFOCUS")) {
        layer.creatorDofAutoFocus = *v;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "manual_focus")) {
        layer.creatorDofManualFocusDepth = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "DOF_MANUALFOCUSDEPTH")) {
        layer.creatorDofManualFocusDepth = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "infinite_focus")) {
        layer.creatorDofInfiniteFocus = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "DOF_INFINITEFOCUS")) {
        layer.creatorDofInfiniteFocus = static_cast<float>(*v);
    }
    if (const auto fp = TryParseJsonVec2(p, "focus_point")) {
        layer.creatorDofFocusPoint = *fp;
    } else if (const auto fp = TryParseJsonVec2(p, "DOF_FOCUSPOINT")) {
        layer.creatorDofFocusPoint = *fp;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "focus_radius")) {
        layer.creatorDofFocusRadius = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "DOF_FOCUSRADIUS")) {
        layer.creatorDofFocusRadius = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "focus_samples")) {
        layer.creatorDofFocusSamples = static_cast<int>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "DOF_FOCUSSAMPLES")) {
        layer.creatorDofFocusSamples = static_cast<int>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "near_blur_curve")) {
        layer.creatorDofNearBlurCurve = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "DOF_NEARBLURCURVE")) {
        layer.creatorDofNearBlurCurve = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "far_blur_curve")) {
        layer.creatorDofFarBlurCurve = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "DOF_FARBLURCURVE")) {
        layer.creatorDofFarBlurCurve = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blur_radius")) {
        layer.creatorDofBlurRadius = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "DOF_BLURRADIUS")) {
        layer.creatorDofBlurRadius = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "ring_samples")) {
        layer.creatorDofRingSamples = static_cast<int>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "iRingDOFSamples")) {
        layer.creatorDofRingSamples = static_cast<int>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "ring_rings")) {
        layer.creatorDofRingRings = static_cast<int>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "iRingDOFRings")) {
        layer.creatorDofRingRings = static_cast<int>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "ring_threshold")) {
        layer.creatorDofRingThreshold = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fRingDOFThreshold")) {
        layer.creatorDofRingThreshold = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "ring_gain")) {
        layer.creatorDofRingGain = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fRingDOFGain")) {
        layer.creatorDofRingGain = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "ring_bias")) {
        layer.creatorDofRingBias = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fRingDOFBias")) {
        layer.creatorDofRingBias = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "ring_fringe")) {
        layer.creatorDofRingFringe = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fRingDOFFringe")) {
        layer.creatorDofRingFringe = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeAmbientLight(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "intensity")) {
        layer.ambientLightIntensity = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "alInt")) {
        layer.ambientLightIntensity = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "threshold")) {
        layer.ambientLightThreshold = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "alThreshold")) {
        layer.ambientLightThreshold = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "adaptation")) {
        layer.ambientLightAdaptation = *v;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "AL_Adaptation")) {
        layer.ambientLightAdaptation = *v;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "adapt")) {
        layer.ambientLightAdapt = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "alAdapt")) {
        layer.ambientLightAdapt = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "adapt_base_mult")) {
        layer.ambientLightAdaptBaseMult = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "alAdaptBaseMult")) {
        layer.ambientLightAdaptBaseMult = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "adapt_black_level")) {
        layer.ambientLightAdaptBlackLevel = static_cast<int>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "alAdaptBaseBlackLvL")) {
        layer.ambientLightAdaptBlackLevel = static_cast<int>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "dither")) {
        layer.ambientLightDither = *v;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "AL_Dither")) {
        layer.ambientLightDither = *v;
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "dirt")) {
        layer.ambientLightDirt = *v;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "AL_Dirt")) {
        layer.ambientLightDirt = *v;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "adaptive_mode")) {
        layer.ambientLightAdaptiveMode = static_cast<int>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "AL_Adaptive")) {
        layer.ambientLightAdaptiveMode = static_cast<int>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "dirt_int")) {
        layer.ambientLightDirtInt = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "alDirtInt")) {
        layer.ambientLightDirtInt = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "dirt_ovr_int")) {
        layer.ambientLightDirtOvrInt = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "alDirtOVInt")) {
        layer.ambientLightDirtOvrInt = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeFakeMotionBlur(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "recall")) {
        layer.fakeMotionBlurRecall = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "mbRecall")) {
        layer.fakeMotionBlurRecall = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "softness")) {
        layer.fakeMotionBlurSoftness = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "mbSoftness")) {
        layer.fakeMotionBlurSoftness = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeReflectiveBumpMapping(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "strength")) {
        layer.reflectiveBumpMappingStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blur_width")) {
        layer.reflectiveBumpMappingBlurWidthPixels = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fRBM_BlurWidthPixels")) {
        layer.reflectiveBumpMappingBlurWidthPixels = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sample_count")) {
        layer.reflectiveBumpMappingSampleCount = static_cast<int>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "iRBM_SampleCount")) {
        layer.reflectiveBumpMappingSampleCount = static_cast<int>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "relief_height")) {
        layer.reflectiveBumpMappingReliefHeight = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fRBM_ReliefHeight")) {
        layer.reflectiveBumpMappingReliefHeight = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fresnel_reflectance")) {
        layer.reflectiveBumpMappingFresnelReflectance = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fRBM_FresnelReflectance")) {
        layer.reflectiveBumpMappingFresnelReflectance = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fresnel_mult")) {
        layer.reflectiveBumpMappingFresnelMult = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fRBM_FresnelMult")) {
        layer.reflectiveBumpMappingFresnelMult = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "lower_threshold")) {
        layer.reflectiveBumpMappingLowerThreshold = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fRBM_LowerThreshold")) {
        layer.reflectiveBumpMappingLowerThreshold = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "upper_threshold")) {
        layer.reflectiveBumpMappingUpperThreshold = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fRBM_UpperThreshold")) {
        layer.reflectiveBumpMappingUpperThreshold = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "color_mask_red")) {
        layer.reflectiveBumpMappingColorMaskRed = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fRBM_ColorMask_Red")) {
        layer.reflectiveBumpMappingColorMaskRed = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "color_mask_orange")) {
        layer.reflectiveBumpMappingColorMaskOrange = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fRBM_ColorMask_Orange")) {
        layer.reflectiveBumpMappingColorMaskOrange = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "color_mask_yellow")) {
        layer.reflectiveBumpMappingColorMaskYellow = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fRBM_ColorMask_Yellow")) {
        layer.reflectiveBumpMappingColorMaskYellow = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "color_mask_green")) {
        layer.reflectiveBumpMappingColorMaskGreen = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fRBM_ColorMask_Green")) {
        layer.reflectiveBumpMappingColorMaskGreen = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "color_mask_cyan")) {
        layer.reflectiveBumpMappingColorMaskCyan = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fRBM_ColorMask_Cyan")) {
        layer.reflectiveBumpMappingColorMaskCyan = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "color_mask_blue")) {
        layer.reflectiveBumpMappingColorMaskBlue = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fRBM_ColorMask_Blue")) {
        layer.reflectiveBumpMappingColorMaskBlue = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "color_mask_magenta")) {
        layer.reflectiveBumpMappingColorMaskMagenta = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fRBM_ColorMask_Magenta")) {
        layer.reflectiveBumpMappingColorMaskMagenta = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "depth_far_plane")) {
        layer.reflectiveBumpMappingDepthFarPlane = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeTechnicolor(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> powv = detail::ExtractJsonDouble(p, "power")) {
        layer.technicolorPower = static_cast<float>(*powv);
    }
    if (const auto neg = TryParseJsonVec3(p, "rgb_negative")) {
        layer.technicolorRgbNegative = *neg;
    } else if (const auto neg = TryParseJsonVec3(p, "technicolor_rgb_negative")) {
        layer.technicolorRgbNegative = *neg;
    }
    if (const std::optional<double> s = detail::ExtractJsonDouble(p, "strength")) {
        layer.technicolorStrength = static_cast<float>(*s);
    }
    MergePresentationObject(p, layer);
}

void MergeTechnicolor2(std::string_view p, PostProcessParameters& layer) {
    if (const auto cs = TryParseJsonVec3(p, "color_strength")) {
        layer.technicolor2ColorStrength = *cs;
    } else if (const auto cs = TryParseJsonVec3(p, "technicolor2_color_strength")) {
        layer.technicolor2ColorStrength = *cs;
    }
    if (const std::optional<double> b = detail::ExtractJsonDouble(p, "brightness")) {
        layer.technicolor2Brightness = static_cast<float>(*b);
    }
    if (const std::optional<double> s = detail::ExtractJsonDouble(p, "saturation")) {
        layer.technicolor2Saturation = static_cast<float>(*s);
    }
    if (const std::optional<double> s = detail::ExtractJsonDouble(p, "strength")) {
        layer.technicolor2Strength = static_cast<float>(*s);
    }
    MergePresentationObject(p, layer);
}

void MergePd80Technicolor(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_technicolor_strength")) {
        layer.pd80TechnicolorStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "mix")) {
        layer.pd80TechnicolorStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blend")) {
        layer.pd80TechnicolorStrength = static_cast<float>(*v);
    }
    if (const auto v = TryParseJsonVec3(p, "Red2strip")) {
        layer.pd80TechnicolorRed2strip = *v;
    } else if (const auto v = TryParseJsonVec3(p, "red_2strip")) {
        layer.pd80TechnicolorRed2strip = *v;
    } else if (const auto v = TryParseJsonVec3(p, "pd80_technicolor_red_2strip")) {
        layer.pd80TechnicolorRed2strip = *v;
    }
    if (const auto v = TryParseJsonVec3(p, "Cyan2strip")) {
        layer.pd80TechnicolorCyan2strip = *v;
    } else if (const auto v = TryParseJsonVec3(p, "cyan_2strip")) {
        layer.pd80TechnicolorCyan2strip = *v;
    } else if (const auto v = TryParseJsonVec3(p, "pd80_technicolor_cyan_2strip")) {
        layer.pd80TechnicolorCyan2strip = *v;
    }
    if (const auto v = TryParseJsonVec3(p, "colorKey")) {
        layer.pd80TechnicolorColorKey = *v;
    } else if (const auto v = TryParseJsonVec3(p, "color_key")) {
        layer.pd80TechnicolorColorKey = *v;
    } else if (const auto v = TryParseJsonVec3(p, "pd80_technicolor_color_key")) {
        layer.pd80TechnicolorColorKey = *v;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Saturation2")) {
        layer.pd80TechnicolorSaturation2 = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "saturation_2")) {
        layer.pd80TechnicolorSaturation2 = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_technicolor_saturation_2")) {
        layer.pd80TechnicolorSaturation2 = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable3strip")) {
        layer.pd80TechnicolorEnable3strip = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_3strip")) {
        layer.pd80TechnicolorEnable3strip = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_technicolor_enable_3strip")) {
        layer.pd80TechnicolorEnable3strip = *v ? 1.0f : 0.0f;
    }
    if (const auto v = TryParseJsonVec3(p, "ColorStrength")) {
        layer.pd80Technicolor3ColorStrength = *v;
    } else if (const auto v = TryParseJsonVec3(p, "pd80_technicolor_3_color_strength")) {
        layer.pd80Technicolor3ColorStrength = *v;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Brightness")) {
        layer.pd80Technicolor3Brightness = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_technicolor_3_brightness")) {
        layer.pd80Technicolor3Brightness = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_technicolor_3_saturation")) {
        layer.pd80Technicolor3Saturation = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Saturation")) {
        layer.pd80Technicolor3Saturation = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_technicolor_3_strength")) {
        layer.pd80Technicolor3Strength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Strength")) {
        layer.pd80Technicolor3Strength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80ColorTemperature(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Kelvin")) {
        layer.pd80ColorTemperatureKelvin = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "kelvin")) {
        layer.pd80ColorTemperatureKelvin = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_color_temperature_kelvin")) {
        layer.pd80ColorTemperatureKelvin = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "LumPreservation")) {
        layer.pd80ColorTemperatureLuminancePreservation = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "luminance_preservation")) {
        layer.pd80ColorTemperatureLuminancePreservation = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_color_temperature_luminance_preservation")) {
        layer.pd80ColorTemperatureLuminancePreservation = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "kMix")) {
        layer.pd80ColorTemperatureMix = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "k_mix")) {
        layer.pd80ColorTemperatureMix = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_color_temperature_mix")) {
        layer.pd80ColorTemperatureMix = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_color_temperature_strength")) {
        layer.pd80ColorTemperatureStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80SaturationLimit(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "saturation_limit")) {
        layer.pd80SaturationLimit = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_saturation_limit")) {
        layer.pd80SaturationLimit = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_saturation_limit_strength")) {
        layer.pd80SaturationLimitStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80ColorBalance(std::string_view p, PostProcessParameters& layer) {
    if (const auto v = TryParseJsonVec3(p, "pd80_color_balance_shadow")) {
        layer.pd80ColorBalanceShadow = *v;
    }
    if (const auto v = TryParseJsonVec3(p, "pd80_color_balance_mid")) {
        layer.pd80ColorBalanceMid = *v;
    }
    if (const auto v = TryParseJsonVec3(p, "pd80_color_balance_high")) {
        layer.pd80ColorBalanceHigh = *v;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "s_RedShift")) {
        layer.pd80ColorBalanceShadow.x = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "s_GreenShift")) {
        layer.pd80ColorBalanceShadow.y = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "s_BlueShift")) {
        layer.pd80ColorBalanceShadow.z = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "m_RedShift")) {
        layer.pd80ColorBalanceMid.x = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "m_GreenShift")) {
        layer.pd80ColorBalanceMid.y = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "m_BlueShift")) {
        layer.pd80ColorBalanceMid.z = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "h_RedShift")) {
        layer.pd80ColorBalanceHigh.x = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "h_GreenShift")) {
        layer.pd80ColorBalanceHigh.y = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "h_BlueShift")) {
        layer.pd80ColorBalanceHigh.z = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "preserve_luma")) {
        layer.pd80ColorBalancePreserveLuma = *v ? 1.0f : 0.0f;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "separation_mode")) {
        layer.pd80ColorBalanceSeparationMode = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_color_balance_strength")) {
        layer.pd80ColorBalanceStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80Levels(std::string_view p, PostProcessParameters& layer) {
    if (const auto v = TryParseJsonVec3(p, "pd80_levels_black_in")) {
        layer.pd80LevelsBlackIn = *v;
    } else if (const auto v = TryParseJsonVec3(p, "ib")) {
        layer.pd80LevelsBlackIn = *v;
    }
    if (const auto v = TryParseJsonVec3(p, "pd80_levels_white_in")) {
        layer.pd80LevelsWhiteIn = *v;
    } else if (const auto v = TryParseJsonVec3(p, "iw")) {
        layer.pd80LevelsWhiteIn = *v;
    }
    if (const auto v = TryParseJsonVec3(p, "pd80_levels_black_out")) {
        layer.pd80LevelsBlackOut = *v;
    } else if (const auto v = TryParseJsonVec3(p, "ob")) {
        layer.pd80LevelsBlackOut = *v;
    }
    if (const auto v = TryParseJsonVec3(p, "pd80_levels_white_out")) {
        layer.pd80LevelsWhiteOut = *v;
    } else if (const auto v = TryParseJsonVec3(p, "ow")) {
        layer.pd80LevelsWhiteOut = *v;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "ig")) {
        layer.pd80LevelsGamma = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_levels_gamma")) {
        layer.pd80LevelsGamma = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_dither")) {
        layer.pd80LevelsEnableDither = *v ? 1.0f : 0.0f;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "dither_strength")) {
        layer.pd80LevelsDitherStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_levels_dither_strength")) {
        layer.pd80LevelsDitherStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_levels_strength")) {
        layer.pd80LevelsStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80BlackWhite(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "bw_mode")) {
        layer.pd80BlackWhiteMode = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_black_white_mode")) {
        layer.pd80BlackWhiteMode = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "curve_str")) {
        layer.pd80BlackWhiteCurveStr = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_black_white_curve_str")) {
        layer.pd80BlackWhiteCurveStr = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_dither")) {
        layer.pd80BlackWhiteEnableDither = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_black_white_enable_dither")) {
        layer.pd80BlackWhiteEnableDither = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "dither_strength")) {
        layer.pd80BlackWhiteDitherStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_black_white_dither_strength")) {
        layer.pd80BlackWhiteDitherStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "redchannel")) {
        layer.pd80BlackWhiteRedChannel = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_black_white_red_channel")) {
        layer.pd80BlackWhiteRedChannel = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "yellowchannel")) {
        layer.pd80BlackWhiteYellowChannel = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_black_white_yellow_channel")) {
        layer.pd80BlackWhiteYellowChannel = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "greenchannel")) {
        layer.pd80BlackWhiteGreenChannel = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_black_white_green_channel")) {
        layer.pd80BlackWhiteGreenChannel = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "cyanchannel")) {
        layer.pd80BlackWhiteCyanChannel = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_black_white_cyan_channel")) {
        layer.pd80BlackWhiteCyanChannel = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "bluechannel")) {
        layer.pd80BlackWhiteBlueChannel = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_black_white_blue_channel")) {
        layer.pd80BlackWhiteBlueChannel = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "magentachannel")) {
        layer.pd80BlackWhiteMagentaChannel = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_black_white_magenta_channel")) {
        layer.pd80BlackWhiteMagentaChannel = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "use_tint")) {
        layer.pd80BlackWhiteUseTint = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_black_white_use_tint")) {
        layer.pd80BlackWhiteUseTint = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "tinthue")) {
        layer.pd80BlackWhiteTintHue = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_black_white_tint_hue")) {
        layer.pd80BlackWhiteTintHue = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "tintsat")) {
        layer.pd80BlackWhiteTintSat = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_black_white_tint_sat")) {
        layer.pd80BlackWhiteTintSat = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "show_clip")) {
        layer.pd80BlackWhiteShowClip = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_black_white_show_clip")) {
        layer.pd80BlackWhiteShowClip = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "strength")) {
        layer.pd80BlackWhiteStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_black_white_strength")) {
        layer.pd80BlackWhiteStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80ContrastBriSat(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_dither")) {
        layer.pd80CbsEnableDither = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_enable_dither")) {
        layer.pd80CbsEnableDither = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "dither_strength")) {
        layer.pd80CbsDitherStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_dither_strength")) {
        layer.pd80CbsDitherStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "tint")) {
        layer.pd80CbsTint = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_tint")) {
        layer.pd80CbsTint = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "exposureN")) {
        layer.pd80CbsExposure = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_exposure")) {
        layer.pd80CbsExposure = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "contrast")) {
        layer.pd80CbsContrast = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_contrast")) {
        layer.pd80CbsContrast = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "brightness")) {
        layer.pd80CbsBrightness = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_brightness")) {
        layer.pd80CbsBrightness = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "saturation")) {
        layer.pd80CbsSaturation = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_saturation")) {
        layer.pd80CbsSaturation = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "vibrance")) {
        layer.pd80CbsVibrance = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_vibrance")) {
        layer.pd80CbsVibrance = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "huemid")) {
        layer.pd80CbsHueMid = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_hue_mid")) {
        layer.pd80CbsHueMid = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "huerange")) {
        layer.pd80CbsHueRange = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_hue_range")) {
        layer.pd80CbsHueRange = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sat_custom")) {
        layer.pd80CbsSatCustom = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_sat_custom")) {
        layer.pd80CbsSatCustom = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sat_r")) {
        layer.pd80CbsSatR = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_sat_r")) {
        layer.pd80CbsSatR = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sat_y")) {
        layer.pd80CbsSatY = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_sat_y")) {
        layer.pd80CbsSatY = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sat_g")) {
        layer.pd80CbsSatG = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_sat_g")) {
        layer.pd80CbsSatG = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sat_a")) {
        layer.pd80CbsSatA = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_sat_a")) {
        layer.pd80CbsSatA = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sat_b")) {
        layer.pd80CbsSatB = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_sat_b")) {
        layer.pd80CbsSatB = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sat_p")) {
        layer.pd80CbsSatP = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_sat_p")) {
        layer.pd80CbsSatP = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sat_m")) {
        layer.pd80CbsSatM = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_sat_m")) {
        layer.pd80CbsSatM = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_depth")) {
        layer.pd80CbsEnableDepth = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_enable_depth")) {
        layer.pd80CbsEnableDepth = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "display_depth")) {
        layer.pd80CbsDisplayDepth = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_display_depth")) {
        layer.pd80CbsDisplayDepth = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "depthStart")) {
        layer.pd80CbsDepthStart = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_depth_start")) {
        layer.pd80CbsDepthStart = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "depthEnd")) {
        layer.pd80CbsDepthEnd = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_depth_end")) {
        layer.pd80CbsDepthEnd = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "depthCurve")) {
        layer.pd80CbsDepthCurve = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_depth_curve")) {
        layer.pd80CbsDepthCurve = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "exposureD")) {
        layer.pd80CbsExposureFar = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_exposure_far")) {
        layer.pd80CbsExposureFar = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "contrastD")) {
        layer.pd80CbsContrastFar = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_contrast_far")) {
        layer.pd80CbsContrastFar = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "brightnessD")) {
        layer.pd80CbsBrightnessFar = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_brightness_far")) {
        layer.pd80CbsBrightnessFar = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "saturationD")) {
        layer.pd80CbsSaturationFar = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_saturation_far")) {
        layer.pd80CbsSaturationFar = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "vibranceD")) {
        layer.pd80CbsVibranceFar = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_vibrance_far")) {
        layer.pd80CbsVibranceFar = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "strength")) {
        layer.pd80CbsStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cbs_strength")) {
        layer.pd80CbsStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80LumaSharpen(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ls_master_strength")) {
        layer.pd80LsMasterStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "master_strength")) {
        layer.pd80LsMasterStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "BlurSigma")) {
        layer.pd80LsBlurSigma = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ls_blur_sigma")) {
        layer.pd80LsBlurSigma = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Sharpening")) {
        layer.pd80LsSharpening = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ls_sharpening")) {
        layer.pd80LsSharpening = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Threshold")) {
        layer.pd80LsThreshold = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ls_threshold")) {
        layer.pd80LsThreshold = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "limiter")) {
        layer.pd80LsLimiter = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ls_limiter")) {
        layer.pd80LsLimiter = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enableShowEdges")) {
        layer.pd80LsShowEdges = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ls_show_edges")) {
        layer.pd80LsShowEdges = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_depth")) {
        layer.pd80LsEnableDepth = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ls_enable_depth")) {
        layer.pd80LsEnableDepth = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_reverse")) {
        layer.pd80LsEnableReverse = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ls_enable_reverse")) {
        layer.pd80LsEnableReverse = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "display_depth")) {
        layer.pd80LsDisplayDepth = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ls_display_depth")) {
        layer.pd80LsDisplayDepth = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "depthStart")) {
        layer.pd80LsDepthStart = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ls_depth_start")) {
        layer.pd80LsDepthStart = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "depthEnd")) {
        layer.pd80LsDepthEnd = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ls_depth_end")) {
        layer.pd80LsDepthEnd = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "depthCurve")) {
        layer.pd80LsDepthCurve = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ls_depth_curve")) {
        layer.pd80LsDepthCurve = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80FilmGrain(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_master_strength")) {
        layer.pd80FgMasterStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "master_strength")) {
        layer.pd80FgMasterStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "grainAdjust")) {
        layer.pd80FgGrainAdjust = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_grain_adjust")) {
        layer.pd80FgGrainAdjust = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "grainSize")) {
        layer.pd80FgGrainSize = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_grain_size")) {
        layer.pd80FgGrainSize = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "grainMotion")) {
        layer.pd80FgGrainMotion = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_grain_motion")) {
        layer.pd80FgGrainMotion = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "grainOrigColor")) {
        layer.pd80FgGrainOrigColor = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_grain_orig_color")) {
        layer.pd80FgGrainOrigColor = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "use_negnoise")) {
        layer.pd80FgUseNegnoise = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_use_negnoise")) {
        layer.pd80FgUseNegnoise = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "grainColor")) {
        layer.pd80FgGrainColor = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_grain_color")) {
        layer.pd80FgGrainColor = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "grainAmount")) {
        layer.pd80FgGrainAmount = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_grain_amount")) {
        layer.pd80FgGrainAmount = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "grainIntensity")) {
        layer.pd80FgGrainIntensity = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_grain_intensity")) {
        layer.pd80FgGrainIntensity = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "grainDensity")) {
        layer.pd80FgGrainDensity = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_grain_density")) {
        layer.pd80FgGrainDensity = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "grainIntHigh")) {
        layer.pd80FgGrainIntHigh = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_grain_int_high")) {
        layer.pd80FgGrainIntHigh = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "grainIntLow")) {
        layer.pd80FgGrainIntLow = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_grain_int_low")) {
        layer.pd80FgGrainIntLow = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_test")) {
        layer.pd80FgEnableTest = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_enable_test")) {
        layer.pd80FgEnableTest = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_depth")) {
        layer.pd80FgEnableDepth = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_enable_depth")) {
        layer.pd80FgEnableDepth = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "display_depth")) {
        layer.pd80FgDisplayDepth = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_display_depth")) {
        layer.pd80FgDisplayDepth = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "depthStart")) {
        layer.pd80FgDepthStart = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_depth_start")) {
        layer.pd80FgDepthStart = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "depthEnd")) {
        layer.pd80FgDepthEnd = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_depth_end")) {
        layer.pd80FgDepthEnd = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "depthCurve")) {
        layer.pd80FgDepthCurve = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fg_depth_curve")) {
        layer.pd80FgDepthCurve = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80DepthSlicer(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ds_master_strength")) {
        layer.pd80DsMasterStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "master_strength")) {
        layer.pd80DsMasterStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ds_depth_near")) {
        layer.pd80DsDepthNear = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "depth_near")) {
        layer.pd80DsDepthNear = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ds_depth_pos")) {
        layer.pd80DsDepthPos = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "depthpos")) {
        layer.pd80DsDepthPos = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ds_depth_far")) {
        layer.pd80DsDepthFar = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "depth_far")) {
        layer.pd80DsDepthFar = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ds_depth_smoothing")) {
        layer.pd80DsDepthSmoothing = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "depth_smoothing")) {
        layer.pd80DsDepthSmoothing = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ds_intensity")) {
        layer.pd80DsIntensity = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "intensity")) {
        layer.pd80DsIntensity = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ds_hue")) {
        layer.pd80DsHue = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "hue")) {
        layer.pd80DsHue = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ds_saturation")) {
        layer.pd80DsSaturation = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "saturation")) {
        layer.pd80DsSaturation = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ds_blend_mode")) {
        layer.pd80DsBlendMode = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blendmode_1")) {
        layer.pd80DsBlendMode = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ds_opacity")) {
        layer.pd80DsOpacity = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "opacity")) {
        layer.pd80DsOpacity = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80ColorSpaceCurves(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_csc_master_strength")) {
        layer.pd80CscMasterStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "master_strength")) {
        layer.pd80CscMasterStrength = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_dither")) {
        layer.pd80CscEnableDither = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_csc_enable_dither")) {
        layer.pd80CscEnableDither = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "dither_strength")) {
        layer.pd80CscDitherStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_csc_dither_strength")) {
        layer.pd80CscDitherStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "color_space")) {
        layer.pd80CscColorSpace = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_csc_color_space")) {
        layer.pd80CscColorSpace = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos0_toe_grey")) {
        layer.pd80CscPos0ToeGrey = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_csc_pos0_toe_grey")) {
        layer.pd80CscPos0ToeGrey = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos1_toe_grey")) {
        layer.pd80CscPos1ToeGrey = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_csc_pos1_toe_grey")) {
        layer.pd80CscPos1ToeGrey = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos0_shoulder_grey")) {
        layer.pd80CscPos0ShoulderGrey = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_csc_pos0_shoulder_grey")) {
        layer.pd80CscPos0ShoulderGrey = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos1_shoulder_grey")) {
        layer.pd80CscPos1ShoulderGrey = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_csc_pos1_shoulder_grey")) {
        layer.pd80CscPos1ShoulderGrey = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "colorsat")) {
        layer.pd80CscColorSat = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_csc_color_sat")) {
        layer.pd80CscColorSat = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80ShadowsMidtonesHighlights(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_master_strength")) {
        layer.pd80SmhMasterStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "master_strength")) {
        layer.pd80SmhMasterStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "luma_mode")) {
        layer.pd80SmhLumaMode = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_luma_mode")) {
        layer.pd80SmhLumaMode = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "separation_mode")) {
        layer.pd80SmhSeparationMode = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_separation_mode")) {
        layer.pd80SmhSeparationMode = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_dither")) {
        layer.pd80SmhEnableDither = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_enable_dither")) {
        layer.pd80SmhEnableDither = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "dither_strength")) {
        layer.pd80SmhDitherStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_dither_strength")) {
        layer.pd80SmhDitherStrength = static_cast<float>(*v);
    }
    if (const auto v = TryParseJsonVec3(p, "blendcolor_s")) {
        layer.pd80SmhBlendColorShadow = *v;
    } else if (const auto v = TryParseJsonVec3(p, "pd80_smh_shadow_blend_color")) {
        layer.pd80SmhBlendColorShadow = *v;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "exposure_s")) {
        layer.pd80SmhShadowExposure = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_shadow_exposure")) {
        layer.pd80SmhShadowExposure = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "contrast_s")) {
        layer.pd80SmhShadowContrast = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_shadow_contrast")) {
        layer.pd80SmhShadowContrast = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "brightness_s")) {
        layer.pd80SmhShadowBrightness = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_shadow_brightness")) {
        layer.pd80SmhShadowBrightness = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blendmode_s")) {
        layer.pd80SmhShadowBlendMode = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_shadow_blend_mode")) {
        layer.pd80SmhShadowBlendMode = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "opacity_s")) {
        layer.pd80SmhShadowOpacity = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_shadow_opacity")) {
        layer.pd80SmhShadowOpacity = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "tint_s")) {
        layer.pd80SmhShadowTint = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_shadow_tint")) {
        layer.pd80SmhShadowTint = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "saturation_s")) {
        layer.pd80SmhShadowSaturation = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_shadow_saturation")) {
        layer.pd80SmhShadowSaturation = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "vibrance_s")) {
        layer.pd80SmhShadowVibrance = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_shadow_vibrance")) {
        layer.pd80SmhShadowVibrance = static_cast<float>(*v);
    }
    if (const auto v = TryParseJsonVec3(p, "blendcolor_m")) {
        layer.pd80SmhBlendColorMid = *v;
    } else if (const auto v = TryParseJsonVec3(p, "pd80_smh_mid_blend_color")) {
        layer.pd80SmhBlendColorMid = *v;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "exposure_m")) {
        layer.pd80SmhMidExposure = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_mid_exposure")) {
        layer.pd80SmhMidExposure = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "contrast_m")) {
        layer.pd80SmhMidContrast = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_mid_contrast")) {
        layer.pd80SmhMidContrast = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "brightness_m")) {
        layer.pd80SmhMidBrightness = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_mid_brightness")) {
        layer.pd80SmhMidBrightness = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blendmode_m")) {
        layer.pd80SmhMidBlendMode = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_mid_blend_mode")) {
        layer.pd80SmhMidBlendMode = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "opacity_m")) {
        layer.pd80SmhMidOpacity = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_mid_opacity")) {
        layer.pd80SmhMidOpacity = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "tint_m")) {
        layer.pd80SmhMidTint = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_mid_tint")) {
        layer.pd80SmhMidTint = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "saturation_m")) {
        layer.pd80SmhMidSaturation = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_mid_saturation")) {
        layer.pd80SmhMidSaturation = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "vibrance_m")) {
        layer.pd80SmhMidVibrance = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_mid_vibrance")) {
        layer.pd80SmhMidVibrance = static_cast<float>(*v);
    }
    if (const auto v = TryParseJsonVec3(p, "blendcolor_h")) {
        layer.pd80SmhBlendColorHighlight = *v;
    } else if (const auto v = TryParseJsonVec3(p, "pd80_smh_highlight_blend_color")) {
        layer.pd80SmhBlendColorHighlight = *v;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "exposure_h")) {
        layer.pd80SmhHighlightExposure = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_highlight_exposure")) {
        layer.pd80SmhHighlightExposure = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "contrast_h")) {
        layer.pd80SmhHighlightContrast = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_highlight_contrast")) {
        layer.pd80SmhHighlightContrast = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "brightness_h")) {
        layer.pd80SmhHighlightBrightness = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_highlight_brightness")) {
        layer.pd80SmhHighlightBrightness = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blendmode_h")) {
        layer.pd80SmhHighlightBlendMode = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_highlight_blend_mode")) {
        layer.pd80SmhHighlightBlendMode = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "opacity_h")) {
        layer.pd80SmhHighlightOpacity = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_highlight_opacity")) {
        layer.pd80SmhHighlightOpacity = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "tint_h")) {
        layer.pd80SmhHighlightTint = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_highlight_tint")) {
        layer.pd80SmhHighlightTint = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "saturation_h")) {
        layer.pd80SmhHighlightSaturation = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_highlight_saturation")) {
        layer.pd80SmhHighlightSaturation = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "vibrance_h")) {
        layer.pd80SmhHighlightVibrance = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_smh_highlight_vibrance")) {
        layer.pd80SmhHighlightVibrance = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80CurvedLevels(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_master_strength")) {
        layer.pd80ClMasterStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "master_strength")) {
        layer.pd80ClMasterStrength = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_dither")) {
        layer.pd80ClEnableDither = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_enable_dither")) {
        layer.pd80ClEnableDither = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "dither_strength")) {
        layer.pd80ClDitherStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_dither_strength")) {
        layer.pd80ClDitherStrength = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_cl_enable_rgb")) {
        layer.pd80ClEnableRgb = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "enable_rgb")) {
        layer.pd80ClEnableRgb = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_enable_rgb")) {
        layer.pd80ClEnableRgb = static_cast<float>(*v);
    }

    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "black_in_grey")) {
        layer.pd80ClGreyBlackIn = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_grey_black_in")) {
        layer.pd80ClGreyBlackIn = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "white_in_grey")) {
        layer.pd80ClGreyWhiteIn = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_grey_white_in")) {
        layer.pd80ClGreyWhiteIn = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "black_out_grey")) {
        layer.pd80ClGreyBlackOut = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_grey_black_out")) {
        layer.pd80ClGreyBlackOut = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "white_out_grey")) {
        layer.pd80ClGreyWhiteOut = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_grey_white_out")) {
        layer.pd80ClGreyWhiteOut = static_cast<float>(*v);
    }

    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos0_shoulder_grey")) {
        layer.pd80ClGreyPos0Shoulder = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_grey_pos0_shoulder")) {
        layer.pd80ClGreyPos0Shoulder = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos1_shoulder_grey")) {
        layer.pd80ClGreyPos1Shoulder = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_grey_pos1_shoulder")) {
        layer.pd80ClGreyPos1Shoulder = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos0_toe_grey")) {
        layer.pd80ClGreyPos0Toe = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_grey_pos0_toe")) {
        layer.pd80ClGreyPos0Toe = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos1_toe_grey")) {
        layer.pd80ClGreyPos1Toe = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_grey_pos1_toe")) {
        layer.pd80ClGreyPos1Toe = static_cast<float>(*v);
    }

    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "black_in_red")) {
        layer.pd80ClRedBlackIn = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_red_black_in")) {
        layer.pd80ClRedBlackIn = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "white_in_red")) {
        layer.pd80ClRedWhiteIn = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_red_white_in")) {
        layer.pd80ClRedWhiteIn = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "black_out_red")) {
        layer.pd80ClRedBlackOut = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_red_black_out")) {
        layer.pd80ClRedBlackOut = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "white_out_red")) {
        layer.pd80ClRedWhiteOut = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_red_white_out")) {
        layer.pd80ClRedWhiteOut = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos0_shoulder_red")) {
        layer.pd80ClRedPos0Shoulder = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_red_pos0_shoulder")) {
        layer.pd80ClRedPos0Shoulder = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos1_shoulder_red")) {
        layer.pd80ClRedPos1Shoulder = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_red_pos1_shoulder")) {
        layer.pd80ClRedPos1Shoulder = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos0_toe_red")) {
        layer.pd80ClRedPos0Toe = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_red_pos0_toe")) {
        layer.pd80ClRedPos0Toe = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos1_toe_red")) {
        layer.pd80ClRedPos1Toe = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_red_pos1_toe")) {
        layer.pd80ClRedPos1Toe = static_cast<float>(*v);
    }

    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "black_in_green")) {
        layer.pd80ClGreenBlackIn = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_green_black_in")) {
        layer.pd80ClGreenBlackIn = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "white_in_green")) {
        layer.pd80ClGreenWhiteIn = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_green_white_in")) {
        layer.pd80ClGreenWhiteIn = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "black_out_green")) {
        layer.pd80ClGreenBlackOut = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_green_black_out")) {
        layer.pd80ClGreenBlackOut = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "white_out_green")) {
        layer.pd80ClGreenWhiteOut = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_green_white_out")) {
        layer.pd80ClGreenWhiteOut = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos0_shoulder_green")) {
        layer.pd80ClGreenPos0Shoulder = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_green_pos0_shoulder")) {
        layer.pd80ClGreenPos0Shoulder = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos1_shoulder_green")) {
        layer.pd80ClGreenPos1Shoulder = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_green_pos1_shoulder")) {
        layer.pd80ClGreenPos1Shoulder = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos0_toe_green")) {
        layer.pd80ClGreenPos0Toe = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_green_pos0_toe")) {
        layer.pd80ClGreenPos0Toe = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos1_toe_green")) {
        layer.pd80ClGreenPos1Toe = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_green_pos1_toe")) {
        layer.pd80ClGreenPos1Toe = static_cast<float>(*v);
    }

    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "black_in_blue")) {
        layer.pd80ClBlueBlackIn = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_blue_black_in")) {
        layer.pd80ClBlueBlackIn = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "white_in_blue")) {
        layer.pd80ClBlueWhiteIn = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_blue_white_in")) {
        layer.pd80ClBlueWhiteIn = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "black_out_blue")) {
        layer.pd80ClBlueBlackOut = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_blue_black_out")) {
        layer.pd80ClBlueBlackOut = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "white_out_blue")) {
        layer.pd80ClBlueWhiteOut = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_blue_white_out")) {
        layer.pd80ClBlueWhiteOut = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos0_shoulder_blue")) {
        layer.pd80ClBluePos0Shoulder = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_blue_pos0_shoulder")) {
        layer.pd80ClBluePos0Shoulder = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos1_shoulder_blue")) {
        layer.pd80ClBluePos1Shoulder = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_blue_pos1_shoulder")) {
        layer.pd80ClBluePos1Shoulder = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos0_toe_blue")) {
        layer.pd80ClBluePos0Toe = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_blue_pos0_toe")) {
        layer.pd80ClBluePos0Toe = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pos1_toe_blue")) {
        layer.pd80ClBluePos1Toe = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cl_blue_pos1_toe")) {
        layer.pd80ClBluePos1Toe = static_cast<float>(*v);
    }

    MergePresentationObject(p, layer);
}

void MergePd80SelectiveColor(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_sc_master_strength")) {
        layer.pd80ScMasterStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "master_strength")) {
        layer.pd80ScMasterStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "corr_method")) {
        layer.pd80ScCorrectionMethod = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_sc_correction_method")) {
        layer.pd80ScCorrectionMethod = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "corr_method2")) {
        layer.pd80ScCorrectionMethodSaturation = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_sc_correction_method_saturation")) {
        layer.pd80ScCorrectionMethodSaturation = static_cast<float>(*v);
    }

    auto grab = [&](std::string_view key, float& out) {
        if (const std::optional<double> v = detail::ExtractJsonDouble(p, key)) {
            out = static_cast<float>(*v);
        }
    };

    grab("r_adj_cya", layer.pd80ScRedsCyan);
    grab("r_adj_mag", layer.pd80ScRedsMagenta);
    grab("r_adj_yel", layer.pd80ScRedsYellow);
    grab("r_adj_bla", layer.pd80ScRedsBlack);
    grab("r_adj_sat", layer.pd80ScRedsSaturation);
    grab("r_adj_vib", layer.pd80ScRedsVibrance);

    grab("y_adj_cya", layer.pd80ScYellowsCyan);
    grab("y_adj_mag", layer.pd80ScYellowsMagenta);
    grab("y_adj_yel", layer.pd80ScYellowsYellow);
    grab("y_adj_bla", layer.pd80ScYellowsBlack);
    grab("y_adj_sat", layer.pd80ScYellowsSaturation);
    grab("y_adj_vib", layer.pd80ScYellowsVibrance);

    grab("g_adj_cya", layer.pd80ScGreensCyan);
    grab("g_adj_mag", layer.pd80ScGreensMagenta);
    grab("g_adj_yel", layer.pd80ScGreensYellow);
    grab("g_adj_bla", layer.pd80ScGreensBlack);
    grab("g_adj_sat", layer.pd80ScGreensSaturation);
    grab("g_adj_vib", layer.pd80ScGreensVibrance);

    grab("c_adj_cya", layer.pd80ScCyansCyan);
    grab("c_adj_mag", layer.pd80ScCyansMagenta);
    grab("c_adj_yel", layer.pd80ScCyansYellow);
    grab("c_adj_bla", layer.pd80ScCyansBlack);
    grab("c_adj_sat", layer.pd80ScCyansSaturation);
    grab("c_adj_vib", layer.pd80ScCyansVibrance);

    grab("b_adj_cya", layer.pd80ScBluesCyan);
    grab("b_adj_mag", layer.pd80ScBluesMagenta);
    grab("b_adj_yel", layer.pd80ScBluesYellow);
    grab("b_adj_bla", layer.pd80ScBluesBlack);
    grab("b_adj_sat", layer.pd80ScBluesSaturation);
    grab("b_adj_vib", layer.pd80ScBluesVibrance);

    grab("m_adj_cya", layer.pd80ScMagentasCyan);
    grab("m_adj_mag", layer.pd80ScMagentasMagenta);
    grab("m_adj_yel", layer.pd80ScMagentasYellow);
    grab("m_adj_bla", layer.pd80ScMagentasBlack);
    grab("m_adj_sat", layer.pd80ScMagentasSaturation);
    grab("m_adj_vib", layer.pd80ScMagentasVibrance);

    grab("w_adj_cya", layer.pd80ScWhitesCyan);
    grab("w_adj_mag", layer.pd80ScWhitesMagenta);
    grab("w_adj_yel", layer.pd80ScWhitesYellow);
    grab("w_adj_bla", layer.pd80ScWhitesBlack);
    grab("w_adj_sat", layer.pd80ScWhitesSaturation);
    grab("w_adj_vib", layer.pd80ScWhitesVibrance);

    grab("n_adj_cya", layer.pd80ScNeutralsCyan);
    grab("n_adj_mag", layer.pd80ScNeutralsMagenta);
    grab("n_adj_yel", layer.pd80ScNeutralsYellow);
    grab("n_adj_bla", layer.pd80ScNeutralsBlack);
    grab("n_adj_sat", layer.pd80ScNeutralsSaturation);
    grab("n_adj_vib", layer.pd80ScNeutralsVibrance);

    grab("bk_adj_cya", layer.pd80ScBlacksCyan);
    grab("bk_adj_mag", layer.pd80ScBlacksMagenta);
    grab("bk_adj_yel", layer.pd80ScBlacksYellow);
    grab("bk_adj_bla", layer.pd80ScBlacksBlack);
    grab("bk_adj_sat", layer.pd80ScBlacksSaturation);
    grab("bk_adj_vib", layer.pd80ScBlacksVibrance);

    MergePresentationObject(p, layer);
}

void MergePd80PosterizePixelate(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "master_strength")) {
        layer.pd80PpMasterStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_pp_master_strength")) {
        layer.pd80PpMasterStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "number_of_levels")) {
        layer.pd80PpNumberOfLevels = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_pp_number_of_levels")) {
        layer.pd80PpNumberOfLevels = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pixel_size")) {
        layer.pd80PpPixelSize = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_pp_pixel_size")) {
        layer.pd80PpPixelSize = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "border_str")) {
        layer.pd80PpBorderStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_pp_border_strength")) {
        layer.pd80PpBorderStrength = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_dither")) {
        layer.pd80PpEnableDither = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_pp_enable_dither")) {
        layer.pd80PpEnableDither = *v ? 1.0f : 0.0f;
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "dither_motion")) {
        layer.pd80PpDitherMotion = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_pp_dither_motion")) {
        layer.pd80PpDitherMotion = *v ? 1.0f : 0.0f;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "dither_strength")) {
        layer.pd80PpDitherStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_pp_dither_strength")) {
        layer.pd80PpDitherStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80MagicalRectangle(std::string_view p, PostProcessParameters& layer) {
    auto grab = [&](std::string_view key, float& out) {
        if (const std::optional<double> v = detail::ExtractJsonDouble(p, key)) {
            out = static_cast<float>(*v);
        }
    };

    grab("shape", layer.pd80MrShape);
    grab("pd80_mr_shape", layer.pd80MrShape);
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "invert_shape")) {
        layer.pd80MrInvertShape = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_mr_invert_shape")) {
        layer.pd80MrInvertShape = *v ? 1.0f : 0.0f;
    }
    grab("rotation", layer.pd80MrRotation);
    grab("pd80_mr_rotation", layer.pd80MrRotation);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "center_x")) {
        layer.pd80MrCenter.x = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_mr_center_x")) {
        layer.pd80MrCenter.x = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "center_y")) {
        layer.pd80MrCenter.y = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_mr_center_y")) {
        layer.pd80MrCenter.y = static_cast<float>(*v);
    }
    grab("ret_size_x", layer.pd80MrSizeX);
    grab("pd80_mr_size_x", layer.pd80MrSizeX);
    grab("ret_size_y", layer.pd80MrSizeY);
    grab("pd80_mr_size_y", layer.pd80MrSizeY);
    grab("depthpos", layer.pd80MrDepthPosition);
    grab("pd80_mr_depth_position", layer.pd80MrDepthPosition);
    grab("smoothing", layer.pd80MrSmoothing);
    grab("pd80_mr_smoothing", layer.pd80MrSmoothing);
    grab("depth_smoothing", layer.pd80MrDepthSmoothing);
    grab("pd80_mr_depth_smoothing", layer.pd80MrDepthSmoothing);
    grab("dither_strength", layer.pd80MrDitherStrength);
    grab("pd80_mr_dither_strength", layer.pd80MrDitherStrength);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "reccolor_r")) {
        layer.pd80MrColor.x = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_mr_color_r")) {
        layer.pd80MrColor.x = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "reccolor_g")) {
        layer.pd80MrColor.y = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_mr_color_g")) {
        layer.pd80MrColor.y = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "reccolor_b")) {
        layer.pd80MrColor.z = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_mr_color_b")) {
        layer.pd80MrColor.z = static_cast<float>(*v);
    }
    grab("mr_exposure", layer.pd80MrExposure);
    grab("pd80_mr_exposure", layer.pd80MrExposure);
    grab("mr_contrast", layer.pd80MrContrast);
    grab("pd80_mr_contrast", layer.pd80MrContrast);
    grab("mr_brightness", layer.pd80MrBrightness);
    grab("pd80_mr_brightness", layer.pd80MrBrightness);
    grab("mr_hue", layer.pd80MrHue);
    grab("pd80_mr_hue", layer.pd80MrHue);
    grab("mr_saturation", layer.pd80MrSaturation);
    grab("pd80_mr_saturation", layer.pd80MrSaturation);
    grab("mr_vibrance", layer.pd80MrVibrance);
    grab("pd80_mr_vibrance", layer.pd80MrVibrance);
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_gradient")) {
        layer.pd80MrEnableGradient = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_mr_enable_gradient")) {
        layer.pd80MrEnableGradient = *v ? 1.0f : 0.0f;
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "gradient_type")) {
        layer.pd80MrGradientType = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_mr_gradient_type")) {
        layer.pd80MrGradientType = *v ? 1.0f : 0.0f;
    }
    grab("gradient_curve", layer.pd80MrGradientCurve);
    grab("pd80_mr_gradient_curve", layer.pd80MrGradientCurve);
    grab("intensity_boost", layer.pd80MrIntensityBoost);
    grab("pd80_mr_intensity_boost", layer.pd80MrIntensityBoost);
    grab("blendmode_1", layer.pd80MrBlendMode);
    grab("pd80_mr_blend_mode", layer.pd80MrBlendMode);
    grab("opacity", layer.pd80MrOpacity);
    grab("pd80_mr_opacity", layer.pd80MrOpacity);
    MergePresentationObject(p, layer);
}

void MergePd80BonusLutPack(std::string_view p, PostProcessParameters& layer) {
    auto grab = [&](std::string_view key, float& out) {
        if (const std::optional<double> v = detail::ExtractJsonDouble(p, key)) {
            out = static_cast<float>(*v);
        }
    };
    grab("master_strength", layer.pd80BlpMasterStrength);
    grab("pd80_blp_master_strength", layer.pd80BlpMasterStrength);
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_dither")) {
        layer.pd80BlpEnableDither = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_blp_enable_dither")) {
        layer.pd80BlpEnableDither = *v ? 1.0f : 0.0f;
    }
    grab("dither_strength", layer.pd80BlpDitherStrength);
    grab("pd80_blp_dither_strength", layer.pd80BlpDitherStrength);
    grab("PD80_LutSelector", layer.pd80BlpLutSelector);
    grab("pd80_blp_lut_selector", layer.pd80BlpLutSelector);
    grab("PD80_MixChroma", layer.pd80BlpMixChroma);
    grab("pd80_blp_mix_chroma", layer.pd80BlpMixChroma);
    grab("PD80_MixLuma", layer.pd80BlpMixLuma);
    grab("pd80_blp_mix_luma", layer.pd80BlpMixLuma);
    grab("ig", layer.pd80BlpGamma);
    grab("pd80_blp_gamma", layer.pd80BlpGamma);

    grab("ib_r", layer.pd80BlpBlackIn.x);
    grab("ib_g", layer.pd80BlpBlackIn.y);
    grab("ib_b", layer.pd80BlpBlackIn.z);
    grab("pd80_blp_black_in_r", layer.pd80BlpBlackIn.x);
    grab("pd80_blp_black_in_g", layer.pd80BlpBlackIn.y);
    grab("pd80_blp_black_in_b", layer.pd80BlpBlackIn.z);
    grab("iw_r", layer.pd80BlpWhiteIn.x);
    grab("iw_g", layer.pd80BlpWhiteIn.y);
    grab("iw_b", layer.pd80BlpWhiteIn.z);
    grab("pd80_blp_white_in_r", layer.pd80BlpWhiteIn.x);
    grab("pd80_blp_white_in_g", layer.pd80BlpWhiteIn.y);
    grab("pd80_blp_white_in_b", layer.pd80BlpWhiteIn.z);
    grab("ob_r", layer.pd80BlpBlackOut.x);
    grab("ob_g", layer.pd80BlpBlackOut.y);
    grab("ob_b", layer.pd80BlpBlackOut.z);
    grab("pd80_blp_black_out_r", layer.pd80BlpBlackOut.x);
    grab("pd80_blp_black_out_g", layer.pd80BlpBlackOut.y);
    grab("pd80_blp_black_out_b", layer.pd80BlpBlackOut.z);
    grab("ow_r", layer.pd80BlpWhiteOut.x);
    grab("ow_g", layer.pd80BlpWhiteOut.y);
    grab("ow_b", layer.pd80BlpWhiteOut.z);
    grab("pd80_blp_white_out_r", layer.pd80BlpWhiteOut.x);
    grab("pd80_blp_white_out_g", layer.pd80BlpWhiteOut.y);
    grab("pd80_blp_white_out_b", layer.pd80BlpWhiteOut.z);
    MergePresentationObject(p, layer);
}

void MergePd80CinetoolsLut(std::string_view p, PostProcessParameters& layer) {
    auto grab = [&](std::string_view key, float& out) {
        if (const std::optional<double> v = detail::ExtractJsonDouble(p, key)) {
            out = static_cast<float>(*v);
        }
    };
    grab("master_strength", layer.pd80CltMasterStrength);
    grab("pd80_clt_master_strength", layer.pd80CltMasterStrength);
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_dither")) {
        layer.pd80CltEnableDither = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_clt_enable_dither")) {
        layer.pd80CltEnableDither = *v ? 1.0f : 0.0f;
    }
    grab("dither_strength", layer.pd80CltDitherStrength);
    grab("pd80_clt_dither_strength", layer.pd80CltDitherStrength);
    grab("PD80_LutSelector", layer.pd80CltLutSelector);
    grab("pd80_clt_lut_selector", layer.pd80CltLutSelector);
    grab("PD80_MixChroma", layer.pd80CltMixChroma);
    grab("pd80_clt_mix_chroma", layer.pd80CltMixChroma);
    grab("PD80_MixLuma", layer.pd80CltMixLuma);
    grab("pd80_clt_mix_luma", layer.pd80CltMixLuma);
    grab("ig", layer.pd80CltGamma);
    grab("pd80_clt_gamma", layer.pd80CltGamma);

    grab("ib_r", layer.pd80CltBlackIn.x);
    grab("ib_g", layer.pd80CltBlackIn.y);
    grab("ib_b", layer.pd80CltBlackIn.z);
    grab("pd80_clt_black_in_r", layer.pd80CltBlackIn.x);
    grab("pd80_clt_black_in_g", layer.pd80CltBlackIn.y);
    grab("pd80_clt_black_in_b", layer.pd80CltBlackIn.z);
    grab("iw_r", layer.pd80CltWhiteIn.x);
    grab("iw_g", layer.pd80CltWhiteIn.y);
    grab("iw_b", layer.pd80CltWhiteIn.z);
    grab("pd80_clt_white_in_r", layer.pd80CltWhiteIn.x);
    grab("pd80_clt_white_in_g", layer.pd80CltWhiteIn.y);
    grab("pd80_clt_white_in_b", layer.pd80CltWhiteIn.z);
    grab("ob_r", layer.pd80CltBlackOut.x);
    grab("ob_g", layer.pd80CltBlackOut.y);
    grab("ob_b", layer.pd80CltBlackOut.z);
    grab("pd80_clt_black_out_r", layer.pd80CltBlackOut.x);
    grab("pd80_clt_black_out_g", layer.pd80CltBlackOut.y);
    grab("pd80_clt_black_out_b", layer.pd80CltBlackOut.z);
    grab("ow_r", layer.pd80CltWhiteOut.x);
    grab("ow_g", layer.pd80CltWhiteOut.y);
    grab("ow_b", layer.pd80CltWhiteOut.z);
    grab("pd80_clt_white_out_r", layer.pd80CltWhiteOut.x);
    grab("pd80_clt_white_out_g", layer.pd80CltWhiteOut.y);
    grab("pd80_clt_white_out_b", layer.pd80CltWhiteOut.z);
    MergePresentationObject(p, layer);
}

void MergePd80LutCreator(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "master_strength")) {
        layer.pd80LcMasterStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_lc_master_strength")) {
        layer.pd80LcMasterStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "texture_width")) {
        layer.pd80LcTextureWidth = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_lc_texture_width")) {
        layer.pd80LcTextureWidth = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "texture_height")) {
        layer.pd80LcTextureHeight = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_lc_texture_height")) {
        layer.pd80LcTextureHeight = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80LumaFade(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "master_strength")) {
        layer.pd80LfMasterStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_lf_master_strength")) {
        layer.pd80LfMasterStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "transition_speed")) {
        layer.pd80LfTransitionSpeed = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_lf_transition_speed")) {
        layer.pd80LfTransitionSpeed = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "minlevel")) {
        layer.pd80LfMinLevel = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_lf_min_level")) {
        layer.pd80LfMinLevel = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "maxlevel")) {
        layer.pd80LfMaxLevel = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_lf_max_level")) {
        layer.pd80LfMaxLevel = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80ColorGradients(std::string_view p, PostProcessParameters& layer) {
    auto grab = [&](std::string_view key, float& out) {
        if (const std::optional<double> v = detail::ExtractJsonDouble(p, key)) {
            out = static_cast<float>(*v);
        }
    };
    grab("master_strength", layer.pd80Cg4MasterStrength);
    grab("pd80_cg4_master_strength", layer.pd80Cg4MasterStrength);
    grab("luma_mode", layer.pd80Cg4LumaMode);
    grab("pd80_cg4_luma_mode", layer.pd80Cg4LumaMode);
    grab("separation_mode", layer.pd80Cg4SeparationMode);
    grab("pd80_cg4_separation_mode", layer.pd80Cg4SeparationMode);
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_dither")) {
        layer.pd80Cg4EnableDither = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_cg4_enable_dither")) {
        layer.pd80Cg4EnableDither = *v ? 1.0f : 0.0f;
    }
    grab("dither_strength", layer.pd80Cg4DitherStrength);
    grab("pd80_cg4_dither_strength", layer.pd80Cg4DitherStrength);
    grab("CGdesat", layer.pd80Cg4DesaturateBase);
    grab("pd80_cg4_desaturate_base", layer.pd80Cg4DesaturateBase);
    grab("finalmix", layer.pd80Cg4FinalMix);
    grab("pd80_cg4_final_mix", layer.pd80Cg4FinalMix);
    grab("blendmode_ls_m", layer.pd80Cg4LightSceneMidBlendMode);
    grab("opacity_ls_m", layer.pd80Cg4LightSceneMidOpacity);
    grab("blendmode_ls_s", layer.pd80Cg4LightSceneShadowBlendMode);
    grab("opacity_ls_s", layer.pd80Cg4LightSceneShadowOpacity);
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_ds")) {
        layer.pd80Cg4EnableDarkScene = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_cg4_enable_ds")) {
        layer.pd80Cg4EnableDarkScene = *v ? 1.0f : 0.0f;
    }
    grab("blendmode_ds_m", layer.pd80Cg4DarkSceneMidBlendMode);
    grab("opacity_ds_m", layer.pd80Cg4DarkSceneMidOpacity);
    grab("blendmode_ds_s", layer.pd80Cg4DarkSceneShadowBlendMode);
    grab("opacity_ds_s", layer.pd80Cg4DarkSceneShadowOpacity);
    grab("minlevel", layer.pd80Cg4MinLevel);
    grab("maxlevel", layer.pd80Cg4MaxLevel);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blendcolor_ls_m_r")) layer.pd80Cg4LightSceneMidColor.x = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blendcolor_ls_m_g")) layer.pd80Cg4LightSceneMidColor.y = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blendcolor_ls_m_b")) layer.pd80Cg4LightSceneMidColor.z = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blendcolor_ls_s_r")) layer.pd80Cg4LightSceneShadowColor.x = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blendcolor_ls_s_g")) layer.pd80Cg4LightSceneShadowColor.y = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blendcolor_ls_s_b")) layer.pd80Cg4LightSceneShadowColor.z = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blendcolor_ds_m_r")) layer.pd80Cg4DarkSceneMidColor.x = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blendcolor_ds_m_g")) layer.pd80Cg4DarkSceneMidColor.y = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blendcolor_ds_m_b")) layer.pd80Cg4DarkSceneMidColor.z = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blendcolor_ds_s_r")) layer.pd80Cg4DarkSceneShadowColor.x = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blendcolor_ds_s_g")) layer.pd80Cg4DarkSceneShadowColor.y = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "blendcolor_ds_s_b")) layer.pd80Cg4DarkSceneShadowColor.z = static_cast<float>(*v);
    MergePresentationObject(p, layer);
}

void MergePd80CorrectContrast(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "master_strength")) {
        layer.pd80CcMasterStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cc_master_strength")) {
        layer.pd80CcMasterStrength = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "rt_enable_whitepoint_correction")) {
        layer.pd80CcEnableWhitepoint = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_cc_enable_whitepoint")) {
        layer.pd80CcEnableWhitepoint = *v ? 1.0f : 0.0f;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "rt_wp_str")) {
        layer.pd80CcWhitepointStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cc_whitepoint_strength")) {
        layer.pd80CcWhitepointStrength = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "rt_enable_blackpoint_correction")) {
        layer.pd80CcEnableBlackpoint = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_cc_enable_blackpoint")) {
        layer.pd80CcEnableBlackpoint = *v ? 1.0f : 0.0f;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "rt_bp_str")) {
        layer.pd80CcBlackpointStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cc_blackpoint_strength")) {
        layer.pd80CcBlackpointStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80RtCorrectColor(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "master_strength")) {
        layer.pd80RccMasterStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_rcc_master_strength")) {
        layer.pd80RccMasterStrength = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_dither")) {
        layer.pd80RccEnableDither = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_rcc_enable_dither")) {
        layer.pd80RccEnableDither = *v ? 1.0f : 0.0f;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "dither_strength")) {
        layer.pd80RccDitherStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_rcc_dither_strength")) {
        layer.pd80RccDitherStrength = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "rt_enable_whitepoint_correction")) {
        layer.pd80RccEnableWhitepoint = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_rcc_enable_whitepoint")) {
        layer.pd80RccEnableWhitepoint = *v ? 1.0f : 0.0f;
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "rt_whitepoint_respect_luma")) {
        layer.pd80RccWhitepointRespectLuma = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_rcc_whitepoint_respect_luma")) {
        layer.pd80RccWhitepointRespectLuma = *v ? 1.0f : 0.0f;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "rt_whitepoint_method")) {
        layer.pd80RccWhitepointMethod = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_rcc_whitepoint_method")) {
        layer.pd80RccWhitepointMethod = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "rt_wp_str")) {
        layer.pd80RccWhitepointStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_rcc_whitepoint_strength")) {
        layer.pd80RccWhitepointStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "rt_wp_rl_str")) {
        layer.pd80RccWhitepointLumaStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_rcc_whitepoint_luma_strength")) {
        layer.pd80RccWhitepointLumaStrength = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "rt_enable_blackpoint_correction")) {
        layer.pd80RccEnableBlackpoint = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_rcc_enable_blackpoint")) {
        layer.pd80RccEnableBlackpoint = *v ? 1.0f : 0.0f;
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "rt_blackpoint_respect_luma")) {
        layer.pd80RccBlackpointRespectLuma = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_rcc_blackpoint_respect_luma")) {
        layer.pd80RccBlackpointRespectLuma = *v ? 1.0f : 0.0f;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "rt_blackpoint_method")) {
        layer.pd80RccBlackpointMethod = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_rcc_blackpoint_method")) {
        layer.pd80RccBlackpointMethod = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "rt_bp_str")) {
        layer.pd80RccBlackpointStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_rcc_blackpoint_strength")) {
        layer.pd80RccBlackpointStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "rt_bp_rl_str")) {
        layer.pd80RccBlackpointLumaStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_rcc_blackpoint_luma_strength")) {
        layer.pd80RccBlackpointLumaStrength = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "rt_enable_midpoint_correction")) {
        layer.pd80RccEnableMidpoint = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_rcc_enable_midpoint")) {
        layer.pd80RccEnableMidpoint = *v ? 1.0f : 0.0f;
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "rt_midpoint_respect_luma")) {
        layer.pd80RccMidpointRespectLuma = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_rcc_midpoint_respect_luma")) {
        layer.pd80RccMidpointRespectLuma = *v ? 1.0f : 0.0f;
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "mid_use_alt_method")) {
        layer.pd80RccMidUseAltMethod = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_rcc_mid_use_alt_method")) {
        layer.pd80RccMidUseAltMethod = *v ? 1.0f : 0.0f;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "midCC_scale")) {
        layer.pd80RccMidScale = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_rcc_mid_scale")) {
        layer.pd80RccMidScale = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80FilmicAdaptation(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "master_strength")) {
        layer.pd80FaMasterStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fa_master_strength")) {
        layer.pd80FaMasterStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "adj_shoulder")) {
        layer.pd80FaAdjustShoulder = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fa_adjust_shoulder")) {
        layer.pd80FaAdjustShoulder = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "adj_linear")) {
        layer.pd80FaAdjustLinear = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fa_adjust_linear")) {
        layer.pd80FaAdjustLinear = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "adj_toe")) {
        layer.pd80FaAdjustToe = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_fa_adjust_toe")) {
        layer.pd80FaAdjustToe = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80HqBloom(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "master_strength")) layer.pd80HbMasterStrength = static_cast<float>(*v);
    else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_hb_master_strength")) layer.pd80HbMasterStrength = static_cast<float>(*v);
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "debugBloom")) layer.pd80HbDebugBloom = *v ? 1.0f : 0.0f;
    else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "pd80_hb_debug_bloom")) layer.pd80HbDebugBloom = *v ? 1.0f : 0.0f;
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "dither_strength")) layer.pd80HbDitherStrength = static_cast<float>(*v);
    else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_hb_dither_strength")) layer.pd80HbDitherStrength = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "BloomMix")) layer.pd80HbMix = static_cast<float>(*v);
    else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_hb_mix")) layer.pd80HbMix = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "BloomLimit")) layer.pd80HbThreshold = static_cast<float>(*v);
    else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_hb_threshold")) layer.pd80HbThreshold = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "GreyValue")) layer.pd80HbGreyValue = static_cast<float>(*v);
    else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_hb_grey_value")) layer.pd80HbGreyValue = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "bExposure")) layer.pd80HbExposure = static_cast<float>(*v);
    else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_hb_exposure")) layer.pd80HbExposure = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "BlurSigma")) layer.pd80HbBlurSigma = static_cast<float>(*v);
    else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_hb_blur_sigma")) layer.pd80HbBlurSigma = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "BloomSaturation")) layer.pd80HbSaturation = static_cast<float>(*v);
    else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_hb_saturation")) layer.pd80HbSaturation = static_cast<float>(*v);
    MergePresentationObject(p, layer);
}

void MergePd80SelectiveColorV2(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "master_strength")) layer.pd80Sc2MasterStrength = static_cast<float>(*v);
    else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_sc2_master_strength")) layer.pd80Sc2MasterStrength = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "corr_method")) layer.pd80Sc2CorrectionMethod = static_cast<float>(*v);
    else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_sc2_correction_method")) layer.pd80Sc2CorrectionMethod = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_sc2_saturation_scale")) layer.pd80Sc2SaturationScale = static_cast<float>(*v);
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_sc2_lightness_scale")) layer.pd80Sc2LightnessScale = static_cast<float>(*v);
    MergePresentationObject(p, layer);
}

void MergePd80ColorGamut(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_cg_master_strength")) {
        layer.pd80CgMasterStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "master_strength")) {
        layer.pd80CgMasterStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_color_gamut")) {
        layer.pd80ColorGamut = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "colorgamut")) {
        layer.pd80ColorGamut = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergePd80ChromaticAberration(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_master_strength")) {
        layer.pd80CaMasterStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "master_strength")) {
        layer.pd80CaMasterStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "CA_strength")) {
        layer.pd80CaEffectStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_effect_strength")) {
        layer.pd80CaEffectStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "CA")) {
        layer.pd80CaGlobalWidth = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_global_width")) {
        layer.pd80CaGlobalWidth = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sampleSTEPS")) {
        layer.pd80CaSampleSteps = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_sample_steps")) {
        layer.pd80CaSampleSteps = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "CA_type")) {
        layer.pd80CaType = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_type")) {
        layer.pd80CaType = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "degrees")) {
        layer.pd80CaDegrees = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_degrees")) {
        layer.pd80CaDegrees = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "CA_width")) {
        layer.pd80CaWidth = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_width")) {
        layer.pd80CaWidth = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "CA_curve")) {
        layer.pd80CaCurve = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_curve")) {
        layer.pd80CaCurve = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "oX")) {
        layer.pd80CaOX = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_o_x")) {
        layer.pd80CaOX = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "oY")) {
        layer.pd80CaOY = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_o_y")) {
        layer.pd80CaOY = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "CA_shapeX")) {
        layer.pd80CaShapeX = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_shape_x")) {
        layer.pd80CaShapeX = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "CA_shapeY")) {
        layer.pd80CaShapeY = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_shape_y")) {
        layer.pd80CaShapeY = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "show_CA")) {
        layer.pd80CaShowCa = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_show_ca")) {
        layer.pd80CaShowCa = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_depth_int")) {
        layer.pd80CaEnableDepthInt = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_enable_depth_int")) {
        layer.pd80CaEnableDepthInt = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "enable_depth_width")) {
        layer.pd80CaEnableDepthWidth = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_enable_depth_width")) {
        layer.pd80CaEnableDepthWidth = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "display_depth")) {
        layer.pd80CaDisplayDepth = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_display_depth")) {
        layer.pd80CaDisplayDepth = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "depthStart")) {
        layer.pd80CaDepthStart = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_depth_start")) {
        layer.pd80CaDepthStart = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "depthEnd")) {
        layer.pd80CaDepthEnd = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_depth_end")) {
        layer.pd80CaDepthEnd = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "depthCurve")) {
        layer.pd80CaDepthCurve = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_ca_depth_curve")) {
        layer.pd80CaDepthCurve = static_cast<float>(*v);
    }
    if (const auto v = TryParseJsonVec3(p, "vignetteColor")) {
        layer.pd80CaVignetteColor = *v;
    }
    MergePresentationObject(p, layer);
}

void MergePd80ColorIsolation(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "hueMid")) {
        layer.pd80ColorIsolationHueMid = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "hue_mid")) {
        layer.pd80ColorIsolationHueMid = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_color_isolation_hue_mid")) {
        layer.pd80ColorIsolationHueMid = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "hueRange")) {
        layer.pd80ColorIsolationHueRange = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "hue_range")) {
        layer.pd80ColorIsolationHueRange = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_color_isolation_hue_range")) {
        layer.pd80ColorIsolationHueRange = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "satLimit")) {
        layer.pd80ColorIsolationSatLimit = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sat_limit")) {
        layer.pd80ColorIsolationSatLimit = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_color_isolation_sat_limit")) {
        layer.pd80ColorIsolationSatLimit = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fxcolorMix")) {
        layer.pd80ColorIsolationFxMix = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fx_color_mix")) {
        layer.pd80ColorIsolationFxMix = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_color_isolation_fx_mix")) {
        layer.pd80ColorIsolationFxMix = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "pd80_color_isolation_strength")) {
        layer.pd80ColorIsolationStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeSepia(std::string_view p, PostProcessParameters& layer) {
    if (const auto t = TryParseJsonVec3(p, "sepia_tint")) {
        layer.sepiaTint = *t;
    } else if (const auto t = TryParseJsonVec3(p, "sepiaTint")) {
        layer.sepiaTint = *t;
    } else if (const auto t = TryParseJsonVec3(p, "tint")) {
        layer.sepiaTint = *t;
    }
    if (const std::optional<double> s = detail::ExtractJsonDouble(p, "sepia_strength")) {
        layer.sepiaStrength = static_cast<float>(*s);
    } else if (const std::optional<double> s = detail::ExtractJsonDouble(p, "strength")) {
        layer.sepiaStrength = static_cast<float>(*s);
    }
    MergePresentationObject(p, layer);
}

void MergeMonochrome(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "monochrome_preset")) {
        layer.monochromePreset = std::clamp(static_cast<int>(std::lround(*pr)), 0, 17);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "preset")) {
        layer.monochromePreset = std::clamp(static_cast<int>(std::lround(*pr)), 0, 17);
    }
    if (const auto c = TryParseJsonVec3(p, "monochrome_custom_coeff")) {
        layer.monochromeCustomCoeff = *c;
    } else if (const auto c = TryParseJsonVec3(p, "monochrome_conversion_values")) {
        layer.monochromeCustomCoeff = *c;
    } else if (const auto c = TryParseJsonVec3(p, "conversion")) {
        layer.monochromeCustomCoeff = *c;
    } else if (const auto c = TryParseJsonVec3(p, "custom_coeff")) {
        layer.monochromeCustomCoeff = *c;
    }
    if (const std::optional<double> sat = detail::ExtractJsonDouble(p, "monochrome_color_saturation")) {
        layer.monochromeColorSaturation = static_cast<float>(*sat);
    } else if (const std::optional<double> sat = detail::ExtractJsonDouble(p, "color_saturation")) {
        layer.monochromeColorSaturation = static_cast<float>(*sat);
    }
    MergePresentationObject(p, layer);
}

void MergeLevels(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "black_point")) {
        layer.levelsBlackPoint = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "BlackPoint")) {
        layer.levelsBlackPoint = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "levels_black_point")) {
        layer.levelsBlackPoint = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "white_point")) {
        layer.levelsWhitePoint = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "WhitePoint")) {
        layer.levelsWhitePoint = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "levels_white_point")) {
        layer.levelsWhitePoint = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "strength")) {
        layer.levelsStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "levels_strength")) {
        layer.levelsStrength = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "highlight_clipping")) {
        layer.levelsClipHighlight = *v ? 1.0f : 0.0f;
    } else if (const std::optional<bool> v = detail::ExtractJsonBool(p, "HighlightClipping")) {
        layer.levelsClipHighlight = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "levels_clip_highlight")) {
        layer.levelsClipHighlight = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeSweetFxCartoon(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Power")) {
        layer.sweetFxCartoonPower = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_cartoon_power")) {
        layer.sweetFxCartoonPower = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "EdgeSlope")) {
        layer.sweetFxCartoonEdgeSlope = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_cartoon_edge_slope")) {
        layer.sweetFxCartoonEdgeSlope = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_cartoon_strength")) {
        layer.sweetFxCartoonStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeSweetFxTonemap(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Gamma")) {
        layer.sweetFxTonemapGamma = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_tonemap_gamma")) {
        layer.sweetFxTonemapGamma = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Exposure")) {
        layer.sweetFxTonemapExposure = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_tonemap_exposure")) {
        layer.sweetFxTonemapExposure = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Saturation")) {
        layer.sweetFxTonemapSaturation = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_tonemap_saturation")) {
        layer.sweetFxTonemapSaturation = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Bleach")) {
        layer.sweetFxTonemapBleach = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_tonemap_bleach")) {
        layer.sweetFxTonemapBleach = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Defog")) {
        layer.sweetFxTonemapDefog = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_tonemap_defog")) {
        layer.sweetFxTonemapDefog = static_cast<float>(*v);
    }
    if (const auto fogRgbA = TryParseJsonVec3(p, "FogColor")) {
        layer.sweetFxTonemapFogColor = *fogRgbA;
    } else if (const auto fogRgbB = TryParseJsonVec3(p, "sweet_fx_tonemap_fog_color")) {
        layer.sweetFxTonemapFogColor = *fogRgbB;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_tonemap_strength")) {
        layer.sweetFxTonemapStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeSweetFxSplitscreen(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "Mode")) {
        layer.sweetFxSplitscreenMode = std::clamp(static_cast<int>(std::lround(*pr)), 0, 6);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "splitscreen_mode")) {
        layer.sweetFxSplitscreenMode = std::clamp(static_cast<int>(std::lround(*pr)), 0, 6);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "sweet_fx_splitscreen_mode")) {
        layer.sweetFxSplitscreenMode = std::clamp(static_cast<int>(std::lround(*pr)), 0, 6);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_splitscreen_strength")) {
        layer.sweetFxSplitscreenStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeSweetFxNostalgia(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "Nostalgia_palette")) {
        layer.sweetFxNostalgiaPalette = std::clamp(static_cast<int>(std::lround(*pr)), 0, 14);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "sweet_fx_nostalgia_palette")) {
        layer.sweetFxNostalgiaPalette = std::clamp(static_cast<int>(std::lround(*pr)), 0, 14);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "Nostalgia_scanlines")) {
        layer.sweetFxNostalgiaScanlines = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "sweet_fx_nostalgia_scanlines")) {
        layer.sweetFxNostalgiaScanlines = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Nostalgia_dither")) {
        layer.sweetFxNostalgiaDither = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_nostalgia_dither")) {
        layer.sweetFxNostalgiaDither = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_nostalgia_strength")) {
        layer.sweetFxNostalgiaStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeSweetFxCompare(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "compare_mode")) {
        layer.sweetFxCompareMode = std::clamp(static_cast<int>(std::lround(*pr)), 0, 8);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "Mode")) {
        layer.sweetFxCompareMode = std::clamp(static_cast<int>(std::lround(*pr)), 0, 8);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "sweet_fx_compare_mode")) {
        layer.sweetFxCompareMode = std::clamp(static_cast<int>(std::lround(*pr)), 0, 8);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "difference_scale")) {
        layer.sweetFxCompareDifferenceScale = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "DifferenceScale")) {
        layer.sweetFxCompareDifferenceScale = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_compare_difference_scale")) {
        layer.sweetFxCompareDifferenceScale = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_compare_strength")) {
        layer.sweetFxCompareStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeSweetFxLayer(std::string_view p, PostProcessParameters& layer) {
    if (const auto pos = TryParseJsonVec2(p, "Layer_Pos")) {
        layer.sweetFxLayerPosition.x = pos->x;
        layer.sweetFxLayerPosition.y = pos->y;
    } else if (const auto pos = TryParseJsonVec2(p, "sweet_fx_layer_position")) {
        layer.sweetFxLayerPosition.x = pos->x;
        layer.sweetFxLayerPosition.y = pos->y;
    } else {
        if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Layer_PosX")) {
            layer.sweetFxLayerPosition.x = static_cast<float>(*v);
        } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_layer_pos_x")) {
            layer.sweetFxLayerPosition.x = static_cast<float>(*v);
        }
        if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Layer_PosY")) {
            layer.sweetFxLayerPosition.y = static_cast<float>(*v);
        } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_layer_pos_y")) {
            layer.sweetFxLayerPosition.y = static_cast<float>(*v);
        }
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Layer_Scale")) {
        layer.sweetFxLayerScale = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_layer_scale")) {
        layer.sweetFxLayerScale = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Layer_Blend")) {
        layer.sweetFxLayerBlend = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_layer_blend")) {
        layer.sweetFxLayerBlend = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "LAYER_SIZE_X")) {
        layer.sweetFxLayerTexWidth = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_layer_tex_width")) {
        layer.sweetFxLayerTexWidth = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "LAYER_SIZE_Y")) {
        layer.sweetFxLayerTexHeight = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_layer_tex_height")) {
        layer.sweetFxLayerTexHeight = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeSweetFxFxaa(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Subpix")) {
        layer.sweetFxFxaaSubpix = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "subpix")) {
        layer.sweetFxFxaaSubpix = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_fxaa_subpix")) {
        layer.sweetFxFxaaSubpix = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "EdgeThreshold")) {
        layer.sweetFxFxaaEdgeThreshold = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "edge_threshold")) {
        layer.sweetFxFxaaEdgeThreshold = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_fxaa_edge_threshold")) {
        layer.sweetFxFxaaEdgeThreshold = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "EdgeThresholdMin")) {
        layer.sweetFxFxaaEdgeThresholdMin = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "edge_threshold_min")) {
        layer.sweetFxFxaaEdgeThresholdMin = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_fxaa_edge_threshold_min")) {
        layer.sweetFxFxaaEdgeThresholdMin = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_fxaa_strength")) {
        layer.sweetFxFxaaStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeSweetFxCrt(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Amount")) {
        layer.sweetFxCrtAmount = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_crt_amount")) {
        layer.sweetFxCrtAmount = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Resolution")) {
        layer.sweetFxCrtResolution = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_crt_resolution")) {
        layer.sweetFxCrtResolution = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Gamma")) {
        layer.sweetFxCrtGamma = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_crt_gamma")) {
        layer.sweetFxCrtGamma = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "MonitorGamma")) {
        layer.sweetFxCrtMonitorGamma = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_crt_monitor_gamma")) {
        layer.sweetFxCrtMonitorGamma = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Brightness")) {
        layer.sweetFxCrtBrightness = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_crt_brightness")) {
        layer.sweetFxCrtBrightness = static_cast<float>(*v);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "ScanlineIntensity")) {
        layer.sweetFxCrtScanlineIntensity = std::clamp(static_cast<int>(std::lround(*pr)), 2, 4);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "sweet_fx_crt_scanline_intensity")) {
        layer.sweetFxCrtScanlineIntensity = std::clamp(static_cast<int>(std::lround(*pr)), 2, 4);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "ScanlineGaussian")) {
        layer.sweetFxCrtScanlineGaussian = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_crt_scanline_gaussian")) {
        layer.sweetFxCrtScanlineGaussian = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Curvature")) {
        layer.sweetFxCrtCurvature = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_crt_curvature")) {
        layer.sweetFxCrtCurvature = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "CurvatureRadius")) {
        layer.sweetFxCrtCurvatureRadius = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_crt_curvature_radius")) {
        layer.sweetFxCrtCurvatureRadius = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "CornerSize")) {
        layer.sweetFxCrtCornerSize = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_crt_corner_size")) {
        layer.sweetFxCrtCornerSize = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "ViewerDistance")) {
        layer.sweetFxCrtViewerDistance = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_crt_viewer_distance")) {
        layer.sweetFxCrtViewerDistance = static_cast<float>(*v);
    }
    if (const auto a = TryParseJsonVec2(p, "Angle")) {
        layer.sweetFxCrtAngle = *a;
    } else if (const auto a = TryParseJsonVec2(p, "sweet_fx_crt_angle")) {
        layer.sweetFxCrtAngle = *a;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Overscan")) {
        layer.sweetFxCrtOverscan = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_crt_overscan")) {
        layer.sweetFxCrtOverscan = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Oversample")) {
        layer.sweetFxCrtOversample = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_crt_oversample")) {
        layer.sweetFxCrtOversample = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeSweetFxAscii(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "Ascii_spacing")) {
        layer.sweetFxAsciiSpacing = std::clamp(static_cast<int>(std::lround(*pr)), 0, 5);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "sweet_fx_ascii_spacing")) {
        layer.sweetFxAsciiSpacing = std::clamp(static_cast<int>(std::lround(*pr)), 0, 5);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "Ascii_font")) {
        layer.sweetFxAsciiFont = std::clamp(static_cast<int>(std::lround(*pr)), 0, 1);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "sweet_fx_ascii_font")) {
        layer.sweetFxAsciiFont = std::clamp(static_cast<int>(std::lround(*pr)), 0, 1);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "Ascii_font_color_mode")) {
        layer.sweetFxAsciiFontColorMode = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "sweet_fx_ascii_font_color_mode")) {
        layer.sweetFxAsciiFontColorMode = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    }
    if (const auto v = TryParseJsonVec3(p, "Ascii_font_color")) {
        layer.sweetFxAsciiFontColor = *v;
    } else if (const auto v = TryParseJsonVec3(p, "sweet_fx_ascii_font_color")) {
        layer.sweetFxAsciiFontColor = *v;
    }
    if (const auto v = TryParseJsonVec3(p, "Ascii_background_color")) {
        layer.sweetFxAsciiBackgroundColor = *v;
    } else if (const auto v = TryParseJsonVec3(p, "sweet_fx_ascii_background_color")) {
        layer.sweetFxAsciiBackgroundColor = *v;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Ascii_swap_colors")) {
        layer.sweetFxAsciiSwapColors = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_ascii_swap_colors")) {
        layer.sweetFxAsciiSwapColors = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Ascii_invert_brightness")) {
        layer.sweetFxAsciiInvertBrightness = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_ascii_invert_brightness")) {
        layer.sweetFxAsciiInvertBrightness = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Ascii_dithering")) {
        layer.sweetFxAsciiDithering = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_ascii_dithering")) {
        layer.sweetFxAsciiDithering = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Ascii_dithering_intensity")) {
        layer.sweetFxAsciiDitheringIntensity = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_ascii_dithering_intensity")) {
        layer.sweetFxAsciiDitheringIntensity = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Ascii_dithering_debug_gradient")) {
        layer.sweetFxAsciiDitheringDebugGradient = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_ascii_dithering_debug_gradient")) {
        layer.sweetFxAsciiDitheringDebugGradient = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_ascii_strength")) {
        layer.sweetFxAsciiStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeSweetFxSmaa(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "EdgeDetectionType")) {
        layer.sweetFxSmaaEdgeDetectionType = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "sweet_fx_smaa_edge_detection_type")) {
        layer.sweetFxSmaaEdgeDetectionType = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "EdgeDetectionThreshold")) {
        layer.sweetFxSmaaEdgeThreshold = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_smaa_edge_threshold")) {
        layer.sweetFxSmaaEdgeThreshold = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "DepthEdgeDetectionThreshold")) {
        layer.sweetFxSmaaDepthThreshold = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_smaa_depth_threshold")) {
        layer.sweetFxSmaaDepthThreshold = static_cast<float>(*v);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "MaxSearchSteps")) {
        layer.sweetFxSmaaMaxSearchSteps = std::clamp(static_cast<int>(std::lround(*pr)), 0, 112);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "sweet_fx_smaa_max_search_steps")) {
        layer.sweetFxSmaaMaxSearchSteps = std::clamp(static_cast<int>(std::lround(*pr)), 0, 112);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "MaxSearchStepsDiagonal")) {
        layer.sweetFxSmaaMaxSearchStepsDiagonal = std::clamp(static_cast<int>(std::lround(*pr)), 0, 20);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "sweet_fx_smaa_max_search_steps_diagonal")) {
        layer.sweetFxSmaaMaxSearchStepsDiagonal = std::clamp(static_cast<int>(std::lround(*pr)), 0, 20);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "CornerRounding")) {
        layer.sweetFxSmaaCornerRounding = std::clamp(static_cast<int>(std::lround(*pr)), 0, 100);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "sweet_fx_smaa_corner_rounding")) {
        layer.sweetFxSmaaCornerRounding = std::clamp(static_cast<int>(std::lround(*pr)), 0, 100);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "DebugOutput")) {
        layer.sweetFxSmaaDebugOutput = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_smaa_debug_output")) {
        layer.sweetFxSmaaDebugOutput = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_smaa_strength")) {
        layer.sweetFxSmaaStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeReShadeDaltonize(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "Type")) {
        layer.reshadeDaltonizeType = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "reshade_daltonize_type")) {
        layer.reshadeDaltonizeType = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "reshade_daltonize_strength")) {
        layer.reshadeDaltonizeStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeReShadeDisplayDepth(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "PresentType")) {
        layer.reshadeDisplayDepthPresentType = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "reshade_display_depth_present_type")) {
        layer.reshadeDisplayDepthPresentType = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "reshade_display_depth_strength")) {
        layer.reshadeDisplayDepthStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeReShadeLut(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fLUT_AmountChroma")) {
        layer.reshadeLutAmountChroma = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "reshade_lut_amount_chroma")) {
        layer.reshadeLutAmountChroma = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fLUT_AmountLuma")) {
        layer.reshadeLutAmountLuma = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "reshade_lut_amount_luma")) {
        layer.reshadeLutAmountLuma = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "reshade_lut_strength")) {
        layer.reshadeLutStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeSweetFxBorder(std::string_view p, PostProcessParameters& layer) {
    if (const auto bw = TryParseJsonVec2(p, "border_width")) {
        layer.sweetFxBorderWidthX = bw->x;
        layer.sweetFxBorderWidthY = bw->y;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "border_ratio")) {
        layer.sweetFxBorderRatio = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_border_ratio")) {
        layer.sweetFxBorderRatio = static_cast<float>(*v);
    }
    if (const auto borderRgb = TryParseJsonVec3(p, "border_color")) {
        layer.sweetFxBorderColor = *borderRgb;
    } else if (const auto borderRgbSnake = TryParseJsonVec3(p, "sweet_fx_border_color")) {
        layer.sweetFxBorderColor = *borderRgbSnake;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_border_strength")) {
        layer.sweetFxBorderStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeSweetFxChromaticAberration(std::string_view p, PostProcessParameters& layer) {
    if (const auto shiftPair = TryParseJsonVec2(p, "Shift")) {
        layer.sweetFxChromaticAberrationShiftX = shiftPair->x;
        layer.sweetFxChromaticAberrationShiftY = shiftPair->y;
    } else if (const auto shiftLower = TryParseJsonVec2(p, "shift")) {
        layer.sweetFxChromaticAberrationShiftX = shiftLower->x;
        layer.sweetFxChromaticAberrationShiftY = shiftLower->y;
    } else {
        if (const std::optional<double> v = detail::ExtractJsonDouble(p, "ShiftX")) {
            layer.sweetFxChromaticAberrationShiftX = static_cast<float>(*v);
        } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_chromatic_aberration_shift_x")) {
            layer.sweetFxChromaticAberrationShiftX = static_cast<float>(*v);
        }
        if (const std::optional<double> v = detail::ExtractJsonDouble(p, "ShiftY")) {
            layer.sweetFxChromaticAberrationShiftY = static_cast<float>(*v);
        } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_chromatic_aberration_shift_y")) {
            layer.sweetFxChromaticAberrationShiftY = static_cast<float>(*v);
        }
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Strength")) {
        layer.sweetFxChromaticAberrationStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_chromatic_aberration_strength")) {
        layer.sweetFxChromaticAberrationStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeSweetFxCurves(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "Contrast")) {
        layer.sweetFxCurvesContrast = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_curves_contrast")) {
        layer.sweetFxCurvesContrast = static_cast<float>(*v);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "Mode")) {
        layer.sweetFxCurvesMode = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "sweet_fx_curves_mode")) {
        layer.sweetFxCurvesMode = std::clamp(static_cast<int>(std::lround(*pr)), 0, 2);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "Formula")) {
        layer.sweetFxCurvesFormula = std::clamp(static_cast<int>(std::lround(*pr)), 0, 10);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "sweet_fx_curves_formula")) {
        layer.sweetFxCurvesFormula = std::clamp(static_cast<int>(std::lround(*pr)), 0, 10);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sweet_fx_curves_strength")) {
        layer.sweetFxCurvesStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeLumaSharpen(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sharp_strength")) {
        layer.lumaSharpenStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "luma_sharpen_strength")) {
        layer.lumaSharpenStrength = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "sharp_clamp")) {
        layer.lumaSharpenClamp = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "luma_sharpen_clamp")) {
        layer.lumaSharpenClamp = static_cast<float>(*v);
    }
    if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "pattern")) {
        layer.lumaSharpenPattern = std::clamp(static_cast<int>(std::lround(*pr)), 0, 3);
    } else if (const std::optional<double> pr = detail::ExtractJsonDouble(p, "luma_sharpen_pattern")) {
        layer.lumaSharpenPattern = std::clamp(static_cast<int>(std::lround(*pr)), 0, 3);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "offset_bias")) {
        layer.lumaSharpenOffsetBias = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "luma_sharpen_offset_bias")) {
        layer.lumaSharpenOffsetBias = static_cast<float>(*v);
    }
    if (const std::optional<bool> v = detail::ExtractJsonBool(p, "show_sharpen")) {
        layer.lumaSharpenShowPattern = *v ? 1.0f : 0.0f;
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "luma_sharpen_show_pattern")) {
        layer.lumaSharpenShowPattern = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeFakeHdr(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "hdr_power")) {
        layer.fakeHdrPower = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "HDRPower")) {
        layer.fakeHdrPower = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fake_hdr_power")) {
        layer.fakeHdrPower = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "radius1")) {
        layer.fakeHdrRadius1 = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fake_hdr_radius1")) {
        layer.fakeHdrRadius1 = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "radius2")) {
        layer.fakeHdrRadius2 = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fake_hdr_radius2")) {
        layer.fakeHdrRadius2 = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "strength")) {
        layer.fakeHdrStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "fake_hdr_strength")) {
        layer.fakeHdrStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeColorMatrix(std::string_view p, PostProcessParameters& layer) {
    if (const auto v = TryParseJsonVec3(p, "matrix_red")) {
        layer.colorMatrixRed = *v;
    } else if (const auto v = TryParseJsonVec3(p, "color_matrix_red")) {
        layer.colorMatrixRed = *v;
    } else if (const auto v = TryParseJsonVec3(p, "ColorMatrix_Red")) {
        layer.colorMatrixRed = *v;
    }
    if (const auto v = TryParseJsonVec3(p, "matrix_green")) {
        layer.colorMatrixGreen = *v;
    } else if (const auto v = TryParseJsonVec3(p, "color_matrix_green")) {
        layer.colorMatrixGreen = *v;
    } else if (const auto v = TryParseJsonVec3(p, "ColorMatrix_Green")) {
        layer.colorMatrixGreen = *v;
    }
    if (const auto v = TryParseJsonVec3(p, "matrix_blue")) {
        layer.colorMatrixBlue = *v;
    } else if (const auto v = TryParseJsonVec3(p, "color_matrix_blue")) {
        layer.colorMatrixBlue = *v;
    } else if (const auto v = TryParseJsonVec3(p, "ColorMatrix_Blue")) {
        layer.colorMatrixBlue = *v;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "strength")) {
        layer.colorMatrixStrength = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "color_matrix_strength")) {
        layer.colorMatrixStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeDpx(std::string_view p, PostProcessParameters& layer) {
    if (const auto v = TryParseJsonVec3(p, "rgb_curve")) {
        layer.dpxRgbCurve = *v;
    } else if (const auto v = TryParseJsonVec3(p, "dpx_rgb_curve")) {
        layer.dpxRgbCurve = *v;
    }
    if (const auto v = TryParseJsonVec3(p, "rgb_c")) {
        layer.dpxRgbC = *v;
    } else if (const auto v = TryParseJsonVec3(p, "dpx_rgb_c")) {
        layer.dpxRgbC = *v;
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "contrast")) {
        layer.dpxContrast = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "dpx_saturation")) {
        layer.dpxSaturation = static_cast<float>(*v);
    } else if (const std::optional<double> v = detail::ExtractJsonDouble(p, "saturation")) {
        layer.dpxSaturation = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "colorfulness")) {
        layer.dpxColorfulness = static_cast<float>(*v);
    }
    if (const std::optional<double> v = detail::ExtractJsonDouble(p, "strength")) {
        layer.dpxStrength = static_cast<float>(*v);
    }
    MergePresentationObject(p, layer);
}

void MergeGrade(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> tintStr = detail::ExtractJsonDouble(p, "tint_strength")) {
        layer.tintStrength = static_cast<float>(*tintStr);
    }
    if (const std::optional<double> curve = detail::ExtractJsonDouble(p, "curve")) {
        layer.toneCurveStrength = static_cast<float>(*curve);
    } else if (const std::optional<double> toneCurve = detail::ExtractJsonDouble(p, "tone_curve")) {
        layer.toneCurveStrength = static_cast<float>(*toneCurve);
    }
    if (const std::optional<double> dither = detail::ExtractJsonDouble(p, "dither")) {
        layer.outputDitherStrength = static_cast<float>(*dither);
    }
    MergePresentationObject(p, layer);
}

void MergeFilm(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> grain = detail::ExtractJsonDouble(p, "grain")) {
        layer.filmGrainIntensity = static_cast<float>(*grain);
    } else if (const std::optional<double> intensity = detail::ExtractJsonDouble(p, "intensity")) {
        layer.filmGrainIntensity = static_cast<float>(*intensity);
    }
    if (const std::optional<double> noise = detail::ExtractJsonDouble(p, "noise")) {
        layer.noiseAmount = static_cast<float>(*noise);
    }
    if (const std::optional<double> scanlines = detail::ExtractJsonDouble(p, "scanlines")) {
        layer.scanlineAmount = static_cast<float>(*scanlines);
    } else if (const std::optional<double> scanAmt = detail::ExtractJsonDouble(p, "scanline_amount")) {
        layer.scanlineAmount = static_cast<float>(*scanAmt);
    }
    if (const std::optional<double> chromatic = detail::ExtractJsonDouble(p, "chromatic")) {
        layer.chromaticAberration = static_cast<float>(*chromatic);
    }
    if (const std::optional<double> aberration = detail::ExtractJsonDouble(p, "aberration")) {
        layer.chromaticAberration = static_cast<float>(*aberration);
    }
    if (const std::optional<double> staticAmt = detail::ExtractJsonDouble(p, "static")) {
        layer.staticFadeAmount = static_cast<float>(*staticAmt);
    } else if (const std::optional<double> staticFade = detail::ExtractJsonDouble(p, "static_fade")) {
        layer.staticFadeAmount = static_cast<float>(*staticFade);
    }
    if (const std::optional<double> blur = detail::ExtractJsonDouble(p, "blur")) {
        layer.blurAmount = static_cast<float>(*blur);
    }
    MergePresentationObject(p, layer);
}

void MergeOutput(std::string_view p, PostProcessParameters& layer) {
    if (const std::optional<double> dither = detail::ExtractJsonDouble(p, "dither")) {
        layer.outputDitherStrength = static_cast<float>(*dither);
    }
    MergePresentationObject(p, layer);
}

bool ApplyEffectBlock(std::string_view effect, PostProcessParameters& accum) {
    const std::optional<bool> enabled = detail::ExtractJsonBool(effect, "enabled");
    if (enabled.has_value() && !*enabled) {
        return true;
    }

    const std::optional<std::string> typeFromType = detail::ExtractJsonString(effect, "type");
    const std::optional<std::string> typeFromId = detail::ExtractJsonString(effect, "id");
    const std::string typeNorm = NormalizeToken(typeFromType.has_value() ? *typeFromType
            : typeFromId.has_value()                          ? *typeFromId
                                                              : "");
    if (typeNorm.empty()) {
        ri::core::LogInfo("shader.cfg: effect entry missing id/type; skipped.");
        return true;
    }

    if (const std::optional<std::string> mathTag = detail::ExtractJsonString(effect, "math")) {
        if (!mathTag->empty()) {
            std::string suffix;
            if (typeNorm == "sepia") {
                suffix = " (SweetFX Sepia.fx Tint: col*(tint*2.55), not global tint/grade).";
            } else if (typeNorm == "monochrome") {
                suffix = " (SweetFX Monochrome.fx v1.1 dot + mix toward color).";
            } else if (typeNorm == "dpx") {
                suffix = " (SweetFX DPX.fx Loadus sigmoid + XYZ/RGB matrices).";
            } else if (typeNorm == "color_matrix") {
                suffix = " (SweetFX ColorMatrix.fx v1.0 row-wise lerp toward M*c).";
            } else if (typeNorm == "fake_hdr" || typeNorm == "sweetfx_fake_hdr" || typeNorm == "fakehdr") {
                suffix = " (SweetFX FakeHDR.fx — neighbor rings + pow(|blend|,|HDRPower|)+HDR).";
            } else if (typeNorm == "levels" || typeNorm == "sweetfx_levels") {
                suffix = " (SweetFX Levels.fx v1.2 — black/white linear stretch + optional clip overlay).";
            } else if (typeNorm == "luma_sharpen" || typeNorm == "sweetfx_luma_sharpen") {
                suffix = " (SweetFX LumaSharpen.fx v1.5 — luma unsharp + clamp; separate from CAS).";
            } else if (typeNorm == "sweetfx_curves") {
                suffix = " (SweetFX Curves.fx — 11 S-curve formulas + luma/chroma/both; not built-in tone curve).";
            } else if (typeNorm == "sweetfx_chromatic_aberration" || typeNorm == "sweet_fx_chromatic_aberration") {
                suffix = " (SweetFX ChromaticAberration.fx — RGB channel shift in pixels; distinct from radial chromatic_aberration).";
            } else if (typeNorm == "sweetfx_border" || typeNorm == "sweet_fx_border") {
                suffix = " (SweetFX Border.fx — letterbox/pillarbox from ratio or pixel widths).";
            } else if (typeNorm == "sweetfx_cartoon" || typeNorm == "sweet_fx_cartoon") {
                suffix = " (SweetFX Cartoon.fx — diagonal luma edge shrink).";
            } else if (typeNorm == "sweetfx_tonemap" || typeNorm == "sweet_fx_tonemap") {
                suffix = " (SweetFX Tonemap.fx v1.1 — defog/exposure/gamma/bleach/saturation).";
            } else if (typeNorm == "sweetfx_splitscreen" || typeNorm == "sweet_fx_splitscreen") {
                suffix = " (SweetFX Splitscreen.fx v2.0 — mode-driven pre/post region compare).";
            } else if (typeNorm == "sweetfx_nostalgia" || typeNorm == "sweet_fx_nostalgia") {
                suffix = " (SweetFX Nostalgia.fx v1.3 — nearest palette + dither + scanline variants).";
            } else if (typeNorm == "sweetfx_compare" || typeNorm == "sweet_fx_compare") {
                suffix = " (SweetFX Compare.fx v1.0 — split modes + abs/signed difference blend).";
            } else if (typeNorm == "sweetfx_layer" || typeNorm == "sweet_fx_layer") {
                suffix = " (SweetFX Layer.fx v0.2 — BUFFER_SCREEN_SIZE / (LAYER_SIZE*Scale) UV + BORDER blend).";
            } else if (typeNorm == "sweetfx_fxaa" || typeNorm == "sweet_fx_fxaa") {
                suffix = " (SweetFX FXAA 3.11 — luma edge search + subpixel blend; not CAS/sharpen).";
            } else if (typeNorm == "sweetfx_crt" || typeNorm == "sweet_fx_crt") {
                suffix = " (SweetFX CRT.fx — cgwg beam profile + curvature transform + dot-mask).";
            } else if (typeNorm == "sweetfx_ascii" || typeNorm == "sweet_fx_ascii") {
                suffix = " (SweetFX ASCII.fx — quantized luma to bitfield glyphs + optional dither/color modes).";
            } else if (typeNorm == "sweetfx_smaa" || typeNorm == "sweet_fx_smaa") {
                suffix = " (SweetFX SMAA.fx — morphological edge map + directional blend weights).";
            } else if (typeNorm == "reshade_daltonize" || typeNorm == "daltonize") {
                suffix = " (ReShade Daltonize.fx — LMS color deficiency simulation + compensation transform).";
            } else if (typeNorm == "reshade_display_depth" || typeNorm == "display_depth") {
                suffix = " (ReShade DisplayDepth.fx — depth map / normal map visualization modes).";
            } else if (typeNorm == "reshade_lut" || typeNorm == "lut") {
                suffix = " (ReShade LUT.fx — strip LUT remap with chroma/luma controls).";
            } else if (typeNorm == "pd80_technicolor" || typeNorm == "prod80_technicolor" || typeNorm == "pd80_04_technicolor") {
                suffix = " (PD80_04_Technicolor.fx — 2-strip dye + quaternion hue + optional 3-strip).";
            } else if (typeNorm == "pd80_color_temperature" || typeNorm == "prod80_color_temperature"
                || typeNorm == "pd80_04_color_temperature") {
                suffix = " (PD80_04_Color_Temperature.fx — KelvinToRGB * kMix + HSL luminance restore).";
            } else if (typeNorm == "pd80_saturation_limit" || typeNorm == "prod80_saturation_limiter"
                || typeNorm == "pd80_04_saturation_limiter") {
                suffix = " (PD80_04_Saturation_Limit.fx — min(HSL saturation, saturation_limit)).";
            } else if (typeNorm == "pd80_color_balance" || typeNorm == "prod80_color_balance"
                || typeNorm == "pd80_04_color_balance") {
                suffix = " (PD80_04_Color_Balance.fx — tonal RGB curves + optional luma preservation).";
            } else if (typeNorm == "pd80_color_isolation" || typeNorm == "prod80_color_isolation"
                || typeNorm == "pd80_04_color_isolation") {
                suffix = " (PD80_04_Color_Isolation.fx — HSV hue band + smootherstep + Rec.709 luma).";
            } else if (typeNorm == "pd80_levels" || typeNorm == "prod80_levels" || typeNorm == "pd80_03_levels") {
                suffix = " (PD80_03_Levels.fx — expand/compress + gamma; procedural dither vs RGB noise tex).";
            } else if (typeNorm == "pd80_black_white" || typeNorm == "prod80_black_white"
                || typeNorm == "pd80_04_black_white") {
                suffix = " (PD80_04_BlacknWhite.fx — HSL ProcessBW + iq curve + tint/clipping).";
            } else if (typeNorm == "pd80_contrast_brightness_saturation"
                || typeNorm == "prod80_04_contrastbrightnesssaturation" || typeNorm == "pd80_04_cbs") {
                suffix = " (PD80_04_Contrast_Brightness_Saturation.fx — prod80 base ops + selective sat + depth).";
            } else if (typeNorm == "pd80_chromatic_aberration" || typeNorm == "prod80_06_chromaticaberration"
                || typeNorm == "pd80_06_ca") {
                suffix = " (PD80_06_Chromatic_Aberration.fx — hue-ring offsets; core-chain taps only).";
            } else if (typeNorm == "pd80_luma_sharpen" || typeNorm == "prod80_05_lumasharpen"
                || typeNorm == "pd80_05_sharpening" || typeNorm == "pd80_05_sharpen") {
                suffix = " (PD80_05_Sharpening.fx — separable Gaussian + luma screen; grade-chain taps only).";
            } else if (typeNorm == "pd80_film_grain" || typeNorm == "prod80_06_filmgrain"
                || typeNorm == "pd80_06_film_grain") {
                suffix = " (PD80_06_Film_Grain.fx — simplex grain + merge; procedural perm lookup).";
            } else if (typeNorm == "pd80_depth_slicer" || typeNorm == "prod80_06_depthslicer"
                || typeNorm == "prod80_06_depth_slicer" || typeNorm == "pd80_06_depth_slicer") {
                suffix = " (PD80_06_Depth_Slicer.fx — depth band + HSV tint + PD80 blend modes).";
            } else if (typeNorm == "pd80_color_gamut" || typeNorm == "prod80_01_colorgamut"
                || typeNorm == "prod80_01_color_gamut" || typeNorm == "pd80_01_color_gamut") {
                suffix = " (PD80_01_Color_Gamut.fx — sRGB↔XYZ + selectable output primaries / white).";
            } else if (typeNorm == "pd80_color_space_curves" || typeNorm == "prod80_03_colorspacecurves"
                || typeNorm == "pd80_03_color_space_curves" || typeNorm == "prod80_03_color_space_curves") {
                suffix = " (PD80_03_Color_Space_Curves.fx — hyperbolic tonemap + RGB/LAB/HSL/HSV paths).";
            } else if (typeNorm == "pd80_curved_levels" || typeNorm == "prod80_03_curvedlevels" || typeNorm == "pd80_03_curved_levels"
                || typeNorm == "prod80_03_curved_levels") {
                suffix = " (PD80_03_Curved_Levels.fx — ishiyama hyperbolic curve + black/white IN/OUT; optional RGB channels).";
            } else if (typeNorm == "pd80_selective_color" || typeNorm == "prod80_04_selectivecolor" || typeNorm == "pd80_04_selective_color"
                || typeNorm == "prod80_04_selective_color") {
                suffix =
                    " (PD80_04_Selective_Color.fx — CMYK per range via adjustcolor() with absolute/relative + sat/vib scaling).";
            } else if (typeNorm == "pd80_posterize_pixelate" || typeNorm == "prod80_06_posterizepixelate"
                || typeNorm == "pd80_06_posterize_pixelate" || typeNorm == "prod80_06_posterize_pixelate") {
                suffix =
                    " (PD80_06_Posterize_Pixelate.fx — level quantization + pixel-cell border darkening + optional dither).";
            } else if (typeNorm == "pd80_magical_rectangle" || typeNorm == "prod80_04_magicalrectangle"
                || typeNorm == "pd80_04_magical_rectangle" || typeNorm == "prod80_04_magical_rectangle") {
                suffix =
                    " (PD80_04_Magical_Rectangle.fx — transformed shape mask + depth fade + blend-moded region grading).";
            } else if (typeNorm == "pd80_bonus_lut_pack" || typeNorm == "prod80_02_bonus_lut_pack"
                || typeNorm == "pd80_02_bonus_lut_pack") {
                suffix =
                    " (PD80_02_Bonus_LUT_pack.fx — PD80_LUT_v2 atlas sampling + LAB luma/chroma mix + level shaping).";
            } else if (typeNorm == "pd80_cinetools_lut" || typeNorm == "prod80_02_cinetools_lut"
                || typeNorm == "pd80_02_cinetools_lut") {
                suffix =
                    " (PD80_02_Cinetools_LUT.fx — PD80_LUT_v2 atlas sampling + LAB luma/chroma mix + level shaping).";
            } else if (typeNorm == "pd80_lut_creator" || typeNorm == "prod80_02_lut_creator"
                || typeNorm == "pd80_02_lut_creator") {
                suffix =
                    " (PD80_02_LUT_Creator.fx — neutral LUT overlay region for LUT-baking workflows).";
            } else if (typeNorm == "pd80_luma_fade" || typeNorm == "prod80_06_lumafade"
                || typeNorm == "pd80_06_luma_fade") {
                suffix =
                    " (PD80_06_Luma_Fade.fx — luma-gated blend between pre-chain and post-chain color).";
            } else if (typeNorm == "pd80_color_gradients" || typeNorm == "prod80_04_colorgradient"
                || typeNorm == "pd80_04_color_gradients") {
                suffix =
                    " (PD80_04_Color_Gradients.fx — luma-separated shadow/mid gradients with light/dark scene transition).";
            } else if (typeNorm == "pd80_rt_correct_contrast" || typeNorm == "prod80_01a_rt_correct_contrast"
                || typeNorm == "pd80_01a_rt_correct_contrast") {
                suffix =
                    " (PD80_01A_RT_Correct_Contrast.fx — adaptive local black/white point normalization).";
            } else if (typeNorm == "pd80_rt_correct_color" || typeNorm == "prod80_01b_rt_correct_color"
                || typeNorm == "pd80_01b_rt_correct_color") {
                suffix =
                    " (PD80_01B_RT_Correct_Color.fx — adaptive tint removal from black/mid/white references).";
            } else if (typeNorm == "pd80_filmic_adaptation" || typeNorm == "prod80_03_filmictonemap"
                || typeNorm == "pd80_03_filmic_adaptation") {
                suffix =
                    " (PD80_03_Filmic_Adaptation.fx — uncharted filmic curve with scene-luma toe adaptation).";
            } else if (typeNorm == "pd80_hq_bloom" || typeNorm == "prod80_02_bloom" || typeNorm == "pd80_02_bloom") {
                suffix =
                    " (PD80_02_Bloom.fx — thresholded HQ bloom with gaussian blur and screen blend).";
            } else if (typeNorm == "pd80_selective_color_v2" || typeNorm == "prod80_04_selectivecolor_v2"
                || typeNorm == "pd80_04_selective_color_v2") {
                suffix =
                    " (PD80_04_Selective_Color_v2.fx — alternate selective-color weighting path with tonal shaping).";
            } else if (typeNorm == "pd80_shadows_midtones_highlights"
                || typeNorm == "prod80_03_shadowsmidtoneshighlights"
                || typeNorm == "prod80_03_shadows_midtones_highlights"
                || typeNorm == "pd80_03_shadows_midtones_highlights") {
                suffix =
                    " (PD80_03_Shadows_Midtones_Highlights.fx — luma weights + per-band prod80 base ops + blendmode).";
            }
            ri::core::LogInfo(
                "shader.cfg: effect id/type='" + std::string(typeNorm) + "' math='" + *mathTag + "'." + suffix);
        }
    }

    float layerBlend = 1.0f;
    if (const std::optional<double> b = detail::ExtractJsonDouble(effect, "blend")) {
        layerBlend = static_cast<float>(*b);
    }

    if (typeNorm == "preset") {
        const std::optional<std::string> slug = detail::ExtractJsonString(effect, "preset");
        const std::optional<std::string> name = detail::ExtractJsonString(effect, "name");
        const std::string_view presetToken =
            slug.has_value() ? std::string_view(*slug) : name.has_value() ? std::string_view(*name) : "";
        const std::optional<PostProcessPreset> preset = TryParsePostProcessPreset(presetToken);
        if (preset.has_value()) {
            accum = BlendPostProcessParameters(accum, MakePostProcessPreset(*preset), layerBlend);
        } else {
            ri::core::LogInfo("shader.cfg: unknown preset slug; skipped.");
        }
        return true;
    }

    const std::string_view p = ParamsOrEffect(effect);
    PostProcessParameters layer = accum;

    if (typeNorm == "cas") {
        MergeCas(p, layer);
    } else if (typeNorm == "bloom") {
        MergeBloom(p, layer);
    } else if (typeNorm == "deband") {
        MergeDeband(p, layer);
    } else if (typeNorm == "vignette") {
        MergeVignette(p, layer);
    } else if (typeNorm == "grade") {
        MergeGrade(p, layer);
    } else if (typeNorm == "film") {
        MergeFilm(p, layer);
    } else if (typeNorm == "output") {
        MergeOutput(p, layer);
    } else if (typeNorm == "tone" || typeNorm == "curves") {
        MergeGrade(p, layer);
    } else if (typeNorm == "lift_gamma_gain" || typeNorm == "lgg" || typeNorm == "sweetfx_lift_gamma_gain"
        || typeNorm == "reshade_lift_gamma_gain" || typeNorm == "rgb_lift_gamma_gain") {
        MergeLiftGammaGain(p, layer);
    } else if (typeNorm == "vibrance" || typeNorm == "sweetfx_vibrance" || typeNorm == "reshade_vibrance") {
        MergeVibrance(p, layer);
    } else if (typeNorm == "technicolor") {
        MergeTechnicolor(p, layer);
    } else if (typeNorm == "technicolor2") {
        MergeTechnicolor2(p, layer);
    } else if (typeNorm == "pd80_technicolor" || typeNorm == "prod80_technicolor" || typeNorm == "pd80_04_technicolor") {
        MergePd80Technicolor(p, layer);
    } else if (typeNorm == "pd80_color_temperature" || typeNorm == "prod80_color_temperature"
        || typeNorm == "pd80_04_color_temperature") {
        MergePd80ColorTemperature(p, layer);
    } else if (typeNorm == "pd80_saturation_limit" || typeNorm == "prod80_saturation_limiter"
        || typeNorm == "pd80_04_saturation_limiter") {
        MergePd80SaturationLimit(p, layer);
    } else if (typeNorm == "pd80_color_balance" || typeNorm == "prod80_color_balance"
        || typeNorm == "pd80_04_color_balance") {
        MergePd80ColorBalance(p, layer);
    } else if (typeNorm == "pd80_color_isolation" || typeNorm == "prod80_color_isolation"
        || typeNorm == "pd80_04_color_isolation") {
        MergePd80ColorIsolation(p, layer);
    } else if (typeNorm == "pd80_levels" || typeNorm == "prod80_levels" || typeNorm == "pd80_03_levels") {
        MergePd80Levels(p, layer);
    } else if (typeNorm == "pd80_black_white" || typeNorm == "prod80_black_white"
        || typeNorm == "pd80_04_black_white") {
        MergePd80BlackWhite(p, layer);
    } else if (typeNorm == "pd80_contrast_brightness_saturation"
        || typeNorm == "prod80_04_contrastbrightnesssaturation" || typeNorm == "pd80_04_cbs") {
        MergePd80ContrastBriSat(p, layer);
    } else if (typeNorm == "pd80_luma_sharpen" || typeNorm == "prod80_05_lumasharpen"
        || typeNorm == "pd80_05_sharpening" || typeNorm == "pd80_05_sharpen") {
        MergePd80LumaSharpen(p, layer);
    } else if (typeNorm == "pd80_film_grain" || typeNorm == "prod80_06_filmgrain"
        || typeNorm == "pd80_06_film_grain") {
        MergePd80FilmGrain(p, layer);
    } else if (typeNorm == "pd80_depth_slicer" || typeNorm == "prod80_06_depthslicer"
        || typeNorm == "prod80_06_depth_slicer" || typeNorm == "pd80_06_depth_slicer") {
        MergePd80DepthSlicer(p, layer);
    } else if (typeNorm == "pd80_color_gamut" || typeNorm == "prod80_01_colorgamut"
        || typeNorm == "prod80_01_color_gamut" || typeNorm == "pd80_01_color_gamut") {
        MergePd80ColorGamut(p, layer);
    } else if (typeNorm == "pd80_color_space_curves" || typeNorm == "prod80_03_colorspacecurves"
        || typeNorm == "pd80_03_color_space_curves" || typeNorm == "prod80_03_color_space_curves") {
        MergePd80ColorSpaceCurves(p, layer);
    } else if (typeNorm == "pd80_shadows_midtones_highlights"
        || typeNorm == "prod80_03_shadowsmidtoneshighlights"
        || typeNorm == "prod80_03_shadows_midtones_highlights"
        || typeNorm == "pd80_03_shadows_midtones_highlights") {
        MergePd80ShadowsMidtonesHighlights(p, layer);
    } else if (typeNorm == "pd80_curved_levels" || typeNorm == "prod80_03_curvedlevels" || typeNorm == "pd80_03_curved_levels"
        || typeNorm == "prod80_03_curved_levels") {
        MergePd80CurvedLevels(p, layer);
    } else if (typeNorm == "pd80_selective_color" || typeNorm == "prod80_04_selectivecolor" || typeNorm == "pd80_04_selective_color"
        || typeNorm == "prod80_04_selective_color") {
        MergePd80SelectiveColor(p, layer);
    } else if (typeNorm == "pd80_posterize_pixelate" || typeNorm == "prod80_06_posterizepixelate"
        || typeNorm == "pd80_06_posterize_pixelate" || typeNorm == "prod80_06_posterize_pixelate") {
        MergePd80PosterizePixelate(p, layer);
    } else if (typeNorm == "pd80_magical_rectangle" || typeNorm == "prod80_04_magicalrectangle"
        || typeNorm == "pd80_04_magical_rectangle" || typeNorm == "prod80_04_magical_rectangle") {
        MergePd80MagicalRectangle(p, layer);
    } else if (typeNorm == "pd80_bonus_lut_pack" || typeNorm == "prod80_02_bonus_lut_pack"
        || typeNorm == "pd80_02_bonus_lut_pack") {
        MergePd80BonusLutPack(p, layer);
    } else if (typeNorm == "pd80_cinetools_lut" || typeNorm == "prod80_02_cinetools_lut"
        || typeNorm == "pd80_02_cinetools_lut") {
        MergePd80CinetoolsLut(p, layer);
    } else if (typeNorm == "pd80_lut_creator" || typeNorm == "prod80_02_lut_creator"
        || typeNorm == "pd80_02_lut_creator") {
        MergePd80LutCreator(p, layer);
    } else if (typeNorm == "pd80_luma_fade" || typeNorm == "prod80_06_lumafade"
        || typeNorm == "pd80_06_luma_fade") {
        MergePd80LumaFade(p, layer);
    } else if (typeNorm == "pd80_color_gradients" || typeNorm == "prod80_04_colorgradient"
        || typeNorm == "pd80_04_color_gradients") {
        MergePd80ColorGradients(p, layer);
    } else if (typeNorm == "pd80_rt_correct_contrast" || typeNorm == "prod80_01a_rt_correct_contrast"
        || typeNorm == "pd80_01a_rt_correct_contrast") {
        MergePd80CorrectContrast(p, layer);
    } else if (typeNorm == "pd80_rt_correct_color" || typeNorm == "prod80_01b_rt_correct_color"
        || typeNorm == "pd80_01b_rt_correct_color") {
        MergePd80RtCorrectColor(p, layer);
    } else if (typeNorm == "pd80_filmic_adaptation" || typeNorm == "prod80_03_filmictonemap"
        || typeNorm == "pd80_03_filmic_adaptation") {
        MergePd80FilmicAdaptation(p, layer);
    } else if (typeNorm == "pd80_hq_bloom" || typeNorm == "prod80_02_bloom" || typeNorm == "pd80_02_bloom") {
        MergePd80HqBloom(p, layer);
    } else if (typeNorm == "pd80_selective_color_v2" || typeNorm == "prod80_04_selectivecolor_v2"
        || typeNorm == "pd80_04_selective_color_v2") {
        MergePd80SelectiveColorV2(p, layer);
    } else if (typeNorm == "pd80_chromatic_aberration" || typeNorm == "prod80_06_chromaticaberration"
        || typeNorm == "pd80_06_ca") {
        MergePd80ChromaticAberration(p, layer);
    } else if (typeNorm == "sepia" || typeNorm == "sweetfx_sepia" || typeNorm == "sweetfx_tint") {
        MergeSepia(p, layer);
    } else if (typeNorm == "monochrome" || typeNorm == "sweetfx_monochrome") {
        MergeMonochrome(p, layer);
    } else if (typeNorm == "dpx" || typeNorm == "sweetfx_dpx") {
        MergeDpx(p, layer);
    } else if (typeNorm == "color_matrix" || typeNorm == "sweetfx_color_matrix") {
        MergeColorMatrix(p, layer);
    } else if (typeNorm == "fake_hdr" || typeNorm == "sweetfx_fake_hdr" || typeNorm == "fakehdr") {
        MergeFakeHdr(p, layer);
    } else if (typeNorm == "levels" || typeNorm == "sweetfx_levels") {
        MergeLevels(p, layer);
    } else if (typeNorm == "luma_sharpen" || typeNorm == "sweetfx_luma_sharpen") {
        MergeLumaSharpen(p, layer);
    } else if (typeNorm == "sweetfx_curves") {
        MergeSweetFxCurves(p, layer);
    } else if (typeNorm == "sweetfx_chromatic_aberration" || typeNorm == "sweet_fx_chromatic_aberration") {
        MergeSweetFxChromaticAberration(p, layer);
    } else if (typeNorm == "sweetfx_border" || typeNorm == "sweet_fx_border") {
        MergeSweetFxBorder(p, layer);
    } else if (typeNorm == "sweetfx_cartoon" || typeNorm == "sweet_fx_cartoon") {
        MergeSweetFxCartoon(p, layer);
    } else if (typeNorm == "sweetfx_tonemap" || typeNorm == "sweet_fx_tonemap") {
        MergeSweetFxTonemap(p, layer);
    } else if (typeNorm == "sweetfx_splitscreen" || typeNorm == "sweet_fx_splitscreen") {
        MergeSweetFxSplitscreen(p, layer);
    } else if (typeNorm == "sweetfx_nostalgia" || typeNorm == "sweet_fx_nostalgia") {
        MergeSweetFxNostalgia(p, layer);
    } else if (typeNorm == "sweetfx_compare" || typeNorm == "sweet_fx_compare") {
        MergeSweetFxCompare(p, layer);
    } else if (typeNorm == "sweetfx_layer" || typeNorm == "sweet_fx_layer") {
        MergeSweetFxLayer(p, layer);
    } else if (typeNorm == "sweetfx_fxaa" || typeNorm == "sweet_fx_fxaa") {
        MergeSweetFxFxaa(p, layer);
    } else if (typeNorm == "sweetfx_crt" || typeNorm == "sweet_fx_crt") {
        MergeSweetFxCrt(p, layer);
    } else if (typeNorm == "sweetfx_ascii" || typeNorm == "sweet_fx_ascii") {
        MergeSweetFxAscii(p, layer);
    } else if (typeNorm == "sweetfx_smaa" || typeNorm == "sweet_fx_smaa") {
        MergeSweetFxSmaa(p, layer);
    } else if (typeNorm == "reshade_daltonize" || typeNorm == "daltonize") {
        MergeReShadeDaltonize(p, layer);
    } else if (typeNorm == "reshade_display_depth" || typeNorm == "display_depth") {
        MergeReShadeDisplayDepth(p, layer);
    } else if (typeNorm == "reshade_lut" || typeNorm == "lut") {
        MergeReShadeLut(p, layer);
    } else if (typeNorm == "colourfulness" || typeNorm == "colorfulness" || typeNorm == "sweetfx_colourfulness"
        || typeNorm == "reshade_colourfulness") {
        MergeColourfulness(p, layer);
    } else if (typeNorm == "filmic_pass" || typeNorm == "filmicpass" || typeNorm == "sweetfx_filmic_pass"
        || typeNorm == "reshade_filmic_pass") {
        MergeFilmicPass(p, layer);
    } else if (typeNorm == "film_grain2" || typeNorm == "filmgrain2" || typeNorm == "sweetfx_film_grain2"
        || typeNorm == "reshade_film_grain2") {
        MergeFilmGrain2(p, layer);
    } else if (typeNorm == "denoise" || typeNorm == "sweetfx_denoise" || typeNorm == "reshade_denoise"
        || typeNorm == "nvidia_denoise") {
        MergeDenoise(p, layer);
    } else if (typeNorm == "adaptive_sharpen" || typeNorm == "adaptivesharpen" || typeNorm == "sweetfx_adaptive_sharpen"
        || typeNorm == "reshade_adaptive_sharpen") {
        MergeAdaptiveSharpen(p, layer);
    } else if (typeNorm == "gaussian_blur" || typeNorm == "gaussianblur" || typeNorm == "sweetfx_gaussian_blur"
        || typeNorm == "reshade_gaussian_blur") {
        MergeGaussianBlur(p, layer);
    } else if (typeNorm == "fine_sharp" || typeNorm == "finesharp" || typeNorm == "sweetfx_fine_sharp"
        || typeNorm == "reshade_fine_sharp") {
        MergeFineSharp(p, layer);
    } else if (typeNorm == "marty_bloom" || typeNorm == "reshade_marty_bloom" || typeNorm == "bloom_and_lens"
        || typeNorm == "sweetfx_marty_bloom" || typeNorm == "reshade_bloom_fx") {
        MergeMartyBloom(p, layer);
    } else if (typeNorm == "ring_dof" || typeNorm == "reshade_ring_dof" || typeNorm == "marty_dof"
        || typeNorm == "creator_dof" || typeNorm == "reshade_dof") {
        MergeRingDof(p, layer);
    } else if (typeNorm == "ambient_light" || typeNorm == "reshade_ambient_light"
        || typeNorm == "sweetfx_ambient_light") {
        MergeAmbientLight(p, layer);
    } else if (typeNorm == "fake_motion_blur" || typeNorm == "reshade_fake_motion_blur"
        || typeNorm == "motion_blur_fake") {
        MergeFakeMotionBlur(p, layer);
    } else if (typeNorm == "reflective_bump_mapping" || typeNorm == "reflective_bumpmapping"
        || typeNorm == "reshade_reflective_bump_mapping" || typeNorm == "rbm" || typeNorm == "marty_rbm") {
        MergeReflectiveBumpMapping(p, layer);
    } else if (typeNorm == "crop_resize" || typeNorm == "crop_scale" || typeNorm == "resizer") {
        if (const std::optional<double> v = detail::ExtractJsonDouble(p, "content_width")) {
            layer.cropScaleContentWidth = static_cast<float>(*v);
        }
        if (const std::optional<double> v = detail::ExtractJsonDouble(p, "content_height")) {
            layer.cropScaleContentHeight = static_cast<float>(*v);
        }
        if (const std::optional<double> v = detail::ExtractJsonDouble(p, "intermediate_width")) {
            layer.cropScaleIntermediateWidth = static_cast<float>(*v);
        }
        if (const std::optional<double> v = detail::ExtractJsonDouble(p, "intermediate_height")) {
            layer.cropScaleIntermediateHeight = static_cast<float>(*v);
        }
        if (const std::optional<double> v = detail::ExtractJsonDouble(p, "final_width")) {
            layer.cropScaleFinalWidth = static_cast<float>(*v);
        }
        if (const std::optional<double> v = detail::ExtractJsonDouble(p, "final_height")) {
            layer.cropScaleFinalHeight = static_cast<float>(*v);
        }
        if (const std::optional<std::int32_t> v = detail::ExtractJsonInt(p, "filter")) {
            layer.cropScaleFilter = *v;
        }
        if (const std::optional<double> v = detail::ExtractJsonDouble(p, "strength")) {
            layer.cropScaleStrength = static_cast<float>(*v);
        }
        MergePresentationObject(p, layer);
    } else if (typeNorm == "barbatos_fake_hdr" || typeNorm == "ufakehdr" || typeNorm == "u_fake_hdr") {
        if (const std::optional<std::int32_t> v = detail::ExtractJsonInt(p, "preset")) {
            layer.barbatosFakeHdrPreset = *v;
        }
        if (const std::optional<double> v = detail::ExtractJsonDouble(p, "strength")) {
            layer.barbatosFakeHdrStrength = static_cast<float>(*v);
        }
        MergePresentationObject(p, layer);
    } else if (typeNorm == "ri_adaptive_deband" || typeNorm == "rawiron_adaptive_deband") {
        if (const auto v = detail::ExtractJsonDouble(p, "strength")) layer.riAdaptiveDebandStrength = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonDouble(p, "radius")) layer.riAdaptiveDebandRadius = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonDouble(p, "threshold")) layer.riAdaptiveDebandThreshold = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonInt(p, "iterations")) layer.riAdaptiveDebandIterations = *v;
        MergePresentationObject(p, layer);
    } else if (typeNorm == "ri_local_sharpen" || typeNorm == "rawiron_local_sharpen") {
        if (const auto v = detail::ExtractJsonDouble(p, "strength")) layer.riLocalSharpenStrength = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonDouble(p, "radius")) layer.riLocalSharpenRadius = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonDouble(p, "clamp")) layer.riLocalSharpenClamp = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonDouble(p, "edge_limit")) layer.riLocalSharpenEdgeLimit = static_cast<float>(*v);
        MergePresentationObject(p, layer);
    } else if (typeNorm == "ri_ink_outline" || typeNorm == "rawiron_ink_outline") {
        if (const auto v = detail::ExtractJsonDouble(p, "strength")) layer.riOutlineStrength = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonDouble(p, "thickness")) layer.riOutlineThickness = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonDouble(p, "depth_sensitivity")) layer.riOutlineDepthSensitivity = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonDouble(p, "color_sensitivity")) layer.riOutlineColorSensitivity = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonInt(p, "method")) layer.riOutlineMethod = *v;
        if (const auto v = TryParseJsonVec3(p, "color")) layer.riOutlineColor = *v;
        if (const auto v = detail::ExtractJsonDouble(p, "wobble_amount")) layer.riOutlineWobbleAmount = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonDouble(p, "wobble_speed")) layer.riOutlineWobbleSpeed = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonDouble(p, "wobble_frequency")) layer.riOutlineWobbleFrequency = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonDouble(p, "debug")) layer.riOutlineDebug = static_cast<float>(*v);
        MergePresentationObject(p, layer);
    } else if (typeNorm == "ri_signal_glitch" || typeNorm == "rawiron_signal_glitch") {
        if (const auto v = detail::ExtractJsonDouble(p, "strength")) layer.riSignalGlitchStrength = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonDouble(p, "block_size")) layer.riSignalGlitchBlockSize = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonDouble(p, "color_shift_pixels")) {
            layer.riSignalGlitchColorShiftPixels = static_cast<float>(*v);
        }
        if (const auto v = detail::ExtractJsonDouble(p, "speed")) layer.riSignalGlitchSpeed = static_cast<float>(*v);
        MergePresentationObject(p, layer);
    } else if (typeNorm == "ri_night_vision" || typeNorm == "rawiron_night_vision") {
        if (const auto v = detail::ExtractJsonDouble(p, "strength")) layer.riNightVisionStrength = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonDouble(p, "gain")) layer.riNightVisionGain = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonDouble(p, "noise")) layer.riNightVisionNoise = static_cast<float>(*v);
        if (const auto v = detail::ExtractJsonDouble(p, "vignette")) layer.riNightVisionVignette = static_cast<float>(*v);
        MergePresentationObject(p, layer);
    } else if (typeNorm == "legacy_post" || typeNorm == "stylized" || typeNorm == "vintage") {
        MergePresentationObject(p, layer);
    } else {
        ri::core::LogInfo("shader.cfg: unknown effect id '" + std::string(typeNorm) + "'; skipped.");
        return true;
    }

    accum = BlendPostProcessParameters(accum, layer, ClampUnit(layerBlend));
    return true;
}

[[nodiscard]] bool ParseShaderCfgDocument(std::string_view text, ShaderPresentationConfig* out, std::string* error) {
    if (out == nullptr) {
        if (error != nullptr) {
            *error = "ParseShaderCfgDocument: output pointer was null.";
        }
        return false;
    }
    if (text.find('{') == std::string_view::npos) {
        if (error != nullptr) {
            *error = "shader.cfg: document has no JSON object.";
        }
        return false;
    }

    PostProcessParameters accum{};
    if (const std::optional<std::string_view> post = detail::ExtractJsonObject(text, "post")) {
        MergePresentationObject(*post, accum);
    }

    const std::vector<std::string_view> effects = detail::SplitJsonArrayObjects(text, "effects");
    for (std::string_view block : effects) {
        (void)ApplyEffectBlock(block, accum);
    }

    if (const std::optional<std::string_view> presentation = detail::ExtractJsonObject(text, "presentation")) {
        MergePresentationObject(*presentation, accum);
    }

    ShaderPresentationConfig parsed{};
    parsed.parameters = SanitizePostProcessParameters(accum);
    parsed.loaded = true;

    if (const std::optional<double> bw = detail::ExtractJsonDouble(text, "blendWeight")) {
        parsed.blendWeight = static_cast<float>(*bw);
    }

    if (const std::optional<bool> rep = detail::ExtractJsonBool(text, "replace")) {
        parsed.replace = *rep;
    }

    *out = std::move(parsed);
    return true;
}

} // namespace

std::optional<fs::path> ResolveShaderCfgPath(const fs::path& workspaceOrGameRoot, const fs::path& executableDirectory) {
    std::error_code ec{};
    if (!executableDirectory.empty()) {
        ec.clear();
        const fs::path exeCanon = fs::weakly_canonical(executableDirectory, ec);
        if (!ec) {
            const fs::path nearExe = exeCanon / "shader.cfg";
            ec.clear();
            if (fs::is_regular_file(nearExe, ec)) {
                return nearExe;
            }
        }
    }

    if (workspaceOrGameRoot.empty()) {
        return std::nullopt;
    }
    ec.clear();
    const fs::path root = fs::weakly_canonical(workspaceOrGameRoot, ec);
    if (ec || root.empty()) {
        return std::nullopt;
    }
    const fs::path candidates[] = {
        root / "Assets" / "shader.cfg",
        root / "shader.cfg",
        root / "Content" / "shader.cfg",
    };
    for (const fs::path& candidate : candidates) {
        ec.clear();
        if (fs::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }
    return std::nullopt;
}

bool LoadShaderCfg(const fs::path& path, ShaderPresentationConfig* out, std::string* error) {
    if (out == nullptr) {
        if (error != nullptr) {
            *error = "LoadShaderCfg: output pointer was null.";
        }
        return false;
    }
    std::error_code ec{};
    if (!fs::is_regular_file(path, ec)) {
        if (error != nullptr) {
            *error = "shader.cfg not found: " + path.generic_string();
        }
        return false;
    }
    std::string utf8 = detail::ReadTextFile(path);
    if (utf8.empty()) {
        if (error != nullptr) {
            *error = "shader.cfg empty or unreadable: " + path.generic_string();
        }
        return false;
    }
    utf8 = StripUtf8BomAndLineComments(utf8);
    if (!ParseShaderCfgDocument(utf8, out, error)) {
        return false;
    }
    return true;
}

bool TryLoadShaderCfgFromRoot(const fs::path& workspaceOrGameRoot, ShaderPresentationConfig* out, std::string* error) {
    const std::optional<fs::path> path = ResolveShaderCfgPath(workspaceOrGameRoot, {});
    if (!path.has_value()) {
        return false;
    }
    return LoadShaderCfg(*path, out, error);
}

void ApplyShaderConfig(PostProcessParameters& io, const ShaderPresentationConfig& cfg) {
    if (!cfg.loaded) {
        return;
    }
    const float time = io.timeSeconds;
    if (cfg.replace) {
        io = cfg.parameters;
    } else {
        const float t = std::clamp(cfg.blendWeight, 0.0f, 1.0f);
        io = BlendPostProcessParameters(io, cfg.parameters, t);
    }
    io.timeSeconds = time;
}

} // namespace ri::render
