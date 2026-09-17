#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace ri::scene {

/// Mixamo/FBX bone path → stable compact key for retargeting and animation binding.
/// Strips `mixamorig` / `armature|` prefixes, lowercases, removes non-alphanumerics, then applies known aliases.
[[nodiscard]] std::string CanonicalHumanoidBoneKey(std::string_view rawBoneName);

struct HumanoidSlotBoneSpec {
    std::string slotKey{};
    std::string boneName{};
    /// Empty when the slot attaches directly under the rig root bone.
    std::string parentSlotKey{};
};

/// Default RawIron humanoid naming for a convention slot key such as `lefthand`.
[[nodiscard]] std::optional<HumanoidSlotBoneSpec> HumanoidSlotBoneSpecForKey(std::string_view slotKey);

} // namespace ri::scene
