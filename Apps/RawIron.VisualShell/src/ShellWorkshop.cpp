#include "ShellWorkshop.h"
#include "VisualShellUiColors.h"
#include "WorkshopForgeGpu.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <gdiplus.h>
#pragma comment(lib, "Gdiplus.lib")
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace ri::shell {
namespace {

std::once_flag gGdiplusInit;
std::uintptr_t gGdiplusToken = 0;
ULONG_PTR gGdiplusTokenU = 0;

Gdiplus::Bitmap* AsBitmap(void* p) {
    return static_cast<Gdiplus::Bitmap*>(p);
}

void DeleteBitmap(void* p) {
    if (p != nullptr) {
        delete AsBitmap(p);
    }
}

} // namespace

std::vector<GameProject> EnumerateGameProjects(const std::filesystem::path& workspaceRoot) {
    std::vector<GameProject> out;
    const std::filesystem::path games = workspaceRoot / "Games";
    if (!std::filesystem::exists(games) || !std::filesystem::is_directory(games)) {
        return out;
    }
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(games)) {
        if (!entry.is_directory()) {
            continue;
        }
        const std::filesystem::path manifest = entry.path() / "manifest.json";
        if (!std::filesystem::exists(manifest)) {
            continue;
        }
        const std::string text = ri::core::detail::ReadTextFile(manifest);
        if (text.empty()) {
            continue;
        }
        GameProject project{};
        project.root = entry.path();
        project.id = ri::core::detail::ExtractJsonString(text, "id").value_or(entry.path().filename().string());
        project.name = ri::core::detail::ExtractJsonString(text, "name").value_or(project.id);
        if (!project.id.empty()) {
            out.push_back(std::move(project));
        }
    }
    std::sort(out.begin(), out.end(), [](const GameProject& a, const GameProject& b) {
        return a.name < b.name;
    });
    return out;
}

std::filesystem::path RecentSessionsPath(const std::filesystem::path& workspaceRoot) {
    return workspaceRoot / "Saved" / "Shell" / "recent_sessions.txt";
}

void LoadRecentSessionPaths(const std::filesystem::path& workspaceRoot, std::vector<std::string>& outUtf8Paths) {
    outUtf8Paths.clear();
    const std::filesystem::path path = RecentSessionsPath(workspaceRoot);
    if (!std::filesystem::exists(path)) {
        return;
    }
    const std::string text = ri::core::detail::ReadTextFile(path);
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        if (!line.empty()) {
            outUtf8Paths.push_back(line);
        }
    }
}

void SaveRecentSessionPaths(const std::filesystem::path& workspaceRoot, const std::vector<std::string>& pathsUtf8) {
    const std::filesystem::path path = RecentSessionsPath(workspaceRoot);
    std::filesystem::create_directories(path.parent_path());
    std::ostringstream joined;
    for (std::size_t i = 0; i < pathsUtf8.size(); ++i) {
        if (i > 0) {
            joined << '\n';
        }
        joined << pathsUtf8[i];
    }
    static_cast<void>(ri::core::detail::WriteTextFile(path, joined.str()));
}

void TouchRecentPath(const std::filesystem::path& workspaceRoot, const std::filesystem::path& path) {
    std::vector<std::string> paths;
    LoadRecentSessionPaths(workspaceRoot, paths);
    const std::string key = path.generic_string();
    paths.erase(std::remove(paths.begin(), paths.end(), key), paths.end());
    paths.insert(paths.begin(), key);
    while (paths.size() > 8U) {
        paths.pop_back();
    }
    SaveRecentSessionPaths(workspaceRoot, paths);
}

WorkshopImage::WorkshopImage() = default;

WorkshopImage::~WorkshopImage() {
    DeleteBitmap(bitmap_);
    bitmap_ = nullptr;
}

WorkshopImage::WorkshopImage(WorkshopImage&& other) noexcept
    : bitmap_(other.bitmap_) {
    other.bitmap_ = nullptr;
}

WorkshopImage& WorkshopImage::operator=(WorkshopImage&& other) noexcept {
    if (this != &other) {
        DeleteBitmap(bitmap_);
        bitmap_ = other.bitmap_;
        other.bitmap_ = nullptr;
    }
    return *this;
}

