#include "SceneStateIOCommitRecovery.h"

#include <cstdlib>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace {

using ri::scene::detail::CommitOperationResult;
using ri::scene::detail::CommitPathState;
using ri::scene::detail::ReplaceRecoveryState;
using ri::scene::detail::ReplaceTransitionResult;

constexpr std::uint32_t kUnableToRemoveReplaced = 1175U;
constexpr std::uint32_t kUnableToMoveReplacement = 1176U;
constexpr std::uint32_t kUnableToMoveReplacement2 = 1177U;
constexpr std::uint32_t kInjectedFailure = 12345U;

struct FakeFilesystem {
    std::unordered_map<std::string, std::string> files;
    std::uint32_t replaceError = 0U;
    std::string inspectFailurePath;
    std::string removeFailurePath;
    bool failRestore = false;
    bool targetAppearsDuringRestore = false;

    [[nodiscard]] CommitOperationResult Replace(
        const std::string& target,
        const std::string& replacement,
        const std::string& backup) {
        if (replaceError == 0U) {
            files[backup] = files.at(target);
            files[target] = files.at(replacement);
            files.erase(replacement);
            return {true, 0U};
        }
        if (replaceError == kUnableToMoveReplacement2) {
            files[backup] = files.at(target);
            files.erase(target);
            // Per ReplaceFileW's contract, replacement retains its name.
        }
        // 1175, 1176 with an explicit backup, and ordinary failures retain
        // the original names and need no fake mutation.
        return {false, replaceError};
    }

    [[nodiscard]] CommitPathState Inspect(const std::string& path, std::uint32_t& error) const {
        if (path == inspectFailurePath) {
            error = kInjectedFailure;
            return CommitPathState::InspectionFailed;
        }
        return files.contains(path) ? CommitPathState::Present : CommitPathState::Missing;
    }

    [[nodiscard]] CommitOperationResult RestoreBackup(
        const std::string& backup,
        const std::string& target) {
        if (targetAppearsDuringRestore) {
            files[target] = "concurrent-writer";
            return {false, kInjectedFailure};
        }
        if (failRestore) {
            return {false, kInjectedFailure};
        }
        if (!files.contains(backup) || files.contains(target)) {
            return {false, kInjectedFailure};
        }
        files[target] = files.at(backup);
        files.erase(backup);
        return {true, 0U};
    }

    [[nodiscard]] CommitOperationResult Remove(const std::string& path) {
        if (path == removeFailurePath) {
            return {false, kInjectedFailure};
        }
        files.erase(path);
        return {true, 0U};
    }
};

[[nodiscard]] FakeFilesystem InitialFilesystem(const std::uint32_t replaceError) {
    FakeFilesystem filesystem{};
    filesystem.files.emplace("target", "old");
    filesystem.files.emplace("replacement", "new");
    filesystem.replaceError = replaceError;
    return filesystem;
}

[[nodiscard]] ReplaceTransitionResult Execute(FakeFilesystem& filesystem) {
    return ri::scene::detail::ReplaceExistingWithRecovery(
        filesystem,
        std::string("target"),
        std::string("replacement"),
        std::string("backup"));
}

[[nodiscard]] bool OriginalWasPreserved(FakeFilesystem filesystem, const std::uint32_t error) {
    filesystem.replaceError = error;
    const ReplaceTransitionResult result = Execute(filesystem);
    return !result.committed
        && result.replaceError == error
        && result.recoveryState == ReplaceRecoveryState::DestinationPreserved
        && filesystem.files.at("target") == "old"
        && !filesystem.files.contains("replacement")
        && !filesystem.files.contains("backup")
        && !result.retainReplacement
        && !result.retainBackup;
}

} // namespace

