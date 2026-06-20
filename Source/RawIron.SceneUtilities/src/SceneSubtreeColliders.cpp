#include "RawIron/Scene/SceneSubtreeColliders.h"

#include "RawIron/Scene/SceneUtils.h"

#include <algorithm>
#include <unordered_set>

namespace ri::scene {
namespace {

[[nodiscard]] bool ShouldEmitBlockingColliderForStructuralBrush(const Node& node) {
    if (node.structuralBrush.brushId.empty()) {
        return true;
    }
    switch (node.structuralBrush.collision) {
        case StructuralBrushCollisionPolicy::None:
        case StructuralBrushCollisionPolicy::Query:
        case StructuralBrushCollisionPolicy::Detail:
            return false;
        case StructuralBrushCollisionPolicy::Solid:
        case StructuralBrushCollisionPolicy::Player:
        case StructuralBrushCollisionPolicy::Custom:
            return true;
    }
    return true;
}

void AppendStructuralBrushSemanticTags(const StructuralBrushMetadata& metadata,
                                       std::vector<std::string>& tags) {
    if (metadata.brushId.empty()) {
        return;
    }
    tags.push_back("structural.brush:" + metadata.brushId);
    if (!metadata.region.empty()) {
        tags.push_back("structural.region:" + metadata.region);
    }
    tags.push_back("structural.operation:" + ToString(metadata.operation));
    tags.push_back("structural.role:" + ToString(metadata.role));
    tags.push_back("structural.collision:" + ToString(metadata.collision));
    tags.push_back("structural.visibility:" + ToString(metadata.visibility));
    tags.push_back("structural.navigation:" + ToString(metadata.navigation));
    tags.push_back("structural.rebuild:" + ToString(metadata.rebuildScope));
}

} // namespace

std::string MakeSubtreeColliderId(const Scene& scene, const int nodeHandle, const std::string_view idPrefix) {
    if (nodeHandle < 0 || static_cast<std::size_t>(nodeHandle) >= scene.NodeCount()) {
        return {};
    }
    const std::string& name = scene.GetNode(nodeHandle).name;
    if (!name.empty()) {
        return name;
    }
    std::string base(idPrefix.empty() ? "collider" : std::string(idPrefix));
    base.push_back('_');
    base += std::to_string(nodeHandle);
    return base;
}

std::size_t AppendTraceCollidersForSubtree(const Scene& scene,
                                           const int rootNodeHandle,
                                           const SubtreeColliderBuildOptions& options,
                                           std::vector<ri::trace::TraceCollider>& outColliders) {
    const std::vector<int> subtree = CollectNodeSubtree(scene, rootNodeHandle, true);
    std::size_t added = 0U;
    for (const int nodeHandle : subtree) {
        const Node& node = scene.GetNode(nodeHandle);
        if (options.respectStructuralBrushCollisionPolicy
            && !ShouldEmitBlockingColliderForStructuralBrush(node)) {
            continue;
        }
        if (options.requireStructuralBrushQueryMeshChannel
            && !node.structuralBrush.brushId.empty()
            && !StructuralBrushParticipatesInChannel(node.structuralBrush,
                                                     StructuralBrushChannel::QueryMesh)) {
            continue;
        }
        const std::optional<ri::spatial::Aabb> bounds = TryComputeMeshNodeWorldAabb(scene, nodeHandle);
        if (!bounds.has_value()) {
            if (options.skipNodesWithoutBounds) {
                continue;
            }
            continue;
        }
        std::string id = MakeSubtreeColliderId(scene, nodeHandle, options.idPrefix);
        ri::trace::TraceCollider collider{};
        collider.id = std::move(id);
        collider.bounds = *bounds;
        collider.structural = options.structural;
        collider.dynamic = options.dynamic;
        collider.simulationTags = options.extraSimulationTags;
        if (options.appendStructuralBrushSemanticTags) {
            AppendStructuralBrushSemanticTags(node.structuralBrush, collider.simulationTags);
        }
        collider.simulationFlags = options.simulationFlagsOrMask;
        outColliders.push_back(std::move(collider));
        ++added;
    }
    return added;
}

std::size_t RemoveTraceCollidersWithIdPrefix(std::vector<ri::trace::TraceCollider>& colliders,
                                             const std::string_view prefix) {
    if (prefix.empty()) {
        return 0U;
    }
    const auto matchesPrefix = [prefix](const ri::trace::TraceCollider& c) {
        return c.id.size() >= prefix.size()
            && std::string_view(c.id.data(), prefix.size()) == prefix;
    };
    const std::size_t before = colliders.size();
    colliders.erase(std::remove_if(colliders.begin(), colliders.end(), matchesPrefix), colliders.end());
    return before - colliders.size();
}

std::size_t RemoveTraceCollidersWithIds(std::vector<ri::trace::TraceCollider>& colliders,
                                       const std::vector<std::string_view>& ids) {
    std::unordered_set<std::string> wanted;
    wanted.reserve(ids.size());
    for (const std::string_view id : ids) {
        wanted.insert(std::string(id));
    }
    const std::size_t before = colliders.size();
    colliders.erase(std::remove_if(colliders.begin(),
                                   colliders.end(),
                                   [&](const ri::trace::TraceCollider& c) {
                                       return wanted.find(c.id) != wanted.end();
                                   }),
                    colliders.end());
    return before - colliders.size();
}

std::size_t RemoveTraceCollidersIf(std::vector<ri::trace::TraceCollider>& colliders,
                                   const std::function<bool(const ri::trace::TraceCollider&)>& predicate) {
    const std::size_t before = colliders.size();
    colliders.erase(std::remove_if(colliders.begin(), colliders.end(), predicate), colliders.end());
    return before - colliders.size();
}

} // namespace ri::scene
