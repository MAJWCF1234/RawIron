#pragma once

#include <cstddef>

namespace ri::dev {

/// Runtime options for \ref DevelopmentInspector.
/// Intended for development / automation only; disable in shipping player configurations when desired.
struct InspectorConfig {
    bool enabled = true;
    /// When false, \ref TryHandleCommand always rejects. Prevents accidental remote control in packaged builds.
    bool allowDevelopmentCommands = false;
    /// Hard cap on queued diagnostics before \ref Pump; oldest entries are dropped first (FIFO). Use `0` for the default (8192).
    std::size_t maxBufferedDiagnostics = 8192;
};

} // namespace ri::dev
