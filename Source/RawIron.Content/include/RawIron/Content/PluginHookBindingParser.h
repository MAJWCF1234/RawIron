#pragma once

#include "RawIron/Content/PluginProjectData.h"

#include <filesystem>
#include <vector>

namespace ri::content {

[[nodiscard]] std::vector<PluginHookBinding> LoadPluginHookBindings(const std::filesystem::path& path);

} // namespace ri::content
