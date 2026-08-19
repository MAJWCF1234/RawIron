#include "RawIron/Render/VulkanWarmupCache.h"

#include "RawIron/Core/JobSystem.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

namespace fs = std::filesystem;

bool WriteOnePixelBmp(const fs::path& path) {
    constexpr std::array<std::uint8_t, 58> bytes{
        0x42, 0x4D, 0x3A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00,
        0x28, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
        0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x13, 0x0B, 0x00, 0x00,
        0x13, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xFF, 0x00,
    };
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(stream);
}

} // namespace

int main() {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / ("RawIronVulkanWarmupCacheSmoke-" + std::to_string(nonce));
    std::error_code ec{};
    fs::create_directories(root, ec);
    if (ec) {
        return EXIT_FAILURE;
    }
    const fs::path imagePath = root / "pixel.bmp";
    if (!WriteOnePixelBmp(imagePath)) {
        fs::remove_all(root, ec);
        return EXIT_FAILURE;
    }

    ri::render::vulkan::VulkanWarmupCache cache{};
    const ri::render::vulkan::VulkanWarmupCacheOptions burst{
        .mode = ri::render::vulkan::VulkanWarmupMode::Burst,
        .maxWorkerThreads = 4,
        .maxDecodedBytes = 1024,
    };
    const ri::render::vulkan::VulkanWarmupCacheStats stats =
        cache.Preload({imagePath, imagePath, root / "missing.bmp"}, burst);
    if (stats.requestedPaths != 3U || stats.uniquePaths != 2U || stats.decodedPaths != 1U
        || stats.failedPaths != 1U || stats.workerThreads == 0U || stats.workerThreads > 2U
        || stats.retainedBytes != 4U || cache.CachedEntryCount() != 1U) {
        fs::remove_all(root, ec);
        return EXIT_FAILURE;
    }
    const auto first = cache.Load(imagePath);
    const auto second = cache.Load(imagePath);
    if (!first || !first->Valid() || first != second) {
        fs::remove_all(root, ec);
        return EXIT_FAILURE;
    }

    ri::core::JobSystem sharedJobs({.workerCount = 2U, .maxWorkerCount = 2U});
    const ri::render::vulkan::VulkanWarmupCacheStats sharedStats =
        cache.Preload({imagePath, root / "missing.bmp"}, burst, &sharedJobs);
    if (sharedStats.decodedPaths != 1U || sharedStats.failedPaths != 1U
        || sharedJobs.Metrics().executedJobs == 0U || !sharedJobs.AcceptingJobs()) {
        fs::remove_all(root, ec);
        return EXIT_FAILURE;
    }

    const ri::render::vulkan::VulkanWarmupCacheStats limited = cache.Preload(
        {imagePath},
        ri::render::vulkan::VulkanWarmupCacheOptions{
            .mode = ri::render::vulkan::VulkanWarmupMode::Burst,
            .maxWorkerThreads = 1,
            .maxDecodedBytes = 3,
        });
    if (limited.decodedPaths != 0U || limited.budgetSkippedPaths != 1U
        || limited.retainedBytes != 0U || cache.CachedEntryCount() != 0U) {
        fs::remove_all(root, ec);
        return EXIT_FAILURE;
    }

    const ri::render::vulkan::VulkanPipelineCacheIdentity identity{
        .vendorId = 0x10DEU,
        .deviceId = 0x2489U,
        .driverVersion = 1234U,
        .uuid = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    };
    const std::vector<std::uint8_t> payload{1, 3, 3, 7, 9, 2, 5, 8};
    const fs::path pipelinePath = root / "pipeline.riwarm";
    if (!ri::render::vulkan::SaveVulkanPipelineWarmupBlob(pipelinePath, identity, payload)) {
        fs::remove_all(root, ec);
        return EXIT_FAILURE;
    }
    if (ri::render::vulkan::LoadVulkanPipelineWarmupBlob(pipelinePath, identity) != payload) {
        fs::remove_all(root, ec);
        return EXIT_FAILURE;
    }
    auto mismatchedIdentity = identity;
    mismatchedIdentity.driverVersion += 1U;
    if (!ri::render::vulkan::LoadVulkanPipelineWarmupBlob(pipelinePath, mismatchedIdentity).empty()) {
        fs::remove_all(root, ec);
        return EXIT_FAILURE;
    }

    {
        std::fstream corrupt(pipelinePath, std::ios::binary | std::ios::in | std::ios::out);
        corrupt.seekp(-1, std::ios::end);
        corrupt.put(static_cast<char>(0xFF));
    }
    if (!ri::render::vulkan::LoadVulkanPipelineWarmupBlob(pipelinePath, identity).empty()) {
        fs::remove_all(root, ec);
        return EXIT_FAILURE;
    }

    fs::remove_all(root, ec);
    return !sharedJobs.Shutdown() || ec ? EXIT_FAILURE : EXIT_SUCCESS;
}
