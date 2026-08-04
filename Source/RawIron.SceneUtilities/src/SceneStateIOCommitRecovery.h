#pragma once

#include <cstdint>

namespace ri::scene::detail {

// This seam is deliberately platform-neutral: the production Win32 adapter
// and the deterministic fake used by the regression test execute the same
// recovery state machine.
enum class CommitPathState : std::uint8_t {
    Missing,
    Present,
    InspectionFailed,
};

enum class ReplaceRecoveryState : std::uint8_t {
    None,
    DestinationPreserved,
    DestinationRestored,
    ManualRecoveryRequired,
};

struct CommitOperationResult {
    bool succeeded = false;
    std::uint32_t error = 0U;
};

struct ReplaceTransitionResult {
    bool committed = false;
    ReplaceRecoveryState recoveryState = ReplaceRecoveryState::None;
    std::uint32_t replaceError = 0U;
    std::uint32_t inspectionError = 0U;
    std::uint32_t recoveryError = 0U;
    std::uint32_t replacementCleanupError = 0U;
    std::uint32_t backupCleanupError = 0U;
    bool retainReplacement = false;
    bool retainBackup = false;
};

template <typename Operations, typename Path>
[[nodiscard]] ReplaceTransitionResult ReplaceExistingWithRecovery(
    Operations& operations,
    const Path& targetPath,
    const Path& replacementPath,
    const Path& backupPath) {
    ReplaceTransitionResult result{};
    const CommitOperationResult replaced = operations.Replace(
        targetPath,
        replacementPath,
        backupPath);
    if (replaced.succeeded) {
        result.committed = true;
        const CommitOperationResult backupRemoved = operations.Remove(backupPath);
        if (!backupRemoved.succeeded) {
            result.backupCleanupError = backupRemoved.error;
            result.retainBackup = true;
        }
        return result;
    }

    result.replaceError = replaced.error;
    std::uint32_t targetInspectionError = 0U;
    std::uint32_t backupInspectionError = 0U;
    const CommitPathState targetState = operations.Inspect(targetPath, targetInspectionError);
    const CommitPathState backupState = operations.Inspect(backupPath, backupInspectionError);
    if (targetState == CommitPathState::InspectionFailed
        || backupState == CommitPathState::InspectionFailed) {
        result.recoveryState = ReplaceRecoveryState::ManualRecoveryRequired;
        result.inspectionError = targetState == CommitPathState::InspectionFailed
            ? targetInspectionError
            : backupInspectionError;
        result.retainReplacement = true;
        result.retainBackup = backupState != CommitPathState::Missing;
        return result;
    }

    if (targetState == CommitPathState::Missing && backupState == CommitPathState::Present) {
        const CommitOperationResult restored = operations.RestoreBackup(backupPath, targetPath);
        if (!restored.succeeded) {
            result.recoveryState = ReplaceRecoveryState::ManualRecoveryRequired;
            result.recoveryError = restored.error;
            result.retainReplacement = true;
            result.retainBackup = true;
            return result;
        }
        result.recoveryState = ReplaceRecoveryState::DestinationRestored;
    } else if (targetState == CommitPathState::Present
               && backupState == CommitPathState::Missing) {
        // ERROR_UNABLE_TO_REMOVE_REPLACED, ERROR_UNABLE_TO_MOVE_REPLACEMENT
        // with an explicit backup, and ordinary errors all retain the original
        // names. Inspecting the actual state avoids relying on the error value.
        result.recoveryState = ReplaceRecoveryState::DestinationPreserved;
    } else {
        // Both present is ambiguous under cross-process interference; both
        // missing means the original cannot be located. Never overwrite or
        // delete either recovery artifact in either case.
        result.recoveryState = ReplaceRecoveryState::ManualRecoveryRequired;
        result.retainReplacement = true;
        result.retainBackup = backupState == CommitPathState::Present;
        return result;
    }

    const CommitOperationResult replacementRemoved = operations.Remove(replacementPath);
    if (!replacementRemoved.succeeded) {
        result.replacementCleanupError = replacementRemoved.error;
        result.retainReplacement = true;
    }
    return result;
}

} // namespace ri::scene::detail
