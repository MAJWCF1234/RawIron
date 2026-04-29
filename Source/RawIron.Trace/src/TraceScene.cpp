#include "RawIron/Trace/TraceScene.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_set>

namespace ri::trace {
namespace {

ri::spatial::Aabb TranslateBox(const ri::spatial::Aabb& box, const ri::math::Vec3& delta) {
    if (ri::spatial::IsEmpty(box)) {
        return box;
    }
    return ri::spatial::Aabb{
        .min = box.min + delta,
        .max = box.max + delta,
    };
}

ri::spatial::Aabb IntersectionBox(const ri::spatial::Aabb& lhs, const ri::spatial::Aabb& rhs) {
    if (!ri::spatial::Intersects(lhs, rhs)) {
        return ri::spatial::MakeEmptyAabb();
    }
    return ri::spatial::Aabb{
        .min = {
            std::max(lhs.min.x, rhs.min.x),
            std::max(lhs.min.y, rhs.min.y),
            std::max(lhs.min.z, rhs.min.z),
        },
        .max = {
            std::min(lhs.max.x, rhs.max.x),
            std::min(lhs.max.y, rhs.max.y),
            std::min(lhs.max.z, rhs.max.z),
        },
    };
}

float ClampFloat(float value, float minValue, float maxValue) {
    return std::max(minValue, std::min(maxValue, value));
}

std::optional<TraceHit> ComputeTraceBoxHit(const ri::spatial::Aabb& queryBox,
                                           const TraceCollider& collider) {
    const ri::spatial::Aabb intersection = IntersectionBox(queryBox, collider.bounds);
    if (ri::spatial::IsEmpty(intersection)) {
        return std::nullopt;
    }

    const ri::math::Vec3 size = ri::spatial::Size(intersection);
    if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
        return std::nullopt;
    }

    const ri::math::Vec3 queryCenter = ri::spatial::Center(queryBox);
    const ri::math::Vec3 colliderCenter = ri::spatial::Center(collider.bounds);
    const ri::math::Vec3 deltaCenter = queryCenter - colliderCenter;

    char axis = 'x';
    float penetration = size.x;
    ri::math::Vec3 normal{(deltaCenter.x >= 0.0f ? 1.0f : -1.0f), 0.0f, 0.0f};

    if (size.y < penetration) {
        axis = 'y';
        penetration = size.y;
        normal = {0.0f, (deltaCenter.y >= 0.0f ? 1.0f : -1.0f), 0.0f};
    }
    if (size.z < penetration) {
        axis = 'z';
        penetration = size.z;
        normal = {0.0f, 0.0f, (deltaCenter.z >= 0.0f ? 1.0f : -1.0f)};
    }

    ri::math::Vec3 point = queryCenter;
    if (axis == 'x') {
        point.x = normal.x > 0.0f ? collider.bounds.max.x : collider.bounds.min.x;
        point.y = ClampFloat(queryCenter.y, collider.bounds.min.y, collider.bounds.max.y);
        point.z = ClampFloat(queryCenter.z, collider.bounds.min.z, collider.bounds.max.z);
    } else if (axis == 'y') {
        point.y = normal.y > 0.0f ? collider.bounds.max.y : collider.bounds.min.y;
        point.x = ClampFloat(queryCenter.x, collider.bounds.min.x, collider.bounds.max.x);
        point.z = ClampFloat(queryCenter.z, collider.bounds.min.z, collider.bounds.max.z);
    } else {
        point.z = normal.z > 0.0f ? collider.bounds.max.z : collider.bounds.min.z;
        point.x = ClampFloat(queryCenter.x, collider.bounds.min.x, collider.bounds.max.x);
        point.y = ClampFloat(queryCenter.y, collider.bounds.min.y, collider.bounds.max.y);
    }

