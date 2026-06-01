#pragma once

#include "RawIron/Runtime/RuntimeCore.h"
#include "RawIron/Runtime/RuntimeNetcode.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ri::runtime {

struct BotClientProfile {
    std::string id;
    int strafePeriodTicks = 96;
    int jumpEveryTicks = 84;
    int fireEveryTicks = 30;
    float aimJitter = 0.02f;
};

struct BotSwarmConfig {
    int botCount = 8;
    int commandChannel = 0;
    bool reliableCommands = false;
};

class BotSwarmModule final : public RuntimeModule {
public:
    BotSwarmModule(AuthoritativeNetModule* netcode, BotSwarmConfig config);

    [[nodiscard]] std::string_view Name() const noexcept override;
    bool OnRuntimeStartup(RuntimeContext& context, const ri::core::CommandLine& commandLine) override;
    bool OnRuntimeFrame(RuntimeContext& context, const ri::core::FrameContext& frame) override;

    [[nodiscard]] std::uint64_t CommandsSent() const noexcept;

private:
    void BuildProfiles();
    std::vector<std::uint8_t> BuildCommandPayload(const BotClientProfile& bot, int frameIndex) const;

    AuthoritativeNetModule* netcode_ = nullptr;
    BotSwarmConfig config_{};
    std::vector<BotClientProfile> profiles_{};
    std::uint64_t commandsSent_ = 0;
};

} // namespace ri::runtime

