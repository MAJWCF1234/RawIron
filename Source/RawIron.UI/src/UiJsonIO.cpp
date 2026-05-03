#include "RawIron/Ui/UiJsonIO.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>

namespace ri::ui {

namespace json = ri::core::detail;

namespace {

[[nodiscard]] std::string ReadEntireFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {};
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

[[nodiscard]] bool ParseFloatToken(std::string_view text, std::size_t start, std::size_t* endOut, float* out) {
    while (start < text.size() && (text[start] == ' ' || text[start] == '\t' || text[start] == '\n' || text[start] == '\r')) {
        ++start;
    }
    if (start >= text.size()) {
        return false;
    }
    std::size_t i = start;
    if (i < text.size() && (text[i] == '-' || text[i] == '+')) {
        ++i;
    }
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])) != 0) {
        ++i;
    }
    if (i < text.size() && text[i] == '.') {
        ++i;
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])) != 0) {
            ++i;
        }
    }
    if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
        ++i;
        if (i < text.size() && (text[i] == '-' || text[i] == '+')) {
            ++i;
        }
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])) != 0) {
            ++i;
        }
    }
    if (i == start) {
        return false;
    }
    const std::string token(text.substr(start, i - start));
    char* parseEnd = nullptr;
    const float v = std::strtof(token.c_str(), &parseEnd);
    if (parseEnd == token.c_str()) {
        return false;
    }
    *out = v;
    *endOut = i;
    return true;
}

[[nodiscard]] bool TryParseFloat4AfterKey(std::string_view text, std::string_view key, std::array<float, 4>& out) {
    const std::optional<std::size_t> valueIndex = json::FindJsonKey(text, key);
    if (!valueIndex.has_value()) {
        return false;
    }
    std::size_t cursor = json::SkipWhitespace(text, *valueIndex);
    if (cursor >= text.size() || text[cursor] != '[') {
        return false;
    }
    ++cursor;
    for (int k = 0; k < 4; ++k) {
        cursor = json::SkipWhitespace(text, cursor);
        float component = 0.0f;
        std::size_t end = cursor;
        if (!ParseFloatToken(text, cursor, &end, &component)) {
            return false;
        }
        out[static_cast<std::size_t>(k)] = component;
        cursor = json::SkipWhitespace(text, end);
        if (k < 3) {
            if (cursor >= text.size() || text[cursor] != ',') {
                return false;
            }
            ++cursor;
        }
    }
    cursor = json::SkipWhitespace(text, cursor);
    return cursor < text.size() && text[cursor] == ']';
}

void ParseActionConditions(std::string_view actionText, UiAction& action) {
    const std::optional<std::string_view> when = json::ExtractJsonObject(actionText, "when");
    if (when.has_value()) {
        action.whenVar = json::ExtractJsonString(*when, "var").value_or("");
        action.whenEquals = json::ExtractJsonString(*when, "equals").value_or("");
    }
    const std::optional<std::string_view> setVar = json::ExtractJsonObject(actionText, "setVar");
    if (setVar.has_value()) {
        action.setVarId = json::ExtractJsonString(*setVar, "id").value_or("");
        if (action.setVarId.empty()) {
            action.setVarId = json::ExtractJsonString(*setVar, "name").value_or("");
        }
        action.setVarValue = json::ExtractJsonString(*setVar, "value").value_or("");
    }
}

[[nodiscard]] UiAction ParseActionObject(std::string_view actionText) {
    UiAction action{};
    const std::optional<std::string> type = json::ExtractJsonString(actionText, "type");
    if (!type.has_value()) {
        return action;
    }
    if (*type == "navigate") {
        action.kind = UiActionKind::Navigate;
        action.target = json::ExtractJsonString(actionText, "target").value_or("");
        ParseActionConditions(actionText, action);
        return action;
    }
    if (*type == "emit") {
        action.kind = UiActionKind::Emit;
        action.target = json::ExtractJsonString(actionText, "id").value_or("");
        if (action.target.empty()) {
            action.target = json::ExtractJsonString(actionText, "actionId").value_or("");
        }
        ParseActionConditions(actionText, action);
        return action;
    }
    if (*type == "back") {
        action.kind = UiActionKind::Back;
        ParseActionConditions(actionText, action);
        return action;
    }
    if (*type == "setVar") {
        action.kind = UiActionKind::SetVariable;
        action.target = json::ExtractJsonString(actionText, "id").value_or("");
        if (action.target.empty()) {
            action.target = json::ExtractJsonString(actionText, "name").value_or("");
        }
        action.value = json::ExtractJsonString(actionText, "value").value_or("");
        ParseActionConditions(actionText, action);
        return action;
    }
    return action;
}

