#include "RawIron/Core/SpscRingBuffer.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <thread>

int main() {
    // Capacity means usable values, including the smallest supported buffer.
    ri::core::SpscRingBuffer<int, 1U> single;
    if (single.CapacityValue() != 1U || !single.Empty() || single.Full()
        || !single.Push(17) || !single.Full() || single.Size() != 1U || single.Push(18)) {
        return EXIT_FAILURE;
    }
    const auto singleValue = single.Pop();
    if (!singleValue.has_value() || *singleValue != 17 || !single.Empty() || single.Full()) {
        return EXIT_FAILURE;
    }

    ri::core::SpscRingBuffer<int, 3U> wrapped;
    if (!wrapped.Push(1) || !wrapped.Push(2) || !wrapped.Push(3) || !wrapped.Full()
        || wrapped.Size() != 3U || wrapped.Pop() != 1 || !wrapped.Push(4)) {
        return EXIT_FAILURE;
    }
    if (wrapped.Pop() != 2 || wrapped.Pop() != 3 || wrapped.Pop() != 4
        || wrapped.Pop().has_value()) {
        return EXIT_FAILURE;
    }

    // Exercise publication, wraparound, and FIFO ordering under the intended one-producer /
    // one-consumer concurrency model.
    constexpr std::uint32_t kValueCount = 250'000U;
    ri::core::SpscRingBuffer<std::uint32_t, 1024U> concurrent;
    std::atomic<bool> failed{false};
    std::thread producer([&] {
        for (std::uint32_t value = 0; value < kValueCount; ++value) {
            while (!concurrent.Push(value)) {
                std::this_thread::yield();
            }
        }
    });
    std::thread consumer([&] {
        for (std::uint32_t expected = 0; expected < kValueCount; ++expected) {
            auto value = concurrent.Pop();
            while (!value.has_value()) {
                std::this_thread::yield();
                value = concurrent.Pop();
            }
            if (*value != expected) {
                failed.store(true, std::memory_order_relaxed);
                return;
            }
        }
    });
    producer.join();
    consumer.join();

    if (failed.load(std::memory_order_relaxed) || !concurrent.Empty()
        || concurrent.Size() != 0U) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
