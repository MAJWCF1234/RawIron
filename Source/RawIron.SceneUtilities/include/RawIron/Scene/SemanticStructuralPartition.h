#pragma once

#include "RawIron/Scene/Components.h"
#include "RawIron/Scene/Raycast.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Spatial/Aabb.h"
#include "RawIron/Spatial/SpatialIndex.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::scene {

namespace detail {

/// Heterogeneous hash so `std::string`-keyed maps accept `std::string_view` lookups without allocating.
struct TransparentStringHash {
    using is_transparent = void;
    [[nodiscard]] std::size_t operator()(const std::string_view value) const noexcept {
        return std::hash<std::string_view>{}(value);
    }
};

} // namespace detail

struct SemanticStructuralPartitionEntry {
    std::string id;
    int nodeHandle = kInvalidHandle;
    ri::spatial::Aabb bounds;
    StructuralBrushMetadata metadata{};
    std::uint64_t metadataSignature = 0;
};

struct SemanticStructuralPartitionQuery {
    std::optional<StructuralBrushChannel> channel{};
    std::optional<StructuralBrushQueryPurpose> queryPurpose{};
    std::optional<StructuralBrushOperation> operation{};
    std::optional<StructuralBrushSemanticRole> role{};
    std::optional<StructuralBrushCollisionPolicy> collision{};
    std::optional<StructuralBrushVisibilityPolicy> visibility{};
    std::optional<StructuralBrushNavigationPolicy> navigation{};
    std::optional<StructuralBrushRebuildScope> rebuildScope{};
    std::string_view brushId{};
    std::string_view region{};
    /// When set, exact raycasts skip this node's mesh so a target cannot occlude itself.
    int ignoreNodeHandle = kInvalidHandle;
};

struct SemanticStructuralPartitionHit {
    const SemanticStructuralPartitionEntry* entry = nullptr;
    float distance = 0.0f;
};

struct SemanticStructuralPickHit {
    SemanticStructuralPartitionEntry entry;
    float distance = 0.0f;
};

/// Exact mesh hit resolved after semantic-partition broad-phase filtering. `entry` is copied so
/// the result stays valid when a cache or transient partition is rebuilt after the query.
struct SemanticStructuralRaycastHit {
    SemanticStructuralPartitionEntry entry;
    RaycastHit hit{};
};

struct SemanticStructuralPartitionRoleCounts {
    std::size_t structure = 0;
    std::size_t wall = 0;
    std::size_t floor = 0;
    std::size_t ceiling = 0;
    std::size_t pillar = 0;
    std::size_t stair = 0;
    std::size_t portal = 0;
    std::size_t trim = 0;
    std::size_t cover = 0;
    std::size_t water = 0;
    std::size_t trigger = 0;
    std::size_t decor = 0;
    std::size_t volume = 0;
};

struct SemanticStructuralPartitionOperationCounts {
    std::size_t unspecified = 0;
    std::size_t solid = 0;
    std::size_t subtract = 0;
    std::size_t intersect = 0;
    std::size_t stamp = 0;
    std::size_t merge = 0;
    std::size_t detail = 0;
};

struct SemanticStructuralPartitionRebuildScopeCounts {
    std::size_t local = 0;
    std::size_t region = 0;
    std::size_t global = 0;
    std::size_t manual = 0;
};

struct SemanticStructuralPartitionChannelCounts {
    std::size_t visualMesh = 0;
    std::size_t physicsMesh = 0;
    std::size_t queryMesh = 0;
    std::size_t informationLayer = 0;
};

struct SemanticStructuralPartitionQueryPurposeCounts {
    std::size_t raycast = 0;
    std::size_t trace = 0;
    std::size_t placement = 0;
    std::size_t interaction = 0;
};

struct SemanticStructuralPartitionMetrics {
    std::size_t entryCount = 0;
    std::size_t regionCount = 0;
    std::size_t uniqueMetadataSignatureCount = 0;
    std::size_t duplicateMetadataSignatureCount = 0;
    std::size_t boxQueries = 0;
    std::size_t rayQueries = 0;
    std::size_t boxCandidatesScanned = 0;
    std::size_t rayCandidatesScanned = 0;
    /// Candidates that survived semantic filtering. Comparing matched vs scanned shows how much
    /// work the broad-phase handed to filters; comparing scanned vs entryCount shows how much the
    /// region subpartitions and BSP trees pruned before filtering.
    std::size_t boxCandidatesMatched = 0;
    std::size_t rayCandidatesMatched = 0;
    /// Region-scoped queries that resolved against a single region subpartition instead of every tree.
    std::size_t regionScopedBoxQueries = 0;
    std::size_t regionScopedRayQueries = 0;
    /// Subpartition family shape: one spatial tree per authored region plus one regionless tree.
    std::size_t subpartitionCount = 0;
    /// Rebuild locality: subpartitions whose content signature was unchanged keep their spatial
    /// tree on `Rebuild` instead of re-splitting.
    std::size_t lastRebuildSubpartitionsReused = 0;
    std::size_t lastRebuildSubpartitionsRebuilt = 0;
    SemanticStructuralPartitionRoleCounts roleCounts{};
    SemanticStructuralPartitionOperationCounts operationCounts{};
    SemanticStructuralPartitionRebuildScopeCounts rebuildScopeCounts{};
    SemanticStructuralPartitionChannelCounts channelCounts{};
    SemanticStructuralPartitionQueryPurposeCounts queryPurposeCounts{};
};

