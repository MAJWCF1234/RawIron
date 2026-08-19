#pragma once

#include "RawIron/Core/SessionExtensions.h"
#include "RawIron/Runtime/NetTransport.h"

#include <optional>
#include <string>

namespace ri::runtime::session_protocol {

[[nodiscard]] std::optional<NetPacket> EncodeOffer(
    const ri::core::SessionExtensionContract& contract);

[[nodiscard]] bool DecodeOffer(
    const NetPacket& packet,
    ri::core::SessionExtensionContract& contract);

[[nodiscard]] std::optional<NetPacket> EncodeAcknowledgement(
    const ri::core::SessionExtensionContract& contract,
    bool accepted);

[[nodiscard]] bool DecodeAcknowledgement(
    const NetPacket& packet,
    bool& accepted,
    std::string& fingerprint);

[[nodiscard]] bool IsControlPacket(const NetPacket& packet) noexcept;

} // namespace ri::runtime::session_protocol
