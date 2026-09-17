#include "RawIron/Content/NativeSculptDocument.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace ri::content {
namespace {

namespace json = ri::core::detail;

[[nodiscard]] bool IsFinite(const ri::math::Vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

[[nodiscard]] std::optional<std::string_view> ExtractJsonArray(const std::string_view text, const std::string_view key) {
    const std::optional<std::size_t> valueIndex = json::FindJsonKey(text, key);
    if (!valueIndex.has_value()) {
        return std::nullopt;
    }
    const std::size_t index = json::SkipWhitespace(text, *valueIndex);
    if (index >= text.size() || text[index] != '[') {
        return std::nullopt;
    }
    int depth = 1;
    for (std::size_t cursor = index + 1; cursor < text.size(); ++cursor) {
        if (text[cursor] == '[') {
            ++depth;
        } else if (text[cursor] == ']') {
            --depth;
            if (depth == 0) {
                return text.substr(index, cursor - index + 1);
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::vector<float> ParseNumberArray(const std::string_view arrayText) {
    std::vector<float> values{};
    if (arrayText.size() < 2 || arrayText.front() != '[' || arrayText.back() != ']') {
        return values;
    }
    std::string_view body = arrayText.substr(1, arrayText.size() - 2);
    while (!body.empty()) {
        const std::size_t start = json::SkipWhitespace(body, 0);
        if (start >= body.size()) {
            break;
        }
        body.remove_prefix(start);
        if (body.empty() || body.front() == ',') {
            if (!body.empty()) {
                body.remove_prefix(1);
            }
            continue;
        }
        float value = 0.0f;
        const auto parsed = std::from_chars(body.data(), body.data() + body.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr == body.data()) {
            return {};
        }
        values.push_back(value);
        body.remove_prefix(static_cast<std::size_t>(parsed.ptr - body.data()));
        const std::size_t comma = json::SkipWhitespace(body, 0);
        if (comma < body.size() && body[comma] == ',') {
            body.remove_prefix(comma + 1);
        } else {
            body.remove_prefix(std::min(comma, body.size()));
        }
    }
    return values;
}

void WriteNumberArray(std::ostringstream& jsonOut, const std::string_view key, const std::vector<float>& values) {
    jsonOut << "  \"" << key << "\": [";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            jsonOut << ',';
        }
        if (index % 12U == 0U) {
            jsonOut << "\n    ";
        }
        jsonOut << values[index];
    }
    jsonOut << "\n  ]";
}

} // namespace

NativeSculptValidationReport ValidateNativeSculptDocument(const NativeSculptDocument& document) {
    NativeSculptValidationReport report{};
    report.vertexCount = document.mesh.positions.size();
    report.triangleCount = document.mesh.indices.size() / 3U;
    if (document.formatVersion != NativeSculptDocument::kFormatVersion) {
        report.errors.push_back("Unsupported sculpt formatVersion.");
    }
    if (document.id.empty()) {
        report.errors.push_back("Sculpt id is required.");
    }
    const std::string cage = LowerAscii(document.cage);
    if (cage != "sphere" && cage != "cube") {
        report.errors.push_back("Sculpt cage must be sphere or cube.");
    }
    if (document.mesh.positions.empty() || document.mesh.indices.size() < 3U) {
        report.errors.push_back("Sculpt mesh is empty.");
    }
    if (document.mesh.positions.size() > static_cast<std::size_t>(NativeSculptDocument::kMaxVertices)
        || document.mesh.indices.size() > static_cast<std::size_t>(NativeSculptDocument::kMaxIndices)) {
        report.errors.push_back("Sculpt mesh exceeds the native authoring budget.");
    }
    if (document.mesh.indices.size() % 3U != 0U) {
        report.errors.push_back("Sculpt indices must be triangles.");
    }
    if (!document.mesh.normals.empty() && document.mesh.normals.size() != document.mesh.positions.size()) {
        report.errors.push_back("Sculpt normals do not match vertex count.");
    }
    if (!document.mesh.texCoords.empty() && document.mesh.texCoords.size() != document.mesh.positions.size()) {
        report.errors.push_back("Sculpt UVs do not match vertex count.");
    }
    for (const ri::math::Vec3& position : document.mesh.positions) {
        if (!IsFinite(position)) {
            report.errors.push_back("Sculpt positions contain non-finite values.");
            break;
        }
    }
    for (const int index : document.mesh.indices) {
        if (index < 0 || index >= static_cast<int>(document.mesh.positions.size())) {
            report.errors.push_back("Sculpt indices reference missing vertices.");
            break;
        }
    }
    if (!document.vertexBoneNames.empty()
        && document.vertexBoneNames.size() != document.mesh.positions.size()) {
        report.errors.push_back("Sculpt bone weights do not match vertex count.");
    }
    if (!document.vertexInfluences.empty()
        && document.vertexInfluences.size() != document.mesh.positions.size()) {
        report.errors.push_back("Sculpt blended weights do not match vertex count.");
    }
    for (const std::vector<NativeSculptVertexInfluence>& influences : document.vertexInfluences) {
        if (influences.size() > static_cast<std::size_t>(NativeSculptDocument::kMaxInfluences)) {
            report.errors.push_back("Sculpt vertex has too many bone influences.");
            break;
        }
        for (const NativeSculptVertexInfluence& influence : influences) {
            if (!std::isfinite(influence.weight) || influence.weight < 0.0f) {
                report.errors.push_back("Sculpt influence weight is invalid.");
                break;
            }
            if (influence.weight > 0.0f && influence.boneName.empty()) {
                report.errors.push_back("Sculpt influence is missing a bone name.");
                break;
            }
        }
    }
    if (!document.rigPath.empty() && !document.mesh.positions.empty()) {
        const bool hasNames = document.vertexBoneNames.size() == document.mesh.positions.size();
        const bool hasInfluences = document.vertexInfluences.size() == document.mesh.positions.size();
        for (std::size_t vertex = 0; vertex < document.mesh.positions.size(); ++vertex) {
            const bool boundByName = hasNames && !document.vertexBoneNames[vertex].empty();
            float weightSum = 0.0f;
            if (hasInfluences) {
                for (const NativeSculptVertexInfluence& influence : document.vertexInfluences[vertex]) {
                    if (!influence.boneName.empty() && influence.weight > 0.0f && std::isfinite(influence.weight)) {
                        weightSum += influence.weight;
                    }
                }
            }
            if (boundByName || weightSum > 0.000001f) {
                if (hasInfluences && !document.vertexInfluences[vertex].empty()
                    && std::abs(weightSum - 1.0f) > 0.02f) {
                    report.warnings.push_back("Sculpt vertex weights are not normalized to 1.");
                    break;
                }
                if (hasInfluences && document.vertexInfluences[vertex].size() > 1U) {
                    ++report.blendedVertexCount;
                }
            } else {
                ++report.unboundVertexCount;
            }
        }
        if (report.unboundVertexCount > 0U) {
            report.warnings.push_back(
                "Sculpt has " + std::to_string(report.unboundVertexCount) + " unbound vertices.");
        }
    }
    report.valid = report.errors.empty();
    return report;
}

std::string SerializeNativeSculptDocument(const NativeSculptDocument& document) {
    std::vector<float> positions{};
    positions.reserve(document.mesh.positions.size() * 3U);
    for (const ri::math::Vec3& value : document.mesh.positions) {
        positions.push_back(value.x);
        positions.push_back(value.y);
        positions.push_back(value.z);
    }
    std::vector<float> normals{};
    normals.reserve(document.mesh.normals.size() * 3U);
    for (const ri::math::Vec3& value : document.mesh.normals) {
        normals.push_back(value.x);
        normals.push_back(value.y);
        normals.push_back(value.z);
    }
    std::vector<float> texCoords{};
    texCoords.reserve(document.mesh.texCoords.size() * 2U);
    for (const ri::math::Vec2& value : document.mesh.texCoords) {
        texCoords.push_back(value.x);
        texCoords.push_back(value.y);
    }
    std::vector<float> indices{};
    indices.reserve(document.mesh.indices.size());
    for (const int index : document.mesh.indices) {
        indices.push_back(static_cast<float>(index));
    }

    std::ostringstream jsonOut;
    jsonOut << std::setprecision(9);
    jsonOut << "{\n";
    jsonOut << "  \"formatVersion\": " << document.formatVersion << ",\n";
    jsonOut << "  \"id\": \"" << json::EscapeJsonString(document.id) << "\",\n";
    jsonOut << "  \"displayName\": \"" << json::EscapeJsonString(document.displayName) << "\",\n";
    jsonOut << "  \"cage\": \"" << json::EscapeJsonString(document.cage) << "\",\n";
    jsonOut << "  \"segmentsAround\": " << document.segmentsAround << ",\n";
    jsonOut << "  \"segmentsDown\": " << document.segmentsDown << ",\n";
    WriteNumberArray(jsonOut, "positions", positions);
    jsonOut << ",\n";
    WriteNumberArray(jsonOut, "normals", normals);
    jsonOut << ",\n";
    WriteNumberArray(jsonOut, "texCoords", texCoords);
    jsonOut << ",\n";
    WriteNumberArray(jsonOut, "indices", indices);
    jsonOut << ",\n";
    jsonOut << "  \"rigPath\": \"" << json::EscapeJsonString(document.rigPath) << "\"";
    if (!document.vertexBoneNames.empty()) {
        jsonOut << ",\n  \"vertexBoneNames\": [";
        for (std::size_t index = 0; index < document.vertexBoneNames.size(); ++index) {
            if (index > 0) {
                jsonOut << ',';
            }
            if (index % 8U == 0U) {
                jsonOut << "\n    ";
            }
            jsonOut << '"' << json::EscapeJsonString(document.vertexBoneNames[index]) << '"';
        }
        jsonOut << "\n  ]";
    }
    if (!document.vertexInfluences.empty()) {
        jsonOut << ",\n  \"vertexInfluences\": [\n";
        for (std::size_t vertex = 0; vertex < document.vertexInfluences.size(); ++vertex) {
            const std::vector<NativeSculptVertexInfluence>& influences = document.vertexInfluences[vertex];
            jsonOut << "    {\"bones\":[";
            for (std::size_t index = 0; index < influences.size(); ++index) {
                if (index > 0) {
                    jsonOut << ',';
                }
                jsonOut << '"' << json::EscapeJsonString(influences[index].boneName) << '"';
            }
            jsonOut << "],\"weights\":[";
            for (std::size_t index = 0; index < influences.size(); ++index) {
                if (index > 0) {
                    jsonOut << ',';
                }
                jsonOut << influences[index].weight;
            }
            jsonOut << "]}" << (vertex + 1U < document.vertexInfluences.size() ? "," : "") << "\n";
        }
        jsonOut << "  ]";
    }
    jsonOut << "\n}\n";
    return jsonOut.str();
}

std::optional<NativeSculptDocument> ParseNativeSculptDocument(const std::string_view jsonText) {
    NativeSculptDocument document{};
    document.formatVersion = json::ExtractJsonInt(jsonText, "formatVersion").value_or(0);
    document.id = json::ExtractJsonString(jsonText, "id").value_or("");
    document.displayName = json::ExtractJsonString(jsonText, "displayName").value_or(document.id);
    document.cage = json::ExtractJsonString(jsonText, "cage").value_or("sphere");
    document.segmentsAround = json::ExtractJsonInt(jsonText, "segmentsAround").value_or(32);
    document.segmentsDown = json::ExtractJsonInt(jsonText, "segmentsDown").value_or(16);

    const auto positions = ParseNumberArray(ExtractJsonArray(jsonText, "positions").value_or("[]"));
    const auto normals = ParseNumberArray(ExtractJsonArray(jsonText, "normals").value_or("[]"));
    const auto texCoords = ParseNumberArray(ExtractJsonArray(jsonText, "texCoords").value_or("[]"));
    const auto indices = ParseNumberArray(ExtractJsonArray(jsonText, "indices").value_or("[]"));
    if (positions.size() % 3U != 0U || (!normals.empty() && normals.size() % 3U != 0U)
        || (!texCoords.empty() && texCoords.size() % 2U != 0U)) {
        return std::nullopt;
    }

    document.mesh.name = document.displayName.empty() ? document.id : document.displayName;
    document.mesh.primitive = ri::scene::PrimitiveType::Custom;
    document.mesh.positions.reserve(positions.size() / 3U);
    for (std::size_t index = 0; index + 2 < positions.size(); index += 3) {
        document.mesh.positions.push_back({positions[index], positions[index + 1], positions[index + 2]});
    }
    document.mesh.normals.reserve(normals.size() / 3U);
    for (std::size_t index = 0; index + 2 < normals.size(); index += 3) {
        document.mesh.normals.push_back({normals[index], normals[index + 1], normals[index + 2]});
    }
    document.mesh.texCoords.reserve(texCoords.size() / 2U);
    for (std::size_t index = 0; index + 1 < texCoords.size(); index += 2) {
        document.mesh.texCoords.push_back({texCoords[index], texCoords[index + 1]});
    }
    document.mesh.indices.reserve(indices.size());
    for (const float index : indices) {
        if (!std::isfinite(index) || index < 0.0f || index > 10000000.0f) {
            return std::nullopt;
        }
        document.mesh.indices.push_back(static_cast<int>(index + 0.5f));
    }
    document.mesh.vertexCount = static_cast<int>(document.mesh.positions.size());
    document.mesh.indexCount = static_cast<int>(document.mesh.indices.size());
    document.rigPath = json::ExtractJsonString(jsonText, "rigPath").value_or("");
    document.vertexBoneNames = json::ExtractJsonStringArray(jsonText, "vertexBoneNames");
    for (const std::string_view influenceObject : json::SplitJsonArrayObjects(jsonText, "vertexInfluences")) {
        std::vector<NativeSculptVertexInfluence> influences{};
        const std::vector<std::string> bones = json::ExtractJsonStringArray(influenceObject, "bones");
        const std::vector<float> weights =
            ParseNumberArray(ExtractJsonArray(influenceObject, "weights").value_or("[]"));
        const std::size_t count = (std::min)(bones.size(), weights.size());
        influences.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            influences.push_back(NativeSculptVertexInfluence{
                .boneName = bones[index],
                .weight = weights[index],
            });
        }
        document.vertexInfluences.push_back(std::move(influences));
    }
    if (!ValidateNativeSculptDocument(document).valid) {
        return std::nullopt;
    }
    return document;
}

std::optional<NativeSculptDocument> LoadNativeSculptDocument(const std::filesystem::path& path) {
    const std::string text = json::ReadTextFile(path);
    if (text.empty()) {
        return std::nullopt;
    }
    return ParseNativeSculptDocument(text);
}

bool SaveNativeSculptDocument(const std::filesystem::path& path, const NativeSculptDocument& document) {
    if (!ValidateNativeSculptDocument(document).valid) {
        return false;
    }
    return json::WriteTextFile(path, SerializeNativeSculptDocument(document));
}

} // namespace ri::content
