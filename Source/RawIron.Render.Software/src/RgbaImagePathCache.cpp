#include "RawIron/Render/RgbaImagePathCache.h"

#include <system_error>
#include <utility>

namespace ri::render::software {
namespace {

[[nodiscard]] std::string StableCacheKey(const std::filesystem::path& path) {
    std::error_code ec{};
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
    const std::filesystem::path& usePath = ec ? path : canonical;
    return usePath.generic_string();
}

} // namespace

std::shared_ptr<const RgbaImage> RgbaImagePathCache::Load(const std::filesystem::path& path) {
    const std::string key = StableCacheKey(path);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (const auto it = cache_.find(key); it != cache_.end()) {
            return it->second;
        }
    }
    RgbaImage image = LoadRgbaImageFile(path);
    if (!image.Valid()) {
        return nullptr;
    }
    auto shared = std::make_shared<RgbaImage>(std::move(image));
    std::lock_guard<std::mutex> lock(mutex_);
    if (const auto it = cache_.find(key); it != cache_.end()) {
        return it->second;
    }
    cache_[key] = shared;
    return shared;
}

void RgbaImagePathCache::Clear() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

std::size_t RgbaImagePathCache::CachedEntryCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

} // namespace ri::render::software
