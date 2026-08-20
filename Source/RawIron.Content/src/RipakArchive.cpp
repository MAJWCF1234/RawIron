#include "RawIron/Content/RipakArchive.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>

namespace ri::content {
namespace {

constexpr std::uint32_t kLocalHeaderSignature = 0x04034b50U;
constexpr std::uint32_t kCentralHeaderSignature = 0x02014b50U;
constexpr std::uint32_t kEndSignature = 0x06054b50U;
constexpr std::size_t kEndRecordBytes = 22U;
constexpr std::size_t kMaximumEndSearchBytes = 65557U;
constexpr std::uint16_t kFlagEncrypted = 0x0001U;
constexpr std::uint16_t kFlagDataDescriptor = 0x0008U;
constexpr std::uint16_t kFlagUtf8 = 0x0800U;
constexpr std::uint16_t kFlagStrongEncryption = 0x0040U;
constexpr std::uint16_t kFlagMaskedHeader = 0x2000U;
constexpr std::uint16_t kAllowedFlags = kFlagUtf8;

[[noreturn]] void Reject(const std::string& reason) {
    throw std::runtime_error("RIPAK mount rejected: " + reason);
}

[[nodiscard]] std::uint16_t ReadU16(const std::byte* bytes) noexcept {
    return static_cast<std::uint16_t>(std::to_integer<unsigned char>(bytes[0]))
        | (static_cast<std::uint16_t>(std::to_integer<unsigned char>(bytes[1])) << 8U);
}

[[nodiscard]] std::uint32_t ReadU32(const std::byte* bytes) noexcept {
    return static_cast<std::uint32_t>(ReadU16(bytes))
        | (static_cast<std::uint32_t>(ReadU16(bytes + 2U)) << 16U);
}

[[nodiscard]] bool CheckedAdd(std::uint64_t left, std::uint64_t right, std::uint64_t* result) noexcept {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return false;
    }
    *result = left + right;
    return true;
}

void ReadExactly(std::ifstream& input, std::uint64_t offset, std::byte* output,
                 std::size_t count, std::string_view context) {
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())
        || count > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        Reject(std::string(context) + " exceeds host stream limits");
    }
    input.clear();
    input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!input) {
        Reject("could not seek to " + std::string(context));
    }
    if (count != 0U) {
        input.read(reinterpret_cast<char*>(output), static_cast<std::streamsize>(count));
        if (input.gcount() != static_cast<std::streamsize>(count)) {
            Reject("archive ended while reading " + std::string(context));
        }
    }
}

[[nodiscard]] std::vector<std::byte> ReadBytes(std::ifstream& input, std::uint64_t offset,
                                               std::size_t count, std::string_view context) {
    std::vector<std::byte> result(count);
    ReadExactly(input, offset, result.data(), result.size(), context);
    return result;
}

