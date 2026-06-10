#pragma once

#include "RawIron/Math/Vec3.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace ri::editor {

class EditorLogicLayer;

enum class AuthoringCatalogSection {
    Structural = 0,
    Volumes = 1,
    Logic = 2,
};

enum class AuthoringCatalogSpawnKind {
    StructuralBrush,
    TriggerVolume,
    VolumeMarker,
    LogicPort,
};

struct AuthoringCatalogPreset {
    const char* label = "item";
    const char* typeId = "box";
    AuthoringCatalogSection section = AuthoringCatalogSection::Structural;
    AuthoringCatalogSpawnKind spawnKind = AuthoringCatalogSpawnKind::StructuralBrush;
};

[[nodiscard]] std::size_t AuthoringCatalogPresetCount(AuthoringCatalogSection section);
[[nodiscard]] const AuthoringCatalogPreset& AuthoringCatalogPresetAt(AuthoringCatalogSection section, std::size_t index);
[[nodiscard]] std::string AuthoringCatalogPresetLabel(AuthoringCatalogSection section, std::size_t index);
[[nodiscard]] std::string_view AuthoringCatalogSectionLabel(AuthoringCatalogSection section);
[[nodiscard]] ri::math::Vec3 AuthoringCatalogWireColor(AuthoringCatalogSection section, std::string_view typeId);
[[nodiscard]] bool AuthoringCatalogUsesWireframe(AuthoringCatalogSection section, std::string_view typeId);

/// When set, the Logic catalog tab lists all LogicKit manifest nodes instead of hardcoded stubs.
void BindAuthoringLogicCatalog(const EditorLogicLayer* layer);

} // namespace ri::editor
