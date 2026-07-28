#include "RawIron/Content/PrimitiveModelDocument.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace ri::content {
namespace {

namespace detail_scan = ri::core::detail;

[[nodiscard]] bool IsFinite(const DeclarativeVec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] DeclarativeVec3 ReadVec3(const std::string_view objectText,
                                       const DeclarativeVec3 fallback = {}) {
    return DeclarativeVec3{
        .x = static_cast<float>(detail_scan::ExtractJsonDouble(objectText, "x").value_or(fallback.x)),
        .y = static_cast<float>(detail_scan::ExtractJsonDouble(objectText, "y").value_or(fallback.y)),
        .z = static_cast<float>(detail_scan::ExtractJsonDouble(objectText, "z").value_or(fallback.z)),
    };
}

[[nodiscard]] PrimitiveModelTransform ReadTransform(const std::string_view objectText) {
    PrimitiveModelTransform transform{};
    if (const auto value = detail_scan::ExtractJsonObject(objectText, "translation")) {
        transform.translation = ReadVec3(*value);
    }
    if (const auto value = detail_scan::ExtractJsonObject(objectText, "rotationDegrees")) {
        transform.rotationDegrees = ReadVec3(*value);
    }
    if (const auto value = detail_scan::ExtractJsonObject(objectText, "scale")) {
        transform.scale = ReadVec3(*value, {1.0F, 1.0F, 1.0F});
    }
    return transform;
}

void WriteVec3(std::ostringstream& json,
               const std::string_view key,
               const DeclarativeVec3& value,
               const int indent) {
    const std::string padding(static_cast<std::size_t>(indent), ' ');
    json << padding << "\"" << key << "\": {\"x\": " << std::setprecision(9) << value.x
         << ", \"y\": " << value.y << ", \"z\": " << value.z << "}";
}

void WriteTransform(std::ostringstream& json, const PrimitiveModelTransform& transform, const int indent) {
    const std::string padding(static_cast<std::size_t>(indent), ' ');
    json << padding << "\"transform\": {\n";
    WriteVec3(json, "translation", transform.translation, indent + 2);
    json << ",\n";
    WriteVec3(json, "rotationDegrees", transform.rotationDegrees, indent + 2);
    json << ",\n";
    WriteVec3(json, "scale", transform.scale, indent + 2);
    json << "\n" << padding << "}";
}

[[nodiscard]] std::string Slugify(std::string value, const std::string_view fallback) {
    std::string slug{};
    slug.reserve(value.size());
    bool pendingUnderscore = false;
    for (const unsigned char raw : value) {
        if ((raw >= 'a' && raw <= 'z') || (raw >= '0' && raw <= '9')) {
            if (pendingUnderscore && !slug.empty()) {
                slug.push_back('_');
            }
            pendingUnderscore = false;
            slug.push_back(static_cast<char>(raw));
        } else if (raw >= 'A' && raw <= 'Z') {
            if (pendingUnderscore && !slug.empty()) {
                slug.push_back('_');
            }
            pendingUnderscore = false;
            slug.push_back(static_cast<char>(raw - 'A' + 'a'));
        } else {
            pendingUnderscore = true;
        }
    }
    return slug.empty() ? std::string(fallback) : slug;
}

template <typename Range>
[[nodiscard]] std::string UniqueId(const Range& values,
                                   std::string candidate,
                                   const std::string_view fallback) {
    const std::string base = Slugify(std::move(candidate), fallback);
    auto exists = [&values](const std::string_view probe) {
        return std::any_of(values.begin(), values.end(), [probe](const auto& value) {
            return value.id == probe;
        });
    };
    if (!exists(base)) {
        return base;
    }
    for (std::size_t suffix = 2; suffix < 100000U; ++suffix) {
        const std::string next = base + "_" + std::to_string(suffix);
        if (!exists(next)) {
            return next;
        }
    }
    return {};
}

} // namespace

