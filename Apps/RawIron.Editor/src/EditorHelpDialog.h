#pragma once

#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace ri::editor {

#if defined(_WIN32)
[[nodiscard]] std::string BuildEditorHelpGuideText();

void ShowEditorHelpDialog(HWND ownerHwnd);
#endif

} // namespace ri::editor
