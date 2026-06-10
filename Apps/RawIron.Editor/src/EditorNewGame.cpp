#include "EditorNewGame.h"

#include "EditorProjectScaffolding.h"
#include "EditorWorkspace.h"
#include "RawIron/Core/Detail/JsonScan.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <string_view>

namespace ri::editor {

namespace fs = std::filesystem;

namespace {

std::string EscapeJsonString(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size() + 8);
    for (char ch : text) {
        switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped.push_back(ch); break;
        }
    }
    return escaped;
}

std::string CompactPascalCase(std::string_view text) {
    std::string result;
    bool capitalizeNext = true;
    for (char ch : text) {
        const unsigned char uc = static_cast<unsigned char>(ch);
        if (!std::isalnum(uc)) {
            capitalizeNext = true;
            continue;
        }
        if (capitalizeNext) {
            result.push_back(static_cast<char>(std::toupper(uc)));
            capitalizeNext = false;
        } else {
            result.push_back(ch);
        }
    }
    return result.empty() ? std::string("Project") : result;
}

std::string ToLowerAscii(std::string text) {
    for (char& ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

[[nodiscard]] bool DirectoryIsEmptyOrMissing(const fs::path& path) {
    std::error_code ec{};
    if (!fs::exists(path, ec)) {
        return true;
    }
    if (!fs::is_directory(path, ec)) {
        return false;
    }
    return fs::directory_iterator(path, ec) == fs::directory_iterator{};
}

[[nodiscard]] std::string FindUniqueGameId(const fs::path& workspaceRoot, std::string baseSlug) {
    baseSlug = ToLowerAscii(baseSlug);
    const fs::path gamesRoot = workspaceRoot / "Games";
    auto idExists = [&](const std::string& id) {
        std::error_code ec{};
        if (!fs::exists(gamesRoot, ec)) {
            return false;
        }
        for (const fs::directory_entry& entry : fs::directory_iterator(gamesRoot, ec)) {
            if (!entry.is_directory()) {
                continue;
            }
            const fs::path manifestPath = entry.path() / "manifest.json";
            if (!fs::exists(manifestPath, ec)) {
                continue;
            }
            const std::optional<ri::content::GameManifest> manifest = ri::content::LoadGameManifest(manifestPath);
            if (manifest.has_value() && manifest->id == id) {
                return true;
            }
        }
        return false;
    };
    if (!idExists(baseSlug)) {
        return baseSlug;
    }
    for (int suffix = 2; suffix < 100; ++suffix) {
        const std::string candidate = baseSlug + "-" + std::to_string(suffix);
        if (!idExists(candidate)) {
            return candidate;
        }
    }
    return baseSlug + "-new";
}

[[nodiscard]] fs::path FindUniqueProjectRoot(const fs::path& gamesRoot, std::string folderStem) {
    folderStem = CompactPascalCase(folderStem);
    fs::path candidate = gamesRoot / folderStem;
    if (DirectoryIsEmptyOrMissing(candidate) && !fs::exists(candidate / "manifest.json")) {
        return candidate;
    }
    for (int suffix = 2; suffix < 100; ++suffix) {
        candidate = gamesRoot / (folderStem + std::to_string(suffix));
        if (DirectoryIsEmptyOrMissing(candidate) && !fs::exists(candidate / "manifest.json")) {
            return candidate;
        }
    }
    return gamesRoot / (folderStem + "New");
}

} // namespace

std::string NewGameTemplateLabel(const NewGameTemplate templateKind) {
    switch (templateKind) {
        case NewGameTemplate::EmptyStudio: return "Empty Studio";
        case NewGameTemplate::OutdoorScene: return "Outdoor Scene";
        case NewGameTemplate::InteriorRoom: return "Interior Room";
    }
    return "Empty Studio";
}

std::string DefaultDisplayNameForTemplate(const NewGameTemplate templateKind) {
    switch (templateKind) {
        case NewGameTemplate::EmptyStudio: return "Empty Studio";
        case NewGameTemplate::OutdoorScene: return "Sunny Mesa";
        case NewGameTemplate::InteriorRoom: return "Quiet Room";
    }
    return "Empty Studio";
}

std::string SlugFromDisplayName(const std::string_view displayName) {
    std::string slug;
    slug.reserve(displayName.size());
    bool pendingDash = false;
    for (char ch : displayName) {
        const unsigned char uc = static_cast<unsigned char>(ch);
        if (std::isalnum(uc)) {
            if (pendingDash && !slug.empty()) {
                slug.push_back('-');
                pendingDash = false;
            }
            slug.push_back(static_cast<char>(std::tolower(uc)));
        } else if (!slug.empty()) {
            pendingDash = true;
        }
    }
    return slug.empty() ? std::string("new-game") : slug;
}

std::string BuildNewGameManifestJson(const std::string_view projectId,
                                     const std::string_view projectName,
                                     const std::string_view description) {
    const std::string projectIdString(projectId);
    const std::string projectNameString(projectName);
    const std::string descriptionString(description);
    const std::string moduleStem = CompactPascalCase(projectNameString);
    const std::string runtimeModule = "RawIron.Game." + moduleStem;
    const std::string entry = "RawIron." + moduleStem + "Game";
    const std::string editorProjectArg = "--game=" + projectIdString;

    std::ostringstream stream;
    stream << "{\n"
           << "  \"id\": \"" << EscapeJsonString(projectIdString) << "\",\n"
           << "  \"name\": \"" << EscapeJsonString(projectNameString) << "\",\n"
           << "  \"format\": \"rawiron-game-v1.3.7\",\n"
           << "  \"type\": \"game\",\n"
           << "  \"entry\": \"" << EscapeJsonString(entry) << "\",\n"
           << "  \"runtimeContract\": \"rawiron-runtime-v1\",\n"
           << "  \"runtimeModule\": \"" << EscapeJsonString(runtimeModule) << "\",\n"
           << "  \"runtimeHost\": \"RuntimeCore\",\n"
           << "  \"runtimeServices\": [\n"
           << "    \"lifecycle\",\n"
           << "    \"events\",\n"
           << "    \"services\",\n"
           << "    \"paths\",\n"
           << "    \"frame-clock\"\n"
           << "  ],\n"
           << "  \"version\": \"1.0.0\",\n"
           << "  \"author\": \"RawIron Editor\",\n"
           << "  \"editorProjectArg\": \"" << EscapeJsonString(editorProjectArg) << "\",\n"
           << "  \"primaryLevel\": \"levels/assembly.primitives.csv\",\n"
           << "  \"description\": \"" << EscapeJsonString(descriptionString) << "\",\n"
           << "  \"controls\": {\n"
           << "    \"move\": \"WASD\",\n"
           << "    \"look\": \"Mouse\",\n"
           << "    \"jump\": \"Space\",\n"
           << "    \"sprint\": \"Shift\",\n"
           << "    \"quit\": \"Esc\"\n"
           << "  },\n"
           << "  \"editorOpenArgs\": [\n"
           << "    \"" << EscapeJsonString(editorProjectArg) << "\"\n"
           << "  ]\n"
           << "}\n";
    return stream.str();
}

std::string TemplateLevelPrimitivesCsv(const NewGameTemplate templateKind) {
    switch (templateKind) {
        case NewGameTemplate::OutdoorScene:
            return "name,primitive,parent,px,py,pz,rx,ry,rz,sx,sy,sz,material,texture,tx,ty,r,g,b,a\n"
                   "ground,plane,,0,0,0,0,0,0,24,1,24,starter_ground,smooth_stone.png,6,6,0.58,0.62,0.48,1\n"
                   "rock_a,cube,,4,0.8,2,0,15,0,1.6,1.2,1.4,starter_rock,iron_block.png,1,1,0.46,0.44,0.42,1\n"
                   "rock_b,cube,,-3,0.5,-4,0,-20,0,2.2,1.0,1.8,starter_rock,iron_block.png,1,1,0.50,0.48,0.45,1\n"
                   "rock_c,cube,,6,0.4,-3,0,35,0,1.1,0.9,1.0,starter_rock,iron_block.png,1,1,0.42,0.40,0.38,1\n"
                   "spawn_marker,cube,,0,1,0,0,0,0,0.6,0.6,0.6,starter_block,iron_block.png,1,1,0.72,0.74,0.78,1\n";
        case NewGameTemplate::InteriorRoom:
            return "name,primitive,parent,px,py,pz,rx,ry,rz,sx,sy,sz,material,texture,tx,ty,r,g,b,a\n"
                   "floor,plane,,0,0,0,0,0,0,10,1,10,starter_floor,smooth_stone.png,4,4,0.34,0.34,0.36,1\n"
                   "wall_n,cube,,0,2,-5,0,0,0,10,4,0.4,starter_wall,smooth_stone.png,2,2,0.42,0.42,0.46,1\n"
                   "wall_s,cube,,0,2,5,0,0,0,10,4,0.4,starter_wall,smooth_stone.png,2,2,0.42,0.42,0.46,1\n"
                   "wall_e,cube,,5,2,0,0,0,0,0.4,4,10,starter_wall,smooth_stone.png,2,2,0.42,0.42,0.46,1\n"
                   "wall_w,cube,,-5,2,0,0,0,0,0.4,4,10,starter_wall,smooth_stone.png,2,2,0.42,0.42,0.46,1\n"
                   "spawn_block,cube,,0,1,0,0,0,0,0.8,0.8,0.8,starter_block,iron_block.png,1,1,0.62,0.64,0.68,1\n";
        case NewGameTemplate::EmptyStudio:
        default:
            return "name,primitive,parent,px,py,pz,rx,ry,rz,sx,sy,sz,material,texture,tx,ty,r,g,b,a\n"
                   "floor,plane,,0,0,0,0,0,0,16,1,16,starter_floor,smooth_stone.png,4,4,0.72,0.74,0.78,1\n"
                   "spawn_block,cube,,0,1,0,0,0,0,1,1,1,starter_block,iron_block.png,1,1,0.55,0.58,0.62,1\n";
    }
}

NewGameCreationResult CreateNewGameProject(const fs::path& workspaceRoot,
                                           const NewGameTemplate templateKind,
                                           const std::string_view displayName,
                                           const std::string_view author) {
    NewGameCreationResult result{};
    if (displayName.empty()) {
        result.error = "Enter a game name before creating the project.";
        return result;
    }

    const std::string slugBase = SlugFromDisplayName(displayName);
    const std::string projectId = FindUniqueGameId(workspaceRoot, slugBase);
    const fs::path gamesRoot = workspaceRoot / "Games";
    const fs::path projectRoot = FindUniqueProjectRoot(gamesRoot, std::string(displayName));

    std::error_code ec{};
    fs::create_directories(projectRoot, ec);
    if (ec) {
        result.error = "Could not create project folder: " + projectRoot.string();
        return result;
    }

    const std::string description = std::string("Created from the ") + NewGameTemplateLabel(templateKind) +
                                    " template in RawIron Editor.";
    std::string manifestJson = BuildNewGameManifestJson(projectId, displayName, description);
    if (!author.empty()) {
        const std::string authorToken = "\"author\": \"RawIron Editor\"";
        const std::string replacement = "\"author\": \"" + EscapeJsonString(std::string(author)) + "\"";
        const std::size_t pos = manifestJson.find(authorToken);
        if (pos != std::string::npos) {
            manifestJson.replace(pos, authorToken.size(), replacement);
        }
    }

    const fs::path manifestPath = projectRoot / "manifest.json";
    if (!ri::core::detail::WriteTextFile(manifestPath, manifestJson)) {
        result.error = "Could not write manifest.json.";
        return result;
    }

    const std::optional<ri::content::GameManifest> manifest = ri::content::LoadGameManifest(manifestPath);
    if (!manifest.has_value()) {
        result.error = "New manifest could not be loaded.";
        return result;
    }

    std::size_t createdCount = 0;
    std::vector<std::string> createdFiles;
    std::string scaffoldError;
    if (!EnsureMountedGameScaffold(*manifest, createdCount, createdFiles, &scaffoldError)) {
        result.error = scaffoldError.empty() ? "Scaffold failed for new project." : scaffoldError;
        return result;
    }

    const fs::path levelPath = projectRoot / "levels" / "assembly.primitives.csv";
    if (!ri::core::detail::WriteTextFile(levelPath, TemplateLevelPrimitivesCsv(templateKind))) {
        result.error = "Could not write starter level for template.";
        return result;
    }

    EnsureProjectDevConfig(projectRoot);

    const std::optional<ri::content::GameManifest> reloaded = ri::content::LoadGameManifest(manifestPath);
    if (!reloaded.has_value()) {
        result.error = "New project manifest could not be reloaded.";
        return result;
    }

    result.ok = true;
    result.manifest = *reloaded;
    result.projectRoot = projectRoot;
    return result;
}

} // namespace ri::editor
