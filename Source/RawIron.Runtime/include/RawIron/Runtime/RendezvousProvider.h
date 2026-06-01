#pragma once

#include "RawIron/Runtime/NetModes.h"
#include "RawIron/Runtime/NetTransport.h"

#include <memory>
#include <optional>
#include <cstdint>
#include <string>

namespace ri::runtime {

enum class RendezvousProviderKind : std::uint8_t {
    None = 0,
    DirectToken,
    EpicOnlineServices,
};

struct JoinCodeIssueRequest {
    NetEndpoint hostEndpoint{};
    NetMode mode = NetMode::ListenHost;
    int ttlSeconds = 3600;
};

struct JoinCodeResolveResult {
    NetEndpoint endpoint{};
    NetMode mode = NetMode::ListenHost;
};

class IRendezvousProvider {
public:
    virtual ~IRendezvousProvider() = default;

    virtual bool Startup() = 0;
    virtual void Shutdown() = 0;
    [[nodiscard]] virtual std::optional<std::string> IssueJoinCode(const JoinCodeIssueRequest& request) = 0;
    [[nodiscard]] virtual std::optional<JoinCodeResolveResult> ResolveJoinCode(const std::string& code) = 0;
};

[[nodiscard]] std::unique_ptr<IRendezvousProvider> CreateRendezvousProvider(RendezvousProviderKind kind);

} // namespace ri::runtime
