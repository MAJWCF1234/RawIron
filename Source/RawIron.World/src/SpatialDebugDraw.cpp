#include "RawIron/World/SpatialDebugDraw.h"

#include "RawIron/Spatial/Aabb.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ri::world {
namespace {

ri::spatial::Aabb BuildRuntimeVolumeBoundsLocal(const RuntimeVolume& volume) {
    switch (volume.shape) {
    case VolumeShape::Box: {
        const ri::math::Vec3 halfExtents{
            std::max(0.001f, std::fabs(volume.size.x) * 0.5f),
            std::max(0.001f, std::fabs(volume.size.y) * 0.5f),
            std::max(0.001f, std::fabs(volume.size.z) * 0.5f),
        };
        return {.min = volume.position - halfExtents, .max = volume.position + halfExtents};
    }
    case VolumeShape::Cylinder: {
        const float radius = std::max(0.001f, std::fabs(std::isfinite(volume.radius) ? volume.radius : 0.5f));
        const float halfHeight = std::max(
            0.001f,
            std::fabs(std::isfinite(volume.height) ? volume.height : volume.size.y) * 0.5f);
        const ri::math::Vec3 extents{radius, halfHeight, radius};
        return {.min = volume.position - extents, .max = volume.position + extents};
    }
    case VolumeShape::Sphere:
    default: {
        const float radius = std::max(0.001f, std::fabs(std::isfinite(volume.radius) ? volume.radius : 0.5f));
        const ri::math::Vec3 extents{radius, radius, radius};
        return {.min = volume.position - extents, .max = volume.position + extents};
    }
    }
}

std::string PickSemanticColor(std::string_view type) {
    if (type.find("trigger") != std::string_view::npos) return "#6bd16b";
    if (type.find("damage") != std::string_view::npos || type.find("kill") != std::string_view::npos) return "#ff6b6b";
    if (type.find("camera") != std::string_view::npos) return "#6bc5ff";
    if (type.find("safe") != std::string_view::npos) return "#ffd36b";
    if (type.find("proxy") != std::string_view::npos) return "#c18aff";
    return "#b8b8b8";
}

std::uint32_t PickLayer(std::string_view type) {
    if (type.find("trigger") != std::string_view::npos) return 20U;
    if (type.find("collision") != std::string_view::npos || type.find("proxy") != std::string_view::npos) return 10U;
    return 30U;
}

} // namespace

std::vector<DebugVolumeDrawItem> BuildDebugVolumeVisualizerItems(const std::vector<RuntimeVolume>& volumes,
                                                                 const DebugVolumeVisualizationRule& rule) {
    std::vector<DebugVolumeDrawItem> items;
    const std::size_t stride = std::max<std::size_t>(1U, rule.lodStride);
    items.reserve((volumes.size() / stride) + 1U);
    for (std::size_t index = 0; index < volumes.size(); index += stride) {
        const RuntimeVolume& volume = volumes[index];
        if (!rule.includeHidden && !volume.debugVisible) {
            continue;
        }
        if (rule.typePrefix.has_value() && !rule.typePrefix->empty() && volume.type.rfind(*rule.typePrefix, 0) != 0) {
            continue;
        }
        const ri::spatial::Aabb bounds = BuildRuntimeVolumeBoundsLocal(volume);
        items.push_back(DebugVolumeDrawItem{
            .id = volume.id,
            .type = volume.type,
            .min = bounds.min,
            .max = bounds.max,
            .colorHex = PickSemanticColor(volume.type),
            .layer = PickLayer(volume.type),
        });
    }
    std::sort(items.begin(), items.end(), [](const DebugVolumeDrawItem& lhs, const DebugVolumeDrawItem& rhs) {
        if (lhs.layer != rhs.layer) return lhs.layer < rhs.layer;
        return lhs.id < rhs.id;
    });
    return items;
}

RuntimeDebugVisibilityController::RuntimeDebugVisibilityController(RuntimeDebugVisibilityPolicy policy)
    : policy_(std::move(policy)) {}

void RuntimeDebugVisibilityController::SetPolicy(RuntimeDebugVisibilityPolicy policy) {
    policy_ = std::move(policy);
}

