#include "RawIron/Trace/BvhLifetimeRegistry.h"

namespace ri::trace {

void BvhLifetimeRegistry::MarkParticipating(std::string colliderId, bool usesBvhAcceleration) {
    if (colliderId.empty()) {
        return;
    }
    const bool wasPresent = participatingIds_.find(colliderId) != participatingIds_.end();
    if (!wasPresent) {
        participatingIds_.insert(colliderId);
    }
    const bool wasBvh = bvhEnabledById_[colliderId];
    bvhEnabledById_[colliderId] = usesBvhAcceleration;
    if (usesBvhAcceleration && !wasBvh) {
        ++bvhActiveCount_;
    } else if (!usesBvhAcceleration && wasBvh) {
        if (bvhActiveCount_ > 0U) {
            --bvhActiveCount_;
        }
    }
}

bool BvhLifetimeRegistry::ReleaseParticipating(const std::string_view colliderId) {
    if (colliderId.empty()) {
        return false;
    }
    const std::string key(colliderId);
    const auto it = participatingIds_.find(key);
    if (it == participatingIds_.end()) {
        return false;
    }
    participatingIds_.erase(it);
    const auto bvhIt = bvhEnabledById_.find(key);
    if (bvhIt != bvhEnabledById_.end() && bvhIt->second) {
        if (bvhActiveCount_ > 0U) {
            --bvhActiveCount_;
        }
    }
    bvhEnabledById_.erase(key);
    return true;
}

std::size_t BvhLifetimeRegistry::ActiveParticipationCount() const noexcept {
    return bvhActiveCount_;
}

bool BvhLifetimeRegistry::IsParticipating(const std::string_view colliderId) const {
    if (colliderId.empty()) {
        return false;
    }
    return participatingIds_.find(std::string(colliderId)) != participatingIds_.end();
}

void BvhLifetimeRegistry::Clear() {
    participatingIds_.clear();
    bvhEnabledById_.clear();
    bvhActiveCount_ = 0U;
}

} // namespace ri::trace
