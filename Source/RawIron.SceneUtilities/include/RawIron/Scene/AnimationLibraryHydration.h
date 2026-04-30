#pragma once

#include "RawIron/Scene/Animation.h"
#include "RawIron/Scene/Scene.h"

#include <cstdint>
#include <filesystem>
#include <future>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::scene {

struct AnimationLibraryDefinition {
    std::string sourcePath{};
    std::string profileName{"humanoid_default"};
    std::int32_t priority = 0;
    std::unordered_map<std::string, std::string> clipAlias{};
};

using ProfileAnimationLibraryDefinitions = std::unordered_map<std::string, std::vector<AnimationLibraryDefinition>>;

[[nodiscard]] ProfileAnimationLibraryDefinitions GetProfileAnimationLibraryDefinitions(std::string_view gameAssetsJsonText);

struct LoadedAnimationLibrarySource {
    bool success = false;
    std::string error{};
    std::string sourcePath{};
    std::string profileName{"humanoid_default"};
    std::int32_t priority = 0;
    std::vector<AnimationClip> clips{};
    std::unordered_map<std::string, std::string> clipAlias{};
};

[[nodiscard]] std::future<LoadedAnimationLibrarySource> LoadAnimationLibrarySource(
    const AnimationLibraryDefinition& definition);

struct HumanoidAnimationSource {
    std::string sourcePath{};
    std::string profileName{"humanoid_default"};
    std::int32_t priority = 0;
    std::unordered_map<std::string, AnimationClip> clipMap{};
    std::unordered_map<std::string, std::string> normalizedBoneAliasLookup{};
};

struct HumanoidAnimationSourceRegistry {
    std::vector<HumanoidAnimationSource> humanoidLibraries{};
    std::unordered_map<std::string, HumanoidAnimationSource> humanoidStandard{};
};

[[nodiscard]] HumanoidAnimationSource RegisterHumanoidAnimationSource(
    HumanoidAnimationSourceRegistry& registry,
    const Scene& sourceScene,
    const LoadedAnimationLibrarySource& librarySource);

} // namespace ri::scene

