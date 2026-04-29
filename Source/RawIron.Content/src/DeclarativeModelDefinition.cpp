#include "RawIron/Content/DeclarativeModelDefinition.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <iomanip>
#include <sstream>
#include <utility>

namespace ri::content {
namespace {

namespace detail_scan = ri::core::detail;

[[nodiscard]] DeclarativeVec3 ReadVec3(const std::string_view objectText) {
    DeclarativeVec3 result{};
    result.x = static_cast<float>(detail_scan::ExtractJsonDouble(objectText, "x").value_or(0.0));
    result.y = static_cast<float>(detail_scan::ExtractJsonDouble(objectText, "y").value_or(0.0));
    result.z = static_cast<float>(detail_scan::ExtractJsonDouble(objectText, "z").value_or(0.0));
    return result;
}

[[nodiscard]] DeclarativeQuat ReadQuat(const std::string_view objectText) {
    DeclarativeQuat result{};
    result.x = static_cast<float>(detail_scan::ExtractJsonDouble(objectText, "x").value_or(0.0));
    result.y = static_cast<float>(detail_scan::ExtractJsonDouble(objectText, "y").value_or(0.0));
    result.z = static_cast<float>(detail_scan::ExtractJsonDouble(objectText, "z").value_or(0.0));
    result.w = static_cast<float>(detail_scan::ExtractJsonDouble(objectText, "w").value_or(1.0));
    return result;
}

[[nodiscard]] DeclarativeModelPart ParsePart(const std::string_view partText) {
    DeclarativeModelPart part{};
    part.name = detail_scan::ExtractJsonString(partText, "name").value_or("");
    part.parentName = detail_scan::ExtractJsonString(partText, "parentName").value_or("");
    part.meshId = detail_scan::ExtractJsonString(partText, "meshId").value_or("");
    part.materialId = detail_scan::ExtractJsonString(partText, "materialId").value_or("");
    part.tags = detail_scan::ExtractJsonStringArray(partText, "tags");

    if (const std::optional<std::string_view> translationObject = detail_scan::ExtractJsonObject(partText, "translation")) {
        part.translation = ReadVec3(*translationObject);
    }
    if (const std::optional<std::string_view> rotationObject = detail_scan::ExtractJsonObject(partText, "rotation")) {
        part.rotation = ReadQuat(*rotationObject);
    }
    if (const std::optional<std::string_view> scaleObject = detail_scan::ExtractJsonObject(partText, "scale")) {
        part.scale = ReadVec3(*scaleObject);
    }

    return part;
}

[[nodiscard]] std::string FormatFloat(const float value) {
    std::ostringstream stream;
    stream << std::setprecision(9) << std::defaultfloat << value;
    return stream.str();
}

void AppendVec3Object(std::ostringstream& json, const std::string_view key, const DeclarativeVec3& vector) {
    json << "    \"" << key << "\": {\n";
    json << "      \"x\": " << FormatFloat(vector.x) << ",\n";
    json << "      \"y\": " << FormatFloat(vector.y) << ",\n";
    json << "      \"z\": " << FormatFloat(vector.z) << "\n";
    json << "    }";
}

void AppendQuatObject(std::ostringstream& json, const DeclarativeQuat& quaternion) {
    json << "    \"rotation\": {\n";
    json << "      \"x\": " << FormatFloat(quaternion.x) << ",\n";
    json << "      \"y\": " << FormatFloat(quaternion.y) << ",\n";
    json << "      \"z\": " << FormatFloat(quaternion.z) << ",\n";
    json << "      \"w\": " << FormatFloat(quaternion.w) << "\n";
    json << "    }";
}

} // namespace

std::string SerializeDeclarativeModelDefinition(const DeclarativeModelDefinition& model) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"formatVersion\": " << model.formatVersion << ",\n";
    json << "  \"modelId\": \"" << detail_scan::EscapeJsonString(model.modelId) << "\",\n";
    json << "  \"parts\": [\n";

    for (std::size_t partIndex = 0; partIndex < model.parts.size(); ++partIndex) {
        const DeclarativeModelPart& part = model.parts[partIndex];
        json << "    {\n";
        json << "      \"name\": \"" << detail_scan::EscapeJsonString(part.name) << "\",\n";
        json << "      \"parentName\": \"" << detail_scan::EscapeJsonString(part.parentName) << "\",\n";
        json << "      \"meshId\": \"" << detail_scan::EscapeJsonString(part.meshId) << "\",\n";
        json << "      \"materialId\": \"" << detail_scan::EscapeJsonString(part.materialId) << "\",\n";
        AppendVec3Object(json, "translation", part.translation);
        json << ",\n";
        AppendQuatObject(json, part.rotation);
        json << ",\n";
        AppendVec3Object(json, "scale", part.scale);
        json << ",\n";
        json << "      \"tags\": [";
        for (std::size_t tagIndex = 0; tagIndex < part.tags.size(); ++tagIndex) {
            json << "\"" << detail_scan::EscapeJsonString(part.tags[tagIndex]) << "\"";
            if (tagIndex + 1U < part.tags.size()) {
                json << ", ";
            }
        }
        json << "]\n";
        json << "    }";
        if (partIndex + 1U < model.parts.size()) {
            json << ",";
        }
        json << "\n";
    }

    json << "  ]\n";
    json << "}\n";
    return json.str();
}

std::optional<DeclarativeModelDefinition> ParseDeclarativeModelDefinition(const std::string_view jsonText) {
    DeclarativeModelDefinition model{};
    model.formatVersion = detail_scan::ExtractJsonInt(jsonText, "formatVersion").value_or(DeclarativeModelDefinition::kFormatVersion);
    model.modelId = detail_scan::ExtractJsonString(jsonText, "modelId").value_or("");

    const std::vector<std::string_view> partObjects = detail_scan::SplitJsonArrayObjects(jsonText, "parts");
    model.parts.reserve(partObjects.size());
    for (const std::string_view partText : partObjects) {
        model.parts.push_back(ParsePart(partText));
    }

    return model;
}

std::optional<DeclarativeModelDefinition> LoadDeclarativeModelDefinition(const std::filesystem::path& path) {
    const std::string text = detail_scan::ReadTextFile(path);
    if (text.empty()) {
        return std::nullopt;
    }
    return ParseDeclarativeModelDefinition(text);
}

bool SaveDeclarativeModelDefinition(const std::filesystem::path& path, const DeclarativeModelDefinition& model) {
    return detail_scan::WriteTextFile(path, SerializeDeclarativeModelDefinition(model));
}

} // namespace ri::content
