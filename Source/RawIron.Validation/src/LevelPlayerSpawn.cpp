#include "RawIron/Validation/LevelPlayerSpawn.h"

#include <array>
#include <cctype>
#include <sstream>
#include <string_view>

namespace ri::validation {
namespace {

bool CiEquals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
        const char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
        if (ca != cb) {
            return false;
        }
    }
    return true;
}

} // namespace

std::size_t CountPlayerStartSpawners(std::span<const ri::structural::StructuralNode> spawners) {
    std::size_t count = 0;
    for (const ri::structural::StructuralNode& node : spawners) {
        if (CiEquals(node.type, "player_start")) {
            ++count;
        }
    }
    return count;
}

std::vector<PlayerStartCandidate> CollectPlayerStartSpawners(
    std::span<const ri::structural::StructuralNode> spawners) {
    std::vector<PlayerStartCandidate> out;
    for (const ri::structural::StructuralNode& node : spawners) {
        if (!CiEquals(node.type, "player_start")) {
            continue;
        }
        PlayerStartCandidate candidate{};
        candidate.id = node.id;
        candidate.name = node.name;
        candidate.feetPosition = node.position;
        candidate.rotationEulerDegrees = node.rotation;
        out.push_back(std::move(candidate));
    }
    return out;
}

std::optional<PlayerStartCandidate> TryGetPrimaryPlayerStart(
    std::span<const ri::structural::StructuralNode> spawners) {
    for (const ri::structural::StructuralNode& node : spawners) {
        if (!CiEquals(node.type, "player_start")) {
            continue;
        }
        PlayerStartCandidate candidate{};
        candidate.id = node.id;
        candidate.name = node.name;
        candidate.feetPosition = node.position;
        candidate.rotationEulerDegrees = node.rotation;
        return candidate;
    }
    return std::nullopt;
}

std::optional<std::string> ValidateAtMostOnePlayerStart(std::span<const ri::structural::StructuralNode> spawners,
                                                        std::string_view contextLabel) {
    const std::size_t n = CountPlayerStartSpawners(spawners);
    if (n <= 1) {
        return std::nullopt;
    }
    std::ostringstream message;
    message << contextLabel << " contains " << n << " player_start spawners; expected at most one.";
    return message.str();
}

std::span<const ri::math::Vec3> PlayerSpawnRecoveryOffsets() noexcept {
    static constexpr std::array<ri::math::Vec3, 13> kOffsets = {
        ri::math::Vec3{0.0f, 0.0f, 0.0f},
        ri::math::Vec3{0.75f, 0.0f, 0.0f},
        ri::math::Vec3{-0.75f, 0.0f, 0.0f},
        ri::math::Vec3{0.0f, 0.0f, 0.75f},
        ri::math::Vec3{0.0f, 0.0f, -0.75f},
        ri::math::Vec3{1.5f, 0.0f, 0.0f},
        ri::math::Vec3{-1.5f, 0.0f, 0.0f},
        ri::math::Vec3{0.0f, 0.0f, 1.5f},
        ri::math::Vec3{0.0f, 0.0f, -1.5f},
        ri::math::Vec3{0.0f, 0.35f, 0.0f},
        ri::math::Vec3{0.0f, 1.05f, 0.0f},
        ri::math::Vec3{0.0f, 2.1f, 0.0f},
        ri::math::Vec3{0.0f, 3.5f, 0.0f},
    };
    return {kOffsets.data(), kOffsets.size()};
}

} // namespace ri::validation
