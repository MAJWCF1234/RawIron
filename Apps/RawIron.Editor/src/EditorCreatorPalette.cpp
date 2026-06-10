#include "EditorCreatorPalette.h"

#include "EditorRenderer.h"
#include "EditorUiTheme.h"

#include "RawIron/Content/ScriptScalars.h"
#include "RawIron/Core/Detail/JsonScan.h"

#include <algorithm>

namespace ri::editor {

namespace fs = std::filesystem;

namespace {

constexpr int kLeftPanelTabHeight = 24;

[[nodiscard]] RECT CreatorCardRect(const RECT& hierarchyInner) {
    return RECT{hierarchyInner.left + 4, hierarchyInner.top + 34, hierarchyInner.right - 4, hierarchyInner.bottom - 8};
}

[[nodiscard]] bool HitRect(const POINT& point, const RECT& rect) {
    return PtInRect(&rect, point) != FALSE;
}

[[nodiscard]] CreatorPanelHit HitFromRect(const POINT& point, const RECT& rect, const CreatorPanelHitType type) {
    if (HitRect(point, rect)) {
        return {.type = type};
    }
    return {};
}

void DrawSectionLabel(HDC dc, const RECT& rect, const std::string& text, HFONT font) {
    EditorRenderer::DrawTextLine(dc, rect, text, EditorUiTheme::kCreatorSection, font, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
}

} // namespace

RECT CreateTabRect(const RECT& hierarchyInner) {
    return RECT{
        hierarchyInner.left + 72,
        hierarchyInner.top + 4,
        hierarchyInner.left + 136,
        hierarchyInner.top + 4 + kLeftPanelTabHeight,
    };
}

CreatorPanelLayout ComputeCreatorPanelLayout(const RECT& hierarchyInner) {
    const RECT card = CreatorCardRect(hierarchyInner);
    CreatorPanelLayout layout{};
    const int left = card.left + 10;
    const int right = card.right - 10;
    const int width = std::max(40, (right - left - 8) / 3);

    int top = card.top + 10 + 22 + 40;
    top += 22;
    layout.templateEmptyBtn = RECT{left, top, left + width, top + 28};
    layout.templateOutdoorBtn = RECT{left + width + 4, top, left + 2 * width + 4, top + 28};
    layout.templateInteriorBtn = RECT{left + 2 * width + 8, top, right, top + 28};
    top = layout.templateInteriorBtn.bottom + 10;

    top += 22;
    layout.namePrevBtn = RECT{left, top + 22, left + 24, top + 46};
    layout.nameNextBtn = RECT{left + 28, top + 22, left + 52, top + 46};
    layout.createProjectBtn = RECT{left + 56, top + 22, right, top + 50};
    top = layout.createProjectBtn.bottom + 12;

    top += 22;
    layout.atmosphereMenuBtn = RECT{left, top, right, top + 26};
    top = layout.atmosphereMenuBtn.bottom + 10;

    top += 22;
    layout.insertMenuBtn = RECT{left, top, right, top + 26};
    top = layout.insertMenuBtn.bottom + 10;

    top += 22;
    layout.cameraMenuBtn = RECT{left, top, right, top + 26};
    top = layout.cameraMenuBtn.bottom + 12;

    layout.setupFilesBtn = RECT{left, top, right, top + 28};
    return layout;
}

CreatorPanelHit HitTestCreatorPanel(const CreatorPanelLayout& layout, const POINT& point) {
    const CreatorPanelHit hits[] = {
        HitFromRect(point, layout.templateEmptyBtn, CreatorPanelHitType::TemplateEmpty),
        HitFromRect(point, layout.templateOutdoorBtn, CreatorPanelHitType::TemplateOutdoor),
        HitFromRect(point, layout.templateInteriorBtn, CreatorPanelHitType::TemplateInterior),
        HitFromRect(point, layout.namePrevBtn, CreatorPanelHitType::NamePrev),
        HitFromRect(point, layout.nameNextBtn, CreatorPanelHitType::NameNext),
        HitFromRect(point, layout.createProjectBtn, CreatorPanelHitType::CreateProject),
        HitFromRect(point, layout.atmosphereMenuBtn, CreatorPanelHitType::AtmosphereMenu),
        HitFromRect(point, layout.insertMenuBtn, CreatorPanelHitType::InsertMenu),
        HitFromRect(point, layout.cameraMenuBtn, CreatorPanelHitType::CameraMenu),
        HitFromRect(point, layout.setupFilesBtn, CreatorPanelHitType::SetupFiles),
    };
    for (const CreatorPanelHit& hit : hits) {
        if (hit.type != CreatorPanelHitType::None) {
            return hit;
        }
    }
    return {};
}

bool DispatchCreatorPanelClick(const RECT& hierarchyInner,
                               const POINT& point,
                               const CreatorPanelDispatchCallbacks& callbacks) {
    const CreatorPanelLayout layout = ComputeCreatorPanelLayout(hierarchyInner);
    switch (HitTestCreatorPanel(layout, point).type) {
        case CreatorPanelHitType::TemplateEmpty:
            if (callbacks.onSelectTemplate) callbacks.onSelectTemplate(NewGameTemplate::EmptyStudio);
            return true;
        case CreatorPanelHitType::TemplateOutdoor:
            if (callbacks.onSelectTemplate) callbacks.onSelectTemplate(NewGameTemplate::OutdoorScene);
            return true;
        case CreatorPanelHitType::TemplateInterior:
            if (callbacks.onSelectTemplate) callbacks.onSelectTemplate(NewGameTemplate::InteriorRoom);
            return true;
        case CreatorPanelHitType::NamePrev:
            if (callbacks.onCycleNameVariant) callbacks.onCycleNameVariant(-1);
            return true;
        case CreatorPanelHitType::NameNext:
            if (callbacks.onCycleNameVariant) callbacks.onCycleNameVariant(1);
            return true;
        case CreatorPanelHitType::CreateProject:
            if (callbacks.onCreateProject) callbacks.onCreateProject();
            return true;
        case CreatorPanelHitType::AtmosphereMenu:
            if (callbacks.onCycleAtmosphereMenu) callbacks.onCycleAtmosphereMenu();
            return true;
        case CreatorPanelHitType::InsertMenu:
            if (callbacks.onCycleInsertMenu) callbacks.onCycleInsertMenu();
            return true;
        case CreatorPanelHitType::CameraMenu:
            if (callbacks.onCycleCameraMenu) callbacks.onCycleCameraMenu();
            return true;
        case CreatorPanelHitType::SetupFiles:
            if (callbacks.onSetupFiles) callbacks.onSetupFiles();
            return true;
        case CreatorPanelHitType::None:
            break;
    }
    return false;
}

void RenderCreatorPanel(HDC dc,
                        const RECT& hierarchyInner,
                        const CreatorPanelLayout& layout,
                        const CreatorPanelModel& model,
                        HFONT headerFont,
                        HFONT bodyFont,
                        HFONT smallFont,
                        const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton) {
    const RECT card = CreatorCardRect(hierarchyInner);
    EditorRenderer::DrawInsetFrame(
        dc, card, EditorUiTheme::kCreatorCardFill, EditorUiTheme::kCreatorCardHi, EditorUiTheme::kCreatorCardShadow);
    EditorRenderer::FillRectColor(
        dc, RECT{card.left + 1, card.top + 1, card.right - 1, card.top + 5}, EditorUiTheme::kCreatorCardAccent);

    const int savedDc = SaveDC(dc);
    IntersectClipRect(dc, card.left, card.top, card.right, card.bottom);

    int top = card.top + 10;
    DrawSectionLabel(dc, RECT{card.left + 10, top, card.right - 10, top + 18}, "Creator Lab", headerFont);
    top += 22;
    EditorRenderer::DrawTextLine(dc,
                                 RECT{card.left + 10, top, card.right - 10, top + 34},
                                 "Templates and starter presets. Use Scene tab + viewport catalog for day-to-day editing.",
                                 EditorUiTheme::kCreatorBody,
                                 smallFont,
                                 DT_LEFT | DT_WORDBREAK);
    top += 40;

    DrawSectionLabel(dc, RECT{card.left + 10, top, card.right - 10, top + 18}, "1. New Game Template", bodyFont);
    top += 22;
    drawToolbarButton(dc,
                      layout.templateEmptyBtn,
                      "Empty",
                      model.selectedTemplate == NewGameTemplate::EmptyStudio);
    drawToolbarButton(dc,
                      layout.templateOutdoorBtn,
                      "Outdoor",
                      model.selectedTemplate == NewGameTemplate::OutdoorScene);
    drawToolbarButton(dc,
                      layout.templateInteriorBtn,
                      "Interior",
                      model.selectedTemplate == NewGameTemplate::InteriorRoom);
    top = layout.templateInteriorBtn.bottom + 10;

    DrawSectionLabel(dc, RECT{card.left + 10, top, card.right - 10, top + 18}, "2. Name and Create", bodyFont);
    top += 22;
    EditorRenderer::DrawTextLine(dc,
                                 RECT{card.left + 10, top, card.right - 10, top + 20},
                                 model.displayNameDraft + "  ->  " + model.slugPreview,
                                 EditorUiTheme::kTextGold,
                                 bodyFont,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    drawToolbarButton(dc, layout.namePrevBtn, "<", false);
    drawToolbarButton(dc, layout.nameNextBtn, ">", false);
    drawToolbarButton(dc, layout.createProjectBtn, "Create Game Project", true);
    top = layout.createProjectBtn.bottom + 12;

    DrawSectionLabel(dc, RECT{card.left + 10, top, card.right - 10, top + 18}, "3. Atmosphere", bodyFont);
    top += 22;
    drawToolbarButton(dc,
                      layout.atmosphereMenuBtn,
                      std::string("Apply: ") + AtmospherePresetLabel(model.selectedAtmosphere) + "  v",
                      true);
    top = layout.atmosphereMenuBtn.bottom + 10;

    DrawSectionLabel(dc, RECT{card.left + 10, top, card.right - 10, top + 18}, "4. Insert Objects", bodyFont);
    top += 22;
    drawToolbarButton(dc,
                      layout.insertMenuBtn,
                      std::string("Place: ") + InsertPresetLabel(model.selectedInsert) + "  v",
                      false);
    top = layout.insertMenuBtn.bottom + 10;

    DrawSectionLabel(dc, RECT{card.left + 10, top, card.right - 10, top + 18}, "5. Camera Presets", bodyFont);
    top += 22;
    drawToolbarButton(dc,
                      layout.cameraMenuBtn,
                      std::string("Set: ") + CameraPresetLabel(model.selectedCamera) + "  v",
                      false);
    top = layout.cameraMenuBtn.bottom + 12;

    drawToolbarButton(dc, layout.setupFilesBtn, "Fill Missing Project Files", false);
    top = layout.setupFilesBtn.bottom + 8;

    const std::string mountedLine = model.hasMountedGame
        ? ("Mounted: " + model.mountedGameLabel + "  |  Ctrl+2 toggles mesh catalog")
        : "No game mounted yet — create one above or open with --game=<id>.";
    EditorRenderer::DrawTextLine(dc,
                                 RECT{card.left + 10, top, card.right - 10, card.bottom - 8},
                                 mountedLine,
                                 EditorUiTheme::kCreatorBody,
                                 smallFont,
                                 DT_LEFT | DT_WORDBREAK);

    RestoreDC(dc, savedDc);
}

std::string AtmospherePresetLabel(const CreatorAtmospherePreset preset) {
    switch (preset) {
        case CreatorAtmospherePreset::ClearDay: return "Clear Day";
        case CreatorAtmospherePreset::GoldenHour: return "Golden Hour";
        case CreatorAtmospherePreset::FoggyVoid: return "Foggy Void";
        case CreatorAtmospherePreset::NightStudio: return "Night Studio";
    }
    return "Clear Day";
}

std::string InsertPresetLabel(const CreatorInsertPreset preset) {
    switch (preset) {
        case CreatorInsertPreset::GroundPlate: return "Ground";
        case CreatorInsertPreset::RockCluster: return "Rocks";
        case CreatorInsertPreset::WaterSurface: return "Water";
        case CreatorInsertPreset::SkyBackdrop: return "Sky Slab";
        case CreatorInsertPreset::PortalArch: return "Arch";
    }
    return "Ground";
}

std::string CameraPresetLabel(const CreatorCameraPreset preset) {
    switch (preset) {
        case CreatorCameraPreset::Hero: return "Hero";
        case CreatorCameraPreset::TopDown: return "Top Down";
        case CreatorCameraPreset::LowAngle: return "Low Angle";
    }
    return "Hero";
}

CreatorAtmospherePreset CycleCreatorAtmosphere(const CreatorAtmospherePreset current) {
    switch (current) {
        case CreatorAtmospherePreset::ClearDay: return CreatorAtmospherePreset::GoldenHour;
        case CreatorAtmospherePreset::GoldenHour: return CreatorAtmospherePreset::FoggyVoid;
        case CreatorAtmospherePreset::FoggyVoid: return CreatorAtmospherePreset::NightStudio;
        case CreatorAtmospherePreset::NightStudio: return CreatorAtmospherePreset::ClearDay;
    }
    return CreatorAtmospherePreset::ClearDay;
}

CreatorInsertPreset CycleCreatorInsert(const CreatorInsertPreset current) {
    switch (current) {
        case CreatorInsertPreset::GroundPlate: return CreatorInsertPreset::RockCluster;
        case CreatorInsertPreset::RockCluster: return CreatorInsertPreset::WaterSurface;
        case CreatorInsertPreset::WaterSurface: return CreatorInsertPreset::SkyBackdrop;
        case CreatorInsertPreset::SkyBackdrop: return CreatorInsertPreset::PortalArch;
        case CreatorInsertPreset::PortalArch: return CreatorInsertPreset::GroundPlate;
    }
    return CreatorInsertPreset::GroundPlate;
}

CreatorCameraPreset CycleCreatorCamera(const CreatorCameraPreset current) {
    switch (current) {
        case CreatorCameraPreset::Hero: return CreatorCameraPreset::TopDown;
        case CreatorCameraPreset::TopDown: return CreatorCameraPreset::LowAngle;
        case CreatorCameraPreset::LowAngle: return CreatorCameraPreset::Hero;
    }
    return CreatorCameraPreset::Hero;
}

bool ApplyAtmospherePreset(const fs::path& gameRoot, const CreatorAtmospherePreset preset, std::string* errorOut) {
    std::string rendering;
    std::string postprocess;
    switch (preset) {
        case CreatorAtmospherePreset::ClearDay:
            rendering =
                "# RawIron atmosphere preset: Clear Day\n"
                "clear_top_r=0.55\nclear_top_g=0.62\nclear_top_b=0.72\n"
                "clear_bottom_r=0.35\nclear_bottom_g=0.42\nclear_bottom_b=0.52\n"
                "fog_r=0.48\nfog_g=0.52\nfog_b=0.58\n"
                "fog_far_r=0.62\nfog_far_g=0.68\nfog_far_b=0.76\n"
                "fog_start=4.0\nfog_end=28.0\nfog_strength=0.55\n"
                "ambient_r=0.12\nambient_g=0.13\nambient_b=0.14\n";
            postprocess = "# RawIron postprocess preset: Clear Day\nnative_exposure=1.05\nnative_saturation=1.0\n";
            break;
        case CreatorAtmospherePreset::GoldenHour:
            rendering =
                "# RawIron atmosphere preset: Golden Hour\n"
                "clear_top_r=0.82\nclear_top_g=0.58\nclear_top_b=0.34\n"
                "clear_bottom_r=0.42\nclear_bottom_g=0.28\nclear_bottom_b=0.18\n"
                "fog_r=0.62\nfog_g=0.48\nfog_b=0.32\n"
                "fog_far_r=0.78\nfog_far_g=0.58\nfog_far_b=0.36\n"
                "fog_start=3.0\nfog_end=24.0\nfog_strength=0.62\n"
                "ambient_r=0.10\nambient_g=0.08\nambient_b=0.06\n";
            postprocess = "# RawIron postprocess preset: Golden Hour\nnative_exposure=1.12\nnative_saturation=1.08\n";
            break;
        case CreatorAtmospherePreset::FoggyVoid:
            rendering =
                "# RawIron atmosphere preset: Foggy Void\n"
                "clear_top_r=0.62\nclear_top_g=0.64\nclear_top_b=0.66\n"
                "clear_bottom_r=0.38\nclear_bottom_g=0.38\nclear_bottom_b=0.40\n"
                "fog_r=0.58\nfog_g=0.58\nfog_b=0.60\n"
                "fog_far_r=0.66\nfog_far_g=0.66\nfog_far_b=0.68\n"
                "fog_start=1.5\nfog_end=18.0\nfog_strength=0.92\n"
                "ambient_r=0.08\nambient_g=0.08\nambient_b=0.09\n";
            postprocess = "# RawIron postprocess preset: Foggy Void\nnative_exposure=0.95\nnative_saturation=0.88\n";
            break;
        case CreatorAtmospherePreset::NightStudio:
            rendering =
                "# RawIron atmosphere preset: Night Studio\n"
                "clear_top_r=0.08\nclear_top_g=0.10\nclear_top_b=0.16\n"
                "clear_bottom_r=0.02\nclear_bottom_g=0.03\nclear_bottom_b=0.06\n"
                "fog_r=0.10\nfog_g=0.11\nfog_b=0.14\n"
                "fog_far_r=0.14\nfog_far_g=0.16\nfog_far_b=0.20\n"
                "fog_start=2.0\nfog_end=32.0\nfog_strength=0.78\n"
                "ambient_r=0.04\nambient_g=0.04\nambient_b=0.05\n";
            postprocess = "# RawIron postprocess preset: Night Studio\nnative_exposure=0.82\nnative_saturation=0.92\n";
            break;
    }

    const fs::path renderingPath = gameRoot / "scripts" / "rendering.riscript";
    const fs::path postPath = gameRoot / "scripts" / "postprocess.riscript";
    std::error_code ec{};
    if (!fs::exists(gameRoot, ec)) {
        if (errorOut != nullptr) {
            *errorOut = "No mounted game root.";
        }
        return false;
    }
    const ri::content::ScriptScalarMap renderingPatches = ri::content::LoadScriptScalarsFromText(rendering);
    const ri::content::ScriptScalarMap postprocessPatches = ri::content::LoadScriptScalarsFromText(postprocess);
    if (!ri::content::PatchScriptScalarsFile(renderingPath, renderingPatches, errorOut)) {
        return false;
    }
    if (!ri::content::PatchScriptScalarsFile(postPath, postprocessPatches, errorOut)) {
        return false;
    }
    return true;
}

} // namespace ri::editor
