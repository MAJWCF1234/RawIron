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

[[nodiscard]] std::optional<std::size_t> FindRigBoneIndex(
    const RigDefinition& rig,
    std::string_view boneName);
[[nodiscard]] bool SetRigBoneRestLocal(
    RigDefinition& rig,
    std::string_view boneName,
    const Transform& restLocal);
[[nodiscard]] Transform MirrorTransformAcrossX(const Transform& source);
[[nodiscard]] std::optional<std::string> MirrorPartnerBoneName(std::string_view boneName);
/// Copies the selected bone's rest pose to its left/right partner with an X-axis mirror.
[[nodiscard]] bool MirrorRigBoneRestAcrossX(RigDefinition& rig, std::string_view sourceBoneName);

struct RigBoneRenameResult {
    bool valid = false;
    std::string summary{};
};

[[nodiscard]] RigBoneRenameResult RenameRigBone(
    RigDefinition& rig,
    std::string_view oldName,
    std::string_view newName);

struct RigAddBoneResult {
    bool valid = false;
    std::string boneName{};
    std::size_t boneIndex = 0;
    std::string summary{};
};

/// Appends a deform bone under `parentName`. When `desiredName` is empty, generates a unique name.
[[nodiscard]] RigAddBoneResult AddRigChildBone(
    RigDefinition& rig,
    std::string_view parentName,
    std::string_view desiredName = {},
    const Transform& restLocal = Transform{.position = {0.0f, 0.12f, 0.0f}});

struct RigDeleteBoneResult {
    bool valid = false;
    std::size_t promotedChildCount = 0;
    std::string summary{};
};

/// Removes a bone, promotes direct children to its parent, and preserves promoted rest poses in world space.
[[nodiscard]] RigDeleteBoneResult DeleteRigBone(RigDefinition& rig, std::string_view boneName);

struct RigReparentBoneResult {
    bool valid = false;
    std::string summary{};
};

/// Moves `boneName` under `newParentName`. Rejects self-parenting and hierarchy cycles.
[[nodiscard]] RigReparentBoneResult ReparentRigBone(
    RigDefinition& rig,
    std::string_view boneName,
    std::string_view newParentName);

struct RigBoneTreeEntry {
    std::size_t boneIndex = 0;
    int depth = 0;
};

/// Depth-first rig bone order for tree UIs. Unreachable bones are appended last.
[[nodiscard]] std::vector<RigBoneTreeEntry> BuildRigBoneDepthFirstOrder(const RigDefinition& rig);

[[nodiscard]] bool IsHumanoidRequiredBoneKey(std::string_view canonicalKey);
[[nodiscard]] std::vector<std::string> MissingHumanoidBoneKeys(const RigDefinition& rig);
[[nodiscard]] std::optional<std::size_t> FindRigBoneIndexForHumanoidSlot(
    const RigDefinition& rig,
    std::string_view slotKey);

/// Adds the default RawIron humanoid bone for a missing convention slot.
[[nodiscard]] RigAddBoneResult AddHumanoidSlotBone(RigDefinition& rig, std::string_view slotKey);

[[nodiscard]] std::string SerializeRigDefinition(const RigDefinition& rig);
[[nodiscard]] std::optional<RigDefinition> ParseRigDefinition(std::string_view jsonText);
[[nodiscard]] std::optional<RigDefinition> LoadRigDefinition(const std::filesystem::path& path);
/// Resolves a workspace-relative or document-relative `.ri_rig.json` next to, or above, `documentPath`.
[[nodiscard]] std::optional<RigDefinition> LoadSidecarRigDefinition(
    std::string_view rigPath,
    const std::filesystem::path& documentPath);
[[nodiscard]] bool SaveRigDefinition(const std::filesystem::path& path, const RigDefinition& rig);

} // namespace ri::scene
