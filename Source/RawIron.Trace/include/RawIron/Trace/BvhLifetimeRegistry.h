#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace ri::trace {

/// Tracks mesh/collider entries that participate in BVH-style acceleration so teardown can
/// decrement global counters and clear participation flags without silent drift.
class BvhLifetimeRegistry {
public:
    void MarkParticipating(std::string colliderId, bool usesBvhAcceleration = true);
    [[nodiscard]] bool ReleaseParticipating(std::string_view colliderId);
    [[nodiscard]] std::size_t ActiveParticipationCount() const noexcept;
    [[nodiscard]] bool IsParticipating(std::string_view colliderId) const;
    void Clear();

private:
    std::unordered_set<std::string> participatingIds_{};
    std::unordered_map<std::string, bool> bvhEnabledById_{};
    std::size_t bvhActiveCount_ = 0U;
};

} // namespace ri::trace
