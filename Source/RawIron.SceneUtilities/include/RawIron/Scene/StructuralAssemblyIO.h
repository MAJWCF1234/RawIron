#pragma once

#include "RawIron/Scene/Scene.h"

#include <cstddef>
#include <filesystem>

namespace ri::scene {

struct StructuralAssemblySpawnOptions {
    int parent = kInvalidHandle;
    std::string materialNamePrefix = "struct_asm";
};

struct StructuralAssemblySpawnResult {
    std::size_t spawnedCount = 0;
    std::size_t skippedCount = 0;
};

/// Loads `assembly.structural.csv` rows into custom structural mesh nodes under `options.parent`.
/// Column layout matches `Games/LiminalHall/levels/assembly.structural.csv`.
[[nodiscard]] StructuralAssemblySpawnResult SpawnStructuralAssemblyFromCsv(
    Scene& scene,
    const std::filesystem::path& csvPath,
    const StructuralAssemblySpawnOptions& options);

} // namespace ri::scene
