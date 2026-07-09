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
    : storage_((bytes / sizeof(std::max_align_t)) + (bytes % sizeof(std::max_align_t) == 0 ? 0U : 1U)),
      capacityBytes_(bytes) {}

std::size_t FrameArena::CapacityBytes() const noexcept {
    return capacityBytes_;
}

std::size_t FrameArena::UsedBytes() const noexcept {
    return cursor_;
}

std::size_t FrameArena::RemainingBytes() const noexcept {
    return capacityBytes_ - std::min(cursor_, capacityBytes_);
}

void FrameArena::Reset() noexcept {
    cursor_ = 0;
}

std::size_t FrameArena::Mark() const noexcept {
    return cursor_;
}

void FrameArena::Rewind(std::size_t mark) noexcept {
    cursor_ = std::min(mark, capacityBytes_);
}

void* FrameArena::Allocate(std::size_t bytes, std::size_t alignment) {
    if (bytes == 0) {
        return nullptr;
    }
    if (!IsPowerOfTwo(alignment)) {
        throw std::invalid_argument("FrameArena alignment must be a power of two.");
    }
    if (alignment > alignof(std::max_align_t)) {
        throw std::invalid_argument("FrameArena alignment exceeds supported maximum alignment.");
    }

    const std::size_t alignedOffset = AlignUp(cursor_, alignment);
    if (alignedOffset > capacityBytes_) {
        throw std::bad_alloc{};
    }
    if (bytes > (capacityBytes_ - alignedOffset)) {
        throw std::bad_alloc{};
    }

    cursor_ = alignedOffset + bytes;
    auto* const base = reinterpret_cast<std::uint8_t*>(storage_.data());
    return static_cast<void*>(base + alignedOffset);
}

} // namespace ri::core
