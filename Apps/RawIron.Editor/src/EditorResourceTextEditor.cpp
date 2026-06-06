#include "EditorResourceTextEditor.h"

#include "EditorRenderer.h"
#include "EditorResourceDocument.h"
#include "EditorWorkspace.h"

#include <algorithm>

#if defined(_WIN32)
#include <shellapi.h>
#endif

namespace ri::editor {

#if defined(_WIN32)
namespace {

[[nodiscard]] std::wstring ReadWindowText(HWND hwnd) {
    if (hwnd == nullptr) {
        return {};
    }
    const int len = GetWindowTextLengthW(hwnd);
    std::wstring wide(static_cast<std::size_t>(std::max(0, len) + 2), L'\0');
    const int copied = len <= 0 ? 0 : GetWindowTextW(hwnd, wide.data(), len + 1);
    wide.resize(static_cast<std::size_t>(std::max(0, copied)));
    return wide;
}

} // namespace

void DestroyResourceTextEditorControl(HWND& resourceTextEditHwnd) {
    if (resourceTextEditHwnd != nullptr) {
        DestroyWindow(resourceTextEditHwnd);
        resourceTextEditHwnd = nullptr;
    }
}

void EnsureResourceTextEditorCreated(HWND hwnd,
                                     HWND& resourceTextEditHwnd,
                                     HFONT bodyFont,
                                     const bool filesPanelActive,
                                     const std::filesystem::path& loadedResourceAbsolutePath,
                                     const std::string& loadedResourceUtf8,
                                     const std::string& resourceEditorAuxMessage,
                                     bool& resourceFileDirty) {
    if (resourceTextEditHwnd != nullptr || hwnd == nullptr) {
        return;
    }
    if (!filesPanelActive || !resourceEditorAuxMessage.empty()) {
        return;
    }
    if (loadedResourceAbsolutePath.empty() || !IsLikelyTextResourcePath(loadedResourceAbsolutePath)) {
        return;
    }
    resourceTextEditHwnd =
        CreateWindowExW(WS_EX_CLIENTEDGE,
                        L"EDIT",
                        EditorRenderer::Utf8ToWide(loadedResourceUtf8).c_str(),
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE |
                            ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
                        0,
                        0,
                        10,
                        10,
                        hwnd,
                        reinterpret_cast<HMENU>(static_cast<ULONG_PTR>(2051)),
                        GetModuleHandleW(nullptr),
                        nullptr);
    if (resourceTextEditHwnd != nullptr) {
        SendMessageW(resourceTextEditHwnd, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont), MAKELPARAM(TRUE, 0));
        SendMessageW(resourceTextEditHwnd, EM_SETLIMITTEXT, 0, 0);
        resourceFileDirty = false;
        SetFocus(resourceTextEditHwnd);
    }
}

void LayoutResourceTextEditorControl(HWND hwnd,
                                     HWND resourceTextEditHwnd,
                                     const bool filesPanelActive,
                                     const std::filesystem::path& loadedResourceAbsolutePath,
                                     const std::string& resourceEditorAuxMessage,
                                     const RECT& inspectorInner) {
    if (resourceTextEditHwnd == nullptr || hwnd == nullptr) {
        return;
    }
    const bool visible = filesPanelActive && resourceEditorAuxMessage.empty()
        && !loadedResourceAbsolutePath.empty() && IsLikelyTextResourcePath(loadedResourceAbsolutePath);
    if (!visible) {
        ShowWindow(resourceTextEditHwnd, SW_HIDE);
        return;
    }
    constexpr int kInspectorMetaHeight = 236;
    const int top = inspectorInner.top + kInspectorMetaHeight;
    const int bottom = static_cast<int>(inspectorInner.bottom) - 10;
    ShowWindow(resourceTextEditHwnd, SW_SHOW);
    MoveWindow(resourceTextEditHwnd,
               inspectorInner.left + 10,
               top,
               std::max(40, static_cast<int>(inspectorInner.right - inspectorInner.left - 20)),
               std::max(40, bottom - top),
               TRUE);
    SetWindowPos(resourceTextEditHwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void SyncResourceTextEditorContent(HWND resourceTextEditHwnd,
                                   const std::string& loadedResourceUtf8,
                                   const bool resourceFileDirty) {
    if (resourceTextEditHwnd == nullptr || resourceFileDirty) {
        return;
    }
    const std::wstring wanted = EditorRenderer::Utf8ToWide(loadedResourceUtf8);
    const std::wstring current = ReadWindowText(resourceTextEditHwnd);
    if (current == wanted) {
        return;
    }
    SetWindowTextW(resourceTextEditHwnd, wanted.c_str());
}

bool SaveActiveResourceFileFromEditor(HWND resourceTextEditHwnd,
                                      const std::filesystem::path& loadedResourceAbsolutePath,
                                      std::string& loadedResourceUtf8) {
    if (resourceTextEditHwnd == nullptr || loadedResourceAbsolutePath.empty()) {
        return false;
    }
    const std::wstring wide = ReadWindowText(resourceTextEditHwnd);
    const std::string utf8 = EditorRenderer::WideToUtf8(wide);
    if (!SaveResourceDocumentUtf8(loadedResourceAbsolutePath, utf8)) {
        return false;
    }
    loadedResourceUtf8 = utf8;
    return true;
}

void OpenActiveResourceInExplorer(HWND hwnd, const std::filesystem::path& loadedResourceAbsolutePath) {
    if (loadedResourceAbsolutePath.empty()) {
        return;
    }
    std::wstring arg = L"/select,\"";
    arg += loadedResourceAbsolutePath.wstring();
    arg += L"\"";
    ShellExecuteW(hwnd, L"open", L"explorer.exe", arg.c_str(), nullptr, SW_SHOWNORMAL);
}

bool ResolveDirtyResourceBeforeContextSwitch(HWND hwnd,
                                             std::string_view action,
                                             const std::filesystem::path& loadedResourceAbsolutePath,
                                             bool& resourceFileDirty,
                                             const std::function<bool()>& saveFn,
                                             std::string& statusOut) {
    if (!resourceFileDirty) {
        return true;
    }

    const std::string fileName = loadedResourceAbsolutePath.empty()
        ? std::string("resource")
        : loadedResourceAbsolutePath.filename().string();

    if (hwnd != nullptr) {
        const std::string prompt =
            "Save changes to '" + fileName + "' before " + std::string(action) +
            "?\n\nChoose No to discard the unsaved edits, or Cancel to keep editing.";
        const int choice = MessageBoxW(hwnd,
                                       EditorRenderer::Widen(prompt).c_str(),
                                       L"RawIron Editor - Unsaved Resource",
                                       MB_ICONWARNING | MB_YESNOCANCEL);
        if (choice == IDCANCEL) {
            statusOut = "Canceled " + std::string(action) + " to keep editing " + fileName + ".";
            return false;
        }
        if (choice == IDNO) {
            resourceFileDirty = false;
            statusOut = "Discarded unsaved edits to " + fileName + ".";
            return true;
        }
    }

    if (saveFn()) {
        statusOut = "Saved " + fileName + " before " + std::string(action) + ".";
        return true;
    }

    statusOut = "Save failed for " + fileName + "; kept resource open.";
    return false;
}
#endif

} // namespace ri::editor
