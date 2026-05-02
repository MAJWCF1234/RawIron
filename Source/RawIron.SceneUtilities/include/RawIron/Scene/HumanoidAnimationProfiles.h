#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ri::scene {

/// Built-in survivor NPC clip routing (Mixamo-style names). Used by authoring import / hydration.
[[nodiscard]] bool HumanoidAnimationProfileExists(std::string_view profileId);

/// Ordered candidate clip names for a logical action (`idle`, `walk`, …). Empty if the profile or action is unknown.
[[nodiscard]] const std::vector<std::string>& HumanoidProfileClipCandidates(std::string_view profileId,
                                                                            std::string_view action);

[[nodiscard]] std::vector<std::string_view> ListHumanoidAnimationProfileIds();

} // namespace ri::scene