PrimitiveModelDocument CreatePrimitiveModelDocument(std::string modelId, std::string displayName) {
    PrimitiveModelDocument document{};
    document.modelId = Slugify(std::move(modelId), "primitive_model");
    document.displayName = displayName.empty() ? document.modelId : std::move(displayName);
    document.groups.push_back(PrimitiveModelGroup{
        .id = "root",
        .name = "Root",
    });
    return document;
}

PrimitiveModelValidationReport ValidatePrimitiveModelDocument(const PrimitiveModelDocument& document) {
    PrimitiveModelValidationReport report{};
    if (document.formatVersion != PrimitiveModelDocument::kFormatVersion) {
        report.errors.push_back("Primitive model formatVersion is unsupported.");
    }
    if (document.modelId.empty()) {
        report.errors.push_back("Primitive model id must be non-empty.");
    }
    if (document.displayName.empty()) {
        report.warnings.push_back("Primitive model display name is empty.");
    }
    if (!std::isfinite(document.bake.weldEpsilon) || document.bake.weldEpsilon <= 0.0F
        || document.bake.weldEpsilon > 0.1F) {
        report.errors.push_back("Primitive model weld epsilon must be finite and in (0, 0.1].");
    }

    std::unordered_map<std::string, std::size_t> groupById{};
    for (std::size_t index = 0; index < document.groups.size(); ++index) {
        const PrimitiveModelGroup& group = document.groups[index];
        if (group.id.empty()) {
            report.errors.push_back("Primitive model group id must be non-empty.");
        } else if (!groupById.emplace(group.id, index).second) {
            report.errors.push_back("Duplicate primitive model group id: " + group.id);
        }
        if (!IsFinite(group.transform.translation) || !IsFinite(group.transform.rotationDegrees)
            || !IsFinite(group.transform.scale)) {
            report.errors.push_back("Primitive model group has a non-finite transform: " + group.id);
        }
        if (std::abs(group.transform.scale.x) <= 1.0e-6F
            || std::abs(group.transform.scale.y) <= 1.0e-6F
            || std::abs(group.transform.scale.z) <= 1.0e-6F) {
            report.errors.push_back("Primitive model group scale cannot contain zero: " + group.id);
        }
        if (group.parentId.empty()) {
            ++report.rootGroupCount;
        }
    }
    for (const PrimitiveModelGroup& group : document.groups) {
        if (!group.parentId.empty() && !groupById.contains(group.parentId)) {
            report.errors.push_back("Primitive model group references missing parent: " + group.id
                                    + " -> " + group.parentId);
        }
        std::set<std::string> ancestry{};
        const PrimitiveModelGroup* cursor = &group;
        while (cursor != nullptr && !cursor->parentId.empty()) {
            if (!ancestry.insert(cursor->id).second) {
                report.errors.push_back("Primitive model group hierarchy contains a cycle at: " + group.id);
                break;
            }
            const auto parent = groupById.find(cursor->parentId);
            cursor = parent == groupById.end() ? nullptr : &document.groups[parent->second];
        }
    }
    if (!document.groups.empty() && report.rootGroupCount == 0U) {
        report.errors.push_back("Primitive model requires at least one root group.");
    }

    std::set<std::string> partIds{};
    for (const PrimitiveModelPart& part : document.parts) {
        if (part.id.empty()) {
            report.errors.push_back("Primitive model part id must be non-empty.");
        } else if (!partIds.insert(part.id).second) {
            report.errors.push_back("Duplicate primitive model part id: " + part.id);
        }
        if (!part.groupId.empty() && !groupById.contains(part.groupId)) {
            report.errors.push_back("Primitive model part references missing group: " + part.id
                                    + " -> " + part.groupId);
        }
        if (part.primitivePreset.empty()) {
            report.errors.push_back("Primitive model part has no primitive preset: " + part.id);
        }
        if (!IsFinite(part.transform.translation) || !IsFinite(part.transform.rotationDegrees)
            || !IsFinite(part.transform.scale)) {
            report.errors.push_back("Primitive model part has a non-finite transform: " + part.id);
        }
        if (std::abs(part.transform.scale.x) <= 1.0e-6F
            || std::abs(part.transform.scale.y) <= 1.0e-6F
            || std::abs(part.transform.scale.z) <= 1.0e-6F) {
            report.errors.push_back("Primitive model part scale cannot contain zero: " + part.id);
        }
        if (part.enabled) {
            ++report.enabledPartCount;
        }
    }
    if (report.enabledPartCount == 0U) {
        report.warnings.push_back("Primitive model contains no enabled parts.");
    }
    report.valid = report.errors.empty();
    return report;
}