    return TraceHit{
        .id = collider.id,
        .bounds = collider.bounds,
        .point = point,
        .normal = normal,
        .penetration = penetration,
        .time = 0.0f,
        .endBox = queryBox,
    };
}

std::optional<TraceHit> ComputeRayHit(const ri::math::Vec3& origin,
                                      const ri::math::Vec3& direction,
                                      float far,
                                      const TraceCollider& collider) {
    if (ri::spatial::IsEmpty(collider.bounds) || ri::math::LengthSquared(direction) < 1e-20f || !std::isfinite(far) || far <= 0.0f) {
        return std::nullopt;
    }

    const ri::math::Vec3 dir = ri::math::Normalize(direction);
    float tMin = 0.0f;
    float tMax = far;
    char hitAxis = 'x';
    float hitSign = 1.0f;

    auto updateAxis = [&](float originComponent, float dirComponent, float minValue, float maxValue, char axisName) {
        if (std::fabs(dirComponent) <= 1e-8f) {
            return originComponent >= minValue && originComponent <= maxValue;
        }
        const float invDir = 1.0f / dirComponent;
        float t0 = (minValue - originComponent) * invDir;
        float t1 = (maxValue - originComponent) * invDir;
        float entrySign = dirComponent > 0.0f ? -1.0f : 1.0f;
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        if (t0 > tMin) {
            tMin = t0;
            hitAxis = axisName;
            hitSign = entrySign;
        }
        tMax = std::min(tMax, t1);
        return tMax >= tMin;
    };

    if (!updateAxis(origin.x, dir.x, collider.bounds.min.x, collider.bounds.max.x, 'x')
        || !updateAxis(origin.y, dir.y, collider.bounds.min.y, collider.bounds.max.y, 'y')
        || !updateAxis(origin.z, dir.z, collider.bounds.min.z, collider.bounds.max.z, 'z')) {
        return std::nullopt;
    }

    if (tMin > far) {
        return std::nullopt;
    }

    const ri::math::Vec3 point = origin + (dir * tMin);
    ri::math::Vec3 normal{};
    if (hitAxis == 'x') {
        normal = {hitSign, 0.0f, 0.0f};
    } else if (hitAxis == 'y') {
        normal = {0.0f, hitSign, 0.0f};
    } else {
        normal = {0.0f, 0.0f, hitSign};
    }

    return TraceHit{
        .id = collider.id,
        .bounds = collider.bounds,
        .point = point,
        .normal = normal,
        .penetration = 0.0f,
        .time = tMin,
        .endBox = ri::spatial::MakeEmptyAabb(),
    };
}

std::optional<TraceHit> ComputeSweptBoxHit(const ri::spatial::Aabb& queryBox,
                                           const ri::math::Vec3& delta,
                                           const TraceCollider& collider) {
    if (ri::spatial::Intersects(queryBox, collider.bounds)) {
        std::optional<TraceHit> overlap = ComputeTraceBoxHit(queryBox, collider);
        if (!overlap.has_value()) {
            return std::nullopt;
        }
        overlap->time = 0.0f;
        overlap->endBox = queryBox;
        return overlap;
    }

    float entryTime = -std::numeric_limits<float>::infinity();
    float exitTime = std::numeric_limits<float>::infinity();
    char hitAxis = 'x';
    float hitSign = 0.0f;

    auto updateAxis = [&](char axis,
                          float boxMin,
                          float boxMax,
                          float staticMin,
                          float staticMax,
                          float velocity) {
        if (std::fabs(velocity) < 1e-8f) {
            return !(boxMax <= staticMin || boxMin >= staticMax);
        }

        const float invVelocity = 1.0f / velocity;
        float axisEntry = velocity > 0.0f
            ? (staticMin - boxMax) * invVelocity
            : (staticMax - boxMin) * invVelocity;
        float axisExit = velocity > 0.0f
            ? (staticMax - boxMin) * invVelocity
            : (staticMin - boxMax) * invVelocity;
        if (axisEntry > axisExit) {
            std::swap(axisEntry, axisExit);
        }

        if (axisEntry > entryTime) {
            entryTime = axisEntry;
            hitAxis = axis;
            hitSign = velocity > 0.0f ? -1.0f : 1.0f;
        }
        exitTime = std::min(exitTime, axisExit);
        return entryTime <= exitTime;
    };

    if (!updateAxis('x', queryBox.min.x, queryBox.max.x, collider.bounds.min.x, collider.bounds.max.x, delta.x)
        || !updateAxis('y', queryBox.min.y, queryBox.max.y, collider.bounds.min.y, collider.bounds.max.y, delta.y)
        || !updateAxis('z', queryBox.min.z, queryBox.max.z, collider.bounds.min.z, collider.bounds.max.z, delta.z)) {
        return std::nullopt;
    }

    if (entryTime < 0.0f || entryTime > 1.0f || exitTime < 0.0f) {
        return std::nullopt;
    }

    const ri::spatial::Aabb endBox = TranslateBox(queryBox, delta * entryTime);
    const ri::math::Vec3 movedCenter = ri::spatial::Center(endBox);
    ri::math::Vec3 normal{};
    ri::math::Vec3 point = movedCenter;
    if (hitAxis == 'x') {
        normal = {hitSign != 0.0f ? hitSign : 1.0f, 0.0f, 0.0f};
        point.x = hitSign > 0.0f ? collider.bounds.max.x : collider.bounds.min.x;
        point.y = ClampFloat(movedCenter.y, collider.bounds.min.y, collider.bounds.max.y);
        point.z = ClampFloat(movedCenter.z, collider.bounds.min.z, collider.bounds.max.z);
    } else if (hitAxis == 'y') {
        normal = {0.0f, hitSign != 0.0f ? hitSign : 1.0f, 0.0f};
        point.y = hitSign > 0.0f ? collider.bounds.max.y : collider.bounds.min.y;
        point.x = ClampFloat(movedCenter.x, collider.bounds.min.x, collider.bounds.max.x);
        point.z = ClampFloat(movedCenter.z, collider.bounds.min.z, collider.bounds.max.z);
    } else {
        normal = {0.0f, 0.0f, hitSign != 0.0f ? hitSign : 1.0f};
        point.z = hitSign > 0.0f ? collider.bounds.max.z : collider.bounds.min.z;
        point.x = ClampFloat(movedCenter.x, collider.bounds.min.x, collider.bounds.max.x);
        point.y = ClampFloat(movedCenter.y, collider.bounds.min.y, collider.bounds.max.y);
    }

    return TraceHit{
        .id = collider.id,
        .bounds = collider.bounds,
        .point = point,
        .normal = normal,
        .penetration = 0.0f,
        .time = entryTime,
        .endBox = endBox,
    };
}

} // namespace

