#include "EditorFilesInspector.h"

#include "EditorInspectorPanels.h"
#include "EditorRenderer.h"

#include <algorithm>

namespace ri::editor {

#if defined(_WIN32)

int ComputeProjectHealthCardHeight(const FilesInspectorPanelModel& model) {
    if (!model.showProjectHealth) {
        return 0;
    }
    int height = 52;
    height += static_cast<int>(std::min<std::size_t>(model.projectHealthWarnings.size(), 4U)) * 18;
    return height;
}

FilesInspectorLayout ComputeFilesInspectorLayout(const RECT& inspectorInner) {
    FilesInspectorLayout layout{};
    const int left = inspectorInner.left + 10;
    const int right = inspectorInner.right - 10;
    const int gap = 6;
    const int width = (right - left - (gap * 2)) / 3;
    const int row0 = inspectorInner.top + 48 + 26 + 34;
    const int row1 = row0 + 30;
    const int row2 = row1 + 30;
    layout.shortcuts.manifest = RECT{left, row0, left + width, row0 + 24};
    layout.shortcuts.level = RECT{left + width + gap, row0, left + width * 2 + gap, row0 + 24};
    layout.shortcuts.gameplay = RECT{left + width * 2 + gap * 2, row0, right, row0 + 24};
    layout.shortcuts.rendering = RECT{left, row1, left + width, row1 + 24};
    layout.shortcuts.uiLayout = RECT{left + width + gap, row1, left + width * 2 + gap, row1 + 24};
    layout.shortcuts.uiStyle = RECT{left + width * 2 + gap * 2, row1, right, row1 + 24};
    layout.shortcuts.menu = RECT{left, row2, left + width, row2 + 24};
    layout.shortcuts.ai = RECT{left + width + gap, row2, left + width * 2 + gap, row2 + 24};
    layout.shortcuts.network = RECT{left + width * 2 + gap * 2, row2, right, row2 + 24};
    layout.shortcuts.plugins = RECT{left, row2 + 30, left + width, row2 + 54};
    const int actionTop = InspectorContentBottom(inspectorInner) - 34;
    layout.saveBtn = RECT{left, actionTop, left + 108, actionTop + 26};
    layout.explorerBtn = RECT{left + 114, actionTop, right, actionTop + 26};
    layout.footerText = RECT{left, actionTop - 18, right, actionTop - 2};
    return layout;
}

ProjectShortcutLayout ComputeProjectShortcutLayout(const RECT& inspectorInner) {
    return ComputeFilesInspectorLayout(inspectorInner).shortcuts;
}

int ComputeFilesInspectorTextEditorTop(const RECT& inspectorInner, const FilesInspectorPanelModel& model) {
    const FilesInspectorLayout layout = ComputeFilesInspectorLayout(inspectorInner);
    int infoTop = layout.shortcuts.plugins.bottom + 18;
    if (model.showProjectHealth) {
        infoTop += ComputeProjectHealthCardHeight(model) + 10;
    }
    if (model.hasSelection) {
        int fileCardBottom = infoTop + 112 + static_cast<int>(model.manifestIssues.size()) * 34;
        fileCardBottom = std::min<int>(fileCardBottom, static_cast<int>(layout.footerText.top) - 10);
        infoTop = fileCardBottom + 10;
    } else {
        const int emptyBottom = std::min<int>(infoTop + 68, static_cast<int>(layout.footerText.top) - 10);
        infoTop = emptyBottom + 10;
    }
    if (!model.auxMessage.empty() && infoTop + 56 < layout.footerText.top) {
        infoTop += 66;
    }
    return infoTop;
}

FilesInspectorPanelModel BuildFilesInspectorPanelModel(const WorkspaceResourceEntry* selectedEntry,
                                                       const std::vector<std::string>& manifestIssues,
                                                       const std::string& auxMessage,
                                                       const bool resourceFileDirty,
                                                       const std::string& resourceFocusSummary) {
    FilesInspectorPanelModel model{};
    model.heading = "Project workspace";
    model.sectionLabel = "Jump to core files";
    model.emptySelectionMessage = "Choose a file from Project Archive to inspect or edit it here.";
    model.auxMessage = auxMessage;
    model.saveLabel = resourceFileDirty ? "Save*" : "Save";
    model.footerHint = "Ctrl+S saves the active resource. Ctrl+Shift+M scaffolds missing project files. Focus: "
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

void RenderFilesInspectorPanel(HDC dc,
                               const RECT& inspectorInner,
                               const FilesInspectorPanelModel& model,
                               HFONT headerFont,
                               HFONT smallFont,
                               const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton) {
    const FilesInspectorLayout layout = ComputeFilesInspectorLayout(inspectorInner);
    const int contentBottom = InspectorContentBottom(inspectorInner);
    const int savedDc = SaveDC(dc);
    IntersectClipRect(dc, inspectorInner.left, inspectorInner.top, inspectorInner.right, contentBottom);

    int infoTop = inspectorInner.top + 48;
    EditorRenderer::DrawTextLine(dc,
                                 RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 22},
                                 model.heading,
                                 RGB(224, 224, 236),
                                 headerFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    infoTop += 26;
    const RECT shortcutCard{
        inspectorInner.left + 10, infoTop + 2, inspectorInner.right - 10, layout.shortcuts.plugins.bottom + 12};
    EditorRenderer::DrawInsetFrame(dc, shortcutCard, RGB(54, 60, 70), RGB(160, 166, 174), RGB(22, 24, 30));
    EditorRenderer::FillRectColor(
        dc, RECT{shortcutCard.left + 1, shortcutCard.top + 1, shortcutCard.right - 1, shortcutCard.top + 5}, RGB(124, 150, 198));
    EditorRenderer::DrawTextLine(dc,
                                 RECT{inspectorInner.left + 20, infoTop + 10, inspectorInner.right - 20, infoTop + 28},
                                 model.sectionLabel,
                                 RGB(210, 214, 220),
                                 smallFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    drawToolbarButton(dc, layout.shortcuts.manifest, "Manifest", false);
    drawToolbarButton(dc, layout.shortcuts.level, "Level", false);
    drawToolbarButton(dc, layout.shortcuts.gameplay, "Gameplay", false);
    drawToolbarButton(dc, layout.shortcuts.rendering, "Render", false);
    drawToolbarButton(dc, layout.shortcuts.uiLayout, "UI Flow", false);
    drawToolbarButton(dc, layout.shortcuts.uiStyle, "VN Flow", false);
    drawToolbarButton(dc, layout.shortcuts.menu, "Menu", false);
    drawToolbarButton(dc, layout.shortcuts.ai, "AI", false);
    drawToolbarButton(dc, layout.shortcuts.network, "Network", false);
    drawToolbarButton(dc, layout.shortcuts.plugins, "Plugins", false);
    infoTop = layout.shortcuts.plugins.bottom + 18;

    if (model.showProjectHealth) {
        const int healthHeight = ComputeProjectHealthCardHeight(model);
        const RECT healthCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + healthHeight};
        EditorRenderer::DrawInsetFrame(dc, healthCard, RGB(52, 58, 48), RGB(156, 170, 138), RGB(20, 24, 20));
        EditorRenderer::FillRectColor(
            dc, RECT{healthCard.left + 1, healthCard.top + 1, healthCard.right - 1, healthCard.top + 5}, RGB(118, 168, 128));
        EditorRenderer::DrawTextLine(dc,
                                     RECT{healthCard.left + 10, healthCard.top + 8, healthCard.right - 10, healthCard.top + 26},
                                     "Project readiness",
                                     RGB(214, 232, 210),
                                     headerFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        int healthLineTop = healthCard.top + 28;
        if (!model.projectHealthReadyLine.empty()) {
            EditorRenderer::DrawTextLine(dc,
                                         RECT{healthCard.left + 10, healthLineTop, healthCard.right - 10, healthLineTop + 18},
                                         model.projectHealthReadyLine,
                                         RGB(170, 220, 176),
                                         smallFont,
                                         DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            healthLineTop += 20;
        }
        for (std::size_t index = 0; index < model.projectHealthWarnings.size() && index < 4U; ++index) {
            EditorRenderer::DrawTextLine(dc,
                                         RECT{healthCard.left + 10, healthLineTop, healthCard.right - 10, healthLineTop + 18},
                                         "• " + model.projectHealthWarnings[index],
                                         RGB(255, 196, 140),
                                         smallFont,
                                         DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            healthLineTop += 18;
        }
        infoTop = healthCard.bottom + 10;
    }

    if (model.hasSelection) {
        int fileCardBottom = infoTop + 112;
        fileCardBottom += static_cast<int>(model.manifestIssues.size()) * 34;
        fileCardBottom = std::min<int>(fileCardBottom, static_cast<int>(layout.footerText.top) - 10);
        const RECT fileCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, fileCardBottom};
        EditorRenderer::DrawInsetFrame(dc, fileCard, RGB(54, 58, 68), RGB(160, 166, 176), RGB(20, 24, 30));
        EditorRenderer::FillRectColor(
            dc, RECT{fileCard.left + 1, fileCard.top + 1, fileCard.right - 1, fileCard.top + 5}, RGB(106, 154, 122));
        EditorRenderer::DrawTextLine(dc,
                                     RECT{fileCard.left + 10, fileCard.top + 10, fileCard.right - 10, fileCard.top + 28},
                                     model.selectedPath,
                                     RGB(228, 236, 248),
                                     smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        EditorRenderer::DrawTextLine(dc,
                                     RECT{fileCard.left + 10, fileCard.top + 34, fileCard.right - 10, fileCard.top + 52},
                                     model.categoryLabel,
                                     RGB(208, 212, 220),
                                     smallFont,
                                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        int issueTop = fileCard.top + 58;
        if (!model.manifestStatus.empty()) {
            EditorRenderer::DrawTextLine(dc,
                                         RECT{fileCard.left + 10, issueTop, fileCard.right - 10, issueTop + 18},
                                         model.manifestStatus,
                                         model.manifestOk ? RGB(160, 220, 170) : RGB(255, 180, 120),
                                         smallFont,
                                         DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            issueTop += 20;
            for (const std::string& issue : model.manifestIssues) {
                EditorRenderer::DrawTextLine(dc,
                                             RECT{fileCard.left + 14, issueTop, fileCard.right - 10, issueTop + 32},
                                             issue,
                                             RGB(230, 190, 150),
                                             smallFont,
                                             DT_LEFT | DT_WORDBREAK);
                issueTop += 34;
            }
        }
        infoTop = fileCard.bottom + 10;
    } else {
        const int emptyBottom = std::min<int>(infoTop + 68, static_cast<int>(layout.footerText.top) - 10);
        const RECT emptyCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, emptyBottom};
        EditorRenderer::DrawInsetFrame(dc, emptyCard, RGB(54, 58, 68), RGB(156, 162, 170), RGB(22, 24, 30));
        EditorRenderer::FillRectColor(
            dc, RECT{emptyCard.left + 1, emptyCard.top + 1, emptyCard.right - 1, emptyCard.top + 5}, RGB(108, 116, 132));
        EditorRenderer::DrawTextLine(dc,
                                     RECT{emptyCard.left + 10, emptyCard.top + 12, emptyCard.right - 10, emptyCard.bottom - 10},
                                     model.emptySelectionMessage,
                                     RGB(180, 180, 190),
                                     smallFont,
                                     DT_LEFT | DT_WORDBREAK);
        infoTop = emptyCard.bottom + 10;
    }

    if (!model.auxMessage.empty() && infoTop + 56 < layout.footerText.top) {
        const RECT auxCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 56};
        EditorRenderer::DrawInsetFrame(dc, auxCard, RGB(68, 60, 54), RGB(184, 162, 138), RGB(26, 22, 20));
        EditorRenderer::FillRectColor(
            dc, RECT{auxCard.left + 1, auxCard.top + 1, auxCard.right - 1, auxCard.top + 5}, RGB(204, 145, 60));
        EditorRenderer::DrawTextLine(dc,
                                     RECT{auxCard.left + 10, auxCard.top + 10, auxCard.right - 10, auxCard.bottom - 10},
                                     model.auxMessage,
                                     RGB(220, 160, 120),
                                     smallFont,
                                     DT_LEFT | DT_WORDBREAK);
    }

    EditorRenderer::DrawTextLine(
        dc, layout.footerText, model.footerHint, RGB(200, 196, 160), smallFont, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    drawToolbarButton(dc, layout.saveBtn, model.saveLabel, false);
    drawToolbarButton(dc, layout.explorerBtn, "Explorer", false);

    RestoreDC(dc, savedDc);
}
#endif

} // namespace ri::editor
