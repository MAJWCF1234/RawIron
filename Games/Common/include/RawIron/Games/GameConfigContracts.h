#pragma once

#include <filesystem>
#include <string>

namespace ri::games {

enum class GameConfigContractMode {
    Strict,
    Balanced,
    Permissive,
};

struct GameConfigContractOptions {
    GameConfigContractMode mode = GameConfigContractMode::Balanced;
};

[[nodiscard]] bool EnforceGameConfigContracts(const std::filesystem::path& gameRoot,
                                              const GameConfigContractOptions& options,
                                              std::string* error = nullptr);

} // namespace ri::games
