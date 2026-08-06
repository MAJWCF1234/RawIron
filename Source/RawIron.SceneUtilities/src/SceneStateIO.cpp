#include "RawIron/Scene/SceneStateIO.h"

#include "RawIron/Math/Vec3.h"
#include "SceneStateIOCommitRecovery.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <mutex>
#include <new>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ri::scene {
namespace {

namespace fs = std::filesystem;

constexpr std::size_t kTemporaryCollisionRetries = 64U;

struct PendingNodeTransform {
    std::size_t index = 0U;
    Transform transform{};
};

struct DestinationInfo {
    bool exists = false;
#if !defined(_WIN32)
    mode_t mode = 0;
#endif
};

struct OwnedTemporaryFile {
    fs::path path{};
#if defined(_WIN32)
    fs::path backupPath{};
#endif
#if defined(_WIN32)
    HANDLE handle = INVALID_HANDLE_VALUE;
#else
    int descriptor = -1;
#endif
    bool ownsPath = false;

    OwnedTemporaryFile() = default;
    OwnedTemporaryFile(const OwnedTemporaryFile&) = delete;
    OwnedTemporaryFile& operator=(const OwnedTemporaryFile&) = delete;

    ~OwnedTemporaryFile() noexcept {
#if defined(_WIN32)
        if (handle != INVALID_HANDLE_VALUE) {
            (void)CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
        if (ownsPath && !path.empty()) {
            (void)DeleteFileW(path.c_str());
        }
#else
        if (descriptor >= 0) {
            (void)close(descriptor);
            descriptor = -1;
        }
        if (ownsPath && !path.empty()) {
            (void)unlink(path.c_str());
        }
#endif
    }

    [[nodiscard]] bool IsOpen() const noexcept {
#if defined(_WIN32)
        return handle != INVALID_HANDLE_VALUE;
#else
        return descriptor >= 0;
#endif
    }

    void Disarm() noexcept {
        ownsPath = false;
        path.clear();
#if defined(_WIN32)
        backupPath.clear();
#endif
    }
};

class CountingWriter {
public:
    explicit CountingWriter(const std::size_t limit) noexcept : limit_(limit) {}

    [[nodiscard]] bool Append(const std::string_view text) noexcept {
        if (text.size() > Remaining()) {
            exceeded_ = true;
            return false;
        }
        size_ += text.size();
        return true;
    }

    [[nodiscard]] bool AppendChar(const char value) noexcept {
        if (Remaining() == 0U) {
            exceeded_ = true;
            return false;
        }
        (void)value;
        ++size_;
        return true;
    }

    [[nodiscard]] std::size_t Size() const noexcept { return size_; }
    [[nodiscard]] std::size_t Remaining() const noexcept { return limit_ - size_; }
    [[nodiscard]] bool Exceeded() const noexcept { return exceeded_; }

private:
    std::size_t limit_ = 0U;
    std::size_t size_ = 0U;
    bool exceeded_ = false;
};

class FixedBufferWriter {
public:
    FixedBufferWriter(char* output, const std::size_t size) noexcept : output_(output), size_(size) {}

    [[nodiscard]] bool Append(const std::string_view text) noexcept {
        if (text.size() > Remaining()) {
            return false;
        }
        if (!text.empty()) {
            std::memcpy(output_ + position_, text.data(), text.size());
            position_ += text.size();
        }
        return true;
    }

    [[nodiscard]] bool AppendChar(const char value) noexcept {
        if (Remaining() == 0U) {
            return false;
        }
        output_[position_++] = value;
        return true;
    }

