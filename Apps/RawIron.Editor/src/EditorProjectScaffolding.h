#pragma once

#include "RawIron/Content/GameManifest.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ri::editor {

bool EnsureMountedGameScaffold(const ri::content::GameManifest& manifest,
                               std::size_t& createdCount,
                               std::vector<std::string>& createdFiles,
                               std::string* error = nullptr);

} // namespace ri::editor
