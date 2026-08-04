#include "SecureRipakArchive.h"

#include "RawIron/Content/AssetPackageManifest.h"

#include "stb/stb_image.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <process.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ri::tooling {
namespace {

namespace fs = std::filesystem;

constexpr std::uint32_t kLocalHeaderSignature = 0x04034b50U;
constexpr std::uint32_t kCentralHeaderSignature = 0x02014b50U;
constexpr std::uint32_t kDataDescriptorSignature = 0x08074b50U;
constexpr std::uint32_t kEndSignature = 0x06054b50U;
constexpr std::size_t kEndRecordBytes = 22U;
constexpr std::size_t kMaximumEndSearchBytes = 65557U;
constexpr std::uint16_t kFlagEncrypted = 0x0001U;
constexpr std::uint16_t kFlagDataDescriptor = 0x0008U;
constexpr std::uint16_t kFlagStrongEncryption = 0x0040U;
constexpr std::uint16_t kFlagUtf8 = 0x0800U;
constexpr std::uint16_t kFlagMaskedHeader = 0x2000U;
constexpr std::uint16_t kAllowedFlags = 0x000EU | kFlagUtf8;
constexpr std::uint16_t kMethodStored = 0U;
constexpr std::uint16_t kMethodDeflate = 8U;

[[nodiscard]] std::uint16_t ReadU16(const std::byte* data) noexcept {
    return static_cast<std::uint16_t>(std::to_integer<unsigned char>(data[0]))
        | (static_cast<std::uint16_t>(std::to_integer<unsigned char>(data[1])) << 8U);
}

[[nodiscard]] std::uint32_t ReadU32(const std::byte* data) noexcept {
    return static_cast<std::uint32_t>(ReadU16(data))
        | (static_cast<std::uint32_t>(ReadU16(data + 2U)) << 16U);
}

[[nodiscard]] bool CheckedAdd(
    const std::uint64_t left,
    const std::uint64_t right,
    std::uint64_t& result) noexcept {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return false;
    }
    result = left + right;
    return true;
}

[[nodiscard]] bool IsValidUtf8(const std::string_view value) noexcept {
    std::size_t index = 0U;
    while (index < value.size()) {
        const auto first = static_cast<unsigned char>(value[index]);
        std::size_t continuationCount = 0U;
        std::uint32_t codePoint = 0U;
        if (first <= 0x7FU) {
            ++index;
            continue;
        }
        if (first >= 0xC2U && first <= 0xDFU) {
            continuationCount = 1U;
            codePoint = first & 0x1FU;
        } else if (first >= 0xE0U && first <= 0xEFU) {
            continuationCount = 2U;
            codePoint = first & 0x0FU;
        } else if (first >= 0xF0U && first <= 0xF4U) {
            continuationCount = 3U;
            codePoint = first & 0x07U;
        } else {
            return false;
        }
        if (index + continuationCount >= value.size()) {
            return false;
        }
        for (std::size_t offset = 1U; offset <= continuationCount; ++offset) {
            const auto continuation = static_cast<unsigned char>(value[index + offset]);
            if ((continuation & 0xC0U) != 0x80U) {
                return false;
            }
            codePoint = (codePoint << 6U) | (continuation & 0x3FU);
        }
        if ((continuationCount == 2U && codePoint < 0x800U)
            || (continuationCount == 3U && codePoint < 0x10000U)
            || codePoint > 0x10FFFFU
            || (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
            return false;
        }
        index += continuationCount + 1U;
    }
    return true;
}

[[nodiscard]] std::string EntryLabel(const std::string_view name) {
    constexpr std::size_t maximumLabelBytes = 160U;
    std::string label;
    label.reserve(std::min(name.size(), maximumLabelBytes));
    for (const unsigned char byte : name.substr(0U, maximumLabelBytes)) {
        if (byte >= 0x20U && byte != 0x7FU) {
            label.push_back(static_cast<char>(byte));
        } else {
            label.push_back('?');
        }
    }
    if (name.size() > maximumLabelBytes) {
        label += "...";
    }
    return label;
}

[[noreturn]] void Reject(const std::string& reason) {
    throw std::runtime_error("Unsafe .ripak archive: " + reason);
}

void ReadExactly(
    std::ifstream& input,
    const std::uint64_t offset,
    std::byte* destination,
    const std::size_t bytes,
    const std::string_view context) {
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())
        || bytes > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        Reject(std::string(context) + " exceeds host stream limits");
    }
    input.clear();
    input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!input) {
        Reject("could not seek to " + std::string(context));
    }
    if (bytes != 0U) {
        input.read(reinterpret_cast<char*>(destination), static_cast<std::streamsize>(bytes));
        if (input.gcount() != static_cast<std::streamsize>(bytes)) {
            Reject("archive ended while reading " + std::string(context));
        }
    }
}

