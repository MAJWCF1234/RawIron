#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace ri::core {

// Authoring-time labels and item presentation derived from legacy prototype data.
// Headless-safe: no graphics, only string resolution for UI, checkpoints, and tools.

struct ItemPresentation {
    std::string id;
    std::string name;
    std::string texture;
    std::string type;
};

struct ItemPresentationOverrides {
    std::optional<std::string> name;
    std::optional<std::string> texture;
    std::optional<std::string> type;
};

[[nodiscard]] std::string ResolveKeyLabel(std::string_view keyId);

[[nodiscard]] std::optional<ItemPresentation> ResolveItemPresentation(
    std::string_view itemId,
    const ItemPresentationOverrides& overrides = {});

} // namespace ri::core
