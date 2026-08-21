#include "RawIron/World/InteractivePropReplication.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace ri::world {
namespace {

constexpr std::uint32_t kMagic = 0x46504952U; // "RIPF" in little-endian bytes.
constexpr std::uint16_t kVersion = 1U;
constexpr std::size_t kMaximumProps = 4096U;

void WriteU16(std::vector<std::uint8_t>& output, const std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void WriteU32(std::vector<std::uint8_t>& output, const std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        output.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void WriteFloat(std::vector<std::uint8_t>& output, const float value) {
    WriteU32(output, std::bit_cast<std::uint32_t>(value));
}

bool ReadU16(const std::span<const std::uint8_t> input, std::size_t& at, std::uint16_t& value) {
    if (at > input.size() || input.size() - at < 2U) return false;
    value = static_cast<std::uint16_t>(input[at])
        | static_cast<std::uint16_t>(input[at + 1U]) << 8U;
    at += 2U;
    return true;
}

bool ReadU32(const std::span<const std::uint8_t> input, std::size_t& at, std::uint32_t& value) {
    if (at > input.size() || input.size() - at < 4U) return false;
    value = static_cast<std::uint32_t>(input[at])
        | static_cast<std::uint32_t>(input[at + 1U]) << 8U
        | static_cast<std::uint32_t>(input[at + 2U]) << 16U
        | static_cast<std::uint32_t>(input[at + 3U]) << 24U;
    at += 4U;
    return true;
}

bool ReadFloat(const std::span<const std::uint8_t> input, std::size_t& at, float& value) {
    std::uint32_t bits = 0U;
    if (!ReadU32(input, at, bits)) return false;
    value = std::bit_cast<float>(bits);
    return std::isfinite(value);
}

bool ReadVec3(const std::span<const std::uint8_t> input, std::size_t& at, ri::math::Vec3& value) {
    return ReadFloat(input, at, value.x)
        && ReadFloat(input, at, value.y)
        && ReadFloat(input, at, value.z);
}

} // namespace

ri::runtime::SnapshotBlob BuildInteractivePropSnapshot(
    const std::span<const InteractivePropState> props,
    const std::uint32_t tick) {
    ri::runtime::SnapshotBlob snapshot{.tick = tick};
    if (props.size() > kMaximumProps) return snapshot;
    snapshot.bytes.reserve(8U + props.size() * 49U);
    WriteU32(snapshot.bytes, kMagic);
    WriteU16(snapshot.bytes, kVersion);
    WriteU16(snapshot.bytes, static_cast<std::uint16_t>(props.size()));
    for (const InteractivePropState& prop : props) {
        snapshot.bytes.push_back(static_cast<std::uint8_t>(
            (prop.active ? 1U : 0U) | (prop.grabbed ? 2U : 0U)));
        WriteU32(snapshot.bytes, prop.owner);
        for (const ri::math::Vec3 value : {
                 prop.position, prop.velocity, prop.angularVelocityDegrees}) {
            WriteFloat(snapshot.bytes, value.x);
            WriteFloat(snapshot.bytes, value.y);
            WriteFloat(snapshot.bytes, value.z);
        }
        WriteFloat(snapshot.bytes, prop.ageSeconds);
        WriteFloat(snapshot.bytes, prop.lifetimeSeconds);
    }
    return snapshot;
}

bool ApplyInteractivePropSnapshot(
    const ri::runtime::SnapshotBlob& snapshot,
    const std::span<InteractivePropState> props,
    std::string* error) {
    const auto fail = [error](const char* reason) {
        if (error != nullptr) *error = reason;
        return false;
    };
    const std::span<const std::uint8_t> bytes(snapshot.bytes);
    std::size_t at = 0U;
    std::uint32_t magic = 0U;
    std::uint16_t version = 0U;
    std::uint16_t count = 0U;
    if (!ReadU32(bytes, at, magic) || !ReadU16(bytes, at, version)
        || !ReadU16(bytes, at, count) || magic != kMagic || version != kVersion) {
        return fail("Interactive prop snapshot header is invalid.");
    }
    if (count != props.size() || count > kMaximumProps) {
        return fail("Interactive prop snapshot pool contract does not match the local package.");
    }
    std::vector<InteractivePropState> decoded(props.begin(), props.end());
    for (InteractivePropState& prop : decoded) {
        if (at >= bytes.size()) return fail("Interactive prop snapshot is truncated.");
        const std::uint8_t flags = bytes[at++];
        if ((flags & ~3U) != 0U || !ReadU32(bytes, at, prop.owner)
            || !ReadVec3(bytes, at, prop.position)
            || !ReadVec3(bytes, at, prop.velocity)
            || !ReadVec3(bytes, at, prop.angularVelocityDegrees)
            || !ReadFloat(bytes, at, prop.ageSeconds)
            || !ReadFloat(bytes, at, prop.lifetimeSeconds)
            || prop.ageSeconds < 0.0f || prop.lifetimeSeconds < 0.0f) {
            return fail("Interactive prop snapshot contains invalid dynamic state.");
        }
        prop.active = (flags & 1U) != 0U;
        prop.grabbed = (flags & 2U) != 0U;
        if ((!prop.grabbed && prop.owner != 0U) || (prop.grabbed && prop.owner == 0U)) {
            return fail("Interactive prop snapshot contains inconsistent ownership.");
        }
    }
    if (at != bytes.size()) return fail("Interactive prop snapshot has trailing data.");
    for (std::size_t index = 0; index < props.size(); ++index) props[index] = std::move(decoded[index]);
    if (error != nullptr) error->clear();
    return true;
}

} // namespace ri::world
