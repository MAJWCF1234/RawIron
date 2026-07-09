#include "RawIron/Scene/RigAuthoring.h"

#include "RawIron/Core/Detail/JsonScan.h"
#include "RawIron/Scene/HumanoidRigNames.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace ri::scene {
namespace {

namespace json = ri::core::detail;

std::string NormalizeBoneName(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const char character : value) {
        if (std::isalnum(static_cast<unsigned char>(character)) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
        }
    }
    return normalized;
}

bool IsFinite(const ri::math::Vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

std::string Escape(std::string_view value) {
    return json::EscapeJsonString(value);
}

void WriteVec3Json(std::ostringstream& stream, const ri::math::Vec3& value) {
    stream << "{\"x\":" << value.x << ",\"y\":" << value.y << ",\"z\":" << value.z << '}';
}

std::optional<ri::math::Vec3> ParseVec3(std::string_view object) {
    const std::optional<double> x = json::ExtractJsonDouble(object, "x");
    const std::optional<double> y = json::ExtractJsonDouble(object, "y");
    const std::optional<double> z = json::ExtractJsonDouble(object, "z");
    if (!x.has_value() || !y.has_value() || !z.has_value()) {
        return std::nullopt;
    }
    return ri::math::Vec3{static_cast<float>(*x), static_cast<float>(*y), static_cast<float>(*z)};
}

std::optional<Transform> ParseRestTransform(std::string_view boneObject) {
    const std::optional<std::string_view> rest = json::ExtractJsonObject(boneObject, "restLocal");
    if (!rest.has_value()) {
        return std::nullopt;
    }
    const std::optional<std::string_view> position = json::ExtractJsonObject(*rest, "position");
    const std::optional<std::string_view> rotation = json::ExtractJsonObject(*rest, "rotationDegrees");
    const std::optional<std::string_view> scale = json::ExtractJsonObject(*rest, "scale");
    if (!position.has_value() || !rotation.has_value() || !scale.has_value()) {
        return std::nullopt;
    }
    const std::optional<ri::math::Vec3> parsedPosition = ParseVec3(*position);
    const std::optional<ri::math::Vec3> parsedRotation = ParseVec3(*rotation);
    const std::optional<ri::math::Vec3> parsedScale = ParseVec3(*scale);
    if (!parsedPosition.has_value() || !parsedRotation.has_value() || !parsedScale.has_value()) {
        return std::nullopt;
    }
    return Transform{.position = *parsedPosition, .rotationDegrees = *parsedRotation, .scale = *parsedScale};
}

void AddBone(RigDefinition& rig,
             std::string name,
             int parentIndex,
             ri::math::Vec3 position,
             bool deform = true) {
    rig.bones.push_back(RigBone{
        .name = std::move(name),
        .parentIndex = parentIndex,
        .restLocal = Transform{.position = position},
        .deform = deform,
    });
}

} // namespace

std::string RigProfileName(const RigProfile profile) {
    switch (profile) {
        case RigProfile::Generic: return "generic";
        case RigProfile::Humanoid: return "humanoid";
    }
    return "generic";
}

std::optional<RigProfile> ParseRigProfile(const std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const char character : value) {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }
    if (normalized == "generic") {
        return RigProfile::Generic;
    }
    if (normalized == "humanoid") {
        return RigProfile::Humanoid;
    }
    return std::nullopt;
}