TraceScene::TraceScene(std::vector<TraceCollider> colliders, ri::spatial::SpatialIndexOptions indexOptions) {
    SetColliders(std::move(colliders), indexOptions);
}

bool TraceScene::TrySetDynamicColliderBounds(std::string_view id, const ri::spatial::Aabb& bounds) {
    if (id.empty() || ri::spatial::IsEmpty(bounds)) {
        return false;
    }
    for (const std::size_t colliderIndex : dynamicColliderIndices_) {
        TraceCollider& collider = colliders_[colliderIndex];
        if (!collider.dynamic || collider.id != id) {
            continue;
        }
        collider.bounds = bounds;
        return true;
    }
    return false;
}

void TraceScene::SetColliders(std::vector<TraceCollider> colliders, ri::spatial::SpatialIndexOptions indexOptions) {
    colliders_.clear();
    dynamicColliderIndices_.clear();
    metrics_.colliderCount = 0;
    metrics_.staticColliderCount = 0;
    metrics_.structuralStaticColliderCount = 0;
    metrics_.dynamicColliderCount = 0;

    std::vector<ri::spatial::SpatialEntry> staticEntries;
    std::vector<ri::spatial::SpatialEntry> structuralEntries;
    std::unordered_set<std::string> seenColliderIds;

    for (TraceCollider& collider : colliders) {
        if (collider.id.empty() || ri::spatial::IsEmpty(collider.bounds)) {
            continue;
        }
        if (!seenColliderIds.insert(collider.id).second) {
            continue;
        }

        const std::size_t index = colliders_.size();
        colliders_.push_back(std::move(collider));
        const TraceCollider& stored = colliders_.back();
        if (stored.dynamic) {
            dynamicColliderIndices_.push_back(index);
            metrics_.dynamicColliderCount += 1;
        } else {
            staticEntries.push_back({stored.id, stored.bounds});
            metrics_.staticColliderCount += 1;
            if (stored.structural) {
                structuralEntries.push_back({stored.id, stored.bounds});
                metrics_.structuralStaticColliderCount += 1;
            }
        }
    }

    metrics_.colliderCount = colliders_.size();
    staticIndex_.Rebuild(std::move(staticEntries), indexOptions);
    structuralIndex_.Rebuild(std::move(structuralEntries), indexOptions);
}

