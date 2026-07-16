#include "RawIron/Scene/SceneEntityPhysics.h"
#include "RawIron/Spatial/Aabb.h"

#include <cmath>
#include <cstdlib>
#include <limits>

int main() {
    ri::scene::Scene scene{"SceneEntityPhysicsSmoke"};
    const int root = scene.CreateNode("Root");
    const int body = scene.CreateNode("Body", root);

    ri::trace::KinematicBodyState state{};
    state.bounds = {
        .min = {2.0f, 4.0f, 6.0f},
        .max = {4.0f, 8.0f, 10.0f},
    };
    if (!ri::scene::ApplyKinematicBodyStateWorldCenterToSceneNode(scene, body, state)) {
        return EXIT_FAILURE;
    }
    const ri::math::Vec3 positioned = scene.GetNode(body).localTransform.position;
    if (positioned.x != 3.0f || positioned.y != 6.0f || positioned.z != 8.0f) {
        return EXIT_FAILURE;
    }

    const float maximum = std::numeric_limits<float>::max();
    const ri::spatial::Aabb extreme{.min = {maximum, maximum, maximum}, .max = {maximum, maximum, maximum}};
    const ri::math::Vec3 extremeCenter = ri::spatial::Center(extreme);
    if (!std::isfinite(extremeCenter.x) || extremeCenter.x != maximum) {
        return EXIT_FAILURE;
    }
    const ri::spatial::Aabb invalidSegment = ri::spatial::BuildSegmentBounds(
        {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f});
    if (!ri::spatial::IsEmpty(invalidSegment)) {
        return EXIT_FAILURE;
    }

    scene.GetNode(body).localTransform.rotationDegrees.x = std::numeric_limits<float>::quiet_NaN();
    const ri::math::Vec3 before = scene.GetNode(body).localTransform.position;
    if (ri::scene::ApplyKinematicBodyStateWorldCenterToSceneNode(scene, body, state)) {
        return EXIT_FAILURE;
    }
    const ri::math::Vec3 after = scene.GetNode(body).localTransform.position;
    if (after.x != before.x || after.y != before.y || after.z != before.z) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
