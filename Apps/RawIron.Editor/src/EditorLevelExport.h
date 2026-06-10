#pragma once

#include "RawIron/Scene/Scene.h"

#include <cstddef>
#include <filesystem>
#include <string>

namespace ri::editor {

struct LevelExportResult {
    bool success = false;
    std::size_t rowCount = 0;
    std::string error;
};

struct LevelImportResult {
    bool success = false;
    std::size_t importedCount = 0;
    std::size_t skippedCount = 0;
    std::string error;
};

[[nodiscard]] LevelExportResult TryExportAssemblyLightingCsv(const ri::scene::Scene& scene,
                                                             const std::filesystem::path& outputPath);

[[nodiscard]] LevelExportResult TryExportAssemblyCollidersCsv(const ri::scene::Scene& scene,
                                                              const std::filesystem::path& outputPath);

[[nodiscard]] LevelImportResult TryImportAssemblyLightingCsv(ri::scene::Scene& scene,
                                                             int worldRootNodeHandle,
                                                             const std::filesystem::path& inputPath);

[[nodiscard]] LevelImportResult TryImportAssemblyCollidersCsv(ri::scene::Scene& scene,
                                                              int worldRootNodeHandle,
                                                              const std::filesystem::path& inputPath);

[[nodiscard]] LevelExportResult TryExportAssemblyTriggersCsv(const ri::scene::Scene& scene,
                                                             const std::filesystem::path& outputPath);

[[nodiscard]] LevelImportResult TryImportAssemblyTriggersCsv(ri::scene::Scene& scene,
                                                             int worldRootNodeHandle,
                                                             const std::filesystem::path& inputPath);

} // namespace ri::editor