bool WorkshopImage::Load(const std::filesystem::path& path) {
    DeleteBitmap(bitmap_);
    bitmap_ = nullptr;
    if (!std::filesystem::exists(path)) {
        return false;
    }
    std::wstring wpath = path.wstring();
    auto* bmp = Gdiplus::Bitmap::FromFile(wpath.c_str());
    if (bmp == nullptr || bmp->GetLastStatus() != Gdiplus::Ok) {
        delete bmp;
        return false;
    }
    bitmap_ = bmp;
    return true;
}

bool WorkshopImage::Valid() const noexcept {
    return bitmap_ != nullptr;
}

std::uintptr_t StartupGdiplus() {
    std::call_once(gGdiplusInit, []() {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartup(&gGdiplusTokenU, &input, nullptr);
        gGdiplusToken = static_cast<std::uintptr_t>(gGdiplusTokenU);
    });
    return gGdiplusToken;
}

void ShutdownGdiplus(std::uintptr_t token) noexcept {
    (void)token;
    if (gGdiplusTokenU != 0) {
        Gdiplus::GdiplusShutdown(gGdiplusTokenU);
        gGdiplusTokenU = 0;
        gGdiplusToken = 0;
    }
}

namespace {

/// Above this client pixel count, forge compose runs at half resolution; above kHugeWindowPixels, quarter res.
constexpr std::int64_t kLargeWindowPixels = 700'000;
constexpr std::int64_t kHugeWindowPixels = 2'200'000;
/// Large soft pools that warm the forged plate (drawn before the veil).
constexpr int kEmberGlowCount = 20;
/// Small bright sparks on top of the veil.
constexpr int kEmberSparkCount = 12;

/// Lighting on the forged plate is recomputed only when this bucket changes (see gForged… cache).
constexpr float kForgeLightingKeyframesPerSec = 3.0f;

float QuantizedForgeLightingTime(float timeSec) {
    const float hz = kForgeLightingKeyframesPerSec;
    return std::floor(timeSec * hz) / hz;
}

std::unique_ptr<Gdiplus::Bitmap> gForgedComposeCache;
int gForgedCacheClientW = -1;
int gForgedCacheClientH = -1;
int gForgedCacheComposeW = -1;
int gForgedCacheComposeH = -1;
int gForgedCacheScaleDiv = 0;
int gForgedCacheLightFrame = INT_MIN;

void ReadBgraPixel(const BYTE* p, BYTE& r, BYTE& g, BYTE& b, BYTE& a) {
    b = p[0];
    g = p[1];
    r = p[2];
    a = p[3];
}

const BYTE* ScanLine(const Gdiplus::BitmapData& d, int y) {
    return static_cast<const BYTE*>(d.Scan0) + y * d.Stride;
}

void SampleBilinear(const Gdiplus::BitmapData& d,
                    int iw,
                    int ih,
                    float u,
                    float v,
                    BYTE& r,
                    BYTE& g,
                    BYTE& b,
                    BYTE& a) {
    u = (std::clamp)(u, 0.0f, 1.0f);
    v = (std::clamp)(v, 0.0f, 1.0f);
    if (iw <= 0 || ih <= 0) {
        r = g = b = 0;
        a = 255;
        return;
    }
    if (iw == 1 && ih == 1) {
        ReadBgraPixel(ScanLine(d, 0), r, g, b, a);
        return;
    }

    const float fx = u * static_cast<float>((std::max)(0, iw - 1));
    const float fy = v * static_cast<float>((std::max)(0, ih - 1));
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const int x1 = (std::min)(iw - 1, x0 + 1);
    const int y1 = (std::min)(ih - 1, y0 + 1);
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);

    BYTE r00, g00, b00, a00, r10, g10, b10, a10, r01, g01, b01, a01, r11, g11, b11, a11;
    ReadBgraPixel(ScanLine(d, y0) + x0 * 4, r00, g00, b00, a00);
    ReadBgraPixel(ScanLine(d, y0) + x1 * 4, r10, g10, b10, a10);
    ReadBgraPixel(ScanLine(d, y1) + x0 * 4, r01, g01, b01, a01);
    ReadBgraPixel(ScanLine(d, y1) + x1 * 4, r11, g11, b11, a11);

    const float w00 = (1.0f - tx) * (1.0f - ty);
    const float w10 = tx * (1.0f - ty);
    const float w01 = (1.0f - tx) * ty;
    const float w11 = tx * ty;

