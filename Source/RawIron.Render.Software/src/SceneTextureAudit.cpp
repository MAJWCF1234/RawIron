#include "RawIron/Render/SceneTextureAudit.h"
#include "RawIron/Render/PreviewTexture.h"
#include "RawIron/Content/RipakArchive.h"

#include <unordered_map>

namespace ri::render::software {

std::vector<SceneTextureAuditEntry> AuditSceneTextures(
    const ri::scene::Scene& scene, const std::filesystem::path& textureRoot,
    const std::shared_ptr<const ri::content::CookedTexturePack>& pack) {
    std::vector<SceneTextureAuditEntry> entries;
    struct Decoded { int width = 0; int height = 0; std::string error; };
    std::unordered_map<std::string, Decoded> cache;
    for (std::size_t index = 0; index < scene.MaterialCount(); ++index) {
        const auto& material = scene.GetMaterial(static_cast<int>(index));
        const auto inspect = [&](const std::string& slot, const std::string& path, bool srgb) {
            if (path.empty()) return;
            const auto* record = pack ? pack->Find(path) : nullptr;
            const bool cooked = record != nullptr;
            const auto loose = std::filesystem::absolute(textureRoot / std::filesystem::path(path)).lexically_normal();
            const std::string source = cooked ? "ripak:" + pack->Archive().Path().generic_string() + "|" + record->blobPath
                                              : loose.generic_string();
            auto [found, inserted] = cache.try_emplace(source);
            if (inserted) {
                try {
                    const auto image = cooked ? LoadRgbaImageMemory(pack->ReadPng(path)) : LoadRgbaImageFile(loose);
                    if (image.Valid()) {
                        found->second.width = image.width;
                        found->second.height = image.height;
                    } else found->second.error = "missing, invalid, or undecodable texture; fallback forbidden";
                } catch (const std::exception& error) {
                    found->second.error = "texture read failed; fallback forbidden: " + std::string(error.what());
                }
            }
            entries.push_back({material.name, slot, path, source, srgb ? "sRGB" : "linear",
                found->second.width, found->second.height, found->second.error});
        };
        inspect("albedo", material.baseColorTexture, true);
        inspect("normal", material.normalTexture, false);
        inspect("orm/specular", material.ormTexture, false);
        inspect("roughness", material.roughnessTexture, false);
        inspect("metallic", material.metallicTexture, false);
        inspect("occlusion", material.occlusionTexture, false);
        inspect("emissive", material.emissiveTexture, true);
        inspect("opacity", material.opacityTexture, false);
        inspect("detail", material.detailTexture, true);
        for (std::size_t frame = 0; frame < material.baseColorTextureFrames.size(); ++frame)
            inspect("albedo-frame-" + std::to_string(frame), material.baseColorTextureFrames[frame], true);
    }
    return entries;
}

std::string DescribeSceneTexture(const SceneTextureAuditEntry& entry) {
    return "Texture audit: material=" + entry.material + " slot=" + entry.slot
        + " requested=" + entry.requested + " source=" + entry.source
        + " dimensions=" + std::to_string(entry.width) + "x" + std::to_string(entry.height)
        + " decoded=RGBA8 colorSpace=" + entry.colorSpace
        + " fallback=" + (entry.error.empty() ? "none" : entry.error);
}

} // namespace ri::render::software
