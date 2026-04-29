#include "RawIron/Core/InputPolling.h"

#include <algorithm>

namespace ri::core {

void InputPollingBuffer::BeginFrame(std::uint64_t timestampMicros) noexcept {
    if (frameActive_) {
        (void)EndFrame(timestampMicros);
    }
    frameBeginTimestampMicros_ = timestampMicros;
    frameSampleCount_ = 0;
    frameActive_ = true;
}

bool InputPollingBuffer::PushSample(const InputSample& sample) noexcept {
    if (!samples_.Push(sample)) {
        ++droppedSampleCount_;
        return false;
    }
    if (frameActive_) {
        ++frameSampleCount_;
    }
    return true;
}

bool InputPollingBuffer::EndFrame(std::uint64_t timestampMicros) noexcept {
    if (!frameActive_) {
        return false;
    }

    const std::uint64_t endTimestampMicros = std::max(timestampMicros, frameBeginTimestampMicros_);
    const InputPollingFrame frame{
        .frameId = nextFrameId_++,
        .beginTimestampMicros = frameBeginTimestampMicros_,
        .endTimestampMicros = endTimestampMicros,
        .sampleCount = frameSampleCount_,
    };
    frameActive_ = false;
    frameSampleCount_ = 0;

    if (!frames_.Push(frame)) {
        ++droppedFrameCount_;
        return false;
    }
    return true;
}

std::optional<InputSample> InputPollingBuffer::PopSample() noexcept {
    return samples_.Pop();
}

std::optional<InputPollingFrame> InputPollingBuffer::PopFrame() noexcept {
    return frames_.Pop();
}

bool InputPollingBuffer::FrameActive() const noexcept {
    return frameActive_;
}

std::size_t InputPollingBuffer::PendingSampleCount() const noexcept {
    return samples_.Size();
}

std::size_t InputPollingBuffer::PendingFrameCount() const noexcept {
    return frames_.Size();
}

std::uint64_t InputPollingBuffer::DroppedSampleCount() const noexcept {
    return droppedSampleCount_;
}

std::uint64_t InputPollingBuffer::DroppedFrameCount() const noexcept {
    return droppedFrameCount_;
}

void InputPollingBuffer::Reset() noexcept {
    samples_.Clear();
    frames_.Clear();
    nextFrameId_ = 1;
    frameBeginTimestampMicros_ = 0;
    frameSampleCount_ = 0;
    frameActive_ = false;
    droppedSampleCount_ = 0;
    droppedFrameCount_ = 0;
}

} // namespace ri::core