std::size_t TraceScene::ColliderCount() const {
    return colliders_.size();
}

std::vector<std::string> TraceScene::QueryCollidablesForBox(const ri::spatial::Aabb& box, bool structuralOnly) const {
    metrics_.boxQueries += 1;
    std::vector<const TraceCollider*> candidates = CollectCandidatesForBox(box, structuralOnly, {});
    std::vector<std::string> ids;
    ids.reserve(candidates.size());
    for (const TraceCollider* collider : candidates) {
        ids.push_back(collider->id);
    }
    return ids;
}

std::vector<std::string> TraceScene::QueryCollidablesForRay(const ri::math::Vec3& origin,
                                                            const ri::math::Vec3& direction,
                                                            float far,
                                                            bool structuralOnly) const {
    metrics_.rayQueries += 1;
    std::vector<const TraceCollider*> candidates = CollectCandidatesForRay(origin, direction, far, structuralOnly, {});
    std::vector<std::string> ids;
    ids.reserve(candidates.size());
    for (const TraceCollider* collider : candidates) {
        ids.push_back(collider->id);
    }
    return ids;
}

std::optional<TraceHit> TraceScene::TraceBox(const ri::spatial::Aabb& queryBox, const TraceOptions& options) const {
    metrics_.traceBoxQueries += 1;
    if (ri::spatial::IsEmpty(queryBox)) {
        return std::nullopt;
    }

    std::optional<TraceHit> bestHit;
    for (const TraceCollider* collider : CollectCandidatesForBox(queryBox, options.structuralOnly, options.ignoreId)) {
        const std::optional<TraceHit> hit = ComputeTraceBoxHit(queryBox, *collider);
        if (!hit.has_value()) {
            continue;
        }
        if (!bestHit.has_value() || hit->penetration < bestHit->penetration) {
            bestHit = hit;
        }
    }
    return bestHit;
}

std::optional<TraceHit> TraceScene::TraceRay(const ri::math::Vec3& origin,
                                             const ri::math::Vec3& direction,
                                             float far,
                                             const TraceOptions& options) const {
    metrics_.traceRayQueries += 1;
    if (!std::isfinite(far) || far <= 0.0f || ri::math::LengthSquared(direction) < 1e-20f) {
        return std::nullopt;
    }

    std::optional<TraceHit> bestHit;
    for (const TraceCollider* collider : CollectCandidatesForRay(origin, direction, far, options.structuralOnly, options.ignoreId)) {
        const std::optional<TraceHit> hit = ComputeRayHit(origin, direction, far, *collider);
        if (!hit.has_value()) {
            continue;
        }
        if (!bestHit.has_value() || hit->time < bestHit->time) {
            bestHit = hit;
        }
    }
    return bestHit;
}

std::optional<TraceHit> TraceScene::TraceSweptBox(const ri::spatial::Aabb& queryBox,
                                                  const ri::math::Vec3& delta,
                                                  const TraceOptions& options) const {
    metrics_.sweptBoxQueries += 1;
    if (ri::spatial::IsEmpty(queryBox)) {
        return std::nullopt;
    }

    ri::spatial::Aabb sweepQueryBox = ri::spatial::Union(queryBox, TranslateBox(queryBox, delta));
    std::optional<TraceHit> bestHit;
    for (const TraceCollider* collider : CollectCandidatesForBox(sweepQueryBox, options.structuralOnly, options.ignoreId)) {
        const std::optional<TraceHit> hit = ComputeSweptBoxHit(queryBox, delta, *collider);
        if (!hit.has_value()) {
            continue;
        }
        if (!bestHit.has_value() || hit->time < bestHit->time) {
            bestHit = hit;
        }
    }
    return bestHit;
}

