#pragma once

#include "EditorNewGame.h"

#include <cstddef>
#include <functional>
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

enum class CreatorAtmospherePreset {
    ClearDay,
    GoldenHour,
    FoggyVoid,
    NightStudio,
};

enum class CreatorInsertPreset {
    GroundPlate,
    RockCluster,
    WaterSurface,
    SkyBackdrop,
    PortalArch,
};

enum class CreatorCameraPreset {
    Hero,
    TopDown,
    LowAngle,
};

enum class CreatorPanelHitType {
    None,
    TemplateEmpty,
    TemplateOutdoor,
    TemplateInterior,
    NamePrev,
    NameNext,
    CreateProject,
    AtmosphereMenu,
    InsertMenu,
    CameraMenu,
    SetupFiles,
};

struct CreatorPanelHit {
    CreatorPanelHitType type = CreatorPanelHitType::None;
};

struct CreatorPanelLayout {
    RECT templateEmptyBtn{};
    RECT templateOutdoorBtn{};
    RECT templateInteriorBtn{};
    RECT namePrevBtn{};
    RECT nameNextBtn{};
    RECT createProjectBtn{};
    RECT atmosphereMenuBtn{};
    RECT insertMenuBtn{};
    RECT cameraMenuBtn{};
    RECT setupFilesBtn{};
};

struct CreatorPanelModel {
    NewGameTemplate selectedTemplate = NewGameTemplate::EmptyStudio;
    std::string displayNameDraft;
    std::string slugPreview;
    std::string mountedGameLabel;
    bool hasMountedGame = false;
    CreatorAtmospherePreset selectedAtmosphere = CreatorAtmospherePreset::ClearDay;
    CreatorInsertPreset selectedInsert = CreatorInsertPreset::GroundPlate;
    CreatorCameraPreset selectedCamera = CreatorCameraPreset::Hero;
};

struct CreatorPanelDispatchCallbacks {
    std::function<void(NewGameTemplate)> onSelectTemplate;
    std::function<void(int)> onCycleNameVariant;
    std::function<void()> onCreateProject;
    std::function<void(CreatorAtmospherePreset)> onApplyAtmosphere;
    std::function<void(CreatorInsertPreset)> onInsertPreset;
    std::function<void(CreatorCameraPreset)> onApplyCamera;
    std::function<void()> onSetupFiles;
    std::function<void()> onCycleAtmosphereMenu;
    std::function<void()> onCycleInsertMenu;
    std::function<void()> onCycleCameraMenu;
};

#if defined(_WIN32)
[[nodiscard]] RECT CreateTabRect(const RECT& hierarchyInner);
[[nodiscard]] CreatorPanelLayout ComputeCreatorPanelLayout(const RECT& hierarchyInner);
[[nodiscard]] CreatorPanelHit HitTestCreatorPanel(const CreatorPanelLayout& layout, const POINT& point);
[[nodiscard]] bool DispatchCreatorPanelClick(const RECT& hierarchyInner,
                                             const POINT& point,
                                             const CreatorPanelDispatchCallbacks& callbacks);
void RenderCreatorPanel(HDC dc,
                          const RECT& hierarchyInner,
                          const CreatorPanelLayout& layout,
                          const CreatorPanelModel& model,
                          HFONT headerFont,
                          HFONT bodyFont,
                          HFONT smallFont,
                          const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton);
#endif

[[nodiscard]] std::string AtmospherePresetLabel(CreatorAtmospherePreset preset);
[[nodiscard]] std::string InsertPresetLabel(CreatorInsertPreset preset);
[[nodiscard]] std::string CameraPresetLabel(CreatorCameraPreset preset);
[[nodiscard]] CreatorAtmospherePreset CycleCreatorAtmosphere(CreatorAtmospherePreset current);
[[nodiscard]] CreatorInsertPreset CycleCreatorInsert(CreatorInsertPreset current);
[[nodiscard]] CreatorCameraPreset CycleCreatorCamera(CreatorCameraPreset current);
[[nodiscard]] bool ApplyAtmospherePreset(const std::filesystem::path& gameRoot,
                                         CreatorAtmospherePreset preset,
                                         std::string* errorOut);

} // namespace ri::editor
