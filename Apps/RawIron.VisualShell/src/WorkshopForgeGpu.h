#pragma once

#include <memory>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <gdiplus.h>

namespace ri::shell {

[[nodiscard]] bool WorkshopForgeGpuTryInit() noexcept;
void WorkshopForgeGpuShutdown() noexcept;

/// Renders the forged workshop plate on the GPU (matches CPU `ComposeForgedBackground`).
/// Returns nullptr if GPU path is unavailable or fails — caller uses CPU fallback.
[[nodiscard]] std::unique_ptr<Gdiplus::Bitmap> WorkshopForgeGpuCompose(
    int outW,
    int outH,
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
    float lightingTime);

[[nodiscard]] bool WorkshopForgeGpuAvailable() noexcept;

} // namespace ri::shell
