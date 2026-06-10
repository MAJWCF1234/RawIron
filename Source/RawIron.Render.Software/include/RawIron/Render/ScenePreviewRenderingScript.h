#pragma once

#include "RawIron/Content/ScriptScalars.h"
#include "RawIron/Render/ScenePreview.h"

#include <filesystem>

namespace ri::render::software {

/// Applies `scripts/rendering.riscript` scalar keys onto software preview atmosphere options.
void ApplyRenderingScriptScalarsToScenePreview(const ri::content::ScriptScalarMap& scalars,
                                               ScenePreviewOptions& options);

/// Loads rendering.riscript when present; returns false if the file is missing.
[[nodiscard]] bool TryApplyRenderingScriptFileToScenePreview(const std::filesystem::path& renderingScriptPath,
                                                             ScenePreviewOptions& options);

/// Applies postprocess scalars with rendering.riscript fallback (matches runtime native tuning precedence).
void ApplyPostprocessScriptScalarsToScenePreview(const ri::content::ScriptScalarMap& postprocess,
                                                 const ri::content::ScriptScalarMap& renderingFallback,
                                                 ScenePreviewOptions& options);

/// Loads postprocess.riscript when present; uses rendering.riscript for fallback keys.
[[nodiscard]] bool TryApplyPostprocessScriptFileToScenePreview(
    const std::filesystem::path& postprocessScriptPath,
    const std::filesystem::path& renderingScriptPath,
    ScenePreviewOptions& options);

struct GamePreviewScriptTimestamps {
    std::filesystem::file_time_type rendering{};
    std::filesystem::file_time_type postprocess{};
    bool renderingTracked = false;
    bool postprocessTracked = false;
};

/// Records current script mtimes without reporting a change (call after mount/save/preset apply).
void SnapshotGamePreviewScriptTimestamps(const std::filesystem::path& gameRoot,
                                         GamePreviewScriptTimestamps& lastKnown);

/// Returns true when rendering/postprocess scripts changed on disk since the last snapshot.
[[nodiscard]] bool DidGamePreviewScriptsChange(const std::filesystem::path& gameRoot,
                                               GamePreviewScriptTimestamps& lastKnown);

/// Normalized fog depth in `[0, 1]` from eye-space distance.
[[nodiscard]] float ComputeScenePreviewFogFactor(const ScenePreviewOptions& options, float eyeSpaceDepth);

/// Blends `fogColor` toward authored `fogColorFar` across distance.
[[nodiscard]] ri::math::Vec3 ResolveScenePreviewFogTint(const ScenePreviewOptions& options, float fogFactor);

} // namespace ri::render::software