    auto blend = [&](BYTE c00, BYTE c10, BYTE c01, BYTE c11) -> BYTE {
        const float sum = w00 * static_cast<float>(c00) + w10 * static_cast<float>(c10) +
                          w01 * static_cast<float>(c01) + w11 * static_cast<float>(c11);
        const float c = (std::clamp)(sum, 0.0f, 255.0f);
        return static_cast<BYTE>(std::lround(static_cast<double>(c)));
    };

    r = blend(r00, r10, r01, r11);
    g = blend(g00, g10, g01, g11);
    b = blend(b00, b10, b01, b11);
    a = blend(a00, a10, a01, a11);
}

struct LockedBitmapRead {
    Gdiplus::Bitmap* bmp = nullptr;
    std::unique_ptr<Gdiplus::Bitmap> converted;
    Gdiplus::BitmapData data{};
    bool locked = false;

    explicit LockedBitmapRead(Gdiplus::Bitmap* source) {
        if (source == nullptr) {
            return;
        }
        bmp = source;
        const INT sw = source->GetWidth();
        const INT sh = source->GetHeight();
        Gdiplus::Rect r(0, 0, sw, sh);
        if (source->LockBits(&r, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) == Gdiplus::Ok) {
            locked = true;
            return;
        }
        converted.reset(source->Clone(0, 0, sw, sh, PixelFormat32bppARGB));
        bmp = converted.get();
        if (bmp != nullptr &&
            bmp->LockBits(&r, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) == Gdiplus::Ok) {
            locked = true;
        }
    }

    LockedBitmapRead(LockedBitmapRead&& other) noexcept
        : bmp(other.bmp), converted(std::move(other.converted)), data(other.data), locked(other.locked) {
        other.bmp = nullptr;
        other.locked = false;
    }

    LockedBitmapRead& operator=(LockedBitmapRead&& other) noexcept {
        if (this != &other) {
            if (locked && bmp != nullptr) {
                bmp->UnlockBits(&data);
            }
            bmp = other.bmp;
            converted = std::move(other.converted);
            data = other.data;
            locked = other.locked;
            other.bmp = nullptr;
            other.locked = false;
        }
        return *this;
    }

    LockedBitmapRead(const LockedBitmapRead&) = delete;
    LockedBitmapRead& operator=(const LockedBitmapRead&) = delete;

    ~LockedBitmapRead() {
        if (locked && bmp != nullptr) {
            bmp->UnlockBits(&data);
        }
    }

    [[nodiscard]] bool LocksOk() const noexcept {
        return locked && data.Scan0 != nullptr && data.Width > 0 && data.Height > 0;
    }
};

