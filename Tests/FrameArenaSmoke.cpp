#include "RawIron/Core/FrameArena.h"

#include <cstdint>
#include <cstdlib>
#include <stdexcept>

int main() {
    ri::core::FrameArena arena(128U);
    auto* const maxAligned = arena.AllocateArray<std::max_align_t>();
    if (maxAligned == nullptr
        || reinterpret_cast<std::uintptr_t>(maxAligned) % alignof(std::max_align_t) != 0U
        || arena.UsedBytes() != sizeof(std::max_align_t)) {
        return EXIT_FAILURE;
    }

    const std::size_t mark = arena.Mark();
    auto* const alignedBytes = static_cast<std::uint8_t*>(arena.Allocate(7U, alignof(std::uint64_t)));
    if (alignedBytes == nullptr
        || reinterpret_cast<std::uintptr_t>(alignedBytes) % alignof(std::uint64_t) != 0U
        || arena.UsedBytes() <= mark) {
        return EXIT_FAILURE;
    }
    arena.Rewind(mark);
    if (arena.UsedBytes() != mark) {
        return EXIT_FAILURE;
    }

    bool rejectedOverAlignment = false;
    try {
        (void)arena.Allocate(1U, alignof(std::max_align_t) * 2U);
    } catch (const std::invalid_argument&) {
        rejectedOverAlignment = true;
    }
    if (!rejectedOverAlignment) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