    [[nodiscard]] std::size_t Size() const noexcept { return position_; }
    [[nodiscard]] std::size_t Remaining() const noexcept { return size_ - position_; }

private:
    char* output_ = nullptr;
    std::size_t size_ = 0U;
    std::size_t position_ = 0U;
};

[[nodiscard]] SceneStateIOResult Failure(const SceneStateIOError error,
                                         const std::error_code systemError = {}) {
    SceneStateIOResult result{};
    result.error = error;
    result.systemError = systemError;
    return result;
}

#if defined(_WIN32)
[[nodiscard]] std::error_code WindowsError(const DWORD value = GetLastError()) {
    return {static_cast<int>(value), std::system_category()};
}
#else
[[nodiscard]] std::error_code PosixError(const int value = errno) {
    return {value, std::generic_category()};
}
#endif

[[nodiscard]] bool Vec3Finite(const ri::math::Vec3& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool TransformFinite(const Transform& transform) noexcept {
    return Vec3Finite(transform.position)
        && Vec3Finite(transform.rotationDegrees)
        && Vec3Finite(transform.scale);
}

template <typename Writer, typename Integer>
    requires std::is_integral_v<Integer>
[[nodiscard]] bool AppendInteger(Writer& writer, const Integer value) noexcept {
    char buffer[32]{};
    const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return converted.ec == std::errc{}
        && writer.Append(std::string_view(buffer, static_cast<std::size_t>(converted.ptr - buffer)));
}

template <typename Writer>
[[nodiscard]] bool AppendFloat(Writer& writer, const float value) noexcept {
    char buffer[64]{};
    const auto converted = std::to_chars(buffer,
                                         buffer + sizeof(buffer),
                                         value,
                                         std::chars_format::general,
                                         std::numeric_limits<float>::max_digits10);
    return converted.ec == std::errc{}
        && writer.Append(std::string_view(buffer, static_cast<std::size_t>(converted.ptr - buffer)));
}

template <typename Writer>
[[nodiscard]] bool AppendQuotedName(Writer& writer, const std::string_view name) noexcept {
    if (writer.Remaining() < 2U) {
        return writer.Append("\"\"");
    }
    if (!writer.AppendChar('"')) {
        return false;
    }
    for (const char value : name) {
        if ((value == '"' || value == '\\') && !writer.AppendChar('\\')) {
            return false;
        }
        if (!writer.AppendChar(value)) {
            return false;
        }
    }
    return writer.AppendChar('"');
}

template <typename Writer>
[[nodiscard]] bool SerializeScene(const Scene& scene, Writer& writer) noexcept {
    if (!writer.Append("RAWIRON_SCENE_STATE_V1\nnode_count ")
        || !AppendInteger(writer, scene.NodeCount())
        || !writer.AppendChar('\n')) {
        return false;
    }

    const auto& nodes = scene.Nodes();
    for (std::size_t index = 0U; index < nodes.size(); ++index) {
        const Node& node = nodes[index];
        const Transform& transform = node.localTransform;
        if (!writer.Append("node ")
            || !AppendInteger(writer, index)
            || !writer.AppendChar(' ')
            || !AppendQuotedName(writer, node.name)
            || !writer.AppendChar(' ')
            || !AppendFloat(writer, transform.position.x)
            || !writer.AppendChar(' ')
            || !AppendFloat(writer, transform.position.y)
            || !writer.AppendChar(' ')
            || !AppendFloat(writer, transform.position.z)
            || !writer.AppendChar(' ')
            || !AppendFloat(writer, transform.rotationDegrees.x)
            || !writer.AppendChar(' ')
            || !AppendFloat(writer, transform.rotationDegrees.y)
            || !writer.AppendChar(' ')
            || !AppendFloat(writer, transform.rotationDegrees.z)
            || !writer.AppendChar(' ')
            || !AppendFloat(writer, transform.scale.x)
            || !writer.AppendChar(' ')
            || !AppendFloat(writer, transform.scale.y)
            || !writer.AppendChar(' ')
            || !AppendFloat(writer, transform.scale.z)
            || !writer.AppendChar('\n')) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] SceneStateIOResult SerializeBounded(const Scene& scene,
                                                  std::unique_ptr<char[]>& bytes,
                                                  std::size_t& byteCount) {
    if (scene.NodeCount() > kMaxSceneStateNodes) {
        return Failure(SceneStateIOError::SceneNodeLimitExceeded);
    }
    for (const Node& node : scene.Nodes()) {
        if (!TransformFinite(node.localTransform)) {
            return Failure(SceneStateIOError::NonFiniteTransform);
        }
    }

    CountingWriter counter(static_cast<std::size_t>(kMaxSceneStateFileBytes));
    if (!SerializeScene(scene, counter)) {
        return Failure(counter.Exceeded()
            ? SceneStateIOError::SerializedDataTooLarge
            : SceneStateIOError::SerializationFailed);
    }

    try {
        bytes = std::unique_ptr<char[]>(new char[counter.Size()]);
    } catch (const std::bad_alloc&) {
        return Failure(SceneStateIOError::SerializationFailed,
                       std::make_error_code(std::errc::not_enough_memory));
    }
    byteCount = counter.Size();
    FixedBufferWriter writer(bytes.get(), byteCount);
    if (!SerializeScene(scene, writer) || writer.Size() != byteCount) {
        bytes.reset();
        byteCount = 0U;
        return Failure(SceneStateIOError::SerializationFailed);
    }
    return {};
}

[[nodiscard]] std::uint64_t CurrentProcessIdValue() noexcept {
#if defined(_WIN32)
    return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(getpid());
#endif
}

struct TemporaryPaths {
    fs::path replacement{};
#if defined(_WIN32)
    fs::path backup{};
#endif
};

[[nodiscard]] TemporaryPaths NextTemporaryPaths(const fs::path& targetPath) {
    static std::atomic<std::uint64_t> sequence{0U};
    const std::string stem = ".ri-scene-" + std::to_string(CurrentProcessIdValue()) + "-"
        + std::to_string(sequence.fetch_add(1U, std::memory_order_relaxed));
    const fs::path directory = targetPath.parent_path().empty()
        ? fs::path(".")
        : targetPath.parent_path();
    TemporaryPaths paths{};
    paths.replacement = directory / (stem + ".tmp");
#if defined(_WIN32)
    paths.backup = directory / (stem + ".bak");
#endif
    return paths;
}

[[nodiscard]] SceneStateIOResult InspectDestination(const fs::path& path, DestinationInfo& info) {
    info = {};
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return {};
        }
        return Failure(SceneStateIOError::DestinationInspectionFailed, WindowsError(error));
    }
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) {
        return Failure(SceneStateIOError::DestinationSymlinkUnsupported);
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
        return Failure(SceneStateIOError::DestinationTypeUnsupported);
    }
    info.exists = true;
#else
    struct stat status {};
    if (lstat(path.c_str(), &status) != 0) {
        const int error = errno;
        if (error == ENOENT) {
            return {};
        }
        return Failure(SceneStateIOError::DestinationInspectionFailed, PosixError(error));
    }
    if (S_ISLNK(status.st_mode)) {
        return Failure(SceneStateIOError::DestinationSymlinkUnsupported);
    }
    if (!S_ISREG(status.st_mode)) {
        return Failure(SceneStateIOError::DestinationTypeUnsupported);
    }
    info.exists = true;
    info.mode = status.st_mode;
#endif
    return {};
}

[[nodiscard]] bool OpenExclusiveTemporary(const fs::path& targetPath,
                                          OwnedTemporaryFile& temporary,
                                          SceneStateIOResult& failure) {
    std::error_code lastCollisionError;
    for (std::size_t attempt = 0U; attempt < kTemporaryCollisionRetries; ++attempt) {
        TemporaryPaths candidates = NextTemporaryPaths(targetPath);
        // Assign paths before opening. If path allocation throws, no native
        // resource or filesystem entry has been acquired yet.
        temporary.path = std::move(candidates.replacement);
#if defined(_WIN32)
        temporary.backupPath = std::move(candidates.backup);
#endif
#if defined(_WIN32)
        HANDLE handle = CreateFileW(temporary.path.c_str(),
                                    GENERIC_WRITE,
                                    0U,
                                    nullptr,
                                    CREATE_NEW,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
        if (handle != INVALID_HANDLE_VALUE) {
            temporary.handle = handle;
            temporary.ownsPath = true;
            return true;
        }
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) {
            lastCollisionError = WindowsError(error);
            continue;
        }
        failure = Failure(SceneStateIOError::TemporaryFileOpenFailed, WindowsError(error));
        return false;
#else
        int flags = O_WRONLY | O_CREAT | O_EXCL;
#if defined(O_CLOEXEC)
        flags |= O_CLOEXEC;
#endif
#if defined(O_NOFOLLOW)
        flags |= O_NOFOLLOW;
#endif
        const int descriptor = open(temporary.path.c_str(), flags, 0666);
        if (descriptor >= 0) {
            temporary.descriptor = descriptor;
            temporary.ownsPath = true;
            return true;
        }
        const int error = errno;
        if (error == EEXIST) {
            lastCollisionError = PosixError(error);
            continue;
        }
        failure = Failure(SceneStateIOError::TemporaryFileOpenFailed, PosixError(error));
        return false;
#endif
    }
    failure = Failure(SceneStateIOError::TemporaryFileCollisionLimit, lastCollisionError);
    return false;
}

[[nodiscard]] bool CloseTemporary(OwnedTemporaryFile& temporary, std::error_code& error) noexcept {
    if (!temporary.IsOpen()) {
        return true;
    }
#if defined(_WIN32)
    const HANDLE handle = temporary.handle;
    temporary.handle = INVALID_HANDLE_VALUE;
    if (CloseHandle(handle) == FALSE) {
        error = WindowsError();
        return false;
    }
#else
    const int descriptor = temporary.descriptor;
    temporary.descriptor = -1;
    if (close(descriptor) != 0) {
        error = PosixError();
        return false;
    }
#endif
    return true;
}

[[nodiscard]] bool RemoveOwnedTemporary(const fs::path& path, std::error_code& error) noexcept {
    if (path.empty()) {
        return true;
    }
#if defined(_WIN32)
    if (DeleteFileW(path.c_str()) != FALSE) {
        return true;
    }
    const DWORD value = GetLastError();
    if (value == ERROR_FILE_NOT_FOUND || value == ERROR_PATH_NOT_FOUND) {
        return true;
    }
    error = WindowsError(value);
#else
    if (unlink(path.c_str()) == 0 || errno == ENOENT) {
        return true;
    }
    error = PosixError();
#endif
    return false;
}

[[nodiscard]] SceneStateIOResult FailAndCleanTemporary(OwnedTemporaryFile& temporary,
                                                       const SceneStateIOError error,
                                                       const std::error_code systemError = {}) {
    SceneStateIOResult result = Failure(error, systemError);
    std::error_code closeError;
    if (!CloseTemporary(temporary, closeError)) {
        result.temporaryCloseFailed = true;
        result.temporaryCloseError = closeError;
        if (result.error == SceneStateIOError::None) {
            result.error = SceneStateIOError::TemporaryCloseFailed;
            result.systemError = closeError;
        }
    }
    std::error_code cleanupError;
    if (!RemoveOwnedTemporary(temporary.path, cleanupError)) {
        result.temporaryCleanupFailed = true;
        result.temporaryCleanupError = cleanupError;
    } else {
        temporary.ownsPath = false;
        temporary.path.clear();
    }
    return result;
}

[[nodiscard]] SceneStateIOResult WriteTemporary(OwnedTemporaryFile& temporary,
                                                const std::string_view bytes) {
    std::size_t offset = 0U;
    while (offset < bytes.size()) {
#if defined(_WIN32)
        const DWORD chunkSize = static_cast<DWORD>(std::min<std::size_t>(
            bytes.size() - offset,
            static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
        DWORD written = 0U;
        if (WriteFile(temporary.handle, bytes.data() + offset, chunkSize, &written, nullptr) == FALSE
            || written == 0U) {
            return FailAndCleanTemporary(temporary,
                                         SceneStateIOError::TemporaryWriteFailed,
                                         WindowsError());
        }
        offset += static_cast<std::size_t>(written);
#else
        const std::size_t chunkSize = std::min<std::size_t>(bytes.size() - offset, 1024U * 1024U);
        const ssize_t written = write(temporary.descriptor, bytes.data() + offset, chunkSize);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return FailAndCleanTemporary(temporary,
                                         SceneStateIOError::TemporaryWriteFailed,
                                         PosixError());
        }
        offset += static_cast<std::size_t>(written);
#endif
    }
    return {};
}

[[nodiscard]] SceneStateIOResult SynchronizeTemporary(OwnedTemporaryFile& temporary,
                                                      const fs::path& targetPath) {
    DestinationInfo destination{};
    SceneStateIOResult inspected = InspectDestination(targetPath, destination);
    if (!inspected.Succeeded()) {
        return FailAndCleanTemporary(temporary, inspected.error, inspected.systemError);
    }

#if !defined(_WIN32)
    if (destination.exists && fchmod(temporary.descriptor, destination.mode & 07777) != 0) {
        return FailAndCleanTemporary(temporary,
                                     SceneStateIOError::DestinationMetadataFailed,
                                     PosixError());
    }
#endif

#if defined(_WIN32)
    if (FlushFileBuffers(temporary.handle) == FALSE) {
        return FailAndCleanTemporary(temporary,
                                     SceneStateIOError::TemporaryDataSyncFailed,
                                     WindowsError());
    }
#else
    while (fsync(temporary.descriptor) != 0) {
        if (errno == EINTR) {
            continue;
        }
        return FailAndCleanTemporary(temporary,
                                     SceneStateIOError::TemporaryDataSyncFailed,
                                     PosixError());
    }
#endif

    std::error_code closeError;
    if (!CloseTemporary(temporary, closeError)) {
        SceneStateIOResult result = FailAndCleanTemporary(
            temporary,
            SceneStateIOError::TemporaryCloseFailed,
            closeError);
        result.temporaryCloseFailed = true;
        result.temporaryCloseError = closeError;
        return result;
    }

    SceneStateIOResult result{};
    result.fileDataSynchronized = true;
    return result;
}

enum class ParentSyncOutcome : std::uint8_t {
    Synchronized,
    Unsupported,
    Failed,
};

#if !defined(_WIN32)
[[nodiscard]] bool DirectorySyncUnsupported(const int error) noexcept {
    if (error == EINVAL || error == ENOSYS) {
        return true;
    }
#if defined(ENOTSUP)
    if (error == ENOTSUP) {
        return true;
    }
#endif
#if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || EOPNOTSUPP != ENOTSUP)
    if (error == EOPNOTSUPP) {
        return true;
    }
#endif
    return false;
}

[[nodiscard]] ParentSyncOutcome SynchronizeParentDirectory(const fs::path& targetPath,
                                                           std::error_code& error) {
    const fs::path parentPath = targetPath.parent_path().empty() ? fs::path(".") : targetPath.parent_path();
    int flags = O_RDONLY;
#if defined(O_CLOEXEC)
    flags |= O_CLOEXEC;
#endif
#if defined(O_DIRECTORY)
    flags |= O_DIRECTORY;
#endif
    const int descriptor = open(parentPath.c_str(), flags);
    if (descriptor < 0) {
        const int value = errno;
        if (DirectorySyncUnsupported(value)) {
            return ParentSyncOutcome::Unsupported;
        }
        error = PosixError(value);
        return ParentSyncOutcome::Failed;
    }

    int syncResult = 0;
    do {
        syncResult = fsync(descriptor);
    } while (syncResult != 0 && errno == EINTR);
    const int syncError = syncResult == 0 ? 0 : errno;
    const int closeResult = close(descriptor);
    const int closeError = closeResult == 0 ? 0 : errno;
    if (syncResult != 0) {
        if (DirectorySyncUnsupported(syncError)) {
            return ParentSyncOutcome::Unsupported;
        }
        error = PosixError(syncError);
        return ParentSyncOutcome::Failed;
    }
    if (closeResult != 0) {
        error = PosixError(closeError);
        return ParentSyncOutcome::Failed;
    }
    return ParentSyncOutcome::Synchronized;
}
#endif

#if defined(_WIN32)
class WindowsReplaceOperations {
public:
    [[nodiscard]] detail::CommitOperationResult Replace(
        const fs::path& targetPath,
        const fs::path& replacementPath,
        const fs::path& backupPath) const noexcept {
        if (ReplaceFileW(targetPath.c_str(),
                         replacementPath.c_str(),
                         backupPath.c_str(),
                         0U,
                         nullptr,
                         nullptr) != FALSE) {
            return {true, 0U};
        }
        return {false, static_cast<std::uint32_t>(GetLastError())};
    }

    [[nodiscard]] detail::CommitPathState Inspect(
        const fs::path& path,
        std::uint32_t& error) const noexcept {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES) {
            return detail::CommitPathState::Present;
        }
        const DWORD value = GetLastError();
        if (value == ERROR_FILE_NOT_FOUND || value == ERROR_PATH_NOT_FOUND) {
            return detail::CommitPathState::Missing;
        }
        error = static_cast<std::uint32_t>(value);
        return detail::CommitPathState::InspectionFailed;
    }

    [[nodiscard]] detail::CommitOperationResult RestoreBackup(
        const fs::path& backupPath,
        const fs::path& targetPath) const noexcept {
        // No REPLACE_EXISTING: if another process creates the destination
        // during recovery, preserve both files for manual reconciliation.
        if (MoveFileExW(backupPath.c_str(), targetPath.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE) {
            return {true, 0U};
        }
        return {false, static_cast<std::uint32_t>(GetLastError())};
    }

    [[nodiscard]] detail::CommitOperationResult Remove(const fs::path& path) const noexcept {
        if (DeleteFileW(path.c_str()) != FALSE) {
            return {true, 0U};
        }
        const DWORD value = GetLastError();
        if (value == ERROR_FILE_NOT_FOUND || value == ERROR_PATH_NOT_FOUND) {
            return {true, 0U};
        }
        return {false, static_cast<std::uint32_t>(value)};
    }
};

[[nodiscard]] SceneStateIORecoveryState PublicRecoveryState(
    const detail::ReplaceRecoveryState state) noexcept {
    switch (state) {
    case detail::ReplaceRecoveryState::None:
        return SceneStateIORecoveryState::None;
    case detail::ReplaceRecoveryState::DestinationPreserved:
        return SceneStateIORecoveryState::DestinationPreserved;
    case detail::ReplaceRecoveryState::DestinationRestored:
        return SceneStateIORecoveryState::DestinationRestored;
    case detail::ReplaceRecoveryState::ManualRecoveryRequired:
        return SceneStateIORecoveryState::ManualRecoveryRequired;
    }
    return SceneStateIORecoveryState::ManualRecoveryRequired;
}

[[nodiscard]] bool SelectUnusedBackupPath(const fs::path& targetPath,
                                          OwnedTemporaryFile& temporary,
                                          SceneStateIOResult& failure) {
    WindowsReplaceOperations operations{};
    for (std::size_t attempt = 0U; attempt < kTemporaryCollisionRetries; ++attempt) {
        std::uint32_t inspectionError = 0U;
        const detail::CommitPathState state = operations.Inspect(
            temporary.backupPath,
            inspectionError);
        if (state == detail::CommitPathState::Missing) {
            return true;
        }
        if (state == detail::CommitPathState::InspectionFailed) {
            failure = Failure(
                SceneStateIOError::DestinationInspectionFailed,
                WindowsError(static_cast<DWORD>(inspectionError)));
            return false;
        }
        // A stale backup never authorizes deletion. Select a new independent
        // sibling token and leave the foreign file untouched.
        temporary.backupPath = std::move(NextTemporaryPaths(targetPath).backup);
    }
    failure = Failure(
        SceneStateIOError::TemporaryFileCollisionLimit,
        WindowsError(ERROR_ALREADY_EXISTS));
    return false;
}
#endif

[[nodiscard]] SceneStateIOResult CommitTemporary(OwnedTemporaryFile& temporary,
                                                 const fs::path& targetPath,
                                                 const bool fileDataSynchronized) {
#if defined(_WIN32)
    // ReplaceFile uses internal metadata-merge temporaries and is not reliable
    // under overlapping calls or aliased spellings of the same target. Keep
    // Win32 commits process-wide serial; cross-process contention can still
    // make one save fail, but never shares or truncates Raw Iron's temp file.
    static std::mutex commitMutex{};
    std::lock_guard commitLock(commitMutex);
#endif
    DestinationInfo destination{};
    SceneStateIOResult inspected = InspectDestination(targetPath, destination);
    if (!inspected.Succeeded()) {
        return FailAndCleanTemporary(temporary, inspected.error, inspected.systemError);
    }

#if defined(_WIN32)
    SceneStateIOResult result{};
    result.fileDataSynchronized = fileDataSynchronized;
    // Win32 exposes write-through rename/replace but no generally supported
    // equivalent of fsync on an opened directory handle.
    result.parentDirectorySyncUnsupported = true;

    if (destination.exists) {
        WindowsReplaceOperations operations{};
        SceneStateIOResult backupFailure{};
        if (!SelectUnusedBackupPath(targetPath, temporary, backupFailure)) {
            SceneStateIOResult failure = FailAndCleanTemporary(
                temporary,
                backupFailure.error,
                backupFailure.systemError);
            failure.fileDataSynchronized = fileDataSynchronized;
            failure.parentDirectorySyncUnsupported = true;
            return failure;
        }

        const detail::ReplaceTransitionResult transition =
            detail::ReplaceExistingWithRecovery(
                operations,
                targetPath,
                temporary.path,
                temporary.backupPath);
        result.committed = transition.committed;
        result.recoveryState = PublicRecoveryState(transition.recoveryState);

        if (transition.committed) {
            // ReplaceFile consumed the replacement path. A failed backup delete
            // is reported with committed=true and the exact preserved path.
            temporary.ownsPath = false;
            temporary.path.clear();
            if (transition.backupCleanupError != 0U) {
                result.error = SceneStateIOError::BackupCleanupFailed;
                result.systemError = WindowsError(
                    static_cast<DWORD>(transition.backupCleanupError));
                result.backupCleanupFailed = true;
                result.backupCleanupError = result.systemError;
                result.retainedBackupPath = std::move(temporary.backupPath);
            } else {
                temporary.backupPath.clear();
            }
            return result;
        }

        result.error = SceneStateIOError::AtomicReplaceFailed;
        result.systemError = WindowsError(static_cast<DWORD>(transition.replaceError));
        if (transition.inspectionError != 0U) {
            result.recoverySystemError = WindowsError(
                static_cast<DWORD>(transition.inspectionError));
        } else if (transition.recoveryError != 0U) {
            result.recoverySystemError = WindowsError(
                static_cast<DWORD>(transition.recoveryError));
        }
        if (transition.replacementCleanupError != 0U) {
            result.temporaryCleanupFailed = true;
            result.temporaryCleanupError = WindowsError(
                static_cast<DWORD>(transition.replacementCleanupError));
        }
        if (transition.retainReplacement) {
            // This is intentional retention, not an ownership leak. The path
            // is transferred into diagnostics before the RAII guard unwinds.
            temporary.ownsPath = false;
            result.retainedReplacementPath = std::move(temporary.path);
        } else {
            temporary.ownsPath = false;
            temporary.path.clear();
        }
        if (transition.retainBackup) {
            result.retainedBackupPath = std::move(temporary.backupPath);
        } else {
            temporary.backupPath.clear();
        }
        return result;
    }

    // The destination was inspected as absent. Do not replace a file created
    // by another process in the meantime; preserve that writer's snapshot.
    if (MoveFileExW(temporary.path.c_str(),
                    targetPath.c_str(),
                    MOVEFILE_WRITE_THROUGH) == FALSE) {
        SceneStateIOResult failure = FailAndCleanTemporary(
            temporary,
            SceneStateIOError::AtomicReplaceFailed,
            WindowsError());
        failure.fileDataSynchronized = fileDataSynchronized;
        failure.parentDirectorySyncUnsupported = true;
        return failure;
    }
    temporary.Disarm();
    result.committed = true;
    return result;
#else
    if (rename(temporary.path.c_str(), targetPath.c_str()) != 0) {
        return FailAndCleanTemporary(temporary,
                                     SceneStateIOError::AtomicReplaceFailed,
                                     PosixError());
    }
#endif

    temporary.Disarm();
    SceneStateIOResult committedResult{};
    committedResult.committed = true;
    committedResult.fileDataSynchronized = fileDataSynchronized;
#if !defined(_WIN32)
    std::error_code parentError;
    switch (SynchronizeParentDirectory(targetPath, parentError)) {
    case ParentSyncOutcome::Synchronized:
        committedResult.parentDirectorySynchronized = true;
        break;
    case ParentSyncOutcome::Unsupported:
        committedResult.parentDirectorySyncUnsupported = true;
        break;
    case ParentSyncOutcome::Failed:
        committedResult.error = SceneStateIOError::ParentDirectorySyncFailed;
        committedResult.systemError = parentError;
        break;
    }
#endif
    return committedResult;
}

[[nodiscard]] SceneStateIOResult ReadBoundedFile(const fs::path& inputPath, std::string& bytes) {
    DestinationInfo destination{};
    SceneStateIOResult inspected = InspectDestination(inputPath, destination);
    if (!inspected.Succeeded()) {
        // Load rejects the same symlink/reparse destinations that save already rejects.
        return inspected;
    }
    if (!destination.exists) {
        return Failure(SceneStateIOError::InputInspectionFailed,
                       std::make_error_code(std::errc::no_such_file_or_directory));
    }

#if defined(_WIN32)
    // Open the path itself (not a reparse target) so a symlink swap after the
    // attribute probe cannot redirect the subsequent read.
    const HANDLE fileHandle = CreateFileW(inputPath.c_str(),
                                          GENERIC_READ,
                                          FILE_SHARE_READ,
                                          nullptr,
                                          OPEN_EXISTING,
                                          FILE_FLAG_OPEN_REPARSE_POINT,
                                          nullptr);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        return Failure(SceneStateIOError::InputReadFailed, WindowsError(GetLastError()));
    }
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(fileHandle, &info)
        || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U
        || (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
        CloseHandle(fileHandle);
        return Failure(SceneStateIOError::DestinationSymlinkUnsupported);
    }
    const std::uintmax_t fileSize =
        (static_cast<std::uintmax_t>(info.nFileSizeHigh) << 32U) | info.nFileSizeLow;
    if (fileSize > kMaxSceneStateFileBytes) {
        CloseHandle(fileHandle);
        return Failure(SceneStateIOError::InputTooLarge);
    }
    try {
        bytes.assign(static_cast<std::size_t>(fileSize), '\0');
    } catch (const std::bad_alloc&) {
        CloseHandle(fileHandle);
        return Failure(SceneStateIOError::InputReadFailed,
                       std::make_error_code(std::errc::not_enough_memory));
    }
    if (fileSize > 0U) {
        DWORD bytesRead = 0U;
        if (!ReadFile(fileHandle,
                      bytes.data(),
                      static_cast<DWORD>(bytes.size()),
                      &bytesRead,
                      nullptr)
            || bytesRead != bytes.size()) {
            CloseHandle(fileHandle);
            return Failure(SceneStateIOError::InputReadFailed);
        }
    }
    // Detect concurrent growth after the size probe (parity with POSIX peek).
    char extraByte = '\0';
    DWORD peeked = 0U;
    if (ReadFile(fileHandle, &extraByte, 1U, &peeked, nullptr) && peeked > 0U) {
        CloseHandle(fileHandle);
        return Failure(SceneStateIOError::InputTooLarge);
    }
    CloseHandle(fileHandle);
    return {};
#else
    // Bind with O_NOFOLLOW so a symlink swap after InspectDestination cannot
    // redirect the subsequent read to an attacker-controlled target.
    int flags = O_RDONLY;
#if defined(O_CLOEXEC)
    flags |= O_CLOEXEC;
#endif
#if defined(O_NOFOLLOW)
    flags |= O_NOFOLLOW;
#endif
    const int fd = ::open(inputPath.c_str(), flags);
    if (fd < 0) {
        if (errno == ELOOP) {
            return Failure(SceneStateIOError::DestinationSymlinkUnsupported, PosixError());
        }
        return Failure(SceneStateIOError::InputReadFailed, PosixError());
    }
    struct stat status {};
    if (::fstat(fd, &status) != 0) {
        const int error = errno;
        ::close(fd);
        return Failure(SceneStateIOError::InputInspectionFailed, PosixError(error));
    }
    if (!S_ISREG(status.st_mode)) {
        ::close(fd);
        return Failure(SceneStateIOError::DestinationSymlinkUnsupported);
    }
    if (static_cast<std::uintmax_t>(status.st_size) > kMaxSceneStateFileBytes) {
        ::close(fd);
        return Failure(SceneStateIOError::InputTooLarge);
    }
    const std::uintmax_t fileSize = static_cast<std::uintmax_t>(status.st_size);
    try {
        bytes.assign(static_cast<std::size_t>(fileSize), '\0');
    } catch (const std::bad_alloc&) {
        ::close(fd);
        return Failure(SceneStateIOError::InputReadFailed,
                       std::make_error_code(std::errc::not_enough_memory));
    }
    std::size_t totalRead = 0U;
    while (totalRead < bytes.size()) {
        const ssize_t chunk = ::read(fd, bytes.data() + totalRead, bytes.size() - totalRead);
        if (chunk < 0) {
            if (errno == EINTR) {
                continue;
            }
            const int error = errno;
            ::close(fd);
            return Failure(SceneStateIOError::InputReadFailed, PosixError(error));
        }
        if (chunk == 0) {
            ::close(fd);
            return Failure(SceneStateIOError::InputReadFailed);
        }
        totalRead += static_cast<std::size_t>(chunk);
    }
    char extraByte = '\0';
    for (;;) {
        const ssize_t peek = ::read(fd, &extraByte, 1);
        if (peek < 0) {
            if (errno == EINTR) {
                continue;
            }
            const int error = errno;
            ::close(fd);
            return Failure(SceneStateIOError::InputReadFailed, PosixError(error));
        }
        if (peek > 0) {
            ::close(fd);
            return Failure(SceneStateIOError::InputTooLarge);
        }
        break;
    }
    ::close(fd);
    return {};
#endif
}

void RemoveTrailingCarriageReturn(std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

enum class NumberParseStatus : std::uint8_t {
    Valid,
    Malformed,
    OutOfRange,
    NonFinite,
};

[[nodiscard]] NumberParseStatus ReadFloat(std::istream& stream, float& value) {
    std::string token;
    if (!(stream >> token) || token.empty()) {
        return NumberParseStatus::Malformed;
    }
    const char* begin = token.data();
    const char* end = begin + token.size();
    if (*begin == '+') {
        ++begin;
        if (begin == end) {
            return NumberParseStatus::Malformed;
        }
    }
    const auto parsed = std::from_chars(begin, end, value, std::chars_format::general);
    if (parsed.ec == std::errc::result_out_of_range) {
        return NumberParseStatus::OutOfRange;
    }
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
        return NumberParseStatus::Malformed;
    }
    return std::isfinite(value) ? NumberParseStatus::Valid : NumberParseStatus::NonFinite;
}

[[nodiscard]] SceneStateIOError ReadTransform(std::istream& stream, Transform& transform) {
    float* components[] = {
        &transform.position.x,
        &transform.position.y,
        &transform.position.z,
        &transform.rotationDegrees.x,
        &transform.rotationDegrees.y,
        &transform.rotationDegrees.z,
        &transform.scale.x,
        &transform.scale.y,
        &transform.scale.z,
    };
    for (float* component : components) {
        switch (ReadFloat(stream, *component)) {
        case NumberParseStatus::Valid:
            break;
        case NumberParseStatus::Malformed:
            return SceneStateIOError::MalformedNodeRecord;
        case NumberParseStatus::OutOfRange:
            return SceneStateIOError::NumericOutOfRange;
        case NumberParseStatus::NonFinite:
            return SceneStateIOError::NonFiniteTransform;
        }
    }
    return SceneStateIOError::None;
}

} // namespace

