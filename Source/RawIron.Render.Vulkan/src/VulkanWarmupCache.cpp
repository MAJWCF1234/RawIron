#include "RawIron/Render/VulkanWarmupCache.h"

#include "RawIron/Core/JobSystem.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <unordered_set>

namespace ri::render::vulkan {
namespace {

namespace fs = std::filesystem;

constexpr std::array<char, 8> kPipelineCacheMagic{'R', 'I', 'W', 'A', 'R', 'M', '1', '\0'};
constexpr std::uint32_t kPipelineCacheFormatVersion = 1;

struct PipelineCacheFileHeader {
    std::array<char, 8> magic{};
    std::uint32_t version = 0;
    std::uint32_t vendorId = 0;
    std::uint32_t deviceId = 0;
    std::uint32_t driverVersion = 0;
    std::array<std::uint8_t, kVulkanPipelineCacheUuidBytes> uuid{};
    std::uint64_t payloadBytes = 0;
    std::uint64_t payloadHash = 0;
};

[[nodiscard]] std::uint64_t StableHash64(const std::vector<std::uint8_t>& bytes) noexcept {
    std::uint64_t hash = 0xCBF29CE484222325ULL;
    for (const std::uint8_t byte : bytes) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 0x00000100000001B3ULL;
    }
    return hash;
}

[[nodiscard]] std::uint32_t ResolveWorkerCount(const VulkanWarmupCacheOptions& options,
                                               const std::size_t jobCount) noexcept {
    if (options.mode == VulkanWarmupMode::Disabled || jobCount == 0U) {
        return 0;
    }
    const std::uint32_t hardwareThreads = std::max(1U, std::thread::hardware_concurrency());
    std::uint32_t allowance = options.maxWorkerThreads;
    if (allowance == 0U) {
        if (options.mode == VulkanWarmupMode::Burst) {
            allowance = hardwareThreads > 1U ? hardwareThreads - 1U : 1U;
        } else {
            allowance = std::min(4U, std::max(1U, (hardwareThreads + 1U) / 2U));
        }
    }
    return std::max(1U, std::min<std::uint32_t>(allowance, static_cast<std::uint32_t>(jobCount)));
}

} // namespace

std::string VulkanWarmupCache::StableKey(const fs::path& path) {
    std::error_code ec{};
    const fs::path canonical = fs::weakly_canonical(path, ec);
    return (ec ? path.lexically_normal() : canonical).generic_string();
}

bool VulkanWarmupCache::TryStore(
    const std::string& key,
    const std::shared_ptr<const ri::render::software::RgbaImage>& image) {
    if (!image || !image->Valid()) {
        return false;
    }
    const std::uint64_t imageBytes = static_cast<std::uint64_t>(image->rgba.size());
    std::lock_guard<std::mutex> lock(mutex_);
    if (images_.contains(key)) {
        return true;
    }
    if (imageBytes > maxDecodedBytes_ || cachedBytes_ > maxDecodedBytes_ - imageBytes) {
        return false;
    }
    images_.emplace(key, image);
    cachedBytes_ += imageBytes;
    return true;
}

VulkanWarmupCacheStats VulkanWarmupCache::Preload(const std::vector<fs::path>& paths,
                                                   const VulkanWarmupCacheOptions& options,
                                                   ri::core::JobSystem* sharedJobs) {
    const auto started = std::chrono::steady_clock::now();
    Clear();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        maxDecodedBytes_ = options.mode == VulkanWarmupMode::Disabled ? 0U : options.maxDecodedBytes;
    }

    VulkanWarmupCacheStats stats{};
    stats.requestedPaths = paths.size();
    std::vector<std::pair<std::string, fs::path>> jobs{};
    jobs.reserve(paths.size());
    std::unordered_set<std::string> seen{};
    seen.reserve(paths.size());
    for (const fs::path& path : paths) {
        if (path.empty()) {
            continue;
        }
        std::string key = StableKey(path);
        if (seen.emplace(key).second) {
            jobs.emplace_back(std::move(key), path);
        }
    }
    stats.uniquePaths = jobs.size();
    stats.workerThreads = ResolveWorkerCount(options, jobs.size());
    if (stats.workerThreads == 0U || options.maxDecodedBytes == 0U) {
        stats.elapsedMilliseconds = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
        return stats;
    }

    std::atomic_size_t nextJob{0};
    std::atomic_size_t decoded{0};
    std::atomic_size_t failed{0};
    std::atomic_size_t skipped{0};
    std::vector<std::uint8_t> failedJobs(jobs.size(), 0U);

    std::unique_ptr<ri::core::JobSystem> ownedJobs;
    if (sharedJobs == nullptr) {
        ownedJobs = std::make_unique<ri::core::JobSystem>(ri::core::JobSystemConfig{
            .workerCount = stats.workerThreads,
            .maxWorkerCount = stats.workerThreads,
        });
        sharedJobs = ownedJobs.get();
    }

    ri::core::JobFence decodeFence;
    for (std::uint32_t workerIndex = 0; workerIndex < stats.workerThreads; ++workerIndex) {
        (void)workerIndex;
        if (!sharedJobs->Submit(decodeFence, [&] {
            while (true) {
                const std::size_t jobIndex = nextJob.fetch_add(1U, std::memory_order_relaxed);
                if (jobIndex >= jobs.size()) {
                    break;
                }
                ri::render::software::RgbaImage decodedImage =
                    ri::render::software::LoadRgbaImageFile(jobs[jobIndex].second);
                if (!decodedImage.Valid()) {
                    failedJobs[jobIndex] = 1U;
                    failed.fetch_add(1U, std::memory_order_relaxed);
                    continue;
                }
                auto shared = std::make_shared<ri::render::software::RgbaImage>(std::move(decodedImage));
                if (TryStore(jobs[jobIndex].first, shared)) {
                    decoded.fetch_add(1U, std::memory_order_relaxed);
                } else {
                    skipped.fetch_add(1U, std::memory_order_relaxed);
                }
            }
        })) {
            try {
                sharedJobs->Wait(decodeFence);
            } catch (...) {
                // Submission failure remains the primary diagnostic.
            }
            throw std::runtime_error("Vulkan warmup could not submit decode work.");
        }
    }
    sharedJobs->Wait(decodeFence);

    stats.decodedPaths = decoded.load(std::memory_order_relaxed);
    stats.failedPaths = failed.load(std::memory_order_relaxed);
    stats.budgetSkippedPaths = skipped.load(std::memory_order_relaxed);
    stats.retainedBytes = CachedBytes();
    constexpr std::size_t kMaxFailedPathSamples = 4U;
    for (std::size_t jobIndex = 0; jobIndex < failedJobs.size()
         && stats.failedPathSamples.size() < kMaxFailedPathSamples; ++jobIndex) {
        if (failedJobs[jobIndex] != 0U) {
            stats.failedPathSamples.push_back(jobs[jobIndex].second);
        }
    }
    stats.elapsedMilliseconds = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
    return stats;
}

