#pragma once

#include "EditorFilesInspector.h"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

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
void DestroyResourceTextEditorControl(HWND& resourceTextEditHwnd);
void EnsureResourceTextEditorCreated(HWND hwnd,
                                     HWND& resourceTextEditHwnd,
                                     HFONT bodyFont,
                                     bool filesPanelActive,
                                     const std::filesystem::path& loadedResourceAbsolutePath,
                                     const std::string& loadedResourceUtf8,
                                     const std::string& resourceEditorAuxMessage,
                                     bool& resourceFileDirty);
void LayoutResourceTextEditorControl(HWND hwnd,
                                     HWND resourceTextEditHwnd,
                                     bool filesPanelActive,
                                     const std::filesystem::path& loadedResourceAbsolutePath,
                                     const std::string& resourceEditorAuxMessage,
                                     const RECT& inspectorInner,
                                     const FilesInspectorPanelModel& filesPanelModel);
void SyncResourceTextEditorContent(HWND resourceTextEditHwnd,
                                   const std::string& loadedResourceUtf8,
                                   bool resourceFileDirty);
[[nodiscard]] bool SaveActiveResourceFileFromEditor(HWND resourceTextEditHwnd,
                                                    const std::filesystem::path& loadedResourceAbsolutePath,
                                                    std::string& loadedResourceUtf8);
void OpenActiveResourceInExplorer(HWND hwnd, const std::filesystem::path& loadedResourceAbsolutePath);
[[nodiscard]] bool ResolveDirtyResourceBeforeContextSwitch(HWND hwnd,
                                                           std::string_view action,
                                                           const std::filesystem::path& loadedResourceAbsolutePath,
                                                           bool& resourceFileDirty,
                                                           const std::function<bool()>& saveFn,
                                                           std::string& statusOut);
#endif

} // namespace ri::editor
