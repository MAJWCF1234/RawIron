#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace ri::core {

struct CrashDiagnosticsConfig {
    bool installTerminateHandler = true;
    bool installUnhandledExceptionFilter = true;
    std::size_t maxStackFrames = 48;
};

[[nodiscard]] bool AreCrashDiagnosticsEnabled() noexcept;
void InitializeCrashDiagnostics(const CrashDiagnosticsConfig& config = {});
[[nodiscard]] std::string CaptureStackTraceText(std::size_t maxFrames = 48);
void LogCurrentExceptionWithStackTrace(std::string_view sectionTitle);

} // namespace ri::core
