#pragma once

#include "RawIron/Scene/Helpers.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::scene {

struct SceneKitExampleDefinition {
    std::string slug;
    std::string title;
    std::string officialUrl;
    std::string rawIronTrack;
    std::string statusLabel;
};

struct SceneKitPreview {
    std::string slug;
    std::string title;
    std::string officialUrl;
    std::string rawIronTrack;
    std::string statusLabel;
    std::string detail;
    Scene scene{"SceneKitPreview"};
    OrbitCameraHandles orbitCamera{};
    int focusNode = kInvalidHandle;
};

struct SceneKitMilestoneOptions {
    std::filesystem::path assetRoot;
};

struct SceneKitMilestoneCallbacks {
    std::function<bool(const std::string& slug, const Scene& scene, int cameraNode, std::string& detail)> renderValidator;
};

struct SceneKitMilestoneResult {
    std::string slug;
    std::string title;
    std::string officialUrl;
    std::string rawIronTrack;
    std::string statusLabel;
    bool passed = false;
    std::string detail;
    Scene scene{"SceneKitMilestone"};
    int cameraNode = kInvalidHandle;
    int focusNode = kInvalidHandle;
};

std::vector<SceneKitExampleDefinition> GetSceneKitExampleDefinitions();
SceneKitPreview BuildLitCubeSceneKitPreview();
std::optional<SceneKitPreview> BuildSceneKitPreview(std::string_view slug,
                                                    const SceneKitMilestoneOptions& options = {});
std::optional<SceneKitMilestoneResult> BuildSceneKitMilestone(std::string_view slug,
                                                              const SceneKitMilestoneOptions& options = {},
                                                              const SceneKitMilestoneCallbacks& callbacks = {});
std::vector<SceneKitMilestoneResult> RunSceneKitMilestoneChecks(const SceneKitMilestoneOptions& options,
                                                                const SceneKitMilestoneCallbacks& callbacks = {});
bool AllSceneKitMilestonesPassed(const std::vector<SceneKitMilestoneResult>& results);

} // namespace ri::scene
