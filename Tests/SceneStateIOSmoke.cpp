#include "RawIron/Scene/SceneStateIO.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

namespace fs = std::filesystem;

[[nodiscard]] bool SameVec3(const ri::math::Vec3& lhs, const ri::math::Vec3& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

[[nodiscard]] bool SameTransform(const ri::scene::Transform& lhs, const ri::scene::Transform& rhs) {
    return SameVec3(lhs.position, rhs.position)
        && SameVec3(lhs.rotationDegrees, rhs.rotationDegrees)
        && SameVec3(lhs.scale, rhs.scale);
}

[[nodiscard]] bool SameSceneTransforms(const ri::scene::Scene& lhs, const ri::scene::Scene& rhs) {
    if (lhs.NodeCount() != rhs.NodeCount()) {
        return false;
    }
    for (std::size_t index = 0U; index < lhs.NodeCount(); ++index) {
        if (!SameTransform(lhs.Nodes()[index].localTransform, rhs.Nodes()[index].localTransform)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool PrefixMatches(const ri::scene::Scene& scene, const ri::scene::Scene& prefix) {
    if (scene.NodeCount() < prefix.NodeCount()) {
        return false;
    }
    for (std::size_t index = 0U; index < prefix.NodeCount(); ++index) {
        if (!SameTransform(scene.Nodes()[index].localTransform, prefix.Nodes()[index].localTransform)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::uint64_t CurrentProcessIdValue() {
#if defined(_WIN32)
    return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(getpid());
#endif
}

[[nodiscard]] std::string ReadText(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

[[nodiscard]] bool WriteText(const fs::path& path, const std::string& text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        return false;
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.flush();
    const bool flushed = stream.good();
    stream.close();
    return flushed && !stream.fail();
}

[[nodiscard]] bool ReplaceFirst(std::string& text, const std::string& from, const std::string& to) {
    const std::size_t position = text.find(from);
    if (position == std::string::npos) {
        return false;
    }
    text.replace(position, from.size(), to);
    return true;
}

[[nodiscard]] bool ReplaceTokenAfter(std::string& text,
                                     const std::string& marker,
                                     const std::string& replacement) {
    const std::size_t markerPosition = text.find(marker);
    if (markerPosition == std::string::npos) {
        return false;
    }
    const std::size_t tokenStart = markerPosition + marker.size();
    const std::size_t tokenEnd = text.find_first_of(" \r\n", tokenStart);
    if (tokenEnd == std::string::npos) {
        return false;
    }
    text.replace(tokenStart, tokenEnd - tokenStart, replacement);
    return true;
}

[[nodiscard]] bool HasTemporarySceneStateFiles(const fs::path& directory) {
    std::error_code iterationError;
    for (fs::directory_iterator it(directory, iterationError), end; !iterationError && it != end;
         it.increment(iterationError)) {
        const std::string filename = it->path().filename().string();
        if ((filename.rfind(".ri-scene-", 0U) == 0U
             && (it->path().extension() == ".tmp" || it->path().extension() == ".bak"))
            || filename.find(".tmp.") != std::string::npos
            || (filename.rfind("~RF", 0U) == 0U && it->path().extension() == ".TMP")) {
            return true;
        }
    }
    return static_cast<bool>(iterationError);
}

ri::scene::Scene BuildSourceScene() {
    ri::scene::Scene scene{"SceneStateSource"};
    const int root = scene.CreateNode("Root");
    const int quoted = scene.CreateNode("Quoted \"Node\" \\ Path", root);
    const int third = scene.CreateNode("Third Node", root);

    scene.GetNode(root).localTransform = {
        .position = {0.123456791f, -98765.4297f, 42.0000038f},
        .rotationDegrees = {-179.999985f, 89.1234589f, 0.000012345678f},
        .scale = {0.5f, 1.25f, 2.75f},
    };
    scene.GetNode(quoted).localTransform = {
        .position = {-12.0f, 0.0f, 999.25f},
        .rotationDegrees = {360.0f, -720.5f, 45.125f},
        .scale = {1.0f, 1.0f, 1.0f},
    };
    scene.GetNode(third).localTransform = {
        .position = {3.0f, 2.0f, 1.0f},
        .rotationDegrees = {11.0f, 22.0f, 33.0f},
        .scale = {4.0f, 5.0f, 6.0f},
    };
    return scene;
}

ri::scene::Scene BuildLoadTarget() {
    ri::scene::Scene scene{"SceneStateTarget"};
    const int root = scene.CreateNode("Root");
    (void)scene.CreateNode("Quoted \"Node\" \\ Path", root);
    (void)scene.CreateNode("Third Node", root);
    (void)scene.CreateNode("Newer Node", root);
    for (std::size_t index = 0U; index < scene.NodeCount(); ++index) {
        scene.GetNode(static_cast<int>(index)).localTransform = {
            .position = {-1000.0f - static_cast<float>(index), -2000.0f, -3000.0f},
            .rotationDegrees = {-10.0f, -20.0f, -30.0f},
            .scale = {7.0f, 8.0f, 9.0f},
        };
    }
    return scene;
}

} // namespace

int main() {
    const auto uniqueStamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path()
        / ("RawIronSceneStateIOSmoke." + std::to_string(uniqueStamp));
    std::error_code cleanupError;
    fs::remove_all(root, cleanupError);
    fs::create_directories(root, cleanupError);
    if (cleanupError) {
        return EXIT_FAILURE;
    }

    const fs::path validPath = root / "scene_state.ri_state";
    const ri::scene::Scene source = BuildSourceScene();

    // The first generated candidate is deliberately occupied. CREATE_NEW /
    // O_EXCL must retry without truncating or deleting the foreign file.
    const fs::path foreignTemporary = root
        / (".ri-scene-" + std::to_string(CurrentProcessIdValue()) + "-0.tmp");
    const std::string foreignBytes = "foreign temporary owned by another writer";
    if (!WriteText(foreignTemporary, foreignBytes)) {
        return EXIT_FAILURE;
    }
    const ri::scene::SceneStateIOResult firstSave =
        ri::scene::SaveSceneNodeTransformsDetailed(source, validPath);
    if (!firstSave.Succeeded()
        || !firstSave.committed
        || !firstSave.fileDataSynchronized
        || (!firstSave.parentDirectorySynchronized && !firstSave.parentDirectorySyncUnsupported)
        || ReadText(foreignTemporary) != foreignBytes) {
        return EXIT_FAILURE;
    }
    fs::remove(foreignTemporary, cleanupError);
    if (cleanupError) {
        return EXIT_FAILURE;
    }
    const std::string validText = ReadText(validPath);
    if (validText.empty() || HasTemporarySceneStateFiles(root)) {
        return EXIT_FAILURE;
    }

    // A valid older prefix snapshot remains loadable after a scene gains nodes.
    // Saved float precision must also preserve every transform exactly.
    ri::scene::Scene target = BuildLoadTarget();
    const ri::scene::Transform newerNodeBefore = target.Nodes()[3].localTransform;
    if (!ri::scene::LoadSceneNodeTransforms(target, validPath)) {
        return EXIT_FAILURE;
    }
    for (std::size_t index = 0U; index < source.NodeCount(); ++index) {
        if (!SameTransform(source.Nodes()[index].localTransform, target.Nodes()[index].localTransform)) {
            return EXIT_FAILURE;
        }
    }
    if (!SameTransform(newerNodeBefore, target.Nodes()[3].localTransform)) {
        return EXIT_FAILURE;
    }

    // The legacy editor magic remains source-compatible with the transform
    // grammar and still uses the same validate-before-commit path.
    std::string legacyEditorText = validText;
    if (!ReplaceFirst(legacyEditorText, "RAWIRON_SCENE_STATE_V1", "RAWIRON_EDITOR_STATE_V1")) {
        return EXIT_FAILURE;
    }
    const fs::path legacyPath = root / "legacy_editor_state.ri_state";
    if (!WriteText(legacyPath, legacyEditorText)) {
        return EXIT_FAILURE;
    }
    ri::scene::Scene legacyTarget = BuildLoadTarget();
    if (!ri::scene::LoadSceneNodeTransformsDetailed(legacyTarget, legacyPath).Succeeded()
        || !PrefixMatches(legacyTarget, source)) {
        return EXIT_FAILURE;
    }

    // Zero-node snapshots are an explicit valid no-op, including when loaded
    // into a newer non-empty scene.
    const fs::path emptyPath = root / "empty_scene_state.ri_state";
    const ri::scene::Scene emptySource{"EmptySceneState"};
    const ri::scene::SceneStateIOResult emptySave =
        ri::scene::SaveSceneNodeTransformsDetailed(emptySource, emptyPath);
    ri::scene::Scene emptyTarget = BuildLoadTarget();
    const ri::scene::Scene emptyTargetBefore = emptyTarget;
    const ri::scene::SceneStateIOResult emptyLoad =
        ri::scene::LoadSceneNodeTransformsDetailed(emptyTarget, emptyPath);
    if (!emptySave.Succeeded()
        || !emptyLoad.Succeeded()
        || !emptyLoad.committed
        || !SameSceneTransforms(emptyTarget, emptyTargetBefore)) {
        return EXIT_FAILURE;
    }

    // max_digits10 plus from_chars must round-trip every finite float boundary
    // used by creator transforms, including signed zero and denormals.
    ri::scene::Scene numericSource = source;
    numericSource.GetNode(0).localTransform = {
        .position = {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::denorm_min(),
        },
        .rotationDegrees = {
            -0.0f,
            std::numeric_limits<float>::min(),
            -std::numeric_limits<float>::denorm_min(),
        },
        .scale = {1.0f, 0.0f, -1.0f},
    };
    const fs::path numericPath = root / "numeric_boundaries.ri_state";
    if (!ri::scene::SaveSceneNodeTransformsDetailed(numericSource, numericPath).Succeeded()) {
        return EXIT_FAILURE;
    }
    ri::scene::Scene numericTarget = BuildLoadTarget();
    if (!ri::scene::LoadSceneNodeTransformsDetailed(numericTarget, numericPath).Succeeded()
        || !PrefixMatches(numericTarget, numericSource)
        || !std::signbit(numericTarget.Nodes()[0].localTransform.rotationDegrees.x)) {
        return EXIT_FAILURE;
    }

    std::vector<std::string> corruptDocuments;
    bool preparedAllCorruptions = true;
    const auto addReplacement = [&](const std::string& from, const std::string& to) {
        std::string damaged = validText;
        preparedAllCorruptions = ReplaceFirst(damaged, from, to) && preparedAllCorruptions;
        corruptDocuments.push_back(std::move(damaged));
    };

    addReplacement("RAWIRON_SCENE_STATE_V1", "NOT_A_SCENE_STATE");
    addReplacement("node_count 3", "node_count 2"); // undeclared trailing record
    addReplacement("node_count 3", "node_count 4"); // missing fourth record
    addReplacement("node 2 \"Third Node\"", "node 1 \"Third Node\""); // duplicate index
    addReplacement("node 2 \"Third Node\"", "node 3 \"Third Node\""); // index outside declared range
    addReplacement("node 1 ", "invalid_record 1 ");
    addReplacement("\"Root\"", "\"Wrong Root\"");
    addReplacement("\"Root\"", "Root"); // quoted names are part of the V1 grammar

    std::string nonFinite = validText;
    preparedAllCorruptions = ReplaceTokenAfter(nonFinite, "node 0 \"Root\" ", "nan")
        && preparedAllCorruptions;
    corruptDocuments.push_back(std::move(nonFinite));

    std::string truncated = validText;
    const std::size_t lastValueSeparator = truncated.find_last_of(' ');
    if (lastValueSeparator == std::string::npos) {
        preparedAllCorruptions = false;
    } else {
        truncated.resize(lastValueSeparator);
    }
    corruptDocuments.push_back(std::move(truncated));
    corruptDocuments.push_back(validText + "unexpected trailing record\n");
    if (!preparedAllCorruptions) {
        return EXIT_FAILURE;
    }

    const fs::path corruptPath = root / "corrupt.ri_state";
    const ri::scene::Scene unchanged = BuildLoadTarget();
    for (const std::string& corruptDocument : corruptDocuments) {
        if (!WriteText(corruptPath, corruptDocument)) {
            return EXIT_FAILURE;
        }
        ri::scene::Scene candidate = unchanged;
        if (ri::scene::LoadSceneNodeTransforms(candidate, corruptPath)
            || !SameSceneTransforms(candidate, unchanged)) {
            return EXIT_FAILURE;
        }
    }

    const auto expectLoadError = [&](std::string damaged, const ri::scene::SceneStateIOError expected) {
        if (!WriteText(corruptPath, damaged)) {
            return false;
        }
        ri::scene::Scene candidate = unchanged;
        const ri::scene::SceneStateIOResult result =
            ri::scene::LoadSceneNodeTransformsDetailed(candidate, corruptPath);
        return result.error == expected && !result.committed && SameSceneTransforms(candidate, unchanged);
    };
    std::string explicitNan = validText;
    std::string explicitInfinity = validText;
    std::string explicitOverflow = validText;
    if (!ReplaceTokenAfter(explicitNan, "node 0 \"Root\" ", "nan")
        || !ReplaceTokenAfter(explicitInfinity, "node 0 \"Root\" ", "inf")
        || !ReplaceTokenAfter(explicitOverflow, "node 0 \"Root\" ", "1e999")
        || !expectLoadError(std::move(explicitNan), ri::scene::SceneStateIOError::NonFiniteTransform)
        || !expectLoadError(std::move(explicitInfinity), ri::scene::SceneStateIOError::NonFiniteTransform)
        || !expectLoadError(std::move(explicitOverflow), ri::scene::SceneStateIOError::NumericOutOfRange)) {
        return EXIT_FAILURE;
    }

    // The reader's byte bound is enforced before parsing or mutating the scene.
    if (!WriteText(corruptPath, std::string(16U * 1024U * 1024U + 1U, 'x'))) {
        return EXIT_FAILURE;
    }
    ri::scene::Scene oversizedCandidate = unchanged;
    if (ri::scene::LoadSceneNodeTransforms(oversizedCandidate, corruptPath)
        || !SameSceneTransforms(oversizedCandidate, unchanged)) {
        return EXIT_FAILURE;
    }

    // The writer accepts exactly the documented byte limit and rejects the
    // very next byte during its allocation-free counting pass.
    ri::scene::Scene sizeProbe{"SceneStateSizeProbe"};
    const int sizeProbeNode = sizeProbe.CreateNode("");
    const fs::path sizeProbePath = root / "size_probe.ri_state";
    if (!ri::scene::SaveSceneNodeTransformsDetailed(sizeProbe, sizeProbePath).Succeeded()) {
        return EXIT_FAILURE;
    }
    const std::string sizeProbeBytes = ReadText(sizeProbePath);
    if (sizeProbeBytes.size() >= ri::scene::kMaxSceneStateFileBytes) {
        return EXIT_FAILURE;
    }
    const std::size_t exactNameBytes = static_cast<std::size_t>(ri::scene::kMaxSceneStateFileBytes)
        - sizeProbeBytes.size();
    sizeProbe.GetNode(sizeProbeNode).name.assign(exactNameBytes, 'b');
    const fs::path exactBoundPath = root / "exact_bound.ri_state";
    const ri::scene::SceneStateIOResult exactBoundSave =
        ri::scene::SaveSceneNodeTransformsDetailed(sizeProbe, exactBoundPath);
    std::error_code sizeError;
    if (!exactBoundSave.Succeeded()
        || fs::file_size(exactBoundPath, sizeError) != ri::scene::kMaxSceneStateFileBytes
        || sizeError) {
        return EXIT_FAILURE;
    }
    sizeProbe.GetNode(sizeProbeNode).name.push_back('b');
    const ri::scene::SceneStateIOResult oneByteTooLarge =
        ri::scene::SaveSceneNodeTransformsDetailed(sizeProbe, exactBoundPath);
    if (oneByteTooLarge.error != ri::scene::SceneStateIOError::SerializedDataTooLarge
        || oneByteTooLarge.committed
        || fs::file_size(exactBoundPath, sizeError) != ri::scene::kMaxSceneStateFileBytes
        || sizeError
        || HasTemporarySceneStateFiles(root)) {
        return EXIT_FAILURE;
    }

    // A rejected save must never truncate the previously valid snapshot.
    ri::scene::Scene invalidSource = source;
    invalidSource.GetNode(0).localTransform.position.x = std::numeric_limits<float>::quiet_NaN();
    const ri::scene::SceneStateIOResult invalidSave =
        ri::scene::SaveSceneNodeTransformsDetailed(invalidSource, validPath);
    if (invalidSave.error != ri::scene::SceneStateIOError::NonFiniteTransform
        || invalidSave.committed
        || ReadText(validPath) != validText) {
        return EXIT_FAILURE;
    }

    // A huge untrusted name is rejected by preflight, before any temp is owned.
    ri::scene::Scene oversizedSource = source;
    oversizedSource.GetNode(0).name.assign(16U * 1024U * 1024U, 'n');
    const ri::scene::SceneStateIOResult oversizedSave =
        ri::scene::SaveSceneNodeTransformsDetailed(oversizedSource, validPath);
    if (oversizedSave.error != ri::scene::SceneStateIOError::SerializedDataTooLarge
        || oversizedSave.committed
        || ReadText(validPath) != validText
        || HasTemporarySceneStateFiles(root)) {
        return EXIT_FAILURE;
    }

    // Concurrent writers must own distinct temporaries; the destination may be
    // either complete snapshot, never a mixture or truncated intermediate.
    const fs::path concurrentPath = root / "concurrent_scene_state.ri_state";
    ri::scene::Scene concurrentA = source;
    ri::scene::Scene concurrentB = source;
    concurrentA.GetNode(0).localTransform.position = {111.0f, 222.0f, 333.0f};
    concurrentB.GetNode(0).localTransform.position = {-444.0f, -555.0f, -666.0f};
    if (!ri::scene::SaveSceneNodeTransformsDetailed(concurrentA, concurrentPath).Succeeded()) {
        return EXIT_FAILURE;
    }
    std::atomic<bool> startConcurrent{false};
    std::atomic<int> concurrentFailures{0};
    std::vector<std::thread> writers;
    for (int writerIndex = 0; writerIndex < 4; ++writerIndex) {
        writers.emplace_back([&, writerIndex]() {
            while (!startConcurrent.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            const ri::scene::Scene& writerScene = (writerIndex % 2) == 0 ? concurrentA : concurrentB;
            for (int iteration = 0; iteration < 8; ++iteration) {
                if (!ri::scene::SaveSceneNodeTransformsDetailed(writerScene, concurrentPath).Succeeded()) {
                    concurrentFailures.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    startConcurrent.store(true, std::memory_order_release);
    for (std::thread& writer : writers) {
        writer.join();
    }
    ri::scene::Scene concurrentTarget = BuildLoadTarget();
    if (concurrentFailures.load(std::memory_order_relaxed) != 0
        || !ri::scene::LoadSceneNodeTransformsDetailed(concurrentTarget, concurrentPath).Succeeded()
        || (!PrefixMatches(concurrentTarget, concurrentA) && !PrefixMatches(concurrentTarget, concurrentB))
        || HasTemporarySceneStateFiles(root)) {
        return EXIT_FAILURE;
    }

    // Saving/loading through a symlink/reparse destination is explicitly unsupported;
    // if the platform permits creating one, neither the link nor target moves.
    const fs::path symlinkPath = root / "scene_state_link.ri_state";
    std::error_code symlinkError;
    fs::create_symlink(validPath, symlinkPath, symlinkError);
    if (!symlinkError) {
        ri::scene::Scene symlinkReplacement = source;
        symlinkReplacement.GetNode(0).localTransform.position.x += 123.0f;
        const ri::scene::SceneStateIOResult symlinkSave =
            ri::scene::SaveSceneNodeTransformsDetailed(symlinkReplacement, symlinkPath);
        if (symlinkSave.error != ri::scene::SceneStateIOError::DestinationSymlinkUnsupported
            || symlinkSave.committed
            || ReadText(validPath) != validText
            || !fs::is_symlink(fs::symlink_status(symlinkPath, symlinkError))
            || symlinkError) {
            return EXIT_FAILURE;
        }
        ri::scene::Scene symlinkLoadTarget = BuildLoadTarget();
        const ri::scene::SceneStateIOResult symlinkLoad =
            ri::scene::LoadSceneNodeTransformsDetailed(symlinkLoadTarget, symlinkPath);
        if (symlinkLoad.Succeeded()
            || (symlinkLoad.error != ri::scene::SceneStateIOError::DestinationSymlinkUnsupported
                && symlinkLoad.error != ri::scene::SceneStateIOError::InputInspectionFailed)) {
            return EXIT_FAILURE;
        }
        fs::remove(symlinkPath, symlinkError);
        if (symlinkError) {
            return EXIT_FAILURE;
        }
    }

#if defined(_WIN32)
    {
        // Parent directory junctions must also fail closed (leaf checks are not enough).
        const fs::path outside = root / "outside_divert";
        const fs::path aliasParent = root / "alias_parent";
        fs::create_directories(outside, cleanupError);
        fs::remove_all(aliasParent, cleanupError);
        const bool aliasOk =
            CreateSymbolicLinkW(aliasParent.c_str(),
                                outside.c_str(),
                                SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)
            || CreateSymbolicLinkW(aliasParent.c_str(), outside.c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY);
        if (aliasOk) {
            const fs::path divertedPath = aliasParent / "diverted.ri_state";
            const fs::path outsideVictim = outside / "diverted.ri_state";
            ri::scene::Scene diverted = source;
            diverted.GetNode(0).localTransform.position.x += 55.0f;
            const ri::scene::SceneStateIOResult divertedSave =
                ri::scene::SaveSceneNodeTransformsDetailed(diverted, divertedPath);
            if (divertedSave.error != ri::scene::SceneStateIOError::DestinationSymlinkUnsupported
                || divertedSave.committed
                || fs::exists(outsideVictim, cleanupError)) {
                return EXIT_FAILURE;
            }
            fs::remove_all(aliasParent, cleanupError);
            fs::remove_all(outside, cleanupError);
        }
    }
#endif

#if !defined(_WIN32)
    // POSIX replacement copies deterministic permission bits from the current
    // destination before fsync + rename.
    const fs::path metadataPath = root / "metadata_scene_state.ri_state";
    if (!ri::scene::SaveSceneNodeTransformsDetailed(source, metadataPath).Succeeded()) {
        return EXIT_FAILURE;
    }
    constexpr fs::perms expectedPermissions = fs::perms::owner_read | fs::perms::owner_write;
    fs::permissions(metadataPath, expectedPermissions, fs::perm_options::replace, cleanupError);
    ri::scene::Scene metadataReplacement = source;
    metadataReplacement.GetNode(0).localTransform.position.x += 77.0f;
    const ri::scene::SceneStateIOResult metadataSave =
        ri::scene::SaveSceneNodeTransformsDetailed(metadataReplacement, metadataPath);
    constexpr fs::perms permissionMask = fs::perms::owner_all
        | fs::perms::group_all
        | fs::perms::others_all;
    const fs::perms actualPermissions = fs::status(metadataPath, cleanupError).permissions() & permissionMask;
    if (cleanupError || !metadataSave.Succeeded() || actualPermissions != expectedPermissions) {
        return EXIT_FAILURE;
    }
#endif

#if defined(_WIN32)
    // Holding the destination without delete sharing provides a deterministic
    // atomic-replace failure after the temporary file has been fully written.
    HANDLE destinationLock = CreateFileW(validPath.c_str(),
                                         GENERIC_READ,
                                         FILE_SHARE_READ,
                                         nullptr,
                                         OPEN_EXISTING,
                                         FILE_ATTRIBUTE_NORMAL,
                                         nullptr);
    if (destinationLock == INVALID_HANDLE_VALUE) {
        return EXIT_FAILURE;
    }
    ri::scene::Scene replacement = source;
    replacement.GetNode(0).localTransform.position.x += 10.0f;
    const ri::scene::SceneStateIOResult lockedSave =
        ri::scene::SaveSceneNodeTransformsDetailed(replacement, validPath);
    CloseHandle(destinationLock);
    if (lockedSave.error != ri::scene::SceneStateIOError::AtomicReplaceFailed
        || lockedSave.committed
        || lockedSave.recoveryState != ri::scene::SceneStateIORecoveryState::DestinationPreserved
        || lockedSave.temporaryCleanupFailed
        || !lockedSave.retainedBackupPath.empty()
        || !lockedSave.retainedReplacementPath.empty()
        || ReadText(validPath) != validText
        || HasTemporarySceneStateFiles(root)) {
        return EXIT_FAILURE;
    }
#endif

    ri::scene::Scene preservedTarget = BuildLoadTarget();
    if (!ri::scene::LoadSceneNodeTransforms(preservedTarget, validPath)) {
        return EXIT_FAILURE;
    }
    for (std::size_t index = 0U; index < source.NodeCount(); ++index) {
        if (!SameTransform(source.Nodes()[index].localTransform, preservedTarget.Nodes()[index].localTransform)) {
            return EXIT_FAILURE;
        }
    }

    fs::remove_all(root, cleanupError);
    return cleanupError ? EXIT_FAILURE : EXIT_SUCCESS;
}