std::unique_ptr<Gdiplus::Bitmap> ComposeForgedBackground(int w,
                                                           int h,
                                                           const Gdiplus::BitmapData& albedo,
                                                           int aw,
                                                           int ah,
                                                           const Gdiplus::BitmapData* ambient,
                                                           int ambW,
                                                           int ambH,
                                                           const Gdiplus::BitmapData& normal,
                                                           int nw,
                                                           int nh,
                                                           const Gdiplus::BitmapData* displacement,
                                                           int dw,
                                                           int dh,
                                                           const Gdiplus::BitmapData* specular,
                                                           int sw,
                                                           int sh,
                                                           float timeSec) {
    auto out = std::make_unique<Gdiplus::Bitmap>(w, h, PixelFormat32bppARGB);
    Gdiplus::Rect dr(0, 0, w, h);
    Gdiplus::BitmapData dd{};
    if (out->LockBits(&dr, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &dd) != Gdiplus::Ok) {
        return nullptr;
    }

    const bool hasAmb = ambient != nullptr && ambW > 0 && ambH > 0;
    const bool hasDisp = displacement != nullptr && dw > 0 && dh > 0;
    const bool hasSpec = specular != nullptr && sw > 0 && sh > 0;

    // Keep all texture layers locked together. Animated UV offsets made the normal
    // map swim against the albedo/ambient layers, which read as shimmer.
    constexpr float kStaticDisplacementStrength = 0.018f;

    float lx = 0.45f * std::sin(timeSec * 0.31f) + 0.2f;
    float ly = 0.38f * std::cos(timeSec * 0.27f) + 0.15f;
    constexpr float lz = 0.74f;
    const float invLLen =
        1.0f / std::sqrt(lx * lx + ly * ly + lz * lz);
    lx *= invLLen;
    ly *= invLLen;
    const float lzNorm = lz * invLLen;

    constexpr float kx = 0.22f;
    constexpr float ky = -0.62f;
    constexpr float kz = 0.75f;
    const float invKLen =
        1.0f / std::sqrt(kx * kx + ky * ky + kz * kz);

    float hx = lx;
    float hy = ly;
    float hz = lzNorm + 1.0f;
    const float invHLen = 1.0f / std::sqrt(hx * hx + hy * hy + hz * hz);
    hx *= invHLen;
    hy *= invHLen;
    hz *= invHLen;

    for (int y = 0; y < h; ++y) {
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(h);
        BYTE* dstRow = static_cast<BYTE*>(dd.Scan0) + y * dd.Stride;
        for (int x = 0; x < w; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(w);
            float uu = u;
            float vv = v;
            if (hasDisp) {
                BYTE Dr, Dg, Db, Da;
                SampleBilinear(*displacement, dw, dh, u, v, Dr, Dg, Db, Da);
                uu = (std::clamp)(
                    u + (static_cast<float>(Dr) / 255.0f - 0.5f) * kStaticDisplacementStrength, 0.0f, 1.0f);
                vv = (std::clamp)(
                    v + (static_cast<float>(Dg) / 255.0f - 0.5f) * kStaticDisplacementStrength, 0.0f, 1.0f);
            }

            BYTE Nr, Ng, Nb, Na;
            SampleBilinear(normal, nw, nh, uu, vv, Nr, Ng, Nb, Na);

            float nx = static_cast<float>(Nr) / 127.5f - 1.0f;
            float ny = static_cast<float>(Ng) / 127.5f - 1.0f;
            float nzSq = 1.0f - nx * nx - ny * ny;
            float nz = nzSq > 0.0f ? std::sqrt(nzSq) : 0.0f;

            float diff = nx * lx + ny * ly + nz * lzNorm;
            if (diff < 0.0f) {
                diff = 0.0f;
            }

            float diff2 = (nx * kx + ny * ky + nz * kz) * invKLen;
            if (diff2 < 0.0f) {
                diff2 = 0.0f;
            }

            float specDot = nx * hx + ny * hy + nz * hz;
            if (specDot < 0.0f) {
                specDot = 0.0f;
            }

            const float du = u - 0.5f;
            const float dv = v - 0.5f;
            const float edgeDist = std::sqrt(du * du + dv * dv) * 1.41421356f;

            float lit = 0.14f + 0.58f * diff + 0.34f * diff2;
            lit *= 0.88f + 0.12f * (1.0f - edgeDist);

            BYTE Ar, Ag, Ab, Aa;
            SampleBilinear(albedo, aw, ah, uu, vv, Ar, Ag, Ab, Aa);

            BYTE Mr = 255;
            BYTE Mg = 255;
            BYTE Mb = 255;
            if (hasAmb) {
                BYTE Ma = 255;
                SampleBilinear(*ambient, ambW, ambH, uu, vv, Mr, Mg, Mb, Ma);
                (void)Ma;
            }

            float specMask = 0.0f;
            if (hasSpec) {
                BYTE Sr, Sg, Sb, Sa;
                SampleBilinear(*specular, sw, sh, uu, vv, Sr, Sg, Sb, Sa);
                (void)Sa;
                specMask =
                    (0.2126f * static_cast<float>(Sr) + 0.7152f * static_cast<float>(Sg) +
                     0.0722f * static_cast<float>(Sb)) /
                    255.0f;
            }
            const float spec = specMask * std::pow(specDot, 28.0f) * (0.35f + 0.65f * diff);

            float fr =
                (static_cast<float>(Ar) / 255.0f) * (static_cast<float>(Mr) / 255.0f) * lit * 255.0f;
            float fg =
                (static_cast<float>(Ag) / 255.0f) * (static_cast<float>(Mg) / 255.0f) * lit * 255.0f;
            float fb =
                (static_cast<float>(Ab) / 255.0f) * (static_cast<float>(Mb) / 255.0f) * lit * 255.0f;

            fr += 18.0f * diff2;
            fg += 8.0f * diff2;
            fb += 2.0f * diff2;

            fr += 110.0f * spec;
            fg += 78.0f * spec;
            fb += 42.0f * spec;

            BYTE Or = static_cast<BYTE>((std::clamp)(fr, 0.0f, 255.0f));
            BYTE Og = static_cast<BYTE>((std::clamp)(fg, 0.0f, 255.0f));
            BYTE Ob = static_cast<BYTE>((std::clamp)(fb, 0.0f, 255.0f));
            (void)Aa;
            BYTE Oa = 255;

            BYTE* px = dstRow + x * 4;
            px[0] = Ob;
            px[1] = Og;
            px[2] = Or;
            px[3] = Oa;
        }
    }

    out->UnlockBits(&dd);
    return out;
}