const RuntimeDebugVisibilityPolicy& RuntimeDebugVisibilityController::GetPolicy() const {
    return policy_;
}

bool RuntimeDebugVisibilityController::IsDomainVisible(const DebugDomain domain) const {
    return policy_.enabledDomains.contains(domain);
}

bool RuntimeDebugVisibilityController::CanRenderArtifacts(const std::size_t artifactCount) const {
    return artifactCount <= policy_.maxArtifactsPerTick;
}

void DebugHelperRegistry::Register(DebugArtifactRecord artifact) {
    if (artifact.id.empty()) {
        artifact.id = artifact.category + ":" + artifact.source;
    }
    artifacts_[artifact.id] = std::move(artifact);
}

void DebugHelperRegistry::Tick(const double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0) {
        return;
    }
    for (auto& [id, artifact] : artifacts_) {
        (void)id;
        artifact.ageSeconds += deltaSeconds;
    }
    for (auto it = artifacts_.begin(); it != artifacts_.end();) {
        if (it->second.ttlSeconds > 0.0 && it->second.ageSeconds >= it->second.ttlSeconds) {
            it = artifacts_.erase(it);
        } else {
            ++it;
        }
    }
}

void DebugHelperRegistry::CleanupForHotReload() {
    artifacts_.clear();
}

std::vector<DebugArtifactRecord> DebugHelperRegistry::QueryByCategory(const std::string_view category) const {
    std::vector<DebugArtifactRecord> records;
    for (const auto& [id, artifact] : artifacts_) {
        (void)id;
        if (!category.empty() && artifact.category != category) {
            continue;
        }
        records.push_back(artifact);
    }
    std::sort(records.begin(), records.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.category != rhs.category) return lhs.category < rhs.category;
        return lhs.id < rhs.id;
    });
    return records;
}

std::vector<DebugVolumeDrawItem> BuildComposableDebugVisualizations(
    const std::vector<ComposableDebugVisualizationSource>& sources,
    const RuntimeDebugVisibilityController& visibilityController,
    const DebugVolumeVisualizationRule& rule) {
    std::vector<RuntimeVolume> filteredVolumes;
    filteredVolumes.reserve(sources.size());
    for (const ComposableDebugVisualizationSource& source : sources) {
        if (!visibilityController.IsDomainVisible(source.domain)) {
            continue;
        }
        RuntimeVolume volume = source.volume;
        if (!source.annotation.empty()) {
            volume.type = source.volume.type + ":" + source.annotation;
        }
        filteredVolumes.push_back(std::move(volume));
    }
    if (!visibilityController.CanRenderArtifacts(filteredVolumes.size())) {
        filteredVolumes.resize(visibilityController.GetPolicy().maxArtifactsPerTick);
    }
    return BuildDebugVolumeVisualizerItems(filteredVolumes, rule);
}

void SpatialRelationDebugTracer::Push(SpatialRelationDebugSegment segment) {
    segment.sequence = nextSequence_++;
    if (segment.ttlSeconds < 0.0) {
        segment.ttlSeconds = 0.0;
    }
    if (segment.ageSeconds < 0.0) {
        segment.ageSeconds = 0.0;
    }
    segments_.push_back(std::move(segment));
}

void SpatialRelationDebugTracer::Tick(const double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0) {
        return;
    }
    for (SpatialRelationDebugSegment& segment : segments_) {
        segment.ageSeconds += deltaSeconds;
    }
    segments_.erase(std::remove_if(segments_.begin(),
                                   segments_.end(),
                                   [](const SpatialRelationDebugSegment& segment) {
                                       return segment.ageSeconds >= segment.ttlSeconds;
                                   }),
                    segments_.end());
}

std::vector<SpatialRelationDebugSegment> SpatialRelationDebugTracer::Query(std::string_view channel) const {
    std::vector<SpatialRelationDebugSegment> filtered;
    for (const SpatialRelationDebugSegment& segment : segments_) {
        if (!channel.empty() && segment.channel != channel) {
            continue;
        }
        filtered.push_back(segment);
    }
    std::sort(filtered.begin(), filtered.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.sequence < rhs.sequence;
    });
    return filtered;
}

void SpatialRelationDebugTracer::Clear() {
    segments_.clear();
}

} // namespace ri::world
