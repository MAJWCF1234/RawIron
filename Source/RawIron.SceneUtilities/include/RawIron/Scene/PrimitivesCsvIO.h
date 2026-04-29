#pragma once

#include <filesystem>
#include <string>

namespace ri::scene {

class Scene;

/// Writes primitives in the **LiminalHall assembly** CSV format consumed by
/// `Games/.../levels/assembly.primitives.csv` (cube/plane primitives with material RGB,
/// shading, texture filename, tiling, euler rotation).
///
/// Exports every renderable node whose mesh primitive is Cube or Plane. Skips editor/helper
/// nodes by name substring (Grid, Orbit, Camera, Player, Sun, etc.).
[[nodiscard]] bool TryExportAssemblyPrimitivesCsv(const Scene& scene,
                                                    int worldRootNodeHandle,
                                                    const std::filesystem::path& outputPath,
                                                    std::string* errorMessage);

} // namespace ri::scene
