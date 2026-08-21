#include "RawIron/Games/CubeTest/CubeTestAuthority.h"

#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/SessionExtensions.h"
#include "RawIron/World/InteractivePropReplication.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <string>

namespace ri::games::cubetest {
namespace {

constexpr std::uint32_t kSnapshotMagic = 0x41544352U; // "RCTA" little-endian.
constexpr std::uint16_t kSnapshotVersion = 1U;
constexpr std::uint8_t kProjectileCommand = 1U;
constexpr std::uint32_t kMaxCommandsPerAuthorityTick = 4U;

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
    return ReadU32(input, at, bits) && std::isfinite(value = std::bit_cast<float>(bits));
}

bool ReadVec3(const std::span<const std::uint8_t> input, std::size_t& at, ri::math::Vec3& value) {
    return ReadFloat(input, at, value.x) && ReadFloat(input, at, value.y) && ReadFloat(input, at, value.z);
}

bool ReadBlob(const std::span<const std::uint8_t> input,
              std::size_t& at,
              const std::uint32_t tick,
              ri::runtime::SnapshotBlob& blob) {
    std::uint32_t bytes = 0U;
    if (!ReadU32(input, at, bytes) || bytes > input.size() - at) return false;
    blob.tick = tick;
    blob.bytes.assign(input.begin() + static_cast<std::ptrdiff_t>(at),
                      input.begin() + static_cast<std::ptrdiff_t>(at + bytes));
    at += bytes;
    return true;
}

ri::runtime::NetMode ParseNetworkMode(const std::string_view value, bool& enabled) {
    enabled = true;
    if (value == "listen") return ri::runtime::NetMode::ListenHost;
    if (value == "dedicated") return ri::runtime::NetMode::Dedicated;
    if (value == "client") return ri::runtime::NetMode::ClientOnly;
    enabled = false;
    return ri::runtime::NetMode::ClientOnly;
}

} // namespace

CubeTestAuthorityBridge::CubeTestAuthorityBridge(CubeTestWorld* world) noexcept : world_(world) {}

void CubeTestAuthorityBridge::SetWorld(CubeTestWorld* world) noexcept {
    world_ = world;
    authorityTick_ = 0U;
    peerCommandBudgets_.clear();
}

std::optional<ri::runtime::SnapshotBlob> CubeTestAuthorityBridge::CaptureSnapshot(const std::uint32_t tick) {
    if (world_ == nullptr) return std::nullopt;
    const ri::runtime::SnapshotBlob interaction =
        ri::world::BuildInteractivePropSnapshot(world_->interactionProps, tick);
    const ri::runtime::SnapshotBlob projectiles =
        ri::world::BuildInteractivePropSnapshot(world_->projectileProps, tick);
    if (interaction.bytes.empty() || projectiles.bytes.empty()) return std::nullopt;
    ri::runtime::SnapshotBlob snapshot{.tick = tick};
    snapshot.bytes.reserve(16U + interaction.bytes.size() + projectiles.bytes.size());
    WriteU32(snapshot.bytes, kSnapshotMagic);
    WriteU16(snapshot.bytes, kSnapshotVersion);
    WriteU16(snapshot.bytes, 0U);
    WriteU32(snapshot.bytes, static_cast<std::uint32_t>(interaction.bytes.size()));
    snapshot.bytes.insert(snapshot.bytes.end(), interaction.bytes.begin(), interaction.bytes.end());
    WriteU32(snapshot.bytes, static_cast<std::uint32_t>(projectiles.bytes.size()));
    snapshot.bytes.insert(snapshot.bytes.end(), projectiles.bytes.begin(), projectiles.bytes.end());
    authorityTick_ = tick;
    return snapshot;
}

bool CubeTestAuthorityBridge::ApplySnapshot(const ri::runtime::SnapshotBlob& snapshot, std::string* error) {
    const auto fail = [error](const char* reason) {
        if (error != nullptr) *error = reason;
        return false;
    };
    if (world_ == nullptr) return fail("Cube Test authority world is unavailable.");
    const std::span<const std::uint8_t> bytes(snapshot.bytes);
    std::size_t at = 0U;
    std::uint32_t magic = 0U;
    std::uint16_t version = 0U;
    std::uint16_t flags = 0U;
    ri::runtime::SnapshotBlob interaction{};
    ri::runtime::SnapshotBlob projectiles{};
    if (!ReadU32(bytes, at, magic) || !ReadU16(bytes, at, version) || !ReadU16(bytes, at, flags)
        || magic != kSnapshotMagic || version != kSnapshotVersion || flags != 0U
        || !ReadBlob(bytes, at, snapshot.tick, interaction)
        || !ReadBlob(bytes, at, snapshot.tick, projectiles) || at != bytes.size()) {
        return fail("Cube Test authority snapshot is malformed.");
    }
    // Both constituent decoders are atomic; restore the first pool if the second rejects so a
    // corrupt combined snapshot never produces a half-updated world.
    const std::vector<ri::world::InteractivePropState> interactionBefore = world_->interactionProps;
    if (!ri::world::ApplyInteractivePropSnapshot(interaction, world_->interactionProps, error)) return false;
    if (!ri::world::ApplyInteractivePropSnapshot(projectiles, world_->projectileProps, error)) {
        world_->interactionProps = interactionBefore;
        return false;
    }
    if (error != nullptr) error->clear();
    return true;
}

