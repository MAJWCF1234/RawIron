#pragma once

#include "RawIron/Games/CubeTest/CubeTestWorld.h"
#include "RawIron/World/InteractivePropAuthority.h"
#include <filesystem>

namespace ri::games::cubetest {
// Experience binding only: packet validation/serialization lives in RawIron.World.
class CubeTestAuthorityBridge final : public ri::world::InteractivePropAuthorityBridge {
public:
    explicit CubeTestAuthorityBridge(CubeTestWorld* world = nullptr);
    void SetWorld(CubeTestWorld* world);
};
[[nodiscard]] ri::runtime::AuthoritativeNetConfig BuildCubeTestAuthorityConfig(
    const ri::core::CommandLine& commandLine, std::shared_ptr<CubeTestAuthorityBridge> bridge,
    const std::filesystem::path& gameRoot = {});
} // namespace ri::games::cubetest