std::string SerializePrimitiveModelDocument(const PrimitiveModelDocument& document) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"formatVersion\": " << document.formatVersion << ",\n";
    json << "  \"modelId\": \"" << detail_scan::EscapeJsonString(document.modelId) << "\",\n";
    json << "  \"displayName\": \"" << detail_scan::EscapeJsonString(document.displayName) << "\",\n";
    json << "  \"rigPath\": \"" << detail_scan::EscapeJsonString(document.rigPath) << "\",\n";
    json << "  \"bake\": {\"cullInternalFaces\": "
         << (document.bake.cullInternalFaces ? "true" : "false")
         << ", \"weldEpsilon\": " << std::setprecision(9) << document.bake.weldEpsilon << "},\n";
    json << "  \"groups\": [\n";
    for (std::size_t index = 0; index < document.groups.size(); ++index) {
        const PrimitiveModelGroup& group = document.groups[index];
        json << "    {\n";
        json << "      \"id\": \"" << detail_scan::EscapeJsonString(group.id) << "\",\n";
        json << "      \"name\": \"" << detail_scan::EscapeJsonString(group.name) << "\",\n";
        json << "      \"parentId\": \"" << detail_scan::EscapeJsonString(group.parentId) << "\",\n";
        json << "      \"boneName\": \"" << detail_scan::EscapeJsonString(group.boneName) << "\",\n";
        WriteTransform(json, group.transform, 6);
        json << "\n    }" << (index + 1U < document.groups.size() ? "," : "") << "\n";
    }
    json << "  ],\n";
    json << "  \"parts\": [\n";
    for (std::size_t index = 0; index < document.parts.size(); ++index) {
        const PrimitiveModelPart& part = document.parts[index];
        json << "    {\n";
        json << "      \"id\": \"" << detail_scan::EscapeJsonString(part.id) << "\",\n";
        json << "      \"name\": \"" << detail_scan::EscapeJsonString(part.name) << "\",\n";
        json << "      \"groupId\": \"" << detail_scan::EscapeJsonString(part.groupId) << "\",\n";
        json << "      \"primitivePreset\": \"" << detail_scan::EscapeJsonString(part.primitivePreset) << "\",\n";
        json << "      \"materialId\": \"" << detail_scan::EscapeJsonString(part.materialId) << "\",\n";
        json << "      \"boneName\": \"" << detail_scan::EscapeJsonString(part.boneName) << "\",\n";
        json << "      \"enabled\": " << (part.enabled ? "true" : "false") << ",\n";
        WriteTransform(json, part.transform, 6);
        json << "\n    }" << (index + 1U < document.parts.size() ? "," : "") << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

std::optional<PrimitiveModelDocument> ParsePrimitiveModelDocument(const std::string_view jsonText) {
    PrimitiveModelDocument document{};
    document.formatVersion = detail_scan::ExtractJsonInt(jsonText, "formatVersion")
                                 .value_or(PrimitiveModelDocument::kFormatVersion);
    document.modelId = detail_scan::ExtractJsonString(jsonText, "modelId").value_or("");
    document.displayName = detail_scan::ExtractJsonString(jsonText, "displayName").value_or(document.modelId);
    document.rigPath = detail_scan::ExtractJsonString(jsonText, "rigPath").value_or("");
    if (const auto bake = detail_scan::ExtractJsonObject(jsonText, "bake")) {
        document.bake.cullInternalFaces =
            detail_scan::ExtractJsonBool(*bake, "cullInternalFaces").value_or(true);
        document.bake.weldEpsilon =
            static_cast<float>(detail_scan::ExtractJsonDouble(*bake, "weldEpsilon").value_or(0.0001));
    }

    for (const std::string_view object : detail_scan::SplitJsonArrayObjects(jsonText, "groups")) {
        PrimitiveModelGroup group{};
        group.id = detail_scan::ExtractJsonString(object, "id").value_or("");
        group.name = detail_scan::ExtractJsonString(object, "name").value_or(group.id);
        group.parentId = detail_scan::ExtractJsonString(object, "parentId").value_or("");
        group.boneName = detail_scan::ExtractJsonString(object, "boneName").value_or("");
        if (const auto transform = detail_scan::ExtractJsonObject(object, "transform")) {
            group.transform = ReadTransform(*transform);
        }
        document.groups.push_back(std::move(group));
    }
    for (const std::string_view object : detail_scan::SplitJsonArrayObjects(jsonText, "parts")) {
        PrimitiveModelPart part{};
        part.id = detail_scan::ExtractJsonString(object, "id").value_or("");
        part.name = detail_scan::ExtractJsonString(object, "name").value_or(part.id);
        part.groupId = detail_scan::ExtractJsonString(object, "groupId").value_or("");
        part.primitivePreset = detail_scan::ExtractJsonString(object, "primitivePreset").value_or("box");
        part.materialId = detail_scan::ExtractJsonString(object, "materialId").value_or("default");
        part.boneName = detail_scan::ExtractJsonString(object, "boneName").value_or("");
        part.enabled = detail_scan::ExtractJsonBool(object, "enabled").value_or(true);
        if (const auto transform = detail_scan::ExtractJsonObject(object, "transform")) {
            part.transform = ReadTransform(*transform);
        }
        document.parts.push_back(std::move(part));
    }
    if (document.modelId.empty()) {
        return std::nullopt;
    }
    return document;
}

std::optional<PrimitiveModelDocument> LoadPrimitiveModelDocument(const std::filesystem::path& path) {
    const std::string json = detail_scan::ReadTextFile(path);
    return json.empty() ? std::nullopt : ParsePrimitiveModelDocument(json);
}

bool SavePrimitiveModelDocument(const std::filesystem::path& path,
                                const PrimitiveModelDocument& document) {
    if (!ValidatePrimitiveModelDocument(document).valid) {
        return false;
    }
    std::error_code error{};
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            return false;
        }
    }
    return detail_scan::WriteTextFile(path, SerializePrimitiveModelDocument(document));
}