RigDefinition CreateHumanoidRigDefinition(std::string id, std::string displayName) {
    if (displayName.empty()) {
        displayName = id.empty() ? "RawIron Humanoid" : id;
    }

    RigDefinition rig{};
    rig.id = std::move(id);
    rig.displayName = std::move(displayName);
    rig.profile = RigProfile::Humanoid;

    AddBone(rig, "root", -1, {0.0f, 0.0f, 0.0f}, false);
    AddBone(rig, "pelvis", 0, {0.0f, 1.00f, 0.0f});
    AddBone(rig, "spine", 1, {0.0f, 0.18f, 0.0f});
    AddBone(rig, "chest", 2, {0.0f, 0.20f, 0.0f});
    AddBone(rig, "neck", 3, {0.0f, 0.24f, 0.0f});
    AddBone(rig, "head", 4, {0.0f, 0.18f, 0.0f});

    AddBone(rig, "left_clavicle", 3, {-0.14f, 0.17f, 0.0f});
    AddBone(rig, "left_upper_arm", 6, {-0.22f, 0.0f, 0.0f});
    AddBone(rig, "left_lower_arm", 7, {-0.28f, 0.0f, 0.0f});
    AddBone(rig, "left_hand", 8, {-0.24f, 0.0f, 0.0f});
    AddBone(rig, "right_clavicle", 3, {0.14f, 0.17f, 0.0f});
    AddBone(rig, "right_upper_arm", 10, {0.22f, 0.0f, 0.0f});
    AddBone(rig, "right_lower_arm", 11, {0.28f, 0.0f, 0.0f});
    AddBone(rig, "right_hand", 12, {0.24f, 0.0f, 0.0f});

    AddBone(rig, "left_upper_leg", 1, {-0.12f, -0.24f, 0.0f});
    AddBone(rig, "left_lower_leg", 14, {0.0f, -0.42f, 0.0f});
    AddBone(rig, "left_foot", 15, {0.0f, -0.40f, 0.08f});
    AddBone(rig, "left_toe", 16, {0.0f, -0.04f, 0.19f});
    AddBone(rig, "right_upper_leg", 1, {0.12f, -0.24f, 0.0f});
    AddBone(rig, "right_lower_leg", 18, {0.0f, -0.42f, 0.0f});
    AddBone(rig, "right_foot", 19, {0.0f, -0.40f, 0.08f});
    AddBone(rig, "right_toe", 20, {0.0f, -0.04f, 0.19f});
    return rig;
}

RigValidationReport ValidateRigDefinition(const RigDefinition& rig) {
    RigValidationReport report{};
    if (rig.formatVersion != RigDefinition::kFormatVersion) {
        report.errors.push_back("Unsupported rig formatVersion: " + std::to_string(rig.formatVersion) + ".");
    }
    if (rig.id.empty()) {
        report.errors.push_back("Rig id is required.");
    }
    if (rig.bones.empty()) {
        report.errors.push_back("Rig must contain at least one bone.");
        return report;
    }

    std::unordered_set<std::string> names{};
    std::unordered_set<std::string> humanoidBoneKeys{};
    for (std::size_t index = 0; index < rig.bones.size(); ++index) {
        const RigBone& bone = rig.bones[index];
        const std::string normalizedName = NormalizeBoneName(bone.name);
        if (normalizedName.empty()) {
            report.errors.push_back("Bone " + std::to_string(index) + " has no name.");
        } else if (!names.insert(normalizedName).second) {
            report.errors.push_back("Duplicate bone name: " + bone.name + ".");
        }
        if (bone.parentIndex == -1) {
            ++report.rootBoneCount;
        } else if (bone.parentIndex < 0 || bone.parentIndex >= static_cast<int>(rig.bones.size())) {
            report.errors.push_back("Bone '" + bone.name + "' has an invalid parent index.");
        } else if (bone.parentIndex == static_cast<int>(index)) {
            report.errors.push_back("Bone '" + bone.name + "' cannot parent itself.");
        }
        if (!IsFinite(bone.restLocal.position) || !IsFinite(bone.restLocal.rotationDegrees) || !IsFinite(bone.restLocal.scale)) {
            report.errors.push_back("Bone '" + bone.name + "' has a non-finite rest transform.");
        }
        if (bone.restLocal.scale.x <= 0.0f || bone.restLocal.scale.y <= 0.0f || bone.restLocal.scale.z <= 0.0f) {
            report.errors.push_back("Bone '" + bone.name + "' has a non-positive rest scale.");
        }
        const std::string humanoidKey = CanonicalHumanoidBoneKey(bone.name);
        if (!humanoidKey.empty()) {
            humanoidBoneKeys.insert(humanoidKey);
        }
    }

    if (report.rootBoneCount == 0U) {
        report.errors.push_back("Rig hierarchy has no root bone.");
    } else if (report.rootBoneCount > 1U) {
        report.warnings.push_back("Rig has multiple root bones; runtime retargeting will use the first root.");
    }

    std::vector<unsigned char> marks(rig.bones.size(), 0U);
    std::function<bool(std::size_t)> visit = [&](const std::size_t index) {
        if (marks[index] == 1U) {
            return true;
        }
        if (marks[index] == 2U) {
            return false;
        }
        marks[index] = 1U;
        const int parent = rig.bones[index].parentIndex;
        const bool cyclic = parent >= 0 && parent < static_cast<int>(rig.bones.size()) && visit(static_cast<std::size_t>(parent));
        marks[index] = 2U;
        return cyclic;
    };
    for (std::size_t index = 0; index < rig.bones.size(); ++index) {
        if (visit(index)) {
            report.errors.push_back("Rig hierarchy contains a parent cycle.");
            break;
        }
    }

    if (rig.profile == RigProfile::Humanoid) {
        static constexpr std::array<std::string_view, 22> required = {
            "root", "hips", "spine", "chest", "neck", "head",
            "leftshoulder", "leftarm", "leftforearm", "lefthand",
            "rightshoulder", "rightarm", "rightforearm", "righthand",
            "leftupleg", "leftleg", "leftfoot", "lefttoebase",
            "rightupleg", "rightleg", "rightfoot", "righttoebase",
        };
        report.humanoidRequiredBoneCount = required.size();
        for (const std::string_view bone : required) {
            if (humanoidBoneKeys.contains(std::string(bone))) {
                ++report.humanoidMatchedBoneCount;
            } else {
                report.warnings.push_back("Humanoid convention is missing '" + std::string(bone) + "'.");
            }
        }
    }

    report.valid = report.errors.empty();
    return report;
}

