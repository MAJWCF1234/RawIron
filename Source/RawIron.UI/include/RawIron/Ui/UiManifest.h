#pragma once

/// Declarative UI / screen manifests: menus, credits, and **visual-novel-style** flows (dialogue,
/// branching choices, portraits, background art) without a separate scripting runtime.
/// Authoring: JSON under `Assets/UI/` + `ui_manifest.schema.json` for editor validation.
/// Runtime: `UiFlowSession` + presenters (e.g. RawIron.UiMenu / future in-engine overlay).

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ri::ui {

enum class UiBlockKind : std::uint8_t {
    Heading = 0,
    Paragraph = 1,
    Label = 2,
    Spacer = 3,
    Separator = 4,
    Button = 5,
    /// Character line: `speaker`, `text`, optional `portrait` path + `side` (left|right|center).
    Say = 6,
    /// Italicized / de-emphasized prose (no speaker box).
    Narration = 7,
    /// Branching menu: `choices` array of `{ "label", "action" }` (VN menu / Ren'Py-style menu).
    Choices = 8,
    /// Decorative or CG panel: `image` path, `anchor` (top|bottom|fill), optional `heightHint`.
    Image = 9,
    /// Chapter / section marker: appears in backlog; optional one-line on screen unless `backlogOnly`.
    HistoryNote = 10,
};

enum class UiActionKind : std::uint8_t {
    None = 0,
    Navigate = 1,
    Emit = 2,
    Back = 3,
    /// Persist a string in the flow session store (`id` + `value` in JSON).
    SetVariable = 4,
};

struct UiAction {
    UiActionKind kind = UiActionKind::None;
    /// For Navigate: target screen id. For Emit: application action id. For SetVariable: variable id.
    std::string target{};
    /// SetVariable payload, or optional paired value for other kinds when using `setVar` bundle in JSON.
    std::string value{};
    /// If non-empty, action runs only when the store variable `whenVar` equals `whenEquals` (string match).
    std::string whenVar{};
    std::string whenEquals{};
    /// After `when` passes, assign before navigate / emit / back (VN route flags).
    std::string setVarId{};
    std::string setVarValue{};
};

/// One line in the VN backlog (Ren'Py-style history); `speaker` empty for narration-only lines.
struct UiHistoryLine {
    std::string speaker{};
    std::string text{};
    bool narration = false;
    /// Section break in backlog (from `historyNote` blocks).
    bool chapterMarker = false;
    /// Optional voice / line-audio cue id or path (from `say` block `voice`).
    std::string voiceCue{};
};

struct UiChoiceItem {
    std::string label{};
    UiAction action{};
    std::string visibleWhenVar{};
    std::string visibleWhenEquals{};
};

struct UiBlock {
    UiBlockKind kind = UiBlockKind::Paragraph;
    std::string text{};
    /// `left`, `center`, or `right` (default center for headings, left for paragraphs / say body).
    std::string align{};
    float spacerHeight = 12.0f;
    /// Button label when kind == Button.
    std::string label{};
    UiAction action{};
    /// `Say` / some `Label` uses.
    std::string speaker{};
    /// Optional voice line id or asset path (presenter + backlog hint; engine may play later).
    std::string voiceHint{};
    /// Texture path relative to workspace / texture root (presenter loads when wired; `${var}` interpolation supported).
    std::string portraitRelativePath{};
    /// `left`, `right`, or `center` — layout hint for portrait + text column.
    std::string portraitSide{};
    /// `Image` block: asset path relative to workspace (or game content root).
    std::string imageRelativePath{};
    /// `top`, `bottom`, or `fill` — vertical placement hint for `Image`.
    std::string imageAnchor{};
    float imageHeightHint = 0.0f;
    std::vector<UiChoiceItem> choices{};
    /// If `visibleWhenVar` is non-empty, block is shown only when the store matches `visibleWhenEquals`.
    std::string visibleWhenVar{};
    std::string visibleWhenEquals{};
    /// When true (default), `say` / `narration` lines may be appended to the session backlog (presenter opt-in).
    bool rememberInHistory = true;
    /// `historyNote`: if true, do not draw on the main screen (backlog only).
    bool historyBacklogOnly = false;
};

struct UiScreen {
    std::string id{};
    std::string title{};
    /// Premultiplied-friendly RGBA tint behind content (combined with optional background image).
    std::array<float, 4> backgroundRgba{0.02f, 0.02f, 0.06f, 0.94f};
    /// Optional full-screen backdrop image (path relative to workspace); presenters may layer this under blocks.
    std::string backgroundImageRelative{};
    /// Optional music / ambience asset id or path (engine interprets); stored for editor + future audio hook.
    std::string musicHint{};
    /// When set with `advanceOnSpace` / `advanceOnClick` / `advanceOnEnter` / `advanceOnMouseWheel` /
    /// `advanceAfterSeconds`, applies `advanceAction` (VN line advance).
    UiAction advanceAction{};
    bool advanceOnSpace = false;
    bool advanceOnClick = false;
    /// Also advance when Enter / keypad Enter is pressed (common VN binding).
    bool advanceOnEnter = false;
    /// If > 0 and `advance.action` is set, auto-fire once after this many seconds on this stack visit.
    float advanceAfterSeconds = 0.0f;
    /// Advance when the mouse wheel moves over the root window (VN-style scroll-to-continue).
    bool advanceOnMouseWheel = false;
    std::vector<UiBlock> blocks{};
};

struct UiVariableDef {
    std::string id{};
    std::string value{};
};

struct UiManifest {
    int schemaVersion = 1;
    std::string startScreenId{};
    /// Initial string store (Ren'Py-style `default` / game state for branching UI).
    std::vector<UiVariableDef> variables{};
    std::vector<UiScreen> screens{};
};

} // namespace ri::ui
