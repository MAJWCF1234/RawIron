#pragma once

namespace ri::editor {

/// Registers editor workspace preview hooks for game modules linked into this build.
/// Implemented in RawIron.Editor.BundledGames (empty when no bundled games are enabled).
void RegisterBundledGameEditorPreviews();

} // namespace ri::editor
