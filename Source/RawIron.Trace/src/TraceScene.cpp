#include "RawIron/Trace/TraceScene.h"
#include "RawIron/Trace/SweptAabbContact.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <functional>
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

std::optional<TraceHit> ComputeTraceBoxHit(const ri::spatial::Aabb& queryBox, const TraceCollider& collider) {
    return ComputeAabbOverlapTraceHit(queryBox, collider.bounds, collider.id);
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
    return ComputeSweptAabbTraceHit(queryBox, delta, collider.bounds, collider.id);
}

} // namespace

TraceScene::TraceScene(std::vector<TraceCollider> colliders, ri::spatial::SpatialIndexOptions indexOptions) {
    SetColliders(std::move(colliders), indexOptions);
}

bool TraceScene::TrySetDynamicColliderBounds(std::string_view id, const ri::spatial::Aabb& bounds) {
    if (id.empty() || ri::spatial::IsEmpty(bounds)) {
        return false;
    }
    const auto found = colliderIndexById_.find(id);
    if (found == colliderIndexById_.end()) {
        return false;
    }
    TraceCollider& collider = colliders_[found->second];
    if (!collider.dynamic) {
        return false;
    }
    collider.bounds = bounds;
    return true;
}

std::size_t TraceScene::EraseCollidersIf(const std::function<bool(const TraceCollider&)>& shouldRemove,
                                         ri::spatial::SpatialIndexOptions indexOptions) {
    std::vector<TraceCollider> kept;
    kept.reserve(colliders_.size());
    std::size_t removed = 0;
    for (TraceCollider& collider : colliders_) {
        if (shouldRemove(collider)) {
            ++removed;
            continue;
        }
        kept.push_back(std::move(collider));
    }
    SetColliders(std::move(kept), indexOptions);
    return removed;
}

std::size_t TraceScene::EraseCollidersWithIds(const std::vector<std::string_view>& ids,
                                              ri::spatial::SpatialIndexOptions indexOptions) {
    std::unordered_set<std::string> wanted;
    wanted.reserve(ids.size());
    for (const std::string_view id : ids) {
        if (!id.empty()) {
            wanted.emplace(id);
        }
    }
    if (wanted.empty()) {
        return 0;
    }
    return EraseCollidersIf(
        [&](const TraceCollider& c) { return wanted.find(c.id) != wanted.end(); }, std::move(indexOptions));
}

