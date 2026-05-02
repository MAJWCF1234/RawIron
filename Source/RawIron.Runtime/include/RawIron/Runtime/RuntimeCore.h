#pragma once

#include "RawIron/Core/Host.h"
#include "RawIron/Runtime/RuntimeEventBus.h"

#include <any>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace ri::core {
class CommandLine;
}

namespace ri::runtime {

enum class RuntimePhase {
    Uninitialized,
    Starting,
    Loading,
    Running,
    Paused,
    Stopping,
    Stopped,
    Failed,
};

struct RuntimeIdentity {
    std::string id = "rawiron-runtime";
    std::string displayName = "RawIron Runtime";
    std::string mode = "runtime";
    std::string instanceId;
};

struct RuntimePaths {
    std::filesystem::path workspaceRoot;
    std::filesystem::path gameRoot;
    std::filesystem::path saveRoot;
    std::filesystem::path configRoot;
};

struct RuntimeFrameSnapshot {
    int frameIndex = 0;
    double deltaSeconds = 0.0;
    double elapsedSeconds = 0.0;
    double realtimeSeconds = 0.0;
    double realDeltaSeconds = 0.0;
};

class RuntimeServices {
public:
    template <class T>
    bool Register(std::shared_ptr<T> service) {
        if (service == nullptr) {
            return false;
        }
        services_[std::type_index(typeid(T))] = std::move(service);
        return true;
    }

    template <class T>
    [[nodiscard]] std::shared_ptr<T> Resolve() const {
        const auto found = services_.find(std::type_index(typeid(T)));
        if (found == services_.end()) {
            return {};
        }
        return std::any_cast<std::shared_ptr<T>>(found->second);
    }

    template <class T>
    [[nodiscard]] bool Contains() const {
        return services_.find(std::type_index(typeid(T))) != services_.end();
    }

    template <class T>
    bool Unregister() {
        return services_.erase(std::type_index(typeid(T))) > 0U;
    }

    void Clear();
    [[nodiscard]] std::size_t Count() const noexcept;

private:
    std::unordered_map<std::type_index, std::any> services_;
};

class RuntimeContext {
public:
    RuntimeContext(RuntimeIdentity identity, RuntimePaths paths);

    [[nodiscard]] const RuntimeIdentity& Identity() const noexcept;
    [[nodiscard]] const RuntimePaths& Paths() const noexcept;
    [[nodiscard]] RuntimePhase Phase() const noexcept;
    [[nodiscard]] RuntimeEventBus& Events() noexcept;
    [[nodiscard]] const RuntimeEventBus& Events() const noexcept;
    [[nodiscard]] RuntimeServices& Services() noexcept;
    [[nodiscard]] const RuntimeServices& Services() const noexcept;
    [[nodiscard]] RuntimeFrameSnapshot Frame() const noexcept;
    [[nodiscard]] bool StopRequested() const noexcept;
    [[nodiscard]] std::string_view StopReason() const noexcept;
    [[nodiscard]] std::string_view FailureReason() const noexcept;

    void RequestStop(std::string reason = {});
    void Fail(std::string reason);

private:
    friend class RuntimeCore;

    void SetPhase(RuntimePhase phase);
    void SetFrame(RuntimeFrameSnapshot frame);
    void ClearRunState();

    RuntimeIdentity identity_;
    RuntimePaths paths_;
    RuntimePhase phase_ = RuntimePhase::Uninitialized;
    RuntimeEventBus events_;
    RuntimeServices services_;
    RuntimeFrameSnapshot frame_;
    bool stopRequested_ = false;
    std::string stopReason_;
    std::string failureReason_;
};

class RuntimeModule {
public:
    virtual ~RuntimeModule() = default;

    [[nodiscard]] virtual std::string_view Name() const noexcept = 0;
    virtual bool OnRuntimeStartup(RuntimeContext& context, const ri::core::CommandLine& commandLine);
    virtual bool OnRuntimeFrame(RuntimeContext& context, const ri::core::FrameContext& frame);
    virtual void OnRuntimePause(RuntimeContext& context);
    virtual void OnRuntimeResume(RuntimeContext& context);
    virtual void OnRuntimeShutdown(RuntimeContext& context);
};

class RuntimeCore {
public:
    RuntimeCore(RuntimeIdentity identity, RuntimePaths paths = {});

    [[nodiscard]] RuntimeContext& Context() noexcept;
    [[nodiscard]] const RuntimeContext& Context() const noexcept;

    void AddModule(std::unique_ptr<RuntimeModule> module);
    [[nodiscard]] bool TryAddModule(std::unique_ptr<RuntimeModule> module);
    [[nodiscard]] bool HasModule(std::string_view moduleName) const;
    [[nodiscard]] std::vector<std::string> ModuleNames() const;
    [[nodiscard]] std::size_t ModuleCount() const noexcept;

    bool Startup(const ri::core::CommandLine& commandLine);
    bool Frame(const ri::core::FrameContext& frame);
    bool Pause(std::string reason = {});
    bool Resume();
    void Shutdown();

private:
    void EmitPhaseChanged(RuntimePhase from, RuntimePhase to);

    RuntimeContext context_;
    std::vector<std::unique_ptr<RuntimeModule>> modules_;
};

class RuntimeHostAdapter final : public ri::core::Host {
public:
    explicit RuntimeHostAdapter(RuntimeCore& runtime);

    [[nodiscard]] std::string_view GetName() const noexcept override;
    [[nodiscard]] std::string_view GetMode() const noexcept override;
    void OnStartup(const ri::core::CommandLine& commandLine) override;
    [[nodiscard]] bool OnFrame(const ri::core::FrameContext& frame) override;
    void OnShutdown() override;

private:
    RuntimeCore& runtime_;
    bool startupOk_ = false;
};

class RuntimeHostModule final : public RuntimeModule {
public:
    explicit RuntimeHostModule(ri::core::Host& host);

    [[nodiscard]] std::string_view Name() const noexcept override;
    bool OnRuntimeStartup(RuntimeContext& context, const ri::core::CommandLine& commandLine) override;
    bool OnRuntimeFrame(RuntimeContext& context, const ri::core::FrameContext& frame) override;
    void OnRuntimeShutdown(RuntimeContext& context) override;

private:
    ri::core::Host& host_;
};

[[nodiscard]] const char* RuntimePhaseName(RuntimePhase phase) noexcept;
[[nodiscard]] RuntimePaths DetectRuntimePaths(std::filesystem::path start = std::filesystem::current_path());

} // namespace ri::runtime
