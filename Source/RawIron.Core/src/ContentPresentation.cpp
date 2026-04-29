#include "RawIron/Core/ContentPresentation.h"

#include <cctype>
#include <string>
#include <utility>

namespace ri::core {
namespace {

struct KeyLabelRow {
    std::string_view id;
    std::string_view label;
};

struct ItemTableRow {
    std::string_view id;
    std::string_view name;
    std::string_view texture;
    std::string_view type;
};

constexpr KeyLabelRow kAuthoredKeyLabels[] = {
    {.id = "level_b_key", .label = "Level B Keycard"},
    {.id = "level_a_key", .label = "Level A Keycard"},
    {.id = "level_c_key", .label = "Level C Keycard"},
};

constexpr ItemTableRow kItemPresentation[] = {
    {.id = "flashlight", .name = "Flashlight", .texture = "lite1.png", .type = "tool"},
    {.id = "level_b_key", .name = "Level B Keycard", .texture = "level1keycard.png", .type = "key"},
    {.id = "medkit", .name = "First Aid Kit", .texture = "WARN_1A.png", .type = "consumable"},
    {.id = "level_a_key", .name = "Level A Keycard", .texture = "level-2-keycard.png", .type = "key"},
    {.id = "level_c_key", .name = "Level C Keycard", .texture = "level-3-keycard.png", .type = "key"},
};

const KeyLabelRow* FindKeyLabelRow(std::string_view keyId) {
    for (const KeyLabelRow& row : kAuthoredKeyLabels) {
        if (row.id == keyId) {
            return &row;
        }
    }
    return nullptr;
}

const ItemTableRow* FindItemRow(std::string_view itemId) {
    for (const ItemTableRow& row : kItemPresentation) {
        if (row.id == itemId) {
            return &row;
        }
    }
    return nullptr;
}

bool IsBlank(std::string_view text) {
    for (unsigned char ch : text) {
        if (std::isspace(ch) == 0) {
            return false;
        }
    }
    return true;
}

std::string_view TrimWhitespace(std::string_view text) {
    std::size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
        ++start;
    }
    std::size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(start, end - start);
}

std::string TitleCaseSnake(std::string_view id) {
    if (id.empty()) {
        return {};
    }

    std::string out;
    out.reserve(id.size() + 4U);
    bool uppercaseNext = true;
    for (unsigned char uch : id) {
        const char ch = static_cast<char>(uch);
        if (ch == '_' || ch == '-') {
            if (!out.empty() && out.back() != ' ') {
                out.push_back(' ');
            }
            uppercaseNext = true;
            continue;
        }

        if (uppercaseNext) {
            out.push_back(static_cast<char>(std::toupper(uch)));
            uppercaseNext = false;
        } else {
            out.push_back(static_cast<char>(std::tolower(uch)));
        }
    }
    return out;
}

} // namespace

std::string ResolveKeyLabel(std::string_view keyId) {
    keyId = TrimWhitespace(keyId);
    if (keyId.empty()) {
        return {};
    }

    if (const KeyLabelRow* row = FindKeyLabelRow(keyId)) {
        return std::string(row->label);
    }

    return TitleCaseSnake(keyId);
}

std::optional<ItemPresentation> ResolveItemPresentation(std::string_view itemId,
                                                        const ItemPresentationOverrides& overrides) {
    itemId = TrimWhitespace(itemId);
    if (itemId.empty() || IsBlank(itemId)) {
        return std::nullopt;
    }

    const std::string id(itemId);
    ItemPresentation presentation{};

    if (const ItemTableRow* row = FindItemRow(itemId)) {
        presentation.id = id;
        presentation.name = std::string(row->name);
        presentation.texture = std::string(row->texture);
        presentation.type = std::string(row->type);
    } else {
        presentation.id = id;
        presentation.name = ResolveKeyLabel(itemId);
        presentation.texture = "WARN_1A.png";
        presentation.type = "key";
    }

    if (overrides.name.has_value()) {
        presentation.name = *overrides.name;
    }
    if (overrides.texture.has_value()) {
        presentation.texture = *overrides.texture;
    }
    if (overrides.type.has_value()) {
        presentation.type = *overrides.type;
    }

    return presentation;
}

} // namespace ri::core
