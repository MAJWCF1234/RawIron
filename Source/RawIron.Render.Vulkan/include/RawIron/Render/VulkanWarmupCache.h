#pragma once

#include "RawIron/Render/PreviewTexture.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ri::core {
class JobSystem;
}

namespace ri::render::vulkan {

enum class VulkanWarmupMode : std::uint8_t {
    Disabled,
    Balanced,
    Burst,
};

struct VulkanWarmupCacheOptions {
    VulkanWarmupMode mode = VulkanWarmupMode::Burst;
    /// Zero selects an automatic limit. Burst reserves one logical CPU for the OS when possible.
    std::uint32_t maxWorkerThreads = 0;
    /// Hard limit for retained decoded RGBA staging data. Zero disables retention.
    std::uint64_t maxDecodedBytes = 512ULL * 1024ULL * 1024ULL;
};

struct VulkanWarmupCacheStats {
    std::size_t requestedPaths = 0;
    std::size_t uniquePaths = 0;
    std::size_t decodedPaths = 0;
    std::size_t failedPaths = 0;
    std::size_t budgetSkippedPaths = 0;
    std::uint32_t workerThreads = 0;
    std::uint64_t retainedBytes = 0;
    std::uint64_t elapsedMilliseconds = 0;
    /// Bounded diagnostic sample; the full failed set is intentionally not retained.
    std::vector<std::filesystem::path> failedPathSamples{};
};

/// A short-lived, bounded decode cache used before the Vulkan upload phase. Preload() fans unique
/// texture work across the configured CPU allowance; Vulkan object creation remains serialized.
class VulkanWarmupCache {
public:
    [[nodiscard]] VulkanWarmupCacheStats Preload(const std::vector<std::filesystem::path>& paths,
                                                 const VulkanWarmupCacheOptions& options,
                                                 ri::core::JobSystem* sharedJobs = nullptr);
    [[nodiscard]] std::shared_ptr<const ri::render::software::RgbaImage> Load(
        const std::filesystem::path& path);

    void Clear() noexcept;
    [[nodiscard]] std::size_t CachedEntryCount() const noexcept;
    [[nodiscard]] std::uint64_t CachedBytes() const noexcept;

private:
    [[nodiscard]] static std::string StableKey(const std::filesystem::path& path);
    [[nodiscard]] bool TryStore(const std::string& key,
                                const std::shared_ptr<const ri::render::software::RgbaImage>& image);

    mutable std::mutex mutex_{};
    std::unordered_map<std::string, std::shared_ptr<const ri::render::software::RgbaImage>> images_{};
    std::uint64_t maxDecodedBytes_ = 0;
    std::uint64_t cachedBytes_ = 0;
};

inline constexpr std::size_t kVulkanPipelineCacheUuidBytes = 16;
inline constexpr std::uint64_t kVulkanPipelineWarmupMaxBytes = 64ULL * 1024ULL * 1024ULL;

struct VulkanPipelineCacheIdentity {
    std::uint32_t vendorId = 0;
    std::uint32_t deviceId = 0;
    std::uint32_t driverVersion = 0;
    std::array<std::uint8_t, kVulkanPipelineCacheUuidBytes> uuid{};

    [[nodiscard]] bool operator==(const VulkanPipelineCacheIdentity&) const noexcept = default;
};

[[nodiscard]] std::filesystem::path DefaultVulkanPipelineWarmupCachePath();

[[nodiscard]] std::vector<std::uint8_t> LoadVulkanPipelineWarmupBlob(
    const std::filesystem::path& path,
    const VulkanPipelineCacheIdentity& expectedIdentity,
    std::uint64_t maxPayloadBytes = kVulkanPipelineWarmupMaxBytes);

[[nodiscard]] bool SaveVulkanPipelineWarmupBlob(
    const std::filesystem::path& path,
    const VulkanPipelineCacheIdentity& identity,
    const std::vector<std::uint8_t>& payload,
    std::uint64_t maxPayloadBytes = kVulkanPipelineWarmupMaxBytes);

} // namespace ri::render::vulkan
