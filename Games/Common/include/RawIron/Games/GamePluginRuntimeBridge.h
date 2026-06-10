#pragma once

#include "RawIron/Content/PluginProjectData.h"
#include "RawIron/Content/PluginRuntime.h"
#include "RawIron/Content/ScriptScalars.h"
#include "RawIron/Math/Vec3.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ri::runtime {
class RuntimeEventBus;
}

namespace ri::games {

struct GamePluginRenderTuning {
    int* qualityTier = nullptr;
    float* exposure = nullptr;
    ri::math::Vec3* ambientLight = nullptr;
};

/// Shared plugin runtime state for standalone game loops.
struct GamePluginRuntimeHost {
    ri::content::GamePluginRuntimeSession session;
    bool renderBoostActive = false;
    ri::runtime::RuntimeEventBus* runtimeEvents = nullptr;
    std::vector<std::string> recentEvents;
    std::size_t maxRecentEvents = 8;
    double diagnosticsLogAccumSeconds = 0.0;
};

void BootstrapGamePluginRuntime(GamePluginRuntimeHost& host, const std::filesystem::path& gameRoot);

void WireGamePluginEventBus(GamePluginRuntimeHost& host);

void ApplyGamePluginRenderTuning(GamePluginRuntimeHost& host, const GamePluginRenderTuning& tuning);

[[nodiscard]] std::size_t TickGamePluginRuntime(GamePluginRuntimeHost& host, double elapsedSeconds);

void MaybeLogPluginDiagnostics(GamePluginRuntimeHost& host, bool diagnosticsVisible, float deltaSeconds);

[[nodiscard]] std::vector<std::string> CollectPluginDiagnosticLines(const GamePluginRuntimeHost& host);

#if defined(_WIN32)
void DrawPluginDiagnosticsOverlay(void* hwnd, const GamePluginRuntimeHost& host, bool diagnosticsVisible);
#endif

[[nodiscard]] std::string SummarizeGamePluginDiagnostics(const GamePluginRuntimeHost& host);

[[nodiscard]] bool ResolvePluginRenderBoost(const ri::content::ScriptScalarMap& plugins,
                                            const ri::content::ScriptScalarMap& pluginsPolicy,
                                            std::size_t manifestPluginCount);

} // namespace ri::games
