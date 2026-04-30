#pragma once

#include "RawIron/Scene/Transform.h"

namespace ri::scene {

/// Multiplies authored [sx,sy,sz] onto the import-local scale after file transforms are resolved.
/// Stack: world = parent * TRS(import) * S(authoringScale) at this node (combine with your hierarchy rules).
[[nodiscard]] inline Transform ApplyAuthoringScaleMultiply(const Transform& importLocal,
                                                           const ri::math::Vec3& authoredScale) noexcept {
    Transform result = importLocal;
    result.scale.x *= authoredScale.x;
    result.scale.y *= authoredScale.y;
    result.scale.z *= authoredScale.z;
    return result;
}

} // namespace ri::scene
