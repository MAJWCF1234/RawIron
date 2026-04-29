#pragma once

#include "RawIron/Scene/Scene.h"

#include <filesystem>

namespace ri::scene {

bool SaveSceneNodeTransforms(const Scene& scene, const std::filesystem::path& outputPath);
bool LoadSceneNodeTransforms(Scene& scene, const std::filesystem::path& inputPath);

} // namespace ri::scene
