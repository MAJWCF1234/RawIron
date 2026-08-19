#include "RawIron/Core/SessionExtensions.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace ri::core {
namespace {

constexpr std::size_t kMaxExtensions = 256U;
constexpr std::size_t kMaxCapabilitiesPerExtension = 128U;
constexpr std::size_t kMaxFieldBytes = 512U;
constexpr std::size_t kMaxWireBytes = 256U * 1024U;
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

[[nodiscard]] bool IsSafeToken(const std::string_view value) {
    if (value.empty() || value.size() > kMaxFieldBytes) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](const unsigned char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
            || (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '-';
    });
}

[[nodiscard]] bool IsSafeFingerprint(const std::string_view value) {
    if (value.empty() || value.size() > kMaxFieldBytes) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](const unsigned char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
            || (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '-' || ch == ':';
    });
}

[[nodiscard]] std::string FormatFingerprint(const std::uint64_t value) {
    std::ostringstream stream;
    stream << "fnv1a64:" << std::hex << std::nouppercase << std::setw(16) << std::setfill('0') << value;
    return stream.str();
}

void HashToken(std::uint64_t& hash, const std::string_view value) {
    for (const unsigned char byte : value) {
        hash ^= byte;
        hash *= kFnvPrime;
    }
    hash ^= 0xFFU;
    hash *= kFnvPrime;
}

[[nodiscard]] bool IsKnownKind(const SessionExtensionKind kind) noexcept {
    return kind <= SessionExtensionKind::Integration;
}

[[nodiscard]] bool IsKnownPolicy(const SessionExtensionReloadPolicy policy) noexcept {
    return policy <= SessionExtensionReloadPolicy::SimulationBoundary;
}

void WriteU32(std::vector<std::uint8_t>& out, const std::uint32_t value) {
    for (unsigned shift = 0U; shift < 32U; shift += 8U) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

[[nodiscard]] bool ReadU32(const std::string_view source, std::size_t& at, std::uint32_t& out) {
    if (source.size() - at < 4U) {
        return false;
    }
    out = static_cast<std::uint32_t>(static_cast<unsigned char>(source[at]))
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(source[at + 1U])) << 8U)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(source[at + 2U])) << 16U)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(source[at + 3U])) << 24U);
    at += 4U;
    return true;
}

