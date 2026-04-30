#pragma once

#include "RawIron/Scene/Scene.h"
#include "RawIron/Trace/TraceScene.h"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::scene {

struct SubtreeColliderBuildOptions {
    /// Prepended to generated ids when \ref Node::name is empty (e.g. `prop_`).
    std::string idPrefix = "collider";
    bool structural = true;
    bool dynamic = false;
    /// Skip nodes without an attached mesh / empty bounds.
    bool skipNodesWithoutBounds = true;
    /// Extra tags copied onto each \ref ri::trace::TraceCollider::simulationTags (e.g. `subtree_batch`).
    std::vector<std::string> extraSimulationTags{};
    std::uint32_t simulationFlagsOrMask = 0U;
};

/// Append one \ref ri::trace::TraceCollider per mesh-bearing node in the subtree (includes \p root when it has a mesh).
[[nodiscard]] std::size_t AppendTraceCollidersForSubtree(const Scene& scene,
                                                       int rootNodeHandle,
                                                       const SubtreeColliderBuildOptions& options,
                                                       std::vector<ri::trace::TraceCollider>& outColliders);

/// Stable id: `node.name` if non-empty; else `prefix + '_' + decimal handle`.
[[nodiscard]] std::string MakeSubtreeColliderId(const Scene& scene,
                                               int nodeHandle,
                                               std::string_view idPrefix);

/// Remove colliders whose ids start with \p prefix (streaming teardown).
[[nodiscard]] std::size_t RemoveTraceCollidersWithIdPrefix(std::vector<ri::trace::TraceCollider>& colliders,
                                                          std::string_view prefix);

/// Remove colliders whose ids are contained in \p ids (exact match).
[[nodiscard]] std::size_t RemoveTraceCollidersWithIds(std::vector<ri::trace::TraceCollider>& colliders,
                                                     const std::vector<std::string_view>& ids);

/// Erase-if predicate (advanced teardown).
[[nodiscard]] std::size_t RemoveTraceCollidersIf(std::vector<ri::trace::TraceCollider>& colliders,
                                                const std::function<bool(const ri::trace::TraceCollider&)>& predicate);

} // namespace ri::scene
