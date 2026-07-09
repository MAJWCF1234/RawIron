#pragma once

#include "RawIron/Scene/Transform.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::scene {

/// The intended use of a skeleton. Humanoid rigs receive convention checks so imported assets can be retargeted safely.
enum class RigProfile {
    Generic,
    Humanoid,
};

struct RigBone {
    std::string name{};
    /// Index of the parent bone, or -1 for a root bone.
    int parentIndex = -1;
    Transform restLocal{};
    /// False for control-only bones such as the template's root motion node.
    bool deform = true;
};

/// Portable skeleton source document. Mesh skin weights remain owned by the imported mesh format; this asset establishes
/// a stable, inspectable skeleton contract shared by DCC exports, the editor, animation tools, and runtime retargeting.
struct RigDefinition {
    static constexpr int kFormatVersion = 1;

    int formatVersion = kFormatVersion;
    std::string id{};
    std::string displayName{};
    RigProfile profile = RigProfile::Generic;
    std::vector<RigBone> bones{};
};

struct RigValidationReport {
    bool valid = false;
    std::vector<std::string> errors{};
    std::vector<std::string> warnings{};
    std::size_t rootBoneCount = 0;
    std::size_t humanoidRequiredBoneCount = 0;
    std::size_t humanoidMatchedBoneCount = 0;
};

[[nodiscard]] std::string RigProfileName(RigProfile profile);
[[nodiscard]] std::optional<RigProfile> ParseRigProfile(std::string_view value);

/// Generates RawIron's editable baseline humanoid skeleton (root motion + 21 deform bones).
[[nodiscard]] RigDefinition CreateHumanoidRigDefinition(std::string id, std::string displayName = {});

/// Checks hierarchy integrity, finite rest transforms, duplicate names, and the humanoid retargeting convention.
[[nodiscard]] RigValidationReport ValidateRigDefinition(const RigDefinition& rig);

[[nodiscard]] std::string SerializeRigDefinition(const RigDefinition& rig);
[[nodiscard]] std::optional<RigDefinition> ParseRigDefinition(std::string_view jsonText);
[[nodiscard]] std::optional<RigDefinition> LoadRigDefinition(const std::filesystem::path& path);
[[nodiscard]] bool SaveRigDefinition(const std::filesystem::path& path, const RigDefinition& rig);

} // namespace ri::scene
