#include "RawIron/Content/ShaderAsset.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>

namespace ri::content {
namespace {

namespace detail = ri::core::detail;
namespace fs = std::filesystem;

[[nodiscard]] std::string Trim(std::string value) {
    const auto isSpace = [](const unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), isSpace));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), isSpace).base(), value.end());
    return value;
}

[[nodiscard]] std::string NormalizeToken(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const char ch : value) {
        const unsigned char code = static_cast<unsigned char>(ch);
        if (std::isalnum(code)) {
            normalized.push_back(static_cast<char>(std::tolower(code)));
        } else if (ch == '-' || ch == '_' || std::isspace(code)) {
            if (!normalized.empty() && normalized.back() != '_') {
                normalized.push_back('_');
            }
        }
    }
    while (!normalized.empty() && normalized.back() == '_') {
        normalized.pop_back();
    }
    return normalized;
}

[[nodiscard]] bool IsSafeIdentifier(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](const char ch) {
        const unsigned char code = static_cast<unsigned char>(ch);
        return std::isalnum(code) != 0 || ch == '.' || ch == '_' || ch == '-';
    });
}

[[nodiscard]] bool IsSafeRelativePath(const fs::path& path) {
    if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory()) {
        return false;
    }
    for (const fs::path& part : path) {
        const std::string token = part.generic_string();
        if (token.empty() || token == "..") {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string LowerExtension(const fs::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return extension;
}

void AppendError(std::vector<std::string>* errors, std::string message) {
    if (errors != nullptr) {
        errors->push_back(std::move(message));
    }
}

[[nodiscard]] bool IsWithin(const fs::path& candidate, const fs::path& root) {
    auto rootPart = root.begin();
    auto candidatePart = candidate.begin();
    for (; rootPart != root.end(); ++rootPart, ++candidatePart) {
        if (candidatePart == candidate.end()) {
            return false;
        }
#if defined(_WIN32)
        std::string lhs = rootPart->string();
        std::string rhs = candidatePart->string();
        std::transform(lhs.begin(), lhs.end(), lhs.begin(), [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        std::transform(rhs.begin(), rhs.end(), rhs.begin(), [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (lhs != rhs) {
            return false;
        }
#else
        if (*rootPart != *candidatePart) {
            return false;
        }
#endif
    }
    return true;
}

} // namespace

std::string_view ToString(const RawIronShaderDomain domain) noexcept {
    switch (domain) {
    case RawIronShaderDomain::Surface: return "surface";
    case RawIronShaderDomain::PostProcess: return "post_process";
    case RawIronShaderDomain::Ui: return "ui";
    case RawIronShaderDomain::Vfx: return "vfx";
    case RawIronShaderDomain::Compute: return "compute";
    }
    return "surface";
}

std::optional<RawIronShaderDomain> TryParseRawIronShaderDomain(const std::string_view value) noexcept {
    const std::string normalized = NormalizeToken(value);
    if (normalized == "surface" || normalized == "material") return RawIronShaderDomain::Surface;
    if (normalized == "post" || normalized == "post_process" || normalized == "presentation") {
        return RawIronShaderDomain::PostProcess;
    }
    if (normalized == "ui" || normalized == "overlay") return RawIronShaderDomain::Ui;
    if (normalized == "vfx" || normalized == "effect") return RawIronShaderDomain::Vfx;
    if (normalized == "compute") return RawIronShaderDomain::Compute;
    return std::nullopt;
}

bool LoadRawIronShaderAsset(const fs::path& path, RawIronShaderAsset* out, std::string* error) {
    if (out == nullptr) {
        if (error != nullptr) *error = "LoadRawIronShaderAsset: output pointer was null.";
        return false;
    }
    if (LowerExtension(path) != ".rishader") {
        if (error != nullptr) *error = "Native shader asset must use the .rishader extension: " + path.generic_string();
        return false;
    }
    std::error_code ec{};
    const fs::path canonicalPath = fs::weakly_canonical(path, ec);
    if (ec || !fs::is_regular_file(canonicalPath, ec)) {
        if (error != nullptr) *error = "Native shader asset not found: " + path.generic_string();
        return false;
    }
    const std::string text = detail::ReadTextFile(canonicalPath);
    if (text.empty() || text.find('{') == std::string::npos) {
        if (error != nullptr) *error = "Native shader asset is empty or not JSON: " + canonicalPath.generic_string();
        return false;
    }

    RawIronShaderAsset parsed{};
    parsed.formatVersion = detail::ExtractJsonInt(text, "rawironShaderVersion").value_or(0);
    parsed.id = detail::ExtractJsonString(text, "id").value_or("");
    parsed.name = detail::ExtractJsonString(text, "name").value_or(parsed.id);
    const std::string domainText = detail::ExtractJsonString(text, "domain").value_or("");
    const std::optional<RawIronShaderDomain> domain = TryParseRawIronShaderDomain(domainText);
    parsed.stage = detail::ExtractJsonString(text, "stage").value_or("");
    parsed.entryPoint = detail::ExtractJsonString(text, "entryPoint").value_or("main");
    parsed.assetPath = canonicalPath;

    if (parsed.formatVersion != RawIronShaderAsset::kFormatVersion) {
        if (error != nullptr) *error = "Unsupported rawironShaderVersion in " + canonicalPath.generic_string();
        return false;
    }
    if (!IsSafeIdentifier(parsed.id)) {
        if (error != nullptr) *error = "Native shader id is empty or invalid in " + canonicalPath.generic_string();
        return false;
    }
    if (!domain.has_value()) {
        if (error != nullptr) *error = "Native shader domain is missing or invalid in " + canonicalPath.generic_string();
        return false;
    }
    parsed.domain = *domain;
    if (parsed.stage.empty()) {
        parsed.stage = std::string(ToString(parsed.domain));
    }
    if (!IsSafeIdentifier(parsed.entryPoint)) {
        if (error != nullptr) *error = "Native shader entryPoint is invalid in " + canonicalPath.generic_string();
        return false;
    }

    std::set<std::string, std::less<>> textureNames;
    for (const std::string_view textureText : detail::SplitJsonArrayObjects(text, "textures")) {
        RawIronShaderTexture texture{};
        texture.name = detail::ExtractJsonString(textureText, "name").value_or("");
        texture.relativePath = detail::ExtractJsonString(textureText, "path").value_or("");
        texture.required = detail::ExtractJsonBool(textureText, "required").value_or(true);
        texture.srgb = detail::ExtractJsonBool(textureText, "srgb").value_or(true);
        if (const std::optional<std::string> colorSpace = detail::ExtractJsonString(textureText, "colorSpace")) {
            const std::string normalized = NormalizeToken(*colorSpace);
            if (normalized == "srgb" || normalized == "color") {
                texture.srgb = true;
            } else if (normalized == "linear" || normalized == "data") {
                texture.srgb = false;
            } else {
                if (error != nullptr) *error = "Native shader texture colorSpace is invalid in " + canonicalPath.generic_string();
                return false;
            }
        }
        constexpr std::string_view kNativeTexturePrefix = "native://";
        const bool usesNativeTextureBundle = texture.relativePath.starts_with(kNativeTexturePrefix);
        const fs::path relative(usesNativeTextureBundle
            ? texture.relativePath.substr(kNativeTexturePrefix.size())
            : texture.relativePath);
        if (!IsSafeIdentifier(texture.name) || !textureNames.insert(texture.name).second) {
            if (error != nullptr) *error = "Native shader texture name is empty, invalid, or duplicated in " + canonicalPath.generic_string();
            return false;
        }
        if (!IsSafeRelativePath(relative)) {
            if (error != nullptr) *error = "Native shader texture path is unsafe in " + canonicalPath.generic_string();
            return false;
        }
        fs::path textureRoot = canonicalPath.parent_path();
        if (usesNativeTextureBundle) {
            fs::path search = canonicalPath.parent_path();
            textureRoot.clear();
            for (int depth = 0; depth < 8 && !search.empty(); ++depth) {
                const fs::path candidate = search / "NativeTextures";
                ec.clear();
                if (fs::is_directory(candidate, ec) && !ec) {
                    textureRoot = fs::weakly_canonical(candidate, ec);
                    break;
                }
                const fs::path parent = search.parent_path();
                if (parent == search) break;
                search = parent;
            }
            if (textureRoot.empty() || ec) {
                if (error != nullptr) *error = "Native shader texture bundle could not be resolved for " + canonicalPath.generic_string();
                return false;
            }
        }
        texture.resolvedPath = fs::weakly_canonical(textureRoot / relative, ec);
        if (ec || !IsWithin(texture.resolvedPath, textureRoot)) {
            if (error != nullptr) *error = "Native shader texture escapes its allowed texture root in " + canonicalPath.generic_string();
            return false;
        }
        if (texture.required && !fs::is_regular_file(texture.resolvedPath, ec)) {
            if (error != nullptr) *error = "Native shader required texture is missing: " + texture.resolvedPath.generic_string();
            return false;
        }
        parsed.textures.push_back(std::move(texture));
    }

    if (parsed.domain == RawIronShaderDomain::PostProcess) {
        std::string presentationError;
        if (!ri::render::LoadShaderCfg(canonicalPath, &parsed.presentation, &presentationError)) {
            if (error != nullptr) *error = presentationError;
            return false;
        }
    }

    *out = std::move(parsed);
    return true;
}

bool LoadRawIronShaderManifest(const fs::path& gameRoot,
                               std::vector<RawIronShaderManifestEntry>* out,
                               std::vector<std::string>* errors) {
    if (out == nullptr) {
        AppendError(errors, "LoadRawIronShaderManifest: output pointer was null.");
        return false;
    }
    std::error_code ec{};
    const fs::path canonicalRoot = fs::weakly_canonical(gameRoot, ec);
    const fs::path manifestPath = canonicalRoot / "assets" / "shaders.manifest";
    if (ec || !fs::is_regular_file(manifestPath, ec)) {
        AppendError(errors, "Shader manifest not found: " + manifestPath.generic_string());
        return false;
    }

    std::ifstream input(manifestPath);
    if (!input.is_open()) {
        AppendError(errors, "Shader manifest could not be opened: " + manifestPath.generic_string());
        return false;
    }

    std::vector<RawIronShaderManifestEntry> parsedEntries;
    std::set<std::string, std::less<>> keys;
    std::string line;
    std::size_t lineNumber = 0;
    bool valid = true;
    while (std::getline(input, line)) {
        ++lineNumber;
        line = Trim(std::move(line));
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            AppendError(errors, "shaders.manifest line " + std::to_string(lineNumber) + " is missing '='.");
            valid = false;
            continue;
        }
        RawIronShaderManifestEntry entry{};
        entry.key = Trim(line.substr(0, equals));
        entry.relativePath = Trim(line.substr(equals + 1));
        const fs::path relative(entry.relativePath);
        if (!IsSafeIdentifier(entry.key) || !keys.insert(entry.key).second) {
            AppendError(errors, "shaders.manifest line " + std::to_string(lineNumber) + " has an invalid or duplicate key.");
            valid = false;
            continue;
        }
        if (!IsSafeRelativePath(relative) || LowerExtension(relative) != ".rishader") {
            AppendError(errors, "shaders.manifest line " + std::to_string(lineNumber) + " must reference a safe .rishader path.");
            valid = false;
            continue;
        }
        const fs::path absolute = fs::weakly_canonical(canonicalRoot / relative, ec);
        if (ec || !IsWithin(absolute, canonicalRoot)) {
            AppendError(errors, "shaders.manifest line " + std::to_string(lineNumber) + " escapes the game root.");
            valid = false;
            continue;
        }
        std::string assetError;
        if (!LoadRawIronShaderAsset(absolute, &entry.asset, &assetError)) {
            AppendError(errors, "shaders.manifest line " + std::to_string(lineNumber) + ": " + assetError);
            valid = false;
            continue;
        }
        if (entry.asset.id != entry.key) {
            AppendError(errors, "shaders.manifest line " + std::to_string(lineNumber)
                + " key does not match shader id '" + entry.asset.id + "'.");
            valid = false;
            continue;
        }
        parsedEntries.push_back(std::move(entry));
    }
    if (parsedEntries.empty()) {
        AppendError(errors, "Shader manifest contains no shader entries: " + manifestPath.generic_string());
        valid = false;
    }
    if (!valid) {
        return false;
    }
    *out = std::move(parsedEntries);
    return true;
}

std::vector<std::string> ValidateRawIronShaderManifest(const fs::path& gameRoot) {
    const fs::path manifestPath = gameRoot / "assets" / "shaders.manifest";
    std::error_code ec{};
    if (!fs::exists(manifestPath, ec)) {
        return {};
    }
    std::vector<RawIronShaderManifestEntry> entries;
    std::vector<std::string> errors;
    (void)LoadRawIronShaderManifest(gameRoot, &entries, &errors);
    return errors;
}

std::optional<ri::render::ShaderPresentationConfig> ComposeRawIronShaderPresentation(
    const std::vector<RawIronShaderManifestEntry>& entries) {
    ri::render::PostProcessParameters parameters{};
    bool found = false;
    for (const RawIronShaderManifestEntry& entry : entries) {
        if (entry.asset.domain != RawIronShaderDomain::PostProcess || !entry.asset.presentation.loaded) {
            continue;
        }
        ri::render::ApplyShaderConfig(parameters, entry.asset.presentation);
        found = true;
    }
    if (!found) {
        return std::nullopt;
    }
    ri::render::ShaderPresentationConfig composed{};
    composed.parameters = parameters;
    composed.loaded = true;
    composed.replace = true;
    composed.blendWeight = 1.0f;
    return composed;
}

bool TryLoadRawIronShaderPresentation(const fs::path& gameRoot,
                                      ri::render::ShaderPresentationConfig* out,
                                      std::vector<std::string>* errors) {
    if (out == nullptr) {
        AppendError(errors, "TryLoadRawIronShaderPresentation: output pointer was null.");
        return false;
    }
    std::vector<RawIronShaderManifestEntry> entries;
    if (!LoadRawIronShaderManifest(gameRoot, &entries, errors)) {
        return false;
    }
    const auto presentation = ComposeRawIronShaderPresentation(entries);
    if (!presentation.has_value()) {
        return false;
    }
    *out = *presentation;
    return true;
}

} // namespace ri::content
