#include "RawIron/Spatial/Aabb.h"
#include "RawIron/World/RuntimeState.h"
#include "RawIron/World/VolumeDescriptors.h"

#include <cmath>
#include <cstdlib>
#include <limits>

namespace {

bool IsFinite(const ri::math::Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsUsable(const ri::spatial::Aabb& bounds) {
    return IsFinite(bounds.min) && IsFinite(bounds.max) && !ri::spatial::IsEmpty(bounds);
}

} // namespace

int main() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();

    const ri::world::RuntimeVolume created = ri::world::CreateRuntimeVolume(
        ri::world::RuntimeVolumeSeed{
            .position = ri::math::Vec3{nan, infinity, -infinity},
            .rotationRadians = ri::math::Vec3{nan, infinity, -infinity},
            .size = ri::math::Vec3{nan, infinity, -infinity},
            .radius = nan,
            .height = infinity,
        });
    if (!IsFinite(created.position) || !IsFinite(created.size)
        || !std::isfinite(created.radius) || !std::isfinite(created.height)) {
        return EXIT_FAILURE;
    }

    const ri::world::AuthoringRuntimeVolumeRecord authoring = ri::world::BuildAuthoringRuntimeVolumeRecord(
        ri::world::RuntimeVolumeSeed{
            .rotationRadians = ri::math::Vec3{nan, infinity, -infinity},
        });
    if (!authoring.rotationRadians.has_value() || !IsFinite(*authoring.rotationRadians)) {
        return EXIT_FAILURE;
    }

    for (const ri::world::VolumeShape shape : {
             ri::world::VolumeShape::Box,
             ri::world::VolumeShape::Cylinder,
             ri::world::VolumeShape::Sphere,
         }) {
        ri::world::RuntimeVolume malformed{};
        malformed.shape = shape;
        malformed.position = {nan, infinity, -infinity};
        malformed.size = {nan, infinity, -infinity};
        malformed.radius = infinity;
        malformed.height = nan;
        if (!IsUsable(ri::world::BuildRuntimeVolumeBounds(malformed))) {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
