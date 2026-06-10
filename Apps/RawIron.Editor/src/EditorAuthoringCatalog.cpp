#include "EditorAuthoringCatalog.h"
#include "EditorLogicLayer.h"

#include "RawIron/Logic/LogicKitNodeFactory.h"

#include <array>

namespace ri::editor {
namespace {

const EditorLogicLayer* g_logicCatalogLayer = nullptr;
AuthoringCatalogPreset g_dynamicLogicPreset{};
std::string g_dynamicLogicLabel;
std::string g_dynamicLogicTypeId;

[[nodiscard]] bool MatchesType(std::string_view type, std::string_view candidate) {
    return type == candidate;
}

template <std::size_t N>
[[nodiscard]] std::size_t CountSectionPresets(const std::array<AuthoringCatalogPreset, N>& presets,
                                                const AuthoringCatalogSection section) {
    std::size_t count = 0;
    for (const AuthoringCatalogPreset& preset : presets) {
        if (preset.section == section) {
            ++count;
        }
    }
    return count;
}

template <std::size_t N>
[[nodiscard]] const AuthoringCatalogPreset& PresetAtSectionIndex(const std::array<AuthoringCatalogPreset, N>& presets,
                                                                 const AuthoringCatalogSection section,
                                                                 const std::size_t index) {
    std::size_t seen = 0;
    for (const AuthoringCatalogPreset& preset : presets) {
        if (preset.section != section) {
            continue;
        }
        if (seen == index) {
            return preset;
        }
        ++seen;
    }
    return presets.front();
}

constexpr std::array<AuthoringCatalogPreset, 28> kVolumePresets{{
    {.label = "trigger", .typeId = "generic_trigger_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::TriggerVolume},
    {.label = "kill_zone", .typeId = "kill_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "damage", .typeId = "damage_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "teleport", .typeId = "teleport_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "checkpoint", .typeId = "checkpoint_spawn_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "post_process", .typeId = "post_process_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "fog", .typeId = "localized_fog_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "reflection_probe", .typeId = "reflection_probe_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "audio_reverb", .typeId = "audio_reverb_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "audio_occlusion", .typeId = "audio_occlusion_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "physics_zone", .typeId = "physics_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "gravity", .typeId = "custom_gravity_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "nav_bounds", .typeId = "navmesh_bounds_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "nav_exclude", .typeId = "navmesh_exclusion_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "clip", .typeId = "clipping_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "camera_block", .typeId = "camera_blocking_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "safe_zone", .typeId = "safe_zone_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "portal", .typeId = "portal", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "occlusion", .typeId = "occlusion_portal", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "ladder", .typeId = "ladder_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "launch", .typeId = "launch_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "streaming", .typeId = "streaming_level_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "particle_spawn", .typeId = "particle_spawn_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "ambient_audio", .typeId = "ambient_audio_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "filtered_collision", .typeId = "filtered_collision_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "spatial_query", .typeId = "spatial_query_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "cull_distance", .typeId = "culling_distance_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
    {.label = "analytics", .typeId = "analytics_heatmap_volume", .section = AuthoringCatalogSection::Volumes, .spawnKind = AuthoringCatalogSpawnKind::VolumeMarker},
}};

constexpr std::array<AuthoringCatalogPreset, 12> kLogicPresets{{
    {.label = "trigger_port", .typeId = "IoTrigger", .section = AuthoringCatalogSection::Logic, .spawnKind = AuthoringCatalogSpawnKind::LogicPort},
    {.label = "relay", .typeId = "Relay", .section = AuthoringCatalogSection::Logic, .spawnKind = AuthoringCatalogSpawnKind::LogicPort},
    {.label = "timer", .typeId = "Timer", .section = AuthoringCatalogSection::Logic, .spawnKind = AuthoringCatalogSpawnKind::LogicPort},
    {.label = "counter", .typeId = "Counter", .section = AuthoringCatalogSection::Logic, .spawnKind = AuthoringCatalogSpawnKind::LogicPort},
    {.label = "sequencer", .typeId = "Sequencer", .section = AuthoringCatalogSection::Logic, .spawnKind = AuthoringCatalogSpawnKind::LogicPort},
    {.label = "gate_and", .typeId = "GateAnd", .section = AuthoringCatalogSection::Logic, .spawnKind = AuthoringCatalogSpawnKind::LogicPort},
    {.label = "gate_or", .typeId = "GateOr", .section = AuthoringCatalogSection::Logic, .spawnKind = AuthoringCatalogSpawnKind::LogicPort},
    {.label = "gate_not", .typeId = "GateNot", .section = AuthoringCatalogSection::Logic, .spawnKind = AuthoringCatalogSpawnKind::LogicPort},
    {.label = "latch", .typeId = "Latch", .section = AuthoringCatalogSection::Logic, .spawnKind = AuthoringCatalogSpawnKind::LogicPort},
    {.label = "pulse", .typeId = "Pulse", .section = AuthoringCatalogSection::Logic, .spawnKind = AuthoringCatalogSpawnKind::LogicPort},
    {.label = "compare", .typeId = "Compare", .section = AuthoringCatalogSection::Logic, .spawnKind = AuthoringCatalogSpawnKind::LogicPort},
    {.label = "button_io", .typeId = "IoButton", .section = AuthoringCatalogSection::Logic, .spawnKind = AuthoringCatalogSpawnKind::LogicPort},
}};

[[nodiscard]] ri::math::Vec3 VolumeWireColor(const std::string_view typeId) {
    if (MatchesType(typeId, "generic_trigger_volume") || MatchesType(typeId, "kill_volume")
        || MatchesType(typeId, "damage_volume") || MatchesType(typeId, "teleport_volume")
        || MatchesType(typeId, "checkpoint_spawn_volume") || MatchesType(typeId, "launch_volume")
        || MatchesType(typeId, "ladder_volume")) {
        return ri::math::Vec3{0.18f, 0.88f, 0.38f};
    }
    if (MatchesType(typeId, "post_process_volume") || MatchesType(typeId, "localized_fog_volume")
        || MatchesType(typeId, "reflection_probe_volume") || MatchesType(typeId, "light_importance_volume")
        || MatchesType(typeId, "volumetric_fog_blocker") || MatchesType(typeId, "lod_override_volume")) {
        return ri::math::Vec3{0.72f, 0.48f, 0.98f};
    }
    if (MatchesType(typeId, "audio_reverb_volume") || MatchesType(typeId, "audio_occlusion_volume")
        || MatchesType(typeId, "ambient_audio_volume")) {
        return ri::math::Vec3{0.98f, 0.62f, 0.24f};
    }
    if (MatchesType(typeId, "physics_volume") || MatchesType(typeId, "custom_gravity_volume")
        || MatchesType(typeId, "navmesh_bounds_volume") || MatchesType(typeId, "navmesh_exclusion_volume")
        || MatchesType(typeId, "filtered_collision_volume") || MatchesType(typeId, "radial_force_volume")) {
        return ri::math::Vec3{0.95f, 0.34f, 0.34f};
    }
    if (MatchesType(typeId, "portal") || MatchesType(typeId, "occlusion_portal")
        || MatchesType(typeId, "clipping_volume") || MatchesType(typeId, "camera_blocking_volume")
        || MatchesType(typeId, "streaming_level_volume") || MatchesType(typeId, "spatial_query_volume")) {
        return ri::math::Vec3{0.42f, 0.68f, 0.92f};
    }
    return ri::math::Vec3{0.58f, 0.78f, 0.88f};
}

[[nodiscard]] ri::math::Vec3 ParseKitHexColor(std::string_view hex, const ri::math::Vec3& fallback) {
    if (hex.empty() || hex[0] != '#') {
        return fallback;
    }
    const std::string digits(hex.substr(1));
    if (digits.size() != 6U) {
        return fallback;
    }
    auto nybble = [](const char ch) -> int {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f') {
            return 10 + (ch - 'a');
        }
        if (ch >= 'A' && ch <= 'F') {
            return 10 + (ch - 'A');
        }
        return -1;
    };
    const int r = nybble(digits[0]) * 16 + nybble(digits[1]);
    const int g = nybble(digits[2]) * 16 + nybble(digits[3]);
    const int b = nybble(digits[4]) * 16 + nybble(digits[5]);
    if (r < 0 || g < 0 || b < 0) {
        return fallback;
    }
    return ri::math::Vec3{static_cast<float>(r) / 255.0f,
                          static_cast<float>(g) / 255.0f,
                          static_cast<float>(b) / 255.0f};
}

[[nodiscard]] ri::math::Vec3 LogicWireColor(const std::string_view typeId) {
    if (MatchesType(typeId, "IoTrigger") || MatchesType(typeId, "IoButton")) {
        return ri::math::Vec3{0.98f, 0.86f, 0.28f};
    }
    if (MatchesType(typeId, "GateAnd") || MatchesType(typeId, "GateOr") || MatchesType(typeId, "GateNot")) {
        return ri::math::Vec3{0.42f, 0.68f, 1.0f};
    }
    if (MatchesType(typeId, "Timer") || MatchesType(typeId, "Counter") || MatchesType(typeId, "Latch")) {
        return ri::math::Vec3{0.92f, 0.48f, 0.88f};
    }
    return ri::math::Vec3{0.35f, 0.92f, 0.88f};
}

} // namespace

void BindAuthoringLogicCatalog(const EditorLogicLayer* layer) {
    g_logicCatalogLayer = layer;
}

std::size_t AuthoringCatalogPresetCount(const AuthoringCatalogSection section) {
    switch (section) {
        case AuthoringCatalogSection::Volumes:
            return kVolumePresets.size();
        case AuthoringCatalogSection::Logic:
            if (g_logicCatalogLayer != nullptr && g_logicCatalogLayer->LogicCatalogCount() > 0U) {
                return g_logicCatalogLayer->LogicCatalogCount();
            }
            return kLogicPresets.size();
        case AuthoringCatalogSection::Structural:
            break;
    }
    return 0;
}

const AuthoringCatalogPreset& AuthoringCatalogPresetAt(const AuthoringCatalogSection section, const std::size_t index) {
    switch (section) {
        case AuthoringCatalogSection::Volumes:
            return PresetAtSectionIndex(kVolumePresets, section, index);
        case AuthoringCatalogSection::Logic:
            if (g_logicCatalogLayer != nullptr && index < g_logicCatalogLayer->LogicCatalogCount()) {
                const ri::logic::LogicKitNodeManifestEntry& entry = g_logicCatalogLayer->LogicCatalogEntry(index);
                g_dynamicLogicLabel = entry.id;
                if (!ri::logic::LogicKitIdIsExecutable(entry.id)) {
                    g_dynamicLogicLabel += " (visual)";
                }
                g_dynamicLogicTypeId = entry.id;
                g_dynamicLogicPreset = AuthoringCatalogPreset{
                    .label = g_dynamicLogicLabel.c_str(),
                    .typeId = g_dynamicLogicTypeId.c_str(),
                    .section = AuthoringCatalogSection::Logic,
                    .spawnKind = AuthoringCatalogSpawnKind::LogicPort,
                };
                return g_dynamicLogicPreset;
            }
            return PresetAtSectionIndex(kLogicPresets, section, index);
        case AuthoringCatalogSection::Structural:
            break;
    }
    static const AuthoringCatalogPreset kFallback{};
    return kFallback;
}

std::string AuthoringCatalogPresetLabel(const AuthoringCatalogSection section, const std::size_t index) {
    const AuthoringCatalogPreset& preset = AuthoringCatalogPresetAt(section, index);
    return std::string(preset.label);
}

std::string_view AuthoringCatalogSectionLabel(const AuthoringCatalogSection section) {
    switch (section) {
        case AuthoringCatalogSection::Structural:
            return "Meshes";
        case AuthoringCatalogSection::Volumes:
            return "Volumes";
        case AuthoringCatalogSection::Logic:
            return "Logic";
    }
    return "Meshes";
}

ri::math::Vec3 AuthoringCatalogWireColor(const AuthoringCatalogSection section, const std::string_view typeId) {
    switch (section) {
        case AuthoringCatalogSection::Volumes:
            return VolumeWireColor(typeId);
        case AuthoringCatalogSection::Logic:
            if (g_logicCatalogLayer != nullptr) {
                for (std::size_t i = 0; i < g_logicCatalogLayer->LogicCatalogCount(); ++i) {
                    const ri::logic::LogicKitNodeManifestEntry& entry = g_logicCatalogLayer->LogicCatalogEntry(i);
                    if (entry.id == typeId) {
                        return ParseKitHexColor(entry.colorHex, LogicWireColor(typeId));
                    }
                }
            }
            return LogicWireColor(typeId);
        case AuthoringCatalogSection::Structural:
            break;
    }
    return ri::math::Vec3{0.58f, 0.78f, 0.88f};
}

bool AuthoringCatalogUsesWireframe(const AuthoringCatalogSection section, const std::string_view typeId) {
    switch (section) {
        case AuthoringCatalogSection::Volumes:
        case AuthoringCatalogSection::Logic:
            (void)typeId;
            return true;
        case AuthoringCatalogSection::Structural:
            break;
    }
    return false;
}

} // namespace ri::editor