SceneStateIOResult SaveSceneNodeTransformsDetailed(const Scene& scene, const fs::path& outputPath) {
    if (outputPath.empty() || outputPath.filename().empty()) {
        return Failure(SceneStateIOError::InvalidPath);
    }

    std::unique_ptr<char[]> serialized;
    std::size_t serializedSize = 0U;
    SceneStateIOResult serialization = SerializeBounded(scene, serialized, serializedSize);
    if (!serialization.Succeeded()) {
        return serialization;
    }

    const fs::path parentPath = outputPath.parent_path();
    if (!parentPath.empty()) {
        std::error_code createError;
        fs::create_directories(parentPath, createError);
        if (createError) {
            return Failure(SceneStateIOError::DirectoryCreationFailed, createError);
        }
    }

    DestinationInfo initialDestination{};
    SceneStateIOResult inspected = InspectDestination(outputPath, initialDestination);
    if (!inspected.Succeeded()) {
        return inspected;
    }

    OwnedTemporaryFile temporary{};
    SceneStateIOResult openFailure{};
    if (!OpenExclusiveTemporary(outputPath, temporary, openFailure)) {
        return openFailure;
    }

    SceneStateIOResult written = WriteTemporary(
        temporary,
        std::string_view(serialized.get(), serializedSize));
    if (!written.Succeeded()) {
        return written;
    }
    SceneStateIOResult synchronized = SynchronizeTemporary(temporary, outputPath);
    if (!synchronized.Succeeded()) {
        return synchronized;
    }
    return CommitTemporary(temporary, outputPath, synchronized.fileDataSynchronized);
}