bool CubeTestAuthorityBridge::HandleCommand(const std::size_t peerId,
                                            const std::uint32_t channel,
                                            const std::span<const std::uint8_t> payload,
                                            std::string* error) {
    const auto fail = [error](const char* reason) {
        if (error != nullptr) *error = reason;
        return false;
    };
    if (world_ == nullptr) return fail("Cube Test authority world is unavailable.");
    if (channel != 0U || payload.size() != 25U || payload[0] != kProjectileCommand) {
        return fail("Cube Test authority command is unsupported.");
    }
    PeerCommandBudget& budget = peerCommandBudgets_[peerId];
    if (budget.tick != authorityTick_) {
        budget.tick = authorityTick_;
        budget.accepted = 0U;
    }
    if (budget.accepted >= kMaxCommandsPerAuthorityTick) {
        return fail("Cube Test authority command rate limit reached.");
    }
    std::size_t at = 1U;
    ri::math::Vec3 origin{};
    ri::math::Vec3 direction{};
    if (!ReadVec3(payload, at, origin) || !ReadVec3(payload, at, direction) || at != payload.size()
        || ri::math::LengthSquared(direction) < 1.0e-6f) {
        return fail("Cube Test projectile command is invalid.");
    }
    direction = ri::math::Normalize(direction);
    if (origin.x < -256.0f || origin.x > 256.0f || origin.y < -32.0f || origin.y > 128.0f
        || origin.z < -256.0f || origin.z > 256.0f) {
        return fail("Cube Test projectile origin is outside the authority bounds.");
    }
    const ri::world::InteractivePropEmissionResult emitted = EmitCubeTestProjectile(*world_, origin, direction);
    if (emitted.propIndex < 0) return fail("Cube Test projectile pool rejected the command.");
    ++budget.accepted;
    if (error != nullptr) error->clear();
    return true;
}

std::vector<std::uint8_t> CubeTestAuthorityBridge::BuildProjectileCommand(
    const ri::math::Vec3& origin,
    const ri::math::Vec3& direction) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(25U);
    bytes.push_back(kProjectileCommand);
    for (const ri::math::Vec3 value : {origin, direction}) {
        WriteFloat(bytes, value.x);
        WriteFloat(bytes, value.y);
        WriteFloat(bytes, value.z);
    }
    return bytes;
}

ri::runtime::AuthoritativeNetConfig BuildCubeTestAuthorityConfig(
    const ri::core::CommandLine& commandLine,
    std::shared_ptr<CubeTestAuthorityBridge> bridge) {
    ri::runtime::AuthoritativeNetConfig config{};
    bool enabled = false;
    config.mode = ParseNetworkMode(commandLine.GetValue("--net-mode").value_or("offline"), enabled);
    config.enabled = enabled && !commandLine.HasFlag("--offline");
    config.bindEndpoint.host = commandLine.GetValue("--bind-host").value_or("0.0.0.0");
    config.bindEndpoint.port = static_cast<std::uint16_t>(
        std::clamp(commandLine.GetIntOr("--port", 27015), 1, 65535));
    config.connectEndpoint.host = commandLine.GetValue("--connect-host").value_or("127.0.0.1");
    config.connectEndpoint.port = static_cast<std::uint16_t>(
        std::clamp(commandLine.GetIntOr("--connect-port", 27015), 1, 65535));
    config.serverTickRate = std::clamp(commandLine.GetIntOr("--server-tick", 60), 20, 125);
    config.tickRate = config.serverTickRate;
    config.maxPeers = std::clamp(commandLine.GetIntOr("--max-peers", 8), 1, 32);
    config.rendezvousProvider = ri::runtime::RendezvousProviderKind::DirectToken;
    config.requireSessionExtensionAgreement = config.enabled;
    config.simulationBridge = std::move(bridge);
    config.sessionExtensionContract.extensions.push_back({
        .id = "rawiron.cubetest.authority",
        .version = "1.0.0",
        .fingerprint = "fnv1a64:cubetest-props-v1",
        .kind = ri::core::SessionExtensionKind::Gameplay,
        .reloadPolicy = ri::core::SessionExtensionReloadPolicy::SessionRestart,
        .capabilities = {"physics.projectiles", "physics.props", "xr.shared-authority"},
    });
    static_cast<void>(ri::core::NormalizeSessionExtensionContract(config.sessionExtensionContract));
    return config;
}

} // namespace ri::games::cubetest
