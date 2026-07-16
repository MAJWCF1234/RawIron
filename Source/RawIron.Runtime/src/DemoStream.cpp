#include "RawIron/Runtime/DemoStream.h"

#include <cstring>
#include <fstream>

namespace ri::runtime {
namespace {

constexpr std::uint32_t kSupportedSchemaVersion = 1U;
constexpr std::size_t kMaxHeaderStringBytes = 4096U;
constexpr std::size_t kMaxFrameBlobBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaxFrameCount = 1000000U;
constexpr std::uintmax_t kMaxDemoFileBytes = 512U * 1024U * 1024U;

void WriteU32(std::vector<std::uint8_t>& out, const std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((v >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((v >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((v >> 24U) & 0xFFU));
}

bool ReadU32(const std::vector<std::uint8_t>& in, std::size_t& at, std::uint32_t& out) {
    if (at > in.size() || in.size() - at < 4U) {
        return false;
    }
    out = static_cast<std::uint32_t>(in[at]) |
          (static_cast<std::uint32_t>(in[at + 1U]) << 8U) |
          (static_cast<std::uint32_t>(in[at + 2U]) << 16U) |
          (static_cast<std::uint32_t>(in[at + 3U]) << 24U);
    at += 4U;
    return true;
}

void WriteString(std::vector<std::uint8_t>& out, const std::string& value) {
    WriteU32(out, static_cast<std::uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

bool ReadString(const std::vector<std::uint8_t>& in,
                std::size_t& at,
                std::string& value,
                const std::size_t maxLength = kMaxHeaderStringBytes) {
    std::uint32_t len = 0;
    if (!ReadU32(in, at, len) || len > maxLength || at > in.size() || len > in.size() - at) {
        return false;
    }
    value.assign(reinterpret_cast<const char*>(in.data() + at), len);
    at += len;
    return true;
}

void WriteBytes(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& value) {
    WriteU32(out, static_cast<std::uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

bool ReadBytes(const std::vector<std::uint8_t>& in, std::size_t& at, std::vector<std::uint8_t>& value) {
    std::uint32_t len = 0;
    if (!ReadU32(in, at, len) || len > kMaxFrameBlobBytes || at > in.size() || len > in.size() - at) {
        return false;
    }
    value.assign(in.begin() + static_cast<std::ptrdiff_t>(at), in.begin() + static_cast<std::ptrdiff_t>(at + len));
    at += len;
    return true;
}

void PatchU32(std::vector<std::uint8_t>& out, const std::size_t at, const std::uint32_t value) {
    if (at > out.size() || out.size() - at < 4U) {
        return;
    }
    out[at + 0U] = static_cast<std::uint8_t>(value & 0xFFU);
    out[at + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    out[at + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    out[at + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

} // namespace

bool DemoWriter::Open(const std::filesystem::path& path, const DemoHeader& header) {
    Close();
    if (path.empty() || header.schemaVersion != kSupportedSchemaVersion || header.tickRate == 0U
        || header.runtimeId.size() > kMaxHeaderStringBytes || header.mapOrScene.size() > kMaxHeaderStringBytes) {
        return false;
    }
    std::error_code directoryError;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), directoryError);
        if (directoryError) {
            return false;
        }
    }
    std::ofstream probe(path, std::ios::binary | std::ios::trunc);
    if (!probe.is_open()) {
        return false;
    }
    probe.close();
    path_ = path;
    stream_.clear();
    stream_.reserve(4096);
    stream_.insert(stream_.end(), {'R', 'I', 'D', 'E'});
    WriteU32(stream_, header.schemaVersion);
    WriteU32(stream_, header.tickRate);
    WriteU32(stream_, header.streamSeed);
    WriteString(stream_, header.runtimeId);
    WriteString(stream_, header.mapOrScene);
    frameCountOffset_ = stream_.size();
    WriteU32(stream_, 0U); // frame count placeholder
    frameCount_ = 0U;
    lastTick_ = 0U;
    hasFrames_ = false;
    open_ = true;
    return true;
}

bool DemoWriter::Append(const DemoFrameRecord& frame) {
    if (!open_) {
        return false;
    }
    if (frame.inputBytes.size() > kMaxFrameBlobBytes
        || frame.authoritativeSnapshotBytes.size() > kMaxFrameBlobBytes
        || frameCount_ >= kMaxFrameCount
        || (hasFrames_ && frame.tick <= lastTick_)) {
        return false;
    }
    const std::size_t encodedSize = 16U + frame.inputBytes.size() + frame.authoritativeSnapshotBytes.size();
    if (stream_.size() > kMaxDemoFileBytes || encodedSize > kMaxDemoFileBytes - stream_.size()) {
        return false;
    }
    WriteU32(stream_, frame.tick);
    WriteBytes(stream_, frame.inputBytes);
    WriteBytes(stream_, frame.authoritativeSnapshotBytes);
    WriteU32(stream_, frame.deterministicChecksum);
    lastTick_ = frame.tick;
    hasFrames_ = true;
    ++frameCount_;
    return true;
}

void DemoWriter::Close() {
    if (!open_) {
        return;
    }
    PatchU32(stream_, frameCountOffset_, frameCount_);

    std::ofstream file(path_, std::ios::binary | std::ios::trunc);
    if (file.good()) {
        file.write(reinterpret_cast<const char*>(stream_.data()), static_cast<std::streamsize>(stream_.size()));
    }
    open_ = false;
    stream_.clear();
    frameCountOffset_ = 0U;
    frameCount_ = 0U;
    lastTick_ = 0U;
    hasFrames_ = false;
}

bool DemoWriter::IsOpen() const noexcept {
    return open_;
}

bool DemoReader::Open(const std::filesystem::path& path) {
    Close();
    std::error_code sizeError;
    const std::uintmax_t fileSize = std::filesystem::file_size(path, sizeError);
    if (sizeError || fileSize > kMaxDemoFileBytes) {
        return false;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
        return false;
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytes.size() < 16U || std::memcmp(bytes.data(), "RIDE", 4) != 0) {
        return false;
    }

    DemoHeader parsedHeader{};
    std::size_t at = 4U;
    if (!ReadU32(bytes, at, parsedHeader.schemaVersion) || !ReadU32(bytes, at, parsedHeader.tickRate) ||
        !ReadU32(bytes, at, parsedHeader.streamSeed) ||
        !ReadString(bytes, at, parsedHeader.runtimeId) || !ReadString(bytes, at, parsedHeader.mapOrScene)
        || parsedHeader.schemaVersion != kSupportedSchemaVersion || parsedHeader.tickRate == 0U) {
        return false;
    }
    std::uint32_t frameCount = 0;
    if (!ReadU32(bytes, at, frameCount)) {
        return false;
    }
    if (frameCount > kMaxFrameCount) {
        return false;
    }

    std::vector<DemoFrameRecord> parsedFrames;
    parsedFrames.reserve(frameCount);
    std::uint32_t prevTick = 0;
    for (std::uint32_t i = 0; i < frameCount; ++i) {
        DemoFrameRecord frame{};
        if (!ReadU32(bytes, at, frame.tick) || !ReadBytes(bytes, at, frame.inputBytes) ||
            !ReadBytes(bytes, at, frame.authoritativeSnapshotBytes) ||
            !ReadU32(bytes, at, frame.deterministicChecksum)) {
            return false;
        }
        if (i > 0U && frame.tick <= prevTick) {
            return false;
        }
        prevTick = frame.tick;
        parsedFrames.push_back(std::move(frame));
    }
    if (at != bytes.size()) {
        return false;
    }
    header_ = std::move(parsedHeader);
    frames_ = std::move(parsedFrames);
    open_ = true;
    return true;
}

const DemoHeader& DemoReader::Header() const noexcept {
    return header_;
}

std::size_t DemoReader::FrameCount() const noexcept {
    return frames_.size();
}

std::optional<DemoFrameRecord> DemoReader::ReadFrame(const std::size_t index) const {
    if (!open_ || index >= frames_.size()) {
        return std::nullopt;
    }
    return frames_[index];
}

void DemoReader::Close() {
    open_ = false;
    header_ = {};
    frames_.clear();
}

bool DemoReader::IsOpen() const noexcept {
    return open_;
}

} // namespace ri::runtime
