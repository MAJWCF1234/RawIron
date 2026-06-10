#pragma once

#include "EditorWorkspace.h"

#include <functional>
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
    bool showProjectHealth = false;
    std::string projectHealthReadyLine;
    std::vector<std::string> projectHealthWarnings;
};

struct FilesInspectorLayout {
    ProjectShortcutLayout shortcuts{};
    RECT saveBtn{};
    RECT explorerBtn{};
    RECT footerText{};
    int textEditorTop = 0;
};

[[nodiscard]] int ComputeProjectHealthCardHeight(const FilesInspectorPanelModel& model);

[[nodiscard]] int ComputeFilesInspectorTextEditorTop(const RECT& inspectorInner,
                                                       const FilesInspectorPanelModel& model);

[[nodiscard]] FilesInspectorLayout ComputeFilesInspectorLayout(const RECT& inspectorInner);
[[nodiscard]] ProjectShortcutLayout ComputeProjectShortcutLayout(const RECT& inspectorInner);
[[nodiscard]] FilesInspectorPanelModel BuildFilesInspectorPanelModel(
    const WorkspaceResourceEntry* selectedEntry,
    const std::vector<std::string>& manifestIssues,
    const std::string& auxMessage,
    bool resourceFileDirty,
    const std::string& resourceFocusSummary);
void RenderFilesInspectorPanel(HDC dc,
                               const RECT& inspectorInner,
                               const FilesInspectorPanelModel& model,
                               HFONT headerFont,
                               HFONT smallFont,
                               const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton);
#endif

} // namespace ri::editor
