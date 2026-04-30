#include "RawIron/Scene/AnimationLibraryHydration.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <algorithm>
#include <cctype>

namespace ri::scene {
namespace {

std::string NormalizeBoneAlias(std::string_view name) {
    std::string out{};
    out.reserve(name.size());
    for (char ch : name) {
        if (std::isalnum(static_cast<unsigned char>(ch)) != 0) {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    return out;
}

std::unordered_map<std::string, std::string> BuildBoneAliasLookup(const Scene& scene) {
    std::unordered_map<std::string, std::string> lookup{};
    for (std::size_t i = 0; i < scene.NodeCount(); ++i) {
        const Node& node = scene.GetNode(static_cast<int>(i));
        const std::string canonical = NormalizeBoneAlias(node.name);
        if (canonical.empty() || lookup.contains(canonical)) {
            continue;
        }
        lookup.emplace(canonical, node.name);
    }
    return lookup;
}

std::vector<AnimationLibraryDefinition> ParseProfileLibraries(std::string_view profileObj, std::string_view profileName) {
    std::vector<AnimationLibraryDefinition> out{};
    const std::vector<std::string_view> libraries = ri::core::detail::SplitJsonArrayObjects(profileObj, "libraries");
    out.reserve(libraries.size());
    for (const std::string_view entry : libraries) {
        AnimationLibraryDefinition def{};
        def.sourcePath = ri::core::detail::ExtractJsonString(entry, "path").value_or("");
        def.profileName = std::string(profileName);
        def.priority = static_cast<std::int32_t>(ri::core::detail::ExtractJsonInt(entry, "priority").value_or(0));
        const std::vector<std::string_view> aliasEntries = ri::core::detail::SplitJsonArrayObjects(entry, "clipAlias");
        for (const std::string_view aliasEntry : aliasEntries) {
            const std::string from = ri::core::detail::ExtractJsonString(aliasEntry, "from").value_or("");
            const std::string to = ri::core::detail::ExtractJsonString(aliasEntry, "to").value_or("");
            if (!from.empty() && !to.empty()) {
                def.clipAlias[from] = to;
            }
        }
        if (!def.sourcePath.empty()) {
            out.push_back(std::move(def));
        }
    }
    return out;
}

} // namespace

ProfileAnimationLibraryDefinitions GetProfileAnimationLibraryDefinitions(std::string_view gameAssetsJsonText) {
    ProfileAnimationLibraryDefinitions defs{};
    const std::vector<std::string_view> profileObjects =
        ri::core::detail::SplitJsonArrayObjects(gameAssetsJsonText, "animationLibraryProfiles");
    for (const std::string_view profileObj : profileObjects) {
        const std::string profileName = ri::core::detail::ExtractJsonString(profileObj, "profile").value_or("");
        if (profileName.empty()) {
            continue;
        }
        defs[profileName] = ParseProfileLibraries(profileObj, profileName);
    }
    return defs;
}

std::future<LoadedAnimationLibrarySource> LoadAnimationLibrarySource(const AnimationLibraryDefinition& definition) {
    return std::async(std::launch::async, [definition]() {
        LoadedAnimationLibrarySource out{};
        out.sourcePath = definition.sourcePath;
        out.profileName = definition.profileName;
        out.priority = definition.priority;
        out.clipAlias = definition.clipAlias;
        if (definition.sourcePath.empty()) {
            out.error = "Animation library source path is empty.";
            return out;
        }
        const std::filesystem::path path(definition.sourcePath);
        if (!std::filesystem::exists(path)) {
            out.error = "Animation library source does not exist: " + definition.sourcePath;
            return out;
        }

        // RawIron ingestion currently imports structure/meshes via glTF; clip extraction can be added in a
        // subsequent pass when animation channels are promoted into Scene clip tracks.
        out.success = true;
        return out;
    });
}

HumanoidAnimationSource RegisterHumanoidAnimationSource(HumanoidAnimationSourceRegistry& registry,
                                                        const Scene& sourceScene,
                                                        const LoadedAnimationLibrarySource& librarySource) {
    HumanoidAnimationSource entry{};
    entry.sourcePath = librarySource.sourcePath;
    entry.profileName = librarySource.profileName;
    entry.priority = librarySource.priority;
    entry.normalizedBoneAliasLookup = BuildBoneAliasLookup(sourceScene);

    for (const AnimationClip& clip : librarySource.clips) {
        const auto aliasIt = librarySource.clipAlias.find(clip.name);
        const std::string key = aliasIt != librarySource.clipAlias.end() ? aliasIt->second : clip.name;
        if (!key.empty()) {
            entry.clipMap[key] = clip;
        }
    }

    registry.humanoidLibraries.push_back(entry);
    std::stable_sort(registry.humanoidLibraries.begin(), registry.humanoidLibraries.end(), [](const auto& a, const auto& b) {
        return a.priority > b.priority;
    });
    if (!entry.profileName.empty()) {
        const auto it = registry.humanoidStandard.find(entry.profileName);
        if (it == registry.humanoidStandard.end() || it->second.priority < entry.priority) {
            registry.humanoidStandard[entry.profileName] = entry;
        }
    }
    return entry;
}

} // namespace ri::scene