/// Semantic-filtered broad-phase over structural brush entries. Box/ray queries resolve candidates through
/// integer source indices (no string round-trips), so entries with duplicate ids remain individually queryable;
/// `FindEntry` is id-keyed and resolves duplicates to the last entry with that id. Entries with an empty `id`
/// or empty bounds are excluded from spatial queries.
///
/// The partition is a family of per-region subpartitions: entries sharing an authored `metadata.region`
/// share one spatial tree, and regionless entries share a fallback tree. Queries with a `region` filter
/// resolve against that region's subpartition only; unscoped queries walk every subpartition. `Rebuild`
/// keeps the spatial tree of any subpartition whose content (ids, bounds, order) is unchanged, so a single
/// brush edit re-splits only the affected region's tree.
///
/// Read-only lookup and query methods, `Metrics`, and `ResetMetrics` may run concurrently after `Rebuild`
/// returns. `Rebuild` and destruction remain externally synchronized writer operations and must not overlap
/// any access; hits and lookup results contain pointers into the partition and are invalidated by `Rebuild`.
/// Metrics are relaxed diagnostic counters, so snapshots taken during active queries are intentionally not a
/// transactional view of every counter at one instant.
class SemanticStructuralPartition {
public:
    void Rebuild(std::vector<SemanticStructuralPartitionEntry> entries,
                 ri::spatial::SpatialIndexOptions indexOptions = {});

    [[nodiscard]] std::vector<SemanticStructuralPartitionHit> QueryBox(
        const ri::spatial::Aabb& box,
        const SemanticStructuralPartitionQuery& query = {}) const;
    [[nodiscard]] std::vector<SemanticStructuralPartitionHit> QueryRay(
        const ri::math::Vec3& origin,
        const ri::math::Vec3& direction,
        float far,
        const SemanticStructuralPartitionQuery& query = {}) const;
    [[nodiscard]] std::optional<SemanticStructuralPartitionHit> QueryNearestRay(
        const ri::math::Vec3& origin,
        const ri::math::Vec3& direction,
        float far,
        const SemanticStructuralPartitionQuery& query = {}) const;

    [[nodiscard]] const SemanticStructuralPartitionEntry* FindEntry(std::string_view id) const;
    [[nodiscard]] std::vector<const SemanticStructuralPartitionEntry*> FindEntriesByMetadataSignature(
        std::uint64_t metadataSignature) const;
    [[nodiscard]] std::size_t MetadataSignatureBucketCount() const noexcept;
    [[nodiscard]] std::vector<const SemanticStructuralPartitionEntry*> FindEntriesByRegion(
        std::string_view region) const;
    [[nodiscard]] std::size_t RegionBucketCount() const noexcept;
    [[nodiscard]] SemanticStructuralPartitionMetrics Metrics() const noexcept;
    void ResetMetrics() noexcept;

private:
    /// One spatial tree per authored region (plus one regionless tree). `entryIndices` maps the
    /// subpartition-local source indices returned by `index` back to positions in `entries_`.
    /// `contentSignature` hashes the (id, bounds, order) content so `Rebuild` can keep the tree
    /// when a region's geometry did not change.
    struct RegionSubpartition {
        std::string region;
        std::vector<std::size_t> entryIndices;
        std::uint64_t contentSignature = 0;
        ri::spatial::BspSpatialIndex index;
    };

    /// Query-side counters kept separate from content stats so `ResetMetrics` does not disturb
    /// entry/region/signature counts computed at rebuild time.
    struct QueryStats {
        std::atomic<std::size_t> boxQueries{0};
        std::atomic<std::size_t> rayQueries{0};
        std::atomic<std::size_t> boxCandidatesScanned{0};
        std::atomic<std::size_t> rayCandidatesScanned{0};
        std::atomic<std::size_t> boxCandidatesMatched{0};
        std::atomic<std::size_t> rayCandidatesMatched{0};
        std::atomic<std::size_t> regionScopedBoxQueries{0};
        std::atomic<std::size_t> regionScopedRayQueries{0};

        QueryStats() noexcept = default;
        QueryStats(const QueryStats& other) noexcept;
        QueryStats& operator=(const QueryStats& other) noexcept;
        QueryStats(QueryStats&& other) noexcept;
        QueryStats& operator=(QueryStats&& other) noexcept;
        void Reset() noexcept;
    };

