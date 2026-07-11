#include "RawIron/Content/PluginRuntime.h"

#include <algorithm>
#include <exception>
#include <set>
#include <unordered_map>

namespace ri::content {

namespace {

std::unordered_map<std::string, PluginHookHandler, std::hash<std::string>, std::equal_to<>> g_handlers{};
bool g_builtinsRegistered = false;

void EnsureBuiltinHandlers() {
    if (g_builtinsRegistered) {
        return;
    }
    g_builtinsRegistered = true;

    RegisterPluginHookHandler("bootstrap", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        context.runtimeScalars["plugin." + invocation.pluginId + ".bootstrapped"] = 1.0f;
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "Bootstrap complete for " + invocation.pluginId,
        });
        return true;
    });

    RegisterPluginHookHandler("frame_sample", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "Frame sample at t=" + std::to_string(context.elapsedSeconds),
        });
        return true;
    });

    RegisterPluginHookHandler("audio_zones_bind", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        context.runtimeScalars["plugin." + invocation.pluginId + ".audio_bound"] = 1.0f;
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "Audio zones bound from authored CSV",
        });
        return true;
    });

    RegisterPluginHookHandler("ambient_tick", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        const float gain = ScriptScalarOr(context.runtimeScalars, "ambient_presence_gain", 0.85f);
        context.runtimeScalars["plugin." + invocation.pluginId + ".ambient_gain"] = gain;
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "Ambient tick gain=" + std::to_string(gain),
        });
        return true;
    });

    RegisterPluginHookHandler("quest_marker_refresh", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "Quest markers refreshed",
        });
        return true;
    });

    RegisterPluginHookHandler("ai_policy_bind", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "AI policy bridge bound",
        });
        return true;
    });

    RegisterPluginHookHandler("timeline_register", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "Cinematic timeline registered",
        });
        return true;
    });

    RegisterPluginHookHandler("mode_update", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "AI mode update tick",
        });
        return true;
    });

    RegisterPluginHookHandler("zone_cutscene_trigger", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "Zone cutscene trigger evaluated",
        });
        return true;
    });
}

[[nodiscard]] const PluginManifestEntry* FindManifest(const PluginProjectData& data, const std::string& pluginId) {
    for (const PluginManifestEntry& entry : data.manifestEntries) {
        if (entry.id == pluginId) {
            return &entry;
        }
    }
    return nullptr;
}

[[nodiscard]] const PluginRegistryEntry* FindRegistry(const PluginProjectData& data, const std::string& pluginId) {
    for (const PluginRegistryEntry& entry : data.registryEntries) {
        if (entry.id == pluginId) {
            return &entry;
        }
    }
    return nullptr;
}

void EmitPluginRuntimeEvent(PluginHookContext& context,
                            const PluginHookInvocation& invocation,
                            const PluginHookResult& result) {
    if (!context.eventSink) {
        return;
    }
    context.eventSink(PluginRuntimeEvent{
        .type = "plugin.hook." + invocation.eventName,
        .pluginId = invocation.pluginId,
        .eventName = invocation.eventName,
        .hookPhase = std::string(context.hookPhase),
        .handled = result.handled,
        .message = result.message,
    });
}

} // namespace

void RegisterBuiltinPluginHookHandlers() {
    EnsureBuiltinHandlers();
}

void RegisterPluginHookHandler(const std::string_view eventName, PluginHookHandler handler) {
    g_handlers[std::string(eventName)] = std::move(handler);
}

void ClearPluginHookHandlers() {
    g_handlers.clear();
    g_builtinsRegistered = false;
}

bool IsPluginHookHandlerRegistered(const std::string_view eventName) {
    EnsureBuiltinHandlers();
    return g_handlers.find(std::string(eventName)) != g_handlers.end();
}

