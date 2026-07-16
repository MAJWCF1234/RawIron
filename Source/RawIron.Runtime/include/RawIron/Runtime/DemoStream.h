#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ri::runtime {

struct DemoFrameRecord {
    std::uint32_t tick = 0;
    std::vector<std::uint8_t> inputBytes;
    std::vector<std::uint8_t> authoritativeSnapshotBytes;
    std::uint32_t deterministicChecksum = 0;
};

struct DemoHeader {
    std::string runtimeId;
    std::string mapOrScene;
    std::uint32_t tickRate = 60;
    std::uint32_t schemaVersion = 1;
    std::uint32_t streamSeed = 0xC001CAFEu;
};

class DemoWriter {
public:
    bool Open(const std::filesystem::path& path, const DemoHeader& header);
    bool Append(const DemoFrameRecord& frame);
    void Close();
    [[nodiscard]] bool IsOpen() const noexcept;

private:
    std::filesystem::path path_{};
    std::vector<std::uint8_t> stream_{};
    std::size_t frameCountOffset_ = 0;
    std::uint32_t frameCount_ = 0;
    std::uint32_t lastTick_ = 0;
    bool hasFrames_ = false;
    bool open_ = false;
};

class DemoReader {
public:
    bool Open(const std::filesystem::path& path);
    [[nodiscard]] const DemoHeader& Header() const noexcept;
    [[nodiscard]] std::size_t FrameCount() const noexcept;
    [[nodiscard]] std::optional<DemoFrameRecord> ReadFrame(std::size_t index) const;
    void Close();
    [[nodiscard]] bool IsOpen() const noexcept;

private:
    DemoHeader header_{};
    std::vector<DemoFrameRecord> frames_{};
    bool open_ = false;
};

} // namespace ri::runtime
