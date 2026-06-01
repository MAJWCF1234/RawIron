#include "RawIron/Runtime/DemoStream.h"

#include <cstring>
#include <fstream>

namespace ri::runtime {
namespace {

void WriteU32(std::vector<std::uint8_t>& out, const std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((v >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((v >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((v >> 24U) & 0xFFU));
}

bool ReadU32(const std::vector<std::uint8_t>& in, std::size_t& at, std::uint32_t& out) {
    if (at + 4U > in.size()) {
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

bool ReadString(const std::vector<std::uint8_t>& in, std::size_t& at, std::string& value) {
    std::uint32_t len = 0;
    if (!ReadU32(in, at, len) || at + len > in.size()) {
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
    if (!ReadU32(in, at, len) || at + len > in.size()) {
        return false;
    }
    value.assign(in.begin() + static_cast<std::ptrdiff_t>(at), in.begin() + static_cast<std::ptrdiff_t>(at + len));
    at += len;
    return true;
}

} // namespace

bool DemoWriter::Open(const std::filesystem::path& path, const DemoHeader& header) {
    Close();
    path_ = path;
    stream_.clear();
    stream_.reserve(4096);
    stream_.insert(stream_.end(), {'R', 'I', 'D', 'E'});
    WriteU32(stream_, header.schemaVersion);
    WriteU32(stream_, header.tickRate);
    WriteU32(stream_, header.streamSeed);
    WriteString(stream_, header.runtimeId);
    WriteString(stream_, header.mapOrScene);
    WriteU32(stream_, 0U); // frame count placeholder
    open_ = true;
    return true;
}

bool DemoWriter::Append(const DemoFrameRecord& frame) {
    if (!open_) {
        return false;
    }
    if (!stream_.empty()) {
        // Enforce monotonically increasing ticks for deterministic playback ordering.
        // The stream metadata does not expose prior tick cheaply, so this is checked on decode too.
    }
    WriteU32(stream_, frame.tick);
    WriteBytes(stream_, frame.inputBytes);
    WriteBytes(stream_, frame.authoritativeSnapshotBytes);
    WriteU32(stream_, frame.deterministicChecksum);
    return true;
}

void DemoWriter::Close() {
    if (!open_) {
        return;
    }
    // Compute frame count by replaying payload quickly.
    std::size_t at = 0;
    if (stream_.size() >= 4U + 4U + 4U) {
        at = 4U + 4U + 4U;
        std::string s;
        (void)ReadString(stream_, at, s);
        (void)ReadString(stream_, at, s);
    }
    std::size_t frameCountPos = at;
    std::uint32_t frameCount = 0;
    at += 4U;
    while (at < stream_.size()) {
        std::uint32_t tick = 0;
        std::vector<std::uint8_t> blob;
        if (!ReadU32(stream_, at, tick) || !ReadBytes(stream_, at, blob) || !ReadBytes(stream_, at, blob) ||
            !ReadU32(stream_, at, tick)) {
            break;
        }
        ++frameCount;
    }
    stream_[frameCountPos + 0U] = static_cast<std::uint8_t>(frameCount & 0xFFU);
    stream_[frameCountPos + 1U] = static_cast<std::uint8_t>((frameCount >> 8U) & 0xFFU);
    stream_[frameCountPos + 2U] = static_cast<std::uint8_t>((frameCount >> 16U) & 0xFFU);
    stream_[frameCountPos + 3U] = static_cast<std::uint8_t>((frameCount >> 24U) & 0xFFU);

    std::ofstream file(path_, std::ios::binary | std::ios::trunc);
    if (file.good()) {
        file.write(reinterpret_cast<const char*>(stream_.data()), static_cast<std::streamsize>(stream_.size()));
    }
    open_ = false;
    stream_.clear();
}

bool DemoWriter::IsOpen() const noexcept {
    return open_;
}

bool DemoReader::Open(const std::filesystem::path& path) {
    Close();
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
        return false;
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytes.size() < 16U || std::memcmp(bytes.data(), "RIDE", 4) != 0) {
        return false;
    }

    std::size_t at = 4U;
    if (!ReadU32(bytes, at, header_.schemaVersion) || !ReadU32(bytes, at, header_.tickRate) ||
        !ReadU32(bytes, at, header_.streamSeed) ||
        !ReadString(bytes, at, header_.runtimeId) || !ReadString(bytes, at, header_.mapOrScene)) {
        return false;
    }
    std::uint32_t frameCount = 0;
    if (!ReadU32(bytes, at, frameCount)) {
        return false;
    }

    frames_.clear();
    frames_.reserve(frameCount);
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
        frames_.push_back(std::move(frame));
    }
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