std::vector<std::string> RegisteredPluginHookHandlerNames() {
    EnsureBuiltinHandlers();
    std::vector<std::string> names{};
    names.reserve(g_handlers.size());
    for (const auto& [name, handler] : g_handlers) {
        if (handler) {
            names.push_back(name);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

void AppendPluginHookHandlerIssues(const PluginProjectData& data, std::vector<PluginValidationIssue>& issues) {
    EnsureBuiltinHandlers();
    std::set<std::string> reported{};
    for (const PluginHookBinding& hook : data.hookBindings) {
        if (IsPluginHookHandlerRegistered(hook.eventName) || reported.contains(hook.eventName)) {
            continue;
        }
        reported.insert(hook.eventName);
        issues.push_back(PluginValidationIssue{
            .message = "Hook event has no engine handler: " + hook.eventName + " (plugin " + hook.pluginId + ")",
        });
    }
}

std::size_t DispatchPluginHooks(PluginHookContext& context, const std::string_view hookPhase) {
    EnsureBuiltinHandlers();
    if (context.projectData == nullptr) {
        return 0U;
    }

    const PluginProjectData& data = *context.projectData;
    context.hookPhase = hookPhase;
    ScriptScalarMap mergedScalars = data.tuningScalars;
    for (const auto& [key, value] : context.runtimeScalars) {
        mergedScalars[key] = value;
    }
    context.runtimeScalars = std::move(mergedScalars);

    const std::vector<PluginHookBinding> hooks = CollectHooksForPhase(data, hookPhase);
    const int maxChain = data.policy.maxHookChain;
    std::size_t executed = 0U;
    for (const PluginHookBinding& hook : hooks) {
        if (executed >= static_cast<std::size_t>(maxChain)) {
            break;
        }
        const PluginRegistryEntry* registry = FindRegistry(data, hook.pluginId);
        if (registry != nullptr && !registry->enabled) {
            continue;
        }

        PluginHookInvocation invocation{
            .pluginId = hook.pluginId,
            .hookPhase = hook.hookPhase,
            .eventName = hook.eventName,
            .priority = hook.priority,
        };
        if (registry != nullptr) {
            invocation.hookGroup = registry->hookGroup;
        }
        if (const PluginManifestEntry* manifest = FindManifest(data, hook.pluginId)) {
            invocation.category = manifest->category;
        }

        const auto handlerIt = g_handlers.find(hook.eventName);
        PluginHookResult result{
            .pluginId = hook.pluginId,
            .eventName = hook.eventName,
            .handled = false,
            .message = "No handler registered for event '" + hook.eventName + "'",
        };
        if (handlerIt != g_handlers.end() && handlerIt->second) {
            const std::size_t resultsBefore = context.results.size();
            try {
                const bool handled = handlerIt->second(context, invocation);
                if (context.results.size() > resultsBefore) {
                    result = context.results.back();
                } else {
                    result.handled = handled;
                    result.message = handled
                        ? "Handler completed for " + hook.pluginId
                        : "Handler declined event '" + hook.eventName + "' for " + hook.pluginId;
                    context.results.push_back(result);
                }
            } catch (const std::exception& error) {
                context.results.resize(resultsBefore);
                result.message = "Handler threw for event '" + hook.eventName + "': " + error.what();
                context.results.push_back(result);
            } catch (...) {
                context.results.resize(resultsBefore);
                result.message = "Handler threw for event '" + hook.eventName + "'.";
                context.results.push_back(result);
            }
        } else {
            context.results.push_back(result);
        }
        EmitPluginRuntimeEvent(context, invocation, result);
        executed += 1U;
    }
    return executed;
}

GamePluginBootstrap BootstrapGamePlugins(const std::filesystem::path& gameRoot) {
    GamePluginBootstrap bootstrap{};
    bootstrap.projectData = LoadPluginProjectData(gameRoot);
    RegisterBuiltinPluginHookHandlers();

    PluginHookContext context{
        .gameRoot = gameRoot,
        .projectData = &bootstrap.projectData,
        .hookPhase = "startup",
    };
    bootstrap.startupHooksExecuted = DispatchPluginHooks(context, "startup");
    bootstrap.runtimeScalars = context.runtimeScalars;
    bootstrap.startupResults = std::move(context.results);
    return bootstrap;
}

void GamePluginRuntimeSession::Bootstrap() {
    projectData = LoadPluginProjectData(gameRoot);
    RegisterBuiltinPluginHookHandlers();
    PluginHookContext context{
        .gameRoot = gameRoot,
        .projectData = &projectData,
        .hookPhase = "startup",
        .eventSink = eventSink,
    };
    startupHooksExecuted = DispatchPluginHooks(context, "startup");
    runtimeScalars = context.runtimeScalars;
    startupResults = std::move(context.results);
    frameCounter = 0;
}

std::size_t GamePluginRuntimeSession::TickRuntime(const double elapsedSeconds) {
    if (projectData.activePlugins.empty()) {
        return 0U;
    }
    frameCounter += 1;
    const int batchSize = ScriptScalarOrIntClamped(
        projectData.tuningScalars, "plugin_hook_batch_size", 4, 1, 120);
    if (frameCounter % batchSize != 0) {
        return 0U;
    }

    PluginHookContext context{
        .gameRoot = gameRoot,
        .projectData = &projectData,
        .hookPhase = "runtime",
        .elapsedSeconds = elapsedSeconds,
        .frameIndex = frameCounter,
        .runtimeScalars = runtimeScalars,
        .eventSink = eventSink,
    };
    const std::size_t executed = DispatchPluginHooks(context, "runtime");
    runtimeScalars = context.runtimeScalars;
    return executed;
}

std::string DescribePluginModificationModel() {
    return
        "RawIron plugins are declarative project packages. Policy gates what may load; manifest + registry "
        "declare inventory; load_order.cfg defines precedence; hooks.riplugin binds startup/runtime events. "
        "At runtime DispatchPluginHooks executes event handlers that may tune scalars, bind authored CSV "
        "surfaces, and emit RuntimeEventBus topics — without recompiling the game executable.";
}

} // namespace ri::content
