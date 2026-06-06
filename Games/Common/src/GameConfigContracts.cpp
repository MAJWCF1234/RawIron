#include "RawIron/Games/GameConfigContracts.h"

#include "RawIron/Content/ScriptScalars.h"
#include "RawIron/Core/Log.h"

#include <sstream>
#include <string_view>
#include <vector>

namespace ri::games {
namespace {

using ri::content::ScriptScalarSchema;
using ri::content::ScriptScalarRule;

ScriptScalarSchema MakeGameCfgSchema(bool strictUnknownKeys) {
    return ScriptScalarSchema{
        .name = "config/game.cfg",
        .allowUnknownKeys = !strictUnknownKeys,
        .rules = {
            {.key = "runtime_profile", .required = true, .minValue = 0.0f, .maxValue = 8.0f},
            {.key = "editor_profile", .required = true, .minValue = 0.0f, .maxValue = 8.0f},
        },
    };
}

ScriptScalarSchema MakeRenderingSchema(bool strictUnknownKeys) {
    return ScriptScalarSchema{
        .name = "scripts/rendering.riscript",
        .allowUnknownKeys = !strictUnknownKeys,
        .rules = {
            {.key = "clear_top_r", .required = true, .minValue = 0.0f, .maxValue = 1.0f},
            {.key = "clear_top_g", .required = true, .minValue = 0.0f, .maxValue = 1.0f},
            {.key = "clear_top_b", .required = true, .minValue = 0.0f, .maxValue = 1.0f},
            {.key = "clear_bottom_r", .required = true, .minValue = 0.0f, .maxValue = 1.0f},
            {.key = "clear_bottom_g", .required = true, .minValue = 0.0f, .maxValue = 1.0f},
            {.key = "clear_bottom_b", .required = true, .minValue = 0.0f, .maxValue = 1.0f},
            {.key = "fog_r", .required = true, .minValue = 0.0f, .maxValue = 1.0f},
            {.key = "fog_g", .required = true, .minValue = 0.0f, .maxValue = 1.0f},
            {.key = "fog_b", .required = true, .minValue = 0.0f, .maxValue = 1.0f},
            {.key = "ambient_r", .required = true, .minValue = 0.0f, .maxValue = 1.0f},
            {.key = "ambient_g", .required = true, .minValue = 0.0f, .maxValue = 1.0f},
            {.key = "ambient_b", .required = true, .minValue = 0.0f, .maxValue = 1.0f},
        },
    };
}

ScriptScalarSchema MakePostProcessSchema(bool strictUnknownKeys) {
    return ScriptScalarSchema{
        .name = "scripts/postprocess.riscript",
        .allowUnknownKeys = !strictUnknownKeys,
        .rules = {
            {.key = "postprocess_quality", .required = true, .minValue = 0.0f, .maxValue = 3.0f},
            {.key = "postprocess_tint_strength", .required = true, .minValue = 0.0f, .maxValue = 1.0f},
            {.key = "native_exposure", .required = true, .minValue = 0.5f, .maxValue = 2.5f},
            {.key = "native_contrast", .required = true, .minValue = 0.7f, .maxValue = 1.6f},
            {.key = "native_saturation", .required = true, .minValue = 0.0f, .maxValue = 1.8f},
            {.key = "native_fog_density", .required = true, .minValue = 0.0f, .maxValue = 0.05f},
            {.key = "fov_base", .required = true, .minValue = 45.0f, .maxValue = 120.0f},
            {.key = "fov_sprint_add", .required = true, .minValue = 0.0f, .maxValue = 25.0f},
            {.key = "fov_lerp_per_second", .required = true, .minValue = 0.5f, .maxValue = 40.0f},
        },
    };
}

ScriptScalarSchema MakeAudioSchema(bool strictUnknownKeys) {
    return ScriptScalarSchema{
        .name = "scripts/audio.riscript",
        .allowUnknownKeys = !strictUnknownKeys,
        .rules = {
            {.key = "audio_master_gain", .required = true, .minValue = 0.0f, .maxValue = 4.0f},
            {.key = "audio_environment_blend", .required = true, .minValue = 0.0f, .maxValue = 2.0f},
        },
    };
}

ScriptScalarSchema MakePhysicsSchema(bool strictUnknownKeys) {
    return ScriptScalarSchema{
        .name = "scripts/physics.riscript",
        .allowUnknownKeys = !strictUnknownKeys,
        .rules = {
            {.key = "global_gravity_scale", .required = true, .minValue = 0.1f, .maxValue = 4.0f},
            {.key = "global_drag_scale", .required = true, .minValue = 0.1f, .maxValue = 4.0f},
            {.key = "global_jump_scale", .required = true, .minValue = 0.65f, .maxValue = 1.35f},
            {.key = "global_air_control_scale", .required = true, .minValue = 0.75f, .maxValue = 1.35f},
            {.key = "movement_gravity", .required = true, .minValue = 1.0f, .maxValue = 64.0f},
            {.key = "movement_fall_gravity_multiplier", .required = true, .minValue = 0.5f, .maxValue = 4.0f},
        },
    };
}

ScriptScalarSchema MakeUiSchema(bool strictUnknownKeys) {
    return ScriptScalarSchema{
        .name = "scripts/ui.riscript",
        .allowUnknownKeys = !strictUnknownKeys,
        .rules = {
            {.key = "show_runtime_diagnostics", .required = true, .minValue = 0.0f, .maxValue = 1.0f},
            {.key = "show_objective_panel", .required = true, .minValue = 0.0f, .maxValue = 1.0f},
            {.key = "crosshair_scale", .required = true, .minValue = 0.1f, .maxValue = 4.0f},
            {.key = "runtime_ui_boot_flow", .required = false, .minValue = 0.0f, .maxValue = 2.0f},
            {.key = "runtime_ui_hotkeys_enabled", .required = false, .minValue = 0.0f, .maxValue = 1.0f},
        },
    };
}

ScriptScalarSchema MakeStreamingSchema(bool strictUnknownKeys) {
    return ScriptScalarSchema{
        .name = "scripts/streaming.riscript",
        .allowUnknownKeys = !strictUnknownKeys,
        .rules = {
            {.key = "streaming_budget_scale", .required = true, .minValue = 0.1f, .maxValue = 8.0f},
            {.key = "checkpoint_autosave_enabled", .required = true, .minValue = 0.0f, .maxValue = 1.0f},
        },
    };
}

ScriptScalarSchema MakeNetworkSchema(bool strictUnknownKeys) {
    return ScriptScalarSchema{
        .name = "scripts/network.riscript",
        .allowUnknownKeys = !strictUnknownKeys,
        .rules = {
            {.key = "network_tick_hz", .required = true, .minValue = 1.0f, .maxValue = 240.0f},
            {.key = "network_snapshot_rate", .required = true, .minValue = 1.0f, .maxValue = 240.0f},
            {.key = "network_timeout_seconds", .required = true, .minValue = 1.0f, .maxValue = 120.0f},
        },
    };
}

ScriptScalarSchema MakePersistenceSchema(bool strictUnknownKeys) {
    return ScriptScalarSchema{
        .name = "scripts/persistence.riscript",
        .allowUnknownKeys = !strictUnknownKeys,
        .rules = {
            {.key = "autosave_interval_seconds", .required = true, .minValue = 1.0f, .maxValue = 3600.0f},
            {.key = "checkpoint_ring_size", .required = true, .minValue = 1.0f, .maxValue = 512.0f},
            {.key = "persistence_flush_on_checkpoint", .required = true, .minValue = 0.0f, .maxValue = 1.0f},
            {.key = "save_slot_count", .required = true, .minValue = 1.0f, .maxValue = 64.0f},
        },
    };
}

} // namespace

bool EnforceGameConfigContracts(const std::filesystem::path& gameRoot,
                                const GameConfigContractOptions& options,
                                std::string* error) {
    std::vector<std::string> errors{};
    std::vector<std::string> warnings{};
    const bool strictUnknown = options.mode == GameConfigContractMode::Strict;
    const bool failOnValidationIssues = options.mode == GameConfigContractMode::Strict;
    const bool failOnMissingCoreFiles = options.mode != GameConfigContractMode::Permissive;

    const auto validatePath = [&](const std::filesystem::path& path, const ScriptScalarSchema& schema) {
        if (!std::filesystem::exists(path)) {
            const std::string missing = path.string() + ": missing file";
            if (failOnMissingCoreFiles) {
                errors.push_back(missing);
            } else {
                warnings.push_back(missing);
            }
            return;
        }
        const ri::content::ScriptScalarMap values = ri::content::LoadScriptScalars(path);
        const ri::content::ScriptScalarValidationReport report = ri::content::ValidateScriptScalars(values, schema);
        for (const ri::content::ScriptScalarValidationIssue& issue : report.issues) {
            const std::string item = path.string() + ": " + issue.key + " (" + issue.message + ")";
            if (failOnValidationIssues && issue.severity == ri::content::ScriptScalarValidationSeverity::Error) {
                errors.push_back(item);
            } else {
                warnings.push_back(item);
            }
        }
    };
    validatePath(gameRoot / "config" / "game.cfg", MakeGameCfgSchema(strictUnknown));
    validatePath(gameRoot / "scripts" / "rendering.riscript", MakeRenderingSchema(strictUnknown));
    validatePath(gameRoot / "scripts" / "postprocess.riscript", MakePostProcessSchema(strictUnknown));
    validatePath(gameRoot / "scripts" / "audio.riscript", MakeAudioSchema(strictUnknown));
    validatePath(gameRoot / "scripts" / "physics.riscript", MakePhysicsSchema(strictUnknown));
    validatePath(gameRoot / "scripts" / "ui.riscript", MakeUiSchema(strictUnknown));
    validatePath(gameRoot / "scripts" / "streaming.riscript", MakeStreamingSchema(strictUnknown));
    validatePath(gameRoot / "scripts" / "network.riscript", MakeNetworkSchema(strictUnknown));
    validatePath(gameRoot / "scripts" / "persistence.riscript", MakePersistenceSchema(strictUnknown));

    for (const std::string& item : warnings) {
        ri::core::LogInfo("Config contract warning: " + item);
    }

    if (errors.empty()) {
        ri::core::LogInfo("Config contract validation: passed");
        return true;
    }

    std::ostringstream stream;
    stream << "Config contract validation failed (" << errors.size() << " errors)";
    for (const std::string& item : errors) {
        stream << "\n - " << item;
    }
    const std::string message = stream.str();
    ri::core::LogInfo(message);
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

} // namespace ri::games
