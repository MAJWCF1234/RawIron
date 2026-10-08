#include "RawIron/Content/BlockCharacterDocument.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace ri::content {
namespace {

namespace detail_scan = ri::core::detail;

[[nodiscard]] std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

[[nodiscard]] bool IsFinite(const DeclarativeVec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] DeclarativeVec3 ReadVec3(const std::string_view objectText, const DeclarativeVec3 fallback = {}) {
    return DeclarativeVec3{
        .x = static_cast<float>(detail_scan::ExtractJsonDouble(objectText, "x").value_or(fallback.x)),
        .y = static_cast<float>(detail_scan::ExtractJsonDouble(objectText, "y").value_or(fallback.y)),
        .z = static_cast<float>(detail_scan::ExtractJsonDouble(objectText, "z").value_or(fallback.z)),
    };
}

void WriteVec3(std::ostringstream& json, const std::string_view key, const DeclarativeVec3& value, const int indent) {
    const std::string padding(static_cast<std::size_t>(indent), ' ');
    json << padding << "\"" << key << "\": {\"x\": " << std::setprecision(9) << value.x
         << ", \"y\": " << value.y << ", \"z\": " << value.z << "}";
}

[[nodiscard]] bool IsKnownShape(const std::string& shape) {
    const std::string lower = LowerAscii(shape);
    return lower == "box" || lower == "prism" || lower == "dome" || lower == "wedge"
        || lower == "cylinder" || lower == "cyl" || lower == "spike" || lower == "pyramid"
        || lower == "slab" || lower == "bevel" || lower == "chamfer" || lower == "capsule";
}

} // namespace

bool IsBlockCharacterPath(const std::filesystem::path& path) {
    return LowerAscii(path.filename().string()).ends_with(".ri_blockchar.json");
}

BlockCharacterValidationReport ValidateBlockCharacterDocument(const BlockCharacterDocument& document) {
    BlockCharacterValidationReport report{};
    report.partCount = document.parts.size();
    report.enabledPartCount = document.parts.size();
    if (document.formatVersion != BlockCharacterDocument::kFormatVersion) {
        report.errors.push_back("Unsupported block character formatVersion.");
    }
    if (document.id.empty()) {
        report.errors.push_back("Block character id is required.");
    }
    if (document.parts.empty()) {
        report.warnings.push_back("Block character has no parts yet.");
    }
    for (std::size_t i = 0; i < document.parts.size(); ++i) {
        const BlockCharacterPart& part = document.parts[i];
        const std::string prefix = "parts[" + std::to_string(i) + "]";
        if (part.boneName.empty()) {
            report.errors.push_back(prefix + " boneName is required.");
        }
        if (!IsKnownShape(part.shape)) {
            report.errors.push_back(
                prefix
                + " shape must be box, prism, dome, wedge, cylinder, spike, slab, bevel, or capsule.");
        }
        if (part.sides < 0 || part.sides > 24) {
            report.errors.push_back(prefix + " sides must be 0..24.");
        }
        if (part.bevel < 0.0F || part.bevel > 0.45F) {
            report.errors.push_back(prefix + " bevel must be 0..0.45.");
        }
        if (!IsFinite(part.center) || !IsFinite(part.halfExtent) || !IsFinite(part.halfExtentTop)
            || !IsFinite(part.rotationDegrees) || !IsFinite(part.albedoColor)) {
            report.errors.push_back(prefix + " has a non-finite vector.");
        }
        if (part.halfExtent.x <= 0.0F || part.halfExtent.y <= 0.0F || part.halfExtent.z <= 0.0F) {
            report.errors.push_back(prefix + " halfExtent must be positive.");
        }
    }
    report.valid = report.errors.empty();
    return report;
}

