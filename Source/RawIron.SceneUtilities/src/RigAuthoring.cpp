#include "RawIron/Scene/RigAuthoring.h"

#include "RawIron/Core/Detail/JsonScan.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Scene/HumanoidRigNames.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

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
        static constexpr std::array<std::string_view, 22> kRequired = {
            "root", "hips", "spine", "chest", "neck", "head",
            "leftshoulder", "leftarm", "leftforearm", "lefthand",
            "rightshoulder", "rightarm", "rightforearm", "righthand",
            "leftupleg", "leftleg", "leftfoot", "lefttoebase",
            "rightupleg", "rightleg", "rightfoot", "righttoebase",
        };
        report.humanoidRequiredBoneCount = kRequired.size();
        for (const std::string_view bone : kRequired) {
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

std::optional<std::size_t> FindRigBoneIndex(const RigDefinition& rig, const std::string_view boneName) {
    const std::string normalized = NormalizeBoneName(boneName);
    if (normalized.empty()) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < rig.bones.size(); ++index) {
        if (NormalizeBoneName(rig.bones[index].name) == normalized) {
            return index;
        }
    }
    return std::nullopt;
}

bool SetRigBoneRestLocal(
    RigDefinition& rig,
    const std::string_view boneName,
    const Transform& restLocal) {
    const std::optional<std::size_t> index = FindRigBoneIndex(rig, boneName);
    if (!index.has_value()) {
        return false;
    }
    if (!IsFinite(restLocal.position) || !IsFinite(restLocal.rotationDegrees) || !IsFinite(restLocal.scale)) {
        return false;
    }
    if (restLocal.scale.x <= 0.0f || restLocal.scale.y <= 0.0f || restLocal.scale.z <= 0.0f) {
        return false;
    }
    rig.bones[*index].restLocal = restLocal;
    return true;
}

Transform MirrorTransformAcrossX(const Transform& source) {
    Transform mirrored = source;
    mirrored.position.x = -mirrored.position.x;
    mirrored.rotationDegrees.y = -mirrored.rotationDegrees.y;
    mirrored.rotationDegrees.z = -mirrored.rotationDegrees.z;
    return mirrored;
}

std::optional<std::string> MirrorPartnerBoneName(const std::string_view boneName) {
    const std::string normalized = NormalizeBoneName(boneName);
    if (normalized.empty()) {
        return std::nullopt;
    }
    auto swapSide = [](const std::string& name) -> std::optional<std::string> {
        constexpr std::string_view kLeft = "left_";
        constexpr std::string_view kRight = "right_";
        if (name.size() >= kLeft.size() && name.compare(0, kLeft.size(), kLeft) == 0) {
            return std::string(kRight) + name.substr(kLeft.size());
        }
        if (name.size() >= kRight.size() && name.compare(0, kRight.size(), kRight) == 0) {
            return std::string(kLeft) + name.substr(kRight.size());
        }
        if (name.size() >= 2U && name.ends_with("_l")) {
            return name.substr(0, name.size() - 2U) + "_r";
        }
        if (name.size() >= 2U && name.ends_with("_r")) {
            return name.substr(0, name.size() - 2U) + "_l";
        }
        return std::nullopt;
    };
    const std::optional<std::string> swapped = swapSide(std::string(boneName));
    if (!swapped.has_value()) {
        return std::nullopt;
    }
    if (NormalizeBoneName(*swapped) == normalized) {
        return std::nullopt;
    }
    return swapped;
}

std::string TrimBoneName(const std::string_view value) {
    std::string trimmed{value};
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front())) != 0) {
        trimmed.erase(trimmed.begin());
    }
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back())) != 0) {
        trimmed.pop_back();
    }
    return trimmed;
}

std::string MakeUniqueBoneName(const RigDefinition& rig, const std::string_view baseName) {
    std::string base = TrimBoneName(baseName);
    if (base.empty()) {
        base = "bone";
    }
    if (!FindRigBoneIndex(rig, base).has_value()) {
        return base;
    }
    for (int suffix = 2; suffix < 10000; ++suffix) {
        const std::string candidate = base + "_" + std::to_string(suffix);
        if (!FindRigBoneIndex(rig, candidate).has_value()) {
            return candidate;
        }
    }
    return base + "_new";
}

