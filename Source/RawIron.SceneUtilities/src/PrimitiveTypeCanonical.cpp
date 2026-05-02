#include "RawIron/Scene/PrimitiveTypeCanonical.h"

#include <algorithm>
#include <cctype>

namespace ri::scene {
namespace {

std::string NormalizeLower(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const char ch : value) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

} // namespace

std::string CanonicalScenePrimitiveKey(const std::string_view token) {
    return NormalizeLower(token);
}

PrimitiveType ResolveScenePrimitiveTypeFromAuthoring(const std::string_view singleToken, const PrimitiveType fallback) {
    if (singleToken.empty()) {
        return fallback;
    }
    return ResolveScenePrimitiveTypeFromAuthoring(std::optional<std::string_view>(singleToken), std::nullopt, fallback);
}

PrimitiveType ResolveScenePrimitiveTypeFromAuthoring(const std::optional<std::string_view> primaryToken,
                                                     const std::optional<std::string_view> secondaryToken,
                                                     const PrimitiveType fallback) {
    const std::string_view chosen =
        primaryToken.has_value() && !primaryToken->empty()
            ? *primaryToken
            : (secondaryToken.has_value() && !secondaryToken->empty() ? *secondaryToken : std::string_view{});
    if (chosen.empty()) {
        return fallback;
    }
    const std::string normalized = NormalizeLower(chosen);
    if (normalized == "cube" || normalized == "box" || normalized == "block") {
        return PrimitiveType::Cube;
    }
    if (normalized == "plane" || normalized == "quad" || normalized == "ground" || normalized == "floor") {
        return PrimitiveType::Plane;
    }
    if (normalized == "sphere" || normalized == "ball" || normalized == "uv_sphere" || normalized == "uvsphere") {
        return PrimitiveType::Sphere;
    }
    if (normalized == "custom" || normalized == "mesh" || normalized == "triangles") {
        return PrimitiveType::Custom;
    }
    return fallback;
}

std::string ResolveStructuralPrimitiveTypeToken(const std::optional<std::string_view> primaryToken,
                                                const std::optional<std::string_view> secondaryToken,
                                                const std::string_view fallback) {
    const std::string_view chosen =
        primaryToken.has_value() && !primaryToken->empty()
            ? *primaryToken
            : (secondaryToken.has_value() && !secondaryToken->empty() ? *secondaryToken : std::string_view{});
    if (chosen.empty()) {
        return std::string(fallback);
    }
    std::string normalized = NormalizeLower(chosen);
    if (normalized == "cube" || normalized == "box" || normalized == "block") {
        normalized = "box";
    } else if (normalized == "sphere" || normalized == "ball") {
        normalized = "sphere";
    } else if (normalized == "plane" || normalized == "quad" || normalized == "ground") {
        normalized = "plane";
    } else if (normalized == "displacement" || normalized == "displacement_map") {
        normalized = "displacement";
    } else if (normalized == "manifold_sweep" || normalized == "spline_extrusion") {
        normalized = "spline_sweep";
    } else if (normalized == "cellular_shatter") {
        normalized = "voronoi_fracture";
    } else if (normalized == "metaball") {
        normalized = "metaball_cluster";
    } else if (normalized == "lsystem" || normalized == "lsystem_branch") {
        normalized = "lsystem_branch";
    }
    return normalized;
}

} // namespace ri::scene
