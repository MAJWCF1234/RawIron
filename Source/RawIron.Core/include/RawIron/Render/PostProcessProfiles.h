#pragma once

#include "RawIron/Math/Vec3.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace ri::render {

enum class PostProcessPreset {
    Neutral,
    CrispGameplay,
    SoftVhs,
    Vhs,
    ColdFacility,
    IndustrialHaze,
    DreamPulse,
    CombatFocus,
    AnalogHorror,
    StaticTransition,
};

struct PostProcessParameters {
    float timeSeconds = 0.0f;
    float noiseAmount = 0.0f;
    float scanlineAmount = 0.0f;
    float barrelDistortion = 0.0f;
    float chromaticAberration = 0.0f;
    ri::math::Vec3 tintColor{1.0f, 1.0f, 1.0f};
    float tintStrength = 0.0f;
    float blurAmount = 0.0f;
    float staticFadeAmount = 0.0f;
};

struct PostProcessPresetDefinition {
    PostProcessPreset preset = PostProcessPreset::Neutral;
    std::string_view slug{};
    std::string_view label{};
    std::string_view summary{};
};

struct PostProcessPresetLayer {
    PostProcessPreset preset = PostProcessPreset::Neutral;
    float blend = 1.0f;
};

inline constexpr std::array<PostProcessPresetDefinition, 10> kPostProcessPresetDefinitions{{
    {PostProcessPreset::Neutral, "neutral", "Neutral", "No post-process shaping; use the scene as-authored."},
    {PostProcessPreset::CrispGameplay, "crisp_gameplay", "Crisp Gameplay", "Clear low-noise presentation tuned for readable play spaces."},
    {PostProcessPreset::SoftVhs, "soft_vhs", "Soft VHS", "Gentle tape-era breakup without heavy blur or distortion."},
    {PostProcessPreset::Vhs, "vhs", "VHS", "Classic low-intensity scanline and chroma split."},
    {PostProcessPreset::ColdFacility, "cold_facility", "Cold Facility", "Sterile cyan-tinted surveillance look for industrial spaces."},
    {PostProcessPreset::IndustrialHaze, "industrial_haze", "Industrial Haze", "Warm haze and soft blur for dense machinery rooms."},
    {PostProcessPreset::DreamPulse, "dream_pulse", "Dream Pulse", "Blurred tinted pulse suited for flashbacks and unstable states."},
    {PostProcessPreset::CombatFocus, "combat_focus", "Combat Focus", "Subtle urgency shaping with restrained aberration and low clutter."},
    {PostProcessPreset::AnalogHorror, "analog_horror", "Analog Horror", "Heavy distortion and scanline breakup for hostile transitions."},
    {PostProcessPreset::StaticTransition, "static_transition", "Static Transition", "Full static fade layer for room cuts and terminal jumps."},
}};

inline std::span<const PostProcessPresetDefinition> GetPostProcessPresetDefinitions() {
    return kPostProcessPresetDefinitions;
}

inline std::string_view ToString(PostProcessPreset preset) {
    for (const PostProcessPresetDefinition& definition : kPostProcessPresetDefinitions) {
        if (definition.preset == preset) {
            return definition.slug;
        }
    }
    return "neutral";
}

inline std::optional<PostProcessPreset> TryParsePostProcessPreset(std::string_view slug) {
    const auto normalize = [](std::string_view value) {
        std::string normalized;
        normalized.reserve(value.size());
        bool wroteSeparator = false;
        for (char ch : value) {
            const unsigned char code = static_cast<unsigned char>(ch);
            if (std::isalnum(code)) {
                normalized.push_back(static_cast<char>(std::tolower(code)));
                wroteSeparator = false;
                continue;
            }
            if (ch == '_' || ch == '-' || std::isspace(code)) {
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
    };

    const std::string normalizedSlug = normalize(slug);
    if (normalizedSlug.empty()) {
        return std::nullopt;
    }
    for (const PostProcessPresetDefinition& definition : kPostProcessPresetDefinitions) {
        if (definition.slug == normalizedSlug) {
            return definition.preset;
        }
    }
    return std::nullopt;
}

inline float ClampUnit(float value) {
    if (!std::isfinite(value)) {
        return 0.0f;
    }
    return std::clamp(value, 0.0f, 1.0f);
}

inline float ClampFinite(float value, float minValue, float maxValue, float fallback) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, minValue, maxValue);
}

