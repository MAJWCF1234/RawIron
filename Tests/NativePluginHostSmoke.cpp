#include "RawIron/Content/NativePluginHost.h"
#include "RawIron/Content/PluginRuntime.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace {

void WriteText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::trunc);
    stream << text;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    const std::filesystem::path modulePath = argv[1];
    ri::content::ClearPluginHookHandlers();

    ri::content::NativePluginHost host;
    const ri::content::NativePluginLoadResult denied = host.Load(
        modulePath,
        {
            .allowedRoot = modulePath.parent_path(),
            .allowedCapabilities = {},
            .enforceAllowedCapabilities = true,
        });
    if (denied.loaded || denied.diagnostic.find("not allowed") == std::string::npos) {
        return EXIT_FAILURE;
    }

    const ri::content::NativePluginLoadResult loaded = host.Load(
        modulePath,
        {
            .allowedRoot = modulePath.parent_path(),
            .allowedCapabilities = {"native.fixture"},
            .enforceAllowedCapabilities = true,
        });
    if (!loaded.loaded
        || loaded.pluginId != "rawiron.test.native-fixture"
        || loaded.pluginVersion != "1.0.0"
        || loaded.registeredHookCount != 1U
        || host.LoadedPluginCount() != 1U
        || !ri::content::IsPluginHookHandlerRegistered("native_fixture_tick")) {
        return EXIT_FAILURE;
    }

    ri::content::PluginProjectData project{};
    project.policy.enforceDeclaredCapabilities = true;
    project.registryEntries.push_back({
        .id = "creator.native-fixture",
        .enabled = true,
        .capabilities = {"native.fixture"},
    });
    project.activePlugins.push_back({
        .manifest = {.id = "creator.native-fixture", .category = "test"},
        .registry = {
            .id = "creator.native-fixture",
            .enabled = true,
            .capabilities = {"native.fixture"},
        },
        .hooks = {
            {
                .hookPhase = "runtime",
                .pluginId = "creator.native-fixture",
                .eventName = "native_fixture_tick",
            },
        },
    });
    ri::content::PluginHookContext context{
        .projectData = &project,
        .frameIndex = 7,
    };
    const std::size_t executed = ri::content::DispatchPluginHooks(context, "runtime");
    if (executed != 1U
        || context.results.size() != 1U
        || !context.results.front().handled
        || context.capabilityDenials != 0U) {
        return EXIT_FAILURE;
    }

    host.UnloadAll();
    if (host.LoadedPluginCount() != 0U
        || ri::content::IsPluginHookHandlerRegistered("native_fixture_tick")) {
        return EXIT_FAILURE;
    }

    // Unsafe module paths must be rejected before the OS loader runs.
    {
        const std::filesystem::path unsafeRoot =
            std::filesystem::temp_directory_path() / "RawIronNativePluginHostUnsafeSmoke";
        std::error_code unsafeError;
        std::filesystem::remove_all(unsafeRoot, unsafeError);
        const std::filesystem::path pluginsRoot = unsafeRoot / "plugins";
        std::filesystem::create_directories(pluginsRoot, unsafeError);
        if (unsafeError) {
            return EXIT_FAILURE;
        }

        // Always-exercised case: a directory named like a DLL must be rejected.
        const std::filesystem::path directoryModule = pluginsRoot / "directory-as-module.dll";
        std::filesystem::create_directories(directoryModule, unsafeError);
        if (unsafeError) {
            std::filesystem::remove_all(unsafeRoot, unsafeError);
            return EXIT_FAILURE;
        }
        const ri::content::NativePluginLoadResult viaDirectory = host.Load(
            directoryModule,
            {
                .allowedRoot = pluginsRoot,
                .allowedCapabilities = {"native.fixture"},
                .enforceAllowedCapabilities = true,
            });
        if (viaDirectory.loaded) {
            std::filesystem::remove_all(unsafeRoot, unsafeError);
            return EXIT_FAILURE;
        }
        std::filesystem::remove_all(directoryModule, unsafeError);

#if defined(_WIN32)
        const std::filesystem::path realModule = unsafeRoot / "real" / modulePath.filename();
        const std::filesystem::path linkModule = pluginsRoot / modulePath.filename();
        std::filesystem::create_directories(realModule.parent_path(), unsafeError);
        std::filesystem::copy_file(
            modulePath, realModule, std::filesystem::copy_options::overwrite_existing, unsafeError);
        if (unsafeError) {
            std::filesystem::remove_all(unsafeRoot, unsafeError);
            return EXIT_FAILURE;
        }
        const std::wstring linkWide = linkModule.wstring();
        const std::wstring targetWide = realModule.wstring();
        if (CreateSymbolicLinkW(linkWide.c_str(),
                                targetWide.c_str(),
                                SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)
            || CreateSymbolicLinkW(linkWide.c_str(), targetWide.c_str(), 0)) {
            const ri::content::NativePluginLoadResult viaLink = host.Load(
                linkModule,
                {
                    .allowedRoot = pluginsRoot,
                    .allowedCapabilities = {"native.fixture"},
                    .enforceAllowedCapabilities = true,
                });
            const bool rejectedAsUnsafe =
                viaLink.diagnostic.find("symlink") != std::string::npos
                || viaLink.diagnostic.find("reparse") != std::string::npos
                || viaLink.diagnostic.find("escapes") != std::string::npos;
            if (viaLink.loaded || !rejectedAsUnsafe) {
                std::filesystem::remove_all(unsafeRoot, unsafeError);
                return EXIT_FAILURE;
            }
            std::filesystem::remove(linkModule, unsafeError);
        }
#else
        const std::filesystem::path realModule = unsafeRoot / "real" / modulePath.filename();
        const std::filesystem::path linkModule = pluginsRoot / modulePath.filename();
        std::filesystem::create_directories(realModule.parent_path(), unsafeError);
        std::filesystem::copy_file(
            modulePath, realModule, std::filesystem::copy_options::overwrite_existing, unsafeError);
        if (unsafeError) {
            std::filesystem::remove_all(unsafeRoot, unsafeError);
            return EXIT_FAILURE;
        }
        std::filesystem::create_symlink(realModule, linkModule, unsafeError);
        if (unsafeError) {
            // Symlink creation should succeed on POSIX CI; treat failure as a real test fault.
            std::filesystem::remove_all(unsafeRoot, unsafeError);
            return EXIT_FAILURE;
        }
        const ri::content::NativePluginLoadResult viaLink = host.Load(
            linkModule,
            {
                .allowedRoot = pluginsRoot,
                .allowedCapabilities = {"native.fixture"},
                .enforceAllowedCapabilities = true,
            });
        const bool rejectedAsUnsafe =
            viaLink.diagnostic.find("symlink") != std::string::npos
            || viaLink.diagnostic.find("reparse") != std::string::npos
            || viaLink.diagnostic.find("escapes") != std::string::npos;
        if (viaLink.loaded || !rejectedAsUnsafe) {
            std::filesystem::remove_all(unsafeRoot, unsafeError);
            return EXIT_FAILURE;
        }
#endif
        std::filesystem::remove_all(unsafeRoot, unsafeError);
    }

    const std::filesystem::path projectRoot =
        std::filesystem::temp_directory_path() / "RawIronNativePluginHostSmoke";
    std::error_code error;
    std::filesystem::remove_all(projectRoot, error);
    const std::filesystem::path stagedModule =
        projectRoot / "plugins" / modulePath.filename();
    std::filesystem::create_directories(stagedModule.parent_path(), error);
    std::filesystem::copy_file(
        modulePath, stagedModule, std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        return EXIT_FAILURE;
    }
    WriteText(
        projectRoot / "plugins" / "manifest.plugins",
        "creator.native-fixture,1.0,test,plugins/" + stagedModule.filename().generic_string() + "\n");
    WriteText(projectRoot / "plugins" / "load_order.cfg", "creator.native-fixture=1\n");
    WriteText(
        projectRoot / "plugins" / "registry.json",
        R"({"plugins":[{"id":"creator.native-fixture","enabled":true,"capabilities":["native.fixture"]}]})");
    WriteText(
        projectRoot / "plugins" / "hooks.riplugin",
        "runtime,creator.native-fixture,native_fixture_tick,0\n");
    WriteText(
        projectRoot / "config" / "plugins.policy",
        "allow_project_plugins=1\n"
        "enforce_plugin_capabilities=1\n");
    WriteText(projectRoot / "scripts" / "plugins.riscript", "plugin_hook_batch_size=1\n");

    {
        ri::content::GamePluginRuntimeSession session{};
        session.gameRoot = projectRoot;
        session.Bootstrap();
        if (session.nativePluginLoads.size() != 1U
            || !session.nativePluginLoads.front().loaded
            || !ri::content::IsPluginHookHandlerRegistered("native_fixture_tick")) {
            return EXIT_FAILURE;
        }
        for (const ri::content::PluginValidationIssue& issue : session.projectData.issues) {
            if (issue.message.find("no engine handler") != std::string::npos
                || issue.message.find("Native plugin load failed") != std::string::npos) {
                return EXIT_FAILURE;
            }
        }
        for (int frame = 0; frame < 7; ++frame) {
            if (session.TickRuntime(1.0 / 60.0) != 1U) {
                return EXIT_FAILURE;
            }
        }
    }
    if (ri::content::IsPluginHookHandlerRegistered("native_fixture_tick")) {
        return EXIT_FAILURE;
    }
    std::filesystem::remove_all(projectRoot, error);
    return EXIT_SUCCESS;
}