int main() {
    {
        FakeFilesystem filesystem = InitialFilesystem(0U);
        const ReplaceTransitionResult result = Execute(filesystem);
        if (!result.committed
            || result.recoveryState != ReplaceRecoveryState::None
            || filesystem.files.at("target") != "new"
            || filesystem.files.contains("replacement")
            || filesystem.files.contains("backup")) {
            return EXIT_FAILURE;
        }
    }

    // These cover every documented name-preserving ReplaceFileW failure,
    // including the critic's ERROR_UNABLE_TO_MOVE_REPLACEMENT regression.
    if (!OriginalWasPreserved(InitialFilesystem(kUnableToRemoveReplaced),
                              kUnableToRemoveReplaced)
        || !OriginalWasPreserved(InitialFilesystem(kUnableToMoveReplacement),
                                 kUnableToMoveReplacement)
        || !OriginalWasPreserved(InitialFilesystem(kInjectedFailure),
                                 kInjectedFailure)) {
        return EXIT_FAILURE;
    }

    // ERROR_UNABLE_TO_MOVE_REPLACEMENT_2 moves the original to the explicit
    // backup. The shared state machine restores it before deleting the new temp.
    {
        FakeFilesystem filesystem = InitialFilesystem(kUnableToMoveReplacement2);
        const ReplaceTransitionResult result = Execute(filesystem);
        if (result.committed
            || result.replaceError != kUnableToMoveReplacement2
            || result.recoveryState != ReplaceRecoveryState::DestinationRestored
            || filesystem.files.at("target") != "old"
            || filesystem.files.contains("replacement")
            || filesystem.files.contains("backup")
            || result.retainReplacement
            || result.retainBackup) {
            return EXIT_FAILURE;
        }
    }

    // A failed restore deliberately leaves both independently named artifacts
    // and reports their retention instead of allowing RAII cleanup to erase one.
    {
        FakeFilesystem filesystem = InitialFilesystem(kUnableToMoveReplacement2);
        filesystem.failRestore = true;
        const ReplaceTransitionResult result = Execute(filesystem);
        if (result.recoveryState != ReplaceRecoveryState::ManualRecoveryRequired
            || result.recoveryError != kInjectedFailure
            || filesystem.files.contains("target")
            || filesystem.files.at("replacement") != "new"
            || filesystem.files.at("backup") != "old"
            || !result.retainReplacement
            || !result.retainBackup) {
            return EXIT_FAILURE;
        }
    }

    {
        FakeFilesystem filesystem = InitialFilesystem(kUnableToMoveReplacement);
        filesystem.removeFailurePath = "replacement";
        const ReplaceTransitionResult result = Execute(filesystem);
        if (result.recoveryState != ReplaceRecoveryState::DestinationPreserved
            || result.replacementCleanupError != kInjectedFailure
            || !result.retainReplacement
            || filesystem.files.at("replacement") != "new") {
            return EXIT_FAILURE;
        }
    }

    // Once the new file is committed, failure to delete the old backup is a
    // committed warning and the backup remains available at its exact path.
    {
        FakeFilesystem filesystem = InitialFilesystem(0U);
        filesystem.removeFailurePath = "backup";
        const ReplaceTransitionResult result = Execute(filesystem);
        if (!result.committed
            || result.backupCleanupError != kInjectedFailure
            || !result.retainBackup
            || filesystem.files.at("target") != "new"
            || filesystem.files.at("backup") != "old") {
            return EXIT_FAILURE;
        }
    }

    {
        FakeFilesystem filesystem = InitialFilesystem(kUnableToMoveReplacement);
        filesystem.inspectFailurePath = "target";
        const ReplaceTransitionResult result = Execute(filesystem);
        if (result.recoveryState != ReplaceRecoveryState::ManualRecoveryRequired
            || result.inspectionError != kInjectedFailure
            || !result.retainReplacement) {
            return EXIT_FAILURE;
        }
    }

    // A foreign file appearing at the selected backup name makes the state
    // ambiguous. Preserve every named artifact; never infer that either one is
    // safe to delete.
    {
        FakeFilesystem filesystem = InitialFilesystem(kInjectedFailure);
        filesystem.files.emplace("backup", "foreign");
        const ReplaceTransitionResult result = Execute(filesystem);
        if (result.recoveryState != ReplaceRecoveryState::ManualRecoveryRequired
            || !result.retainReplacement
            || !result.retainBackup
            || filesystem.files.at("target") != "old"
            || filesystem.files.at("replacement") != "new"
            || filesystem.files.at("backup") != "foreign") {
            return EXIT_FAILURE;
        }
    }

    // If neither the destination nor backup can be found after failure, the
    // synchronized replacement is the only known complete artifact and must be
    // retained for manual recovery.
    {
        FakeFilesystem filesystem = InitialFilesystem(kInjectedFailure);
        filesystem.files.erase("target");
        const ReplaceTransitionResult result = Execute(filesystem);
        if (result.recoveryState != ReplaceRecoveryState::ManualRecoveryRequired
            || !result.retainReplacement
            || result.retainBackup
            || filesystem.files.at("replacement") != "new") {
            return EXIT_FAILURE;
        }
    }

    {
        FakeFilesystem filesystem = InitialFilesystem(kUnableToMoveReplacement);
        filesystem.inspectFailurePath = "backup";
        const ReplaceTransitionResult result = Execute(filesystem);
        if (result.recoveryState != ReplaceRecoveryState::ManualRecoveryRequired
            || result.inspectionError != kInjectedFailure
            || !result.retainReplacement
            || !result.retainBackup) {
            return EXIT_FAILURE;
        }
    }

    // Restore is deliberately no-replace. If another writer recreates the
    // target after inspection, keep its target plus both recovery artifacts.
    {
        FakeFilesystem filesystem = InitialFilesystem(kUnableToMoveReplacement2);
        filesystem.targetAppearsDuringRestore = true;
        const ReplaceTransitionResult result = Execute(filesystem);
        if (result.recoveryState != ReplaceRecoveryState::ManualRecoveryRequired
            || result.recoveryError != kInjectedFailure
            || !result.retainReplacement
            || !result.retainBackup
            || filesystem.files.at("target") != "concurrent-writer"
            || filesystem.files.at("replacement") != "new"
            || filesystem.files.at("backup") != "old") {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
