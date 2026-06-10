#include "EditorHelpDialog.h"

#include "EditorRenderer.h"

#include <algorithm>
#include <string>

#if defined(_WIN32)

namespace ri::editor {
namespace {

constexpr wchar_t kHelpDialogClassName[] = L"RawIronEditorHelpDialog";
constexpr int kCloseButtonId = 1001;
constexpr int kHelpTextControlId = 1002;

struct HelpDialogState {
    HFONT bodyFont = nullptr;
    HWND textControl = nullptr;
    HWND closeButton = nullptr;
};

void LayoutHelpDialogControls(HWND hwnd, HelpDialogState* state) {
    if (state == nullptr) {
        return;
    }
    RECT client{};
    GetClientRect(hwnd, &client);
    const int margin = 12;
    const int buttonHeight = 30;
    const int buttonWidth = 110;
    const int footerGap = 10;
    const int clientWidth = static_cast<int>(client.right - client.left);
    const int clientHeight = static_cast<int>(client.bottom - client.top);
    const int textBottom = clientHeight - margin - buttonHeight - footerGap;
    if (state->textControl != nullptr) {
        MoveWindow(state->textControl,
                   margin,
                   margin,
                   std::max(40, clientWidth - margin * 2),
                   std::max(40, textBottom - margin),
                   TRUE);
    }
    if (state->closeButton != nullptr) {
        MoveWindow(state->closeButton,
                   (clientWidth - buttonWidth) / 2,
                   clientHeight - margin - buttonHeight,
                   buttonWidth,
                   buttonHeight,
                   TRUE);
    }
}

LRESULT CALLBACK HelpDialogWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    HelpDialogState* state = reinterpret_cast<HelpDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
        case WM_CREATE: {
            auto* createParams = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto* dialogState = new HelpDialogState{};
            dialogState->bodyFont = CreateFontW(-15,
                                                 0,
                                                 0,
                                                 0,
                                                 FW_NORMAL,
                                                 FALSE,
                                                 FALSE,
                                                 FALSE,
                                                 DEFAULT_CHARSET,
                                                 OUT_DEFAULT_PRECIS,
                                                 CLIP_DEFAULT_PRECIS,
                                                 CLEARTYPE_QUALITY,
                                                 DEFAULT_PITCH | FF_DONTCARE,
                                                 L"Segoe UI");
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dialogState));

            RECT client{};
            GetClientRect(hwnd, &client);
            const int margin = 12;
            const int buttonHeight = 30;
            const int footerGap = 10;
            const int clientWidth = static_cast<int>(client.right - client.left);
            const int clientHeight = static_cast<int>(client.bottom - client.top);
            const int textBottom = clientHeight - margin - buttonHeight - footerGap;

            dialogState->textControl = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                EditorRenderer::Widen(BuildEditorHelpGuideText()).c_str(),
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL,
                margin,
                margin,
                std::max(40, clientWidth - margin * 2),
                std::max(40, textBottom - margin),
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kHelpTextControlId)),
                createParams->hInstance,
                nullptr);
            SendMessageW(dialogState->textControl, WM_SETFONT, reinterpret_cast<WPARAM>(dialogState->bodyFont), TRUE);

            dialogState->closeButton = CreateWindowW(L"BUTTON",
                                                     L"Close",
                                                     WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                                     (clientWidth - 110) / 2,
                                                     clientHeight - margin - buttonHeight,
                                                     110,
                                                     buttonHeight,
                                                     hwnd,
                                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCloseButtonId)),
                                                     createParams->hInstance,
                                                     nullptr);
            SendMessageW(dialogState->closeButton, WM_SETFONT, reinterpret_cast<WPARAM>(dialogState->bodyFont), TRUE);
            return 0;
        }
        case WM_SIZE:
            LayoutHelpDialogControls(hwnd, state);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wParam) == kCloseButtonId) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (state != nullptr) {
                if (state->bodyFont != nullptr) {
                    DeleteObject(state->bodyFont);
                }
                delete state;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void EnsureHelpDialogClassRegistered() {
    static bool registered = false;
    if (registered) {
        return;
    }
    WNDCLASSW windowClass{};
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = HelpDialogWindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = kHelpDialogClassName;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    RegisterClassW(&windowClass);
    registered = true;
}

} // namespace