[[nodiscard]] bool ValidUtf8(std::string_view value) noexcept {
    std::size_t index = 0U;
    while (index < value.size()) {
        const auto first = static_cast<unsigned char>(value[index]);
        if (first <= 0x7FU) {
            ++index;
            continue;
        }
        std::size_t continuationCount = 0U;
        std::uint32_t codePoint = 0U;
        if (first >= 0xC2U && first <= 0xDFU) {
            continuationCount = 1U; codePoint = first & 0x1FU;
        } else if (first >= 0xE0U && first <= 0xEFU) {
            continuationCount = 2U; codePoint = first & 0x0FU;
        } else if (first >= 0xF0U && first <= 0xF4U) {
            continuationCount = 3U; codePoint = first & 0x07U;
        } else {
            return false;
        }
        if (index + continuationCount >= value.size()) {
            return false;
        }
        for (std::size_t offset = 1U; offset <= continuationCount; ++offset) {
            const auto next = static_cast<unsigned char>(value[index + offset]);
            if ((next & 0xC0U) != 0x80U) {
                return false;
            }
            codePoint = (codePoint << 6U) | (next & 0x3FU);
        }
        if ((continuationCount == 2U && codePoint < 0x800U)
            || (continuationCount == 3U && codePoint < 0x10000U)
            || codePoint > 0x10FFFFU || (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
            return false;
        }
        index += continuationCount + 1U;
    }
    return true;
}

[[nodiscard]] std::optional<std::string> NormalizePackagePath(std::string_view input) {
    if (input.empty() || input.front() == '/' || input.front() == '\\' || input.back() == '/') {
        return std::nullopt;
    }
    std::string result;
    result.reserve(input.size());
    std::size_t componentStart = 0U;
    for (std::size_t index = 0U; index <= input.size(); ++index) {
        if (index != input.size() && input[index] != '/' && input[index] != '\\') {
            const unsigned char byte = static_cast<unsigned char>(input[index]);
            if (byte < 0x20U || byte == 0x7FU || input[index] == ':') {
                return std::nullopt;
            }
            continue;
        }
        const std::string_view component = input.substr(componentStart, index - componentStart);
        if (component.empty() || component == "." || component == ".."
            || component.back() == ' ' || component.back() == '.') {
            return std::nullopt;
        }
        if (!result.empty()) {
            result.push_back('/');
        }
        result.append(component);
        componentStart = index + 1U;
    }
    if (!ValidUtf8(result)) {
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] std::string PortableKey(std::string_view path) {
    std::string key;
    key.reserve(path.size());
    for (const unsigned char character : path) {
        key.push_back(static_cast<char>(character >= 'A' && character <= 'Z'
            ? character + ('a' - 'A') : character));
    }
    return key;
}

[[nodiscard]] std::uint32_t Crc32(const std::byte* data, std::size_t size) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t index = 0U; index < size; ++index) {
        crc ^= std::to_integer<unsigned char>(data[index]);
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^ ((crc & 1U) != 0U ? 0xEDB88320U : 0U);
        }
    }
    return ~crc;
}

[[nodiscard]] std::uint32_t ToU32(const std::optional<std::uint64_t>& value, std::string_view field) {
    if (!value.has_value() || *value > std::numeric_limits<std::uint32_t>::max()) {
        Reject("texture index has invalid " + std::string(field));
    }
    return static_cast<std::uint32_t>(*value);
}

} // namespace

struct RipakArchive::MountState {
    explicit MountState(const std::filesystem::path& path) : input(path, std::ios::binary) {}

    std::ifstream input;
    std::mutex mutex;
};

