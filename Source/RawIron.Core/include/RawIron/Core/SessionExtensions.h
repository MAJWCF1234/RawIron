#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::core {

/// The session-visible role of an extension. A package can contain any of these.
enum class SessionExtensionKind : std::uint8_t {
    Data = 0,
    Mod,
    Plugin,
    Gameplay,
    Integration,
};

/// Runtime policy advertised with an extension. This is a safety declaration, not a sandbox.
enum class SessionExtensionReloadPolicy : std::uint8_t {
    SessionRestart = 0,
    FrameBoundary,
    SimulationBoundary,
};

struct SessionExtensionDescriptor {
    std::string id{};
    std::string version{};
    /// Exact, reproducible package/content fingerprint. It is an identity check, not publisher trust.
    std::string fingerprint{};
    SessionExtensionKind kind = SessionExtensionKind::Data;
    SessionExtensionReloadPolicy reloadPolicy = SessionExtensionReloadPolicy::SessionRestart;
    std::vector<std::string> capabilities{};
};

struct SessionExtensionContract {
    static constexpr std::uint32_t kProtocolVersion = 1U;

    std::uint32_t protocolVersion = kProtocolVersion;
    std::vector<SessionExtensionDescriptor> extensions{};
    /// Computed from the canonical extension list. Never accept a caller-supplied value as proof.
    std::string fingerprint{};
};

struct SessionExtensionValidationReport {
    bool valid = false;
    std::vector<std::string> issues{};
};

enum class SessionExtensionActivationState : std::uint8_t {
    Idle = 0,
    Staged,
    Rejected,
    Activated,
};

/// Game-agnostic coordination for a proposed live extension set. Package loading and state migration
/// are deliberately owned by the host/game; this class establishes the unanimous contract and tick boundary.
class SessionExtensionCoordinator final {
public:
    [[nodiscard]] bool Stage(SessionExtensionContract proposed,
                             std::uint64_t currentTick,
                             std::uint64_t activationTick,
                             std::vector<std::size_t> requiredPeers);
    [[nodiscard]] bool Acknowledge(std::size_t peerId,
                                   std::string_view contractFingerprint,
                                   bool accepted);
    [[nodiscard]] bool ActivateIfReady(std::uint64_t currentTick);
    void Cancel() noexcept;

    [[nodiscard]] SessionExtensionActivationState State() const noexcept;
    [[nodiscard]] const SessionExtensionContract& ActiveContract() const noexcept;
    [[nodiscard]] const SessionExtensionContract* StagedContract() const noexcept;
    [[nodiscard]] std::uint64_t ActivationTick() const noexcept;

private:
    SessionExtensionContract active_{};
    SessionExtensionContract staged_{};
    std::unordered_map<std::size_t, bool> acknowledgements_{};
    std::uint64_t activationTick_ = 0U;
    SessionExtensionActivationState state_ = SessionExtensionActivationState::Idle;
};

/// Normalizes ordering, rejects ambiguous descriptors, and computes the contract fingerprint.
[[nodiscard]] SessionExtensionValidationReport NormalizeSessionExtensionContract(
    SessionExtensionContract& contract);

[[nodiscard]] bool SessionExtensionContractsMatch(const SessionExtensionContract& expected,
                                                  const SessionExtensionContract& actual) noexcept;

/// Stable bounded wire representation for session negotiation. It carries no paths or executable bytes.
[[nodiscard]] std::vector<std::uint8_t> SerializeSessionExtensionContract(
    const SessionExtensionContract& contract);
[[nodiscard]] bool DeserializeSessionExtensionContract(std::string_view bytes,
                                                        SessionExtensionContract& outContract);

[[nodiscard]] std::string_view ToString(SessionExtensionKind kind) noexcept;
[[nodiscard]] std::string_view ToString(SessionExtensionReloadPolicy policy) noexcept;

} // namespace ri::core
