#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace ri::core {

template <typename T, std::size_t Capacity>
class SpscRingBuffer {
    static_assert(Capacity > 0, "SpscRingBuffer capacity must be greater than zero.");
    static_assert(std::is_trivially_copyable_v<T>, "SpscRingBuffer requires trivially copyable value types.");

public:
    [[nodiscard]] constexpr std::size_t CapacityValue() const noexcept {
        return Capacity;
    }

    [[nodiscard]] std::size_t Size() const noexcept {
        const std::size_t head = head_.load(std::memory_order_acquire);
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        if (head >= tail) {
            return head - tail;
        }
        return Capacity - (tail - head);
    }

    [[nodiscard]] bool Empty() const noexcept {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool Full() const noexcept {
        const std::size_t head = head_.load(std::memory_order_acquire);
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        return Next(head) == tail;
    }

    bool Push(const T& value) noexcept {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t nextHead = Next(head);
        if (nextHead == tail_.load(std::memory_order_acquire)) {
            return false;
        }

        storage_[head] = value;
        head_.store(nextHead, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::optional<T> Pop() noexcept {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (head_.load(std::memory_order_acquire) == tail) {
            return std::nullopt;
        }

        const T value = storage_[tail];
        tail_.store(Next(tail), std::memory_order_release);
        return value;
    }

    [[nodiscard]] const T* Peek() const noexcept {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (head_.load(std::memory_order_acquire) == tail) {
            return nullptr;
        }
        return &storage_[tail];
    }

    void Clear() noexcept {
        head_.store(0, std::memory_order_release);
        tail_.store(0, std::memory_order_release);
    }

private:
    [[nodiscard]] static constexpr std::size_t Next(std::size_t index) noexcept {
        return (index + 1) % Capacity;
    }

    std::array<T, Capacity> storage_{};
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

} // namespace ri::core
