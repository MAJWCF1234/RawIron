#include "RawIron/Content/RipakArchive.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace fs = std::filesystem;

struct FixtureEntry {
    std::string name;
    std::vector<std::byte> bytes;
    std::uint32_t crc = 0U;
    std::uint32_t localOffset = 0U;
    std::uint32_t dataOffset = 0U;
};

struct RemoveTreeOnExit {
    fs::path path;
    ~RemoveTreeOnExit() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

void AppendU16(std::vector<std::byte>& out, std::uint16_t value) {
    out.push_back(static_cast<std::byte>(value & 0xFFU));
    out.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
}

void AppendU32(std::vector<std::byte>& out, std::uint32_t value) {
    AppendU16(out, static_cast<std::uint16_t>(value & 0xFFFFU));
    AppendU16(out, static_cast<std::uint16_t>(value >> 16U));
}

std::uint32_t Crc32(const std::vector<std::byte>& bytes) {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const std::byte value : bytes) {
        crc ^= std::to_integer<unsigned char>(value);
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^ ((crc & 1U) != 0U ? 0xEDB88320U : 0U);
        }
    }
    return ~crc;
}

std::vector<std::byte> Bytes(std::string_view text) {
    std::vector<std::byte> result;
    result.reserve(text.size());
    for (const unsigned char value : text) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

void WriteStoredArchive(const fs::path& path, std::vector<FixtureEntry>& entries) {
    std::vector<std::byte> archive;
    for (FixtureEntry& entry : entries) {
        entry.crc = Crc32(entry.bytes);
        entry.localOffset = static_cast<std::uint32_t>(archive.size());
        AppendU32(archive, 0x04034b50U);
        AppendU16(archive, 20U); AppendU16(archive, 0U); AppendU16(archive, 0U);
        AppendU16(archive, 0U); AppendU16(archive, 0U);
        AppendU32(archive, entry.crc);
        AppendU32(archive, static_cast<std::uint32_t>(entry.bytes.size()));
        AppendU32(archive, static_cast<std::uint32_t>(entry.bytes.size()));
        AppendU16(archive, static_cast<std::uint16_t>(entry.name.size()));
        AppendU16(archive, 0U);
        for (const unsigned char value : entry.name) archive.push_back(static_cast<std::byte>(value));
        entry.dataOffset = static_cast<std::uint32_t>(archive.size());
        archive.insert(archive.end(), entry.bytes.begin(), entry.bytes.end());
    }
    const std::uint32_t centralOffset = static_cast<std::uint32_t>(archive.size());
    for (const FixtureEntry& entry : entries) {
        AppendU32(archive, 0x02014b50U);
        AppendU16(archive, 20U); AppendU16(archive, 20U);
        AppendU16(archive, 0U); AppendU16(archive, 0U);
        AppendU16(archive, 0U); AppendU16(archive, 0U);
        AppendU32(archive, entry.crc);
        AppendU32(archive, static_cast<std::uint32_t>(entry.bytes.size()));
        AppendU32(archive, static_cast<std::uint32_t>(entry.bytes.size()));
        AppendU16(archive, static_cast<std::uint16_t>(entry.name.size()));
        AppendU16(archive, 0U); AppendU16(archive, 0U); AppendU16(archive, 0U); AppendU16(archive, 0U);
        AppendU32(archive, 0100644U << 16U);
        AppendU32(archive, entry.localOffset);
        for (const unsigned char value : entry.name) archive.push_back(static_cast<std::byte>(value));
    }
    const std::uint32_t centralBytes = static_cast<std::uint32_t>(archive.size()) - centralOffset;
    AppendU32(archive, 0x06054b50U);
    AppendU16(archive, 0U); AppendU16(archive, 0U);
    AppendU16(archive, static_cast<std::uint16_t>(entries.size()));
    AppendU16(archive, static_cast<std::uint16_t>(entries.size()));
    AppendU32(archive, centralBytes); AppendU32(archive, centralOffset); AppendU16(archive, 0U);
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(archive.data()), static_cast<std::streamsize>(archive.size()));
}