[[nodiscard]] std::vector<std::byte> ReadBytes(
    std::ifstream& input,
    const std::uint64_t offset,
    const std::size_t bytes,
    const std::string_view context) {
    std::vector<std::byte> result(bytes);
    ReadExactly(input, offset, result.data(), result.size(), context);
    return result;
}

void ValidateExtraFields(const std::vector<std::byte>& fields, const std::string_view name) {
    std::size_t cursor = 0U;
    while (cursor < fields.size()) {
        if (fields.size() - cursor < 4U) {
            Reject("entry '" + EntryLabel(name) + "' has a truncated extra field");
        }
        const std::uint16_t id = ReadU16(fields.data() + cursor);
        const std::uint16_t length = ReadU16(fields.data() + cursor + 2U);
        cursor += 4U;
        if (length > fields.size() - cursor) {
            Reject("entry '" + EntryLabel(name) + "' has an invalid extra-field length");
        }
        if (id == 0x0001U) {
            Reject("ZIP64 entries are unsupported");
        }
        if (id == 0x0017U || id == 0x9901U) {
            Reject("encrypted ZIP entry metadata is unsupported");
        }
        cursor += length;
    }
}

[[nodiscard]] bool ExceedsRatio(
    const std::uint64_t expanded,
    const std::uint64_t compressed,
    const std::uint64_t maximumRatio) noexcept {
    if (expanded == 0U) {
        return false;
    }
    if (compressed == 0U || maximumRatio == 0U) {
        return true;
    }
    return expanded / compressed > maximumRatio
        || (expanded / compressed == maximumRatio && expanded % compressed != 0U);
}

struct ZipEntry {
    std::string name{};
    fs::path destination{};
    bool directory = false;
    std::uint16_t flags = 0U;
    std::uint16_t method = 0U;
    std::uint32_t crc32 = 0U;
    std::uint32_t compressedBytes = 0U;
    std::uint32_t expandedBytes = 0U;
    std::uint32_t localHeaderOffset = 0U;
    std::uint64_t dataOffset = 0U;
    std::uint64_t occupiedEnd = 0U;
};

[[nodiscard]] std::uint32_t UpdateCrc32(
    std::uint32_t crc,
    const std::byte* data,
    const std::size_t size) noexcept {
    crc = ~crc;
    for (std::size_t index = 0U; index < size; ++index) {
        crc ^= std::to_integer<unsigned char>(data[index]);
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^ ((crc & 1U) != 0U ? 0xEDB88320U : 0U);
        }
    }
    return ~crc;
}

[[nodiscard]] std::uint64_t ProcessId() noexcept {
#if defined(_WIN32)
    return static_cast<std::uint64_t>(_getpid());
#else
    return static_cast<std::uint64_t>(getpid());
#endif
}

[[nodiscard]] fs::path CreateExclusiveStagingRoot(const fs::path& archivePath) {
    std::error_code error;
    const fs::path temporaryRoot = fs::canonical(fs::temp_directory_path(), error);
    if (error || !fs::is_directory(temporaryRoot)) {
        throw std::runtime_error("Could not resolve the system temporary directory: " + error.message());
    }

    static std::atomic<std::uint64_t> counter{0U};
    std::random_device random;
    for (std::size_t attempt = 0U; attempt < 128U; ++attempt) {
        std::ostringstream suffix;
        suffix << "RawIronRipak.extract." << ProcessId() << '.'
               << std::hex << std::setw(8) << std::setfill('0') << random()
               << std::setw(8) << random() << '.'
               << counter.fetch_add(1U, std::memory_order_relaxed);
        const fs::path candidate = temporaryRoot / suffix.str();
        error.clear();
        if (fs::create_directory(candidate, error)) {
            const fs::path canonicalCandidate = fs::canonical(candidate, error);
            if (error || canonicalCandidate.parent_path() != temporaryRoot) {
                std::error_code cleanupError;
                fs::remove(candidate, cleanupError);
                throw std::runtime_error("Exclusive .ripak staging root failed containment validation.");
            }
            return canonicalCandidate;
        }
        if (error && error != std::errc::file_exists) {
            throw std::runtime_error(
                "Could not create an exclusive .ripak staging root for "
                + archivePath.string() + ": " + error.message());
        }
    }
    throw std::runtime_error("Could not allocate a unique .ripak staging root after 128 attempts.");
}

