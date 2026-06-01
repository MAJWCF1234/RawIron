#pragma once

#include <cstdint>

namespace ri::runtime {

enum class NetMode : std::uint8_t {
    Dedicated = 0,
    ListenHost,
    HybridP2P,
    ClientOnly,
};

} // namespace ri::runtime

