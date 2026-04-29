#pragma once

#include "RawIron/DevInspector/InspectorConfig.h"
#include "RawIron/DevInspector/InspectorTransport.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::dev {

/// Logical channel for diagnostic stream messages (NDJSON payload uses the numeric value).
enum class InspectorChannel : std::uint8_t {
    Telemetry = 0,
    World = 1,
    DebugVisualization = 2,
    Events = 3,
    Log = 4,
    Command = 5,
};

struct InspectorBrowserLaunchOptions {
    std::string url;
    std::string browserPath;
    bool kioskMode = true;
    bool dryRun = false;
};

[[nodiscard]] std::optional<std::string> BuildInspectorBrowserLaunchCommand(
    const InspectorBrowserLaunchOptions& options);
[[nodiscard]] bool LaunchInspectorBrowser(const InspectorBrowserLaunchOptions& options,
                                          std::string* error = nullptr,
                                          std::string* commandUsed = nullptr);

/// Optional live debugging / introspection side-channel.
///
/// Read-mostly snapshot sources register string payloads (usually JSON text). When enabled, a host may
/// bundle them for external tools, terminal dashboards, or automation. This type does not own gameplay,
/// rendering, or simulation — it only aggregates host-provided views.
///
/// Instances are movable and not copyable (mutex + queues are instance-local).
class DevelopmentInspector {
public:
    explicit DevelopmentInspector(InspectorConfig config = {});

    DevelopmentInspector(const DevelopmentInspector&) = delete;
    DevelopmentInspector& operator=(const DevelopmentInspector&) = delete;

    DevelopmentInspector(DevelopmentInspector&& other) noexcept;
    DevelopmentInspector& operator=(DevelopmentInspector&& other) noexcept;

    ~DevelopmentInspector() = default;

    void SetConfig(InspectorConfig config);
    [[nodiscard]] InspectorConfig Config() const;
    [[nodiscard]] bool IsEnabled() const;

    using SnapshotSource = std::function<std::string()>;
    void RegisterSnapshotSource(std::string id, SnapshotSource source);
    void UnregisterSnapshotSource(std::string_view id);

    [[nodiscard]] std::vector<std::string> SnapshotSourceIds() const;

    /// Bundles snapshot sources into one JSON object.
    ///
    /// Schema **v2**: `{"version":2,"seq":N,"sources":{"id":"<escaped-json-or-text>",...}}`
    ///
    /// If a source callback throws, the corresponding entry stores a tiny JSON error object string.
    [[nodiscard]] std::string BuildSnapshotJson() const;

    void PostDiagnostic(InspectorChannel channel, std::string message);

    void SetTransport(std::shared_ptr<IInspectorTransport> transport);
    [[nodiscard]] std::shared_ptr<IInspectorTransport> Transport() const;

    /// NDJSON schema **v2**: `{"version":2,"channel":n,"seq":N,"msg":"<escaped>"}` .
    void Pump();

    /// Drops queued diagnostics without sending.
    void ClearDiagnosticQueue();

    [[nodiscard]] std::uint64_t DiagnosticsDroppedCount() const;

    using CommandHandler = std::function<std::string(std::string_view name, std::string_view argsJson)>;
    void RegisterCommandHandler(std::string prefix, CommandHandler handler);
    void UnregisterCommandHandlersWithPrefix(std::string_view prefix);

    [[nodiscard]] std::string TryHandleCommand(std::string_view line);

private:
    InspectorConfig config_{};
    mutable std::mutex mutex_;
    std::shared_ptr<IInspectorTransport> transport_;

    struct SnapshotEntry {
        std::string id;
        SnapshotSource source;
    };
    std::vector<SnapshotEntry> sources_;

    struct PendingDiagnostic {
        InspectorChannel channel{};
        std::string message;
    };
    std::deque<PendingDiagnostic> pending_;

    mutable std::uint64_t snapshotSeq_{0};
    std::uint64_t diagnosticSeq_{0};
    std::uint64_t diagnosticsDropped_{0};

    struct CommandEntry {
        std::string prefix;
        CommandHandler handler;
    };
    std::vector<CommandEntry> commandHandlers_;
};

} // namespace ri::dev
