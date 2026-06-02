#include "EditorHierarchy.h"

#include "RawIron/Scene/Helpers.h"

#include <algorithm>
#include <cctype>

namespace ri::editor {

namespace {

void AppendHierarchyDfs(const ri::scene::Scene& scene,
                        int nodeIndex,
                        std::vector<int>& out,
                        int omitSubtreeRoot) {
    if (omitSubtreeRoot >= 0 && nodeIndex == omitSubtreeRoot) {
        return;
    }
    out.push_back(nodeIndex);
    const ri::scene::Node& node = scene.GetNode(nodeIndex);
    for (const int child : node.children) {
        if (omitSubtreeRoot >= 0 && child == omitSubtreeRoot) {
            continue;
        }
        AppendHierarchyDfs(scene, child, out, omitSubtreeRoot);
    }
}

std::string ToLowerAscii(const std::string& text) {
    std::string lowered = text;
    for (char& ch : lowered) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return lowered;
}

} // namespace

std::vector<int> BuildHierarchyDrawOrder(const ri::scene::Scene& scene, const int omitSubtreeRoot) {
    std::vector<int> order;
    order.reserve(scene.NodeCount());
    const std::vector<int> roots = ri::scene::CollectRootNodes(scene);
    for (const int root : roots) {
        AppendHierarchyDfs(scene, root, order, omitSubtreeRoot);
    }
    return order;
}

std::optional<int> FindDrawOrderIndex(const std::vector<int>& order, const std::size_t nodeIndex) {
    const int needle = static_cast<int>(nodeIndex);
    for (int i = 0; i < static_cast<int>(order.size()); ++i) {
        if (order[static_cast<std::size_t>(i)] == needle) {
            return i;
        }
    }
    return std::nullopt;
}

std::vector<int> BuildFilteredHierarchyDrawOrder(const ri::scene::Scene& scene,
                                                 const int omitSubtreeRoot,
                                                 const std::string& searchQuery) {
    const std::vector<int> fullOrder = BuildHierarchyDrawOrder(scene, omitSubtreeRoot);
    if (searchQuery.empty()) {
        return fullOrder;
    }

    const std::string loweredNeedle = ToLowerAscii(searchQuery);
    std::vector<int> filtered;
    filtered.reserve(fullOrder.size());
    for (const int nodeIndex : fullOrder) {
        if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= scene.NodeCount()) {
            continue;
        }
        const ri::scene::Node& node = scene.GetNode(nodeIndex);
        const std::string haystack = ToLowerAscii(std::to_string(nodeIndex) + " " + node.name);
        if (haystack.find(loweredNeedle) != std::string::npos) {
            filtered.push_back(nodeIndex);
        }
    }
    return filtered;
}

int ComputeVisibleHierarchyScrollTop(const int currentScrollTop,
                                     const std::vector<int>& order,
                                     const std::size_t selectedNode,
                                     const int visibleRows) {
    if (order.empty()) {
        return 0;
    }
    const int clampedVisibleRows = std::max(1, visibleRows);
    const int maxScroll = std::max(0, static_cast<int>(order.size()) - clampedVisibleRows);
    int scrollTop = std::clamp(currentScrollTop, 0, maxScroll);
    const std::optional<int> pos = FindDrawOrderIndex(order, selectedNode);
    if (!pos.has_value()) {
        return scrollTop;
    }
    if (*pos < scrollTop) {
        scrollTop = *pos;
    } else if (*pos >= scrollTop + clampedVisibleRows) {
        scrollTop = *pos - clampedVisibleRows + 1;
    }
    return std::clamp(scrollTop, 0, maxScroll);
}

} // namespace ri::editor