SlideMoveResult TraceScene::SlideMoveBox(const ri::spatial::Aabb& queryBox,
                                         const ri::math::Vec3& delta,
                                         std::size_t maxBumps,
                                         float epsilon,
                                         const TraceOptions& options) const {
    SlideMoveResult result{};
    result.endBox = queryBox;
    if (ri::spatial::IsEmpty(queryBox)) {
        return result;
    }

    ri::spatial::Aabb workingBox = queryBox;
    ri::math::Vec3 moved{};
    ri::math::Vec3 remaining = delta;

    for (std::size_t bump = 0; bump < maxBumps; ++bump) {
        if (ri::math::LengthSquared(remaining) <= 1e-10f) {
            break;
        }
        const std::optional<TraceHit> hit = TraceSweptBox(workingBox, remaining, options);
        if (!hit.has_value()) {
            workingBox = TranslateBox(workingBox, remaining);
            moved = moved + remaining;
            remaining = {};
            break;
        }

        result.blocked = true;
        const float moveTime = std::max(0.0f, hit->time - epsilon);
        const ri::math::Vec3 step = remaining * moveTime;
        if (ri::math::LengthSquared(step) > 0.0f) {
            workingBox = TranslateBox(workingBox, step);
            moved = moved + step;
        }

        result.hits.push_back(*hit);

        const float remainingScale = std::max(0.0f, 1.0f - hit->time);
        ri::math::Vec3 clip = remaining * remainingScale;
        const float intoSurface = ri::math::Dot(clip, hit->normal);
        if (intoSurface < 0.0f) {
            clip = clip - (hit->normal * intoSurface);
        }
        remaining = clip;
        const ri::math::Vec3 nudge = hit->normal * (epsilon * 2.0f);
        workingBox = TranslateBox(workingBox, nudge);
        moved = moved + nudge;
    }

    result.positionDelta = moved;
    result.remainingDelta = remaining;
    result.endBox = workingBox;
    return result;
}

std::optional<TraceHit> TraceScene::FindGroundHit(const ri::math::Vec3& origin,
                                                  const GroundTraceOptions& options) const {
    const TraceOptions traceOptions{
        .structuralOnly = options.structuralOnly,
        .ignoreId = options.ignoreId,
    };
    if (const std::optional<TraceHit> hit = TraceRay(origin, {0.0f, -1.0f, 0.0f}, options.maxDistance, traceOptions);
        hit.has_value() && hit->normal.y >= options.minNormalY) {
        return hit;
    }

    const float halfProbe = 0.05f;
    const ri::spatial::Aabb probeBox{
        .min = {origin.x - halfProbe, origin.y - options.maxDistance, origin.z - halfProbe},
        .max = {origin.x + halfProbe, origin.y + 0.001f, origin.z + halfProbe},
    };

    std::optional<TraceHit> bestHit;
    for (const TraceCollider& collider : colliders_) {
        if (!options.ignoreId.empty() && collider.id == options.ignoreId) {
            continue;
        }
        if (options.structuralOnly && !collider.structural) {
            continue;
        }
        if (collider.dynamic || !ri::spatial::Intersects(collider.bounds, probeBox)) {
            continue;
        }
        if (origin.x < collider.bounds.min.x || origin.x > collider.bounds.max.x ||
            origin.z < collider.bounds.min.z || origin.z > collider.bounds.max.z) {
            continue;
        }
        if (collider.bounds.max.y > origin.y + 0.001f) {
            continue;
        }
        const float distance = origin.y - collider.bounds.max.y;
        if (distance < 0.0f || distance > options.maxDistance) {
            continue;
        }

        TraceHit candidate{
            .id = collider.id,
            .bounds = collider.bounds,
            .point = {origin.x, collider.bounds.max.y, origin.z},
            .normal = {0.0f, 1.0f, 0.0f},
            .penetration = 0.0f,
            .time = distance,
            .endBox = ri::spatial::MakeEmptyAabb(),
        };
        if (!bestHit.has_value() || candidate.point.y > bestHit->point.y) {
            bestHit = candidate;
        }
    }
    if (bestHit.has_value() && bestHit->normal.y >= options.minNormalY) {
        return bestHit;
    }
    return std::nullopt;
}

