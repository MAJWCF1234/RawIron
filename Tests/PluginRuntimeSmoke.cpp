#include "RawIron/Content/PluginRuntime.h"

#include <cstdlib>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

int main() {
    ri::content::ClearPluginHookHandlers();
    ri::content::RegisterPluginHookHandler(
        "decline_test",
        [](ri::content::PluginHookContext&, const ri::content::PluginHookInvocation&) {
            return false;
        });
    ri::content::RegisterPluginHookHandler(
        "throw_test",
        [](ri::content::PluginHookContext&, const ri::content::PluginHookInvocation&) -> bool {
            throw std::runtime_error("fixture failure");
        });

    ri::content::PluginProjectData data{};
    data.activePlugins.push_back(ri::content::ActivePlugin{
        .manifest = {.id = "runtime.test", .category = "test"},
        .registry = {.id = "runtime.test", .enabled = true},
        .hooks = {
            {.hookPhase = "runtime", .pluginId = "runtime.test", .eventName = "decline_test", .priority = 0},
            {.hookPhase = "runtime", .pluginId = "runtime.test", .eventName = "throw_test", .priority = 1},
        },
    });

    std::vector<ri::content::PluginRuntimeEvent> events;
    ri::content::PluginHookContext context{
        .projectData = &data,
        .eventSink = [&](const ri::content::PluginRuntimeEvent& event) { events.push_back(event); },
    };
    const std::size_t executed = ri::content::DispatchPluginHooks(context, "runtime");
    ri::content::ClearPluginHookHandlers();

    if (executed != 2U || context.results.size() != 2U || events.size() != 2U) {
        return EXIT_FAILURE;
    }
    if (context.results[0].handled || context.results[0].message.find("declined") == std::string::npos
        || events[0].handled) {
        return EXIT_FAILURE;
    }
    if (context.results[1].handled || context.results[1].message.find("fixture failure") == std::string::npos
        || events[1].handled) {
        return EXIT_FAILURE;
    }

    data.policy.allowRuntimeOverrides = false;
    data.tuningScalars["locked_tuning"] = 1.0f;
    ri::content::PluginHookContext overrideContext{
        .projectData = &data,
        .runtimeScalars = {{"locked_tuning", 9.0f}, {"plugin_owned", 3.0f}},
    };
    (void)ri::content::DispatchPluginHooks(overrideContext, "runtime");
    if (overrideContext.runtimeScalars["locked_tuning"] != 1.0f
        || overrideContext.runtimeScalars["plugin_owned"] != 3.0f) {
        return EXIT_FAILURE;
    }
    data.policy.allowRuntimeOverrides = true;

    ri::content::PluginHookContext throwingSinkContext{
        .projectData = &data,
        .eventSink = [](const ri::content::PluginRuntimeEvent&) { throw std::runtime_error("sink failure"); },
    };
    const std::size_t throwingSinkExecuted = ri::content::DispatchPluginHooks(throwingSinkContext, "runtime");
    if (throwingSinkExecuted != 2U || throwingSinkContext.results.size() != 2U
        || throwingSinkContext.eventSinkFailures != 2U) {
        return EXIT_FAILURE;
    }

    ri::content::ClearPluginHookHandlers();
    ri::content::RegisterPluginHookHandler(
        "slow_startup_test",
        [](ri::content::PluginHookContext&, const ri::content::PluginHookInvocation&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            return true;
        });
    ri::content::RegisterPluginHookHandler(
        "after_budget_test",
        [](ri::content::PluginHookContext&, const ri::content::PluginHookInvocation&) { return true; });
    ri::content::PluginProjectData budgetData{};
    budgetData.policy.startupTimeoutMs = 5;
    budgetData.activePlugins.push_back(ri::content::ActivePlugin{
        .manifest = {.id = "budget.test", .category = "test"},
        .registry = {.id = "budget.test", .enabled = true},
        .hooks = {
            {.hookPhase = "startup", .pluginId = "budget.test", .eventName = "slow_startup_test", .priority = 0},
            {.hookPhase = "startup", .pluginId = "budget.test", .eventName = "after_budget_test", .priority = 1},
        },
    });
    ri::content::PluginHookContext budgetContext{.projectData = &budgetData};
    const std::size_t budgetExecuted = ri::content::DispatchPluginHooks(budgetContext, "startup");
    if (budgetExecuted != 1U || !budgetContext.hookBudgetExceeded
        || budgetContext.hooksSkippedByBudget != 1U || budgetContext.hookElapsedMs < 5.0) {
        return EXIT_FAILURE;
    }

    ri::content::ClearPluginHookHandlers();
    bool capabilityHandlerCalled = false;
    ri::content::RegisterPluginHookHandler(
        "capability_test",
        "test.secure",
        [&capabilityHandlerCalled](ri::content::PluginHookContext&,
                                   const ri::content::PluginHookInvocation&) {
            capabilityHandlerCalled = true;
            return true;
        });
    ri::content::PluginProjectData capabilityData{};
    capabilityData.policy.enforceDeclaredCapabilities = true;
    capabilityData.registryEntries.push_back(
        ri::content::PluginRegistryEntry{.id = "capability.test", .enabled = true});
    capabilityData.activePlugins.push_back(ri::content::ActivePlugin{
        .manifest = {.id = "capability.test", .category = "test"},
        .registry = {.id = "capability.test", .enabled = true},
        .hooks = {
            {.hookPhase = "secure", .pluginId = "capability.test", .eventName = "capability_test", .priority = 0},
        },
    });
    ri::content::PluginHookContext deniedContext{.projectData = &capabilityData};
    const std::size_t deniedExecuted = ri::content::DispatchPluginHooks(deniedContext, "secure");
    if (deniedExecuted != 1U || capabilityHandlerCalled || deniedContext.capabilityDenials != 1U
        || deniedContext.results.size() != 1U || !deniedContext.results.front().capabilityDenied
        || deniedContext.results.front().requiredCapability != "test.secure") {
        return EXIT_FAILURE;
    }

    capabilityData.registryEntries.front().capabilities.push_back("test.secure");
    ri::content::PluginHookContext grantedContext{.projectData = &capabilityData};
    const std::size_t grantedExecuted = ri::content::DispatchPluginHooks(grantedContext, "secure");
    if (grantedExecuted != 1U || !capabilityHandlerCalled || grantedContext.capabilityDenials != 0U
        || grantedContext.results.size() != 1U || !grantedContext.results.front().handled) {
        return EXIT_FAILURE;
    }

    ri::content::ClearPluginHookHandlers();
    std::atomic<bool> registrationFailed = false;
    std::vector<std::thread> workers;
    for (int index = 0; index < 8; ++index) {
        workers.emplace_back([index, &registrationFailed] {
            const std::string name = "parallel_test_" + std::to_string(index);
            ri::content::RegisterPluginHookHandler(
                name,
                [](ri::content::PluginHookContext&, const ri::content::PluginHookInvocation&) { return true; });
            if (!ri::content::IsPluginHookHandlerRegistered(name)) {
                registrationFailed.store(true);
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }
    const std::vector<std::string> names = ri::content::RegisteredPluginHookHandlerNames();
    ri::content::ClearPluginHookHandlers();
    if (registrationFailed || names.size() < 17U) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
