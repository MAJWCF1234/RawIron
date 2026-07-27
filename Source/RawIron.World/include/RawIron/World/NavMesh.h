#pragma once

#include "RawIron/Math/Vec3.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ri::world {

struct NavMeshRegion {
    std::string id;
    ri::math::Vec3 minimum{};
    ri::math::Vec3 maximum{};
    float areaCost = 1.0f;
    std::string flags = "walk";
};

struct NavMeshLink {
    std::string id;
    std::string fromRegion;
    std::string toRegion;
    bool bidirectional = true;
};

struct NavMeshPathQuery {
    float maxEndpointSnapDistance = 2.0f;
    std::size_t maxVisitedRegions = 4096;
};

struct NavMeshPath {
    bool found = false;
    float traversalCost = 0.0f;
    std::vector<std::string> regionIds;
    std::vector<ri::math::Vec3> waypoints;
    std::string diagnostic;
};

class NavMesh {
public:
    [[nodiscard]] static std::optional<NavMesh> LoadDescriptor(
        const std::filesystem::path& path,
        std::string* error = nullptr);

    [[nodiscard]] const std::vector<NavMeshRegion>& Regions() const noexcept;
    [[nodiscard]] const std::vector<NavMeshLink>& Links() const noexcept;
    [[nodiscard]] std::optional<std::size_t> FindContainingRegion(const ri::math::Vec3& point) const;
    [[nodiscard]] NavMeshPath FindPath(
        const ri::math::Vec3& start,
        const ri::math::Vec3& goal,
        const NavMeshPathQuery& query = {}) const;

private:
    std::vector<NavMeshRegion> regions_;
    std::vector<NavMeshLink> links_;
};

} // namespace ri::world
