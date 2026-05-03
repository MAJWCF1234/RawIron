#pragma once

#include "RawIron/Ui/UiManifest.h"

#include <filesystem>
#include <string>

namespace ri::ui {

[[nodiscard]] bool TryParseUiManifestFromJson(std::string_view jsonText, UiManifest& outManifest, std::string* errorMessage);

[[nodiscard]] bool TryLoadUiManifestFromJsonFile(const std::filesystem::path& path,
                                                 UiManifest& outManifest,
                                                 std::string* errorMessage);

} // namespace ri::ui