void WriteString(std::vector<std::uint8_t>& out, const std::string_view value) {
    WriteU32(out, static_cast<std::uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

[[nodiscard]] bool ReadString(const std::string_view source, std::size_t& at, std::string& out) {
    std::uint32_t bytes = 0U;
    if (!ReadU32(source, at, bytes) || bytes > kMaxFieldBytes || bytes > source.size() - at) {
        return false;
    }
    out.assign(source.data() + at, bytes);
    at += bytes;
    return true;
}

} // namespace

std::string_view ToString(const SessionExtensionKind kind) noexcept {
    switch (kind) {
    case SessionExtensionKind::Data: return "data";
    case SessionExtensionKind::Mod: return "mod";
    case SessionExtensionKind::Plugin: return "plugin";
    case SessionExtensionKind::Gameplay: return "gameplay";
    case SessionExtensionKind::Integration: return "integration";
    }
    return "unknown";
}

std::string_view ToString(const SessionExtensionReloadPolicy policy) noexcept {
    switch (policy) {
    case SessionExtensionReloadPolicy::SessionRestart: return "session-restart";
    case SessionExtensionReloadPolicy::FrameBoundary: return "frame-boundary";
    case SessionExtensionReloadPolicy::SimulationBoundary: return "simulation-boundary";
    }
    return "unknown";
}

SessionExtensionValidationReport NormalizeSessionExtensionContract(SessionExtensionContract& contract) {
    SessionExtensionValidationReport report{};
    if (contract.protocolVersion != SessionExtensionContract::kProtocolVersion) {
        report.issues.push_back("unsupported session-extension protocol version");
    }
    if (contract.extensions.size() > kMaxExtensions) {
        report.issues.push_back("session-extension contract exceeds the extension limit");
    }
    for (SessionExtensionDescriptor& extension : contract.extensions) {
        std::sort(extension.capabilities.begin(), extension.capabilities.end());
        if (!IsSafeToken(extension.id) || !IsSafeToken(extension.version) || !IsSafeFingerprint(extension.fingerprint)) {
            report.issues.push_back("extension id, version, and fingerprint must be bounded portable values");
        }
        if (!IsKnownKind(extension.kind) || !IsKnownPolicy(extension.reloadPolicy)) {
            report.issues.push_back("extension kind or reload policy is invalid");
        }
        if (extension.capabilities.size() > kMaxCapabilitiesPerExtension) {
            report.issues.push_back("extension exceeds the capability limit");
        }
        if (std::adjacent_find(extension.capabilities.begin(), extension.capabilities.end()) != extension.capabilities.end()) {
            report.issues.push_back("extension capabilities must not contain duplicates");
        }
        for (const std::string& capability : extension.capabilities) {
            if (!IsSafeToken(capability)) {
                report.issues.push_back("extension capability is not a bounded portable token");
                break;
            }
        }
    }
    std::sort(contract.extensions.begin(), contract.extensions.end(), [](const auto& left, const auto& right) {
        return left.id < right.id;
    });
    for (std::size_t index = 1U; index < contract.extensions.size(); ++index) {
        if (contract.extensions[index - 1U].id == contract.extensions[index].id) {
            report.issues.push_back("session-extension contract contains duplicate extension ids");
            break;
        }
    }
    if (report.issues.empty()) {
        std::uint64_t hash = kFnvOffset;
        HashToken(hash, std::to_string(contract.protocolVersion));
        for (const SessionExtensionDescriptor& extension : contract.extensions) {
            HashToken(hash, extension.id);
            HashToken(hash, extension.version);
            HashToken(hash, extension.fingerprint);
            HashToken(hash, ToString(extension.kind));
            HashToken(hash, ToString(extension.reloadPolicy));
            for (const std::string& capability : extension.capabilities) {
                HashToken(hash, capability);
            }
        }
        contract.fingerprint = FormatFingerprint(hash);
    } else {
        contract.fingerprint.clear();
    }
    report.valid = report.issues.empty();
    return report;
}

bool SessionExtensionContractsMatch(const SessionExtensionContract& expected,
                                    const SessionExtensionContract& actual) noexcept {
    return !expected.fingerprint.empty() && expected.protocolVersion == actual.protocolVersion
        && expected.fingerprint == actual.fingerprint;
}

std::vector<std::uint8_t> SerializeSessionExtensionContract(const SessionExtensionContract& contract) {
    SessionExtensionContract normalized = contract;
    if (!NormalizeSessionExtensionContract(normalized).valid) {
        return {};
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(64U + normalized.extensions.size() * 96U);
    WriteU32(bytes, normalized.protocolVersion);
    WriteU32(bytes, static_cast<std::uint32_t>(normalized.extensions.size()));
    for (const SessionExtensionDescriptor& extension : normalized.extensions) {
        WriteString(bytes, extension.id);
        WriteString(bytes, extension.version);
        WriteString(bytes, extension.fingerprint);
        bytes.push_back(static_cast<std::uint8_t>(extension.kind));
        bytes.push_back(static_cast<std::uint8_t>(extension.reloadPolicy));
        WriteU32(bytes, static_cast<std::uint32_t>(extension.capabilities.size()));
        for (const std::string& capability : extension.capabilities) {
            WriteString(bytes, capability);
        }
    }
    return bytes.size() <= kMaxWireBytes ? bytes : std::vector<std::uint8_t>{};
}

bool DeserializeSessionExtensionContract(const std::string_view bytes, SessionExtensionContract& outContract) {
    if (bytes.empty() || bytes.size() > kMaxWireBytes) {
        return false;
    }
    SessionExtensionContract parsed{};
    std::size_t at = 0U;
    std::uint32_t count = 0U;
    if (!ReadU32(bytes, at, parsed.protocolVersion) || !ReadU32(bytes, at, count) || count > kMaxExtensions) {
        return false;
    }
    parsed.extensions.reserve(count);
    for (std::uint32_t extensionIndex = 0U; extensionIndex < count; ++extensionIndex) {
        SessionExtensionDescriptor extension{};
        if (!ReadString(bytes, at, extension.id) || !ReadString(bytes, at, extension.version)
            || !ReadString(bytes, at, extension.fingerprint) || at > bytes.size() - 2U) {
            return false;
        }
        extension.kind = static_cast<SessionExtensionKind>(static_cast<unsigned char>(bytes[at++]));
        extension.reloadPolicy = static_cast<SessionExtensionReloadPolicy>(static_cast<unsigned char>(bytes[at++]));
        std::uint32_t capabilityCount = 0U;
        if (!ReadU32(bytes, at, capabilityCount) || capabilityCount > kMaxCapabilitiesPerExtension) {
            return false;
        }
        extension.capabilities.reserve(capabilityCount);
        for (std::uint32_t capabilityIndex = 0U; capabilityIndex < capabilityCount; ++capabilityIndex) {
            std::string capability;
            if (!ReadString(bytes, at, capability)) {
                return false;
            }
            extension.capabilities.push_back(std::move(capability));
        }
        parsed.extensions.push_back(std::move(extension));
    }
    if (at != bytes.size() || !NormalizeSessionExtensionContract(parsed).valid) {
        return false;
    }
    outContract = std::move(parsed);
    return true;
}

bool SessionExtensionCoordinator::Stage(SessionExtensionContract proposed,
                                        const std::uint64_t currentTick,
                                        const std::uint64_t activationTick,
                                        std::vector<std::size_t> requiredPeers) {
    if (state_ == SessionExtensionActivationState::Staged || activationTick <= currentTick
        || !NormalizeSessionExtensionContract(proposed).valid) {
        return false;
    }
    std::sort(requiredPeers.begin(), requiredPeers.end());
    if (std::adjacent_find(requiredPeers.begin(), requiredPeers.end()) != requiredPeers.end()) {
        return false;
    }
    staged_ = std::move(proposed);
    acknowledgements_.clear();
    for (const std::size_t peerId : requiredPeers) {
        acknowledgements_.emplace(peerId, false);
    }
    activationTick_ = activationTick;
    state_ = SessionExtensionActivationState::Staged;
    return true;
}

bool SessionExtensionCoordinator::Acknowledge(const std::size_t peerId,
                                              const std::string_view contractFingerprint,
                                              const bool accepted) {
    if (state_ != SessionExtensionActivationState::Staged) {
        return false;
    }
    const auto peer = acknowledgements_.find(peerId);
    if (peer == acknowledgements_.end()) {
        return false;
    }
    if (!accepted || contractFingerprint != staged_.fingerprint) {
        state_ = SessionExtensionActivationState::Rejected;
        return false;
    }
    peer->second = true;
    return true;
}

bool SessionExtensionCoordinator::ActivateIfReady(const std::uint64_t currentTick) {
    if (state_ != SessionExtensionActivationState::Staged || currentTick < activationTick_) {
        return false;
    }
    if (std::any_of(acknowledgements_.begin(), acknowledgements_.end(), [](const auto& item) { return !item.second; })) {
        return false;
    }
    active_ = std::move(staged_);
    staged_ = {};
    acknowledgements_.clear();
    state_ = SessionExtensionActivationState::Activated;
    return true;
}

void SessionExtensionCoordinator::Cancel() noexcept {
    staged_ = {};
    acknowledgements_.clear();
    activationTick_ = 0U;
    state_ = SessionExtensionActivationState::Idle;
}

SessionExtensionActivationState SessionExtensionCoordinator::State() const noexcept {
    return state_;
}

const SessionExtensionContract& SessionExtensionCoordinator::ActiveContract() const noexcept {
    return active_;
}

const SessionExtensionContract* SessionExtensionCoordinator::StagedContract() const noexcept {
    return state_ == SessionExtensionActivationState::Staged ? &staged_ : nullptr;
}

std::uint64_t SessionExtensionCoordinator::ActivationTick() const noexcept {
    return activationTick_;
}

} // namespace ri::core
