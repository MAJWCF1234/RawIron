#include "RawIron/Games/GamePluginRuntimeBridge.h"

#include "RawIron/Core/Log.h"
#include "RawIron/Games/RuntimeDiagnosticsStandaloneDraw.h"
#include "RawIron/Runtime/RuntimeEventBus.h"

#include <algorithm>

namespace ri::games {

namespace {

void RecordPluginEvent(GamePluginRuntimeHost& host, const ri::content::PluginRuntimeEvent& event) {
    std::string line = event.hookPhase + " " + event.pluginId + " :: " + event.eventName;
    if (!event.message.empty()) {
        line += " — " + event.message;
    }
    host.recentEvents.push_back(std::move(line));
    while (host.recentEvents.size() > host.maxRecentEvents) {
        host.recentEvents.erase(host.recentEvents.begin());
    }
}

void EmitToRuntimeBus(GamePluginRuntimeHost& host, const ri::content::PluginRuntimeEvent& event) {
    if (host.runtimeEvents == nullptr) {
        return;
    }
    ri::runtime::RuntimeEvent runtimeEvent{};
    runtimeEvent.id = event.pluginId + "." + event.eventName;
    runtimeEvent.type = event.type;
    runtimeEvent.fields["pluginId"] = event.pluginId;
    runtimeEvent.fields["eventName"] = event.eventName;
    runtimeEvent.fields["hookPhase"] = event.hookPhase;
    runtimeEvent.fields["handled"] = event.handled ? "1" : "0";
    runtimeEvent.fields["message"] = event.message;
    host.runtimeEvents->EmitScoped(event.type, "plugins", "runtime", std::move(runtimeEvent));
}

void InstallPluginEventSink(GamePluginRuntimeHost& host) {
    host.session.eventSink = [&host](const ri::content::PluginRuntimeEvent& event) {
        RecordPluginEvent(host, event);
        EmitToRuntimeBus(host, event);
    };
}

} // namespace

void BootstrapGamePluginRuntime(GamePluginRuntimeHost& host, const std::filesystem::path& gameRoot) {
    host.session.gameRoot = gameRoot;
    InstallPluginEventSink(host);
    host.session.Bootstrap();

    for (const ri::content::PluginValidationIssue& issue : host.session.projectData.issues) {
        ri::core::LogInfo("Plugin validation: " + issue.message);
    }
    for (const ri::content::PluginHookResult& result : host.session.startupResults) {
        if (result.handled) {
            ri::core::LogInfo("Plugin startup [" + result.pluginId + "] " + result.eventName + ": " + result.message);
        }
    }
    ri::core::LogInfo(
        "Plugin runtime: " + ri::content::SummarizePluginProjectData(host.session.projectData)
        + " startupHooks=" + std::to_string(host.session.startupHooksExecuted));
}

void WireGamePluginEventBus(GamePluginRuntimeHost& host) {
    InstallPluginEventSink(host);
    for (const ri::content::PluginHookResult& result : host.session.startupResults) {
        RecordPluginEvent(
            host,
            ri::content::PluginRuntimeEvent{
                .type = "plugin.hook." + result.eventName,
                .pluginId = result.pluginId,
                .eventName = result.eventName,
                .hookPhase = "startup",
                .handled = result.handled,
                .message = result.message,
            });
        EmitToRuntimeBus(
            host,
            ri::content::PluginRuntimeEvent{
                .type = "plugin.hook." + result.eventName,
                .pluginId = result.pluginId,
                .eventName = result.eventName,
                .hookPhase = "startup",
                .handled = result.handled,
                .message = result.message,
            });
    }
}

void ApplyGamePluginRenderTuning(GamePluginRuntimeHost& host, const GamePluginRenderTuning& tuning) {
    const float ambientGain =
        ri::content::ScriptScalarOr(host.session.runtimeScalars, "ambient_presence_gain", -1.0f);
    if (tuning.exposure != nullptr && ambientGain >= 0.0f) {
        *tuning.exposure = std::clamp(*tuning.exposure * (0.88f + ambientGain * 0.12f), 0.5f, 2.5f);
    }
    if (tuning.ambientLight != nullptr && ambientGain >= 0.0f) {
        *tuning.ambientLight = ri::math::Vec3{
            std::clamp(tuning.ambientLight->x * (0.9f + ambientGain * 0.1f), 0.0f, 1.0f),
            std::clamp(tuning.ambientLight->y * (0.9f + ambientGain * 0.1f), 0.0f, 1.0f),
            std::clamp(tuning.ambientLight->z * (0.9f + ambientGain * 0.1f), 0.0f, 1.0f),
        };
    }
    if (tuning.qualityTier != nullptr && host.renderBoostActive) {
        *tuning.qualityTier = std::min(2, *tuning.qualityTier + 1);
    }
    if (tuning.exposure != nullptr && host.renderBoostActive) {
        *tuning.exposure = std::clamp(*tuning.exposure * 1.02f, 0.5f, 2.5f);
    }
}

std::size_t TickGamePluginRuntime(GamePluginRuntimeHost& host, const double elapsedSeconds) {
    const std::size_t executed = host.session.TickRuntime(elapsedSeconds);
    if (executed > 0U && host.session.frameCounter % 60 == 0) {
        ri::core::LogInfo("Plugin runtime tick: " + std::to_string(executed) + " hook(s) at t="
                          + std::to_string(elapsedSeconds));
    }
    return executed;
}

void MaybeLogPluginDiagnostics(GamePluginRuntimeHost& host,
                               const bool diagnosticsVisible,
                               const float deltaSeconds) {
    if (!diagnosticsVisible) {
        host.diagnosticsLogAccumSeconds = 0.0;
        return;
    }
    host.diagnosticsLogAccumSeconds += static_cast<double>(deltaSeconds);
    if (host.diagnosticsLogAccumSeconds < 2.0) {
        return;
    }
    host.diagnosticsLogAccumSeconds = 0.0;
    ri::core::LogInfo("[Plugins] " + SummarizeGamePluginDiagnostics(host));
    for (const std::string& line : host.recentEvents) {
        ri::core::LogInfo("[Plugins] " + line);
    }
}

std::string SummarizeGamePluginDiagnostics(const GamePluginRuntimeHost& host) {
    std::string summary = ri::content::SummarizePluginProjectData(host.session.projectData);
    summary += " frames=" + std::to_string(host.session.frameCounter);
    summary += " recent=" + std::to_string(host.recentEvents.size());
    if (host.renderBoostActive) {
        summary += " renderBoost=on";
    }
    return summary;
}

std::vector<std::string> CollectPluginDiagnosticLines(const GamePluginRuntimeHost& host) {
    std::vector<std::string> lines{};
    lines.push_back("[Plugins] " + SummarizeGamePluginDiagnostics(host));
    for (const ri::content::ActivePlugin& active : host.session.projectData.activePlugins) {
        lines.push_back("  · " + active.manifest.id + " (" + active.manifest.category + ")");
    }
    for (const std::string& eventLine : host.recentEvents) {
        lines.push_back("  " + eventLine);
    }
    while (lines.size() > 10U) {
        lines.erase(lines.begin() + 1);
    }
    return lines;
}

#if defined(_WIN32)
void DrawPluginDiagnosticsOverlay(void* const hwnd,
                                const GamePluginRuntimeHost& host,
                                const bool diagnosticsVisible) {
    if (!diagnosticsVisible || hwnd == nullptr) {
        return;
    }
    DrawStandaloneDiagnosticsTextOverlay(hwnd, CollectPluginDiagnosticLines(host));
}
#endif

bool ResolvePluginRenderBoost(const ri::content::ScriptScalarMap& plugins,
                              const ri::content::ScriptScalarMap& pluginsPolicy,
                              const std::size_t manifestPluginCount) {
    return ri::content::ScriptScalarOrBool(plugins, "plugin_render_priority_boost", false)
        && ri::content::ScriptScalarOrBool(pluginsPolicy, "allow_runtime_plugin_overrides", true)
        && manifestPluginCount > 0U;
}

} // namespace ri::games
