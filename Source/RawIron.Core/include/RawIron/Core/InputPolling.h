#pragma once

#include "RawIron/Core/SpscRingBuffer.h"

#include <cstdint>
#include <optional>

namespace ri::core {

enum class InputSampleType : std::uint8_t {
    Digital = 0,
    Analog = 1,
};

struct InputSample {
    std::uint64_t timestampMicros = 0;
    std::uint32_t deviceId = 0;
    std::uint16_t controlCode = 0;
    InputSampleType type = InputSampleType::Digital;
    float value = 0.0f;
};

struct InputPollingFrame {
    std::uint64_t frameId = 0;
    std::uint64_t beginTimestampMicros = 0;
    std::uint64_t endTimestampMicros = 0;
    std::uint32_t sampleCount = 0;
};

class InputPollingBuffer {
public:
    static constexpr std::size_t kSampleCapacity = 4096;
    static constexpr std::size_t kFrameCapacity = 256;

    void BeginFrame(std::uint64_t timestampMicros) noexcept;
    bool PushSample(const InputSample& sample) noexcept;
    bool EndFrame(std::uint64_t timestampMicros) noexcept;

    [[nodiscard]] std::optional<InputSample> PopSample() noexcept;
    [[nodiscard]] std::optional<InputPollingFrame> PopFrame() noexcept;

    [[nodiscard]] bool FrameActive() const noexcept;
    [[nodiscard]] std::size_t PendingSampleCount() const noexcept;
    [[nodiscard]] std::size_t PendingFrameCount() const noexcept;
    [[nodiscard]] std::uint64_t DroppedSampleCount() const noexcept;
    [[nodiscard]] std::uint64_t DroppedFrameCount() const noexcept;

    void Reset() noexcept;

private:
    SpscRingBuffer<InputSample, kSampleCapacity> samples_;
    SpscRingBuffer<InputPollingFrame, kFrameCapacity> frames_;
    std::uint64_t nextFrameId_ = 1;
    std::uint64_t frameBeginTimestampMicros_ = 0;
    std::uint32_t frameSampleCount_ = 0;
    bool frameActive_ = false;
    std::uint64_t droppedSampleCount_ = 0;
    std::uint64_t droppedFrameCount_ = 0;
};

} // namespace ri::core
