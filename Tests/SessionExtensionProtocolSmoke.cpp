#include "SessionExtensionProtocol.h"

#include <cstdlib>
#include <string>
#include <string_view>

namespace {

[[nodiscard]] ri::core::SessionExtensionContract MakeContract() {
    ri::core::SessionExtensionContract contract{};
    contract.extensions.push_back({
        .id = "studio.foundation",
        .version = "1.2.3",
        .fingerprint = "fnv1a64:0123456789abcdef",
        .kind = ri::core::SessionExtensionKind::Integration,
        .reloadPolicy = ri::core::SessionExtensionReloadPolicy::FrameBoundary,
        .capabilities = {"render.material", "session.shared"},
    });
    if (!ri::core::NormalizeSessionExtensionContract(contract).valid) {
        return {};
    }
    return contract;
}

[[nodiscard]] bool OfferRoundTripIsBounded() {
    const ri::core::SessionExtensionContract expected = MakeContract();
    const auto encoded = ri::runtime::session_protocol::EncodeOffer(expected);
    if (!encoded.has_value() || encoded->channel != 0U || !encoded->reliable
        || !ri::runtime::session_protocol::IsControlPacket(*encoded)) {
        return false;
    }

    ri::core::SessionExtensionContract decoded{};
    if (!ri::runtime::session_protocol::DecodeOffer(*encoded, decoded)
        || !ri::core::SessionExtensionContractsMatch(expected, decoded)) {
        return false;
    }

    for (std::size_t size = 0U; size < encoded->payload.size(); ++size) {
        ri::runtime::NetPacket truncated = *encoded;
        truncated.payload.resize(size);
        if (ri::runtime::session_protocol::DecodeOffer(truncated, decoded)) {
            return false;
        }
    }

    ri::runtime::NetPacket wrongVersion = *encoded;
    wrongVersion.payload[1] = 0xFFU;
    return !ri::runtime::session_protocol::DecodeOffer(wrongVersion, decoded);
}

[[nodiscard]] bool AcknowledgementRoundTripIsBounded() {
    const ri::core::SessionExtensionContract contract = MakeContract();
    const auto encoded = ri::runtime::session_protocol::EncodeAcknowledgement(contract, true);
    if (!encoded.has_value() || !ri::runtime::session_protocol::IsControlPacket(*encoded)) {
        return false;
    }

    bool accepted = false;
    std::string fingerprint;
    if (!ri::runtime::session_protocol::DecodeAcknowledgement(*encoded, accepted, fingerprint)
        || !accepted
        || fingerprint != contract.fingerprint) {
        return false;
    }

    for (std::size_t size = 0U; size < encoded->payload.size(); ++size) {
        ri::runtime::NetPacket truncated = *encoded;
        truncated.payload.resize(size);
        if (ri::runtime::session_protocol::DecodeAcknowledgement(
                truncated, accepted, fingerprint)) {
            return false;
        }
    }

    ri::runtime::NetPacket invalidAccepted = *encoded;
    invalidAccepted.payload[2] = 2U;
    if (ri::runtime::session_protocol::DecodeAcknowledgement(
            invalidAccepted, accepted, fingerprint)) {
        return false;
    }

    ri::core::SessionExtensionContract oversized = contract;
    oversized.fingerprint.assign(513U, 'a');
    return !ri::runtime::session_protocol::EncodeAcknowledgement(oversized, true).has_value();
}

} // namespace

int main() {
    return OfferRoundTripIsBounded() && AcknowledgementRoundTripIsBounded()
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