std::shared_ptr<const ri::render::software::RgbaImage> VulkanWarmupCache::Load(const fs::path& path) {
    if (path.empty()) {
        return nullptr;
    }
    const std::string key = StableKey(path);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (const auto it = images_.find(key); it != images_.end()) {
            return it->second;
        }
    }
    ri::render::software::RgbaImage image = ri::render::software::LoadRgbaImageFile(path);
    if (!image.Valid()) {
        return nullptr;
    }
    auto shared = std::make_shared<ri::render::software::RgbaImage>(std::move(image));
    (void)TryStore(key, shared);
    return shared;
}

void VulkanWarmupCache::Clear() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    images_.clear();
    cachedBytes_ = 0;
    maxDecodedBytes_ = 0;
}

std::size_t VulkanWarmupCache::CachedEntryCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return images_.size();
}

std::uint64_t VulkanWarmupCache::CachedBytes() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return cachedBytes_;
}

fs::path DefaultVulkanPipelineWarmupCachePath() {
    std::error_code ec{};
    const fs::path current = fs::current_path(ec);
    if (!ec) {
        return current / "Saved" / "Cache" / "Vulkan" / "native-scene-pipelines.riwarm";
    }
    const fs::path temporary = fs::temp_directory_path(ec);
    return ec ? fs::path{} : temporary / "RawIron" / "native-scene-pipelines.riwarm";
}

std::vector<std::uint8_t> LoadVulkanPipelineWarmupBlob(const fs::path& path,
                                                       const VulkanPipelineCacheIdentity& expectedIdentity,
                                                       const std::uint64_t maxPayloadBytes) {
    if (path.empty() || maxPayloadBytes == 0U) {
        return {};
    }
    std::error_code ec{};
    const std::uintmax_t fileBytes = fs::file_size(path, ec);
    if (ec || fileBytes < sizeof(PipelineCacheFileHeader)
        || fileBytes > sizeof(PipelineCacheFileHeader) + maxPayloadBytes) {
        return {};
    }

    std::ifstream stream(path, std::ios::binary);
    PipelineCacheFileHeader header{};
    stream.read(reinterpret_cast<char*>(&header), sizeof(header));
    const VulkanPipelineCacheIdentity actualIdentity{
        .vendorId = header.vendorId,
        .deviceId = header.deviceId,
        .driverVersion = header.driverVersion,
        .uuid = header.uuid,
    };
    if (!stream || header.magic != kPipelineCacheMagic || header.version != kPipelineCacheFormatVersion
        || actualIdentity != expectedIdentity || header.payloadBytes == 0U
        || header.payloadBytes > maxPayloadBytes
        || fileBytes != sizeof(PipelineCacheFileHeader) + header.payloadBytes) {
        return {};
    }

    std::vector<std::uint8_t> payload(static_cast<std::size_t>(header.payloadBytes));
    stream.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    if (!stream || StableHash64(payload) != header.payloadHash) {
        return {};
    }
    return payload;
}

bool SaveVulkanPipelineWarmupBlob(const fs::path& path,
                                  const VulkanPipelineCacheIdentity& identity,
                                  const std::vector<std::uint8_t>& payload,
                                  const std::uint64_t maxPayloadBytes) {
    if (path.empty() || payload.empty() || payload.size() > maxPayloadBytes) {
        return false;
    }
    std::error_code ec{};
    fs::create_directories(path.parent_path(), ec);
    if (ec) {
        return false;
    }

    PipelineCacheFileHeader header{
        .magic = kPipelineCacheMagic,
        .version = kPipelineCacheFormatVersion,
        .vendorId = identity.vendorId,
        .deviceId = identity.deviceId,
        .driverVersion = identity.driverVersion,
        .uuid = identity.uuid,
        .payloadBytes = static_cast<std::uint64_t>(payload.size()),
        .payloadHash = StableHash64(payload),
    };
    fs::path temporary = path;
    temporary += ".tmp." + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
        stream.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
        stream.flush();
        if (!stream) {
            fs::remove(temporary, ec);
            return false;
        }
    }

    fs::rename(temporary, path, ec);
    if (ec) {
        ec.clear();
        fs::remove(path, ec);
        ec.clear();
        fs::rename(temporary, path, ec);
    }
    if (ec) {
        std::error_code cleanupEc{};
        fs::remove(temporary, cleanupEc);
        return false;
    }
    return true;
}

} // namespace ri::render::vulkan
