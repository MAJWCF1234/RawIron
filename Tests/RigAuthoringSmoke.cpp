#include "RawIron/Scene/RigAuthoring.h"

#include <cstdlib>
#include <optional>
#include <string>

int main() {
    const ri::scene::RigDefinition humanoid = ri::scene::CreateHumanoidRigDefinition("test_humanoid", "Test Humanoid");
    const ri::scene::RigValidationReport report = ri::scene::ValidateRigDefinition(humanoid);
    if (!report.valid || report.rootBoneCount != 1U || report.humanoidMatchedBoneCount != report.humanoidRequiredBoneCount) {
        return EXIT_FAILURE;
    }

    const std::optional<ri::scene::RigDefinition> parsed =
        ri::scene::ParseRigDefinition(ri::scene::SerializeRigDefinition(humanoid));
    if (!parsed.has_value() || parsed->bones.size() != humanoid.bones.size()
        || parsed->profile != ri::scene::RigProfile::Humanoid) {
        return EXIT_FAILURE;
    }

    ri::scene::RigDefinition invalid = humanoid;
    invalid.bones[1].name = invalid.bones[0].name;
    invalid.bones[2].parentIndex = 3;
    invalid.bones[3].parentIndex = 2;
    const ri::scene::RigValidationReport invalidReport = ri::scene::ValidateRigDefinition(invalid);
    if (invalidReport.valid || invalidReport.errors.size() < 2U) {
        return EXIT_FAILURE;
    }

    ri::scene::RigDefinition edited = humanoid;
    if (!ri::scene::SetRigBoneRestLocal(
            edited, "left_hand", ri::scene::Transform{.position = {-0.30f, 0.05f, 0.0f}})) {
        return EXIT_FAILURE;
    }
    const std::optional<std::size_t> handIndex = ri::scene::FindRigBoneIndex(edited, "left_hand");
    if (!handIndex.has_value()
        || edited.bones[*handIndex].restLocal.position.x > -0.29f) {
        return EXIT_FAILURE;
    }
    const ri::scene::RigValidationReport editedReport = ri::scene::ValidateRigDefinition(edited);
    if (!editedReport.valid) {
        return EXIT_FAILURE;
    }

    if (!ri::scene::MirrorRigBoneRestAcrossX(edited, "left_hand")) {
        return EXIT_FAILURE;
    }
    const std::optional<std::size_t> rightHand = ri::scene::FindRigBoneIndex(edited, "right_hand");
    if (!rightHand.has_value()
        || edited.bones[*rightHand].restLocal.position.x < 0.24f
        || std::abs(edited.bones[*rightHand].restLocal.position.y - 0.05f) > 0.02f) {
        return EXIT_FAILURE;
    }
    if (ri::scene::MirrorPartnerBoneName("spine").has_value()
        || !ri::scene::MirrorPartnerBoneName("left_hand").has_value()) {
        return EXIT_FAILURE;
    }

    const ri::scene::RigBoneRenameResult renamed =
        ri::scene::RenameRigBone(edited, "left_hand", "left_grip");
    if (!renamed.valid || !ri::scene::FindRigBoneIndex(edited, "left_grip").has_value()
        || ri::scene::FindRigBoneIndex(edited, "left_hand").has_value()) {
        return EXIT_FAILURE;
    }
    if (ri::scene::RenameRigBone(edited, "left_grip", "right_hand").valid) {
        return EXIT_FAILURE;
    }

    const ri::scene::RigAddBoneResult added =
        ri::scene::AddRigChildBone(edited, "left_grip", "left_grip_tip");
    if (!added.valid || !ri::scene::FindRigBoneIndex(edited, "left_grip_tip").has_value()
        || edited.bones[added.boneIndex].parentIndex
            != static_cast<int>(*ri::scene::FindRigBoneIndex(edited, "left_grip"))) {
        return EXIT_FAILURE;
    }
    const ri::scene::RigAddBoneResult autoNamed = ri::scene::AddRigChildBone(edited, "left_grip");
    if (!autoNamed.valid || autoNamed.boneName.empty()) {
        return EXIT_FAILURE;
    }
    const ri::scene::RigDeleteBoneResult promotedDelete = ri::scene::DeleteRigBone(edited, "left_grip");
    const std::optional<std::size_t> lowerArm = ri::scene::FindRigBoneIndex(edited, "left_lower_arm");
    const std::optional<std::size_t> promotedTip = ri::scene::FindRigBoneIndex(edited, "left_grip_tip");
    const std::optional<std::size_t> promotedAuto = ri::scene::FindRigBoneIndex(edited, autoNamed.boneName);
    if (!promotedDelete.valid || promotedDelete.promotedChildCount != 2U || !lowerArm.has_value()
        || !promotedTip.has_value() || !promotedAuto.has_value()
        || edited.bones[*promotedTip].parentIndex != static_cast<int>(*lowerArm)
        || edited.bones[*promotedAuto].parentIndex != static_cast<int>(*lowerArm)
        || ri::scene::FindRigBoneIndex(edited, "left_grip").has_value()) {
        return EXIT_FAILURE;
    }
    const ri::scene::RigDeleteBoneResult deletedTip =
        ri::scene::DeleteRigBone(edited, "left_grip_tip");
    if (!deletedTip.valid || ri::scene::FindRigBoneIndex(edited, "left_grip_tip").has_value()) {
        return EXIT_FAILURE;
    }
    const ri::scene::RigDeleteBoneResult deletedAuto =
        ri::scene::DeleteRigBone(edited, autoNamed.boneName);
    if (!deletedAuto.valid || ri::scene::FindRigBoneIndex(edited, autoNamed.boneName).has_value()) {
        return EXIT_FAILURE;
    }

    const ri::scene::RigReparentBoneResult reparented =
        ri::scene::ReparentRigBone(edited, "left_lower_arm", "chest");
    const std::optional<std::size_t> lowerArmIndex = ri::scene::FindRigBoneIndex(edited, "left_lower_arm");
    const std::optional<std::size_t> chest = ri::scene::FindRigBoneIndex(edited, "chest");
    if (!reparented.valid || !lowerArmIndex.has_value() || !chest.has_value()
        || edited.bones[*lowerArmIndex].parentIndex != static_cast<int>(*chest)) {
        return EXIT_FAILURE;
    }
    if (ri::scene::ReparentRigBone(edited, "chest", "left_lower_arm").valid) {
        return EXIT_FAILURE;
    }

    const std::vector<ri::scene::RigBoneTreeEntry> treeOrder =
        ri::scene::BuildRigBoneDepthFirstOrder(edited);
    if (treeOrder.empty() || treeOrder.front().boneIndex != 0U || treeOrder.front().depth != 0) {
        return EXIT_FAILURE;
    }
    bool childAfterParent = false;
    for (const ri::scene::RigBoneTreeEntry& entry : treeOrder) {
        if (entry.depth > 0) {
            childAfterParent = true;
            break;
        }
    }
    if (!childAfterParent) {
        return EXIT_FAILURE;
    }

    const ri::scene::RigDefinition humanoidFresh =
        ri::scene::CreateHumanoidRigDefinition("fresh_humanoid", "Fresh Humanoid");
    if (!ri::scene::MissingHumanoidBoneKeys(humanoidFresh).empty()) {
        return EXIT_FAILURE;
    }
    ri::scene::RigDefinition generic = humanoidFresh;
    generic.profile = ri::scene::RigProfile::Generic;
    if (!ri::scene::MissingHumanoidBoneKeys(generic).empty()) {
        return EXIT_FAILURE;
    }
    if (!ri::scene::IsHumanoidRequiredBoneKey("lefthand")
        || ri::scene::IsHumanoidRequiredBoneKey("not_a_slot")) {
        return EXIT_FAILURE;
    }

    ri::scene::RigDefinition slotTest = humanoidFresh;
    if (!ri::scene::DeleteRigBone(slotTest, "left_hand").valid) {
        return EXIT_FAILURE;
    }
    const ri::scene::RigAddBoneResult slotAdded =
        ri::scene::AddHumanoidSlotBone(slotTest, "lefthand");
    if (!slotAdded.valid || !ri::scene::FindRigBoneIndex(slotTest, "left_hand").has_value()
        || !ri::scene::MissingHumanoidBoneKeys(slotTest).empty()) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
