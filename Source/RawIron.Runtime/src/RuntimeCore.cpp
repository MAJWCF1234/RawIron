#include "RawIron/Runtime/RuntimeCore.h"

#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Runtime/RuntimeId.h"

#include <algorithm>
#include <utility>

namespace ri::runtime {
namespace {

[[nodiscard]] std::string PhaseField(RuntimePhase phase) {
    return RuntimePhaseName(phase);
}

[[nodiscard]] bool LooksLikeWorkspaceRoot(const std::filesystem::path& path) {
    std::error_code ec{};
    return std::filesystem::exists(path / "CMakeLists.txt", ec) &&
           std::filesystem::exists(path / "Source", ec) &&
           std::filesystem::exists(path / "Documentation", ec);
}

} // namespace

void RuntimeServices::Clear() {
    services_.clear();
}

std::size_t RuntimeServices::Count() const noexcept {
    return services_.size();
}

RuntimeContext::RuntimeContext(RuntimeIdentity identity, RuntimePaths paths)
    : identity_(std::move(identity)),
      paths_(std::move(paths)) {
    if (identity_.instanceId.empty()) {
        identity_.instanceId = CreateRuntimeId(identity_.id.empty() ? "runtime" : identity_.id);
    }
}

const RuntimeIdentity& RuntimeContext::Identity() const noexcept {
    return identity_;
}

const RuntimePaths& RuntimeContext::Paths() const noexcept {
    return paths_;
}

RuntimePhase RuntimeContext::Phase() const noexcept {
    return phase_;
}

RuntimeEventBus& RuntimeContext::Events() noexcept {
    return events_;
}

const RuntimeEventBus& RuntimeContext::Events() const noexcept {
    return events_;
}

RuntimeServices& RuntimeContext::Services() noexcept {
    return services_;
}

const RuntimeServices& RuntimeContext::Services() const noexcept {
    return services_;
}

RuntimeFrameSnapshot RuntimeContext::Frame() const noexcept {
    return frame_;
}

bool RuntimeContext::StopRequested() const noexcept {
    return stopRequested_;
}

std::string_view RuntimeContext::StopReason() const noexcept {
    return stopReason_;
}

std::string_view RuntimeContext::FailureReason() const noexcept {
    return failureReason_;
}

void RuntimeContext::RequestStop(std::string reason) {
    stopRequested_ = true;
    stopReason_ = std::move(reason);
}

void RuntimeContext::Fail(std::string reason) {
    failureReason_ = std::move(reason);
    stopRequested_ = true;
}

void RuntimeContext::SetPhase(RuntimePhase phase) {
    phase_ = phase;
}

void RuntimeContext::SetFrame(RuntimeFrameSnapshot frame) {
    frame_ = frame;
}

void RuntimeContext::ClearRunState() {
    stopRequested_ = false;
    stopReason_.clear();
    failureReason_.clear();
    frame_ = {};
}

bool RuntimeModule::OnRuntimeStartup(RuntimeContext&, const ri::core::CommandLine&) {
    return true;
}

bool RuntimeModule::OnRuntimeFrame(RuntimeContext&, const ri::core::FrameContext&) {
    return true;
}

void RuntimeModule::OnRuntimePause(RuntimeContext&) {}

void RuntimeModule::OnRuntimeResume(RuntimeContext&) {}

void RuntimeModule::OnRuntimeShutdown(RuntimeContext&) {}

RuntimeCore::RuntimeCore(RuntimeIdentity identity, RuntimePaths paths)
    : context_(std::move(identity), std::move(paths)) {}

RuntimeContext& RuntimeCore::Context() noexcept {
    return context_;
}

const RuntimeContext& RuntimeCore::Context() const noexcept {
    return context_;
}

void RuntimeCore::AddModule(std::unique_ptr<RuntimeModule> module) {
    (void)TryAddModule(std::move(module));
}

bool RuntimeCore::TryAddModule(std::unique_ptr<RuntimeModule> module) {
    if (module == nullptr || module->Name().empty()) {
        return false;
    }
    if (HasModule(module->Name())) {
        return false;
    }
    modules_.push_back(std::move(module));
    return true;
}

bool RuntimeCore::HasModule(const std::string_view moduleName) const {
    return std::any_of(modules_.begin(), modules_.end(), [moduleName](const std::unique_ptr<RuntimeModule>& module) {
        return module != nullptr && module->Name() == moduleName;
    });
}

std::vector<std::string> RuntimeCore::ModuleNames() const {
    std::vector<std::string> names;
    names.reserve(modules_.size());
    for (const std::unique_ptr<RuntimeModule>& module : modules_) {
        if (module != nullptr) {
            names.push_back(std::string(module->Name()));
        }
    }
    return names;
}

std::size_t RuntimeCore::ModuleCount() const noexcept {
    return modules_.size();
}

bool RuntimeCore::Startup(const ri::core::CommandLine& commandLine) {
    if (context_.phase_ != RuntimePhase::Uninitialized && context_.phase_ != RuntimePhase::Stopped) {
        context_.Fail("Runtime startup requested from invalid phase.");
        EmitPhaseChanged(context_.phase_, RuntimePhase::Failed);
        return false;
    }

    context_.ClearRunState();
    EmitPhaseChanged(context_.phase_, RuntimePhase::Starting);
    ri::core::LogSection("Runtime Core");
    ri::core::LogInfo("Runtime: " + context_.identity_.displayName +
                      " (" + context_.identity_.id + ") mode=" + context_.identity_.mode);
    if (!context_.paths_.workspaceRoot.empty()) {
        ri::core::LogInfo("Workspace root: " + context_.paths_.workspaceRoot.string());
    }
    if (!context_.paths_.gameRoot.empty()) {
        ri::core::LogInfo("Game root: " + context_.paths_.gameRoot.string());
    }

    EmitPhaseChanged(RuntimePhase::Starting, RuntimePhase::Loading);
    for (const std::unique_ptr<RuntimeModule>& module : modules_) {
        if (module == nullptr) {
            continue;
        }
        if (!module->OnRuntimeStartup(context_, commandLine)) {
            context_.Fail("Runtime module startup failed: " + std::string(module->Name()));
            EmitPhaseChanged(context_.phase_, RuntimePhase::Failed);
            return false;
        }
    }

    EmitPhaseChanged(RuntimePhase::Loading, RuntimePhase::Running);
    context_.events_.Emit("runtime.started", RuntimeEvent{
        .id = {},
        .type = {},
        .fields = {
            {"id", context_.identity_.id},
            {"mode", context_.identity_.mode},
            {"instance", context_.identity_.instanceId},
            {"modules", std::to_string(ModuleCount())},
        },
    });
    return true;
}

bool RuntimeCore::Frame(const ri::core::FrameContext& frame) {
    if (context_.phase_ == RuntimePhase::Failed || context_.phase_ == RuntimePhase::Stopped) {
        return false;
    }
    if (context_.phase_ == RuntimePhase::Paused) {
        return !context_.stopRequested_;
    }
    if (context_.phase_ != RuntimePhase::Running) {
        context_.Fail("Runtime frame requested before running phase.");
        EmitPhaseChanged(context_.phase_, RuntimePhase::Failed);
        return false;
    }

    context_.SetFrame(RuntimeFrameSnapshot{
        .frameIndex = frame.frameIndex,
        .deltaSeconds = frame.deltaSeconds,
        .elapsedSeconds = frame.elapsedSeconds,
        .realtimeSeconds = frame.realtimeSeconds,
        .realDeltaSeconds = frame.realDeltaSeconds,
    });
    context_.events_.Emit("runtime.frame", RuntimeEvent{
        .id = {},
        .type = {},
        .fields = {
            {"index", std::to_string(frame.frameIndex)},
            {"elapsed", std::to_string(frame.elapsedSeconds)},
            {"delta", std::to_string(frame.deltaSeconds)},
        },
    });

    for (const std::unique_ptr<RuntimeModule>& module : modules_) {
        if (module == nullptr) {
            continue;
        }
        if (!module->OnRuntimeFrame(context_, frame)) {
            context_.RequestStop("Runtime module requested stop: " + std::string(module->Name()));
            break;
        }
        if (context_.StopRequested()) {
            break;
        }
    }

    if (context_.StopRequested()) {
        context_.events_.Emit("runtime.stop_requested", RuntimeEvent{
            .id = {},
            .type = {},
            .fields = {
                {"id", context_.identity_.id},
                {"reason", context_.stopReason_},
            },
        });
    }
    return !context_.StopRequested();
}

bool RuntimeCore::Pause(std::string reason) {
    if (context_.phase_ != RuntimePhase::Running) {
        return false;
    }
    for (const std::unique_ptr<RuntimeModule>& module : modules_) {
        if (module != nullptr) {
            module->OnRuntimePause(context_);
        }
    }
    EmitPhaseChanged(RuntimePhase::Running, RuntimePhase::Paused);
    context_.events_.Emit("runtime.paused", RuntimeEvent{
        .id = {},
        .type = {},
        .fields = {
            {"id", context_.identity_.id},
            {"reason", reason},
        },
    });
    return true;
}

bool RuntimeCore::Resume() {
    if (context_.phase_ != RuntimePhase::Paused) {
        return false;
    }
    context_.ClearRunState();
    for (const std::unique_ptr<RuntimeModule>& module : modules_) {
        if (module != nullptr) {
            module->OnRuntimeResume(context_);
        }
    }
    EmitPhaseChanged(RuntimePhase::Paused, RuntimePhase::Running);
    context_.events_.Emit("runtime.resumed", RuntimeEvent{
        .id = {},
        .type = {},
        .fields = {
            {"id", context_.identity_.id},
        },
    });
    return true;
}

void RuntimeCore::Shutdown() {
    if (context_.phase_ == RuntimePhase::Stopped || context_.phase_ == RuntimePhase::Uninitialized) {
        EmitPhaseChanged(context_.phase_, RuntimePhase::Stopped);
        return;
    }

    const RuntimePhase previous = context_.phase_;
    EmitPhaseChanged(previous, RuntimePhase::Stopping);
    for (auto it = modules_.rbegin(); it != modules_.rend(); ++it) {
        if (*it != nullptr) {
            (*it)->OnRuntimeShutdown(context_);
        }
    }
    context_.events_.Emit("runtime.stopped", RuntimeEvent{
        .id = {},
        .type = {},
        .fields = {
            {"id", context_.identity_.id},
            {"mode", context_.identity_.mode},
            {"instance", context_.identity_.instanceId},
            {"stopReason", context_.stopReason_},
            {"failureReason", context_.failureReason_},
        },
    });
    EmitPhaseChanged(RuntimePhase::Stopping, RuntimePhase::Stopped);
}

void RuntimeCore::EmitPhaseChanged(RuntimePhase from, RuntimePhase to) {
    if (from == to && context_.phase_ == to) {
        return;
    }
    context_.SetPhase(to);
    context_.events_.Emit("runtime.phase", RuntimeEvent{
        .id = {},
        .type = {},
        .fields = {
            {"from", PhaseField(from)},
            {"to", PhaseField(to)},
        },
    });
}

RuntimeHostAdapter::RuntimeHostAdapter(RuntimeCore& runtime)
    : runtime_(runtime) {}

std::string_view RuntimeHostAdapter::GetName() const noexcept {
    return runtime_.Context().Identity().displayName;
}

std::string_view RuntimeHostAdapter::GetMode() const noexcept {
    return runtime_.Context().Identity().mode;
}

void RuntimeHostAdapter::OnStartup(const ri::core::CommandLine& commandLine) {
    startupOk_ = runtime_.Startup(commandLine);
}

bool RuntimeHostAdapter::OnFrame(const ri::core::FrameContext& frame) {
    if (!startupOk_) {
        return false;
    }
    return runtime_.Frame(frame);
}

void RuntimeHostAdapter::OnShutdown() {
    runtime_.Shutdown();
}

RuntimeHostModule::RuntimeHostModule(ri::core::Host& host)
    : host_(host) {}

std::string_view RuntimeHostModule::Name() const noexcept {
    return host_.GetName();
}

bool RuntimeHostModule::OnRuntimeStartup(RuntimeContext& context, const ri::core::CommandLine& commandLine) {
    host_.OnStartup(commandLine);
    context.Events().Emit("runtime.host.mounted", RuntimeEvent{
        .id = {},
        .type = {},
        .fields = {
            {"host", std::string(host_.GetName())},
            {"mode", std::string(host_.GetMode())},
        },
    });
    return true;
}

bool RuntimeHostModule::OnRuntimeFrame(RuntimeContext&, const ri::core::FrameContext& frame) {
    return host_.OnFrame(frame);
}

void RuntimeHostModule::OnRuntimeShutdown(RuntimeContext& context) {
    host_.OnShutdown();
    context.Events().Emit("runtime.host.unmounted", RuntimeEvent{
        .id = {},
        .type = {},
        .fields = {
            {"host", std::string(host_.GetName())},
            {"mode", std::string(host_.GetMode())},
        },
    });
}

const char* RuntimePhaseName(RuntimePhase phase) noexcept {
    switch (phase) {
    case RuntimePhase::Uninitialized:
        return "uninitialized";
    case RuntimePhase::Starting:
        return "starting";
    case RuntimePhase::Loading:
        return "loading";
    case RuntimePhase::Running:
        return "running";
    case RuntimePhase::Paused:
        return "paused";
    case RuntimePhase::Stopping:
        return "stopping";
    case RuntimePhase::Stopped:
        return "stopped";
    case RuntimePhase::Failed:
        return "failed";
    }
    return "unknown";
}

RuntimePaths DetectRuntimePaths(std::filesystem::path start) {
    RuntimePaths paths{};
    std::error_code ec{};
    start = std::filesystem::weakly_canonical(start, ec);
    if (ec) {
        start = std::filesystem::current_path();
    }

    std::filesystem::path current = start;
    while (!current.empty()) {
        if (LooksLikeWorkspaceRoot(current)) {
            paths.workspaceRoot = current;
            paths.saveRoot = current / "Saved";
            paths.configRoot = current / "Config";
            return paths;
        }
        const std::filesystem::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    paths.workspaceRoot = start;
    paths.saveRoot = start / "Saved";
    paths.configRoot = start / "Config";
    return paths;
}

} // namespace ri::runtime
