#include "RawIron/Scene/SceneEntityPhysics.h"

#include "RawIron/Math/Mat4.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Spatial/Aabb.h"

#include <cmath>

namespace ri::scene {

namespace {

[[nodiscard]] bool NodeHandleValid(const Scene& scene, int handle) noexcept {
    return handle >= 0 && static_cast<std::size_t>(handle) < scene.NodeCount();
}

[[nodiscard]] bool SolveLinear3(const ri::math::Vec3& col0,
                              const ri::math::Vec3& col1,
                              const ri::math::Vec3& col2,
                              const ri::math::Vec3& rhs,
                              ri::math::Vec3& out) noexcept {
    const ri::math::Vec3 cross_c1_c2 = ri::math::Cross(col1, col2);
    const float det = ri::math::Dot(col0, cross_c1_c2);
    if (std::abs(det) < 1.0e-18f) {
        return false;
    }
    const float invDet = 1.0f / det;
    out.x = ri::math::Dot(rhs, ri::math::Cross(col1, col2)) * invDet;
    out.y = ri::math::Dot(rhs, ri::math::Cross(col2, col0)) * invDet;
    out.z = ri::math::Dot(rhs, ri::math::Cross(col0, col1)) * invDet;
    return true;
}

} // namespace

bool ApplyKinematicBodyStateWorldCenterToSceneNode(Scene& scene,
                                                     int nodeHandle,
                                                     const ri::trace::KinematicBodyState& state) {
    if (!NodeHandleValid(scene, nodeHandle)) {
        return false;
    }
    if (ri::spatial::IsEmpty(state.bounds)) {
        return false;
    }

    Node& node = scene.GetNode(nodeHandle);
    const ri::math::Vec3 targetWorld = ri::spatial::Center(state.bounds);

    const ri::math::Mat4 rs =
        ri::math::Multiply(ri::math::RotationXYZDegrees(node.localTransform.rotationDegrees),
                           ri::math::ScaleMatrix(node.localTransform.scale));

    const ri::math::Mat4 pw =
        node.parent == kInvalidHandle ? ri::math::IdentityMatrix() : scene.ComputeWorldMatrix(node.parent);

    const ri::math::Mat4 baseWorldMat = ri::math::Multiply(pw, rs);
    const ri::math::Vec3 baseWorld = ri::math::ExtractTranslation(baseWorldMat);

    auto deltaAxis = [&](const ri::math::Vec3& axis) -> ri::math::Vec3 {
        const ri::math::Mat4 translated =
            ri::math::Multiply(pw, ri::math::Multiply(ri::math::TranslationMatrix(axis), rs));
        return ri::math::ExtractTranslation(translated) - baseWorld;
    };

    const ri::math::Vec3 mx = deltaAxis({1.0f, 0.0f, 0.0f});
    const ri::math::Vec3 my = deltaAxis({0.0f, 1.0f, 0.0f});
    const ri::math::Vec3 mz = deltaAxis({0.0f, 0.0f, 1.0f});

    ri::math::Vec3 localPos{};
    if (!SolveLinear3(mx, my, mz, targetWorld - baseWorld, localPos)) {
        return false;
    }

    node.localTransform.position = localPos;
    return true;
}

void ApplyKinematicBodyStatesToSceneNodes(Scene& scene,
                                          std::span<const int> nodeHandles,
                                          std::span<const ri::trace::KinematicBodyState> states) {
    const std::size_t count = std::min(nodeHandles.size(), states.size());
    for (std::size_t index = 0; index < count; ++index) {
        (void)ApplyKinematicBodyStateWorldCenterToSceneNode(scene, nodeHandles[index], states[index]);
    }
}

} // namespace ri::scene
