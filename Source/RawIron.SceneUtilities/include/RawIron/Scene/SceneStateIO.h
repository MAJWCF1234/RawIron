#pragma once

#include "RawIron/Scene/Scene.h"

#include <cstdint>
#include <filesystem>
#include <system_error>

namespace ri::scene {

inline constexpr std::uintmax_t kMaxSceneStateFileBytes = 16U * 1024U * 1024U;
inline constexpr std::size_t kMaxSceneStateNodes = 100000U;

enum class SceneStateIOError : std::uint8_t {
    None,
    InvalidPath,
    SceneNodeLimitExceeded,
    NonFiniteTransform,
    SerializedDataTooLarge,
    SerializationFailed,
    DestinationInspectionFailed,
    DestinationSymlinkUnsupported,
    DestinationTypeUnsupported,
    DirectoryCreationFailed,
    TemporaryFileCollisionLimit,
    TemporaryFileOpenFailed,
    TemporaryWriteFailed,
    DestinationMetadataFailed,
    TemporaryDataSyncFailed,
    TemporaryCloseFailed,
    AtomicReplaceFailed,
    BackupCleanupFailed,
    ParentDirectorySyncFailed,
    InputInspectionFailed,
    InputTooLarge,
    InputReadFailed,
    UnsupportedMagic,
    MalformedNodeCount,
    NodeCountLimitExceeded,
    TargetSceneTooSmall,
    MalformedNodeRecord,
    InconsistentNodeCount,
    DuplicateNodeIndex,
    NodeIndexOutOfRange,
    NodeNameMismatch,
    NumericOutOfRange,
    TrailingData,
};

enum class SceneStateIORecoveryState : std::uint8_t {
    None,
    DestinationPreserved,
    DestinationRestored,
    ManualRecoveryRequired,
};

struct SceneStateIOResult {
    SceneStateIOError error = SceneStateIOError::None;
    std::error_code systemError{};

    // For saves, committed can be true with ParentDirectorySyncFailed or
    // BackupCleanupFailed: the replacement happened, but directory durability
    // could not be confirmed or the preserved prior snapshot could not be
    // removed. For loads it means validated transforms were applied. Callers
    // that retry or report recovery state should use it.
    bool committed = false;
    bool fileDataSynchronized = false;
    bool parentDirectorySynchronized = false;
    bool parentDirectorySyncUnsupported = false;

    bool temporaryCloseFailed = false;
    std::error_code temporaryCloseError{};
    bool temporaryCleanupFailed = false;
    std::error_code temporaryCleanupError{};

    // ReplaceFileW can report a partial transition. When that happens the
    // original is either confirmed at outputPath, restored from the explicit
    // same-directory backup, or both recovery artifacts are deliberately
    // retained for manual recovery. systemError remains the replace error;
    // recoverySystemError reports a failed inspection/restore operation.
    SceneStateIORecoveryState recoveryState = SceneStateIORecoveryState::None;
    std::error_code recoverySystemError{};
    bool backupCleanupFailed = false;
    std::error_code backupCleanupError{};
    std::filesystem::path retainedBackupPath{};
    std::filesystem::path retainedReplacementPath{};

    [[nodiscard]] bool Succeeded() const noexcept {
        return error == SceneStateIOError::None;
    }

    explicit operator bool() const noexcept {
        return Succeeded();
    }
};

/// Serializes at most kMaxSceneStateFileBytes before touching the filesystem,
/// writes through an exclusively-created same-directory temporary, explicitly
/// synchronizes file data, then atomically replaces outputPath. Existing
/// symlink/reparse-point destinations and non-regular destinations are rejected.
///
/// Windows uses ReplaceFile with a unique same-directory backup for an existing
/// file so supported destination metadata is retained. Documented partial
/// ReplaceFile states are inspected: Raw Iron either confirms the original,
/// restores it from the backup, or retains exact recovery paths in the result.
/// Win32 does not expose a generally supported directory fsync, so that
/// limitation is reported. POSIX retains existing permission bits and fsyncs
/// the parent directory where supported.
/// ACLs, ownership, extended attributes, remote-filesystem behavior, controller
/// caches, and physical-media persistence remain filesystem/platform contracts;
/// the result fields state which synchronization steps this implementation
/// completed. This operation covers one transform file only, not the editor's
/// authored/orbit/logic sidecars as a multi-file transaction.
[[nodiscard]] SceneStateIOResult SaveSceneNodeTransformsDetailed(
    const Scene& scene,
    const std::filesystem::path& outputPath);

/// Parses and validates the complete bounded V1 document before changing scene.
/// Zero-node documents are valid no-ops. A document may target a contiguous
/// prefix of a newer scene, preserving the existing compatibility behavior.
[[nodiscard]] SceneStateIOResult LoadSceneNodeTransformsDetailed(
    Scene& scene,
    const std::filesystem::path& inputPath);

/// Source-compatible convenience wrapper for SaveSceneNodeTransformsDetailed.
[[nodiscard]] bool SaveSceneNodeTransforms(const Scene& scene, const std::filesystem::path& outputPath);

/// Source-compatible convenience wrapper for LoadSceneNodeTransformsDetailed.
[[nodiscard]] bool LoadSceneNodeTransforms(Scene& scene, const std::filesystem::path& inputPath);

} // namespace ri::scene
