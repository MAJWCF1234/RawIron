#pragma once

#include <string>
#include <string_view>

namespace ri::scene {

/// Mixamo/FBX bone path → stable compact key for retargeting and animation binding.
/// Strips `mixamorig` / `armature|` prefixes, lowercases, removes non-alphanumerics, then applies known aliases.
[[nodiscard]] std::string CanonicalHumanoidBoneKey(std::string_view rawBoneName);

} // namespace ri::scene
