#pragma once

#include "RawIron/Render/PreviewTexture.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ri::render::software {

/// Deduplicated synchronous loads keyed by normalized filesystem path. Suitable as the narrow
/// GPU-upload staging dedupe layer above \ref LoadRgbaImageFile (color data normalized to RGBA8).
class RgbaImagePathCache {
public:
    [[nodiscard]] std::shared_ptr<const RgbaImage> Load(const std::filesystem::path& path);

    void Clear() noexcept;

    [[nodiscard]] std::size_t CachedEntryCount() const noexcept;

private:
    mutable std::mutex mutex_{};
    std::unordered_map<std::string, std::shared_ptr<const RgbaImage>> cache_{};
};

} // namespace ri::render::software
