#include "RawIron/Content/EngineAssets.h"

#include "RawIron/Content/GameManifest.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace {

namespace fs = std::filesystem;

bool TextureLibraryDirectoryP(const fs::path& dir) {
    std::error_code ec{};
    if (!fs::is_directory(dir, ec) || ec) {
        return false;
    }
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) {
            return false;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string ext = entry.path().extension().string();
        for (char& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (ext == ".png") {
            return true;
        }
    }
    return false;
}

fs::path MainModuleDirectory() {
#if defined(_WIN32)
    wchar_t buffer[4096]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0U || length >= static_cast<DWORD>(std::size(buffer))) {
        return {};
    }
    buffer[length] = L'\0';
    return fs::path(std::wstring(buffer)).parent_path();
#elif defined(__linux__)
    char pathBuf[4096]{};
    const ssize_t n = ::readlink("/proc/self/exe", pathBuf, sizeof(pathBuf) - 1U);
    if (n <= 0) {
        return {};
    }
    pathBuf[static_cast<std::size_t>(n)] = '\0';
    return fs::path(pathBuf).parent_path();
#else
    return {};
#endif
}

void PushUnique(std::vector<fs::path>& out, fs::path value) {
    value = value.lexically_normal();
    if (value.empty()) {
        return;
    }
    const auto same = [&value](const fs::path& p) {
        return p == value;
    };
    if (std::find_if(out.begin(), out.end(), same) != out.end()) {
        return;
    }
    out.push_back(std::move(value));
}

std::vector<fs::path> SearchAnchorDirectories(const fs::path& applicationPath) {
    std::vector<fs::path> anchors;
    if (!applicationPath.empty()) {
        fs::path anchor = applicationPath.lexically_normal();
        if (fs::is_regular_file(anchor)) {
            anchor = anchor.parent_path();
        }
        PushUnique(anchors, anchor);
    }

    PushUnique(anchors, MainModuleDirectory());
    PushUnique(anchors, fs::current_path());

    PushUnique(anchors, ri::content::DetectWorkspaceRoot(MainModuleDirectory()));
    PushUnique(anchors, ri::content::DetectWorkspaceRoot(fs::current_path()));

    return anchors;
}

std::optional<fs::path> TryResolveFromWalkUp(const fs::path& startAnchor) {
    if (startAnchor.empty()) {
        return std::nullopt;
    }

    fs::path cursor = startAnchor.lexically_normal();
    for (int step = 0; step < 16; ++step) {
        const fs::path assetsTextures = cursor / "Assets" / "Textures";
        if (TextureLibraryDirectoryP(assetsTextures)) {
            return assetsTextures;
        }
        const fs::path legacyEngineTextures = cursor / "Engine" / "Textures";
        if (TextureLibraryDirectoryP(legacyEngineTextures)) {
            return legacyEngineTextures;
        }
        if (!cursor.has_parent_path() || cursor == cursor.root_path()) {
            break;
        }
        cursor = cursor.parent_path();
    }
    return std::nullopt;
}

void AppendLowercaseKey(std::string* out, std::string_view piece) {
    out->reserve(out->size() + piece.size());
    for (char ch : piece) {
        out->push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
}

void DiscoverTextureAliasesRecursive(const fs::path& root,
                                     const fs::path& cursor,
                                     int depth,
                                     std::unordered_map<std::string, std::string>& out,
                                     std::size_t maxEntries,
                                     std::error_code& ec) {
    if (out.size() >= maxEntries || depth > 14) {
        return;
    }
    std::vector<fs::directory_entry> entries;
    for (const auto& entry : fs::directory_iterator(cursor, ec)) {
        if (ec) {
            return;
        }
        entries.push_back(entry);
    }
    std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& lhs, const fs::directory_entry& rhs) {
        return lhs.path().generic_string() < rhs.path().generic_string();
    });

    for (const auto& entry : entries) {
        if (entry.is_directory()) {
            DiscoverTextureAliasesRecursive(root, entry.path(), depth + 1, out, maxEntries, ec);
            if (ec) {
                return;
            }
            continue;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string ext = entry.path().extension().string();
        for (char& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (ext != ".png") {
            continue;
        }
        const fs::path rel = fs::relative(entry.path(), root, ec);
        if (ec) {
            ec.clear();
            continue;
        }
        std::string relStr = rel.generic_string();
        for (char& c : relStr) {
            if (c == '\\') {
                c = '/';
            }
        }
        if (relStr.size() >= 4) {
            std::string tail = relStr.substr(relStr.size() - 4U);
            for (char& c : tail) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (tail == ".png") {
                relStr.resize(relStr.size() - 4U);
            }
        }
        std::string keyFull = relStr;
        for (char& c : keyFull) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        out[keyFull] = relStr;

        const std::string stem = entry.path().stem().string();
        std::string stemKey;
        AppendLowercaseKey(&stemKey, stem);
        if (!stemKey.empty() && out.find(stemKey) == out.end()) {
            out[stemKey] = relStr;
        }
        if (out.size() >= maxEntries) {
            return;
        }
    }
}

} // namespace

