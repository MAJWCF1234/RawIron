#pragma once

#include "RawIron/Games/GameConfigContracts.h"
#include "RawIron/Games/GameRuntimeCore.h"

#include <optional>

namespace ri::gamehost {

struct GameProjectBootOptions {
    std::filesystem::path workspaceRoot;
    std::filesystem::path gameRoot;
    std::string gameId;
    /// This executable must refuse projects belonging to another game module.
    std::string expectedRuntimeModule;
    ri::games::GameConfigContractOptions configContract{};
    /// Optional save/checkpoint location owned by the launching host.
    std::filesystem::path checkpointStorageRoot;
};

struct PreparedGameProject {
    ri::games::GameRuntimeBootServices services;
    ri::runtime::RuntimePaths paths;
    [[nodiscard]] const ri::content::GameManifest& Manifest() const { return *services.manifest; }
    [[nodiscard]] const ri::content::GameScriptBundle& Scripts() const { return *services.scripts; }
    [[nodiscard]] ri::runtime::RuntimeCore CreateRuntime() const;
};

/// Resolve and validate before world construction, window creation or networking.
/// Engine host code does not know about gallery rooms or any particular game.
[[nodiscard]] std::optional<PreparedGameProject> PrepareGameProject(
    const GameProjectBootOptions& options, std::string* error = nullptr);

} // namespace ri::gamehost
