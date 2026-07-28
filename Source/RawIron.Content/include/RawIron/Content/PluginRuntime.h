#pragma once

#include "RawIron/Content/PluginProjectData.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::content {

/// One hook invocation after load-order and policy filtering.
struct PluginHookInvocation {
    std::string pluginId;
    std::string hookPhase;
    std::string eventName;
    int priority = 0;
    std::string hookGroup;
    std::string category;
    std::vector<std::string> grantedCapabilities;
};

struct PluginHookResult {
    std::string pluginId;
    std::string eventName;
    bool handled = false;
    bool capabilityDenied = false;
    std::string requiredCapability;
    std::string message;
};

/// Optional fan-out when a hook executes (games wire this to RuntimeEventBus).
struct PluginRuntimeEvent {
    std::string type;
    std::string pluginId;
    std::string eventName;
    std::string hookPhase;
    bool handled = false;
    bool capabilityDenied = false;
    std::string requiredCapability;
    std::string message;
};

using PluginRuntimeEventSink = std::function<void(const PluginRuntimeEvent& event)>;

struct PluginHookContext {
    std::filesystem::path gameRoot;
    const PluginProjectData* projectData = nullptr;
    std::string_view hookPhase;
    double elapsedSeconds = 0.0;
    int frameIndex = 0;
    std::vector<PluginHookResult> results;
    ScriptScalarMap runtimeScalars;
    PluginRuntimeEventSink eventSink;
    /// Counts event-sink callback failures isolated by the runtime boundary.
    std::size_t eventSinkFailures = 0;
    /// Hooks rejected before handler execution because their registry grant was missing.
    std::size_t capabilityDenials = 0;
    /// Cooperative phase-budget diagnostics. A running native handler is never forcefully terminated;
    /// once the startup budget is exhausted, remaining hooks are skipped.
    bool hookBudgetExceeded = false;
    std::size_t hooksSkippedByBudget = 0;
    double hookElapsedMs = 0.0;
};

struct GamePluginBootstrap {
    PluginProjectData projectData;
    ScriptScalarMap runtimeScalars;
    std::vector<PluginHookResult> startupResults;
    std::size_t startupHooksExecuted = 0;
    bool startupBudgetExceeded = false;
    std::size_t startupHooksSkippedByBudget = 0;
    double startupElapsedMs = 0.0;
};

using PluginHookHandler = std::function<bool(PluginHookContext&, const PluginHookInvocation&)>;

struct PluginHookHandlerInfo {
    std::string eventName;
    std::string requiredCapability;
};

/// Registers built-in handlers for documented event names (bootstrap, frame_sample, …).
void RegisterBuiltinPluginHookHandlers();

void RegisterPluginHookHandler(std::string_view eventName, PluginHookHandler handler);

/// Registers a handler that may only execute for plugins granted `requiredCapability`.
void RegisterPluginHookHandler(std::string_view eventName,
                               std::string_view requiredCapability,
                               PluginHookHandler handler);

void ClearPluginHookHandlers();

[[nodiscard]] bool IsPluginHookHandlerRegistered(std::string_view eventName);

[[nodiscard]] std::vector<std::string> RegisteredPluginHookHandlerNames();

[[nodiscard]] std::vector<PluginHookHandlerInfo> RegisteredPluginHookHandlers();

/// Appends validation issues for hook bindings whose event names have no registered handler.
void AppendPluginHookHandlerIssues(const PluginProjectData& data, std::vector<PluginValidationIssue>& issues);

/// Runs hooks for a phase in priority order. Respects policy max chain and enabled registry entries.
[[nodiscard]] std::size_t DispatchPluginHooks(PluginHookContext& context, std::string_view hookPhase);

/// Loads plugin project data, registers built-in handlers, and runs startup hooks once.
[[nodiscard]] GamePluginBootstrap BootstrapGamePlugins(const std::filesystem::path& gameRoot);

/// Holds plugin project state and throttled runtime hook dispatch for game loops.
struct GamePluginRuntimeSession {
    std::filesystem::path gameRoot;
    PluginProjectData projectData;
    ScriptScalarMap runtimeScalars;
    std::vector<PluginHookResult> startupResults;
    std::size_t startupHooksExecuted = 0;
    bool startupBudgetExceeded = false;
    std::size_t startupHooksSkippedByBudget = 0;
    double startupElapsedMs = 0.0;
    int frameCounter = 0;
    PluginRuntimeEventSink eventSink;

    void Bootstrap();
    [[nodiscard]] std::size_t TickRuntime(double elapsedSeconds);
};

[[nodiscard]] std::string DescribePluginModificationModel();

} // namespace ri::content