[[nodiscard]] bool IsLinkOrReparse(const fs::path& path) {
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return false;
        }
        throw std::system_error(static_cast<int>(error), std::system_category(),
                                "GetFileAttributesW failed for staging path");
    }
    return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
#else
    std::error_code error;
    const fs::file_status status = fs::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory || status.type() == fs::file_type::not_found) {
        return false;
    }
    if (error) {
        throw std::system_error(error, "symlink_status failed for staging path");
    }
    return fs::is_symlink(status);
#endif
}

void RejectLinkComponents(const fs::path& root, const fs::path& destination) {
    fs::path cursor = root;
    const fs::path relative = destination.lexically_relative(root);
    for (const fs::path& component : relative) {
        cursor /= component;
        if (IsLinkOrReparse(cursor)) {
            Reject("staging path acquired a symlink or reparse component");
        }
    }
}

class ExclusiveOutput final {
public:
    explicit ExclusiveOutput(const fs::path& path) : path_(path) {
#if defined(_WIN32)
        handle_ = CreateFileW(
            path.c_str(), GENERIC_WRITE, 0U, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) {
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                    "exclusive archive output creation failed");
        }
#else
        descriptor_ = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
        if (descriptor_ < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "exclusive archive output creation failed");
        }
#endif
    }

    ~ExclusiveOutput() noexcept {
        Close();
        if (!committed_) {
            std::error_code ignored;
            fs::remove(path_, ignored);
        }
    }

    ExclusiveOutput(const ExclusiveOutput&) = delete;
    ExclusiveOutput& operator=(const ExclusiveOutput&) = delete;

    void Write(const std::byte* data, const std::size_t size) {
        std::size_t cursor = 0U;
        while (cursor < size) {
            const std::size_t remaining = size - cursor;
#if defined(_WIN32)
            const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(remaining, 64U * 1024U));
            DWORD written = 0U;
            if (WriteFile(handle_, data + cursor, chunk, &written, nullptr) == FALSE || written == 0U) {
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                        "archive output write failed");
            }
            cursor += written;
#else
            const std::size_t chunk = std::min<std::size_t>(remaining, 64U * 1024U);
            const ssize_t written = write(descriptor_, data + cursor, chunk);
            if (written < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw std::system_error(errno, std::generic_category(), "archive output write failed");
            }
            if (written == 0) {
                throw std::runtime_error("archive output write made no progress");
            }
            cursor += static_cast<std::size_t>(written);
#endif
        }
    }

    void Commit() {
        CloseChecked();
        committed_ = true;
    }

private:
    void CloseChecked() {
#if defined(_WIN32)
        if (handle_ != INVALID_HANDLE_VALUE) {
            if (CloseHandle(std::exchange(handle_, INVALID_HANDLE_VALUE)) == FALSE) {
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                        "archive output close failed");
            }
        }
#else
        if (descriptor_ >= 0) {
            const int descriptor = std::exchange(descriptor_, -1);
            if (close(descriptor) != 0) {
                throw std::system_error(errno, std::generic_category(), "archive output close failed");
            }
        }
#endif
    }

    void Close() noexcept {
#if defined(_WIN32)
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(std::exchange(handle_, INVALID_HANDLE_VALUE));
        }
#else
        if (descriptor_ >= 0) {
            close(std::exchange(descriptor_, -1));
        }
#endif
    }

    fs::path path_{};
    bool committed_ = false;
#if defined(_WIN32)
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    int descriptor_ = -1;
#endif
};

