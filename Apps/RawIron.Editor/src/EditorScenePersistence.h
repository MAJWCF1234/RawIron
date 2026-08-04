#pragma once

#include "EditorLogicLayer.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/SceneStateIO.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ri::editor {

enum class EditorSceneBundleSlotKind : std::uint8_t {
    Canonical,
    Autosave,
    TimestampedSnapshot,
};

/// Stable paths that identify one independently published editor persistence slot.
/// The legacy paths remain load-only compatibility inputs; new saves are written
/// below slotRoot as immutable generation directories.
struct EditorSceneBundleSlotPaths {
    EditorSceneBundleSlotKind kind = EditorSceneBundleSlotKind::Canonical;
    std::filesystem::path slotRoot;
    std::filesystem::path legacyTransformPath;
    std::filesystem::path legacyAuthoredPath;
    std::filesystem::path legacyOrbitPath;
    std::filesystem::path legacyLogicPath;
    std::string label;
};

/// Complete paths for one immutable, generation-qualified editor scene bundle.
struct EditorSceneBundlePaths {
    std::uint64_t generation = 0U;
    std::filesystem::path generationDirectory;
    std::filesystem::path manifestPath;
    std::filesystem::path transformPath;
    std::filesystem::path authoredPath;
    std::filesystem::path orbitPath;
    std::filesystem::path logicPath;
};

[[nodiscard]] EditorSceneBundleSlotPaths ResolveCanonicalEditorSceneBundleSlot(
    const std::filesystem::path& canonicalTransformPath);
[[nodiscard]] EditorSceneBundleSlotPaths ResolveAutosaveEditorSceneBundleSlot(
    const std::filesystem::path& canonicalTransformPath);
[[nodiscard]] EditorSceneBundleSlotPaths ResolveTimestampedEditorSceneBundleSlot(
    const std::filesystem::path& canonicalTransformPath,
    std::string_view timestampToken);

enum class EditorSceneBundleSaveStage : std::uint8_t {
    TransformCommitted,
    AuthoredCommitted,
    OrbitCommitted,
    LogicCommitted,
    ComponentsValidated,
    ManifestPrepared,
};

enum class EditorSceneBundleError : std::uint8_t {
    None,
    InvalidInput,
    SlotDirectoryCreationFailed,
    GenerationReservationFailed,
    SerializationFailed,
    ComponentWriteFailed,
    ComponentValidationFailed,
    ManifestWriteFailed,
    ManifestPublishFailed,
    PublishedGenerationNotFound,
    PublishedGenerationCorrupt,
    LegacyStateNotFound,
    LegacyStateInvalid,
    InjectedFailure,
};

[[nodiscard]] const char* ToString(EditorSceneBundleError error) noexcept;

struct EditorSceneBundleSaveOptions {
    /// Test-only orchestration seam. Returning true aborts immediately after the
    /// named durable stage and before manifest publication.
    std::function<bool(EditorSceneBundleSaveStage)> injectFailureAfterStage;
};

struct EditorSceneBundleSaveInput {
    const ri::scene::Scene* scene = nullptr;
    const ri::scene::Scene* baselineScene = nullptr;
    std::size_t authoredNodeStart = 0U;
    int editorTrashHandle = ri::scene::kInvalidHandle;
    ri::scene::OrbitCameraState orbit{};
    const EditorLogicLayer* logicLayer = nullptr;
    std::filesystem::path workspaceRoot;
    std::optional<std::filesystem::path> gameRoot;
    int worldRoot = ri::scene::kInvalidHandle;
};

struct EditorSceneBundleSaveResult {
    EditorSceneBundleError error = EditorSceneBundleError::None;
    bool committed = false;
    bool durabilityWarning = false;
    std::string diagnostic;
    EditorSceneBundlePaths paths{};
    ri::scene::SceneStateIOResult transformResult{};
    std::vector<std::filesystem::path> retainedRecoveryPaths;
};

[[nodiscard]] EditorSceneBundleSaveResult SaveEditorSceneBundle(
    const EditorSceneBundleSlotPaths& slot,
    const EditorSceneBundleSaveInput& input,
    const EditorSceneBundleSaveOptions& options = {});

struct EditorSceneBundleLoadInput {
    const ri::scene::Scene* baselineScene = nullptr;
    ri::scene::OrbitCameraState baselineOrbit{};
    std::filesystem::path workspaceRoot;
    std::optional<std::filesystem::path> gameRoot;
    int worldRoot = ri::scene::kInvalidHandle;
};

struct EditorSceneBundleLoadedState {
    ri::scene::Scene scene{};
    ri::scene::OrbitCameraState orbit{};
    EditorLogicLayer logicLayer{};
};

struct EditorSceneBundleLoadResult {
    EditorSceneBundleError error = EditorSceneBundleError::None;
    bool loaded = false;
    bool usedLegacyPaths = false;
    bool recoveredPreviousGeneration = false;
    std::string diagnostic;
    EditorSceneBundlePaths paths{};
    ri::scene::SceneStateIOResult transformResult{};
};

/// Resolves, fingerprints, parses, and validates a complete bundle into local
/// candidates. `output` is assigned only after every required component succeeds.
/// When no published generation exists, fixed-name legacy files are tried.
[[nodiscard]] EditorSceneBundleLoadResult LoadEditorSceneBundle(
    const EditorSceneBundleSlotPaths& slot,
    const EditorSceneBundleLoadInput& input,
    EditorSceneBundleLoadedState& output);

/// True when the slot has a published generation or any legacy scene component.
/// This is a discovery hint only; LoadEditorSceneBundle performs authoritative validation.
[[nodiscard]] bool HasEditorSceneBundleState(const EditorSceneBundleSlotPaths& slot);

} // namespace ri::editor