void TraceScene::SetColliders(std::vector<TraceCollider> colliders, ri::spatial::SpatialIndexOptions indexOptions) {
    colliders_.clear();
    dynamicColliderIndices_.clear();
    colliderIndexById_.clear();
    staticColliderIndices_.clear();
    structuralColliderIndices_.clear();
    metrics_.colliderCount = 0;
    metrics_.staticColliderCount = 0;
    metrics_.structuralStaticColliderCount = 0;
    metrics_.dynamicColliderCount = 0;

    std::vector<ri::spatial::SpatialEntry> staticEntries;
    std::vector<ri::spatial::SpatialEntry> structuralEntries;

    for (TraceCollider& collider : colliders) {
        if (collider.id.empty() || ri::spatial::IsEmpty(collider.bounds)) {
            continue;
        }
        if (colliderIndexById_.contains(collider.id)) {
            continue;
        }

        const std::size_t index = colliders_.size();
        colliders_.push_back(std::move(collider));
        const TraceCollider& stored = colliders_.back();
        colliderIndexById_.emplace(stored.id, index);
        if (stored.dynamic) {
            dynamicColliderIndices_.push_back(index);
            metrics_.dynamicColliderCount += 1;
        } else {
            staticEntries.push_back({stored.id, stored.bounds});
            staticColliderIndices_.push_back(index);
            metrics_.staticColliderCount += 1;
            if (stored.structural) {
                structuralEntries.push_back({stored.id, stored.bounds});
                structuralColliderIndices_.push_back(index);
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

    // Both broad-phase trees hold static colliders only, so the dynamic skip of the old linear scan is implicit.
    const std::vector<std::size_t>& sourceToCollider = options.structuralOnly ? structuralColliderIndices_ : staticColliderIndices_;
    const std::vector<std::size_t> probeSources = options.structuralOnly
        ? structuralIndex_.QueryBoxSourceIndices(probeBox)
        : staticIndex_.QueryBoxSourceIndices(probeBox);

    std::optional<TraceHit> bestHit;
    for (const std::size_t sourceIndex : probeSources) {
        const TraceCollider& collider = colliders_[sourceToCollider[sourceIndex]];
        if (!options.ignoreId.empty() && collider.id == options.ignoreId) {
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
    const auto found = colliderIndexById_.find(id);
    return found == colliderIndexById_.end() ? nullptr : &colliders_[found->second];
}

std::vector<const TraceCollider*> TraceScene::CollectCandidatesForBox(const ri::spatial::Aabb& box,
                                                                      bool structuralOnly,
                                                                      std::string_view ignoreId) const {
    std::vector<const TraceCollider*> candidates;

    // Static and dynamic colliders never share ids (SetColliders dedupes), so no cross-set dedupe is needed.
    const std::vector<std::size_t>& sourceToCollider = structuralOnly ? structuralColliderIndices_ : staticColliderIndices_;
    const std::vector<std::size_t> staticSources = structuralOnly
        ? structuralIndex_.QueryBoxSourceIndices(box)
        : staticIndex_.QueryBoxSourceIndices(box);
    metrics_.staticCandidates += staticSources.size();
    candidates.reserve(staticSources.size());
    for (const std::size_t sourceIndex : staticSources) {
        const TraceCollider& collider = colliders_[sourceToCollider[sourceIndex]];
        if (!ignoreId.empty() && collider.id == ignoreId) {
            continue;
        }
        candidates.push_back(&collider);
    }

    for (const std::size_t colliderIndex : dynamicColliderIndices_) {
        const TraceCollider& collider = colliders_[colliderIndex];
        if ((structuralOnly && !collider.structural)
            || (!ignoreId.empty() && collider.id == ignoreId)
            || !ri::spatial::Intersects(collider.bounds, box)) {
            continue;
        }
        candidates.push_back(&collider);
        metrics_.dynamicCandidates += 1;
    }

    return candidates;
}

std::vector<const TraceCollider*> TraceScene::CollectCandidatesForRay(const ri::math::Vec3& origin,
                                                                      const ri::math::Vec3& direction,
                                                                      float far,
                                                                      bool structuralOnly,
                                                                      std::string_view ignoreId) const {
    std::vector<const TraceCollider*> candidates;

    const std::vector<std::size_t>& sourceToCollider = structuralOnly ? structuralColliderIndices_ : staticColliderIndices_;
    const std::vector<ri::spatial::SpatialRayCandidate> staticSources = structuralOnly
        ? structuralIndex_.QueryRayCandidates(origin, direction, far)
        : staticIndex_.QueryRayCandidates(origin, direction, far);
    metrics_.staticCandidates += staticSources.size();
    candidates.reserve(staticSources.size());
    for (const ri::spatial::SpatialRayCandidate& source : staticSources) {
        const TraceCollider& collider = colliders_[sourceToCollider[source.sourceIndex]];
        if (!ignoreId.empty() && collider.id == ignoreId) {
            continue;
        }
        candidates.push_back(&collider);
    }

    for (const std::size_t colliderIndex : dynamicColliderIndices_) {
        const TraceCollider& collider = colliders_[colliderIndex];
        if ((structuralOnly && !collider.structural)
            || (!ignoreId.empty() && collider.id == ignoreId)) {
            continue;
        }
        float hitDistance = 0.0f;
        if (!ri::spatial::IntersectRayAabb(
                ri::spatial::Ray{.origin = origin, .direction = direction}, collider.bounds, far, &hitDistance)) {
            continue;
        }
        candidates.push_back(&collider);
        metrics_.dynamicCandidates += 1;
    }

    return candidates;
}

} // namespace ri::trace