void DrawForgedEmberGlowWash(Gdiplus::Graphics& graphics, int w, int h, float timeSec) {
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::SolidBrush warm(Gdiplus::Color(22, 255, 135, 62));
    const std::int64_t screenPx = static_cast<std::int64_t>(w) * static_cast<std::int64_t>(h);
    const int glowCount =
        screenPx > 2'500'000 ? 8 : (screenPx > 1'200'000 ? 12 : (screenPx > 600'000 ? 16 : kEmberGlowCount));
    for (int i = 0; i < glowCount; ++i) {
        const float seed = static_cast<float>(i) * 2.483f + 13.7f;
        const float gx =
            std::fmod(seed * 53.17f + std::sin(timeSec * 0.088f + seed * 0.052f) * 110.0f,
                      static_cast<float>(w + 260)) -
            130.0f;
        const float gy =
            std::fmod(timeSec * (21.0f + std::fmod(seed, 10.0f) * 2.6f) + seed * 31.9f,
                      static_cast<float>(h + 280)) -
            140.0f;
        constexpr float kGlowScale = 0.38f;
        const float radius =
            kGlowScale * (38.0f + std::fmod(seed * 4.11f, 72.0f));
        const float pulse = 0.52f + 0.48f * std::sin(timeSec * 4.2f + seed * 1.55f);
        const int alpha = static_cast<int>(12.0f + 34.0f * pulse);
        warm.SetColor(Gdiplus::Color(static_cast<BYTE>((std::clamp)(alpha, 3, 85)), 255, 148, 78));
        graphics.FillEllipse(&warm, gx - radius * 0.5f, gy - radius * 0.5f, radius, radius);
    }
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighSpeed);
}

void DrawForgedEmberSparks(Gdiplus::Graphics& graphics, int w, int h, float timeSec) {
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighSpeed);
    Gdiplus::SolidBrush outerBrush(Gdiplus::Color(255, 255, 120, 55));
    Gdiplus::SolidBrush coreBrush(Gdiplus::Color(255, 255, 220, 190));
    const std::int64_t screenPx = static_cast<std::int64_t>(w) * static_cast<std::int64_t>(h);
    const int sparkCount =
        screenPx > 2'500'000 ? 6 : (screenPx > 1'200'000 ? 8 : (screenPx > 600'000 ? 10 : kEmberSparkCount));
    for (int i = 0; i < sparkCount; ++i) {
        const float seed = static_cast<float>(i) * 2.7182818f + 40.0f;
        float sx = std::fmod(seed * 61.913f + std::sin(timeSec * 0.11f + seed * 0.07f) * 95.0f,
                             static_cast<float>(w + 120)) -
                   60.0f;
        float sy = std::fmod(timeSec * (32.0f + std::fmod(seed, 11.0f) * 3.8f) + seed * 37.17f,
                             static_cast<float>(h + 160)) -
                   80.0f;
        constexpr float kSparkScale = 0.52f;
        const float sz = kSparkScale * (0.85f + std::fmod(seed * 1.15f, 2.4f));
        const float flicker = 0.42f + 0.58f * std::sin(timeSec * 8.2f + seed * 3.1f);
        const int alpha = static_cast<int>(38.0f + 95.0f * flicker);
        const int alphaCore = (std::min)(255, alpha + 72);

        outerBrush.SetColor(Gdiplus::Color(static_cast<BYTE>((std::clamp)(alpha, 0, 255)), 255, 118,
                                             static_cast<BYTE>(58.0f + 42.0f * flicker)));
        graphics.FillEllipse(&outerBrush, sx, sy, sz * 2.15f, sz * 2.15f);

        coreBrush.SetColor(
            Gdiplus::Color(static_cast<BYTE>((std::clamp)(alphaCore, 0, 255)), 255, 228, 195));
        graphics.FillEllipse(&coreBrush, sx + sz * 0.65f, sy + sz * 0.65f, sz * 1.05f, sz * 1.05f);
    }
}

} // namespace

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
    float animationTimeSeconds) {
    const int cw = static_cast<int>(client.right - client.left);
    const int ch = static_cast<int>(client.bottom - client.top);
    const int w = (std::max)(1, cw);
    const int h = (std::max)(1, ch);

    RECT opaqueFill{0, 0, w, h};
    HBRUSH opaqueBrush = CreateSolidBrush(ri::vshell::colors::kBackground);
    FillRect(dc, &opaqueFill, opaqueBrush);
    DeleteObject(opaqueBrush);

    Gdiplus::Graphics graphics(dc);
    graphics.ResetClip();
    graphics.SetClip(Gdiplus::Rect(0, 0, w, h), Gdiplus::CombineModeReplace);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighSpeed);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighSpeed);
    graphics.Clear(Gdiplus::Color(static_cast<Gdiplus::ARGB>(
        static_cast<unsigned>(0xFF000000u) |
        static_cast<unsigned>(ri::vshell::colors::kBackground & 0xFFFFFFu))));

    const bool hasAlbedo = backgroundAlbedo != nullptr && backgroundAlbedo->bitmap_ != nullptr;
    const bool hasNormal = backgroundNormal != nullptr && backgroundNormal->bitmap_ != nullptr;

    if (hasAlbedo && hasNormal) {
        Gdiplus::Bitmap* albedoB = AsBitmap(backgroundAlbedo->bitmap_);
        Gdiplus::Bitmap* normalB = AsBitmap(backgroundNormal->bitmap_);
        auto lockA = std::make_unique<LockedBitmapRead>(albedoB);
        auto lockN = std::make_unique<LockedBitmapRead>(normalB);
        if (lockA->LocksOk() && lockN->LocksOk()) {
            std::optional<LockedBitmapRead> lockAmb;
            if (backgroundAmbient != nullptr && backgroundAmbient->bitmap_ != nullptr) {
                lockAmb.emplace(AsBitmap(backgroundAmbient->bitmap_));
            }

            std::optional<LockedBitmapRead> lockDisp;
            if (backgroundDisplacement != nullptr && backgroundDisplacement->bitmap_ != nullptr) {
                lockDisp.emplace(AsBitmap(backgroundDisplacement->bitmap_));
            }

            std::optional<LockedBitmapRead> lockSpec;
            if (backgroundSpecular != nullptr && backgroundSpecular->bitmap_ != nullptr) {
                lockSpec.emplace(AsBitmap(backgroundSpecular->bitmap_));
            }

            const Gdiplus::BitmapData* ambPtr = nullptr;
            int ambW = 0;
            int ambH = 0;
            if (lockAmb.has_value() && lockAmb->LocksOk()) {
                ambPtr = &lockAmb->data;
                ambW = static_cast<int>(lockAmb->data.Width);
                ambH = static_cast<int>(lockAmb->data.Height);
            }

            const Gdiplus::BitmapData* dispPtr = nullptr;
            int dW = 0;
            int dH = 0;
            if (lockDisp.has_value() && lockDisp->LocksOk()) {
                dispPtr = &lockDisp->data;
                dW = static_cast<int>(lockDisp->data.Width);
                dH = static_cast<int>(lockDisp->data.Height);
            }

            const Gdiplus::BitmapData* specPtr = nullptr;
            int sW = 0;
            int sH = 0;
            if (lockSpec.has_value() && lockSpec->LocksOk()) {
                specPtr = &lockSpec->data;
                sW = static_cast<int>(lockSpec->data.Width);
                sH = static_cast<int>(lockSpec->data.Height);
            }

            const std::int64_t pixelCount = static_cast<std::int64_t>(w) * static_cast<std::int64_t>(h);
            int composeScaleDiv = 1;
            if (pixelCount > kHugeWindowPixels) {
                composeScaleDiv = 4;
            } else if (pixelCount > kLargeWindowPixels) {
                composeScaleDiv = 2;
            }
            const int composeW = (std::max)(1, w / composeScaleDiv);
            const int composeH = (std::max)(1, h / composeScaleDiv);

            const int lightFrame = static_cast<int>(std::floor(animationTimeSeconds * kForgeLightingKeyframesPerSec));
            const float lightingTime = QuantizedForgeLightingTime(animationTimeSeconds);

            const bool useForgedCache = gForgedComposeCache &&
                gForgedCacheClientW == w && gForgedCacheClientH == h &&
                gForgedCacheComposeW == composeW && gForgedCacheComposeH == composeH &&
                gForgedCacheScaleDiv == composeScaleDiv && gForgedCacheLightFrame == lightFrame;

            if (useForgedCache) {
                const Gdiplus::InterpolationMode prevInterp = graphics.GetInterpolationMode();
                graphics.SetInterpolationMode(composeScaleDiv > 1 ? Gdiplus::InterpolationModeBilinear
                                                                  : Gdiplus::InterpolationModeNearestNeighbor);
                graphics.DrawImage(gForgedComposeCache.get(), 0, 0, w, h);
                graphics.SetInterpolationMode(prevInterp);
            } else {
                std::unique_ptr<Gdiplus::Bitmap> composed;
                if (WorkshopForgeGpuAvailable()) {
                    composed = WorkshopForgeGpuCompose(composeW,
                                                       composeH,
                                                       lockA->data,
                                                       static_cast<int>(lockA->data.Width),
                                                       static_cast<int>(lockA->data.Height),
                                                       ambPtr,
                                                       ambW,
                                                       ambH,
                                                       lockN->data,
                                                       static_cast<int>(lockN->data.Width),
                                                       static_cast<int>(lockN->data.Height),
                                                       dispPtr,
                                                       dW,
                                                       dH,
                                                       specPtr,
                                                       sW,
                                                       sH,
                                                       lightingTime);
                }
                if (!composed) {
                    composed = ComposeForgedBackground(composeW,
                                                       composeH,
                                                       lockA->data,
                                                       static_cast<int>(lockA->data.Width),
                                                       static_cast<int>(lockA->data.Height),
                                                       ambPtr,
                                                       ambW,
                                                       ambH,
                                                       lockN->data,
                                                       static_cast<int>(lockN->data.Width),
                                                       static_cast<int>(lockN->data.Height),
                                                       dispPtr,
                                                       dW,
                                                       dH,
                                                       specPtr,
                                                       sW,
                                                       sH,
                                                       lightingTime);
                }
                if (composed) {
                    const Gdiplus::InterpolationMode prevInterp = graphics.GetInterpolationMode();
                    graphics.SetInterpolationMode(composeScaleDiv > 1 ? Gdiplus::InterpolationModeBilinear
                                                                      : Gdiplus::InterpolationModeNearestNeighbor);
                    graphics.DrawImage(composed.get(), 0, 0, w, h);
                    graphics.SetInterpolationMode(prevInterp);

                    gForgedComposeCache = std::move(composed);
                    gForgedCacheClientW = w;
                    gForgedCacheClientH = h;
                    gForgedCacheComposeW = composeW;
                    gForgedCacheComposeH = composeH;
                    gForgedCacheScaleDiv = composeScaleDiv;
                    gForgedCacheLightFrame = lightFrame;
                } else {
                    gForgedComposeCache.reset();
                    Gdiplus::Rect dest(0, 0, w, h);
                    graphics.DrawImage(albedoB, dest);
                }
            }
        } else {
            Gdiplus::Rect dest(0, 0, w, h);
            graphics.DrawImage(albedoB, dest);
        }

        DrawForgedEmberGlowWash(graphics, w, h, animationTimeSeconds);
        Gdiplus::SolidBrush veil(Gdiplus::Color(52, 14, 12, 16));
        graphics.FillRectangle(&veil, 0, 0, w, h);
        DrawForgedEmberSparks(graphics, w, h, animationTimeSeconds);
    } else if (hasAlbedo) {
        Gdiplus::Bitmap* alb = AsBitmap(backgroundAlbedo->bitmap_);
        Gdiplus::Rect dest(0, 0, w, h);
        graphics.DrawImage(alb, dest);

        if (backgroundAmbient != nullptr && backgroundAmbient->bitmap_ != nullptr) {
            Gdiplus::ImageAttributes ia;
            Gdiplus::ColorMatrix mx = {};
            mx.m[0][0] = 0.35f;
            mx.m[1][1] = 0.35f;
            mx.m[2][2] = 0.35f;
            mx.m[3][3] = 0.55f;
            ia.SetColorMatrix(&mx);
            graphics.DrawImage(AsBitmap(backgroundAmbient->bitmap_),
                               dest,
                               0,
                               0,
                               AsBitmap(backgroundAmbient->bitmap_)->GetWidth(),
                               AsBitmap(backgroundAmbient->bitmap_)->GetHeight(),
                               Gdiplus::UnitPixel,
                               &ia);
        }

        DrawForgedEmberGlowWash(graphics, w, h, animationTimeSeconds);
        Gdiplus::SolidBrush veil(Gdiplus::Color(85, 18, 16, 20));
        graphics.FillRectangle(&veil, 0, 0, w, h);
        DrawForgedEmberSparks(graphics, w, h, animationTimeSeconds);
    } else {
        Gdiplus::LinearGradientBrush vertical(
            Gdiplus::Point(0, 0),
            Gdiplus::Point(0, h),
            Gdiplus::Color(255, 32, 30, 34),
            Gdiplus::Color(255, 12, 11, 14));
        graphics.FillRectangle(&vertical, 0, 0, w, h);

        Gdiplus::LinearGradientBrush ember(
            Gdiplus::Point(w / 2, h),
            Gdiplus::Point(w / 2, h / 3),
            Gdiplus::Color(180, 90, 40, 12),
            Gdiplus::Color(0, 90, 40, 12));
        graphics.FillRectangle(&ember, 0, 0, w, h);

        Gdiplus::Pen gridPen(Gdiplus::Color(255, 70, 72, 78), 1.0f);
        const int gridStep = (std::max)(10, w / 160);
        for (int i = 0; i < w; i += gridStep) {
            const float t = static_cast<float>(i) / static_cast<float>(w);
            const int a = static_cast<int>(35 + 25 * std::sin(t * 6.28f));
            const BYTE alpha = static_cast<BYTE>((std::clamp)(a, 0, 255));
            gridPen.SetColor(Gdiplus::Color(alpha, 70, 72, 78));
            graphics.DrawLine(&gridPen, i, 0, i + 40, h);
        }

        DrawForgedEmberGlowWash(graphics, w, h, animationTimeSeconds);
        Gdiplus::SolidBrush veil(Gdiplus::Color(110, 14, 12, 18));
        graphics.FillRectangle(&veil, 0, 0, w, h);
        DrawForgedEmberSparks(graphics, w, h, animationTimeSeconds);
    }

    if (logo != nullptr && logo->bitmap_ != nullptr) {
        const int logoMax = 38;
        const int lw = AsBitmap(logo->bitmap_)->GetWidth();
        const int lh = AsBitmap(logo->bitmap_)->GetHeight();
        const int drawW = (lw > lh) ? logoMax : (logoMax * lw) / (std::max)(1, lh);
        const int drawH = (lh >= lw) ? logoMax : (logoMax * lh) / (std::max)(1, lw);
        const int lx = headerRect.left + 12;
        const int ly = headerRect.top + 10;
        graphics.DrawImage(AsBitmap(logo->bitmap_), lx, ly, drawW, drawH);
    }

    if (workspaceTitleWide != nullptr && workspaceTitleWide[0] != L'\0') {
        Gdiplus::FontFamily family(L"Segoe UI");
        Gdiplus::Font font(&family, 14.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush text(Gdiplus::Color(255, 235, 228, 214));
        const int tx = headerRect.left + (logo != nullptr && logo->bitmap_ != nullptr ? 62 : 14);
        const int ty = headerRect.top + 16;
        const INT len = static_cast<INT>(wcslen(workspaceTitleWide));
        graphics.DrawString(workspaceTitleWide, len, &font,
            Gdiplus::PointF(static_cast<Gdiplus::REAL>(tx), static_cast<Gdiplus::REAL>(ty)), &text);
    }
}

} // namespace ri::shell