inline PostProcessParameters SanitizePostProcessParameters(const PostProcessParameters& input) {
    PostProcessParameters out{};
    const float finiteTime = std::isfinite(input.timeSeconds) ? input.timeSeconds : 0.0f;
    out.timeSeconds = std::fmod(finiteTime, 4096.0f);
    out.noiseAmount = ClampFinite(input.noiseAmount, 0.0f, 0.30f, 0.0f);
    out.scanlineAmount = ClampFinite(input.scanlineAmount, 0.0f, 0.20f, 0.0f);
    out.barrelDistortion = ClampFinite(input.barrelDistortion, 0.0f, 0.20f, 0.0f);
    out.chromaticAberration = ClampFinite(input.chromaticAberration, 0.0f, 0.05f, 0.0f);
    out.tintColor = {
        ClampUnit(input.tintColor.x),
        ClampUnit(input.tintColor.y),
        ClampUnit(input.tintColor.z),
    };
    out.tintStrength = ClampFinite(input.tintStrength, 0.0f, 1.0f, 0.0f);
    out.blurAmount = ClampFinite(input.blurAmount, 0.0f, 0.05f, 0.0f);
    out.staticFadeAmount = ClampFinite(input.staticFadeAmount, 0.0f, 1.0f, 0.0f);
    return out;
}

inline PostProcessParameters MakePostProcessPreset(PostProcessPreset preset) {
    switch (preset) {
        case PostProcessPreset::CrispGameplay:
            return PostProcessParameters{
                .noiseAmount = 0.0015f,
                .scanlineAmount = 0.002f,
                .barrelDistortion = 0.002f,
                .chromaticAberration = 0.00015f,
                .tintColor = {1.0f, 1.0f, 1.0f},
                .tintStrength = 0.0f,
                .blurAmount = 0.0f,
                .staticFadeAmount = 0.0f,
            };
        case PostProcessPreset::SoftVhs:
            return PostProcessParameters{
                .noiseAmount = 0.0025f,
                .scanlineAmount = 0.004f,
                .barrelDistortion = 0.001f,
                .chromaticAberration = 0.00035f,
                .tintColor = {1.0f, 1.0f, 1.0f},
                .tintStrength = 0.0f,
                .blurAmount = 0.0f,
                .staticFadeAmount = 0.0f,
            };
        case PostProcessPreset::Vhs:
            return PostProcessParameters{
                .noiseAmount = 0.004f,
                .scanlineAmount = 0.008f,
                .barrelDistortion = 0.0f,
                .chromaticAberration = 0.0007f,
                .tintColor = {1.0f, 1.0f, 1.0f},
                .tintStrength = 0.0f,
                .blurAmount = 0.0f,
                .staticFadeAmount = 0.0f,
            };
        case PostProcessPreset::AnalogHorror:
            return PostProcessParameters{
                .noiseAmount = 0.03f,
                .scanlineAmount = 0.045f,
                .barrelDistortion = 0.03f,
                .chromaticAberration = 0.0012f,
                .tintColor = {1.0f, 1.0f, 1.0f},
                .tintStrength = 0.0f,
                .blurAmount = 0.0f,
                .staticFadeAmount = 0.0f,
            };
        case PostProcessPreset::ColdFacility:
            return PostProcessParameters{
                .noiseAmount = 0.0035f,
                .scanlineAmount = 0.006f,
                .barrelDistortion = 0.003f,
                .chromaticAberration = 0.00045f,
                .tintColor = {0.72f, 0.88f, 1.0f},
                .tintStrength = 0.16f,
                .blurAmount = 0.0015f,
                .staticFadeAmount = 0.0f,
            };
        case PostProcessPreset::IndustrialHaze:
            return PostProcessParameters{
                .noiseAmount = 0.006f,
                .scanlineAmount = 0.004f,
                .barrelDistortion = 0.005f,
                .chromaticAberration = 0.00035f,
                .tintColor = {1.0f, 0.82f, 0.68f},
                .tintStrength = 0.22f,
                .blurAmount = 0.004f,
                .staticFadeAmount = 0.0f,
            };
        case PostProcessPreset::DreamPulse:
            return PostProcessParameters{
                .noiseAmount = 0.009f,
                .scanlineAmount = 0.0025f,
                .barrelDistortion = 0.008f,
                .chromaticAberration = 0.0016f,
                .tintColor = {0.86f, 0.74f, 1.0f},
                .tintStrength = 0.28f,
                .blurAmount = 0.008f,
                .staticFadeAmount = 0.0f,
            };
        case PostProcessPreset::CombatFocus:
            return PostProcessParameters{
                .noiseAmount = 0.0045f,
                .scanlineAmount = 0.0015f,
                .barrelDistortion = 0.0f,
                .chromaticAberration = 0.00055f,
                .tintColor = {1.0f, 0.86f, 0.82f},
                .tintStrength = 0.12f,
                .blurAmount = 0.0f,
                .staticFadeAmount = 0.0f,
            };
        case PostProcessPreset::StaticTransition:
            return PostProcessParameters{
                .noiseAmount = 0.04f,
                .scanlineAmount = 0.02f,
                .barrelDistortion = 0.0f,
                .chromaticAberration = 0.0006f,
                .tintColor = {1.0f, 1.0f, 1.0f},
                .tintStrength = 0.0f,
                .blurAmount = 0.0f,
                .staticFadeAmount = 1.0f,
            };
        case PostProcessPreset::Neutral:
        default:
            return PostProcessParameters{};
    }
}

