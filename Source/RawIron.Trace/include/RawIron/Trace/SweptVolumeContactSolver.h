#pragma once

#include "RawIron/Trace/TraceScene.h"

#include <optional>

namespace ri::trace {

/// Swept AABB contact resolution against the \ref TraceScene collision index: broad-phase candidate gathering,
/// then swept narrow-phase with overlap-first handling (time-of-impact 0) identical to \ref TraceScene::TraceSweptBox.
class SweptVolumeContactSolver {
public:
    explicit SweptVolumeContactSolver(const TraceScene& scene) : scene_(&scene) {}

    [[nodiscard]] std::optional<TraceHit> ResolveFirstContact(const ri::spatial::Aabb& queryBox,
                                                              const ri::math::Vec3& delta,
                                                              const TraceOptions& options = {}) const {
        return scene_->TraceSweptBox(queryBox, delta, options);
    }

    [[nodiscard]] SlideMoveResult SlideResolve(const ri::spatial::Aabb& queryBox,
                                               const ri::math::Vec3& delta,
                                               std::size_t maxBumps = 4,
                                               float epsilon = 0.001f,
                                               const TraceOptions& options = {}) const {
        return scene_->SlideMoveBox(queryBox, delta, maxBumps, epsilon, options);
    }

    /// Single static obstacle, no spatial index (same narrow-phase as the scene).
    [[nodiscard]] static std::optional<TraceHit> ResolveVsStaticAabb(const ri::spatial::Aabb& movingBox,
                                                                      const ri::math::Vec3& delta,
                                                                      const ri::spatial::Aabb& staticBounds,
                                                                      std::string_view obstacleId);

private:
    const TraceScene* scene_ = nullptr;
};

} // namespace ri::trace
