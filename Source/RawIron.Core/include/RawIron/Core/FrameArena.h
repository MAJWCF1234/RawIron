#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace ri::core {

class FrameArena {
public:
    explicit FrameArena(std::size_t bytes);

    [[nodiscard]] std::size_t CapacityBytes() const noexcept;
    [[nodiscard]] std::size_t UsedBytes() const noexcept;
    [[nodiscard]] std::size_t RemainingBytes() const noexcept;

    void Reset() noexcept;
    [[nodiscard]] std::size_t Mark() const noexcept;
    void Rewind(std::size_t mark) noexcept;

    [[nodiscard]] void* Allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t));

    template <typename T>
    [[nodiscard]] T* AllocateArray(std::size_t count = 1) {
        static_assert(std::is_trivially_destructible_v<T>,
                      "FrameArena::AllocateArray requires trivially destructible types.");
        if (count == 0) {
            return nullptr;
        }
        constexpr std::size_t kMax = static_cast<std::size_t>(-1);
        if (sizeof(T) > 0 && count > (kMax / sizeof(T))) {
            throw std::bad_alloc{};
        }
        return static_cast<T*>(Allocate(sizeof(T) * count, alignof(T)));
    }

private:
    std::vector<std::uint8_t> storage_;
    std::size_t cursor_ = 0;
};

} // namespace ri::core
