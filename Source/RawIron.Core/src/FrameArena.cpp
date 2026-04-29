#include "RawIron/Core/FrameArena.h"

#include <algorithm>
#include <limits>

namespace ri::core {
namespace {

bool IsPowerOfTwo(std::size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

std::size_t AlignUp(std::size_t value, std::size_t alignment) {
    const std::size_t add = alignment - 1;
    if (value > (std::numeric_limits<std::size_t>::max() - add)) {
        return std::numeric_limits<std::size_t>::max();
    }
    return (value + add) & ~(alignment - 1);
}

} // namespace

FrameArena::FrameArena(std::size_t bytes)
    : storage_(bytes) {}

std::size_t FrameArena::CapacityBytes() const noexcept {
    return storage_.size();
}

std::size_t FrameArena::UsedBytes() const noexcept {
    return cursor_;
}

std::size_t FrameArena::RemainingBytes() const noexcept {
    return storage_.size() - std::min(cursor_, storage_.size());
}

void FrameArena::Reset() noexcept {
    cursor_ = 0;
}

std::size_t FrameArena::Mark() const noexcept {
    return cursor_;
}

void FrameArena::Rewind(std::size_t mark) noexcept {
    cursor_ = std::min(mark, storage_.size());
}

void* FrameArena::Allocate(std::size_t bytes, std::size_t alignment) {
    if (bytes == 0) {
        return nullptr;
    }
    if (!IsPowerOfTwo(alignment)) {
        throw std::invalid_argument("FrameArena alignment must be a power of two.");
    }

    const std::size_t alignedOffset = AlignUp(cursor_, alignment);
    if (alignedOffset > storage_.size()) {
        throw std::bad_alloc{};
    }
    if (bytes > (storage_.size() - alignedOffset)) {
        throw std::bad_alloc{};
    }

    cursor_ = alignedOffset + bytes;
    return static_cast<void*>(storage_.data() + alignedOffset);
}

} // namespace ri::core