std::string BuildEditorHelpGuideText() {
    return
        "RawIron Editor — User Guide\r\n"
        "================================\r\n"
        "\r\n"
        "OVERVIEW\r\n"
        "--------\r\n"
        "The RawIron Editor is a Win32 scene authoring tool for mounted game projects. "
        "Use the left panel for scene hierarchy and resources, the center for viewports, "
        "and the right panel for node/brush/gameplay inspectors.\r\n"
        "\r\n"
        "Launch with a project:\r\n"
        "  Launch RawIron Editor.cmd          (defaults to liminal-hall)\r\n"
        "  RawIron.Editor.exe --editor-ui --workspace=<root> --game=<id>\r\n"
        "\r\n"
        "LAYOUT & PANELS\r\n"
        "-----------------\r\n"
        "Left panel tabs:\r\n"
        "  Scene      Hierarchy list, search, node selection\r\n"
        "  Create     Creator Lab (templates, atmosphere, inserts, cameras)\r\n"
        "  Resources  Browse and edit text files in the mounted game\r\n"
        "\r\n"
        "Right panel tabs (number keys 1–6 jump tabs):\r\n"
        "  1 Node      Transform, rename, parenting, mesh info\r\n"
        "  2 Brush     Structural brush preset and spawn tools\r\n"
        "  3 Gameplay  Inventory policy, triggers, playtest shortcuts\r\n"
        "  4 Files     Save/open the selected resource file\r\n"
        "  5 Store     Plugin store packages\r\n"
        "  6 UI / VN   UI workbench for screens and dialogue blocks\r\n"
        "\r\n"
        "Panel collapse (more viewport space):\r\n"
        "  Ctrl+[       Collapse / expand left panel (Hierarchy)\r\n"
        "  Ctrl+]       Collapse / expand right panel (Inspector)\r\n"
        "  Click « / »  On panel headers\r\n"
        "\r\n"
        "Mesh authoring catalog (structural presets at bottom of viewport):\r\n"
        "  Ctrl+2       Show / hide catalog strip\r\n"
        "\r\n"
        "VIEWPORTS\r\n"
        "-----------\r\n"
        "Tab toggles layout:\r\n"
        "  Full 3D     One large perspective view (default when a game is mounted)\r\n"
        "  Quad views  Top, Side, Front, and Perspective (bottom-right)\r\n"
        "\r\n"
        "Preview quality:\r\n"
        "  Default     Fast software raster (smooth UI)\r\n"
        "  Ctrl+Shift+R  Toggle ray-traced quality preview (slower)\r\n"
        "\r\n"
        "CAMERA NAVIGATION\r\n"
        "-------------------\r\n"
        "Tool modes (toolbar or keyboard):\r\n"
        "  S            Select mode — pick and edit objects\r\n"
        "  C            Create mode — stamp presets into the scene\r\n"
        "  Camera btn   Camera mode — navigate without placing\r\n"
        "\r\n"
        "Bryce-style create workflow:\r\n"
        "  1. Press C or click Create on the tool strip\r\n"
        "  2. Pick a mesh/volume preset in the bottom catalog gallery\r\n"
        "  3. Click the viewport to stamp it on the ground or a surface\r\n"
        "  4. Double-click a catalog preset to stamp at camera focus\r\n"
        "  5. Click the Sky bar above the viewport to cycle atmosphere\r\n"
        "\r\n"
        "Orbit:\r\n"
        "  LMB drag     On empty space in perspective\r\n"
        "  MMB drag     Anywhere in perspective\r\n"
        "  Alt + LMB    Orbit even when clicking on geometry\r\n"
        "\r\n"
        "Pan:\r\n"
        "  Shift + LMB  Drag in perspective\r\n"
        "  RMB drag     Anywhere in perspective\r\n"
        "\r\n"
        "Zoom:\r\n"
        "  Mouse wheel  Over perspective viewport\r\n"
        "\r\n"
        "Frame camera:\r\n"
        "  F            Frame selected node\r\n"
        "  Home         Frame all renderables\r\n"
        "  Shift+F      Frame all renderables\r\n"
        "  Double-click Hierarchy row frames that node\r\n"
        "\r\n"
        "Other camera:\r\n"
        "  Space        Toggle auto-orbit demo preview\r\n"
        "  Esc          Cancel active camera drag\r\n"
        "\r\n"
        "SELECTION & HIERARCHY\r\n"
        "-----------------------\r\n"
        "  Click row              Select node in hierarchy\r\n"
        "  Click mesh in viewport Select renderable (quad or full 3D)\r\n"
        "  Up / Down / PgUp / PgDn Navigate hierarchy or resources\r\n"
        "  Ctrl+F                 Filter scene or resource list\r\n"
        "  F2                     Rename selected node\r\n"
        "  Esc                    Clear selection to World root\r\n"
        "  , / .                  Previous / next authored node\r\n"
        "\r\n"
        "TRANSFORM GIZMO\r\n"
        "---------------\r\n"
        "  T            Translate mode\r\n"
        "  R            Rotate mode\r\n"
        "  U            Scale (Uniform) mode\r\n"
        "  X / Y / Z    Active axis\r\n"
        "  Drag arrows  Move/rotate/scale on selected node\r\n"
        "\r\n"
        "Grid snap:\r\n"
        "  G            Toggle grid snap on/off\r\n"
        "  Ctrl+G       Snap selected node to grid now\r\n"
        "  - / +        Smaller / larger grid step\r\n"
        "\r\n"
        "ADDING & EDITING CONTENT\r\n"
        "------------------------\r\n"
        "Toolbar (below title bar):\r\n"
        "  T / R / U    Transform modes\r\n"
        "  + Cube / + Plane / + Trigger / + Light\r\n"
        "  Duplicate    Copy selected authored node\r\n"
        "  Export CSV   Export assembly primitives\r\n"
        "  Playtest     Launch player build\r\n"
        "\r\n"
        "Keyboard spawns:\r\n"
        "  Ctrl+Shift+C   Add cube\r\n"
        "  Ctrl+Shift+P   Add plane\r\n"
        "  Ctrl+Shift+T   Add trigger volume\r\n"
        "  Ctrl+Shift+O   Add light\r\n"
        "  Ctrl+Shift+B   Spawn structural brush preset\r\n"
        "  Ctrl+Shift+1–9 Pick structural preset by digit\r\n"
        "  [ / ]          Cycle structural brush preset\r\n"
        "\r\n"
        "Node editing:\r\n"
        "  Ctrl+D         Duplicate selected\r\n"
        "  Del            Delete authored mesh node (moves to editor trash)\r\n"
        "  Ctrl+G         Group selected under new folder\r\n"
        "  Ctrl+Shift+G   Ungroup\r\n"
        "  Ctrl+Shift+N   Create empty group node\r\n"
        "  Ctrl+Shift+W   Reparent selected to World root\r\n"
        "  Ctrl+R         Reset selected transform\r\n"
        "  Ctrl+Z / Ctrl+Y  Undo / Redo\r\n"
        "\r\n"
        "SAVE, EXPORT & PLAY\r\n"
        "-------------------\r\n"
        "Top bar buttons:\r\n"
        "  Save           Persist scene transforms, authored nodes, orbit camera\r\n"
        "  Export Scene   Write level CSV from authored content\r\n"
        "  Playtest       Run the game with current workspace\r\n"
        "  Files          Jump to Resources tab\r\n"
        "  New Game       Open Creator Lab\r\n"
        "\r\n"
        "Shortcuts:\r\n"
        "  Ctrl+S         Save editor scene state\r\n"
        "  Ctrl+Shift+S   Save timestamped snapshot\r\n"
        "  Ctrl+E         Export assembly CSV\r\n"
        "  Ctrl+L         Load persistent editor scene\r\n"
        "  Ctrl+Shift+L   Load autosave if present\r\n"
        "  F5             Reload focused game scene\r\n"
        "  Ctrl+Shift+M   Scaffold mounted game files\r\n"
        "\r\n"
        "LOGIC & DIAGNOSTICS\r\n"
        "-------------------\r\n"
        "  F6             Toggle runtime diagnostics overlay in viewport\r\n"
        "  F7 / Shift+F7  Pulse logic input #1 / #2 on selected node\r\n"
        "  F8             Pulse trigger OnStartTouch on selected volume\r\n"
        "  Alt+W          Wire-pick logic port on selected node\r\n"
        "  Alt+[ / Alt+]  Cycle wire-pick output port\r\n"
        "  Ctrl+Alt+L     Hide/show logic layer in player preview\r\n"
        "\r\n"
        "CREATOR LAB (Create tab)\r\n"
        "------------------------\r\n"
        "Pick a template, cycle name variants, then Create Game Project.\r\n"
        "Dropdown rows apply atmosphere (.riscript), world inserts, and camera presets.\r\n"
        "Viewport menu Create also opens this tab.\r\n"
        "\r\n"
        "TIPS\r\n"
        "----\r\n"
        "  • Ray-traced preview looks best but costs UI performance — use it for shots, not all-day editing.\r\n"
        "  • Collapse the inspector (Ctrl+]) when placing meshes; expand it (Ctrl+]) when tuning properties.\r\n"
        "  • Orbit camera is saved with the scene sidecar when you Ctrl+S.\r\n"
        "  • Status bar at the bottom shows the result of the last action and useful hints.\r\n"
        "\r\n"
        "Press F1 anytime to reopen this guide.\r\n";
}

