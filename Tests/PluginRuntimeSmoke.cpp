#include "RawIron/Content/PluginRuntime.h"

#include <cstdlib>
#include <atomic>
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