std::string SerializeBlockCharacterDocument(const BlockCharacterDocument& document) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"formatVersion\": " << document.formatVersion << ",\n";
    json << "  \"id\": \"" << detail_scan::EscapeJsonString(document.id) << "\",\n";
    json << "  \"displayName\": \"" << detail_scan::EscapeJsonString(document.displayName) << "\",\n";
    json << "  \"rigPath\": \"" << detail_scan::EscapeJsonString(document.rigPath) << "\",\n";
    json << "  \"parts\": [\n";
    for (std::size_t i = 0; i < document.parts.size(); ++i) {
        const BlockCharacterPart& part = document.parts[i];
        json << "    {\n";
        json << "      \"id\": \"" << detail_scan::EscapeJsonString(part.id) << "\",\n";
        json << "      \"name\": \"" << detail_scan::EscapeJsonString(part.name) << "\",\n";
        json << "      \"boneName\": \"" << detail_scan::EscapeJsonString(part.boneName) << "\",\n";
        json << "      \"shape\": \"" << detail_scan::EscapeJsonString(part.shape) << "\",\n";
        WriteVec3(json, "center", part.center, 6);
        json << ",\n";
        WriteVec3(json, "halfExtent", part.halfExtent, 6);
        json << ",\n";
        WriteVec3(json, "halfExtentTop", part.halfExtentTop, 6);
        json << ",\n";
        WriteVec3(json, "rotationDegrees", part.rotationDegrees, 6);
        json << ",\n";
        WriteVec3(json, "albedoColor", part.albedoColor, 6);
        json << ",\n";
        json << "      \"roughness\": " << std::setprecision(9) << part.roughness << ",\n";
        json << "      \"metallic\": " << part.metallic << ",\n";
        json << "      \"sides\": " << part.sides << ",\n";
        json << "      \"bevel\": " << part.bevel << ",\n";
        json << "      \"albedoTexture\": \"" << detail_scan::EscapeJsonString(part.albedoTexture) << "\"\n";
        json << "    }" << (i + 1U < document.parts.size() ? "," : "") << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

std::optional<BlockCharacterDocument> ParseBlockCharacterDocument(const std::string_view jsonText) {
    BlockCharacterDocument document{};
    document.formatVersion = static_cast<int>(
        detail_scan::ExtractJsonDouble(jsonText, "formatVersion").value_or(1.0));
    document.id = detail_scan::ExtractJsonString(jsonText, "id").value_or("");
    document.displayName = detail_scan::ExtractJsonString(jsonText, "displayName").value_or(document.id);
    document.rigPath = detail_scan::ExtractJsonString(jsonText, "rigPath").value_or("");
    for (const std::string_view object : detail_scan::SplitJsonArrayObjects(jsonText, "parts")) {
        BlockCharacterPart part{};
        part.id = detail_scan::ExtractJsonString(object, "id").value_or("");
        part.name = detail_scan::ExtractJsonString(object, "name").value_or(part.id);
        part.boneName = detail_scan::ExtractJsonString(object, "boneName").value_or("");
        part.shape = detail_scan::ExtractJsonString(object, "shape").value_or("box");
        if (const auto center = detail_scan::ExtractJsonObject(object, "center")) {
            part.center = ReadVec3(*center);
        }
        if (const auto half = detail_scan::ExtractJsonObject(object, "halfExtent")) {
            part.halfExtent = ReadVec3(*half, {0.1F, 0.1F, 0.1F});
        }
        if (const auto top = detail_scan::ExtractJsonObject(object, "halfExtentTop")) {
            part.halfExtentTop = ReadVec3(*top);
        }
        if (const auto rot = detail_scan::ExtractJsonObject(object, "rotationDegrees")) {
            part.rotationDegrees = ReadVec3(*rot);
        }
        if (const auto color = detail_scan::ExtractJsonObject(object, "albedoColor")) {
            part.albedoColor = ReadVec3(*color, {0.62F, 0.55F, 0.46F});
        }
        part.roughness = static_cast<float>(
            detail_scan::ExtractJsonDouble(object, "roughness").value_or(0.88));
        part.metallic = static_cast<float>(
            detail_scan::ExtractJsonDouble(object, "metallic").value_or(0.0));
        part.sides = static_cast<int>(detail_scan::ExtractJsonDouble(object, "sides").value_or(0.0));
        part.bevel = static_cast<float>(detail_scan::ExtractJsonDouble(object, "bevel").value_or(0.0));
        part.albedoTexture = detail_scan::ExtractJsonString(object, "albedoTexture").value_or("");
        if (part.id.empty()) {
            part.id = "part_" + std::to_string(document.parts.size());
        }
        if (part.name.empty()) {
            part.name = part.id;
        }
        document.parts.push_back(std::move(part));
    }
    return document;
}

std::optional<BlockCharacterDocument> LoadBlockCharacterDocument(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return ParseBlockCharacterDocument(buffer.str());
}

bool SaveBlockCharacterDocument(const std::filesystem::path& path, const BlockCharacterDocument& document) {
    return detail_scan::WriteTextFile(path, SerializeBlockCharacterDocument(document));
}

} // namespace ri::content