SceneStateIOResult LoadSceneNodeTransformsDetailed(Scene& scene, const fs::path& inputPath) {
    if (inputPath.empty()) {
        return Failure(SceneStateIOError::InvalidPath);
    }
    std::string bytes;
    SceneStateIOResult read = ReadBoundedFile(inputPath, bytes);
    if (!read.Succeeded()) {
        return read;
    }

    std::istringstream stream;
    try {
        stream.str(std::move(bytes));
    } catch (const std::bad_alloc&) {
        return Failure(SceneStateIOError::InputReadFailed,
                       std::make_error_code(std::errc::not_enough_memory));
    }
    stream.imbue(std::locale::classic());

    std::string magic;
    if (!std::getline(stream, magic)) {
        return Failure(SceneStateIOError::UnsupportedMagic);
    }
    RemoveTrailingCarriageReturn(magic);
    if (magic != "RAWIRON_SCENE_STATE_V1" && magic != "RAWIRON_EDITOR_STATE_V1") {
        return Failure(SceneStateIOError::UnsupportedMagic);
    }

    std::string countLine;
    if (!std::getline(stream, countLine)) {
        return Failure(SceneStateIOError::MalformedNodeCount);
    }
    RemoveTrailingCarriageReturn(countLine);
    std::istringstream countStream(countLine);
    countStream.imbue(std::locale::classic());
    std::string header;
    std::size_t nodeCount = 0U;
    if (!(countStream >> header >> nodeCount) || header != "node_count") {
        return Failure(SceneStateIOError::MalformedNodeCount);
    }
    countStream >> std::ws;
    if (!countStream.eof()) {
        return Failure(SceneStateIOError::MalformedNodeCount);
    }
    if (nodeCount > kMaxSceneStateNodes) {
        return Failure(SceneStateIOError::NodeCountLimitExceeded);
    }
    if (nodeCount > scene.NodeCount()) {
        return Failure(SceneStateIOError::TargetSceneTooSmall);
    }

    std::vector<PendingNodeTransform> pendingTransforms;
    std::vector<std::uint8_t> seenIndices;
    try {
        pendingTransforms.reserve(nodeCount);
        seenIndices.assign(nodeCount, 0U);
    } catch (const std::bad_alloc&) {
        return Failure(SceneStateIOError::InputReadFailed,
                       std::make_error_code(std::errc::not_enough_memory));
    }

    for (std::size_t record = 0U; record < nodeCount; ++record) {
        std::string token;
        std::size_t index = 0U;
        if (!(stream >> token >> index)) {
            return Failure(SceneStateIOError::InconsistentNodeCount);
        }
        if (token != "node") {
            return Failure(SceneStateIOError::MalformedNodeRecord);
        }
        if (index >= nodeCount || index >= scene.NodeCount()) {
            return Failure(SceneStateIOError::NodeIndexOutOfRange);
        }
        if (seenIndices[index] != 0U) {
            return Failure(SceneStateIOError::DuplicateNodeIndex);
        }

        stream >> std::ws;
        if (stream.peek() != '"') {
            return Failure(SceneStateIOError::MalformedNodeRecord);
        }
        std::string name;
        if (!(stream >> std::quoted(name))) {
            return Failure(SceneStateIOError::MalformedNodeRecord);
        }
        Transform transform{};
        const SceneStateIOError transformError = ReadTransform(stream, transform);
        if (transformError != SceneStateIOError::None) {
            return Failure(transformError);
        }
        if (name != scene.Nodes()[index].name) {
            return Failure(SceneStateIOError::NodeNameMismatch);
        }

        seenIndices[index] = 1U;
        pendingTransforms.push_back(PendingNodeTransform{index, transform});
    }

    stream >> std::ws;
    if (stream.bad()) {
        return Failure(SceneStateIOError::InputReadFailed);
    }
    if (!stream.eof()) {
        std::string trailingToken;
        stream >> trailingToken;
        return Failure(trailingToken == "node"
            ? SceneStateIOError::InconsistentNodeCount
            : SceneStateIOError::TrailingData);
    }

    for (const PendingNodeTransform& pending : pendingTransforms) {
        scene.GetNode(static_cast<int>(pending.index)).localTransform = pending.transform;
    }
    SceneStateIOResult result{};
    result.committed = true;
    return result;
}

bool SaveSceneNodeTransforms(const Scene& scene, const fs::path& outputPath) {
    // Durability warnings (backup cleanup / parent-dir sync) still leave a committed file.
    // Treat those as success so callers don't retry-overwrite a good publish.
    return SaveSceneNodeTransformsDetailed(scene, outputPath).committed;
}

bool LoadSceneNodeTransforms(Scene& scene, const fs::path& inputPath) {
    return LoadSceneNodeTransformsDetailed(scene, inputPath).Succeeded();
}

} // namespace ri::scene
