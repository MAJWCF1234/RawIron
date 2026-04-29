#include "RawIron/Render/VulkanPipelineStateCache.h"

namespace ri::render::vulkan {

std::optional<VulkanPipelineStateRecord> VulkanPipelineStateCache::Resolve(const VulkanPipelineStateKey& key,
                                                                            const ResolveFn& resolver) {
    stats_.lookups += 1U;
    if (const auto found = records_.find(key); found != records_.end()) {
        stats_.hits += 1U;
        return found->second;
    }

    stats_.misses += 1U;
    if (!resolver) {
        return std::nullopt;
    }
    const std::optional<VulkanPipelineStateRecord> resolved = resolver(key);
    if (!resolved.has_value()) {
        return std::nullopt;
    }

    records_.insert_or_assign(key, *resolved);
    stats_.stored = records_.size();
    return resolved;
}

std::optional<VulkanPipelineStateRecord> VulkanPipelineStateCache::Lookup(const VulkanPipelineStateKey& key) const {
    if (const auto found = records_.find(key); found != records_.end()) {
        return found->second;
    }
    return std::nullopt;
}

void VulkanPipelineStateCache::Clear() noexcept {
    records_.clear();
    stats_.stored = 0;
}

VulkanPipelineStateCacheStats VulkanPipelineStateCache::Stats() const noexcept {
    return stats_;
}

} // namespace ri::render::vulkan
