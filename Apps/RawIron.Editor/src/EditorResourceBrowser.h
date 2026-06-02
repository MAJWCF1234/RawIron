#pragma once

#include "EditorWorkspace.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ri::editor {

struct FilteredResourceView {
    std::vector<int> rows;
    int selectedVisibleRow = -1;
    int resetScrollTop = 0;
};

[[nodiscard]] int FindResourceRowByRelativePath(const std::vector<WorkspaceResourceEntry>& entries,
                                                std::string_view relativePath);

[[nodiscard]] FilteredResourceView BuildFilteredResourceRows(const std::vector<WorkspaceResourceEntry>& entries,
                                                            std::uint32_t categoryMask,
                                                            const std::string& searchQuery,
                                                            int selectedResourceRow);

[[nodiscard]] int ComputeVisibleResourceScrollTop(int currentScrollTop,
                                                  int selectedVisibleRow,
                                                  int totalFilteredRows,
                                                  int visibleRows);

} // namespace ri::editor
