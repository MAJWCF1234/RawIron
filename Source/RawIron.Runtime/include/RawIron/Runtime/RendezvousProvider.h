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
    /// Pump provider backends that need a periodic tick (EOS Platform_Tick).
    virtual void Tick() {}
    [[nodiscard]] virtual std::optional<std::string> IssueJoinCode(const JoinCodeIssueRequest& request) = 0;
    [[nodiscard]] virtual std::optional<JoinCodeResolveResult> ResolveJoinCode(const std::string& code) = 0;
};

[[nodiscard]] std::unique_ptr<IRendezvousProvider> CreateRendezvousProvider(RendezvousProviderKind kind);

/// `RI1:<host>:<port>:<mode>` — a join code that carries the endpoint with no lookup service.
/// The DirectToken provider hands it out verbatim and the EOS provider stores the same string as a
/// lobby attribute, so both must agree on the encoding; keeping one implementation is what stops
/// a code issued by one path from failing to resolve on the other.
[[nodiscard]] std::string EncodeDirectJoinToken(const JoinCodeIssueRequest& request);

/// Returns nullopt for anything that is not a well-formed `RI1:` token, including out-of-range
/// ports and unknown mode ids.
[[nodiscard]] std::optional<JoinCodeResolveResult> DecodeDirectJoinToken(const std::string& code);

} // namespace ri::runtime
