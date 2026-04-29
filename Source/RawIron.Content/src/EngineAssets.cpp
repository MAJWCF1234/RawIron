#include "RawIron/Content/EngineAssets.h"

#include "RawIron/Content/GameManifest.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
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

} // namespace

namespace ri::content {

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
