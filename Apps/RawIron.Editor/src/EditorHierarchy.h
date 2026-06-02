#pragma once

#include "RawIron/Scene/Scene.h"

#include <optional>
#include <string>
#include <vector>

namespace ri::editor {

[[nodiscard]] std::vector<int> BuildHierarchyDrawOrder(const ri::scene::Scene& scene,
                                                       int omitSubtreeRoot = ri::scene::kInvalidHandle);
[[nodiscard]] std::optional<int> FindDrawOrderIndex(const std::vector<int>& order, std::size_t nodeIndex);
[[nodiscard]] std::vector<int> BuildFilteredHierarchyDrawOrder(const ri::scene::Scene& scene,
                                                               int omitSubtreeRoot,
                                                               const std::string& searchQuery);
[[nodiscard]] int ComputeVisibleHierarchyScrollTop(int currentScrollTop,
                                                   const std::vector<int>& order,
                                                   std::size_t selectedNode,
                                                   int visibleRows);

} // namespace ri::editor