inline PostProcessParameters BlendPostProcessParameters(
    const PostProcessParameters& lhs,
    const PostProcessParameters& rhs,
    float alpha) {
    const float t = ClampUnit(alpha);
    const PostProcessParameters a = SanitizePostProcessParameters(lhs);
    const PostProcessParameters b = SanitizePostProcessParameters(rhs);
    return SanitizePostProcessParameters(PostProcessParameters{
        .timeSeconds = a.timeSeconds + ((b.timeSeconds - a.timeSeconds) * t),
        .noiseAmount = a.noiseAmount + ((b.noiseAmount - a.noiseAmount) * t),
        .scanlineAmount = a.scanlineAmount + ((b.scanlineAmount - a.scanlineAmount) * t),
        .barrelDistortion = a.barrelDistortion + ((b.barrelDistortion - a.barrelDistortion) * t),
        .chromaticAberration = a.chromaticAberration + ((b.chromaticAberration - a.chromaticAberration) * t),
        .tintColor = ri::math::Lerp(a.tintColor, b.tintColor, t),
        .tintStrength = a.tintStrength + ((b.tintStrength - a.tintStrength) * t),
        .blurAmount = a.blurAmount + ((b.blurAmount - a.blurAmount) * t),
        .staticFadeAmount = a.staticFadeAmount + ((b.staticFadeAmount - a.staticFadeAmount) * t),
    });
}

inline PostProcessParameters ComposePostProcessPresetStack(
    std::span<const PostProcessPresetLayer> layers,
    const PostProcessParameters& base = {}) {
    PostProcessParameters result = SanitizePostProcessParameters(base);
    for (const PostProcessPresetLayer& layer : layers) {
        const float blend = ClampUnit(layer.blend);
        if (blend <= 0.0f) {
            continue;
        }
        result = BlendPostProcessParameters(result, MakePostProcessPreset(layer.preset), blend);
    }
    return result;
}

} // namespace ri::render
