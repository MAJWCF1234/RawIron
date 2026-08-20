#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::content {

struct RipakMountLimits {
    std::uint64_t maximumArchiveBytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;
    std::uint64_t maximumIndexedBytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;
    std::uint64_t maximumFileBytes = 512ULL * 1024ULL * 1024ULL;
    std::size_t maximumEntries = 65535U;
};

struct RipakEntry {
    std::string path;
    std::uint64_t sizeBytes = 0U;
    std::uint32_t crc32 = 0U;
};

/// Read-only, random-access mount for mandatory cooked Raw Iron packages.
///
/// Runtime mounts intentionally support STORE entries only. Each requested file is read directly
/// from its archive range and CRC-checked; mounting and reading never extract an archive tree.
class RipakArchive final {
public:
    [[nodiscard]] static RipakArchive Open(
        const std::filesystem::path& archivePath,
        const RipakMountLimits& limits = {});

    [[nodiscard]] const std::filesystem::path& Path() const noexcept { return archivePath_; }
    [[nodiscard]] const std::vector<RipakEntry>& Entries() const noexcept { return publicEntries_; }
    [[nodiscard]] bool Contains(std::string_view packagePath) const noexcept;
    [[nodiscard]] std::vector<std::byte> ReadFile(
        std::string_view packagePath,
        std::uint64_t maximumBytes = 512ULL * 1024ULL * 1024ULL) const;

private:
    struct MountState;
    struct IndexedEntry {
        RipakEntry publicEntry;
        std::uint64_t dataOffset = 0U;
    };

    std::filesystem::path archivePath_;
    std::shared_ptr<MountState> state_;
    std::vector<RipakEntry> publicEntries_;
    std::unordered_map<std::string, IndexedEntry> entries_;
};

struct CookedTextureRecord {
    std::string logicalPath;
    std::string blobPath;
    std::uint64_t sizeBytes = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::string mode;
};

/// Logical texture lookup layered over a directly mounted RIPAK.
class CookedTexturePack final {
public:
    [[nodiscard]] static CookedTexturePack Open(
        const std::filesystem::path& archivePath,
        std::string_view indexPath,
        const RipakMountLimits& limits = {});

    [[nodiscard]] const RipakArchive& Archive() const noexcept { return archive_; }
    [[nodiscard]] const CookedTextureRecord* Find(std::string_view logicalPath) const noexcept;
    [[nodiscard]] std::vector<std::byte> ReadPng(
        std::string_view logicalPath,
        std::uint64_t maximumBytes = 64ULL * 1024ULL * 1024ULL) const;
    [[nodiscard]] std::size_t TextureCount() const noexcept { return textures_.size(); }

private:
    RipakArchive archive_;
    std::unordered_map<std::string, CookedTextureRecord> textures_;
};

} // namespace ri::content
