#include "RawIron/Runtime/DemoStream.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

void WriteFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::uint8_t> ReadFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void PatchU32(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint32_t value) {
    bytes[offset + 0U] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    bytes[offset + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

} // namespace

int main() {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "RawIronDemoStreamSmoke";
    std::error_code cleanupError;
    fs::remove_all(root, cleanupError);
    const fs::path goodPath = root / "nested" / "good.ride";

    ri::runtime::DemoWriter writer;
    ri::runtime::DemoHeader header{.runtimeId = "runtime", .mapOrScene = "map"};
    ri::runtime::DemoHeader badSchema = header;
    badSchema.schemaVersion = 2U;
    ri::runtime::DemoHeader zeroRate = header;
    zeroRate.tickRate = 0U;
    ri::runtime::DemoHeader hugeHeader = header;
    hugeHeader.runtimeId.assign(4097U, 'x');
    if (writer.Open({}, header) || writer.Open(root / "schema.ride", badSchema)
        || writer.Open(root / "rate.ride", zeroRate) || writer.Open(root / "header.ride", hugeHeader)
        || !writer.Open(goodPath, header)) {
        return EXIT_FAILURE;
    }

    ri::runtime::DemoFrameRecord first{
        .tick = 10U,
        .inputBytes = {1U, 2U},
        .authoritativeSnapshotBytes = {3U},
        .deterministicChecksum = 100U,
    };
    if (!writer.Append(first) || writer.Append(first)) {
        return EXIT_FAILURE;
    }
    ri::runtime::DemoFrameRecord older = first;
    older.tick = 9U;
    if (writer.Append(older)) {
        return EXIT_FAILURE;
    }
    ri::runtime::DemoFrameRecord oversized = first;
    oversized.tick = 11U;
    oversized.inputBytes.resize(16U * 1024U * 1024U + 1U);
    if (writer.Append(oversized)) {
        return EXIT_FAILURE;
    }
    ri::runtime::DemoFrameRecord second = first;
    second.tick = 11U;
    second.deterministicChecksum = 101U;
    if (!writer.Append(second)) {
        return EXIT_FAILURE;
    }
    writer.Close();

    ri::runtime::DemoReader reader;
    if (!reader.Open(goodPath) || reader.FrameCount() != 2U
        || reader.Header().runtimeId != "runtime"
        || reader.ReadFrame(1U)->tick != 11U || reader.ReadFrame(2U).has_value()) {
        return EXIT_FAILURE;
    }

    const std::vector<std::uint8_t> goodBytes = ReadFile(goodPath);
    std::vector<std::uint8_t> corrupt = goodBytes;
    corrupt.push_back(0xFFU);
    WriteFile(root / "trailing.ride", corrupt);
    if (reader.Open(root / "trailing.ride")) {
        return EXIT_FAILURE;
    }

    corrupt = goodBytes;
    PatchU32(corrupt, 4U, 2U);
    WriteFile(root / "schema-bad.ride", corrupt);
    if (reader.Open(root / "schema-bad.ride")) {
        return EXIT_FAILURE;
    }

    corrupt = goodBytes;
    PatchU32(corrupt, 8U, 0U);
    WriteFile(root / "rate-bad.ride", corrupt);
    if (reader.Open(root / "rate-bad.ride")) {
        return EXIT_FAILURE;
    }

    constexpr std::size_t frameCountOffset = 34U;
    constexpr std::size_t firstInputLengthOffset = frameCountOffset + 4U + 4U;
    corrupt = goodBytes;
    PatchU32(corrupt, frameCountOffset, 1000001U);
    WriteFile(root / "count-bad.ride", corrupt);
    if (reader.Open(root / "count-bad.ride")) {
        return EXIT_FAILURE;
    }

    corrupt = goodBytes;
    PatchU32(corrupt, firstInputLengthOffset, 16U * 1024U * 1024U + 1U);
    WriteFile(root / "blob-bad.ride", corrupt);
    if (reader.Open(root / "blob-bad.ride")) {
        return EXIT_FAILURE;
    }

    corrupt = goodBytes;
    corrupt.resize(corrupt.size() - 1U);
    WriteFile(root / "truncated.ride", corrupt);
    if (reader.Open(root / "truncated.ride")) {
        return EXIT_FAILURE;
    }

    fs::remove_all(root, cleanupError);
    return EXIT_SUCCESS;
}
