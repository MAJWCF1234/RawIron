#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <span>
#include <unordered_map>
#include <vector>

namespace ri::content {

struct AxisAlignedBounds {
    float min[3]{};
    float max[3]{};
};

/// Level-authored trigger volume (CSV: `levels/assembly.triggers.csv`).
struct LevelTriggerVolume {
    std::string id;
    std::string eventType;
    AxisAlignedBounds bounds{};
    std::string param;
};

/// Precomputed occlusion / culling volume (`levels/assembly.occlusion.csv`).
struct OcclusionVolume {
    std::string id;
    std::string cullGroup;
    AxisAlignedBounds bounds{};
};

/// Spatial audio zones (reverb/occlusion) from `levels/assembly.audio.zones`.
struct AudioZoneRow {
    std::string id;
    std::string reverbPreset;
    float occlusionDampening = 0.0f;
    AxisAlignedBounds bounds{};
};

/// LOD culling/distances from `levels/assembly.lods.csv`.
struct LodRangeRow {
    std::string id;
    float nearDistanceMeters = 0.0f;
    float farDistanceMeters = 0.0f;
    float minScale = 1.0f;
    float maxScale = 1.0f;
};

/// Single PBR material row from `assets/materials.manifest`.
struct MaterialManifestEntry {
    std::string binding;
    std::string source;
};

/// Squad formation hints from `ai/squad.tactics`.
struct SquadTacticRow {
    std::string squadId;
    std::string formation;
    float spacingMeters = 0.0f;
};

struct GameRuntimeSupportData {
    std::unordered_map<std::string, int> streamingPrioritiesByPath;
    std::unordered_map<std::string, int> dependencyInboundCountByPath;
    std::unordered_map<std::string, int> zonePriorityByLevelId;
    std::unordered_map<std::string, std::string> lookupIndex;
    std::vector<LevelTriggerVolume> levelTriggers;
    std::vector<OcclusionVolume> occlusionVolumes;
    std::vector<AudioZoneRow> audioZones;
    std::vector<LodRangeRow> lodRanges;
    std::unordered_map<std::string, MaterialManifestEntry> materialsById;
    std::unordered_map<std::string, std::string> audioBankPathById;
    /// Key: `"<font_id>|<weight>"` from `assets/fonts.manifest`.
    std::unordered_map<std::string, std::string> fontPathByFontKey;
    std::optional<std::int32_t> saveSchemaVersion;
    /// Achievement id -> platform key (e.g. steam) -> external api id.
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> achievementIdsByPlatform;
    std::unordered_map<std::string, std::string> perceptionScalars;
    std::vector<SquadTacticRow> squadTactics;
};

struct LevelPreloadEntry {
    std::string payloadPath;
    int priorityScore = 0;
};

struct LevelPreloadPlan {
    std::vector<LevelPreloadEntry> entries;
    std::set<std::string, std::less<>> pendingPayloads;
    std::unordered_map<std::string, int> cachedPriorityScores;
};

[[nodiscard]] GameRuntimeSupportData LoadGameRuntimeSupportData(const std::filesystem::path& gameRoot);

/// Returns a stable priority score used to sort authored resources for early streaming / review.
/// Higher score means earlier in the list.
[[nodiscard]] int ComputeResourcePriorityScore(const GameRuntimeSupportData& supportData,
                                              std::string_view relativeResourcePath);

[[nodiscard]] std::string ResolveLookupValueOr(std::string_view key,
                                              std::string_view fallback,
                                              const GameRuntimeSupportData& supportData);

[[nodiscard]] bool PointInsideAxisAlignedBounds(float x, float y, float z, const AxisAlignedBounds& bounds);

/// Resolves an external achievement/trophy id for a storefront key (`steam`, `xbox`, `psn`, `generic`).
[[nodiscard]] std::optional<std::string> ResolveAchievementExternalId(std::string_view achievementId,
                                                                     std::string_view platformKey,
                                                                     const GameRuntimeSupportData& data);

[[nodiscard]] std::optional<std::string_view> TryGetPerceptionScalar(std::string_view key,
                                                                     const GameRuntimeSupportData& data);

[[nodiscard]] const MaterialManifestEntry* TryGetMaterialEntry(std::string_view materialId,
                                                               const GameRuntimeSupportData& data);

[[nodiscard]] std::optional<std::string_view> TryGetAudioBankPath(std::string_view bankId,
                                                                const GameRuntimeSupportData& data);

[[nodiscard]] std::optional<std::string_view> TryGetFontPath(std::string_view fontId,
                                                             std::string_view weight,
                                                             const GameRuntimeSupportData& data);

/// Returns the authored audio zone containing the point, preferring the smallest matching volume.
[[nodiscard]] const AudioZoneRow* FindAudioZoneAtPoint(float x,
                                                       float y,
                                                       float z,
                                                       const GameRuntimeSupportData& data);

/// Resolves LOD scale from a distance sample using authored near/far and min/max scale.
/// Returns 1.0f when no matching lod id exists.
[[nodiscard]] float ComputeLodScaleForDistance(std::string_view lodId,
                                               float distanceMeters,
                                               const GameRuntimeSupportData& data);

[[nodiscard]] std::vector<std::string> ExpandPreloadLevelPayloads(std::span<const std::string_view> levelIds,
                                                                  const GameRuntimeSupportData& data);
[[nodiscard]] LevelPreloadPlan BuildLevelPreloadPlan(std::span<const std::string_view> levelIds,
                                                     const GameRuntimeSupportData& data);
void MarkPreloadPayloadReady(LevelPreloadPlan& plan, std::string_view payloadPath);

} // namespace ri::content
