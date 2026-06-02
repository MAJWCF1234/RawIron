#include "EditorResourceBrowser.h"

#include <algorithm>
#include <cctype>

namespace ri::editor {

namespace {

std::string ToLowerAscii(const std::string& text) {
    std::string lowered = text;
    for (char& ch : lowered) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return lowered;
}

} // namespace

int FindResourceRowByRelativePath(const std::vector<WorkspaceResourceEntry>& entries,
                                  std::string_view relativePath) {
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        if (entries[static_cast<std::size_t>(i)].relativePathUtf8 == relativePath) {
            return i;
        }
    }
    return -1;
}

FilteredResourceView BuildFilteredResourceRows(const std::vector<WorkspaceResourceEntry>& entries,
                                               const std::uint32_t categoryMask,
                                               const std::string& searchQuery,
                                               const int selectedResourceRow) {
    FilteredResourceView view{};
    const std::string loweredNeedle = ToLowerAscii(searchQuery);
    view.rows.reserve(entries.size());
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        const WorkspaceResourceEntry& entry = entries[static_cast<std::size_t>(i)];
        if ((categoryMask & WorkspaceCategoryBit(entry.category)) == 0u) {
            continue;
        }
        if (!loweredNeedle.empty()) {
            const std::string loweredPath = ToLowerAscii(entry.relativePathUtf8);
            if (loweredPath.find(loweredNeedle) == std::string::npos) {
                continue;
            }
        }
        view.rows.push_back(i);
    }

    if (selectedResourceRow >= 0) {
        for (int i = 0; i < static_cast<int>(view.rows.size()); ++i) {
            if (view.rows[static_cast<std::size_t>(i)] == selectedResourceRow) {
                view.selectedVisibleRow = i;
                break;
            }
        }
    }
    if (view.selectedVisibleRow < 0) {
        view.resetScrollTop = 0;
    }
    return view;
}

int ComputeVisibleResourceScrollTop(const int currentScrollTop,
                                    const int selectedVisibleRow,
                                    const int totalFilteredRows,
                                    const int visibleRows) {
    const int clampedVisibleRows = std::max(1, visibleRows);
    const int maxScroll = std::max(0, totalFilteredRows - clampedVisibleRows);
    int scrollTop = std::clamp(currentScrollTop, 0, maxScroll);
    if (selectedVisibleRow < 0) {
        return scrollTop;
    }
    if (selectedVisibleRow < scrollTop) {
        scrollTop = selectedVisibleRow;
    } else if (selectedVisibleRow >= scrollTop + clampedVisibleRows) {
        scrollTop = selectedVisibleRow - clampedVisibleRows + 1;
    }
    return std::clamp(scrollTop, 0, maxScroll);
}

} // namespace ri::editor