[[nodiscard]] std::vector<ZipEntry> InspectArchive(
    std::ifstream& input,
    const std::uint64_t archiveBytes,
    const fs::path& stagingRoot,
    const SecureRipakLimits& limits) {
    if (archiveBytes < kEndRecordBytes) {
        Reject("file is too small to contain a ZIP end record");
    }
    const std::size_t tailBytes = static_cast<std::size_t>(
        std::min<std::uint64_t>(archiveBytes, kMaximumEndSearchBytes));
    const std::uint64_t tailOffset = archiveBytes - tailBytes;
    const std::vector<std::byte> tail = ReadBytes(input, tailOffset, tailBytes, "ZIP end record");

    std::optional<std::size_t> endIndex;
    for (std::size_t index = tail.size() - kEndRecordBytes + 1U; index-- > 0U;) {
        if (ReadU32(tail.data() + index) != kEndSignature) {
            continue;
        }
        const std::uint16_t commentBytes = ReadU16(tail.data() + index + 20U);
        if (index + kEndRecordBytes + commentBytes == tail.size()) {
            endIndex = index;
            break;
        }
    }
    if (!endIndex.has_value()) {
        Reject("missing or ambiguous ZIP end record");
    }
    const std::byte* end = tail.data() + *endIndex;
    const std::uint16_t disk = ReadU16(end + 4U);
    const std::uint16_t centralDisk = ReadU16(end + 6U);
    const std::uint16_t entriesOnDisk = ReadU16(end + 8U);
    const std::uint16_t entryCount = ReadU16(end + 10U);
    const std::uint32_t centralBytes = ReadU32(end + 12U);
    const std::uint32_t centralOffset = ReadU32(end + 16U);
    if (disk != 0U || centralDisk != 0U || entriesOnDisk != entryCount) {
        Reject("multi-disk ZIP archives are unsupported");
    }
    if (entryCount == 0xFFFFU || centralBytes == 0xFFFFFFFFU || centralOffset == 0xFFFFFFFFU) {
        Reject("ZIP64 archives are unsupported");
    }
    if (entryCount > limits.maximumEntries) {
        Reject("entry-count budget exceeded (limit " + std::to_string(limits.maximumEntries) + ")");
    }
    std::uint64_t centralEnd = 0U;
    if (!CheckedAdd(centralOffset, centralBytes, centralEnd)
        || centralEnd != tailOffset + *endIndex) {
        Reject("central directory bounds do not match the ZIP end record");
    }

    std::vector<ZipEntry> entries;
    entries.reserve(entryCount);
    std::set<fs::path, ri::content::PackageInstallDestinationLess> destinations;
    std::set<std::string, std::less<>> names;
    std::uint64_t cursor = centralOffset;
    std::uint64_t totalExpanded = 0U;
    for (std::size_t entryIndex = 0U; entryIndex < entryCount; ++entryIndex) {
        const std::vector<std::byte> fixed = ReadBytes(input, cursor, 46U, "central-directory entry");
        if (ReadU32(fixed.data()) != kCentralHeaderSignature) {
            Reject("central directory contains an invalid entry signature");
        }
        const std::uint16_t versionNeeded = ReadU16(fixed.data() + 6U);
        const std::uint16_t flags = ReadU16(fixed.data() + 8U);
        const std::uint16_t method = ReadU16(fixed.data() + 10U);
        const std::uint32_t crc32 = ReadU32(fixed.data() + 16U);
        const std::uint32_t compressedBytes = ReadU32(fixed.data() + 20U);
        const std::uint32_t expandedBytes = ReadU32(fixed.data() + 24U);
        const std::uint16_t nameBytes = ReadU16(fixed.data() + 28U);
        const std::uint16_t extraBytes = ReadU16(fixed.data() + 30U);
        const std::uint16_t commentBytes = ReadU16(fixed.data() + 32U);
        const std::uint16_t startDisk = ReadU16(fixed.data() + 34U);
        const std::uint32_t externalAttributes = ReadU32(fixed.data() + 38U);
        const std::uint32_t localOffset = ReadU32(fixed.data() + 42U);
        if (versionNeeded > 20U) {
            Reject("entry requires an unsupported ZIP feature version");
        }
        if ((flags & (kFlagEncrypted | kFlagStrongEncryption | kFlagMaskedHeader)) != 0U) {
            Reject("encrypted or masked ZIP entries are unsupported");
        }
        if ((flags & ~kAllowedFlags) != 0U) {
            Reject("entry uses unsupported general-purpose ZIP flags");
        }
        if (method != kMethodStored && method != kMethodDeflate) {
            Reject("entry uses unsupported compression method " + std::to_string(method));
        }
        if (method == kMethodStored && (flags & 0x0006U) != 0U) {
            Reject("stored entry uses deflate-only option flags");
        }
        if (startDisk != 0U || localOffset >= centralOffset) {
            Reject("entry references an invalid local header");
        }
        if (nameBytes == 0U) {
            Reject("entry has an empty name");
        }
        std::uint64_t variableEnd = 0U;
        if (!CheckedAdd(cursor + 46U, static_cast<std::uint64_t>(nameBytes) + extraBytes + commentBytes, variableEnd)
            || variableEnd > centralEnd) {
            Reject("central-directory entry exceeds declared bounds");
        }
        const std::vector<std::byte> variable = ReadBytes(
            input, cursor + 46U, static_cast<std::size_t>(nameBytes) + extraBytes + commentBytes,
            "central-directory entry fields");
        const std::string name(
            reinterpret_cast<const char*>(variable.data()), static_cast<std::size_t>(nameBytes));
        if ((flags & kFlagUtf8) != 0U) {
            if (!IsValidUtf8(name)) {
                Reject("entry name is not well-formed UTF-8");
            }
        } else if (std::any_of(name.begin(), name.end(), [](const unsigned char value) { return value >= 0x80U; })) {
            Reject("non-ASCII entry names must declare the ZIP UTF-8 flag");
        }
        ValidateExtraFields(
            std::vector<std::byte>(variable.begin() + nameBytes, variable.begin() + nameBytes + extraBytes), name);
        if (!names.insert(name).second) {
            Reject("duplicate entry name '" + EntryLabel(name) + "'");
        }

        const std::uint16_t unixMode = static_cast<std::uint16_t>(externalAttributes >> 16U);
        const std::uint16_t unixType = unixMode & 0170000U;
        const bool typedDirectory = unixType == 0040000U;
        const bool typedRegular = unixType == 0100000U || unixType == 0U;
        if (!typedDirectory && !typedRegular) {
            Reject("entry '" + EntryLabel(name) + "' is a link or unsupported external file type");
        }
        if ((externalAttributes & 0x00000400U) != 0U) {
            Reject("entry '" + EntryLabel(name) + "' carries a Windows reparse attribute");
        }
        const bool trailingSlash = name.back() == '/';
        const bool dosDirectory = (externalAttributes & 0x10U) != 0U;
        if ((typedDirectory || dosDirectory) && !trailingSlash) {
            Reject("directory entry '" + EntryLabel(name) + "' lacks a trailing slash");
        }
        const bool directory = trailingSlash;
        std::string relativeName = directory ? name.substr(0U, name.size() - 1U) : name;
        if (relativeName.empty()) {
            Reject("archive contains a root directory entry");
        }
        ri::content::PackageInstallPathResolution resolved{};
        try {
            resolved = ri::content::ResolvePackageInstallPath(stagingRoot, relativeName);
        } catch (const std::exception& error) {
            Reject("entry '" + EntryLabel(name) + "' path conversion failed: " + error.what());
        }
        if (!resolved.safe) {
            Reject("entry '" + EntryLabel(name) + "' has an unsafe path: " + resolved.issue);
        }
        if (!destinations.insert(resolved.destination).second) {
            Reject("entry '" + EntryLabel(name) + "' collides with another entry on this platform");
        }
        if (directory && (compressedBytes != 0U || expandedBytes != 0U)) {
            Reject("directory entry '" + EntryLabel(name) + "' contains payload bytes");
        }
        if (expandedBytes > limits.maximumFileBytes) {
            Reject("entry '" + EntryLabel(name) + "' exceeds the per-file expanded-byte budget (limit "
                   + std::to_string(limits.maximumFileBytes) + ")");
        }
        if (!CheckedAdd(totalExpanded, expandedBytes, totalExpanded)
            || totalExpanded > limits.maximumExpandedBytes) {
            Reject("total expanded-byte budget exceeded (limit "
                   + std::to_string(limits.maximumExpandedBytes) + ")");
        }
        if (!directory && ExceedsRatio(expandedBytes, compressedBytes, limits.maximumCompressionRatio)) {
            Reject("entry '" + EntryLabel(name) + "' exceeds the compression-ratio budget (limit "
                   + std::to_string(limits.maximumCompressionRatio) + ":1)");
        }
        entries.push_back({
            .name = name,
            .destination = resolved.destination,
            .directory = directory,
            .flags = flags,
            .method = method,
            .crc32 = crc32,
            .compressedBytes = compressedBytes,
            .expandedBytes = expandedBytes,
            .localHeaderOffset = localOffset,
        });
        cursor = variableEnd;
    }
    if (cursor != centralEnd) {
        Reject("central directory entry count does not consume the declared directory");
    }

    for (ZipEntry& entry : entries) {
        const std::vector<std::byte> local = ReadBytes(
            input, entry.localHeaderOffset, 30U, "local ZIP header");
        if (ReadU32(local.data()) != kLocalHeaderSignature) {
            Reject("entry '" + EntryLabel(entry.name) + "' has an invalid local header");
        }
        const std::uint16_t localFlags = ReadU16(local.data() + 6U);
        const std::uint16_t localMethod = ReadU16(local.data() + 8U);
        const std::uint32_t localCrc = ReadU32(local.data() + 14U);
        const std::uint32_t localCompressed = ReadU32(local.data() + 18U);
        const std::uint32_t localExpanded = ReadU32(local.data() + 22U);
        const std::uint16_t localNameBytes = ReadU16(local.data() + 26U);
        const std::uint16_t localExtraBytes = ReadU16(local.data() + 28U);
        std::uint64_t dataOffset = 0U;
        if (!CheckedAdd(entry.localHeaderOffset + 30U,
                        static_cast<std::uint64_t>(localNameBytes) + localExtraBytes, dataOffset)
            || dataOffset > centralOffset) {
            Reject("entry '" + EntryLabel(entry.name) + "' local fields exceed archive bounds");
        }
        const std::vector<std::byte> localVariable = ReadBytes(
            input, entry.localHeaderOffset + 30U,
            static_cast<std::size_t>(localNameBytes) + localExtraBytes,
            "local ZIP entry fields");
        const std::string localName(
            reinterpret_cast<const char*>(localVariable.data()), localNameBytes);
        if (localName != entry.name || localFlags != entry.flags || localMethod != entry.method) {
            Reject("entry '" + EntryLabel(entry.name) + "' central and local headers disagree");
        }
        ValidateExtraFields(
            std::vector<std::byte>(localVariable.begin() + localNameBytes, localVariable.end()), entry.name);
        if ((entry.flags & kFlagDataDescriptor) == 0U
            && (localCrc != entry.crc32 || localCompressed != entry.compressedBytes
                || localExpanded != entry.expandedBytes)) {
            Reject("entry '" + EntryLabel(entry.name) + "' size or CRC metadata disagrees");
        }
        std::uint64_t dataEnd = 0U;
        if (!CheckedAdd(dataOffset, entry.compressedBytes, dataEnd) || dataEnd > centralOffset) {
            Reject("entry '" + EntryLabel(entry.name) + "' compressed payload exceeds archive bounds");
        }
        entry.dataOffset = dataOffset;
        entry.occupiedEnd = dataEnd;
        if ((entry.flags & kFlagDataDescriptor) != 0U) {
            std::vector<std::byte> descriptor = ReadBytes(input, dataEnd, 16U, "ZIP data descriptor");
            std::size_t fieldOffset = 0U;
            if (ReadU32(descriptor.data()) == kDataDescriptorSignature) {
                fieldOffset = 4U;
            } else {
                descriptor.resize(12U);
            }
            if (ReadU32(descriptor.data() + fieldOffset) != entry.crc32
                || ReadU32(descriptor.data() + fieldOffset + 4U) != entry.compressedBytes
                || ReadU32(descriptor.data() + fieldOffset + 8U) != entry.expandedBytes) {
                Reject("entry '" + EntryLabel(entry.name) + "' has an invalid data descriptor");
            }
            entry.occupiedEnd += fieldOffset + 12U;
            if (entry.occupiedEnd > centralOffset) {
                Reject("entry data descriptor exceeds archive bounds");
            }
        }
    }

    std::vector<const ZipEntry*> occupied;
    occupied.reserve(entries.size());
    for (const ZipEntry& entry : entries) {
        occupied.push_back(&entry);
    }
    std::sort(occupied.begin(), occupied.end(), [](const ZipEntry* left, const ZipEntry* right) {
        return left->localHeaderOffset < right->localHeaderOffset;
    });
    for (std::size_t index = 1U; index < occupied.size(); ++index) {
        if (occupied[index]->localHeaderOffset < occupied[index - 1U]->occupiedEnd) {
            Reject("ZIP entries have overlapping local-header or payload regions");
        }
    }

    // A file entry may not also be the implicit parent of another entry.
    std::set<fs::path, ri::content::PackageInstallDestinationLess> fileDestinations;
    for (const ZipEntry& entry : entries) {
        if (!entry.directory) {
            fileDestinations.insert(entry.destination);
        }
    }
    for (const ZipEntry& entry : entries) {
        fs::path parent = entry.destination.parent_path();
        while (parent != stagingRoot && !parent.empty()) {
            if (fileDestinations.contains(parent)) {
                Reject("entry '" + EntryLabel(entry.name)
                       + " has a parent path occupied by a file entry");
            }
            parent = parent.parent_path();
        }
    }
    return entries;
}

