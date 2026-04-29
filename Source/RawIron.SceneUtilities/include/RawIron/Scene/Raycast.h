#pragma once

#include "RawIron/Math/Vec3.h"
#include "RawIron/Scene/Scene.h"

#include <optional>
#include <vector>

namespace ri::scene {

struct Ray {
    ri::math::Vec3 origin{0.0f, 0.0f, 0.0f};
    ri::math::Vec3 direction{0.0f, 0.0f, 1.0f};
};

struct RaycastHit {
    int node = kInvalidHandle;
    int mesh = kInvalidHandle;
    float distance = 0.0f;
    ri::math::Vec3 position{0.0f, 0.0f, 0.0f};
    ri::math::Vec3 normal{0.0f, 0.0f, 0.0f};
};

std::optional<Ray> BuildPerspectiveCameraRay(const Scene& scene,
                                             int cameraNodeHandle,
                                             float viewportX,
                                             float viewportY,
                                             float aspectRatio = 1.0f);
std::optional<RaycastHit> RaycastNode(const Scene& scene, int nodeHandle, const Ray& ray);
std::optional<RaycastHit> RaycastSceneNearest(const Scene& scene, const Ray& ray);
std::vector<RaycastHit> RaycastSceneAll(const Scene& scene, const Ray& ray);

} // namespace ri::scene