    [[nodiscard]] bool MatchesQuery(const SemanticStructuralPartitionEntry& entry,
                                    const SemanticStructuralPartitionQuery& query) const;
    void RebuildSideTables();
    void RebuildSubpartitions(ri::spatial::SpatialIndexOptions indexOptions);
    [[nodiscard]] const RegionSubpartition* FindSubpartition(std::string_view region) const;
    void CollectBoxCandidates(const RegionSubpartition& subpartition,
                              const ri::spatial::Aabb& box,
                              std::vector<std::size_t>& outEntryIndices) const;
    void CollectRayCandidates(const RegionSubpartition& subpartition,
                              const ri::math::Vec3& origin,
                              const ri::math::Vec3& direction,
                              float far,
                              std::vector<ri::spatial::SpatialRayCandidate>& outCandidates) const;

    std::vector<SemanticStructuralPartitionEntry> entries_;
    std::vector<RegionSubpartition> subpartitions_;
    std::unordered_map<std::string, std::size_t, detail::TransparentStringHash, std::equal_to<>>
        subpartitionIndexByRegion_;
    ri::spatial::SpatialIndexOptions indexOptions_{};
    std::size_t lastRebuildSubpartitionsReused_ = 0;
    std::size_t lastRebuildSubpartitionsRebuilt_ = 0;
    std::unordered_map<std::string, std::size_t, detail::TransparentStringHash, std::equal_to<>> entryIndexById_;
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> entryIndicesByMetadataSignature_;
    std::unordered_map<std::string, std::vector<std::size_t>, detail::TransparentStringHash, std::equal_to<>>
        entryIndicesByRegion_;
    SemanticStructuralPartitionMetrics metrics_{};
    mutable QueryStats queryStats_{};
};

/// Owner-side invalidation/rebuild cache. Unlike read-only access to a completed partition, cache mutation is
/// not internally synchronized: `GetOrRebuild`, `Invalidate`, and scene edits require one external writer.
/// References returned by `GetOrRebuild` remain valid only until that cache's next rebuild or destruction.
class SemanticStructuralPartitionCache {
public:
    [[nodiscard]] const SemanticStructuralPartition& GetOrRebuild(
        const Scene& scene,
        ri::spatial::SpatialIndexOptions indexOptions = {});
    void Invalidate() noexcept;
    [[nodiscard]] bool IsDirty() const noexcept;
    [[nodiscard]] bool NeedsRebuild(const Scene& scene) const;
    /// Includes spatial split settings in the dirty check. Use this before changing BSP options at runtime.
    [[nodiscard]] bool NeedsRebuild(const Scene& scene,
                                    ri::spatial::SpatialIndexOptions indexOptions) const;
    [[nodiscard]] std::size_t RebuildCount() const noexcept;
    [[nodiscard]] std::size_t ReuseCount() const noexcept;

private:
    SemanticStructuralPartition partition_{};
    bool dirty_ = true;
    std::size_t rebuildCount_ = 0;
    std::size_t reuseCount_ = 0;
    std::uint64_t sceneSignature_ = 0;
    ri::spatial::SpatialIndexOptions indexOptions_{};
    bool hasIndexOptions_ = false;
};

[[nodiscard]] std::vector<SemanticStructuralPartitionEntry> BuildSemanticStructuralPartitionEntries(
    const Scene& scene);
[[nodiscard]] SemanticStructuralPartition BuildSemanticStructuralPartition(
    const Scene& scene,
    ri::spatial::SpatialIndexOptions indexOptions = {});
[[nodiscard]] std::optional<SemanticStructuralPickHit> PickSemanticStructuralBrush(
    const Scene& scene,
    const Ray& ray,
    float far,
    const SemanticStructuralPartitionQuery& query = {},
    ri::spatial::SpatialIndexOptions indexOptions = {});
/// Performs an exact mesh raycast over a prebuilt semantic partition. The partition supplies the
/// semantic broad phase and filter; the final hit comes from the source node's mesh geometry.
/// Callers that reuse a partition cache (for example editor viewport interaction) avoid rebuilding
/// spatial indices for every input event.
[[nodiscard]] std::optional<SemanticStructuralRaycastHit> RaycastSemanticStructuralPartition(
    const Scene& scene,
    const SemanticStructuralPartition& partition,
    const Ray& ray,
    float far,
    const SemanticStructuralPartitionQuery& query = {});
/// Convenience exact semantic raycast for one-off callers that do not retain a partition cache.
[[nodiscard]] std::optional<SemanticStructuralRaycastHit> RaycastSemanticStructuralBrush(
    const Scene& scene,
    const Ray& ray,
    float far,
    const SemanticStructuralPartitionQuery& query = {},
    ri::spatial::SpatialIndexOptions indexOptions = {});

} // namespace ri::scene
