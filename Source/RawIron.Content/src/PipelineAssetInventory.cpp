#include "RawIron/Content/Pipeline/AssetExtractionInventory.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <sstream>
#include <utility>

namespace ri::content::pipeline {
namespace {

namespace detail_scan = ri::core::detail;

[[nodiscard]] const char* StatusToString(const ExtractionStatus status) noexcept {
    switch (status) {
        case ExtractionStatus::Complete:
            return "complete";
        case ExtractionStatus::Failed:
            return "failed";
        case ExtractionStatus::Pending:
        default:
            return "pending";
    }
}

[[nodiscard]] ExtractionStatus ParseStatus(const std::string_view text) noexcept {
    if (text == "complete") {
        return ExtractionStatus::Complete;
    }
    if (text == "failed") {
        return ExtractionStatus::Failed;
    }
    return ExtractionStatus::Pending;
}

} // namespace

std::string SerializeAssetExtractionInventory(const AssetExtractionInventory& inventory) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"formatVersion\": " << inventory.formatVersion << ",\n";
    json << "  \"generatedAtUtc\": \"" << detail_scan::EscapeJsonString(inventory.generatedAtUtc) << "\",\n";
    json << "  \"archives\": [\n";
    for (std::size_t archiveIndex = 0; archiveIndex < inventory.archives.size(); ++archiveIndex) {
        const ArchiveInventoryEntry& archive = inventory.archives[archiveIndex];
        json << "    {\n";
        json << "      \"path\": \"" << detail_scan::EscapeJsonString(archive.path) << "\",\n";
        json << "      \"identifier\": \"" << detail_scan::EscapeJsonString(archive.identifier) << "\",\n";
        json << "      \"signature\": \"" << detail_scan::EscapeJsonString(archive.signature) << "\",\n";
        json << "      \"extractionStatus\": \"" << StatusToString(archive.extractionStatus) << "\",\n";
        json << "      \"extractedOutputCurrent\": " << (archive.extractedOutputCurrent ? "true" : "false") << ",\n";
        json << "      \"extractedOutputs\": [\n";
        for (std::size_t outputIndex = 0; outputIndex < archive.extractedOutputs.size(); ++outputIndex) {
            const ExtractedOutputEntry& output = archive.extractedOutputs[outputIndex];
            json << "        {\n";
            json << "          \"relativePath\": \"" << detail_scan::EscapeJsonString(output.relativePath) << "\",\n";
            json << "          \"sizeBytes\": " << output.sizeBytes << ",\n";
            json << "          \"modifiedUtc\": \"" << detail_scan::EscapeJsonString(output.modifiedUtc) << "\"\n";
            json << "        }";
            if (outputIndex + 1U < archive.extractedOutputs.size()) {
                json << ",";
            }
            json << "\n";
        }
        json << "      ]\n";
        json << "    }";
        if (archiveIndex + 1U < inventory.archives.size()) {
            json << ",";
        }
        json << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

std::optional<AssetExtractionInventory> ParseAssetExtractionInventory(const std::string_view jsonText) {
    AssetExtractionInventory inventory{};
    inventory.formatVersion = detail_scan::ExtractJsonInt(jsonText, "formatVersion").value_or(AssetExtractionInventory::kFormatVersion);
    inventory.generatedAtUtc = detail_scan::ExtractJsonString(jsonText, "generatedAtUtc").value_or("");

    const std::vector<std::string_view> archiveObjects = detail_scan::SplitJsonArrayObjects(jsonText, "archives");
    inventory.archives.reserve(archiveObjects.size());

    for (const std::string_view archiveText : archiveObjects) {
        ArchiveInventoryEntry entry{};
        entry.path = detail_scan::ExtractJsonString(archiveText, "path").value_or("");
        entry.identifier = detail_scan::ExtractJsonString(archiveText, "identifier").value_or("");
        entry.signature = detail_scan::ExtractJsonString(archiveText, "signature").value_or("");
        entry.extractionStatus = ParseStatus(detail_scan::ExtractJsonString(archiveText, "extractionStatus").value_or(""));
        entry.extractedOutputCurrent = detail_scan::ExtractJsonBool(archiveText, "extractedOutputCurrent").value_or(false);

        const std::vector<std::string_view> outputObjects = detail_scan::SplitJsonArrayObjects(archiveText, "extractedOutputs");
        entry.extractedOutputs.reserve(outputObjects.size());
        for (const std::string_view outputText : outputObjects) {
            ExtractedOutputEntry output{};
            output.relativePath = detail_scan::ExtractJsonString(outputText, "relativePath").value_or("");
            output.sizeBytes = detail_scan::ExtractJsonUInt64(outputText, "sizeBytes").value_or(0ULL);
            output.modifiedUtc = detail_scan::ExtractJsonString(outputText, "modifiedUtc").value_or("");
            entry.extractedOutputs.push_back(std::move(output));
        }

        inventory.archives.push_back(std::move(entry));
    }

    return inventory;
}

std::optional<AssetExtractionInventory> LoadAssetExtractionInventory(const std::filesystem::path& path) {
    const std::string text = detail_scan::ReadTextFile(path);
    if (text.empty()) {
        return std::nullopt;
    }
    return ParseAssetExtractionInventory(text);
}

bool SaveAssetExtractionInventory(const std::filesystem::path& path, const AssetExtractionInventory& inventory) {
    return detail_scan::WriteTextFile(path, SerializeAssetExtractionInventory(inventory));
}

} // namespace ri::content::pipeline
