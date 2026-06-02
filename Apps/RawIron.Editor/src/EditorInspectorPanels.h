#pragma once

#include <functional>
#include <optional>
#include <string>

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
struct GameplayPanelLayout {
    RECT inventoryModeRow{};
    RECT offHandRow{};
    RECT addTriggerBtn{};
    RECT exportBtn{};
    RECT playtestBtn{};
};

struct NodeInspectorPanelModel {
    bool renameTypingActive = false;
    std::string renameDraft;
    std::string nameLine;
    std::string pathLine;
    std::string kindLine;
    std::optional<std::string> meshPrimitiveLine;
    std::string localPosLine;
    std::string localRotLine;
    std::string localScaleLine;
    bool editableAuthored = false;
    std::string editModeLine;
    std::string groupingLine;
    std::string opsLine;
    std::string worldPosLine;
};

struct BrushInspectorPanelModel {
    std::string presetTitleLine;
    std::string headingLine;
    std::string helpLineA;
    std::string helpLineB;
    std::string selectionLine;
    std::string meshAttachedLine;
    std::string boundsSizeLine;
    std::string boundsCenterLine;
};

struct GameplayInspectorPanelModel {
    GameplayPanelLayout layout{};
    std::string headingLine;
    std::string summaryLine;
    std::string inventoryModeLine;
    std::string offHandLine;
    std::string gameplayStorageLine;
    std::string hotbarLine;
    std::string controlsLine;
};

[[nodiscard]] GameplayPanelLayout ComputeGameplayPanelLayout(const RECT& inspectorInner);
void RenderNodeInspectorPanel(HDC dc,
                              const RECT& inspectorInner,
                              const NodeInspectorPanelModel& model,
                              HFONT headerFont,
                              HFONT bodyFont,
                              HFONT smallFont,
                              const std::function<void(HDC, int&, const RECT&, const char*, int)>& drawNudgeRow);
void RenderBrushInspectorPanel(HDC dc,
                               const RECT& inspectorInner,
                               const BrushInspectorPanelModel& model,
                               HFONT headerFont,
                               HFONT bodyFont,
                               HFONT smallFont,
                               const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton);
void RenderGameplayInspectorPanel(HDC dc,
                                  const RECT& inspectorInner,
                                  const GameplayInspectorPanelModel& model,
                                  HFONT headerFont,
                                  HFONT bodyFont,
                                  HFONT smallFont,
                                  const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton);
#endif

} // namespace ri::editor
