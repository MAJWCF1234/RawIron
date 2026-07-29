#include "RawIron/World/NavMesh.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <queue>
#include <sstream>
#include <string_view>

namespace ri::world {
namespace {

[[nodiscard]] std::string_view Trim(std::string_view value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1U);
}

[[nodiscard]] std::vector<std::string_view> SplitCsv(const std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t cursor = 0;
    while (cursor <= line.size()) {
        const std::size_t comma = line.find(',', cursor);
        const std::size_t end = comma == std::string_view::npos ? line.size() : comma;
        fields.push_back(Trim(line.substr(cursor, end - cursor)));
        if (comma == std::string_view::npos) {
            break;
        }
        cursor = comma + 1U;
    }
    return fields;
}

[[nodiscard]] bool ParseFloat(const std::string_view value, float& result) {
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    const auto parsed = std::from_chars(begin, end, result);
    return parsed.ec == std::errc{} && parsed.ptr == end && std::isfinite(result);
}

[[nodiscard]] bool ParseBool(const std::string_view value, bool& result) {
    if (value == "1" || value == "true" || value == "yes") {
        result = true;
        return true;
    }
    if (value == "0" || value == "false" || value == "no") {
        result = false;
        return true;
    }
    return false;
}

[[nodiscard]] ri::math::Vec3 RegionCenter(const NavMeshRegion& region) {
    return {
        (region.minimum.x + region.maximum.x) * 0.5f,
        (region.minimum.y + region.maximum.y) * 0.5f,
        (region.minimum.z + region.maximum.z) * 0.5f,
    };
}

[[nodiscard]] float DistanceToRegion(const NavMeshRegion& region, const ri::math::Vec3& point) {
    const float dx = std::max({region.minimum.x - point.x, 0.0f, point.x - region.maximum.x});
    const float dy = std::max({region.minimum.y - point.y, 0.0f, point.y - region.maximum.y});
    const float dz = std::max({region.minimum.z - point.z, 0.0f, point.z - region.maximum.z});
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

[[nodiscard]] float RegionVolume(const NavMeshRegion& region) {
    return (region.maximum.x - region.minimum.x)
        * (region.maximum.y - region.minimum.y)
        * (region.maximum.z - region.minimum.z);
}

[[nodiscard]] bool HasFlagToken(const std::string_view flags, const std::string_view wanted) {
    if (wanted.empty()) {
        return true;
    }
    std::size_t cursor = 0;
    while (cursor < flags.size()) {
        cursor = flags.find_first_not_of(" \t|;+/", cursor);
        if (cursor == std::string_view::npos) {
            return false;
        }
        const std::size_t end = flags.find_first_of(" \t|;+/", cursor);
        const std::string_view token =
            flags.substr(cursor, end == std::string_view::npos ? flags.size() - cursor : end - cursor);
        if (token == wanted) {
            return true;
        }
        if (end == std::string_view::npos) {
            return false;
        }
        cursor = end + 1U;
    }
    return false;
}

} // namespace

std::optional<NavMesh> NavMesh::LoadDescriptor(const std::filesystem::path& path, std::string* error) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        if (error != nullptr) {
            *error = "Unable to open navmesh descriptor: " + path.string();
        }
        return std::nullopt;
    }

    NavMesh mesh;
    std::unordered_map<std::string, std::size_t> linkById;
    std::string line;
    std::size_t lineNumber = 0;
    auto fail = [&](const std::string& message) -> std::optional<NavMesh> {
        if (error != nullptr) {
            *error = path.string() + ":" + std::to_string(lineNumber) + ": " + message;
        }
        return std::nullopt;
    };

    while (std::getline(stream, line)) {
        ++lineNumber;
        const std::string_view trimmed = Trim(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }
        const std::vector<std::string_view> fields = SplitCsv(trimmed);
        if (fields.size() == 9U) {
            NavMeshRegion region{};
            region.id = std::string(fields[0]);
            if (region.id.empty() || mesh.regionById_.contains(region.id)) {
                return fail("region id is empty or duplicated");
            }
            if (!ParseFloat(fields[1], region.minimum.x)
                || !ParseFloat(fields[2], region.minimum.y)
                || !ParseFloat(fields[3], region.minimum.z)
                || !ParseFloat(fields[4], region.maximum.x)
                || !ParseFloat(fields[5], region.maximum.y)
                || !ParseFloat(fields[6], region.maximum.z)
                || !ParseFloat(fields[7], region.areaCost)) {
                return fail("region contains an invalid numeric value");
            }
            if (region.minimum.x > region.maximum.x || region.minimum.y > region.maximum.y
                || region.minimum.z > region.maximum.z || region.areaCost <= 0.0f) {
                return fail("region bounds or traversal cost are invalid");
            }
            region.flags = std::string(fields[8]);
            if (region.flags.empty()) {
                return fail("region flags cannot be empty");
            }
            mesh.regionById_.emplace(region.id, mesh.regions_.size());
            mesh.regions_.push_back(std::move(region));
            continue;
        }
        if (fields.size() == 4U) {
            NavMeshLink link{};
            link.id = std::string(fields[0]);
            link.fromRegion = std::string(fields[1]);
            link.toRegion = std::string(fields[2]);
            if (link.id.empty() || linkById.contains(link.id) || link.fromRegion.empty() || link.toRegion.empty()
                || !ParseBool(fields[3], link.bidirectional)) {
                return fail("link id, endpoints, or bidirectional flag are invalid");
            }
            linkById.emplace(link.id, mesh.links_.size());
            mesh.links_.push_back(std::move(link));
            continue;
        }
        return fail("expected a 9-field region or 4-field link");
    }

    if (mesh.regions_.empty()) {
        return fail("descriptor contains no navigation regions");
    }
    for (const NavMeshLink& link : mesh.links_) {
        if (!mesh.regionById_.contains(link.fromRegion) || !mesh.regionById_.contains(link.toRegion)) {
            return fail("link '" + link.id + "' references an unknown region");
        }
    }
    mesh.adjacency_.assign(mesh.regions_.size(), {});
    for (const NavMeshLink& link : mesh.links_) {
        const std::size_t from = mesh.regionById_.at(link.fromRegion);
        const std::size_t to = mesh.regionById_.at(link.toRegion);
        mesh.adjacency_[from].push_back(to);
        if (link.bidirectional) {
            mesh.adjacency_[to].push_back(from);
        }
    }
    mesh.minimumAreaCost_ = std::max(
        0.001f,
        std::min_element(mesh.regions_.begin(), mesh.regions_.end(), [](const NavMeshRegion& a, const NavMeshRegion& b) {
            return a.areaCost < b.areaCost;
        })->areaCost);
    if (error != nullptr) {
        error->clear();
    }
    return mesh;
}

