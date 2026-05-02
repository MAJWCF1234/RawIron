#include "RawIron/Games/LiminalHall/LiminalHallBenchmark.h"
#include "RawIron/Games/LiminalHall/LiminalHallWorld.h"

#include "RawIron/Content/EngineAssets.h"
#include "RawIron/Content/GameManifest.h"
#include "RawIron/Render/ScenePreview.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <sstream>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace ri::games::liminal {

namespace {

namespace fs = std::filesystem;

} // namespace

bool RunLiminalHallSoftwareRenderBenchmark(const fs::path& workspaceRoot,
                                           const int viewportWidth,
                                           const int viewportHeight,
                                           const std::uint32_t warmupFrames,
                                           const std::uint32_t timedFrames,
                                           const bool lowSpecMode,
                                           std::string* reportOut,
                                           std::string* errorOut) {
    try {
        if (viewportWidth <= 0 || viewportHeight <= 0) {
            if (errorOut != nullptr) {
                *errorOut = "Benchmark viewport dimensions must be positive.";
            }
            return false;
        }
        if (timedFrames == 0U) {
            if (errorOut != nullptr) {
                *errorOut = "Benchmark timedFrames must be > 0.";
            }
            return false;
        }

        const std::optional<ri::content::GameManifest> manifestOpt =
            ri::content::ResolveGameManifest(workspaceRoot, "liminal-hall");
        if (!manifestOpt.has_value()) {
            if (errorOut != nullptr) {
                *errorOut = "Could not resolve liminal-hall manifest for CPU render benchmark.";
            }
            return false;
        }
        const ri::content::GameManifest& manifest = *manifestOpt;
        World world =
            BuildWorld(manifest.name.empty() ? std::string_view{"LiminalHall"} : std::string_view{manifest.name},
                       manifest.rootPath);

        ri::render::software::ScenePreviewOptions preview{};
        preview.width = viewportWidth;
        preview.height = viewportHeight;
        preview.orderedDither = false;
        preview.pointSampleTextures = true;
        preview.lowSpecMode = lowSpecMode;

        fs::path exePath{};
#if defined(_WIN32)
        wchar_t moduleWide[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, moduleWide, MAX_PATH) > 0) {
            exePath = fs::path(std::wstring(moduleWide));
        }
#endif
        const fs::path textureDir = ri::content::PickEngineTexturesDirectory(workspaceRoot, exePath);
        if (!textureDir.empty()) {
            preview.textureRoot = textureDir;
        }

        ri::render::software::ScenePreviewCache previewCache{};
        ri::render::software::SoftwareImage previewImage{};
        using Clock = std::chrono::high_resolution_clock;
        const auto benchOnce = [&]() {
            const Clock::time_point start = Clock::now();
            ri::render::software::RenderScenePreviewInto(
                world.scene,
                world.playerCameraNode,
                preview,
                previewImage,
                &previewCache);
            const Clock::time_point end = Clock::now();
            return std::chrono::duration<double, std::milli>(end - start).count();
        };

        for (std::uint32_t index = 0; index < warmupFrames; ++index) {
            (void)benchOnce();
        }

        std::vector<double> samples{};
        samples.reserve(static_cast<std::size_t>(timedFrames));
        double sumMs = 0.0;
        double minMs = std::numeric_limits<double>::infinity();
        double maxMs = 0.0;
        for (std::uint32_t index = 0; index < timedFrames; ++index) {
            const double ms = benchOnce();
            samples.push_back(ms);
            sumMs += ms;
            minMs = std::min(minMs, ms);
            maxMs = std::max(maxMs, ms);
        }

        std::vector<double> sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        const double medianMs = sorted[sorted.size() / 2U];
        const double meanMs = sumMs / static_cast<double>(timedFrames);
        const double fpsMedian = medianMs > 1e-9 ? (1000.0 / medianMs) : 0.0;

        std::ostringstream text{};
        text.setf(std::ios::fixed);
        text.precision(3);
        text << "Liminal Hall software renderer (CPU)\n";
        text << "Viewport: " << viewportWidth << "x" << viewportHeight << "\n";
        text << "Low-spec profile: " << (lowSpecMode ? "enabled" : "disabled") << "\n";
        text << "Warmup frames: " << warmupFrames << "  Timed frames: " << timedFrames << "\n";
        text << "Median: " << medianMs << " ms/frame  (~" << fpsMedian << " fps)\n";
        text << "Mean:   " << meanMs << " ms/frame\n";
        text << "Min:    " << minMs << " ms   Max: " << maxMs << " ms\n";
        text << "Note: RunStandalone uses this path each frame; resolution dominates cost.\n";

        if (reportOut != nullptr) {
            *reportOut = text.str();
        }
        return true;
    } catch (const std::exception& exception) {
        if (errorOut != nullptr) {
            *errorOut = exception.what();
        }
        return false;
    }
}

} // namespace ri::games::liminal
