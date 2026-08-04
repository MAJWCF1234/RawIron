#include "RawIron/Runtime/RendezvousProvider.h"

#include "RawIron/Runtime/EosRendezvousProvider.h"

#include "RawIron/Core/Log.h"

#include <sstream>
#include <string_view>

namespace ri::runtime {
namespace {

class NullRendezvousProvider final : public IRendezvousProvider {
public:
    bool Startup() override { return true; }
    void Shutdown() override {}
    [[nodiscard]] std::optional<std::string> IssueJoinCode(const JoinCodeIssueRequest&) override { return std::nullopt; }
    [[nodiscard]] std::optional<JoinCodeResolveResult> ResolveJoinCode(const std::string&) override { return std::nullopt; }
};

class DirectTokenRendezvousProvider final : public IRendezvousProvider {
public:
    bool Startup() override { return true; }
    void Shutdown() override {}

    [[nodiscard]] std::optional<std::string> IssueJoinCode(const JoinCodeIssueRequest& request) override {
        if (request.hostEndpoint.host.empty() || request.hostEndpoint.port == 0) {
            return std::nullopt;
        }
        return EncodeDirectJoinToken(request);
    }

    [[nodiscard]] std::optional<JoinCodeResolveResult> ResolveJoinCode(const std::string& code) override {
        return DecodeDirectJoinToken(code);
    }
};

} // namespace

std::string EncodeDirectJoinToken(const JoinCodeIssueRequest& request) {
    std::ostringstream out;
    out << "RI1:" << request.hostEndpoint.host << ":" << request.hostEndpoint.port << ":"
        << static_cast<int>(request.mode);
    return out.str();
}

std::optional<JoinCodeResolveResult> DecodeDirectJoinToken(const std::string& code) {
    constexpr std::string_view prefix{"RI1:"};
    if (!code.starts_with(prefix)) {
        return std::nullopt;
    }
    const std::string rest = code.substr(prefix.size());
    const std::size_t p1 = rest.find(':');
    const std::size_t p2 = (p1 == std::string::npos) ? std::string::npos : rest.find(':', p1 + 1U);
    if (p1 == std::string::npos || p2 == std::string::npos) {
        return std::nullopt;
    }
    JoinCodeResolveResult result{};
    result.endpoint.host = rest.substr(0, p1);
    if (result.endpoint.host.empty()) {
        return std::nullopt;
    }
    try {
        // Truncating to uint16_t would silently turn port 70000 into 4464.
        // Require the entire port/mode fields to parse — "1extra" must not become mode 1.
        const std::string portText = rest.substr(p1 + 1U, p2 - (p1 + 1U));
        const std::string modeText = rest.substr(p2 + 1U);
        if (portText.empty() || modeText.empty()) {
            return std::nullopt;
        }
        std::size_t portConsumed = 0U;
        const unsigned long rawPort = std::stoul(portText, &portConsumed);
        if (portConsumed != portText.size() || rawPort == 0UL || rawPort > 65535UL) {
            return std::nullopt;
        }
        result.endpoint.port = static_cast<std::uint16_t>(rawPort);

        std::size_t modeConsumed = 0U;
        const unsigned long rawMode = std::stoul(modeText, &modeConsumed);
        if (modeConsumed != modeText.size()) {
            return std::nullopt;
        }
        switch (rawMode) {
        case 0UL: result.mode = NetMode::Dedicated; break;
        case 1UL: result.mode = NetMode::ListenHost; break;
        case 2UL: result.mode = NetMode::HybridP2P; break;
        case 3UL: result.mode = NetMode::ClientOnly; break;
        default: return std::nullopt;
        }
    } catch (...) {
        return std::nullopt;
    }
    return result;
}

std::unique_ptr<IRendezvousProvider> CreateRendezvousProvider(const RendezvousProviderKind kind) {
    switch (kind) {
    case RendezvousProviderKind::None:
        return std::make_unique<NullRendezvousProvider>();
    case RendezvousProviderKind::DirectToken:
        return std::make_unique<DirectTokenRendezvousProvider>();
    case RendezvousProviderKind::EpicOnlineServices:
        return CreateEosRendezvousProvider();
    }
    return std::make_unique<NullRendezvousProvider>();
}

} // namespace ri::runtime