bool RigBoneHasChildren(const RigDefinition& rig, const std::size_t boneIndex) {
    for (const RigBone& bone : rig.bones) {
        if (bone.parentIndex == static_cast<int>(boneIndex)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] ri::math::Vec3 EulerDegreesFromRotation3x3(const ri::math::Mat4& rotationOnly) {
    constexpr float kRadToDeg = 180.0f / ri::math::kPi;
    const float sy = std::sqrt(
        (rotationOnly.m[0][0] * rotationOnly.m[0][0]) + (rotationOnly.m[1][0] * rotationOnly.m[1][0]));
    const bool singular = sy < 1.0e-6f;
    if (!singular) {
        const float x = std::atan2(rotationOnly.m[2][1], rotationOnly.m[2][2]);
        const float y = std::atan2(-rotationOnly.m[2][0], sy);
        const float z = std::atan2(rotationOnly.m[1][0], rotationOnly.m[0][0]);
        return ri::math::Vec3{x * kRadToDeg, y * kRadToDeg, z * kRadToDeg};
    }
    const float x = std::atan2(-rotationOnly.m[1][2], rotationOnly.m[1][1]);
    const float y = std::atan2(-rotationOnly.m[2][0], sy);
    return ri::math::Vec3{x * kRadToDeg, y * kRadToDeg, 0.0f};
}

[[nodiscard]] Transform TransformFromMatrix(const ri::math::Mat4& matrix) {
    Transform transform{};
    transform.position = ri::math::ExtractTranslation(matrix);
    transform.scale = ri::math::ExtractScale(matrix);
    ri::math::Mat4 rotationOnly = matrix;
    for (int column = 0; column < 3; ++column) {
        const float scaleValue =
            column == 0 ? transform.scale.x : (column == 1 ? transform.scale.y : transform.scale.z);
        const float inverseScale = std::fabs(scaleValue) > 1.0e-8f ? 1.0f / scaleValue : 0.0f;
        rotationOnly.m[0][column] *= inverseScale;
        rotationOnly.m[1][column] *= inverseScale;
        rotationOnly.m[2][column] *= inverseScale;
    }
    transform.rotationDegrees = EulerDegreesFromRotation3x3(rotationOnly);
    return transform;
}

[[nodiscard]] std::vector<ri::math::Mat4> BuildRigBoneWorldRestMatrices(const RigDefinition& rig) {
    std::vector<ri::math::Mat4> worlds(rig.bones.size(), ri::math::IdentityMatrix());
    std::vector<unsigned char> ready(rig.bones.size(), 0U);
    const auto worldFor = [&](const auto& self, const std::size_t index) -> ri::math::Mat4 {
        if (index >= rig.bones.size()) {
            return ri::math::IdentityMatrix();
        }
        if (ready[index] != 0U) {
            return worlds[index];
        }
        const ri::math::Mat4 local = rig.bones[index].restLocal.LocalMatrix();
        const int parent = rig.bones[index].parentIndex;
        if (parent < 0) {
            worlds[index] = local;
        } else {
            worlds[index] = ri::math::Multiply(self(self, static_cast<std::size_t>(parent)), local);
        }
        ready[index] = 1U;
        return worlds[index];
    };
    for (std::size_t index = 0; index < rig.bones.size(); ++index) {
        worldFor(worldFor, index);
    }
    return worlds;
}

[[nodiscard]] bool ReparentRestLocalToParentWorld(
    RigBone& bone,
    const ri::math::Mat4& boneWorld,
    const ri::math::Mat4& parentWorld) {
    ri::math::Mat4 parentInverse{};
    if (!ri::math::TryInvertAffineMat4(parentWorld, parentInverse)) {
        return false;
    }
    bone.restLocal = TransformFromMatrix(ri::math::Multiply(parentInverse, boneWorld));
    return IsFinite(bone.restLocal.position) && IsFinite(bone.restLocal.rotationDegrees)
        && IsFinite(bone.restLocal.scale) && bone.restLocal.scale.x > 0.0f && bone.restLocal.scale.y > 0.0f
        && bone.restLocal.scale.z > 0.0f;
}

RigAddBoneResult AddRigChildBone(
    RigDefinition& rig,
    const std::string_view parentName,
    const std::string_view desiredName,
    const Transform& restLocal) {
    RigAddBoneResult result{};
    const std::optional<std::size_t> parentIndex = FindRigBoneIndex(rig, parentName);
    if (!parentIndex.has_value()) {
        result.summary = "Parent bone '" + std::string(parentName) + "' was not found.";
        return result;
    }
    if (!IsFinite(restLocal.position) || !IsFinite(restLocal.rotationDegrees) || !IsFinite(restLocal.scale)
        || restLocal.scale.x <= 0.0f || restLocal.scale.y <= 0.0f || restLocal.scale.z <= 0.0f) {
        result.summary = "Rest transform must be finite with positive scale.";
        return result;
    }

    std::string boneName = TrimBoneName(desiredName);
    if (boneName.empty()) {
        boneName = MakeUniqueBoneName(rig, std::string(parentName) + "_child");
    } else if (NormalizeBoneName(boneName).empty()) {
        result.summary = "Bone name must contain letters or digits.";
        return result;
    } else if (FindRigBoneIndex(rig, boneName).has_value()) {
        result.summary = "Bone '" + boneName + "' already exists.";
        return result;
    }

    const RigDefinition backup = rig;
    const std::size_t newIndex = rig.bones.size();
    rig.bones.push_back(RigBone{
        .name = std::move(boneName),
        .parentIndex = static_cast<int>(*parentIndex),
        .restLocal = restLocal,
        .deform = true,
    });
    const RigValidationReport validation = ValidateRigDefinition(rig);
    if (!validation.valid) {
        rig = backup;
        result.summary = validation.errors.empty() ? "Add bone made the rig invalid."
                                                   : validation.errors.front();
        return result;
    }
    result.valid = true;
    result.boneName = rig.bones[newIndex].name;
    result.boneIndex = newIndex;
    result.summary = "Added bone '" + result.boneName + "' under '" + std::string(parentName) + "'.";
    return result;
}

bool RigBoneIsDescendantOf(
    const RigDefinition& rig,
    const std::size_t candidateIndex,
    const std::size_t ancestorIndex) {
    if (candidateIndex >= rig.bones.size() || ancestorIndex >= rig.bones.size()) {
        return false;
    }
    std::size_t walk = candidateIndex;
    while (true) {
        if (walk == ancestorIndex) {
            return true;
        }
        const int parent = rig.bones[walk].parentIndex;
        if (parent < 0 || static_cast<std::size_t>(parent) >= rig.bones.size()) {
            return false;
        }
        walk = static_cast<std::size_t>(parent);
    }
}

RigReparentBoneResult ReparentRigBone(
    RigDefinition& rig,
    const std::string_view boneName,
    const std::string_view newParentName) {
    RigReparentBoneResult result{};
    const std::optional<std::size_t> boneIndex = FindRigBoneIndex(rig, boneName);
    if (!boneIndex.has_value()) {
        result.summary = "Bone '" + std::string(boneName) + "' was not found.";
        return result;
    }
    const std::optional<std::size_t> newParentIndex = FindRigBoneIndex(rig, newParentName);
    if (!newParentIndex.has_value()) {
        result.summary = "Parent bone '" + std::string(newParentName) + "' was not found.";
        return result;
    }
    if (*boneIndex == *newParentIndex) {
        result.summary = "A bone cannot parent itself.";
        return result;
    }
    if (RigBoneIsDescendantOf(rig, *newParentIndex, *boneIndex)) {
        result.summary = "Cannot parent '" + std::string(boneName) + "' under its own descendant.";
        return result;
    }
    const int previousParent = rig.bones[*boneIndex].parentIndex;
    if (previousParent == static_cast<int>(*newParentIndex)) {
        result.valid = true;
        result.summary = "Bone '" + std::string(boneName) + "' already uses that parent.";
        return result;
    }

    const RigDefinition backup = rig;
    rig.bones[*boneIndex].parentIndex = static_cast<int>(*newParentIndex);
    const RigValidationReport validation = ValidateRigDefinition(rig);
    if (!validation.valid) {
        rig = backup;
        result.summary = validation.errors.empty() ? "Reparent made the rig invalid."
                                                   : validation.errors.front();
        return result;
    }
    result.valid = true;
    result.summary = "Reparented '" + std::string(boneName) + "' under '" + std::string(newParentName)
        + "'.";
    return result;
}

bool IsHumanoidRequiredBoneKey(const std::string_view canonicalKey) {
    static constexpr std::array<std::string_view, 22> kRequired = {
        "root", "hips", "spine", "chest", "neck", "head",
        "leftshoulder", "leftarm", "leftforearm", "lefthand",
        "rightshoulder", "rightarm", "rightforearm", "righthand",
        "leftupleg", "leftleg", "leftfoot", "lefttoebase",
        "rightupleg", "rightleg", "rightfoot", "righttoebase",
    };
    if (canonicalKey.empty()) {
        return false;
    }
    for (const std::string_view required : kRequired) {
        if (required == canonicalKey) {
            return true;
        }
    }
    return false;
}

std::optional<std::size_t> FindRigBoneIndexForHumanoidSlot(
    const RigDefinition& rig,
    const std::string_view slotKey) {
    if (slotKey.empty()) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < rig.bones.size(); ++index) {
        if (CanonicalHumanoidBoneKey(rig.bones[index].name) == slotKey) {
            return index;
        }
    }
    return std::nullopt;
}

std::vector<std::string> MissingHumanoidBoneKeys(const RigDefinition& rig) {
    if (rig.profile != RigProfile::Humanoid) {
        return {};
    }
    std::unordered_set<std::string> present{};
    for (const RigBone& bone : rig.bones) {
        const std::string key = CanonicalHumanoidBoneKey(bone.name);
        if (!key.empty()) {
            present.insert(key);
        }
    }
    static constexpr std::array<std::string_view, 22> kRequired = {
        "root", "hips", "spine", "chest", "neck", "head",
        "leftshoulder", "leftarm", "leftforearm", "lefthand",
        "rightshoulder", "rightarm", "rightforearm", "righthand",
        "leftupleg", "leftleg", "leftfoot", "lefttoebase",
        "rightupleg", "rightleg", "rightfoot", "righttoebase",
    };
    std::vector<std::string> missing{};
    for (const std::string_view required : kRequired) {
        if (!present.contains(std::string(required))) {
            missing.emplace_back(required);
        }
    }
    return missing;
}

RigAddBoneResult AddHumanoidSlotBone(RigDefinition& rig, const std::string_view slotKey) {
    RigAddBoneResult result{};
    if (rig.profile != RigProfile::Humanoid) {
        result.summary = "Humanoid slot bones require a humanoid rig profile.";
        return result;
    }
    const std::optional<HumanoidSlotBoneSpec> spec = HumanoidSlotBoneSpecForKey(slotKey);
    if (!spec.has_value()) {
        result.summary = "Unknown humanoid slot '" + std::string(slotKey) + "'.";
        return result;
    }
    if (FindRigBoneIndexForHumanoidSlot(rig, slotKey).has_value()
        || FindRigBoneIndex(rig, spec->boneName).has_value()) {
        result.summary = "Slot '" + std::string(slotKey) + "' is already filled.";
        return result;
    }

    const RigDefinition templateRig = CreateHumanoidRigDefinition("forge_humanoid_template");
    Transform restLocal{};
    if (const std::optional<std::size_t> templateIndex = FindRigBoneIndex(templateRig, spec->boneName)) {
        restLocal = templateRig.bones[*templateIndex].restLocal;
    }

    if (spec->slotKey == "root") {
        const RigDefinition backup = rig;
        rig.bones.insert(
            rig.bones.begin(),
            RigBone{
                .name = spec->boneName,
                .parentIndex = -1,
                .restLocal = restLocal,
                .deform = false,
            });
        for (RigBone& bone : rig.bones) {
            if (bone.parentIndex >= 0) {
                ++bone.parentIndex;
            }
        }
        const RigValidationReport validation = ValidateRigDefinition(rig);
        if (!validation.valid) {
            rig = backup;
            result.summary = validation.errors.empty() ? "Adding root made the rig invalid."
                                                       : validation.errors.front();
            return result;
        }
        result.valid = true;
        result.boneName = spec->boneName;
        result.boneIndex = 0U;
        result.summary = "Added humanoid slot '" + std::string(slotKey) + "' as " + spec->boneName + ".";
        return result;
    }

    std::string parentName{};
    if (spec->parentSlotKey.empty()) {
        result.summary = "Humanoid slot '" + std::string(slotKey) + "' has no parent mapping.";
        return result;
    }
    if (const std::optional<std::size_t> parentIndex =
            FindRigBoneIndexForHumanoidSlot(rig, spec->parentSlotKey)) {
        parentName = rig.bones[*parentIndex].name;
    } else if (const std::optional<HumanoidSlotBoneSpec> parentSpec =
                   HumanoidSlotBoneSpecForKey(spec->parentSlotKey)) {
        parentName = parentSpec->boneName;
        if (!FindRigBoneIndex(rig, parentName).has_value()) {
            result.summary = "Parent slot '" + spec->parentSlotKey + "' is missing. Add it first.";
            return result;
        }
    } else {
        result.summary = "Parent slot '" + spec->parentSlotKey + "' is unknown.";
        return result;
    }

    const bool deform = spec->slotKey != "root";
    const RigDefinition backup = rig;
    const std::size_t newIndex = rig.bones.size();
    rig.bones.push_back(RigBone{
        .name = spec->boneName,
        .parentIndex = static_cast<int>(*FindRigBoneIndex(rig, parentName)),
        .restLocal = restLocal,
        .deform = deform,
    });
    const RigValidationReport validation = ValidateRigDefinition(rig);
    if (!validation.valid) {
        rig = backup;
        result.summary = validation.errors.empty() ? "Add slot made the rig invalid."
                                                   : validation.errors.front();
        return result;
    }
    result.valid = true;
    result.boneName = spec->boneName;
    result.boneIndex = newIndex;
    result.summary = "Added humanoid slot '" + std::string(slotKey) + "' as " + spec->boneName + ".";
    return result;
}

std::vector<RigBoneTreeEntry> BuildRigBoneDepthFirstOrder(const RigDefinition& rig) {
    std::vector<RigBoneTreeEntry> order{};
    order.reserve(rig.bones.size());
    std::vector<unsigned char> visited(rig.bones.size(), 0U);
    const auto visit = [&](const auto& self, const std::size_t index, const int depth) -> void {
        if (index >= rig.bones.size() || visited[index] != 0U) {
            return;
        }
        visited[index] = 1U;
        order.push_back(RigBoneTreeEntry{.boneIndex = index, .depth = depth});
        for (std::size_t child = 0; child < rig.bones.size(); ++child) {
            if (rig.bones[child].parentIndex == static_cast<int>(index)) {
                self(self, child, depth + 1);
            }
        }
    };
    for (std::size_t index = 0; index < rig.bones.size(); ++index) {
        if (rig.bones[index].parentIndex < 0) {
            visit(visit, index, 0);
        }
    }
    for (std::size_t index = 0; index < rig.bones.size(); ++index) {
        if (visited[index] == 0U) {
            visit(visit, index, 0);
        }
    }
    return order;
}

RigDeleteBoneResult DeleteRigBone(RigDefinition& rig, const std::string_view boneName) {
    RigDeleteBoneResult result{};
    const std::optional<std::size_t> index = FindRigBoneIndex(rig, boneName);
    if (!index.has_value()) {
        result.summary = "Bone '" + std::string(boneName) + "' was not found.";
        return result;
    }

    const RigDefinition backup = rig;
    const int deletedIndex = static_cast<int>(*index);
    const int promoteToParent = rig.bones[*index].parentIndex;
    std::vector<std::pair<std::string, ri::math::Mat4>> promotedChildren{};
    const std::vector<ri::math::Mat4> worldBeforeDelete = BuildRigBoneWorldRestMatrices(rig);
    for (std::size_t childIndex = 0; childIndex < rig.bones.size(); ++childIndex) {
        if (rig.bones[childIndex].parentIndex != deletedIndex) {
            continue;
        }
        promotedChildren.push_back({rig.bones[childIndex].name, worldBeforeDelete[childIndex]});
    }

    rig.bones.erase(rig.bones.begin() + static_cast<std::ptrdiff_t>(*index));
    const int adjustedPromoteParent =
        promoteToParent > deletedIndex ? promoteToParent - 1 : promoteToParent;
    for (RigBone& bone : rig.bones) {
        if (bone.parentIndex == deletedIndex) {
            bone.parentIndex = adjustedPromoteParent;
        } else if (bone.parentIndex > deletedIndex) {
            --bone.parentIndex;
        }
    }

    const std::vector<ri::math::Mat4> worldAfterDelete = BuildRigBoneWorldRestMatrices(rig);
    const ri::math::Mat4 promotionParentWorld =
        adjustedPromoteParent >= 0
            ? worldAfterDelete[static_cast<std::size_t>(adjustedPromoteParent)]
            : ri::math::IdentityMatrix();
    for (const std::pair<std::string, ri::math::Mat4>& promotedChild : promotedChildren) {
        const std::optional<std::size_t> childIndex = FindRigBoneIndex(rig, promotedChild.first);
        if (!childIndex.has_value()) {
            rig = backup;
            result.summary = "Delete lost promoted child '" + promotedChild.first + "'.";
            return result;
        }
        if (adjustedPromoteParent >= 0) {
            if (!ReparentRestLocalToParentWorld(
                    rig.bones[*childIndex], promotedChild.second, promotionParentWorld)) {
                rig = backup;
                result.summary = "Could not preserve rest pose for promoted child '"
                    + promotedChild.first + "'.";
                return result;
            }
        } else {
            rig.bones[*childIndex].restLocal = TransformFromMatrix(promotedChild.second);
        }
    }

    const RigValidationReport validation = ValidateRigDefinition(rig);
    if (!validation.valid) {
        rig = backup;
        result.summary = validation.errors.empty() ? "Delete made the rig invalid."
                                                   : validation.errors.front();
        return result;
    }
    result.valid = true;
    result.promotedChildCount = promotedChildren.size();
    result.summary = "Deleted bone '" + std::string(boneName) + "'";
    if (result.promotedChildCount > 0U) {
        result.summary += " and promoted " + std::to_string(result.promotedChildCount) + " child bone"
            + (result.promotedChildCount == 1U ? "" : "s") + ".";
    } else {
        result.summary += ".";
    }
    return result;
}

RigBoneRenameResult RenameRigBone(
    RigDefinition& rig,
    const std::string_view oldName,
    const std::string_view newName) {
    RigBoneRenameResult result{};
    const std::string trimmedNew = TrimBoneName(newName);
    if (trimmedNew.empty()) {
        result.summary = "New bone name is required.";
        return result;
    }
    if (NormalizeBoneName(trimmedNew).empty()) {
        result.summary = "New bone name must contain letters or digits.";
        return result;
    }
    const std::optional<std::size_t> oldIndex = FindRigBoneIndex(rig, oldName);
    if (!oldIndex.has_value()) {
        result.summary = "Bone '" + std::string(oldName) + "' was not found.";
        return result;
    }
    if (FindRigBoneIndex(rig, trimmedNew).has_value()
        && NormalizeBoneName(rig.bones[*oldIndex].name) != NormalizeBoneName(trimmedNew)) {
        result.summary = "Bone '" + trimmedNew + "' already exists.";
        return result;
    }
    const std::string previousName = rig.bones[*oldIndex].name;
    rig.bones[*oldIndex].name = trimmedNew;
    const RigValidationReport validation = ValidateRigDefinition(rig);
    if (!validation.valid) {
        rig.bones[*oldIndex].name = previousName;
        result.summary = validation.errors.empty() ? "Rename made the rig invalid."
                                                   : validation.errors.front();
        return result;
    }
    result.valid = true;
    result.summary = "Renamed " + std::string(oldName) + " to " + trimmedNew + ".";
    return result;
}

bool MirrorRigBoneRestAcrossX(RigDefinition& rig, const std::string_view sourceBoneName) {
    const std::optional<std::size_t> sourceIndex = FindRigBoneIndex(rig, sourceBoneName);
    const std::optional<std::string> partnerName = MirrorPartnerBoneName(sourceBoneName);
    if (!sourceIndex.has_value() || !partnerName.has_value()) {
        return false;
    }
    const std::optional<std::size_t> partnerIndex = FindRigBoneIndex(rig, *partnerName);
    if (!partnerIndex.has_value()) {
        return false;
    }
    rig.bones[*partnerIndex].restLocal =
        MirrorTransformAcrossX(rig.bones[*sourceIndex].restLocal);
    return true;
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

std::optional<RigDefinition> LoadSidecarRigDefinition(
    const std::string_view rigPath,
    const std::filesystem::path& documentPath) {
    if (rigPath.empty()) {
        return std::nullopt;
    }
    const std::filesystem::path rig{std::string(rigPath)};
    std::vector<std::filesystem::path> candidates;
    if (rig.is_absolute()) {
        candidates.push_back(rig);
    } else {
        std::error_code directoryError{};
        const bool anchorIsDirectory = std::filesystem::is_directory(documentPath, directoryError);
        const std::filesystem::path directory =
            (!directoryError && anchorIsDirectory) ? documentPath
            : (documentPath.has_parent_path() ? documentPath.parent_path() : std::filesystem::path{});
        if (!directory.empty()) {
            candidates.push_back(directory / rig);
            candidates.push_back(directory / "rigs" / rig.filename());
            const std::filesystem::path parent = directory.parent_path();
            if (!parent.empty() && parent != directory) {
                candidates.push_back(parent / rig);
                candidates.push_back(parent / "rigs" / rig.filename());
            }
        }
        candidates.push_back(rig);
    }
    for (const std::filesystem::path& candidate : candidates) {
        std::error_code error{};
        if (!std::filesystem::is_regular_file(candidate, error)) {
            continue;
        }
        if (auto loaded = LoadRigDefinition(candidate); loaded.has_value()) {
            return loaded;
        }
    }
    return std::nullopt;
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
