#include "SessionExtensionProtocol.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace ri::runtime::session_protocol {
namespace {

constexpr std::uint8_t kOfferMarker = 0xB9U;
constexpr std::uint8_t kAcknowledgementMarker = 0xBAU;
constexpr std::uint8_t kProtocolVersion = 1U;
constexpr std::size_t kOfferHeaderSize = 2U;
constexpr std::size_t kAcknowledgementHeaderSize = 7U;
constexpr std::size_t kMaxFingerprintBytes = 512U;

void WriteU32(std::vector<std::uint8_t>& out, const std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

[[nodiscard]] bool ReadU32(
    const std::vector<std::uint8_t>& in,
    const std::size_t offset,
    std::uint32_t& value) noexcept {
    if (offset > in.size() || in.size() - offset < 4U) {
        return false;
    }
    value = static_cast<std::uint32_t>(in[offset]) |
            (static_cast<std::uint32_t>(in[offset + 1U]) << 8U) |
            (static_cast<std::uint32_t>(in[offset + 2U]) << 16U) |
            (static_cast<std::uint32_t>(in[offset + 3U]) << 24U);
    return true;
}

} // namespace

std::optional<NetPacket> EncodeOffer(const ri::core::SessionExtensionContract& contract) {
    const std::vector<std::uint8_t> serialized =
        ri::core::SerializeSessionExtensionContract(contract);
    if (serialized.empty() || serialized.size() > kMaxNetPacketPayloadBytes - kOfferHeaderSize) {
        return std::nullopt;
    }

    NetPacket packet{};
    packet.channel = 0U;
    packet.reliable = true;
    packet.payload.reserve(kOfferHeaderSize + serialized.size());
    packet.payload.push_back(kOfferMarker);
    packet.payload.push_back(kProtocolVersion);
    packet.payload.insert(packet.payload.end(), serialized.begin(), serialized.end());
    return packet;
}

bool DecodeOffer(const NetPacket& packet, ri::core::SessionExtensionContract& contract) {
    if (packet.payload.size() <= kOfferHeaderSize
        || packet.payload[0] != kOfferMarker
        || packet.payload[1] != kProtocolVersion) {
        return false;
    }

    const auto* first = reinterpret_cast<const char*>(packet.payload.data() + kOfferHeaderSize);
    return ri::core::DeserializeSessionExtensionContract(
        std::string_view(first, packet.payload.size() - kOfferHeaderSize), contract);
}

std::optional<NetPacket> EncodeAcknowledgement(
    const ri::core::SessionExtensionContract& contract,
    const bool accepted) {
    if (contract.fingerprint.empty() || contract.fingerprint.size() > kMaxFingerprintBytes) {
        return std::nullopt;
    }

    NetPacket packet{};
    packet.channel = 0U;
    packet.reliable = true;
    packet.payload.reserve(kAcknowledgementHeaderSize + contract.fingerprint.size());
    packet.payload.push_back(kAcknowledgementMarker);
    packet.payload.push_back(kProtocolVersion);
    packet.payload.push_back(accepted ? 1U : 0U);
    WriteU32(packet.payload, static_cast<std::uint32_t>(contract.fingerprint.size()));
    packet.payload.insert(packet.payload.end(), contract.fingerprint.begin(), contract.fingerprint.end());
    return packet;
}

bool DecodeAcknowledgement(
    const NetPacket& packet,
    bool& accepted,
    std::string& fingerprint) {
    if (packet.payload.size() < kAcknowledgementHeaderSize
        || packet.payload[0] != kAcknowledgementMarker
        || packet.payload[1] != kProtocolVersion
        || packet.payload[2] > 1U) {
        return false;
    }

    std::uint32_t length = 0U;
    if (!ReadU32(packet.payload, 3U, length)
        || length == 0U
        || length > kMaxFingerprintBytes
        || static_cast<std::size_t>(length)
            != packet.payload.size() - kAcknowledgementHeaderSize) {
        return false;
    }

    accepted = packet.payload[2] == 1U;
    fingerprint.assign(
        reinterpret_cast<const char*>(packet.payload.data() + kAcknowledgementHeaderSize), length);
    return true;
}

bool IsControlPacket(const NetPacket& packet) noexcept {
    return packet.payload.size() >= kOfferHeaderSize
        && (packet.payload[0] == kOfferMarker
            || packet.payload[0] == kAcknowledgementMarker);
}

} // namespace ri::runtime::session_protocol