RipakArchive RipakArchive::Open(const std::filesystem::path& archivePath, const RipakMountLimits& limits) {
    std::error_code error;
    const std::filesystem::path canonicalPath = std::filesystem::canonical(archivePath, error);
    if (error || !std::filesystem::is_regular_file(canonicalPath, error) || error) {
        throw std::runtime_error("Could not open RIPAK file: " + archivePath.string());
    }
    const std::uint64_t archiveBytes = std::filesystem::file_size(canonicalPath, error);
    if (error || archiveBytes > limits.maximumArchiveBytes || archiveBytes < kEndRecordBytes) {
        Reject("archive size is invalid or exceeds the mount budget");
    }
    auto state = std::make_shared<MountState>(canonicalPath);
    if (!state->input.is_open()) {
        throw std::runtime_error("Could not read RIPAK file: " + canonicalPath.string());
    }
    std::ifstream& input = state->input;

    const std::size_t tailBytes = static_cast<std::size_t>(
        std::min<std::uint64_t>(archiveBytes, kMaximumEndSearchBytes));
    const std::uint64_t tailOffset = archiveBytes - tailBytes;
    const std::vector<std::byte> tail = ReadBytes(input, tailOffset, tailBytes, "ZIP end record");
    std::optional<std::size_t> endIndex;
    for (std::size_t index = tail.size() - kEndRecordBytes + 1U; index-- > 0U;) {
        if (ReadU32(tail.data() + index) == kEndSignature
            && index + kEndRecordBytes + ReadU16(tail.data() + index + 20U) == tail.size()) {
            endIndex = index;
            break;
        }
    }
    if (!endIndex.has_value()) {
        Reject("missing ZIP end record");
    }
    const std::byte* end = tail.data() + *endIndex;
    const std::uint16_t entryCount = ReadU16(end + 10U);
    const std::uint32_t centralBytes = ReadU32(end + 12U);
    const std::uint32_t centralOffset = ReadU32(end + 16U);
    if (ReadU16(end + 4U) != 0U || ReadU16(end + 6U) != 0U
        || ReadU16(end + 8U) != entryCount) {
        Reject("multi-disk archives are unsupported");
    }
    if (entryCount == 0xFFFFU || centralBytes == 0xFFFFFFFFU || centralOffset == 0xFFFFFFFFU) {
        Reject("ZIP64 archives are unsupported");
    }
    if (entryCount > limits.maximumEntries) {
        Reject("entry-count budget exceeded");
    }
    std::uint64_t centralEnd = 0U;
    if (!CheckedAdd(centralOffset, centralBytes, &centralEnd)
        || centralEnd != tailOffset + *endIndex) {
        Reject("central directory bounds are inconsistent");
    }

    RipakArchive result;
    result.archivePath_ = canonicalPath;
    result.state_ = std::move(state);
    result.publicEntries_.reserve(entryCount);
    result.entries_.reserve(entryCount);
    std::uint64_t cursor = centralOffset;
    std::uint64_t totalBytes = 0U;
    std::set<std::string> portableNames;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> occupiedRanges;
    occupiedRanges.reserve(entryCount);
    for (std::size_t entryIndex = 0U; entryIndex < entryCount; ++entryIndex) {
        const std::vector<std::byte> fixed = ReadBytes(input, cursor, 46U, "central entry");
        if (ReadU32(fixed.data()) != kCentralHeaderSignature) {
            Reject("invalid central-directory entry");
        }
        const std::uint16_t versionNeeded = ReadU16(fixed.data() + 6U);
        const std::uint16_t flags = ReadU16(fixed.data() + 8U);
        const std::uint16_t method = ReadU16(fixed.data() + 10U);
        const std::uint32_t crc = ReadU32(fixed.data() + 16U);
        const std::uint32_t compressedBytes = ReadU32(fixed.data() + 20U);
        const std::uint32_t expandedBytes = ReadU32(fixed.data() + 24U);
        const std::uint16_t nameBytes = ReadU16(fixed.data() + 28U);
        const std::uint16_t extraBytes = ReadU16(fixed.data() + 30U);
        const std::uint16_t commentBytes = ReadU16(fixed.data() + 32U);
        const std::uint32_t localOffset = ReadU32(fixed.data() + 42U);
        if (versionNeeded > 20U || method != 0U || (flags != kAllowedFlags && flags != 0U)
            || (flags & (kFlagEncrypted | kFlagDataDescriptor | kFlagStrongEncryption | kFlagMaskedHeader)) != 0U) {
            Reject("runtime mount requires plain STORE entries");
        }
        if (compressedBytes != expandedBytes || expandedBytes > limits.maximumFileBytes
            || nameBytes == 0U || extraBytes != 0U || localOffset >= centralOffset) {
            Reject("invalid STORE entry metadata");
        }
        std::uint64_t variableEnd = 0U;
        if (!CheckedAdd(cursor + 46U, static_cast<std::uint64_t>(nameBytes) + extraBytes + commentBytes,
                        &variableEnd) || variableEnd > centralEnd) {
            Reject("central entry exceeds directory bounds");
        }
        const std::vector<std::byte> variable = ReadBytes(
            input, cursor + 46U, static_cast<std::size_t>(nameBytes) + extraBytes + commentBytes,
            "central entry fields");
        const std::string rawName(reinterpret_cast<const char*>(variable.data()), nameBytes);
        const std::optional<std::string> normalized = NormalizePackagePath(rawName);
        if (!normalized.has_value() || *normalized != rawName) {
            Reject("unsafe or non-canonical entry path");
        }
        if ((flags & kFlagUtf8) == 0U && std::any_of(rawName.begin(), rawName.end(),
                [](unsigned char byte) { return byte >= 0x80U; })) {
            Reject("non-ASCII path lacks the UTF-8 flag");
        }
        if (!portableNames.insert(PortableKey(rawName)).second) {
            Reject("duplicate or case-colliding entry path");
        }
        if (!CheckedAdd(totalBytes, expandedBytes, &totalBytes) || totalBytes > limits.maximumIndexedBytes) {
            Reject("indexed-byte budget exceeded");
        }

        const std::vector<std::byte> local = ReadBytes(input, localOffset, 30U, "local header");
        if (ReadU32(local.data()) != kLocalHeaderSignature
            || ReadU16(local.data() + 6U) != flags || ReadU16(local.data() + 8U) != method
            || ReadU32(local.data() + 14U) != crc
            || ReadU32(local.data() + 18U) != compressedBytes
            || ReadU32(local.data() + 22U) != expandedBytes) {
            Reject("central and local headers disagree");
        }
        const std::uint16_t localNameBytes = ReadU16(local.data() + 26U);
        const std::uint16_t localExtraBytes = ReadU16(local.data() + 28U);
        if (localExtraBytes != 0U) {
            Reject("runtime mount does not accept ZIP extra fields");
        }
        std::uint64_t dataOffset = 0U;
        if (!CheckedAdd(static_cast<std::uint64_t>(localOffset) + 30U,
                        static_cast<std::uint64_t>(localNameBytes) + localExtraBytes, &dataOffset)) {
            Reject("local header overflows");
        }
        const std::vector<std::byte> localVariable = ReadBytes(
            input, static_cast<std::uint64_t>(localOffset) + 30U,
            static_cast<std::size_t>(localNameBytes) + localExtraBytes, "local fields");
        const std::string localName(reinterpret_cast<const char*>(localVariable.data()), localNameBytes);
        std::uint64_t occupiedEnd = 0U;
        if (localName != rawName || !CheckedAdd(dataOffset, compressedBytes, &occupiedEnd)
            || occupiedEnd > centralOffset) {
            Reject("invalid local payload range");
        }
        occupiedRanges.emplace_back(localOffset, occupiedEnd);

        IndexedEntry indexed{{rawName, expandedBytes, crc}, dataOffset};
        result.publicEntries_.push_back(indexed.publicEntry);
        result.entries_.emplace(PortableKey(rawName), std::move(indexed));
        cursor = variableEnd;
    }
    if (cursor != centralEnd) {
        Reject("central directory was not fully consumed");
    }
    std::sort(occupiedRanges.begin(), occupiedRanges.end());
    for (std::size_t index = 1U; index < occupiedRanges.size(); ++index) {
        if (occupiedRanges[index].first < occupiedRanges[index - 1U].second) {
            Reject("local header or payload ranges overlap");
        }
    }
    return result;
}