[[nodiscard]] std::optional<UiBlockKind> TryParseBlockKind(std::string_view name) {
    if (name == "heading") {
        return UiBlockKind::Heading;
    }
    if (name == "paragraph") {
        return UiBlockKind::Paragraph;
    }
    if (name == "label") {
        return UiBlockKind::Label;
    }
    if (name == "spacer") {
        return UiBlockKind::Spacer;
    }
    if (name == "separator") {
        return UiBlockKind::Separator;
    }
    if (name == "button") {
        return UiBlockKind::Button;
    }
    if (name == "say") {
        return UiBlockKind::Say;
    }
    if (name == "narration") {
        return UiBlockKind::Narration;
    }
    if (name == "choices") {
        return UiBlockKind::Choices;
    }
    if (name == "image") {
        return UiBlockKind::Image;
    }
    if (name == "historyNote") {
        return UiBlockKind::HistoryNote;
    }
    return std::nullopt;
}

[[nodiscard]] bool ParseBlock(std::string_view blockJson, UiBlock& out, std::string* error) {
    const std::optional<std::string> typeStr = json::ExtractJsonString(blockJson, "type");
    if (!typeStr.has_value()) {
        if (error != nullptr) {
            *error = "block missing \"type\"";
        }
        return false;
    }
    const std::optional<UiBlockKind> parsedKind = TryParseBlockKind(*typeStr);
    if (!parsedKind.has_value()) {
        if (error != nullptr) {
            *error = std::string("unknown block type: ") + *typeStr;
        }
        return false;
    }
    out = UiBlock{};
    out.kind = *parsedKind;
    out.text = json::ExtractJsonString(blockJson, "text").value_or("");
    out.align = json::ExtractJsonString(blockJson, "align").value_or("");
    out.label = json::ExtractJsonString(blockJson, "label").value_or("");
    out.speaker = json::ExtractJsonString(blockJson, "speaker").value_or("");
    out.voiceHint = json::ExtractJsonString(blockJson, "voice").value_or("");
    out.portraitRelativePath = json::ExtractJsonString(blockJson, "portrait").value_or("");
    out.portraitSide = json::ExtractJsonString(blockJson, "side").value_or("");
    out.imageRelativePath = json::ExtractJsonString(blockJson, "image").value_or("");
    out.imageAnchor = json::ExtractJsonString(blockJson, "anchor").value_or("");
    const std::optional<double> h = json::ExtractJsonDouble(blockJson, "height");
    if (h.has_value()) {
        out.spacerHeight = static_cast<float>(*h);
    }
    const std::optional<double> ihh = json::ExtractJsonDouble(blockJson, "heightHint");
    if (ihh.has_value()) {
        out.imageHeightHint = static_cast<float>(*ihh);
    }
    const std::optional<bool> rememberHist = json::ExtractJsonBool(blockJson, "rememberInHistory");
    if (rememberHist.has_value()) {
        out.rememberInHistory = *rememberHist;
    } else {
        const std::optional<bool> rememberShort = json::ExtractJsonBool(blockJson, "remember");
        if (rememberShort.has_value()) {
            out.rememberInHistory = *rememberShort;
        }
    }
    const std::optional<bool> backlogOnly = json::ExtractJsonBool(blockJson, "backlogOnly");
    if (backlogOnly.has_value()) {
        out.historyBacklogOnly = *backlogOnly;
    }
    const std::optional<std::string_view> actionObj = json::ExtractJsonObject(blockJson, "action");
    if (actionObj.has_value()) {
        out.action = ParseActionObject(*actionObj);
    }

    if (out.kind == UiBlockKind::Choices) {
        const std::vector<std::string_view> choicePieces = json::SplitJsonArrayObjects(blockJson, "choices");
        for (std::string_view piece : choicePieces) {
            UiChoiceItem item{};
            item.label = json::ExtractJsonString(piece, "label").value_or("");
            const std::optional<std::string_view> chAct = json::ExtractJsonObject(piece, "action");
            if (chAct.has_value()) {
                item.action = ParseActionObject(*chAct);
            }
            const std::optional<std::string_view> chVis = json::ExtractJsonObject(piece, "visibleWhen");
            if (chVis.has_value()) {
                item.visibleWhenVar = json::ExtractJsonString(*chVis, "var").value_or("");
                item.visibleWhenEquals = json::ExtractJsonString(*chVis, "equals").value_or("");
            }
            out.choices.push_back(std::move(item));
        }
    }

    const std::optional<std::string_view> visibleWhen = json::ExtractJsonObject(blockJson, "visibleWhen");
    if (visibleWhen.has_value()) {
        out.visibleWhenVar = json::ExtractJsonString(*visibleWhen, "var").value_or("");
        out.visibleWhenEquals = json::ExtractJsonString(*visibleWhen, "equals").value_or("");
    }
    return true;
}

