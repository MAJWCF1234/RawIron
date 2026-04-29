#pragma once

#include "RawIron/Math/Vec3.h"
#include "RawIron/World/RuntimeState.h"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::world {

struct DebugVolumeVisualizationRule {
    bool includeHidden = false;
    std::optional<std::string> typePrefix;
    std::size_t lodStride = 1U;
};

struct DebugVolumeDrawItem {
    std::string id;
    std::string type;
    ri::math::Vec3 min{};
    ri::math::Vec3 max{};
    std::string colorHex;
    std::uint32_t layer = 0U;
};

[[nodiscard]] std::vector<DebugVolumeDrawItem> BuildDebugVolumeVisualizerItems(
    const std::vector<RuntimeVolume>& volumes,
    const DebugVolumeVisualizationRule& rule = {});

struct SpatialRelationDebugSegment {
    std::string channel;
    std::string sourceId;
    std::string targetId;
    ri::math::Vec3 start{};
    ri::math::Vec3 end{};
    std::string colorHex = "#6cc4ff";
    double ttlSeconds = 0.5;
    double ageSeconds = 0.0;
    std::uint64_t sequence = 0;
};

enum class DebugDomain {
    Physics,
    AI,
    Render,
    Audio,
    Network,
};

struct RuntimeDebugVisibilityPolicy {
    std::string role = "dev";
    bool enablePersistence = true;
    std::size_t maxArtifactsPerTick = 2048U;
    std::set<DebugDomain> enabledDomains{
        DebugDomain::Physics,
        DebugDomain::AI,
        DebugDomain::Render,
    };
};

class RuntimeDebugVisibilityController {
public:
    explicit RuntimeDebugVisibilityController(RuntimeDebugVisibilityPolicy policy = {});
    void SetPolicy(RuntimeDebugVisibilityPolicy policy);
    [[nodiscard]] const RuntimeDebugVisibilityPolicy& GetPolicy() const;
    [[nodiscard]] bool IsDomainVisible(DebugDomain domain) const;
    [[nodiscard]] bool CanRenderArtifacts(std::size_t artifactCount) const;

private:
    RuntimeDebugVisibilityPolicy policy_{};
};

struct DebugArtifactRecord {
    std::string id;
    std::string category;
    std::string source;
    double ttlSeconds = 0.0;
    double ageSeconds = 0.0;
};

class DebugHelperRegistry {
public:
    void Register(DebugArtifactRecord artifact);
    void Tick(double deltaSeconds);
    void CleanupForHotReload();
    [[nodiscard]] std::vector<DebugArtifactRecord> QueryByCategory(std::string_view category = {}) const;

private:
    std::unordered_map<std::string, DebugArtifactRecord> artifacts_;
};

struct ComposableDebugVisualizationSource {
    std::string sourceId;
    RuntimeVolume volume;
    std::string annotation;
    DebugDomain domain = DebugDomain::Physics;
};

[[nodiscard]] std::vector<DebugVolumeDrawItem> BuildComposableDebugVisualizations(
    const std::vector<ComposableDebugVisualizationSource>& sources,
    const RuntimeDebugVisibilityController& visibilityController,
    const DebugVolumeVisualizationRule& rule = {});

class SpatialRelationDebugTracer {
public:
    void Push(SpatialRelationDebugSegment segment);
    void Tick(double deltaSeconds);
    [[nodiscard]] std::vector<SpatialRelationDebugSegment> Query(std::string_view channel = {}) const;
    void Clear();

private:
    std::uint64_t nextSequence_ = 1;
    std::vector<SpatialRelationDebugSegment> segments_;
};

} // namespace ri::world
