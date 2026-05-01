#pragma once

#include "RawIron/Logic/LogicPortSchema.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::logic {

/// Repo-relative path to the packaged LogicKit manifest (`kitVersion`, `nodes[]`, screen tables).
inline constexpr std::string_view kLogicKitNodesJsonRelative = "Assets/Packages/LogicKit/logic_kit_nodes.json";

struct LogicKitScreenTextureProfile {
    std::string key;
    std::int32_t px = 256;
    /// JSON `null` or omitted → unset.
    std::optional<std::string> subdir;
};

struct LogicKitPortBinding {
    std::string name;
    std::string scenePathHint;
    std::string hitMeshNameHint;
    std::string portKind;
    std::int32_t portIndex = -1;
};

struct LogicKitPortSchemaSpec {
    std::vector<LogicKitPortBinding> inputs;
    std::vector<LogicKitPortBinding> outputs;
};

struct LogicKitEmbeddedScreen {
    std::string profile;
    std::string state;
    std::int32_t px = 256;
};

/// One kit node row — matches `logic_kit_nodes.json` `nodes[]` entries (kitVersion 4).
struct LogicKitNodeManifestEntry {
    std::string id;
    /// Relative to the LogicKit directory (parent of `logic_kit_nodes.json`).
    std::string glbRelative;
    std::string colorHex;
    /// Top-level `inputs` / `outputs` string lists from JSON.
    std::vector<std::string> summaryInputs;
    std::vector<std::string> summaryOutputs;
    std::unordered_map<std::string, std::string> screenStates;
    /// Profile key (e.g. r256) → screen state → texture path relative to LogicKit root.
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> screenTexturesByProfile;
    std::unordered_map<std::string, std::string> screenTexturesR128;
    std::unordered_map<std::string, std::string> screenTexturesR512;
    std::unordered_map<std::string, std::string> remapPreviewIdle;
    std::optional<LogicKitEmbeddedScreen> embeddedScreen;
    std::string defaultScreenState;
    std::optional<std::int32_t> screenMaterialIndex;
    LogicKitPortSchemaSpec portSchema;
};

struct LogicKitManifest {
    std::int32_t kitVersion = 0;
    std::string screenPixelRemapMode;
    std::vector<std::string> screenStateKeys;
    std::vector<LogicKitScreenTextureProfile> screenTextureProfiles;
    std::vector<std::string> screenRemapPreviewModes;
    std::vector<LogicKitNodeManifestEntry> nodes;
};

[[nodiscard]] std::filesystem::path LogicKitRootDirectory(const std::filesystem::path& manifestPath);

[[nodiscard]] std::filesystem::path ResolveLogicKitGlbPath(const std::filesystem::path& manifestPath,
                                                           std::string_view glbRelative);

[[nodiscard]] std::filesystem::path ResolveLogicKitAssetPath(const std::filesystem::path& manifestPath,
                                                             std::string_view relativePath);

/// When non-null, `GetLogicNodePortSchema(kind)` resolves `kind` against kit `id` first, and
/// `BuildDefaultLogicVisualLibrary()` also registers styles for every kit node.
void SetActiveLogicKitManifest(const LogicKitManifest* manifest);

[[nodiscard]] const LogicKitManifest* ActiveLogicKitManifest();

[[nodiscard]] LogicNodePortSchema BuildLogicNodePortSchemaFromKitEntry(const LogicKitNodeManifestEntry& entry);

[[nodiscard]] std::optional<LogicKitManifest> LoadLogicKitManifest(const std::filesystem::path& manifestPath);

/// Loads the manifest and returns only `nodes` (convenience).
[[nodiscard]] std::optional<std::vector<LogicKitNodeManifestEntry>> LoadLogicKitNodeManifestEntries(
    const std::filesystem::path& manifestPath);

[[nodiscard]] const LogicKitNodeManifestEntry* FindLogicKitNodeManifestEntry(
    const std::vector<LogicKitNodeManifestEntry>& entries,
    std::string_view nodeId);

[[nodiscard]] const LogicKitNodeManifestEntry* FindLogicKitNodeManifestEntry(const LogicKitManifest& manifest,
                                                                             std::string_view nodeId);

} // namespace ri::logic
