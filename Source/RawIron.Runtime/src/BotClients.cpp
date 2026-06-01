#include "RawIron/Runtime/BotClients.h"

#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Log.h"

#include <cmath>

namespace ri::runtime {

BotSwarmModule::BotSwarmModule(AuthoritativeNetModule* netcode, BotSwarmConfig config)
    : netcode_(netcode),
      config_(std::move(config)) {}

std::string_view BotSwarmModule::Name() const noexcept {
    return "rawiron.runtime.bot_swarm";
}

bool BotSwarmModule::OnRuntimeStartup(RuntimeContext&, const ri::core::CommandLine&) {
    BuildProfiles();
    ri::core::LogInfo("Bot swarm startup: " + std::to_string(profiles_.size()) + " bot clients.");
    return true;
}

bool BotSwarmModule::OnRuntimeFrame(RuntimeContext& context, const ri::core::FrameContext& frame) {
    if (netcode_ == nullptr) {
        return true;
    }
    for (const BotClientProfile& bot : profiles_) {
        NetPacket packet{};
        packet.channel = static_cast<std::uint32_t>(config_.commandChannel);
        packet.reliable = config_.reliableCommands;
        packet.payload = BuildCommandPayload(bot, frame.frameIndex);
        if (netcode_->SendPacket(0, std::move(packet), NetChannelKind::Authority)) {
            ++commandsSent_;
        }
    }

    if (frame.frameIndex % 60 == 0) {
        RuntimeEvent ev{};
        ev.fields["commands_sent"] = std::to_string(commandsSent_);
        ev.fields["bots"] = std::to_string(profiles_.size());
        context.Events().Emit("bot.metrics", std::move(ev));
    }
    return true;
}

std::uint64_t BotSwarmModule::CommandsSent() const noexcept {
    return commandsSent_;
}

void BotSwarmModule::BuildProfiles() {
    profiles_.clear();
    for (int i = 0; i < std::max(1, config_.botCount); ++i) {
        BotClientProfile p{};
        p.id = "bot_" + std::to_string(i);
        p.strafePeriodTicks = 72 + (i % 5) * 11;
        p.jumpEveryTicks = 70 + (i % 7) * 9;
        p.fireEveryTicks = 20 + (i % 6) * 8;
        p.aimJitter = 0.01f * static_cast<float>((i % 5) + 1);
        profiles_.push_back(std::move(p));
    }
}

std::vector<std::uint8_t> BotSwarmModule::BuildCommandPayload(const BotClientProfile& bot, const int frameIndex) const {
    const float phase = static_cast<float>(frameIndex % bot.strafePeriodTicks) / static_cast<float>(bot.strafePeriodTicks);
    const float strafe = std::sin(phase * 6.2831853f);
    const bool jump = bot.jumpEveryTicks > 0 && (frameIndex % bot.jumpEveryTicks) == 0;
    const bool fire = bot.fireEveryTicks > 0 && (frameIndex % bot.fireEveryTicks) == 0;

    // Compact binary payload for stress testing:
    // [u8 idLen][id bytes...][f32 strafe][u8 jump][u8 fire][f32 jitter]
    std::vector<std::uint8_t> bytes;
    const std::uint8_t idLen = static_cast<std::uint8_t>(std::min<std::size_t>(255, bot.id.size()));
    bytes.push_back(idLen);
    bytes.insert(bytes.end(), bot.id.begin(), bot.id.begin() + static_cast<std::ptrdiff_t>(idLen));

    auto appendFloat = [&bytes](const float value) {
        const std::uint8_t* raw = reinterpret_cast<const std::uint8_t*>(&value);
        bytes.insert(bytes.end(), raw, raw + sizeof(float));
    };
    appendFloat(strafe);
    bytes.push_back(jump ? 1U : 0U);
    bytes.push_back(fire ? 1U : 0U);
    appendFloat(bot.aimJitter);
    return bytes;
}

} // namespace ri::runtime

