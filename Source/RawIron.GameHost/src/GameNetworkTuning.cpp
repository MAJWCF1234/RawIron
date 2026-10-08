#include "RawIron/Games/GameNetworkTuning.h"
#include "RawIron/Core/Log.h"

#include <algorithm>

namespace ri::games {

void ApplyGameNetworkTuning(ri::runtime::AuthoritativeNetConfig& config,
    const ri::content::ScriptScalarMap& network, const ri::core::CommandLine& commandLine) {
    using ri::content::ScriptScalarOrIntClamped;
    config.tickRate = std::clamp(commandLine.GetIntOr("--net-tick",
        ScriptScalarOrIntClamped(network,"network_tick_hz",config.tickRate,1,240)),1,240);
    config.serverTickRate = std::clamp(commandLine.GetIntOr("--server-tick",
        ScriptScalarOrIntClamped(network, "network_snapshot_rate", config.serverTickRate, 1, 240)), 1, 240);
    config.maxPeers = std::clamp(commandLine.GetIntOr("--max-peers",
        ScriptScalarOrIntClamped(network, "network_max_clients", config.maxPeers, 1, 128)), 1, 128);
    ri::core::LogInfo("Applied network contract: commandHz=" + std::to_string(config.tickRate) + " snapshotHz=" + std::to_string(config.serverTickRate)
        + " maxPeers=" + std::to_string(config.maxPeers));
}

} // namespace ri::games
