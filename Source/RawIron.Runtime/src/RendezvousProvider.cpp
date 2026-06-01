#include "RawIron/Runtime/RendezvousProvider.h"

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
        std::ostringstream out;
        out << "RI1:" << request.hostEndpoint.host << ":" << request.hostEndpoint.port << ":"
            << static_cast<int>(request.mode);
        return out.str();
    }

    [[nodiscard]] std::optional<JoinCodeResolveResult> ResolveJoinCode(const std::string& code) override {
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
        try {
            result.endpoint.port = static_cast<std::uint16_t>(std::stoul(rest.substr(p1 + 1U, p2 - (p1 + 1U))));
            const unsigned long rawMode = std::stoul(rest.substr(p2 + 1U));
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
};

class EosRendezvousProvider final : public IRendezvousProvider {
public:
    bool Startup() override {
#if defined(RAWIRON_HAS_EOS)
        ri::core::LogInfo("EOS rendezvous provider startup requested.");
        return true;
#else
        ri::core::LogInfo("EOS rendezvous unavailable: build without RAWIRON_HAS_EOS.");
        return false;
#endif
    }
    void Shutdown() override {}
    [[nodiscard]] std::optional<std::string> IssueJoinCode(const JoinCodeIssueRequest&) override { return std::nullopt; }
    [[nodiscard]] std::optional<JoinCodeResolveResult> ResolveJoinCode(const std::string&) override { return std::nullopt; }
};

} // namespace

std::unique_ptr<IRendezvousProvider> CreateRendezvousProvider(const RendezvousProviderKind kind) {
    switch (kind) {
    case RendezvousProviderKind::None:
        return std::make_unique<NullRendezvousProvider>();
    case RendezvousProviderKind::DirectToken:
        return std::make_unique<DirectTokenRendezvousProvider>();
    case RendezvousProviderKind::EpicOnlineServices:
        return std::make_unique<EosRendezvousProvider>();
    }
    return std::make_unique<NullRendezvousProvider>();
}

} // namespace ri::runtime