int Fail(std::string_view message) {
    std::cerr << "RipakDirectMountSmoke: " << message << '\n';
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / ("RawIronDirectMountSmoke." + std::to_string(stamp));
    fs::create_directory(root);
    const RemoveTreeOnExit cleanup{root};
    try {
        const fs::path archivePath = root / "fixture.ripak";
        const std::string blobPath = "content/textures/aa/fixture.png";
        std::vector<std::byte> png{
            std::byte{137}, std::byte{80}, std::byte{78}, std::byte{71},
            std::byte{13}, std::byte{10}, std::byte{26}, std::byte{10}, std::byte{1}, std::byte{2}};
        const std::string index =
            "{\"formatVersion\":1,\"mandatoryCooked\":true,\"sourceFileCount\":1,"
            "\"entries\":[{\"source\":\"Tiles/Test.png\",\"blob\":\"" + blobPath
            + "\",\"sizeBytes\":10,\"width\":32,\"height\":32,\"mode\":\"RGBA\"}]}";
        std::vector<FixtureEntry> entries{
            {"indexes/RAWIRONX32.index.json", Bytes(index)},
            {blobPath, png},
        };
        WriteStoredArchive(archivePath, entries);

        std::optional<ri::content::CookedTexturePack> pack;
        pack.emplace(ri::content::CookedTexturePack::Open(
            archivePath, "indexes/RAWIRONX32.index.json"));
        if (pack->TextureCount() != 1U || pack->Find("tiles\\test.png") == nullptr) {
            return Fail("logical texture index did not mount");
        }
        if (pack->ReadPng("TILES/TEST.PNG") != png) {
            return Fail("on-demand texture bytes did not match archive payload");
        }
        std::size_t fileCount = 0U;
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root)) {
            if (entry.is_regular_file()) ++fileCount;
        }
        if (fileCount != 1U) {
            return Fail("mount or read created extracted files");
        }

        const fs::path corruptPath = root / "corrupt.ripak";
        std::vector<FixtureEntry> corruptEntries = entries;
        WriteStoredArchive(corruptPath, corruptEntries);
        {
            std::fstream corrupt(corruptPath, std::ios::binary | std::ios::in | std::ios::out);
            corrupt.seekp(corruptEntries[1].dataOffset + 9U);
            const char changed = 3;
            corrupt.write(&changed, 1);
        }
        bool crcRejected = false;
        try {
            const ri::content::CookedTexturePack corruptPack = ri::content::CookedTexturePack::Open(
                corruptPath, "indexes/RAWIRONX32.index.json");
            (void)corruptPack.ReadPng("Tiles/Test.png");
        } catch (const std::exception&) {
            crcRejected = true;
        }
        if (!crcRejected) {
            return Fail("corrupted on-demand payload was not rejected");
        }
        pack.reset();

        std::vector<FixtureEntry> unsafe{{"../escape.png", png}};
        const fs::path unsafePath = root / "unsafe.ripak";
        WriteStoredArchive(unsafePath, unsafe);
        bool pathRejected = false;
        try {
            (void)ri::content::RipakArchive::Open(unsafePath);
        } catch (const std::exception&) {
            pathRejected = true;
        }
        if (!pathRejected) {
            return Fail("traversal entry was not rejected");
        }

        if (argc == 5) {
            const ri::content::CookedTexturePack realPack = ri::content::CookedTexturePack::Open(argv[1], argv[2]);
            const std::size_t expectedCount = static_cast<std::size_t>(std::stoull(argv[4]));
            if (realPack.TextureCount() != expectedCount || realPack.ReadPng(argv[3]).empty()) {
                return Fail("real cooked pack did not mount and stream as expected");
            }
        } else if (argc != 1) {
            return Fail("expected either no arguments or <ripak> <index> <logical-texture> <count>");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "RipakDirectMountSmoke: " << error.what() << '\n';
        return 1;
    }
}
