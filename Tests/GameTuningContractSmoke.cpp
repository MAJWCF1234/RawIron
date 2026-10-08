#include "RawIron/Games/GamePhysicsTuning.h"
#include "RawIron/Games/GameNetworkTuning.h"
#include "RawIron/Games/GameConfigContracts.h"
#include "RawIron/Content/GameScriptBundle.h"

#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
class ContractTransport final : public ri::runtime::INetTransport {
public:
    bool StartServer(const ri::runtime::NetEndpoint&, std::size_t) override { return true; }
    bool StartClient() override { return false; }
    bool Connect(const ri::runtime::NetEndpoint&) override { return false; }
    void Shutdown() override {}
    bool Send(std::size_t, const ri::runtime::NetPacket&) override { return true; }
    std::vector<ri::runtime::NetPacket> PollReceive(std::size_t) override { return {}; }
    std::vector<std::size_t> ConnectedPeers() const override { return {1}; }
    ri::runtime::NetTransportStats Stats() const noexcept override { return {}; }
};
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::uint64_t SnapshotCount(ri::runtime::AuthoritativeNetConfig config,
    const ri::core::CommandLine& commandLine) {
    config.mode=ri::runtime::NetMode::Dedicated;
    config.rendezvousProvider=ri::runtime::RendezvousProviderKind::DirectToken;
    ri::runtime::AuthoritativeNetModule module(config,std::make_unique<ContractTransport>());
    ri::runtime::RuntimeContext context({},{});
    Require(module.OnRuntimeStartup(context,commandLine), "test network startup");
    for (int frame=0; frame<100; ++frame) {
        ri::core::FrameContext tick{};
        tick.frameIndex=frame;
        tick.deltaSeconds=.01;
        tick.elapsedSeconds=(frame+1)*.01;
        Require(module.OnRuntimeFrame(context,tick), "test network tick");
    }
    const auto count=module.ServerStats().snapshotsBroadcast;
    module.OnRuntimeShutdown(context);
    return count;
}
float JumpPeak(const ri::trace::MovementControllerOptions& options) {
    ri::trace::TraceScene floor({{"floor", {{-100,-1,-100},{100,0,100}}}});
    ri::trace::MovementControllerState player{};
    player.body.bounds = {{-.25f,.01f,-.25f},{.25f,1.81f,.25f}};
    player.onGround = true;
    float peak = 0;
    for (int frame=0; frame<360; ++frame) {
        player = ri::trace::SimulateMovementControllerStep(floor, player,
            {.jumpPressed = frame==0}, 1.f/120, options).state;
        peak = std::max(peak, player.body.bounds.min.y);
    }
    return peak;
}
}

int main(int argc, char** argv) {
    try {
        Require(argc==2, "workspace argument required");
        const auto parsed = ri::content::ParseScriptScalarsChecked(
            "# profile\ngravity=20\ngravity=25\nbad=NaN\nmissing_equals\n");
        Require(parsed.issues.size()==3 && parsed.values.at("gravity")==25,
            "duplicate/invalid assignments must be diagnosed");
        Require(parsed.issues[0].message.find("line 3")!=std::string::npos,
            "diagnostics need source line numbers");
        Require(ri::content::ParseScriptScalarsChecked("  # comment\nvalue = 1.25\n").issues.empty(),
            "valid scalar script must parse");
        Require(ri::content::ParseScriptScalarsChecked("\xEF\xBB\xBF# Windows UTF-8\nvalue=1\n").issues.empty(),
            "Windows UTF-8 BOM must not invalidate a profile");
        const ri::content::ScriptScalarMap nonfinite{{"value", std::numeric_limits<float>::quiet_NaN()}};
        Require(ri::content::ScriptScalarOr(nonfinite, "value", 2)==2, "nonfinite getter fallback");
        Require(!ri::content::ValidateScriptScalars(nonfinite,
            {.name="finite", .rules={{.key="value"}}}).ok(), "nonfinite map validation");

        ri::trace::MovementControllerOptions base{};
        base.simulateStamina = false;
        const auto normal = ri::games::ResolveGamePhysicsTuning(base, {});
        const auto heavy = ri::games::ResolveGamePhysicsTuning(base, {{"global_gravity_scale", 2.f}});
        const auto jumping = ri::games::ResolveGamePhysicsTuning(base, {{"global_jump_scale", 1.3f}});
        Require(JumpPeak(heavy)<JumpPeak(normal)*.75f, "gravity scale must reduce simulated jump height");
        Require(JumpPeak(jumping)>JumpPeak(normal)*1.3f, "jump scale must increase simulated jump height");
        const auto steering = ri::games::ResolveGamePhysicsTuning(base,
            {{"global_drag_scale",2.f},{"global_air_control_scale",1.2f}});
        Require(steering.groundFriction==base.groundFriction*2
            && std::abs(steering.airControl-base.airControl*1.2f)<1e-5f, "drag and steering binding");

        char program[]="contract";
        char* defaultsArgv[]={program};
        ri::core::CommandLine defaults(1,defaultsArgv);
        ri::runtime::AuthoritativeNetConfig net{};
        const ri::content::ScriptScalarMap network{{"network_tick_hz",30.f},
            {"network_snapshot_rate",20.f},{"network_max_clients",8.f}};
        ri::games::ApplyGameNetworkTuning(net,network,defaults);
        Require(net.tickRate==30 && net.serverTickRate==20 && net.maxPeers==8, "network script binding");
        const auto scriptSnapshots=SnapshotCount(net,defaults);
        char tick[]="--net-tick=90", snapshots[]="--server-tick=40", peers[]="--max-peers=12";
        char* overrideArgv[]={program,tick,snapshots,peers};
        ri::core::CommandLine overrides(4,overrideArgv);
        ri::games::ApplyGameNetworkTuning(net,network,overrides);
        Require(net.tickRate==90 && net.serverTickRate==40 && net.maxPeers==12, "CLI precedence");
        const auto overrideSnapshots=SnapshotCount(net,overrides);
        Require(scriptSnapshots>=19 && scriptSnapshots<=20
            && overrideSnapshots>=39 && overrideSnapshots<=40,
            "script and CLI cadence must change actual snapshot broadcasts");

        const std::filesystem::path workspace(argv[1]);
        for (const char* game : {"LiminalHall","WildernessRuins","RawIronMultiplayerSandbox","CubeTest"}) {
            const auto root = workspace/"Games"/game;
            std::string error;
            Require(ri::games::EnforceGameConfigContracts(root, {}, &error), error.c_str());
            const auto scripts = ri::content::LoadGameScriptBundle(root, {.logMissing=false});
            const auto actual = ri::games::ResolveGamePhysicsTuning(base, scripts.physics);
            Require(std::isfinite(actual.gravity) && actual.gravity>0, "showcase physics must be usable");
        }
        // Check the public host gate as well as the parser: Balanced must reject
        // an explicitly malformed request, rather than falling back silently.
        const auto temp = std::filesystem::temp_directory_path()/"rawiron-contract-malformed";
        std::filesystem::create_directories(temp/"scripts");
        std::ofstream(temp/"scripts"/"physics.riscript") << "movement_gravity=oops\n";
        std::string error;
        const bool accepted = ri::games::EnforceGameConfigContracts(temp, {}, &error);
        std::filesystem::remove(temp/"scripts"/"physics.riscript");
        std::filesystem::remove(temp/"scripts");
        std::filesystem::remove(temp);
        Require(!accepted && error.find("line 1")!=std::string::npos, "Balanced must reject malformed physics");
        std::cout << "Game tuning contract: parse, simulation, CLI precedence, and four project profiles passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
