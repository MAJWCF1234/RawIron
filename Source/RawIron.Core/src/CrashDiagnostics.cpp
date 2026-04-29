#include "RawIron/Core/CrashDiagnostics.h"

#include "RawIron/Core/Log.h"

#include <exception>
#include <mutex>
#include <sstream>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if RAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS
#include "backward.hpp"
#endif

namespace ri::core {
namespace {

CrashDiagnosticsConfig g_config{};
std::once_flag g_initializeOnce{};

[[nodiscard]] std::size_t ClampMaxFrames(std::size_t maxFrames) noexcept {
    return maxFrames == 0 ? 1 : maxFrames;
}

#if RAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS
void LogUnhandledThrowableDetails() {
    try {
        throw;
    } catch (const std::exception& exception) {
        LogInfo(std::string("Unhandled exception: ") + exception.what());
    } catch (...) {
        LogInfo("Unhandled exception: non-standard throwable.");
    }
}

[[noreturn]] void TerminateHandler() {
    LogSection("Fatal Terminate");
    LogInfo("std::terminate invoked.");
    if (std::current_exception() != nullptr) {
        LogUnhandledThrowableDetails();
    }
    LogInfo(CaptureStackTraceText(g_config.maxStackFrames));
    std::abort();
}

#if defined(_WIN32)
LONG WINAPI UnhandledExceptionFilterBridge(_EXCEPTION_POINTERS* exceptionPointers) {
    (void)exceptionPointers;
    LogSection("Fatal Unhandled Exception");
    LogInfo("Windows structured exception reached top level.");
    LogInfo(CaptureStackTraceText(g_config.maxStackFrames));
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif
#endif

} // namespace

bool AreCrashDiagnosticsEnabled() noexcept {
#if RAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS
    return true;
#else
    return false;
#endif
}

void InitializeCrashDiagnostics(const CrashDiagnosticsConfig& config) {
    g_config = config;
    g_config.maxStackFrames = ClampMaxFrames(g_config.maxStackFrames);

#if RAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS
    std::call_once(g_initializeOnce, [config]() {
        if (config.installTerminateHandler) {
            std::set_terminate(&TerminateHandler);
        }
#if defined(_WIN32)
        if (config.installUnhandledExceptionFilter) {
            SetUnhandledExceptionFilter(&UnhandledExceptionFilterBridge);
        }
#endif
    });
#else
    (void)config;
#endif
}

std::string CaptureStackTraceText(const std::size_t maxFrames) {
#if RAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS
    backward::StackTrace stackTrace;
    stackTrace.load_here(static_cast<unsigned>(ClampMaxFrames(maxFrames)));

    backward::Printer printer;
    printer.object = true;
    printer.address = true;
    printer.color_mode = backward::ColorMode::never;

    std::ostringstream stream;
    printer.print(stackTrace, stream);
    return stream.str();
#else
    (void)maxFrames;
    return "Stacktrace diagnostics disabled (RAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS=OFF).";
#endif
}

void LogCurrentExceptionWithStackTrace(const std::string_view sectionTitle) {
    LogSection(sectionTitle);
    if (std::current_exception() != nullptr) {
        try {
            throw;
        } catch (const std::exception& exception) {
            LogInfo(std::string("Exception: ") + exception.what());
        } catch (...) {
            LogInfo("Exception: non-standard throwable.");
        }
    } else {
        LogInfo("Exception: <none>");
    }

    LogInfo(CaptureStackTraceText(g_config.maxStackFrames));
}

} // namespace ri::core