const std::vector<NavMeshRegion>& NavMesh::Regions() const noexcept {
    return regions_;
}

const std::vector<NavMeshLink>& NavMesh::Links() const noexcept {
    return links_;
}

std::optional<std::size_t> NavMesh::FindContainingRegion(const ri::math::Vec3& point) const {
    std::optional<std::size_t> best;
    float bestVolume = std::numeric_limits<float>::infinity();
    for (std::size_t index = 0; index < regions_.size(); ++index) {
        const NavMeshRegion& region = regions_[index];
        if (point.x < region.minimum.x || point.x > region.maximum.x
            || point.y < region.minimum.y || point.y > region.maximum.y
            || point.z < region.minimum.z || point.z > region.maximum.z) {
            continue;
        }
        const float volume = RegionVolume(region);
        if (volume < bestVolume) {
            best = index;
            bestVolume = volume;
        }
    }
    return best;
}

NavMeshPath NavMesh::FindPath(
    const ri::math::Vec3& start,
    const ri::math::Vec3& goal,
    const NavMeshPathQuery& query) const {
    NavMeshPath result{};
    if (regions_.empty() || query.maxVisitedRegions == 0U || query.maxEndpointSnapDistance < 0.0f) {
        result.diagnostic = "Navmesh or path query limits are invalid.";
        return result;
    }

    const auto regionAllowed = [&](const std::size_t index) {
        const std::string_view flags = regions_[index].flags;
        return HasFlagToken(flags, query.requiredFlag)
            && (query.excludedFlag.empty() || !HasFlagToken(flags, query.excludedFlag));
    };
    auto resolveEndpoint = [&](const ri::math::Vec3& point) -> std::optional<std::size_t> {
        if (const std::optional<std::size_t> containing = FindContainingRegion(point);
            containing.has_value() && regionAllowed(*containing)) {
            return containing;
        }
        std::optional<std::size_t> nearest;
        float nearestDistance = query.maxEndpointSnapDistance;
        for (std::size_t index = 0; index < regions_.size(); ++index) {
            if (!regionAllowed(index)) {
                continue;
            }
            const float distance = DistanceToRegion(regions_[index], point);
            if (distance <= nearestDistance) {
                nearest = index;
                nearestDistance = distance;
            }
        }
        return nearest;
    };

    const std::optional<std::size_t> startRegion = resolveEndpoint(start);
    const std::optional<std::size_t> goalRegion = resolveEndpoint(goal);
    if (!startRegion.has_value() || !goalRegion.has_value()) {
        result.diagnostic = "Start or goal is outside the navmesh snap range.";
        return result;
    }

    struct OpenEntry {
        float score = 0.0f;
        std::size_t region = 0;
        bool operator>(const OpenEntry& other) const noexcept { return score > other.score; }
    };
    const auto heuristic = [&](const std::size_t region) {
        return ri::math::Distance(RegionCenter(regions_[region]), RegionCenter(regions_[*goalRegion]))
            * minimumAreaCost_;
    };

    std::priority_queue<OpenEntry, std::vector<OpenEntry>, std::greater<>> open;
    std::vector<float> cost(regions_.size(), std::numeric_limits<float>::infinity());
    std::vector<std::size_t> previous(regions_.size(), std::numeric_limits<std::size_t>::max());
    std::vector<bool> closed(regions_.size(), false);
    cost[*startRegion] = 0.0f;
    open.push(OpenEntry{.score = heuristic(*startRegion), .region = *startRegion});
    std::size_t visited = 0;

    while (!open.empty() && visited < query.maxVisitedRegions) {
        const std::size_t current = open.top().region;
        open.pop();
        if (closed[current]) {
            continue;
        }
        closed[current] = true;
        ++visited;
        if (current == *goalRegion) {
            break;
        }
        for (const std::size_t neighbor : adjacency_[current]) {
            if (closed[neighbor] || !regionAllowed(neighbor)) {
                continue;
            }
            const float edgeDistance =
                ri::math::Distance(RegionCenter(regions_[current]), RegionCenter(regions_[neighbor]));
            const float candidate = cost[current] + edgeDistance * regions_[neighbor].areaCost;
            if (candidate >= cost[neighbor]) {
                continue;
            }
            cost[neighbor] = candidate;
            previous[neighbor] = current;
            open.push(OpenEntry{.score = candidate + heuristic(neighbor), .region = neighbor});
        }
    }

    if (!closed[*goalRegion]) {
        result.diagnostic = visited >= query.maxVisitedRegions
            ? "Path search reached its region visit limit."
            : "No linked route connects the start and goal regions.";
        return result;
    }

    std::vector<std::size_t> reversed;
    for (std::size_t current = *goalRegion;; current = previous[current]) {
        reversed.push_back(current);
        if (current == *startRegion) {
            break;
        }
    }
    std::reverse(reversed.begin(), reversed.end());
    result.regionIds.reserve(reversed.size());
    result.waypoints.reserve(reversed.size() + 1U);
    result.waypoints.push_back(start);
    for (std::size_t index = 0; index < reversed.size(); ++index) {
        result.regionIds.push_back(regions_[reversed[index]].id);
        if (index > 0U && index + 1U < reversed.size()) {
            result.waypoints.push_back(RegionCenter(regions_[reversed[index]]));
        }
    }
    result.waypoints.push_back(goal);
    result.traversalCost = cost[*goalRegion]
        + ri::math::Distance(start, RegionCenter(regions_[*startRegion])) * regions_[*startRegion].areaCost
        + ri::math::Distance(RegionCenter(regions_[*goalRegion]), goal) * regions_[*goalRegion].areaCost;
    result.found = true;
    result.diagnostic = "Path found across " + std::to_string(result.regionIds.size()) + " region(s).";
    return result;
}

} // namespace ri::world