std::string SerializeRigDefinition(const RigDefinition& rig) {
    std::ostringstream output{};
    output << std::setprecision(8);
    output << "{\n";
    output << "  \"formatVersion\": " << rig.formatVersion << ",\n";
    output << "  \"id\": \"" << Escape(rig.id) << "\",\n";
    output << "  \"displayName\": \"" << Escape(rig.displayName) << "\",\n";
    output << "  \"profile\": \"" << RigProfileName(rig.profile) << "\",\n";
    output << "  \"bones\": [\n";
    for (std::size_t index = 0; index < rig.bones.size(); ++index) {
        const RigBone& bone = rig.bones[index];
        output << "    {\"name\":\"" << Escape(bone.name) << "\",\"parent\":" << bone.parentIndex
               << ",\"deform\":" << (bone.deform ? "true" : "false") << ",\"restLocal\":{\"position\":";
        WriteVec3Json(output, bone.restLocal.position);
        output << ",\"rotationDegrees\":";
        WriteVec3Json(output, bone.restLocal.rotationDegrees);
        output << ",\"scale\":";
        WriteVec3Json(output, bone.restLocal.scale);
        output << "}}";
        if (index + 1U < rig.bones.size()) {
            output << ',';
        }
        output << '\n';
    }
    output << "  ]\n}\n";
    return output.str();
}

std::optional<RigDefinition> ParseRigDefinition(const std::string_view jsonText) {
    RigDefinition rig{};
    rig.formatVersion = json::ExtractJsonInt(jsonText, "formatVersion").value_or(RigDefinition::kFormatVersion);
    rig.id = json::ExtractJsonString(jsonText, "id").value_or("");
    rig.displayName = json::ExtractJsonString(jsonText, "displayName").value_or(rig.id);
    const std::optional<RigProfile> profile = ParseRigProfile(json::ExtractJsonString(jsonText, "profile").value_or("generic"));
    if (!profile.has_value()) {
        return std::nullopt;
    }
    rig.profile = *profile;

    const std::vector<std::string_view> boneObjects = json::SplitJsonArrayObjects(jsonText, "bones");
    rig.bones.reserve(boneObjects.size());
    for (const std::string_view boneObject : boneObjects) {
        const std::optional<std::string> name = json::ExtractJsonString(boneObject, "name");
        const std::optional<std::int32_t> parent = json::ExtractJsonInt(boneObject, "parent");
        const std::optional<bool> deform = json::ExtractJsonBool(boneObject, "deform");
        const std::optional<Transform> rest = ParseRestTransform(boneObject);
        if (!name.has_value() || !parent.has_value() || !deform.has_value() || !rest.has_value()) {
            return std::nullopt;
        }
        rig.bones.push_back(RigBone{
            .name = *name,
            .parentIndex = *parent,
            .restLocal = *rest,
            .deform = *deform,
        });
    }
    if (rig.id.empty() || rig.bones.empty()) {
        return std::nullopt;
    }
    return rig;
}

std::optional<RigDefinition> LoadRigDefinition(const std::filesystem::path& path) {
    const std::string source = json::ReadTextFile(path);
    return source.empty() ? std::nullopt : ParseRigDefinition(source);
}

bool SaveRigDefinition(const std::filesystem::path& path, const RigDefinition& rig) {
    std::error_code error{};
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return false;
        }
    }
    return json::WriteTextFile(path, SerializeRigDefinition(rig));
}

} // namespace ri::scene
