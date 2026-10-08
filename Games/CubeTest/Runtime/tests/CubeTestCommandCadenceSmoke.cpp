#include "RawIron/Games/CubeTest/CubeTestAuthority.h"
#include "RawIron/Core/CommandLine.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool condition,const char* message) {
    if(!condition) throw std::runtime_error(message);
}
class DemoTransport final : public ri::runtime::INetTransport {
public:
    bool connected=true;
    bool failShutdown=false;
    std::vector<ri::runtime::NetPacket> inbox;
    bool StartServer(const ri::runtime::NetEndpoint&,std::size_t) override {return true;}
    bool StartClient() override {return false;}
    bool Connect(const ri::runtime::NetEndpoint&) override {return false;}
    void Shutdown() override {
        inbox.clear();
        if(failShutdown) throw std::runtime_error("injected transport shutdown failure");
    }
    bool Send(std::size_t,const ri::runtime::NetPacket&) override {return true;}
    std::vector<ri::runtime::NetPacket> PollReceive(std::size_t budget) override {
        const auto count=std::min(budget,inbox.size());
        std::vector<ri::runtime::NetPacket> result;
        result.insert(result.end(),std::make_move_iterator(inbox.begin()),
            std::make_move_iterator(inbox.begin()+count));
        inbox.erase(inbox.begin(),inbox.begin()+count);
        return result;
    }
    std::vector<std::size_t> ConnectedPeers() const override {
        return connected?std::vector<std::size_t>{7}:std::vector<std::size_t>{};
    }
    ri::runtime::NetTransportStats Stats() const noexcept override {return {};}
};
}
int main() {
    try {
        using namespace ri::games::cubetest;
        auto world=BuildCubeTestWorld("Scheduled authority demo");
        auto bridge=std::make_shared<CubeTestAuthorityBridge>(&world);
        auto transport=std::make_unique<DemoTransport>();
        auto* wire=transport.get();
        ri::runtime::AuthoritativeNetConfig config{};
        config.mode=ri::runtime::NetMode::Dedicated;
        config.tickRate=16;config.serverTickRate=64;
        config.simulationBridge=bridge;
        config.rendezvousProvider=ri::runtime::RendezvousProviderKind::DirectToken;
        ri::runtime::AuthoritativeNetModule module(config,std::move(transport));
        ri::runtime::RuntimeContext context({},{});
        char program[]="scheduled-demo";char* argv[]={program};
        ri::core::CommandLine args(1,argv);
        Require(module.OnRuntimeStartup(context,args),"authority startup");
        int index=0;double time=0;
        auto frame=[&](double dt) {
            if(std::isfinite(dt))time+=std::max(0.0,dt);
            ri::core::FrameContext step{};
            step.frameIndex=index++;step.deltaSeconds=dt;step.realtimeSeconds=time;
            Require(module.OnRuntimeFrame(context,step),"authority frame");
        };
        const auto active=[&] {return std::count_if(world.projectileProps.begin(),world.projectileProps.end(),
            [](const auto& prop){return prop.active;});};
        const auto initial=active();
        auto command=CubeTestAuthorityBridge::BuildProjectileCommand({125.2f,1.4f,0},{1,0,0});
        auto queue=[&](int count) {for(int i=0;i<count;++i)
            wire->inbox.push_back({.peerId=7,.channel=0,.reliable=true,.payload=command});};
        queue(5);frame(1.0/32);
        Require(active()==initial && module.ServerStats().pendingCommands==5,
            "requests mutated demo before command boundary");
        Require(module.ServerStats().snapshotsBroadcast==2,"snapshot cadence stalled waiting for commands");
        frame(1.0/32);
        Require(active()==initial+4 && module.ServerStats().commandsDispatched==5,
            "command boundary did not apply four valid demo projectiles and reject fifth");
        queue(1);frame(1.0/32);
        Require(active()==initial+4,"snapshot tick replenished command budget early");
        frame(1.0/32);Require(active()==initial+5,"next command boundary did not replenish budget");
        queue(1);frame(0);
        wire->connected=false;frame(1.0/16);
        Require(active()==initial+5 && module.ServerStats().pendingCommands==0
            && module.ServerStats().staleCommandDrops==1,"disconnected peer retained actionable command");
        wire->connected=true;queue(64);frame(0);
        Require(module.ServerStats().pendingCommands==16 && module.ServerStats().commandQueueDrops==48,
            "per-peer command queue is unbounded");
        frame(1.0/16);Require(active()==initial+9,"bounded burst bypassed domain command budget");
        auto tickBefore=module.ServerStats().commandTicks;
        frame(100);Require(module.ServerStats().commandTicks-tickBefore<=4,"hitch caused unbounded command catch-up");
        tickBefore=module.ServerStats().commandTicks;
        frame(std::numeric_limits<double>::quiet_NaN());frame(-1);
        Require(module.ServerStats().commandTicks==tickBefore,"invalid delta advanced command clock");
        const auto snapshotsBefore=module.ServerStats().snapshotsBroadcast;
        for(int i=0;i<64;++i)frame(1.0/64);
        Require(module.ServerStats().commandTicks-tickBefore==16
            && module.ServerStats().snapshotsBroadcast-snapshotsBefore==64,
            "command cadence and snapshot cadence are coupled");
        const auto dropsBefore=module.ServerStats().commandQueueDrops;
        wire->inbox.push_back({.peerId=7,.channel=0,
            .payload=std::vector<std::uint8_t>(1024*1024,1)});
        frame(0);queue(1);frame(0);
        Require(module.ServerStats().pendingCommandBytes==1024*1024
            && module.ServerStats().commandQueueDrops==dropsBefore+1,
            "command byte budget did not reject an overflowing request");
        frame(1.0/16);
        Require(active()==initial+9,"malformed queued request mutated the demo");
        queue(1);frame(0);
        Require(module.ServerStats().pendingCommands==1,"shutdown test has no pending work");
        module.OnRuntimeShutdown(context);
        Require(module.ServerStats().pendingCommands==0,"shutdown retained queued commands");

        // Reuse one module across sessions with a packet still in the older delay queue.
        auto restartWire=std::make_unique<DemoTransport>();
        auto* delayedWire=restartWire.get();
        config.latencySimulation={.baseDelayMs=500,.enabled=true};
        ri::runtime::AuthoritativeNetModule restarted(config,std::move(restartWire));
        ri::runtime::RuntimeContext firstSession({},{}), secondSession({},{});
        Require(restarted.OnRuntimeStartup(firstSession,args),"first delayed session startup");
        const auto beforeRestart=active();
        delayedWire->inbox.push_back({.peerId=7,.channel=0,.payload=command});
        ri::core::FrameContext delayedFrame{};
        delayedFrame.deltaSeconds=1.0/128;
        Require(restarted.OnRuntimeFrame(firstSession,delayedFrame),"queue delayed command");
        Require(active()==beforeRestart,"delayed command ran early");
        restarted.OnRuntimeShutdown(firstSession);
        Require(!restarted.SendPacket(7,{.payload=command},ri::runtime::NetChannelKind::Authority),
            "stopped module still routes packets");
        Require(restarted.OnRuntimeStartup(secondSession,args),"second delayed session startup");
        delayedFrame.deltaSeconds=1.0/128;delayedFrame.realtimeSeconds=1;
        Require(restarted.OnRuntimeFrame(secondSession,delayedFrame),"second delayed session frame");
        Require(restarted.ServerStats().snapshotsBroadcast==0,
            "new session inherited snapshot cadence remainder");
        delayedFrame.deltaSeconds=1.0/16;
        Require(restarted.OnRuntimeFrame(secondSession,delayedFrame),"second session command boundary");
        Require(active()==beforeRestart,"old delayed command leaked into the new demo session");
        Require(restarted.ServerStats().snapshotsBroadcast==4,
            "new session inherited snapshot cadence remainder");
        delayedWire->failShutdown=true;
        bool shutdownFailed=false;
        try {restarted.OnRuntimeShutdown(secondSession);}
        catch(const std::runtime_error&) {shutdownFailed=true;}
        Require(shutdownFailed && restarted.Config().role==ri::runtime::NetRole::None
            && !restarted.SendPacket(7,{.payload=command},ri::runtime::NetChannelKind::Authority),
            "cleanup exception left a live network route");
        std::cout<<"Cube Test authority: 16 Hz commands / 64 Hz snapshots, queued projectile emission, bursts, disconnect and hitches passed\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
