#include "RawIron/Core/SessionExtensions.h"

#include <cstdlib>
#include <string>

namespace {

ri::core::SessionExtensionContract MakeContract() {
    ri::core::SessionExtensionContract contract{};
    contract.extensions = {
        {
            .id = "studio.physics-slime",
            .version = "1.4.0",
            .fingerprint = "fnv1a64:0123456789abcdef",
            .kind = ri::core::SessionExtensionKind::Gameplay,
            .reloadPolicy = ri::core::SessionExtensionReloadPolicy::SimulationBoundary,
            .capabilities = {"physics.slime", "world.spawn"},
        },
        {
            .id = "studio.midi-bridge",
            .version = "2.0.0",
            .fingerprint = "fnv1a64:fedcba9876543210",
            .kind = ri::core::SessionExtensionKind::Integration,
            .reloadPolicy = ri::core::SessionExtensionReloadPolicy::FrameBoundary,
            .capabilities = {"input.midi"},
        },
    };
    return contract;
}

} // namespace

int main() {
    ri::core::SessionExtensionContract contract = MakeContract();
    if (!ri::core::NormalizeSessionExtensionContract(contract).valid || contract.fingerprint.empty()) {
        return EXIT_FAILURE;
    }
    // Canonical sorting makes authored ordering irrelevant.
    if (contract.extensions.front().id != "studio.midi-bridge") {
        return EXIT_FAILURE;
    }
    const std::vector<std::uint8_t> bytes = ri::core::SerializeSessionExtensionContract(contract);
    ri::core::SessionExtensionContract decoded{};
    if (bytes.empty() || !ri::core::DeserializeSessionExtensionContract(
                             std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()), decoded)
        || !ri::core::SessionExtensionContractsMatch(contract, decoded)) {
        return EXIT_FAILURE;
    }
    decoded.extensions.front().version = "2.0.1";
    if (!ri::core::NormalizeSessionExtensionContract(decoded).valid
        || ri::core::SessionExtensionContractsMatch(contract, decoded)) {
        return EXIT_FAILURE;
    }
    ri::core::SessionExtensionContract malformed = MakeContract();
    malformed.extensions.push_back(malformed.extensions.front());
    if (ri::core::NormalizeSessionExtensionContract(malformed).valid || !malformed.fingerprint.empty()) {
        return EXIT_FAILURE;
    }
    ri::core::SessionExtensionCoordinator coordinator{};
    if (!coordinator.Stage(contract, 100U, 120U, {4U, 9U})
        || coordinator.ActivateIfReady(120U)
        || !coordinator.Acknowledge(4U, contract.fingerprint, true)
        || coordinator.Acknowledge(9U, "fnv1a64:wrong", true)
        || coordinator.State() != ri::core::SessionExtensionActivationState::Rejected) {
        return EXIT_FAILURE;
    }
    coordinator.Cancel();
    if (!coordinator.Stage(contract, 100U, 120U, {4U, 9U})
        || !coordinator.Acknowledge(4U, contract.fingerprint, true)
        || !coordinator.Acknowledge(9U, contract.fingerprint, true)
        || coordinator.ActivateIfReady(119U)
        || !coordinator.ActivateIfReady(120U)
        || coordinator.State() != ri::core::SessionExtensionActivationState::Activated
        || !ri::core::SessionExtensionContractsMatch(contract, coordinator.ActiveContract())) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
