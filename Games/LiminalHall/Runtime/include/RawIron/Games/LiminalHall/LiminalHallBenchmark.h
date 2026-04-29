#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace ri::games::liminal {

/// Headless timing of `RenderScenePreview` on \ref BuildWorld (CPU software rasterizer).
[[nodiscard]] bool RunLiminalHallSoftwareRenderBenchmark(const std::filesystem::path& workspaceRoot,
                                                         int viewportWidth,
                                                         int viewportHeight,
                                                         std::uint32_t warmupFrames,
                                                         std::uint32_t timedFrames,
                                                         std::string* reportOut,
                                                         std::string* errorOut);

} // namespace ri::games::liminal