void ExtractEntry(
    std::ifstream& input,
    const fs::path& stagingRoot,
    const ZipEntry& entry,
    const SecureRipakLimits& limits) {
    RejectLinkComponents(stagingRoot, entry.destination.parent_path());
    std::error_code error;
    fs::create_directories(entry.directory ? entry.destination : entry.destination.parent_path(), error);
    if (error) {
        throw std::runtime_error("Could not create .ripak staging directories: " + error.message());
    }
    RejectLinkComponents(stagingRoot, entry.destination);
    if (entry.directory) {
        std::error_code canonicalError;
        const fs::path canonicalDirectory = fs::canonical(entry.destination, canonicalError);
        if (canonicalError
            || !ri::content::PackageInstallDestinationsCollide(canonicalDirectory, entry.destination)) {
            Reject("directory entry '" + EntryLabel(entry.name)
                   + "' changed or escaped after archive preflight");
        }
        return;
    }
    std::error_code canonicalError;
    const fs::path canonicalParent = fs::canonical(entry.destination.parent_path(), canonicalError);
    const fs::path resolvedDestination = canonicalParent / entry.destination.filename();
    if (canonicalError
        || !ri::content::PackageInstallDestinationsCollide(
            canonicalParent, entry.destination.parent_path())
        || !ri::content::PackageInstallDestinationsCollide(
            resolvedDestination, entry.destination)) {
        Reject("entry '" + EntryLabel(entry.name)
               + "' destination changed after archive preflight ('"
               + entry.destination.string() + "' -> '" + resolvedDestination.string() + "')");
    }

    ExclusiveOutput output(resolvedDestination);
    std::uint64_t actualBytes = 0U;
    std::uint32_t actualCrc = 0U;
    if (entry.method == kMethodStored) {
        std::array<std::byte, 64U * 1024U> buffer{};
        std::uint64_t remaining = entry.compressedBytes;
        std::uint64_t sourceOffset = entry.dataOffset;
        while (remaining != 0U) {
            const std::size_t chunk = static_cast<std::size_t>(
                std::min<std::uint64_t>(remaining, buffer.size()));
            ReadExactly(input, sourceOffset, buffer.data(), chunk, "stored entry payload");
            actualBytes += chunk;
            if (actualBytes > limits.maximumFileBytes
                || actualBytes > limits.maximumExpandedBytes) {
                Reject("actual expanded-byte budget exceeded while extracting '"
                       + EntryLabel(entry.name) + "'");
            }
            actualCrc = UpdateCrc32(actualCrc, buffer.data(), chunk);
            output.Write(buffer.data(), chunk);
            sourceOffset += chunk;
            remaining -= chunk;
        }
    } else {
        if (entry.compressedBytes > static_cast<std::uint32_t>(INT_MAX)
            || entry.expandedBytes > static_cast<std::uint32_t>(INT_MAX)) {
            Reject("entry exceeds the embedded deflate decoder's bounded input range");
        }
        const std::vector<std::byte> compressed = ReadBytes(
            input, entry.dataOffset, entry.compressedBytes, "deflated entry payload");
        std::vector<std::byte> expanded(std::max<std::size_t>(entry.expandedBytes, 1U));
        const int decoded = stbi_zlib_decode_noheader_buffer(
            reinterpret_cast<char*>(expanded.data()), static_cast<int>(entry.expandedBytes),
            reinterpret_cast<const char*>(compressed.data()), static_cast<int>(compressed.size()));
        if (decoded < 0) {
            Reject("deflate output for entry '" + EntryLabel(entry.name)
                   + " is corrupt or exceeds its declared bounded size");
        }
        actualBytes = static_cast<std::uint64_t>(decoded);
        if (actualBytes != entry.expandedBytes
            || actualBytes > limits.maximumFileBytes
            || actualBytes > limits.maximumExpandedBytes
            || ExceedsRatio(actualBytes, entry.compressedBytes, limits.maximumCompressionRatio)) {
            Reject("actual deflate output for entry '" + EntryLabel(entry.name)
                   + " violates declared size or extraction budgets");
        }
        actualCrc = UpdateCrc32(0U, expanded.data(), static_cast<std::size_t>(actualBytes));
        output.Write(expanded.data(), static_cast<std::size_t>(actualBytes));
    }
    if (actualBytes != entry.expandedBytes || actualCrc != entry.crc32) {
        Reject("entry '" + EntryLabel(entry.name) + "' failed actual size/CRC verification");
    }
    output.Commit();
}

} // namespace

