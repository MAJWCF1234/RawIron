#include "RawIron/Content/PackageResolver.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <functional>
#include <map>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <unordered_set>

namespace ri::content {
namespace {

struct SemanticVersion {
    unsigned int major = 0;
    unsigned int minor = 0;
    unsigned int patch = 0;

    auto operator<=>(const SemanticVersion&) const = default;
};

using RequirementMap = std::map<std::string, std::vector<std::string>, std::less<>>;
using SelectedMap = std::map<std::string, std::size_t, std::less<>>;
using CatalogGroups = std::map<std::string, std::vector<std::size_t>, std::less<>>;

[[nodiscard]] std::optional<SemanticVersion> ParseVersion(const std::string_view text) {
    SemanticVersion version{};
    unsigned int* components[] = {&version.major, &version.minor, &version.patch};
    std::size_t cursor = 0U;
    for (std::size_t component = 0U; component < 3U; ++component) {
        const std::size_t dot = text.find('.', cursor);
        const std::size_t end = dot == std::string_view::npos ? text.size() : dot;
        if (cursor == end || (component < 2U && dot == std::string_view::npos)
            || (component == 2U && dot != std::string_view::npos)) {
            return std::nullopt;
        }
        const char* begin = text.data() + cursor;
        const char* finish = text.data() + end;
        const auto parsed = std::from_chars(begin, finish, *components[component]);
        if (parsed.ec != std::errc{} || parsed.ptr != finish) {
            return std::nullopt;
        }
        cursor = end + 1U;
    }
    return version;
}

[[nodiscard]] std::vector<std::string> RequirementTokens(std::string_view requirement) {
    std::string normalized(requirement);
    std::replace(normalized.begin(), normalized.end(), ',', ' ');
    std::istringstream input(normalized);
    std::vector<std::string> tokens;
    std::string token;
    while (input >> token) {
        tokens.push_back(std::move(token));
    }
    return tokens;
}

[[nodiscard]] bool VersionMatchesToken(
    const SemanticVersion& version,
    const std::string_view token) {
    if (token.empty() || token == "*") {
        return true;
    }

    std::string_view operand = token;
    enum class Operation { Equal, Greater, GreaterEqual, Less, LessEqual, Caret, Tilde };
    Operation operation = Operation::Equal;
    if (token.starts_with(">=")) {
        operation = Operation::GreaterEqual;
        operand.remove_prefix(2U);
    } else if (token.starts_with("<=")) {
        operation = Operation::LessEqual;
        operand.remove_prefix(2U);
    } else if (token.starts_with(">")) {
        operation = Operation::Greater;
        operand.remove_prefix(1U);
    } else if (token.starts_with("<")) {
        operation = Operation::Less;
        operand.remove_prefix(1U);
    } else if (token.starts_with("=")) {
        operand.remove_prefix(1U);
    } else if (token.starts_with("^")) {
        operation = Operation::Caret;
        operand.remove_prefix(1U);
    } else if (token.starts_with("~")) {
        operation = Operation::Tilde;
        operand.remove_prefix(1U);
    }

    const std::optional<SemanticVersion> expected = ParseVersion(operand);
    if (!expected.has_value()) {
        return false;
    }
    switch (operation) {
    case Operation::Equal: return version == *expected;
    case Operation::Greater: return version > *expected;
    case Operation::GreaterEqual: return version >= *expected;
    case Operation::Less: return version < *expected;
    case Operation::LessEqual: return version <= *expected;
    case Operation::Tilde:
        if (expected->minor < std::numeric_limits<unsigned int>::max()) {
            return version >= *expected
                && version < SemanticVersion{expected->major, expected->minor + 1U, 0U};
        }
        if (expected->major < std::numeric_limits<unsigned int>::max()) {
            return version >= *expected
                && version < SemanticVersion{expected->major + 1U, 0U, 0U};
        }
        return version >= *expected;
    case Operation::Caret: {
        std::optional<SemanticVersion> upper;
        if (expected->major > 0U) {
            if (expected->major < std::numeric_limits<unsigned int>::max()) {
                upper = SemanticVersion{expected->major + 1U, 0U, 0U};
            }
        } else if (expected->minor > 0U) {
            if (expected->minor < std::numeric_limits<unsigned int>::max()) {
                upper = SemanticVersion{0U, expected->minor + 1U, 0U};
            }
        } else {
            if (expected->patch < std::numeric_limits<unsigned int>::max()) {
                upper = SemanticVersion{0U, 0U, expected->patch + 1U};
            }
        }
        return version >= *expected && (!upper.has_value() || version < *upper);
    }
    }
    return false;
}

[[nodiscard]] bool ContainsToken(
    const std::vector<std::string>& values,
    const std::string_view wanted) {
    return std::find(values.begin(), values.end(), wanted) != values.end();
}

[[nodiscard]] std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

[[nodiscard]] bool PlatformMatches(
    const AssetPackageManifest& manifest,
    const std::string_view platform) {
    if (manifest.supportedPlatforms.empty() || platform.empty()) {
        return true;
    }
    const std::string normalizedPlatform = LowerAscii(std::string(platform));
    return std::any_of(
        manifest.supportedPlatforms.begin(),
        manifest.supportedPlatforms.end(),
        [&](const std::string& candidate) {
            const std::string normalized = LowerAscii(candidate);
            return normalized == "any" || normalized == normalizedPlatform;
        });
}

[[nodiscard]] bool PermissionsAllowed(
    const AssetPackageManifest& manifest,
    const PackageResolverOptions& options) {
    if (!options.enforcePermissions) {
        return true;
    }
    return std::all_of(
        manifest.permissions.begin(),
        manifest.permissions.end(),
        [&](const std::string& permission) {
            return ContainsToken(options.grantedPermissions, permission);
        });
}

[[nodiscard]] bool ConflictsWithSelection(
    const AssetPackageManifest& candidate,
    const std::span<const AssetPackageManifest> catalog,
    const SelectedMap& selected) {
    for (const auto& [selectedId, selectedIndex] : selected) {
        const AssetPackageManifest& existing = catalog[selectedIndex];
        if (ContainsToken(candidate.conflicts, selectedId)
            || ContainsToken(existing.conflicts, candidate.packageId)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool CandidateSatisfiesRequirements(
    const AssetPackageManifest& candidate,
    const std::vector<std::string>& requirements) {
    return std::all_of(
        requirements.begin(),
        requirements.end(),
        [&](const std::string& requirement) {
            return PackageVersionSatisfies(candidate.packageVersion, requirement);
        });
}

[[nodiscard]] bool BaseCompatible(
    const AssetPackageManifest& candidate,
    const PackageResolverOptions& options) {
    return ParseVersion(candidate.packageVersion).has_value()
        && PackageVersionSatisfies(options.engineApiVersion, candidate.engineApiRequirement)
        && PlatformMatches(candidate, options.platform)
        && PermissionsAllowed(candidate, options);
}

[[nodiscard]] std::string ExplainMissingCandidate(
    const std::string& id,
    const std::vector<std::string>& constraints,
    const CatalogGroups& groups,
    const std::span<const AssetPackageManifest> catalog,
    const SelectedMap& selected,
    const PackageResolverOptions& options) {
    const auto group = groups.find(id);
    if (group == groups.end()) {
        return "Required package '" + id + "' was not found in the package catalog.";
    }

    std::vector<std::size_t> versionMatches;
    for (const std::size_t index : group->second) {
        if (CandidateSatisfiesRequirements(catalog[index], constraints)) {
            versionMatches.push_back(index);
        }
    }
    if (versionMatches.empty()) {
        std::ostringstream message;
        message << "No version of package '" << id << "' satisfies";
        for (const std::string& constraint : constraints) {
            message << ' ' << (constraint.empty() ? "*" : constraint);
        }
        message << '.';
        return message.str();
    }

    const auto firstMatching = [&](const auto& predicate) -> const AssetPackageManifest* {
        const auto match = std::find_if(versionMatches.begin(), versionMatches.end(), [&](const std::size_t index) {
            return predicate(catalog[index]);
        });
        return match == versionMatches.end() ? nullptr : &catalog[*match];
    };
    if (firstMatching([&](const AssetPackageManifest& candidate) {
            return PackageVersionSatisfies(options.engineApiVersion, candidate.engineApiRequirement);
        }) == nullptr) {
        return "Package '" + id + "' has no version compatible with engine API "
            + options.engineApiVersion + ".";
    }
    if (const AssetPackageManifest* candidate = firstMatching([&](const AssetPackageManifest& value) {
            return PackageVersionSatisfies(options.engineApiVersion, value.engineApiRequirement)
                && PlatformMatches(value, options.platform);
        }); candidate == nullptr) {
        return "Package '" + id + "' does not support platform '" + options.platform + "'.";
    }
    if (options.enforcePermissions) {
        for (const std::size_t index : versionMatches) {
            const AssetPackageManifest& candidate = catalog[index];
            if (!PackageVersionSatisfies(options.engineApiVersion, candidate.engineApiRequirement)
                || !PlatformMatches(candidate, options.platform)) {
                continue;
            }
            for (const std::string& permission : candidate.permissions) {
                if (!ContainsToken(options.grantedPermissions, permission)) {
                    return "Package '" + id + "' requires ungranted permission '" + permission + "'.";
                }
            }
        }
    }
    for (const std::size_t index : versionMatches) {
        const AssetPackageManifest& candidate = catalog[index];
        if (BaseCompatible(candidate, options) && ConflictsWithSelection(candidate, catalog, selected)) {
            return "Package '" + id + "' conflicts with the selected package set.";
        }
    }
    return "No compatible version of package '" + id + "' is available.";
}

[[nodiscard]] bool ValidateCompletedSelection(
    const std::span<const AssetPackageManifest> catalog,
    const SelectedMap& selected,
    const PackageResolverOptions& options,
    std::string& reason) {
    std::set<std::string, std::less<>> capabilities(
        options.engineCapabilities.begin(),
        options.engineCapabilities.end());
    for (const auto& [id, index] : selected) {
        const AssetPackageManifest& manifest = catalog[index];
        capabilities.insert(manifest.providesCapabilities.begin(), manifest.providesCapabilities.end());
        for (const std::string& conflict : manifest.conflicts) {
            if (selected.contains(conflict)) {
                reason = "Package '" + id + "' conflicts with selected package '" + conflict + "'.";
                return false;
            }
        }
    }
    for (const auto& [id, index] : selected) {
        for (const std::string& required : catalog[index].requiredCapabilities) {
            if (!capabilities.contains(required)) {
                reason = "Package '" + id + "' requires unavailable capability '" + required + "'.";
                return false;
            }
        }
    }

    std::set<std::string, std::less<>> visiting;
    std::set<std::string, std::less<>> visited;
    std::function<bool(const std::string&)> visit = [&](const std::string& id) {
        if (visited.contains(id)) {
            return true;
        }
        if (!visiting.insert(id).second) {
            reason = "Package dependency cycle includes '" + id + "'.";
            return false;
        }
        const AssetPackageManifest& manifest = catalog[selected.at(id)];
        for (const AssetPackageDependency& dependency : manifest.dependencies) {
            if ((!dependency.optional || options.includeOptionalDependencies)
                && selected.contains(dependency.packageId)
                && !visit(dependency.packageId)) {
                return false;
            }
        }
        visiting.erase(id);
        visited.insert(id);
        return true;
    };
    for (const auto& [id, index] : selected) {
        (void)index;
        if (!visit(id)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool Solve(
    const std::span<const AssetPackageManifest> catalog,
    const CatalogGroups& groups,
    RequirementMap requirements,
    SelectedMap selected,
    const PackageResolverOptions& options,
    SelectedMap& solution,
    std::string& reason) {
    for (const auto& [id, index] : selected) {
        const auto requirement = requirements.find(id);
        if (requirement != requirements.end()
            && !CandidateSatisfiesRequirements(catalog[index], requirement->second)) {
            reason = "Selected package '" + id + "' no longer satisfies all version requirements.";
            return false;
        }
    }

    std::string nextId;
    std::vector<std::size_t> nextCandidates;
    bool foundUnresolved = false;
    for (const auto& [id, constraints] : requirements) {
        if (selected.contains(id)) {
            continue;
        }
        const auto group = groups.find(id);
        std::vector<std::size_t> candidates;
        if (group != groups.end()) {
            for (const std::size_t index : group->second) {
                const AssetPackageManifest& candidate = catalog[index];
                if (CandidateSatisfiesRequirements(candidate, constraints)
                    && BaseCompatible(candidate, options)
                    && !ConflictsWithSelection(candidate, catalog, selected)) {
                    candidates.push_back(index);
                }
            }
        }
        if (candidates.empty()) {
            reason = ExplainMissingCandidate(
                id,
                constraints,
                groups,
                catalog,
                selected,
                options);
            return false;
        }
        if (!foundUnresolved || candidates.size() < nextCandidates.size()) {
            foundUnresolved = true;
            nextId = id;
            nextCandidates = std::move(candidates);
        }
    }

    if (!foundUnresolved) {
        if (!ValidateCompletedSelection(catalog, selected, options, reason)) {
            return false;
        }
        solution = std::move(selected);
        return true;
    }

    for (const std::size_t candidateIndex : nextCandidates) {
        RequirementMap branchRequirements = requirements;
        SelectedMap branchSelected = selected;
        branchSelected[nextId] = candidateIndex;
        const AssetPackageManifest& candidate = catalog[candidateIndex];
        for (const AssetPackageDependency& dependency : candidate.dependencies) {
            if (dependency.optional && !options.includeOptionalDependencies) {
                continue;
            }
            if (dependency.optional && !groups.contains(dependency.packageId)) {
                continue;
            }
            branchRequirements[dependency.packageId].push_back(
                dependency.versionRequirement.empty() ? "*" : dependency.versionRequirement);
        }
        std::string branchReason;
        if (Solve(
                catalog,
                groups,
                std::move(branchRequirements),
                std::move(branchSelected),
                options,
                solution,
                branchReason)) {
            return true;
        }
        reason = std::move(branchReason);
    }
    return false;
}

} // namespace

bool IsValidPackageVersionRequirement(const std::string_view requirement) {
    if (requirement.empty() || requirement == "*") {
        return true;
    }
    const std::vector<std::string> tokens = RequirementTokens(requirement);
    if (tokens.empty()) {
        return false;
    }
    return std::all_of(tokens.begin(), tokens.end(), [&](const std::string& token) {
        if (token == "*" || token.empty()) {
            return true;
        }
        std::string_view operand = token;
        if (operand.starts_with(">=") || operand.starts_with("<=")) {
            operand.remove_prefix(2U);
        } else if (operand.starts_with(">") || operand.starts_with("<")
                   || operand.starts_with("=") || operand.starts_with("^")
                   || operand.starts_with("~")) {
            operand.remove_prefix(1U);
        }
        return ParseVersion(operand).has_value();
    });
}

bool PackageVersionSatisfies(
    const std::string_view version,
    const std::string_view requirement) {
    const std::optional<SemanticVersion> parsedVersion = ParseVersion(version);
    if (!parsedVersion.has_value() || !IsValidPackageVersionRequirement(requirement)) {
        return false;
    }
    for (const std::string& token : RequirementTokens(requirement)) {
        if (!VersionMatchesToken(*parsedVersion, token)) {
            return false;
        }
    }
    return true;
}

PackageResolutionResult ResolvePackages(
    const std::span<const AssetPackageManifest> catalog,
    const std::span<const PackageRequest> roots,
    const PackageResolverOptions& options) {
    PackageResolutionResult result{};
    if (!ParseVersion(options.engineApiVersion).has_value()) {
        result.issues.push_back("Package resolver engineApiVersion must be a semantic version triplet.");
        return result;
    }
    if (roots.empty()) {
        result.issues.push_back("Package resolver requires at least one root package.");
        return result;
    }

    CatalogGroups groups;
    for (std::size_t index = 0U; index < catalog.size(); ++index) {
        const AssetPackageManifest& manifest = catalog[index];
        if (manifest.packageId.empty() || !ParseVersion(manifest.packageVersion).has_value()
            || !IsValidPackageVersionRequirement(manifest.engineApiRequirement)) {
            continue;
        }
        groups[manifest.packageId].push_back(index);
    }
    for (auto& [id, candidates] : groups) {
        (void)id;
        std::stable_sort(candidates.begin(), candidates.end(), [&](const std::size_t lhs, const std::size_t rhs) {
            return *ParseVersion(catalog[lhs].packageVersion) > *ParseVersion(catalog[rhs].packageVersion);
        });
    }

    RequirementMap requirements;
    for (const PackageRequest& root : roots) {
        if (root.packageId.empty() || !IsValidPackageVersionRequirement(root.versionRequirement)) {
            result.issues.push_back("Package root request has an invalid id or version requirement.");
            return result;
        }
        requirements[root.packageId].push_back(
            root.versionRequirement.empty() ? "*" : root.versionRequirement);
    }

    SelectedMap solution;
    std::string reason;
    if (!Solve(catalog, groups, std::move(requirements), {}, options, solution, reason)) {
        result.issues.push_back(reason.empty() ? "Package dependency resolution failed." : std::move(reason));
        return result;
    }

    std::set<std::string, std::less<>> emitted;
    std::function<void(const std::string&)> emit = [&](const std::string& id) {
        if (emitted.contains(id)) {
            return;
        }
        const std::size_t index = solution.at(id);
        for (const AssetPackageDependency& dependency : catalog[index].dependencies) {
            if (solution.contains(dependency.packageId)) {
                emit(dependency.packageId);
            }
        }
        emitted.insert(id);
        result.loadOrder.push_back({
            .catalogIndex = index,
            .packageId = catalog[index].packageId,
            .packageVersion = catalog[index].packageVersion,
        });
    };
    for (const auto& [id, index] : solution) {
        (void)index;
        emit(id);
    }
    result.resolved = true;
    return result;
}

} // namespace ri::content