namespace ri::content {

const TextureAliasManifest& GetTextureAliasManifest() {
    static const TextureAliasManifest kManifest = {
        {"dev_floor", "materials/dev/dev_floor"},
        {"dev_wall", "materials/dev/dev_wall"},
        {"dev_ceiling", "materials/dev/dev_ceiling"},
        {"dev_trim", "materials/dev/dev_trim"},
        {"dev_grid", "materials/dev/dev_grid"},
        {"void_floor", "materials/liminal/void_floor"},
        {"void_wall", "materials/liminal/void_wall"},
        {"void_trim", "materials/liminal/void_trim"},
        {"portal_ring", "materials/liminal/portal_ring"},
        {"forest_ground", "materials/forest/ground"},
        {"forest_stone", "materials/forest/stone"},
        {"forest_wood", "materials/forest/wood"},
        {"forest_moss", "materials/forest/moss"},
        {"forest_bark", "materials/forest/bark"},
        {"brick", "materials/common/brick"},
        {"concrete", "materials/common/concrete"},
        {"metal", "materials/common/metal"},
        {"glass", "materials/common/glass"},
        {"water", "materials/common/water"},
        {"plaster", "materials/common/plaster"},
        {"carpet", "materials/common/carpet"},
        {"rubber", "materials/common/rubber"},
        {"plastic", "materials/common/plastic"},
        {"ceramic", "materials/common/ceramic"},
        {"fabric", "materials/common/fabric"},
        {"leather", "materials/common/leather"},
        {"rope", "materials/common/rope"},
        {"grass", "materials/terrain/grass"},
        {"gravel", "materials/terrain/gravel"},
        {"sand", "materials/terrain/sand"},
        {"snow", "materials/terrain/snow"},
        {"ice", "materials/terrain/ice"},
        {"mud", "materials/terrain/mud"},
        {"clay", "materials/terrain/clay"},
        {"foliage", "materials/foliage/default"},
        {"bark", "materials/foliage/bark"},
        {"moss", "materials/foliage/moss"},
        {"noise_clouds", "materials/procedural/noise_clouds"},
        {"noise_perlin", "materials/procedural/noise_perlin"},
        {"noise_voronoi", "materials/procedural/noise_voronoi"},
        {"detail_scratch", "materials/wear/scratch"},
        {"detail_rust", "materials/wear/rust"},
        {"detail_dirt", "materials/wear/dirt"},
        {"trim_metal", "materials/trim/metal"},
        {"trim_wood", "materials/trim/wood"},
        {"panel_acoustic", "materials/architecture/acoustic_panel"},
        {"tile_floor", "materials/architecture/floor_tile"},
        {"tile_wall", "materials/architecture/wall_tile"},
        {"insulation_foil", "materials/architecture/insulation"},
        {"hazard_stripes", "materials/signage/hazard_stripes"},
        {"sign_exit", "materials/signage/exit"},
    };
    return kManifest;
}

std::string ResolveTextureAliasWithManifest(const std::string_view authoredTextureName, const TextureAliasManifest& manifest) {
    if (authoredTextureName.empty()) {
        return {};
    }
    std::string key(authoredTextureName);
    std::transform(key.begin(), key.end(), key.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    const auto found = manifest.find(key);
    if (found != manifest.end()) {
        return found->second;
    }
    return std::string(authoredTextureName);
}

std::string ResolveTextureAlias(const std::string_view authoredTextureName) {
    return ResolveTextureAliasWithManifest(authoredTextureName, GetTextureAliasManifest());
}

TextureAliasManifest MergeTextureAliasManifestsOverlayWins(const TextureAliasManifest& base, const TextureAliasManifest& overlay) {
    TextureAliasManifest merged = base;
    for (const auto& entry : overlay) {
        merged[entry.first] = entry.second;
    }
    return merged;
}

TextureAliasManifest DiscoverTextureAliasesUnderTexturesRoot(const fs::path& texturesRoot, const std::size_t maxEntries) {
    TextureAliasManifest discovered{};
    if (maxEntries == 0U) {
        return discovered;
    }
    std::error_code ec{};
    if (!fs::is_directory(texturesRoot, ec) || ec) {
        return discovered;
    }
    DiscoverTextureAliasesRecursive(texturesRoot, texturesRoot, 0, discovered, maxEntries, ec);
    return discovered;
}

TextureAliasManifest BuildHydratedTextureAliasManifest(const fs::path& texturesRoot, const std::size_t maxEntries) {
    return MergeTextureAliasManifestsOverlayWins(GetTextureAliasManifest(),
                                                 DiscoverTextureAliasesUnderTexturesRoot(texturesRoot, maxEntries));
}

std::string ResolveAssetVariantId(const AssetVariantResolveRequest& request,
                                  const AssetVariantMap& variantManifest,
                                  const TextureAliasManifest& textureAliases) {
    if (request.logicalAssetId.empty()) {
        return {};
    }
    const auto lookupVariant = [&](std::string_view key) -> std::string {
        const auto byAsset = variantManifest.find(request.logicalAssetId);
        if (byAsset == variantManifest.end()) {
            return {};
        }
        const auto found = byAsset->second.find(std::string(key));
        return found == byAsset->second.end() ? std::string{} : found->second;
    };

    if (!request.variant.empty()) {
        if (const std::string resolved = lookupVariant(request.variant); !resolved.empty()) {
            return resolved;
        }
    }
    if (!request.platform.empty()) {
        if (const std::string resolved = lookupVariant(std::string("platform:") + request.platform); !resolved.empty()) {
            return resolved;
        }
    }
    if (!request.qualityTier.empty()) {
        if (const std::string resolved = lookupVariant(std::string("quality:") + request.qualityTier); !resolved.empty()) {
            return resolved;
        }
    }
    if (const std::string resolved = lookupVariant("default"); !resolved.empty()) {
        return resolved;
    }
    return ResolveTextureAliasWithManifest(request.logicalAssetId, textureAliases);
}

bool ValidateAssetVariantManifest(const AssetVariantMap& variantManifest, std::string* error) {
    for (const auto& [logicalId, variants] : variantManifest) {
        if (logicalId.empty()) {
            if (error != nullptr) {
                *error = "Asset variant manifest contains an empty logical asset id.";
            }
            return false;
        }
        for (const auto& [variantKey, concreteId] : variants) {
            if (variantKey.empty() || concreteId.empty()) {
                if (error != nullptr) {
                    std::ostringstream message;
                    message << "Asset variant entry for '" << logicalId << "' has empty key or value.";
                    *error = message.str();
                }
                return false;
            }
        }
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool IsEngineTextureLibraryDirectory(const std::filesystem::path& dir) {
    return TextureLibraryDirectoryP(dir);
}

std::filesystem::path ResolveEngineTexturesDirectory(const std::filesystem::path& applicationPath) {
    for (const std::filesystem::path& anchor : SearchAnchorDirectories(applicationPath)) {
        if (const std::optional<std::filesystem::path> found = TryResolveFromWalkUp(anchor);
            found.has_value()) {
            return *found;
        }
    }
    return {};
}

std::filesystem::path PickEngineTexturesDirectory(const std::filesystem::path& workspaceRoot,
                                                  const std::filesystem::path& applicationExecutablePath) {
    if (!workspaceRoot.empty()) {
        const std::filesystem::path assetsTextures = workspaceRoot / "Assets" / "Textures";
        if (IsEngineTextureLibraryDirectory(assetsTextures)) {
            return assetsTextures;
        }
        const std::filesystem::path legacyEngineTextures = workspaceRoot / "Engine" / "Textures";
        if (IsEngineTextureLibraryDirectory(legacyEngineTextures)) {
            return legacyEngineTextures;
        }
    }
    return ResolveEngineTexturesDirectory(applicationExecutablePath);
}

} // namespace ri::content
