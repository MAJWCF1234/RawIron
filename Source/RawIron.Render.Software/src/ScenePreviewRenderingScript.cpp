#include "RawIron/Render/ScenePreviewRenderingScript.h"

#include "RawIron/Math/Vec3.h"

#include <algorithm>

namespace ri::render::software {

namespace {

[[nodiscard]] ri::math::Vec3 EffectiveFogColorFar(const ScenePreviewOptions& options) {
    const bool hasAuthoredFogFar = (options.fogColorFar.x + options.fogColorFar.y + options.fogColorFar.z) > 1e-4f;
    return hasAuthoredFogFar ? options.fogColorFar : options.fogColor;
}

} // namespace

float ComputeScenePreviewFogFactor(const ScenePreviewOptions& options, const float eyeSpaceDepth) {
    const float fogSpan = std::max(options.fogEndDepth - options.fogStartDepth, 0.001f);
    return std::clamp((eyeSpaceDepth - options.fogStartDepth) / fogSpan, 0.0f, 1.0f);
}

ri::math::Vec3 ResolveScenePreviewFogTint(const ScenePreviewOptions& options, const float fogFactor) {
    const float clamped = std::clamp(fogFactor, 0.0f, 1.0f);
    return ri::math::Lerp(options.fogColor, EffectiveFogColorFar(options), clamped);
}

void ApplyRenderingScriptScalarsToScenePreview(const ri::content::ScriptScalarMap& scalars,
                                               ScenePreviewOptions& options) {
    options.clearTop = ri::math::Vec3{
        ri::content::ScriptScalarOr(scalars, "clear_top_r", options.clearTop.x),
        ri::content::ScriptScalarOr(scalars, "clear_top_g", options.clearTop.y),
        ri::content::ScriptScalarOr(scalars, "clear_top_b", options.clearTop.z),
    };
    options.clearBottom = ri::math::Vec3{
        ri::content::ScriptScalarOr(scalars, "clear_bottom_r", options.clearBottom.x),
        ri::content::ScriptScalarOr(scalars, "clear_bottom_g", options.clearBottom.y),
        ri::content::ScriptScalarOr(scalars, "clear_bottom_b", options.clearBottom.z),
    };
    options.fogColor = ri::math::Vec3{
        ri::content::ScriptScalarOr(scalars, "fog_r", options.fogColor.x),
        ri::content::ScriptScalarOr(scalars, "fog_g", options.fogColor.y),
        ri::content::ScriptScalarOr(scalars, "fog_b", options.fogColor.z),
    };
    const ri::math::Vec3 fogNear = options.fogColor;
    options.fogColorFar = ri::math::Vec3{
        ri::content::ScriptScalarOr(scalars, "fog_far_r", fogNear.x),
        ri::content::ScriptScalarOr(scalars, "fog_far_g", fogNear.y),
        ri::content::ScriptScalarOr(scalars, "fog_far_b", fogNear.z),
    };
    options.ambientLight = ri::math::Vec3{
        ri::content::ScriptScalarOr(scalars, "ambient_r", options.ambientLight.x),
        ri::content::ScriptScalarOr(scalars, "ambient_g", options.ambientLight.y),
        ri::content::ScriptScalarOr(scalars, "ambient_b", options.ambientLight.z),
    };
    options.fogStartDepth =
        ri::content::ScriptScalarOr(scalars, "fog_start", options.fogStartDepth);
    options.fogEndDepth =
        ri::content::ScriptScalarOr(scalars, "fog_end", options.fogEndDepth);
    options.fogStrength =
        ri::content::ScriptScalarOr(scalars, "fog_strength", options.fogStrength);
}

bool TryApplyRenderingScriptFileToScenePreview(const std::filesystem::path& renderingScriptPath,
                                               ScenePreviewOptions& options) {
    std::error_code ec{};
    if (!std::filesystem::exists(renderingScriptPath, ec)) {
        return false;
    }
    const ri::content::ScriptScalarMap scalars = ri::content::LoadScriptScalars(renderingScriptPath);
    if (scalars.empty()) {
        return false;
    }
    ApplyRenderingScriptScalarsToScenePreview(scalars, options);
    return true;
}

void ApplyPostprocessScriptScalarsToScenePreview(const ri::content::ScriptScalarMap& postprocess,
                                                 const ri::content::ScriptScalarMap& renderingFallback,
                                                 ScenePreviewOptions& options) {
    options.previewExposure = ri::content::ScriptScalarOrClamped(
        postprocess,
        "native_exposure",
        ri::content::ScriptScalarOrClamped(
            renderingFallback, "native_exposure", options.previewExposure, 0.5f, 2.5f),
        0.5f,
        2.5f);
    options.previewContrast = ri::content::ScriptScalarOrClamped(
        postprocess,
        "native_contrast",
        ri::content::ScriptScalarOrClamped(
            renderingFallback, "native_contrast", options.previewContrast, 0.7f, 1.6f),
        0.7f,
        1.6f);
    options.previewSaturation = ri::content::ScriptScalarOrClamped(
        postprocess,
        "native_saturation",
        ri::content::ScriptScalarOrClamped(
            renderingFallback, "native_saturation", options.previewSaturation, 0.0f, 1.8f),
        0.0f,
        1.8f);
    options.previewVignetteStrength = ri::content::ScriptScalarOrClamped(
        postprocess, "vignette_strength", options.previewVignetteStrength, 0.0f, 1.0f);
    options.previewBloomStrength = ri::content::ScriptScalarOrClamped(
        postprocess, "bloom_strength", options.previewBloomStrength, 0.0f, 1.0f);
    options.previewSharpenAmount = ri::content::ScriptScalarOrClamped(
        postprocess,
        "sharpen_amount",
        ri::content::ScriptScalarOrClamped(
            postprocess, "cas_sharpen_amount", options.previewSharpenAmount, 0.0f, 1.0f),
        0.0f,
        1.0f);
    options.previewTintStrength = ri::content::ScriptScalarOrClamped(
        postprocess,
        "postprocess_tint_strength",
        ri::content::ScriptScalarOrClamped(postprocess, "tint_strength", options.previewTintStrength, 0.0f, 1.0f),
        0.0f,
        1.0f);
    options.previewTintColor = ri::math::Vec3{
        ri::content::ScriptScalarOrClamped(postprocess, "tint_r", options.previewTintColor.x, 0.0f, 2.0f),
        ri::content::ScriptScalarOrClamped(postprocess, "tint_g", options.previewTintColor.y, 0.0f, 2.0f),
        ri::content::ScriptScalarOrClamped(postprocess, "tint_b", options.previewTintColor.z, 0.0f, 2.0f),
    };
}

namespace {

[[nodiscard]] bool ReadScriptWriteTime(const std::filesystem::path& path,
                                       std::filesystem::file_time_type& outTime) {
    std::error_code ec{};
    if (!std::filesystem::exists(path, ec)) {
        return false;
    }
    outTime = std::filesystem::last_write_time(path, ec);
    return !ec;
}

} // namespace

void SnapshotGamePreviewScriptTimestamps(const std::filesystem::path& gameRoot,
                                         GamePreviewScriptTimestamps& lastKnown) {
    const std::filesystem::path renderingScript = gameRoot / "scripts" / "rendering.riscript";
    const std::filesystem::path postprocessScript = gameRoot / "scripts" / "postprocess.riscript";

    std::filesystem::file_time_type renderingTime{};
    if (ReadScriptWriteTime(renderingScript, renderingTime)) {
        lastKnown.rendering = renderingTime;
        lastKnown.renderingTracked = true;
    } else {
        lastKnown.renderingTracked = false;
    }

    std::filesystem::file_time_type postprocessTime{};
    if (ReadScriptWriteTime(postprocessScript, postprocessTime)) {
        lastKnown.postprocess = postprocessTime;
        lastKnown.postprocessTracked = true;
    } else {
        lastKnown.postprocessTracked = false;
    }
}

bool DidGamePreviewScriptsChange(const std::filesystem::path& gameRoot,
                                 GamePreviewScriptTimestamps& lastKnown) {
    bool changed = false;
    const std::filesystem::path renderingScript = gameRoot / "scripts" / "rendering.riscript";
    const std::filesystem::path postprocessScript = gameRoot / "scripts" / "postprocess.riscript";

    std::filesystem::file_time_type renderingTime{};
    if (ReadScriptWriteTime(renderingScript, renderingTime)) {
        if (lastKnown.renderingTracked && renderingTime != lastKnown.rendering) {
            changed = true;
        }
        lastKnown.rendering = renderingTime;
        lastKnown.renderingTracked = true;
    } else {
        lastKnown.renderingTracked = false;
    }

    std::filesystem::file_time_type postprocessTime{};
    if (ReadScriptWriteTime(postprocessScript, postprocessTime)) {
        if (lastKnown.postprocessTracked && postprocessTime != lastKnown.postprocess) {
            changed = true;
        }
        lastKnown.postprocess = postprocessTime;
        lastKnown.postprocessTracked = true;
    } else {
        lastKnown.postprocessTracked = false;
    }

    return changed;
}

bool TryApplyPostprocessScriptFileToScenePreview(const std::filesystem::path& postprocessScriptPath,
                                                 const std::filesystem::path& renderingScriptPath,
                                                 ScenePreviewOptions& options) {
    std::error_code ec{};
    ri::content::ScriptScalarMap renderingFallback{};
    if (std::filesystem::exists(renderingScriptPath, ec)) {
        renderingFallback = ri::content::LoadScriptScalars(renderingScriptPath);
    }
    if (!std::filesystem::exists(postprocessScriptPath, ec)) {
        if (!renderingFallback.empty()) {
            ApplyPostprocessScriptScalarsToScenePreview({}, renderingFallback, options);
            return true;
        }
        return false;
    }
    const ri::content::ScriptScalarMap postprocess = ri::content::LoadScriptScalars(postprocessScriptPath);
    if (postprocess.empty() && renderingFallback.empty()) {
        return false;
    }
    ApplyPostprocessScriptScalarsToScenePreview(postprocess, renderingFallback, options);
    return true;
}

} // namespace ri::render::software
