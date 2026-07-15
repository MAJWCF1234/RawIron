#pragma once

#include "EditorAuthoringCatalog.h"
#include "RawIron/Render/SoftwarePreview.h"
#include "RawIron/Scene/StructuralPrimitivePresets.h"
#include "RawIron/Math/Vec3.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
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

inline constexpr int kStructuralPickerCellSize = 52;
inline constexpr int kStructuralPickerLabelHeight = 11;
inline constexpr int kStructuralPickerHeaderHeight = 18;
inline constexpr int kStructuralPickerTabHeight = 18;
inline constexpr int kStructuralPickerFooterHeight = 20;
inline constexpr int kStructuralPickerVisibleRows = 2;

[[nodiscard]] inline int StructuralPickerGridHeight() {
    return (kStructuralPickerCellSize + kStructuralPickerLabelHeight + 4) * kStructuralPickerVisibleRows;
}

[[nodiscard]] inline int ComputeStructuralPickerPanelHeight() {
    return kStructuralPickerHeaderHeight + kStructuralPickerTabHeight + 4 + StructuralPickerGridHeight()
           + kStructuralPickerFooterHeight + 8;
}

inline constexpr int kStructuralPickerCollapsedBarHeight = 26;

[[nodiscard]] inline int AuthoringCatalogBottomInset(const bool expanded) {
    if (!expanded) {
        return kStructuralPickerCollapsedBarHeight + 4;
    }
    return ComputeStructuralPickerPanelHeight() + 4;
}

enum class StructuralPickerHitKind {
    None,
    Preset,
    PrevPage,
    NextPage,
    Place,
    SectionStructural,
    SectionVolumes,
    SectionLogic,
    ToggleExpand,
};

struct StructuralPickerHit {
    StructuralPickerHitKind kind = StructuralPickerHitKind::None;
    std::size_t presetIndex = 0;
};

struct StructuralPickerCell {
    std::size_t presetIndex = 0;
    RECT thumbRect{};
    RECT labelRect{};
};

struct StructuralPickerLayout {
    RECT panelRect{};
    RECT collapseToggleBtn{};
    RECT contentRect{};
    RECT meshTabBtn{};
    RECT volumeTabBtn{};
    RECT logicTabBtn{};
    RECT prevPageBtn{};
    RECT nextPageBtn{};
    RECT placeBtn{};
    std::vector<StructuralPickerCell> cells;
    int scrollTopRow = 0;
    int totalRows = 0;
    int visibleRows = 0;
    int columns = 1;
};

struct StructuralPickerModel {
    bool visible = false;
    AuthoringCatalogSection section = AuthoringCatalogSection::Structural;
    std::size_t selectedPresetIndex = 0;
    std::size_t hoveredPresetIndex = SIZE_MAX;
    int scrollTopRow = 0;
    std::string statusLine;
};

class StructuralThumbnailCache {
public:
    void SetPersistentRoot(std::filesystem::path root);
    [[nodiscard]] bool Has(AuthoringCatalogSection section, std::size_t presetIndex) const;
    [[nodiscard]] const ri::render::software::SoftwareImage& Get(AuthoringCatalogSection section, std::size_t presetIndex);
    void PrecacheAll(const std::filesystem::path& textureRoot, bool force = false);
    void Prewarm(AuthoringCatalogSection section, std::size_t presetIndex, const std::filesystem::path& textureRoot);
    void PrewarmVisible(AuthoringCatalogSection section,
                        const std::vector<std::size_t>& presetIndices,
                        const std::filesystem::path& textureRoot,
                        int budget = 6);
    void Clear();
    void ClearPersistent();

private:
    std::vector<ri::render::software::SoftwareImage> structuralImages_{};
    std::vector<bool> structuralReady_{};
    std::vector<ri::render::software::SoftwareImage> volumeImages_{};
    std::vector<bool> volumeReady_{}; 
    std::vector<ri::render::software::SoftwareImage> logicImages_{};
    std::vector<bool> logicReady_{};
    std::string textureFingerprint_{};
    std::filesystem::path persistentRoot_{};
    [[nodiscard]] std::filesystem::path PersistentRoot() const;
    void Ensure(AuthoringCatalogSection section, std::size_t presetIndex, const std::filesystem::path& textureRoot);
};

#if defined(_WIN32)
struct StructuralPickerTheme {
    HFONT headerFont = nullptr;
    HFONT bodyFont = nullptr;
    HFONT smallFont = nullptr;
};

[[nodiscard]] std::size_t ActiveCatalogPresetCount(AuthoringCatalogSection section);
[[nodiscard]] std::string ActiveCatalogPresetLabel(AuthoringCatalogSection section, std::size_t index);
[[nodiscard]] ri::math::Vec3 ActiveCatalogWireColor(AuthoringCatalogSection section, std::size_t index);
[[nodiscard]] bool ActiveCatalogUsesWireframe(AuthoringCatalogSection section, std::size_t index);

[[nodiscard]] StructuralPickerLayout ComputeStructuralPickerLayout(const RECT& viewportInner,
                                                                   AuthoringCatalogSection section,
                                                                   int scrollTopRow);
[[nodiscard]] RECT ComputeStructuralPickerCollapsedBarRect(const RECT& viewportInner);
[[nodiscard]] StructuralPickerHit HitTestStructuralPickerCollapsedBar(const RECT& barRect, const POINT& point);
void RenderStructuralPickerCollapsedBar(HDC dc,
                                        const RECT& barRect,
                                        const StructuralPickerTheme& theme,
                                        const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton);
[[nodiscard]] StructuralPickerHit HitTestStructuralPicker(const StructuralPickerLayout& layout, const POINT& point);
[[nodiscard]] int CountStructuralPickerRows(AuthoringCatalogSection section, const RECT& contentRect, int columns);
void RenderStructuralPickerOverlay(HDC dc,
                                   const StructuralPickerLayout& layout,
                                   const StructuralPickerModel& model,
                                   StructuralThumbnailCache& thumbnails,
                                   const std::filesystem::path& textureRoot,
                                   const StructuralPickerTheme& theme,
                                   const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton);
void BlitSoftwareImage(HDC dc, const RECT& target, const ri::render::software::SoftwareImage& image);
#endif

[[nodiscard]] std::size_t StructuralPresetCount();
[[nodiscard]] const ri::scene::StructuralPrimitivePreset& StructuralPresetAt(std::size_t index);
[[nodiscard]] std::string StructuralPresetDisplayLabel(std::size_t index);

/// Guide / surface / modifier presets (non-solid collision geometry) render as colored wireframes in the catalog.
[[nodiscard]] bool IsGuideStructuralPreset(std::string_view structuralType);
[[nodiscard]] ri::math::Vec3 GuideStructuralWireColor(std::string_view structuralType);

} // namespace ri::editor