[[nodiscard]] bool ParseScreen(std::string_view screenJson, UiScreen& out, std::string* error) {
    out.id = json::ExtractJsonString(screenJson, "id").value_or("");
    out.title = json::ExtractJsonString(screenJson, "title").value_or("");
    if (out.id.empty()) {
        if (error != nullptr) {
            *error = "screen missing \"id\"";
        }
        return false;
    }

    const std::optional<std::string_view> bg = json::ExtractJsonObject(screenJson, "background");
    if (bg.has_value()) {
        std::array<float, 4> rgba = out.backgroundRgba;
        if (TryParseFloat4AfterKey(*bg, "tint", rgba)) {
            out.backgroundRgba = rgba;
        }
        const std::optional<std::string> bgImg = json::ExtractJsonString(*bg, "image");
        if (bgImg.has_value() && !bgImg->empty()) {
            out.backgroundImageRelative = *bgImg;
        }
    }

    out.musicHint = json::ExtractJsonString(screenJson, "music").value_or("");

    const std::optional<std::string_view> advanceObj = json::ExtractJsonObject(screenJson, "advance");
    if (advanceObj.has_value()) {
        out.advanceOnSpace = json::ExtractJsonBool(*advanceObj, "onSpace").value_or(false);
        out.advanceOnClick = json::ExtractJsonBool(*advanceObj, "onClick").value_or(false);
        out.advanceOnEnter = json::ExtractJsonBool(*advanceObj, "onEnter").value_or(false);
        out.advanceOnMouseWheel = json::ExtractJsonBool(*advanceObj, "onMouseWheel").value_or(false);
        const std::optional<double> delaySec = json::ExtractJsonDouble(*advanceObj, "delaySeconds");
        if (delaySec.has_value() && *delaySec > 0.0) {
            out.advanceAfterSeconds = static_cast<float>(*delaySec);
        }
        const std::optional<std::string_view> advAct = json::ExtractJsonObject(*advanceObj, "action");
        if (advAct.has_value()) {
            out.advanceAction = ParseActionObject(*advAct);
        }
    }

    out.blocks.clear();
    const std::vector<std::string_view> blockPieces = json::SplitJsonArrayObjects(screenJson, "blocks");
    for (std::string_view piece : blockPieces) {
        UiBlock block{};
        if (!ParseBlock(piece, block, error)) {
            return false;
        }
        out.blocks.push_back(std::move(block));
    }
    return true;
}

} // namespace

bool TryParseUiManifestFromJson(const std::string_view jsonText, UiManifest& outManifest, std::string* errorMessage) {
    outManifest = UiManifest{};
    const std::optional<std::int32_t> ver = json::ExtractJsonInt(jsonText, "schemaVersion");
    if (ver.has_value()) {
        outManifest.schemaVersion = *ver;
    }
    outManifest.startScreenId = json::ExtractJsonString(jsonText, "startScreen").value_or("");

    const std::vector<std::string_view> variablePieces = json::SplitJsonArrayObjects(jsonText, "variables");
    for (std::string_view piece : variablePieces) {
        UiVariableDef def{};
        def.id = json::ExtractJsonString(piece, "id").value_or("");
        if (def.id.empty()) {
            def.id = json::ExtractJsonString(piece, "name").value_or("");
        }
        def.value = json::ExtractJsonString(piece, "value").value_or("");
        if (!def.id.empty()) {
            outManifest.variables.push_back(std::move(def));
        }
    }

    const std::vector<std::string_view> screenPieces = json::SplitJsonArrayObjects(jsonText, "screens");
    for (std::string_view piece : screenPieces) {
        UiScreen screen{};
        std::string err;
        if (!ParseScreen(piece, screen, &err)) {
            if (errorMessage != nullptr) {
                *errorMessage = err.empty() ? std::string("invalid screen object") : std::move(err);
            }
            return false;
        }
        outManifest.screens.push_back(std::move(screen));
    }

    if (outManifest.startScreenId.empty() && !outManifest.screens.empty()) {
        outManifest.startScreenId = outManifest.screens.front().id;
    }
    return !outManifest.screens.empty();
}

bool TryLoadUiManifestFromJsonFile(const std::filesystem::path& path, UiManifest& outManifest, std::string* errorMessage) {
    const std::string text = ReadEntireFile(path);
    if (text.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "failed to read UI manifest or empty file: " + path.string();
        }
        return false;
    }
    return TryParseUiManifestFromJson(text, outManifest, errorMessage);
}

} // namespace ri::ui
