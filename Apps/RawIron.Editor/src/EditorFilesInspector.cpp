#include "EditorFilesInspector.h"

namespace ri::editor {

#if defined(_WIN32)
ProjectShortcutLayout ComputeProjectShortcutLayout(const RECT& inspectorInner) {
    ProjectShortcutLayout layout{};
    const int left = inspectorInner.left + 10;
    const int right = inspectorInner.right - 10;
    const int gap = 6;
    const int width = (right - left - (gap * 2)) / 3;
    const int row0 = inspectorInner.top + 118;
    const int row1 = row0 + 30;
    const int row2 = row1 + 30;
    layout.manifest = RECT{left, row0, left + width, row0 + 24};
    layout.level = RECT{left + width + gap, row0, left + width * 2 + gap, row0 + 24};
    layout.gameplay = RECT{left + width * 2 + gap * 2, row0, right, row0 + 24};
    layout.rendering = RECT{left, row1, left + width, row1 + 24};
    layout.uiLayout = RECT{left + width + gap, row1, left + width * 2 + gap, row1 + 24};
    layout.uiStyle = RECT{left + width * 2 + gap * 2, row1, right, row1 + 24};
    layout.menu = RECT{left, row2, left + width, row2 + 24};
    layout.ai = RECT{left + width + gap, row2, left + width * 2 + gap, row2 + 24};
    layout.network = RECT{left + width * 2 + gap * 2, row2, right, row2 + 24};
    layout.plugins = RECT{left, row2 + 30, left + width, row2 + 54};
    return layout;
}

FilesInspectorPanelModel BuildFilesInspectorPanelModel(const WorkspaceResourceEntry* selectedEntry,
                                                       const std::vector<std::string>& manifestIssues,
                                                       const std::string& auxMessage,
                                                       const bool resourceFileDirty,
                                                       const std::string& resourceFocusSummary) {
    FilesInspectorPanelModel model{};
    model.heading = "Project files (levels, scripts, config, data, AI, plugins, UI, assets)";
    model.sectionLabel = "Project shortcuts";
    model.emptySelectionMessage = "Pick a file in Resources, or switch tabs for scene nodes.";
    model.auxMessage = auxMessage;
    model.saveLabel = resourceFileDirty ? "Save*" : "Save";
    model.footerHint =
        "Ctrl+S saves resource when Files + modified. Ctrl+Shift+M scaffolds missing project files. Key 4 opens Files tab. Focus: "
        + resourceFocusSummary;

    if (selectedEntry == nullptr) {
        return model;
    }

    model.hasSelection = true;
    model.selectedPath = selectedEntry->relativePathUtf8;
    model.categoryLabel = "Category: " + WorkspaceCategoryLabel(selectedEntry->category);
    if (!manifestIssues.empty()) {
        model.manifestStatus =
            "Manifest validation: " + std::to_string(manifestIssues.size()) + " issue(s)";
        for (std::size_t i = 0; i < manifestIssues.size() && i < 4U; ++i) {
            model.manifestIssues.push_back("• " + manifestIssues[i]);
        }
    } else if (selectedEntry->category == WorkspaceResourceCategory::Manifest) {
        model.manifestStatus = "Manifest validation: OK";
        model.manifestOk = true;
    }

    return model;
}
#endif

} // namespace ri::editor