std::string AddPrimitiveModelGroup(PrimitiveModelDocument& document,
                                   std::string name,
                                   std::string parentId,
                                   std::string boneName) {
    if (!parentId.empty()
        && std::none_of(document.groups.begin(), document.groups.end(), [&parentId](const auto& group) {
               return group.id == parentId;
           })) {
        return {};
    }
    const std::string id = UniqueId(document.groups, name, "group");
    if (id.empty()) {
        return {};
    }
    document.groups.push_back(PrimitiveModelGroup{
        .id = id,
        .name = name.empty() ? id : std::move(name),
        .parentId = std::move(parentId),
        .boneName = std::move(boneName),
    });
    return id;
}

std::string AddPrimitiveModelPart(PrimitiveModelDocument& document,
                                  std::string primitivePreset,
                                  std::string groupId,
                                  std::string name) {
    if (primitivePreset.empty()) {
        return {};
    }
    if (!groupId.empty()
        && std::none_of(document.groups.begin(), document.groups.end(), [&groupId](const auto& group) {
               return group.id == groupId;
           })) {
        return {};
    }
    if (name.empty()) {
        name = primitivePreset;
    }
    const std::string id = UniqueId(document.parts, name, "part");
    if (id.empty()) {
        return {};
    }
    document.parts.push_back(PrimitiveModelPart{
        .id = id,
        .name = std::move(name),
        .groupId = std::move(groupId),
        .primitivePreset = std::move(primitivePreset),
    });
    return id;
}

} // namespace ri::content
