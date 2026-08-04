#pragma once

#include "RawIron/Content/ScriptScalars.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ri::content {

/// Standard per-game script/config scalar maps. Engine owns load + missing logs; games only read.
struct GameScriptBundle {
    ScriptScalarMap gameplay{};
    ScriptScalarMap rendering{};
    ScriptScalarMap ui{};
    ScriptScalarMap audio{};
    ScriptScalarMap streaming{};
    ScriptScalarMap localization{};
    ScriptScalarMap physics{};
    ScriptScalarMap postprocess{};
    ScriptScalarMap init{};
    ScriptScalarMap network{};
    ScriptScalarMap persistence{};
    ScriptScalarMap ai{};
    ScriptScalarMap plugins{};
    ScriptScalarMap animation{};
    ScriptScalarMap vfx{};
    ScriptScalarMap gameCfg{};
    ScriptScalarMap networkCfg{};
    ScriptScalarMap buildProfile{};
    ScriptScalarMap securityPolicy{};
    ScriptScalarMap pluginsPolicy{};
};

struct GameScriptBundleLoadOptions {
    /// When true, log once per missing/empty surface (Balanced-friendly boot chatter).
    bool logMissing = true;
};

[[nodiscard]] GameScriptBundle LoadGameScriptBundle(const std::filesystem::path& gameRoot,
                                                    const GameScriptBundleLoadOptions& options = {});

} // namespace ri::content