TraceSceneMetrics TraceScene::Metrics() const noexcept {
    return metrics_;
}

ri::spatial::SpatialIndexMetrics TraceScene::StaticIndexMetrics() const noexcept {
    return staticIndex_.Metrics();
}

ri::spatial::SpatialIndexMetrics TraceScene::StructuralIndexMetrics() const noexcept {
    return structuralIndex_.Metrics();
}

void TraceScene::ResetMetrics() noexcept {
    metrics_.boxQueries = 0;
    metrics_.rayQueries = 0;
    metrics_.sweptBoxQueries = 0;
    metrics_.traceBoxQueries = 0;
    metrics_.traceRayQueries = 0;
    metrics_.staticCandidates = 0;
    metrics_.dynamicCandidates = 0;
}

const TraceCollider* TraceScene::FindCollider(std::string_view id) const {
    auto found = std::find_if(colliders_.begin(), colliders_.end(), [&](const TraceCollider& collider) {
        return collider.id == id;
    });
    return found == colliders_.end() ? nullptr : &(*found);
}

std::vector<const TraceCollider*> TraceScene::CollectCandidatesForBox(const ri::spatial::Aabb& box,
                                                                      bool structuralOnly,
                                                                      std::string_view ignoreId) const {
    std::vector<const TraceCollider*> candidates;
    std::unordered_set<std::string> seen;

    const std::vector<std::string> staticIds = structuralOnly ? structuralIndex_.QueryBox(box) : staticIndex_.QueryBox(box);
    metrics_.staticCandidates += staticIds.size();
    for (const std::string& id : staticIds) {
        if (!ignoreId.empty() && id == ignoreId) {
            continue;
        }
        if (seen.contains(id)) {
            continue;
        }
        if (const TraceCollider* collider = FindCollider(id)) {
            seen.insert(id);
            candidates.push_back(collider);
        }
    }

    if (!structuralOnly) {
        for (std::size_t colliderIndex : dynamicColliderIndices_) {
            const TraceCollider& collider = colliders_[colliderIndex];
            if ((!ignoreId.empty() && collider.id == ignoreId) || !ri::spatial::Intersects(collider.bounds, box)) {
                continue;
            }
            if (seen.insert(collider.id).second) {
                candidates.push_back(&collider);
                metrics_.dynamicCandidates += 1;
            }
        }
    }

    return candidates;
}

std::vector<const TraceCollider*> TraceScene::CollectCandidatesForRay(const ri::math::Vec3& origin,
                                                                      const ri::math::Vec3& direction,
                                                                      float far,
                                                                      bool structuralOnly,
                                                                      std::string_view ignoreId) const {
    std::vector<const TraceCollider*> candidates;
    std::unordered_set<std::string> seen;

    const std::vector<std::string> staticIds = structuralOnly
        ? structuralIndex_.QueryRay(origin, direction, far)
        : staticIndex_.QueryRay(origin, direction, far);
    metrics_.staticCandidates += staticIds.size();
    for (const std::string& id : staticIds) {
        if (!ignoreId.empty() && id == ignoreId) {
            continue;
        }
        if (seen.contains(id)) {
            continue;
        }
        if (const TraceCollider* collider = FindCollider(id)) {
            seen.insert(id);
            candidates.push_back(collider);
        }
    }

    if (!structuralOnly) {
        for (std::size_t colliderIndex : dynamicColliderIndices_) {
            const TraceCollider& collider = colliders_[colliderIndex];
            if (!ignoreId.empty() && collider.id == ignoreId) {
                continue;
            }
            float hitDistance = 0.0f;
            if (!ri::spatial::IntersectRayAabb(ri::spatial::Ray{.origin = origin, .direction = direction}, collider.bounds, far, &hitDistance)) {
                continue;
            }
            if (seen.insert(collider.id).second) {
                candidates.push_back(&collider);
                metrics_.dynamicCandidates += 1;
            }
        }
    }

    return candidates;
}

} // namespace ri::trace
