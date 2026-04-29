#pragma once

#include "RawIron/Scene/Scene.h"
#include "RawIron/Trace/KinematicPhysics.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

namespace ri::scene {

/// Moves the node's local translation so its world pivot matches the center of `state.bounds`.
/// Rotation and scale from `localTransform` are preserved. Returns false for invalid handles, singular solves,
/// or empty physics bounds.
[[nodiscard]] bool ApplyKinematicBodyStateWorldCenterToSceneNode(Scene& scene,
                                                                  int nodeHandle,
                                                                  const ri::trace::KinematicBodyState& state);

/// Convenience batch; skips entries when handles are invalid or bounds are empty.
void ApplyKinematicBodyStatesToSceneNodes(Scene& scene,
                                           std::span<const int> nodeHandles,
                                           std::span<const ri::trace::KinematicBodyState> states);

inline void ApplyKinematicBodyStatesToSceneNodes(Scene& scene,
                                                 const std::vector<int>& nodeHandles,
                                                 const std::vector<ri::trace::KinematicBodyState>& states) {
    const std::size_t count = std::min(nodeHandles.size(), states.size());
    ApplyKinematicBodyStatesToSceneNodes(scene,
                                         std::span<const int>(nodeHandles.data(), count),
                                         std::span<const ri::trace::KinematicBodyState>(states.data(), count));
}

} // namespace ri::scene
