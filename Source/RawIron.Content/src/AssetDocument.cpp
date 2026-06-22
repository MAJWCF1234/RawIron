#include "RawIron/Content/AssetDocument.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <initializer_list>
#include <sstream>
#include <utility>

namespace ri::content {

namespace detail_scan = ri::core::detail;

namespace {

std::optional<std::string> ExtractFirstJsonString(std::string_view text,
                                                  std::initializer_list<std::string_view> keys) {
    for (const std::string_view key : keys) {
        if (std::optional<std::string> value = detail_scan::ExtractJsonString(text, key)) {
            return value;
        }
    }
    return std::nullopt;
}

} // namespace

std::string SerializeAssetDocument(const AssetDocument& document) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"formatVersion\": " << document.formatVersion << ",\n";
    json << "  \"id\": \"" << detail_scan::EscapeJsonString(document.id) << "\",\n";
    json << "  \"type\": \"" << detail_scan::EscapeJsonString(document.type) << "\",\n";
    json << "  \"displayName\": \"" << detail_scan::EscapeJsonString(document.displayName) << "\",\n";
    json << "  \"sourcePath\": \"" << detail_scan::EscapeJsonString(document.sourcePath) << "\",\n";
    json << "  \"references\": [\n";
    for (std::size_t index = 0; index < document.references.size(); ++index) {
        const AssetReference& reference = document.references[index];
        json << "    {\n";
        json << "      \"kind\": \"" << detail_scan::EscapeJsonString(reference.kind) << "\",\n";
        json << "      \"id\": \"" << detail_scan::EscapeJsonString(reference.id) << "\",\n";
        json << "      \"path\": \"" << detail_scan::EscapeJsonString(reference.path) << "\"\n";
        json << "    }";
        if (index + 1U < document.references.size()) {
            json << ",";
        }
        json << "\n";
    }
    json << "  ],\n";
    json << "  \"payload\": " << (document.payloadJson.empty() ? "{}" : document.payloadJson) << "\n";
    json << "}\n";
    return json.str();
}

std::optional<AssetDocument> ParseAssetDocument(std::string_view jsonText) {
    AssetDocument out{};
    out.formatVersion = detail_scan::ExtractJsonInt(jsonText, "formatVersion").value_or(AssetDocument::kFormatVersion);
    out.id = ExtractFirstJsonString(jsonText, {"id", "assetId", "asset_id"}).value_or("");
    out.type = ExtractFirstJsonString(jsonText, {"type", "assetType", "asset_type", "kind"}).value_or("");
    out.displayName = ExtractFirstJsonString(jsonText, {"displayName", "name", "title"}).value_or("");
    out.sourcePath = ExtractFirstJsonString(jsonText, {"sourcePath", "source", "sourceUri", "sourceURI"}).value_or("");
    if (const std::optional<std::string_view> payload = detail_scan::ExtractJsonObject(jsonText, "payload")) {
        out.payloadJson = std::string(*payload);
    } else if (const std::optional<std::string_view> metadata = detail_scan::ExtractJsonObject(jsonText, "metadata")) {
        out.payloadJson = std::string(*metadata);
    }

    const std::vector<std::string_view> referenceObjects = detail_scan::SplitJsonArrayObjects(jsonText, "references");
    out.references.reserve(referenceObjects.size());
    for (const std::string_view object : referenceObjects) {
        AssetReference reference{};
        reference.kind = ExtractFirstJsonString(object, {"kind", "type"}).value_or("");
        reference.id = ExtractFirstJsonString(object, {"id", "assetId", "asset_id"}).value_or("");
        reference.path = ExtractFirstJsonString(object, {"path", "uri", "sourcePath", "source"}).value_or("");
        out.references.push_back(std::move(reference));
    }

    if (out.id.empty() || out.type.empty()) {
        return std::nullopt;
    }
    return out;
}

std::optional<AssetDocument> LoadAssetDocument(const std::filesystem::path& path) {
    const std::string text = detail_scan::ReadTextFile(path);
    if (text.empty()) {
        return std::nullopt;
    }
    return ParseAssetDocument(text);
}

bool SaveAssetDocument(const std::filesystem::path& path, const AssetDocument& document) {
    return detail_scan::WriteTextFile(path, SerializeAssetDocument(document));
}

} // namespace ri::content