SecureRipakExtraction::~SecureRipakExtraction() noexcept {
    Cleanup();
}

SecureRipakExtraction::SecureRipakExtraction(SecureRipakExtraction&& other) noexcept
    : root_(std::exchange(other.root_, {})) {}

SecureRipakExtraction& SecureRipakExtraction::operator=(SecureRipakExtraction&& other) noexcept {
    if (this != &other) {
        Cleanup();
        root_ = std::exchange(other.root_, {});
    }
    return *this;
}

void SecureRipakExtraction::Cleanup() noexcept {
    if (root_.empty()) {
        return;
    }
    std::error_code ignored;
    fs::remove_all(root_, ignored);
    root_.clear();
}

SecureRipakExtraction SecureRipakExtraction::Extract(
    const fs::path& archivePath,
    const SecureRipakLimits& limits) {
    if (limits.maximumArchiveBytes == 0U || limits.maximumEntries == 0U
        || limits.maximumFileBytes == 0U || limits.maximumExpandedBytes == 0U
        || limits.maximumCompressionRatio == 0U) {
        throw std::invalid_argument("Secure .ripak extraction limits must all be non-zero.");
    }
    std::error_code error;
    const fs::file_status sourceStatus = fs::symlink_status(archivePath, error);
    if (error || !fs::is_regular_file(sourceStatus) || fs::is_symlink(sourceStatus)) {
        throw std::runtime_error(".ripak source must be a regular, non-link file: " + archivePath.string());
    }
#if defined(_WIN32)
    const DWORD sourceAttributes = GetFileAttributesW(archivePath.c_str());
    if (sourceAttributes == INVALID_FILE_ATTRIBUTES
        || (sourceAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) {
        throw std::runtime_error(".ripak source must not be a Windows reparse point: "
                                 + archivePath.string());
    }
#endif
    const std::uint64_t archiveBytes = fs::file_size(archivePath, error);
    if (error) {
        throw std::runtime_error("Could not measure .ripak archive: " + error.message());
    }
    if (archiveBytes > limits.maximumArchiveBytes) {
        Reject("archive-byte budget exceeded (limit "
               + std::to_string(limits.maximumArchiveBytes) + ")");
    }

    std::ifstream input(archivePath, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open .ripak archive: " + archivePath.string());
    }
    SecureRipakExtraction extraction(CreateExclusiveStagingRoot(archivePath));
    const std::vector<ZipEntry> entries =
        InspectArchive(input, archiveBytes, extraction.root_, limits);
    for (const ZipEntry& entry : entries) {
        ExtractEntry(input, extraction.root_, entry, limits);
    }
    return extraction;
}

} // namespace ri::tooling
