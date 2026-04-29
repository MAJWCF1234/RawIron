#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <windows.h>

namespace ri::shell {

struct GameProject {
    std::filesystem::path root;
    std::string id;
    std::string name;
};

[[nodiscard]] std::vector<GameProject> EnumerateGameProjects(const std::filesystem::path& workspaceRoot);

[[nodiscard]] std::filesystem::path RecentSessionsPath(const std::filesystem::path& workspaceRoot);

void LoadRecentSessionPaths(const std::filesystem::path& workspaceRoot, std::vector<std::string>& outUtf8Paths);
void SaveRecentSessionPaths(const std::filesystem::path& workspaceRoot, const std::vector<std::string>& pathsUtf8);
void TouchRecentPath(const std::filesystem::path& workspaceRoot, const std::filesystem::path& path);

/// Opaque GDI+ bitmap (owned).
class WorkshopImage {
public:
    WorkshopImage();
    ~WorkshopImage();
    WorkshopImage(const WorkshopImage&) = delete;
    WorkshopImage& operator=(const WorkshopImage&) = delete;
    WorkshopImage(WorkshopImage&& other) noexcept;
    WorkshopImage& operator=(WorkshopImage&& other) noexcept;

    [[nodiscard]] bool Load(const std::filesystem::path& path);
    [[nodiscard]] bool Valid() const noexcept;

private:
    friend void DrawWorkshopChrome(HDC dc,
        const RECT& client,
        const WorkshopImage* backgroundAlbedo,
        const WorkshopImage* backgroundAmbient,
        const WorkshopImage* backgroundNormal,
        const WorkshopImage* backgroundDisplacement,
        const WorkshopImage* backgroundSpecular,
        const WorkshopImage* logo,
        const RECT& headerRect,
        const wchar_t* workspaceTitleWide,
        float animationTimeSeconds);
    void* bitmap_ = nullptr;
};

[[nodiscard]] std::uintptr_t StartupGdiplus();
void ShutdownGdiplus(std::uintptr_t token) noexcept;

void DrawWorkshopChrome(HDC dc,
    const RECT& client,
    const WorkshopImage* backgroundAlbedo,
    const WorkshopImage* backgroundAmbient,
    const WorkshopImage* backgroundNormal,
    const WorkshopImage* backgroundDisplacement,
    const WorkshopImage* backgroundSpecular,
    const WorkshopImage* logo,
    const RECT& headerRect,
    const wchar_t* workspaceTitleWide,
    float animationTimeSeconds);

} // namespace ri::shell