bool RipakArchive::Contains(std::string_view packagePath) const noexcept {
    const std::optional<std::string> normalized = NormalizePackagePath(packagePath);
    return normalized.has_value() && entries_.contains(PortableKey(*normalized));
}

std::vector<std::byte> RipakArchive::ReadFile(std::string_view packagePath,
                                             std::uint64_t maximumBytes) const {
    const std::optional<std::string> normalized = NormalizePackagePath(packagePath);
    if (!normalized.has_value()) {
        throw std::runtime_error("Invalid RIPAK package path");
    }
    const auto found = entries_.find(PortableKey(*normalized));
    if (found == entries_.end()) {
        throw std::runtime_error("RIPAK entry does not exist: " + *normalized);
    }
    if (found->second.publicEntry.sizeBytes > maximumBytes
        || found->second.publicEntry.sizeBytes > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("RIPAK read exceeds caller byte budget: " + *normalized);
    }
    if (!state_ || !state_->input.is_open()) {
        throw std::runtime_error("RIPAK mount is not open: " + archivePath_.string());
    }
    std::scoped_lock lock(state_->mutex);
    std::vector<std::byte> bytes = ReadBytes(
        state_->input, found->second.dataOffset, static_cast<std::size_t>(found->second.publicEntry.sizeBytes),
        "mounted entry payload");
    if (Crc32(bytes.data(), bytes.size()) != found->second.publicEntry.crc32) {
        Reject("CRC mismatch while reading '" + found->second.publicEntry.path + "'");
    }
    return bytes;
}

