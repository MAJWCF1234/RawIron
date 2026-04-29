#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::content::pipeline {

/// Pipeline bookkeeping for archived source assets and extracted outputs (not a runtime gameplay asset).
enum class ExtractionStatus : std::uint8_t {
    Pending = 0,
    Complete = 1,
    Failed = 2,
};

struct ExtractedOutputEntry {
    std::string relativePath;
    std::uint64_t sizeBytes = 0;
    /// Opaque UTC timestamp string (typically ISO-8601) from the authoring tool.
    std::string modifiedUtc;
};

struct ArchiveInventoryEntry {
    std::string path;
    std::string identifier;
    /// Hash or signature string from the pipeline (algorithm-specific).
    std::string signature;
    ExtractionStatus extractionStatus = ExtractionStatus::Pending;
    bool extractedOutputCurrent = false;
    std::vector<ExtractedOutputEntry> extractedOutputs;
};

struct AssetExtractionInventory {
    static constexpr int kFormatVersion = 1;

    int formatVersion = kFormatVersion;
    std::string generatedAtUtc;
    std::vector<ArchiveInventoryEntry> archives;
};

struct ArchiveExtractionCacheDecision {
    bool shouldExtract = true;
    bool signatureChanged = true;
    bool missingOutputs = true;
    std::string reason;
};

[[nodiscard]] std::string SerializeAssetExtractionInventory(const AssetExtractionInventory& inventory);

[[nodiscard]] std::optional<AssetExtractionInventory> ParseAssetExtractionInventory(std::string_view jsonText);

[[nodiscard]] std::optional<AssetExtractionInventory> LoadAssetExtractionInventory(const std::filesystem::path& path);

[[nodiscard]] bool SaveAssetExtractionInventory(const std::filesystem::path& path,
                                                  const AssetExtractionInventory& inventory);

[[nodiscard]] const ArchiveInventoryEntry* FindArchiveInventoryEntry(const AssetExtractionInventory& inventory,
                                                                     std::string_view identifier) noexcept;

[[nodiscard]] ArchiveExtractionCacheDecision EvaluateArchiveExtractionCache(
    const AssetExtractionInventory& inventory,
    std::string_view identifier,
    std::string_view currentSignature) noexcept;

void UpsertArchiveInventoryEntry(AssetExtractionInventory& inventory, ArchiveInventoryEntry entry);

} // namespace ri::content::pipeline
