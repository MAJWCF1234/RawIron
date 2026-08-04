#include "RawIron/Runtime/RuntimeCore.h"

#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Runtime/HostInputService.h"
#include "RawIron/Runtime/LevelSchedulerRuntimeModule.h"
#include "RawIron/Runtime/RuntimeId.h"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

namespace ri::runtime {
namespace {

class ScopedLifecycleOperation final {
public:
    explicit ScopedLifecycleOperation(bool& flag) noexcept : flag_(flag) {
        flag_ = true;
    }

    ~ScopedLifecycleOperation() {
        flag_ = false;
    }

    ScopedLifecycleOperation(const ScopedLifecycleOperation&) = delete;
    ScopedLifecycleOperation& operator=(const ScopedLifecycleOperation&) = delete;

private:
    bool& flag_;
};

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
    failureReason_ = reason.empty()
        ? "Runtime failure requested without a diagnostic."
        : std::move(reason);
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

RuntimeCore::~RuntimeCore() noexcept {
    if (context_.phase_ == RuntimePhase::Uninitialized || context_.phase_ == RuntimePhase::Stopped) {
        return;
    }
    try {
        Shutdown();
    } catch (...) {
        // Module and event exceptions are isolated by Shutdown. This final boundary exists for
        // allocation/system failures: destructors must never leak an exception into stack unwinding.
        ri::core::LogInfo("Runtime: unexpected exception during destructor shutdown.");
    }
}

RuntimeCore::RuntimeCore(RuntimeCore&& other)
    : context_(RuntimeIdentity{}, RuntimePaths{}) {
    if (!other.CanMove()) {
        throw std::logic_error("RuntimeCore cannot be moved while active or inside a lifecycle callback.");
    }
    context_ = std::move(other.context_);
    modules_ = std::move(other.modules_);
    activeModules_ = std::move(other.activeModules_);
    serviceBaseline_ = std::move(other.serviceBaseline_);
    serviceBaselineCaptured_ = other.serviceBaselineCaptured_;
    lifecycleOperationInProgress_ = false;

    other.activeModules_.clear();
    other.serviceBaseline_.Clear();
    other.serviceBaselineCaptured_ = false;
    other.context_.SetPhase(RuntimePhase::Stopped);
    other.context_.ClearRunState();
    other.lifecycleOperationInProgress_ = false;
}

RuntimeCore& RuntimeCore::operator=(RuntimeCore&& other) {
    if (this == &other) {
        return *this;
    }
    if (!CanMove() || !other.CanMove()) {
        throw std::logic_error("RuntimeCore cannot be move-assigned while active or inside a lifecycle callback.");
    }
    context_ = std::move(other.context_);
    modules_ = std::move(other.modules_);
    activeModules_ = std::move(other.activeModules_);
    serviceBaseline_ = std::move(other.serviceBaseline_);
    serviceBaselineCaptured_ = other.serviceBaselineCaptured_;
    lifecycleOperationInProgress_ = false;

    other.activeModules_.clear();
    other.serviceBaseline_.Clear();
    other.serviceBaselineCaptured_ = false;
    other.context_.SetPhase(RuntimePhase::Stopped);
    other.context_.ClearRunState();
    other.lifecycleOperationInProgress_ = false;
    return *this;
}

void RuntimeCore::AddDefaultModules() {
    (void)TryAddModule(std::make_unique<LevelSchedulerRuntimeModule>());
    // Standalone hosts Bind windows and query keys; headless stays inert with null handles.
    (void)TryAddModule(MakeHostInputRuntimeModule());
}

RuntimeContext& RuntimeCore::Context() noexcept {
    return context_;
}

const RuntimeContext& RuntimeCore::Context() const noexcept {
    return context_;
}

void RuntimeCore::AddModule(std::unique_ptr<RuntimeModule> module) {
    if (!TryAddModule(std::move(module))) {
        ri::core::LogInfo(
            "Runtime module registration failed "
            "(invalid lifecycle phase, null/unnamed module, or duplicate name).");
    }
}

bool RuntimeCore::TryAddModule(std::unique_ptr<RuntimeModule> module) {
    if (lifecycleOperationInProgress_
        || (context_.phase_ != RuntimePhase::Uninitialized && context_.phase_ != RuntimePhase::Stopped)) {
        return false;
    }
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
    if (lifecycleOperationInProgress_) {
        ri::core::LogInfo("Runtime startup rejected during another lifecycle operation.");
        return false;
    }
    ScopedLifecycleOperation operation(lifecycleOperationInProgress_);
    if (context_.phase_ != RuntimePhase::Uninitialized && context_.phase_ != RuntimePhase::Stopped) {
        context_.Fail("Runtime startup requested from invalid phase.");
        EmitPhaseChanged(context_.phase_, RuntimePhase::Failed);
        return false;
    }

    context_.ClearRunState();
    CaptureServiceBaseline();
    EmitPhaseChanged(context_.phase_, RuntimePhase::Starting);
    if (AbortStartupIfRequested("starting transition")) {
        return false;
    }
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
    activeModules_.clear();
    activeModules_.reserve(modules_.size());
    if (AbortStartupIfRequested("loading transition")) {
        return false;
    }

    for (const std::unique_ptr<RuntimeModule>& module : modules_) {
        if (module == nullptr) {
            continue;
        }
        bool startupOk = false;
        try {
            startupOk = module->OnRuntimeStartup(context_, commandLine);
        } catch (const std::exception& ex) {
            context_.Fail("Runtime module startup exception: " + std::string(module->Name()) + ": " + ex.what());
            RollbackStartup(module.get());
            EmitPhaseChanged(context_.phase_, RuntimePhase::Failed);
            return false;
        } catch (...) {
            context_.Fail("Runtime module startup exception: " + std::string(module->Name()) + ": unknown");
            RollbackStartup(module.get());
            EmitPhaseChanged(context_.phase_, RuntimePhase::Failed);
            return false;
        }
        if (!startupOk) {
            if (context_.FailureReason().empty()) {
                std::string reason = "Runtime module startup failed: " + std::string(module->Name());
                if (!context_.StopReason().empty()) {
                    reason += ": " + std::string(context_.StopReason());
                }
                context_.Fail(std::move(reason));
            }
            RollbackStartup(module.get());
            EmitPhaseChanged(context_.phase_, RuntimePhase::Failed);
            return false;
        }
        if (AbortStartupIfRequested("module startup: " + std::string(module->Name()), module.get())) {
            return false;
        }
        activeModules_.push_back(module.get());
    }

    EmitPhaseChanged(RuntimePhase::Loading, RuntimePhase::Running);
    if (AbortStartupIfRequested("running transition")) {
        return false;
    }
    EmitEvent("runtime.started", RuntimeEvent{
        .id = {},
        .type = {},
        .fields = {
            {"id", context_.identity_.id},
            {"mode", context_.identity_.mode},
            {"instance", context_.identity_.instanceId},
            {"modules", std::to_string(ModuleCount())},
        },
    });
    if (AbortStartupIfRequested("runtime.started dispatch")) {
        return false;
    }
    return true;
}

bool RuntimeCore::Frame(const ri::core::FrameContext& frame) {
    if (lifecycleOperationInProgress_) {
        return false;
    }
    ScopedLifecycleOperation operation(lifecycleOperationInProgress_);
    if (context_.phase_ == RuntimePhase::Failed || context_.phase_ == RuntimePhase::Stopped) {
        return false;
    }
    if (context_.phase_ == RuntimePhase::Paused) {
        const bool shouldContinue = ContinueAfterCallback();
        if (!shouldContinue) {
            EmitStopRequestedEvent();
        }
        return shouldContinue;
    }
    if (context_.phase_ != RuntimePhase::Running) {
        context_.Fail("Runtime frame requested before running phase.");
        EmitPhaseChanged(context_.phase_, RuntimePhase::Failed);
        return false;
    }
    if (!ContinueAfterCallback()) {
        EmitStopRequestedEvent();
        return false;
    }

    context_.SetFrame(RuntimeFrameSnapshot{
        .frameIndex = frame.frameIndex,
        .deltaSeconds = frame.deltaSeconds,
        .elapsedSeconds = frame.elapsedSeconds,
        .realtimeSeconds = frame.realtimeSeconds,
        .realDeltaSeconds = frame.realDeltaSeconds,
    });
    EmitEvent("runtime.frame", RuntimeEvent{
        .id = {},
        .type = {},
        .fields = {
            {"index", std::to_string(frame.frameIndex)},
            {"elapsed", std::to_string(frame.elapsedSeconds)},
            {"delta", std::to_string(frame.deltaSeconds)},
        },
    });

    if (ContinueAfterCallback()) {
        for (RuntimeModule* const module : activeModules_) {
            if (module == nullptr) {
                continue;
            }
            bool keepRunning = false;
            try {
                keepRunning = module->OnRuntimeFrame(context_, frame);
            } catch (const std::exception& ex) {
                context_.Fail(
                    "Runtime module frame exception: " + std::string(module->Name()) + ": " + ex.what());
                EmitPhaseChanged(context_.phase_, RuntimePhase::Failed);
                break;
            } catch (...) {
                context_.Fail("Runtime module frame exception: " + std::string(module->Name()) + ": unknown");
                EmitPhaseChanged(context_.phase_, RuntimePhase::Failed);
                break;
            }
            if (!keepRunning) {
                context_.RequestStop("Runtime module requested stop: " + std::string(module->Name()));
                break;
            }
            if (!ContinueAfterCallback()) {
                break;
            }
        }
    }

    (void)ContinueAfterCallback();
    EmitStopRequestedEvent();
    return !context_.StopRequested();
}

bool RuntimeCore::Pause(std::string reason) {
    if (lifecycleOperationInProgress_) {
        return false;
    }
    ScopedLifecycleOperation operation(lifecycleOperationInProgress_);
    if (context_.phase_ != RuntimePhase::Running) {
        return false;
    }
    if (!ContinueAfterCallback()) {
        return false;
    }
    for (RuntimeModule* const module : activeModules_) {
        if (module != nullptr) {
            try {
                module->OnRuntimePause(context_);
            } catch (const std::exception& ex) {
                context_.Fail("Runtime module pause exception: " + std::string(module->Name()) + ": " + ex.what());
                EmitPhaseChanged(context_.phase_, RuntimePhase::Failed);
                return false;
            } catch (...) {
                context_.Fail("Runtime module pause exception: " + std::string(module->Name()) + ": unknown");
                EmitPhaseChanged(context_.phase_, RuntimePhase::Failed);
                return false;
            }
            if (!ContinueAfterCallback()) {
                return false;
            }
        }
    }
    EmitPhaseChanged(RuntimePhase::Running, RuntimePhase::Paused);
    if (!ContinueAfterCallback()) {
        return false;
    }
    EmitEvent("runtime.paused", RuntimeEvent{
        .id = {},
        .type = {},
        .fields = {
            {"id", context_.identity_.id},
            {"reason", reason},
        },
    });
    return ContinueAfterCallback();
}

bool RuntimeCore::Resume() {
    if (lifecycleOperationInProgress_) {
        return false;
    }
    ScopedLifecycleOperation operation(lifecycleOperationInProgress_);
    if (context_.phase_ != RuntimePhase::Paused) {
        return false;
    }
    if (!ContinueAfterCallback()) {
        return false;
    }
    context_.ClearRunState();
    for (RuntimeModule* const module : activeModules_) {
        if (module != nullptr) {
            try {
                module->OnRuntimeResume(context_);
            } catch (const std::exception& ex) {
                context_.Fail("Runtime module resume exception: " + std::string(module->Name()) + ": " + ex.what());
                EmitPhaseChanged(context_.phase_, RuntimePhase::Failed);
                return false;
            } catch (...) {
                context_.Fail("Runtime module resume exception: " + std::string(module->Name()) + ": unknown");
                EmitPhaseChanged(context_.phase_, RuntimePhase::Failed);
                return false;
            }
            if (!ContinueAfterCallback()) {
                return false;
            }
        }
    }
    EmitPhaseChanged(RuntimePhase::Paused, RuntimePhase::Running);
    if (!ContinueAfterCallback()) {
        return false;
    }
    EmitEvent("runtime.resumed", RuntimeEvent{
        .id = {},
        .type = {},
        .fields = {
            {"id", context_.identity_.id},
        },
    });
    return ContinueAfterCallback();
}

void RuntimeCore::Shutdown() {
    if (lifecycleOperationInProgress_) {
        return;
    }
    ScopedLifecycleOperation operation(lifecycleOperationInProgress_);
    if (context_.phase_ == RuntimePhase::Stopped || context_.phase_ == RuntimePhase::Uninitialized) {
        EmitPhaseChanged(context_.phase_, RuntimePhase::Stopped);
        return;
    }

    const RuntimePhase previous = context_.phase_;
    EmitPhaseChanged(previous, RuntimePhase::Stopping);
    while (!activeModules_.empty()) {
        RuntimeModule* const module = activeModules_.back();
        activeModules_.pop_back();
        if (module != nullptr) {
            std::string cleanupFailure = ShutdownModule(*module, "shutdown");
            if (!cleanupFailure.empty()) {
                RecordCleanupFailure(std::move(cleanupFailure));
            }
        }
    }
    RestoreServiceBaseline();
    EmitEvent("runtime.stopped", RuntimeEvent{
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

bool RuntimeCore::AbortStartupIfRequested(const std::string_view stage,
                                          RuntimeModule* const attemptedModule) {
    if (!context_.StopRequested()) {
        return false;
    }
    const bool failedBeforeCleanup = !context_.FailureReason().empty();
    if (!failedBeforeCleanup) {
        EmitPhaseChanged(context_.phase_, RuntimePhase::Stopping);
    }
    RollbackStartup(attemptedModule);
    if (!context_.FailureReason().empty()) {
        EmitPhaseChanged(context_.phase_, RuntimePhase::Failed);
    } else {
        EmitEvent("runtime.stopped", RuntimeEvent{
            .id = {},
            .type = {},
            .fields = {
                {"id", context_.identity_.id},
                {"mode", context_.identity_.mode},
                {"instance", context_.identity_.instanceId},
                {"stopReason", context_.stopReason_},
                {"failureReason", {}},
                {"startupStage", std::string(stage)},
            },
        });
        EmitPhaseChanged(RuntimePhase::Stopping, RuntimePhase::Stopped);
    }
    return true;
}

bool RuntimeCore::ContinueAfterCallback() {
    if (!context_.StopRequested()) {
        return true;
    }
    if (!context_.FailureReason().empty() && context_.phase_ != RuntimePhase::Failed) {
        EmitPhaseChanged(context_.phase_, RuntimePhase::Failed);
    }
    return false;
}

bool RuntimeCore::CanMove() const noexcept {
    const bool inactivePhase = context_.phase_ == RuntimePhase::Uninitialized
        || context_.phase_ == RuntimePhase::Stopped;
    return inactivePhase
        && !lifecycleOperationInProgress_
        && activeModules_.empty()
        && !serviceBaselineCaptured_;
}

std::string RuntimeCore::ShutdownModule(RuntimeModule& module, const std::string_view operation) {
    try {
        module.OnRuntimeShutdown(context_);
        return {};
    } catch (const std::exception& ex) {
        std::string message = "Runtime module " + std::string(operation) + " exception: "
            + std::string(module.Name()) + ": " + ex.what();
        ri::core::LogInfo(message);
        return message;
    } catch (...) {
        std::string message = "Runtime module " + std::string(operation) + " exception: "
            + std::string(module.Name()) + ": unknown";
        ri::core::LogInfo(message);
        return message;
    }
}

void RuntimeCore::RollbackStartup(RuntimeModule* attemptedModule) {
    if (attemptedModule != nullptr) {
        std::string cleanupFailure = ShutdownModule(*attemptedModule, "startup rollback");
        if (!cleanupFailure.empty()) {
            RecordCleanupFailure(std::move(cleanupFailure));
        }
    }
    while (!activeModules_.empty()) {
        RuntimeModule* const module = activeModules_.back();
        activeModules_.pop_back();
        if (module != nullptr) {
            std::string cleanupFailure = ShutdownModule(*module, "startup rollback");
            if (!cleanupFailure.empty()) {
                RecordCleanupFailure(std::move(cleanupFailure));
            }
        }
    }
    RestoreServiceBaseline();
}

void RuntimeCore::CaptureServiceBaseline() {
    serviceBaseline_ = context_.services_;
    serviceBaselineCaptured_ = true;
}

void RuntimeCore::RestoreServiceBaseline() {
    if (!serviceBaselineCaptured_) {
        return;
    }
    context_.services_ = std::move(serviceBaseline_);
    serviceBaseline_.Clear();
    serviceBaselineCaptured_ = false;
}

void RuntimeCore::RecordCleanupFailure(std::string message) {
    if (message.empty()) {
        return;
    }
    if (context_.failureReason_.empty()) {
        context_.failureReason_ = std::move(message);
    } else {
        context_.failureReason_ += " | " + message;
    }
    context_.stopRequested_ = true;
}

void RuntimeCore::EmitEvent(const std::string_view type, RuntimeEvent event) {
    try {
        context_.events_.Emit(type, std::move(event));
    } catch (const std::exception& ex) {
        const std::string message = "Runtime event dispatch exception for " + std::string(type) + ": " + ex.what();
        ri::core::LogInfo(message);
        context_.Fail(message);
    } catch (...) {
        const std::string message = "Runtime event dispatch exception for " + std::string(type) + ": unknown";
        ri::core::LogInfo(message);
        context_.Fail(message);
    }
}

void RuntimeCore::EmitStopRequestedEvent() {
    if (!context_.StopRequested()) {
        return;
    }
    EmitEvent("runtime.stop_requested", RuntimeEvent{
        .id = {},
        .type = {},
        .fields = {
            {"id", context_.identity_.id},
            {"reason", context_.stopReason_.empty() ? context_.failureReason_ : context_.stopReason_},
        },
    });
    (void)ContinueAfterCallback();
}

void RuntimeCore::EmitPhaseChanged(RuntimePhase from, RuntimePhase to) {
    if (from == to && context_.phase_ == to) {
        return;
    }
    context_.SetPhase(to);
    EmitEvent("runtime.phase", RuntimeEvent{
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