CookedTexturePack CookedTexturePack::Open(const std::filesystem::path& archivePath,
                                          std::string_view indexPath,
                                          const RipakMountLimits& limits) {
    CookedTexturePack result;
    result.archive_ = RipakArchive::Open(archivePath, limits);
    const std::vector<std::byte> indexBytes = result.archive_.ReadFile(indexPath, 64ULL * 1024ULL * 1024ULL);
    const std::string indexText(reinterpret_cast<const char*>(indexBytes.data()), indexBytes.size());
    if (ri::core::detail::ExtractJsonInt(indexText, "formatVersion").value_or(0) != 1
        || !ri::core::detail::ExtractJsonBool(indexText, "mandatoryCooked").value_or(false)) {
        Reject("texture index is not mandatory-cooked format version 1");
    }
    const std::vector<std::string_view> rows = ri::core::detail::SplitJsonArrayObjects(indexText, "entries");
    const std::uint64_t declaredCount = ri::core::detail::ExtractJsonUInt64(
        indexText, "sourceFileCount").value_or(std::numeric_limits<std::uint64_t>::max());
    if (rows.size() != declaredCount || rows.size() > limits.maximumEntries) {
        Reject("texture index count is inconsistent or exceeds budget");
    }
    result.textures_.reserve(rows.size());
    for (const std::string_view row : rows) {
        const std::optional<std::string> source = ri::core::detail::ExtractJsonString(row, "source");
        const std::optional<std::string> blob = ri::core::detail::ExtractJsonString(row, "blob");
        const std::optional<std::string> mode = ri::core::detail::ExtractJsonString(row, "mode");
        const std::optional<std::string> normalizedSource = source ? NormalizePackagePath(*source) : std::nullopt;
        const std::optional<std::string> normalizedBlob = blob ? NormalizePackagePath(*blob) : std::nullopt;
        const std::optional<std::uint64_t> size = ri::core::detail::ExtractJsonUInt64(row, "sizeBytes");
        if (!source || !blob || !mode || !normalizedSource || !normalizedBlob
            || *source != *normalizedSource || *blob != *normalizedBlob || !size
            || *size > limits.maximumFileBytes || !result.archive_.Contains(*blob)) {
            Reject("texture index contains an invalid row");
        }
        CookedTextureRecord record{
            *source, *blob, *size,
            ToU32(ri::core::detail::ExtractJsonUInt64(row, "width"), "width"),
            ToU32(ri::core::detail::ExtractJsonUInt64(row, "height"), "height"),
            *mode,
        };
        const std::string key = PortableKey(record.logicalPath);
        if (!result.textures_.emplace(key, std::move(record)).second) {
            Reject("texture index contains duplicate or case-colliding source paths");
        }
    }
    return result;
}

const CookedTextureRecord* CookedTexturePack::Find(std::string_view logicalPath) const noexcept {
    const std::optional<std::string> normalized = NormalizePackagePath(logicalPath);
    if (!normalized) {
        return nullptr;
    }
    const auto found = textures_.find(PortableKey(*normalized));
    return found == textures_.end() ? nullptr : &found->second;
}

std::vector<std::byte> CookedTexturePack::ReadPng(std::string_view logicalPath,
                                                 std::uint64_t maximumBytes) const {
    const CookedTextureRecord* record = Find(logicalPath);
    if (record == nullptr) {
        throw std::runtime_error("Cooked texture does not exist: " + std::string(logicalPath));
    }
    if (record->sizeBytes > maximumBytes) {
        throw std::runtime_error("Cooked texture exceeds caller byte budget: " + record->logicalPath);
    }
    std::vector<std::byte> bytes = archive_.ReadFile(record->blobPath, maximumBytes);
    if (bytes.size() != record->sizeBytes) {
        Reject("texture index size disagrees with blob entry");
    }
    constexpr std::array<unsigned char, 8> pngSignature{137U, 80U, 78U, 71U, 13U, 10U, 26U, 10U};
    if (bytes.size() < pngSignature.size()
        || !std::equal(pngSignature.begin(), pngSignature.end(), bytes.begin(),
            [](unsigned char expected, std::byte actual) {
                return expected == std::to_integer<unsigned char>(actual);
            })) {
        Reject("cooked texture blob is not a PNG");
    }
    return bytes;
}

} // namespace ri::content
