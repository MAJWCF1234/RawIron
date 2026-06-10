#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace ri::logic {

/// Maps LogicKit manifest port names to runtime logic node port names after compile normalization.
[[nodiscard]] std::optional<std::string> MapKitLogicInputToRuntime(std::string_view kitId,
                                                                   std::string_view kitPortName);
[[nodiscard]] std::optional<std::string> MapKitLogicOutputToRuntime(std::string_view kitId,
                                                                    std::string_view kitPortName);

} // namespace ri::logic