void ShowEditorHelpDialog(const HWND ownerHwnd) {
    EnsureHelpDialogClassRegistered();

    RECT ownerRect{};
    if (ownerHwnd != nullptr) {
        GetWindowRect(ownerHwnd, &ownerRect);
    } else {
        ownerRect.left = 0;
        ownerRect.top = 0;
        ownerRect.right = GetSystemMetrics(SM_CXSCREEN);
        ownerRect.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    constexpr int dialogWidth = 780;
    constexpr int dialogHeight = 700;
    const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - dialogWidth) / 2;
    const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - dialogHeight) / 2;

    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME,
                                  kHelpDialogClassName,
                                  L"RawIron Editor — User Guide",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                  x,
                                  y,
                                  dialogWidth,
                                  dialogHeight,
                                  ownerHwnd,
                                  nullptr,
                                  GetModuleHandleW(nullptr),
                                  nullptr);
    if (dialog == nullptr) {
        return;
    }

    ShowWindow(dialog, SW_SHOW);
    UpdateWindow(dialog);
    SetForegroundWindow(dialog);

    if (ownerHwnd != nullptr) {
        EnableWindow(ownerHwnd, FALSE);
    }

    MSG message{};
    while (IsWindow(dialog) && GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE) {
            if (message.hwnd == dialog || IsChild(dialog, message.hwnd) != FALSE) {
                DestroyWindow(dialog);
                continue;
            }
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (ownerHwnd != nullptr) {
        EnableWindow(ownerHwnd, TRUE);
        SetForegroundWindow(ownerHwnd);
    }
}

#endif

} // namespace ri::editor
