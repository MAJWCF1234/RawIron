#pragma once

#include "RawIron/Content/GameManifest.h"

#include <filesystem>
#include <string>

namespace ri::editor {

enum class NewGameTemplate {
    EmptyStudio,
    OutdoorScene,
    InteriorRoom,
};

struct NewGameCreationResult {
    bool ok = false;
    std::string error;
    ri::content::GameManifest manifest{};
    std::filesystem::path projectRoot;
};

[[nodiscard]] std::string NewGameTemplateLabel(NewGameTemplate templateKind);
[[nodiscard]] std::string DefaultDisplayNameForTemplate(NewGameTemplate templateKind);
[[nodiscard]] std::string SlugFromDisplayName(std::string_view displayName);
[[nodiscard]] std::string BuildNewGameManifestJson(std::string_view projectId,
                                                   std::string_view projectName,
                                                   std::string_view description);
[[nodiscard]] std::string TemplateLevelPrimitivesCsv(NewGameTemplate templateKind);
[[nodiscard]] NewGameCreationResult CreateNewGameProject(const std::filesystem::path& workspaceRoot,
                                                           NewGameTemplate templateKind,
                                                           std::string_view displayName,
                                                           std::string_view author = "RawIron Editor");

} // namespace ri::editor
