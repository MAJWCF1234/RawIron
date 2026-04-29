#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>

namespace ri::render::vulkan {

struct VulkanPipelineStateKey {
    std::uint8_t passIndex = 0;
    std::uint16_t pipelineBucket = 0;
    std::uint16_t materialBucket = 0;

    [[nodiscard]] bool operator==(const VulkanPipelineStateKey& other) const noexcept {
        return passIndex == other.passIndex
            && pipelineBucket == other.pipelineBucket
            && materialBucket == other.materialBucket;
    }
};

struct VulkanPipelineStateRecord {
    std::uint64_t pipelineHandle = 0;
    std::uint64_t layoutHandle = 0;
};

struct VulkanPipelineStateCacheStats {
    std::size_t lookups = 0;
    std::size_t hits = 0;
    std::size_t misses = 0;
    std::size_t stored = 0;
};

class VulkanPipelineStateCache {
public:
    using ResolveFn = std::function<std::optional<VulkanPipelineStateRecord>(const VulkanPipelineStateKey&)>;

    [[nodiscard]] std::optional<VulkanPipelineStateRecord> Resolve(const VulkanPipelineStateKey& key,
                                                                   const ResolveFn& resolver);
    [[nodiscard]] std::optional<VulkanPipelineStateRecord> Lookup(const VulkanPipelineStateKey& key) const;
    void Clear() noexcept;
    [[nodiscard]] VulkanPipelineStateCacheStats Stats() const noexcept;

private:
    struct KeyHash {
        [[nodiscard]] std::size_t operator()(const VulkanPipelineStateKey& key) const noexcept {
            const std::uint64_t packed = (static_cast<std::uint64_t>(key.passIndex) << 32U)
                | (static_cast<std::uint64_t>(key.pipelineBucket) << 16U)
                | static_cast<std::uint64_t>(key.materialBucket);
            return static_cast<std::size_t>(packed ^ (packed >> 17U));
        }
    };

    std::unordered_map<VulkanPipelineStateKey, VulkanPipelineStateRecord, KeyHash> records_{};
    VulkanPipelineStateCacheStats stats_{};
};

} // namespace ri::render::vulkan
