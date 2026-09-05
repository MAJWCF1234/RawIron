#pragma once

#include "RawIron/Scene/Scene.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ri::content { class CookedTexturePack; }
namespace ri::render::software {

struct SceneTextureAuditEntry {
    std::string material;
    std::string slot;
    std::string requested;
    std::string source;
    std::string colorSpace;
    int width = 0;
    int height = 0;
    std::string error;
};

// Strict authoring/preflight audit: no extension substitution or generated replacement.
// An existing package entry takes precedence; corrupt package data never falls through to loose files.
// Empty optional slots are intentional scalar/default bindings and are omitted.
[[nodiscard]] std::vector<SceneTextureAuditEntry> AuditSceneTextures(
    const ri::scene::Scene& scene, const std::filesystem::path& textureRoot = {},
    const std::shared_ptr<const ri::content::CookedTexturePack>& pack = {});
[[nodiscard]] std::string DescribeSceneTexture(const SceneTextureAuditEntry& entry);

} // namespace ri::render::software
