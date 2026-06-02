#pragma once

#include "EditorWorkspace.h"

#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace ri::editor {

#if defined(_WIN32)
struct ProjectShortcutLayout {
    RECT manifest{};
    RECT level{};
    RECT gameplay{};
    RECT rendering{};
    RECT uiLayout{};
    RECT uiStyle{};
    RECT menu{};
    RECT ai{};
    RECT network{};
    RECT plugins{};
};

struct FilesInspectorPanelModel {
    std::string heading;
    std::string sectionLabel;
    std::string selectedPath;
    std::string categoryLabel;
    std::string manifestStatus;
    std::vector<std::string> manifestIssues;
    std::string emptySelectionMessage;
    std::string auxMessage;
    std::string saveLabel;
    std::string footerHint;
    bool hasSelection = false;
    bool manifestOk = false;
};

[[nodiscard]] ProjectShortcutLayout ComputeProjectShortcutLayout(const RECT& inspectorInner);
[[nodiscard]] FilesInspectorPanelModel BuildFilesInspectorPanelModel(
    const WorkspaceResourceEntry* selectedEntry,
    const std::vector<std::string>& manifestIssues,
    const std::string& auxMessage,
    bool resourceFileDirty,
    const std::string& resourceFocusSummary);
#endif

} // namespace ri::editor
