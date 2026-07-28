#include "ForgeCatalog.h"
#include "ForgeCatalogIndex.h"
#include "ForgePreviewBuilder.h"
#include "EditorVulkanViewport.h"

#include "RawIron/Core/CommandLine.h"
#include "RawIron/Content/AuthoringHandoff.h"
#include "RawIron/Content/PrimitiveModelDocument.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/PrimitiveModelBake.h"
#include "RawIron/Scene/StructuralPrimitivePresets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <commctrl.h>
#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <uxtheme.h>
#endif

namespace {

namespace fs = std::filesystem;

fs::path ResolveExecutablePath() {
#if defined(_WIN32)
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length > 0 && length < buffer.size()) {
        buffer.resize(length);
        return fs::path(buffer);
    }
#endif
    return fs::current_path();
}

bool LooksLikeWorkspace(const fs::path& path) {
    std::error_code error{};
    return fs::exists(path / "CMakeLists.txt", error) && fs::is_directory(path / "Assets", error)
        && fs::is_directory(path / "Source", error);
}

fs::path FindWorkspaceRoot(fs::path start) {
    std::error_code error{};
    if (!fs::is_directory(start, error)) {
        start = start.parent_path();
    }
    for (fs::path candidate = start; !candidate.empty();) {
        if (LooksLikeWorkspace(candidate)) {
            return fs::weakly_canonical(candidate, error);
        }
        const fs::path parent = candidate.parent_path();
        if (parent == candidate) {
            break;
        }
        candidate = parent;
    }
    return fs::current_path();
}

fs::path ResolveWorkspaceRoot(const ri::core::CommandLine& commandLine) {
    for (const std::string_view option : {"--workspace", "--workspace-root", "--root"}) {
        if (const std::optional<std::string> value = commandLine.GetValue(option);
            value.has_value() && !value->empty()) {
            return FindWorkspaceRoot(fs::path(*value));
        }
    }
    if (LooksLikeWorkspace(fs::current_path())) {
        return FindWorkspaceRoot(fs::current_path());
    }
    return FindWorkspaceRoot(ResolveExecutablePath());
}

fs::path ResolveWorkspacePath(const fs::path& workspaceRoot, const fs::path& path) {
    return path.is_absolute() ? path : workspaceRoot / path;
}

void PrintHeadlessSummary(const ri::forge::AssetCatalog& catalog) {
    std::cout << "[Forge Ready]\n";
    std::cout << "Workspace: " << catalog.workspaceRoot.string() << "\n";
    std::cout << "Asset source: " << catalog.sourceRoot.string() << "\n";
    std::cout << "Model sources: " << catalog.modelCount << "\n";
    std::cout << "Primitive models: " << catalog.primitiveModelCount << "\n";
    std::cout << "Rig assets: " << catalog.rigCount << "\n";
    std::cout << "Invalid primitive models: " << catalog.invalidPrimitiveModelCount << "\n";
    std::cout << "Invalid rigs: " << catalog.invalidRigCount << "\n";
}

int PrintHandoffProbe(const fs::path& workspaceRoot, const fs::path& assetPath) {
    const ri::content::AuthoringHandoffReport report = ri::content::BuildAuthoringHandoff({
        .workspaceRoot = workspaceRoot,
        .assetPath = assetPath,
    });
    std::cout << "Forge handoff: " << (report.valid ? "ready" : "rejected") << "\n";
    std::cout << "Asset kind: " << ri::content::ToString(report.assetKind) << "\n";
    std::cout << "Asset: " << report.assetPath.string() << "\n";
    if (!report.editorArguments.empty()) {
        std::cout << "Editor args:";
        for (const std::string& argument : report.editorArguments) {
            std::cout << " " << argument;
        }
        std::cout << "\n";
    }
    for (const std::string& issue : report.issues) {
        std::cout << "Issue: " << issue << "\n";
    }
    return report.valid ? 0 : 1;
}

#if defined(_WIN32)

std::wstring Widen(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int required = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) {
        return std::wstring(value.begin(), value.end());
    }
    std::wstring wide(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), required);
    return wide;
}

std::string ReadControlTextUtf8(const HWND control) {
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(length + 1), L'\0');
    GetWindowTextW(control, wide.data(), length + 1);
    const int required = WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), length, nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }
    std::string value(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), length, value.data(), required, nullptr, nullptr);
    return value;
}

enum ControlId : int {
    kAssetList = 100,
    kRefresh = 101,
    kNewHumanoid = 102,
    kValidate = 103,
    kOpenSource = 104,
    kOpenInEditor = 105,
    kNewPrimitiveModel = 106,
    kAddPrimitive = 107,
    kAddGroup = 108,
    kBakeModel = 109,
    kModelElement = 110,
    kTransformMode = 111,
    kTransformX = 112,
    kTransformY = 113,
    kTransformZ = 114,
    kApplyTransform = 115,
    kAssetFilter = 116,
    kFocusAssetFilter = 117,
    kPrimitivePresetBase = 2000,
};

constexpr UINT_PTR kCatalogPollTimer = 1U;

fs::path ResolveEditorExecutable() {
    const fs::path forgeExecutable = ResolveExecutablePath();
    const fs::path configuration = forgeExecutable.parent_path().filename();
    return forgeExecutable.parent_path().parent_path().parent_path()
        / "RawIron.Editor" / configuration / "RawIron.Editor.exe";
}

std::wstring QuoteArgument(const std::string& value) {
    std::wstring quoted = L"\"";
    quoted += Widen(value);
    quoted += L"\"";
    return quoted;
}

std::wstring JoinArguments(const std::vector<std::string>& arguments) {
    std::wstring joined{};
    for (const std::string& argument : arguments) {
        if (!joined.empty()) {
            joined += L' ';
        }
        joined += QuoteArgument(argument);
    }
    return joined;
}

class ForgeWindow {
public:
    explicit ForgeWindow(fs::path workspaceRoot, const bool background)
        : workspaceRoot_(std::move(workspaceRoot)),
          catalogIndex_(workspaceRoot_),
          previewBuilder_(),
          background_(background) {}

    int Run(HINSTANCE instance) {
        instance_ = instance;
        INITCOMMONCONTROLSEX controls{
            .dwSize = sizeof(INITCOMMONCONTROLSEX),
            .dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES,
        };
        InitCommonControlsEx(&controls);
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = &ForgeWindow::WindowProc;
        windowClass.hInstance = instance;
        windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        windowClass.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
        windowClass.hbrBackground = nullptr;
        windowClass.lpszClassName = L"RawIronForgeWindow";
        if (RegisterClassExW(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return 1;
        }

        hwnd_ = CreateWindowExW(
            background_ ? WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW : 0,
            windowClass.lpszClassName,
            L"Raw Iron Forge - Model & Rig Workbench",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            background_ ? -32000 : CW_USEDEFAULT,
            background_ ? -32000 : CW_USEDEFAULT,
            1440,
            880,
            nullptr,
            nullptr,
            instance,
            this);
        if (hwnd_ == nullptr) {
            return 1;
        }

        ShowWindow(hwnd_, background_ ? SW_SHOWNOACTIVATE : SW_SHOWDEFAULT);
        UpdateWindow(hwnd_);
        const std::array<ACCEL, 4> acceleratorEntries{{
            ACCEL{FVIRTKEY | FCONTROL, static_cast<WORD>('F'), kFocusAssetFilter},
            ACCEL{FVIRTKEY | FCONTROL, static_cast<WORD>('N'), kNewPrimitiveModel},
            ACCEL{FVIRTKEY | FCONTROL, static_cast<WORD>('S'), kApplyTransform},
            ACCEL{FVIRTKEY, VK_F5, kRefresh},
        }};
        const HACCEL accelerators = CreateAcceleratorTableW(
            const_cast<LPACCEL>(acceleratorEntries.data()),
            static_cast<int>(acceleratorEntries.size()));
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            if (accelerators == nullptr || !TranslateAcceleratorW(hwnd_, accelerators, &message)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        if (accelerators != nullptr) {
            DestroyAcceleratorTable(accelerators);
        }
        return static_cast<int>(message.wParam);
    }

private:
    static constexpr COLORREF kBackgroundColor = RGB(20, 22, 26);
    static constexpr COLORREF kPanelColor = RGB(30, 33, 39);
    static constexpr COLORREF kWellColor = RGB(16, 18, 22);
    static constexpr COLORREF kInputColor = RGB(23, 26, 31);
    static constexpr COLORREF kRaisedColor = RGB(47, 52, 61);
    static constexpr COLORREF kBorderColor = RGB(65, 71, 82);
    static constexpr COLORREF kSelectionColor = RGB(71, 63, 57);
    static constexpr COLORREF kAccentColor = RGB(214, 117, 48);
    static constexpr COLORREF kTextColor = RGB(225, 229, 234);
    static constexpr COLORREF kMutedTextColor = RGB(145, 153, 164);

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        ForgeWindow* self = reinterpret_cast<ForgeWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            self = static_cast<ForgeWindow*>(create->lpCreateParams);
            self->hwnd_ = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        return self != nullptr ? self->HandleMessage(message, wParam, lParam)
                               : DefWindowProcW(hwnd, message, wParam, lParam);
    }

    HWND CreateControl(const wchar_t* className,
                       const wchar_t* text,
                       DWORD style,
                       int id,
                       DWORD extendedStyle = 0) const {
        HWND control = CreateWindowExW(
            extendedStyle,
            className,
            text,
            WS_CHILD | WS_VISIBLE | style,
            0,
            0,
            100,
            30,
            hwnd_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            instance_,
            nullptr);
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont_), TRUE);
        return control;
    }

    HWND CreateButton(const wchar_t* text, const int id) const {
        return CreateControl(L"BUTTON", text, BS_OWNERDRAW | WS_TABSTOP, id);
    }

    static void ApplyDarkControlTheme(const HWND control) {
        if (control != nullptr) {
            SetWindowTheme(control, L"DarkMode_Explorer", nullptr);
        }
    }

    void CreateControls() {
        bodyFont_ = CreateFontW(
            -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        titleFont_ = CreateFontW(
            -27, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        sectionFont_ = CreateFontW(
            -14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        monoFont_ = CreateFontW(
            -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Mono");
        panelBrush_ = CreateSolidBrush(kPanelColor);
        listBrush_ = CreateSolidBrush(kWellColor);
        inputBrush_ = CreateSolidBrush(kInputColor);
        raisedBrush_ = CreateSolidBrush(kRaisedColor);
        borderBrush_ = CreateSolidBrush(kBorderColor);
        accentBrush_ = CreateSolidBrush(kAccentColor);
        selectionBrush_ = CreateSolidBrush(kSelectionColor);
        modelBrush_ = CreateSolidBrush(RGB(65, 113, 154));
        rigBrush_ = CreateSolidBrush(RGB(78, 132, 102));
        errorBrush_ = CreateSolidBrush(RGB(168, 68, 64));
        backgroundBrush_ = CreateSolidBrush(kBackgroundColor);

        title_ = CreateControl(L"STATIC", L"RAW IRON FORGE", SS_LEFT, 0);
        SendMessageW(title_, WM_SETFONT, reinterpret_cast<WPARAM>(titleFont_), TRUE);
        summary_ = CreateControl(L"STATIC", L"", SS_LEFT, 0);
        assetHeading_ = CreateControl(L"STATIC", L"ASSET LIBRARY", SS_LEFT, 0);
        viewportHeading_ =
            CreateControl(L"STATIC", L"3D WORKSPACE    RMB ORBIT  \u00B7  WHEEL ZOOM", SS_LEFT, 0);
        modelHeading_ = CreateControl(L"STATIC", L"MODEL OUTLINER", SS_LEFT, 0);
        transformHeading_ = CreateControl(L"STATIC", L"TRANSFORM", SS_LEFT, 0);
        detailHeading_ = CreateControl(L"STATIC", L"ASSET INSPECTOR", SS_LEFT, 0);
        for (const HWND heading :
             {assetHeading_, viewportHeading_, modelHeading_, transformHeading_, detailHeading_}) {
            SendMessageW(heading, WM_SETFONT, reinterpret_cast<WPARAM>(sectionFont_), TRUE);
        }
        refreshButton_ = CreateButton(L"Refresh", kRefresh);
        newModelButton_ = CreateButton(L"New Model", kNewPrimitiveModel);
        newRigButton_ = CreateButton(L"New Rig", kNewHumanoid);
        validateButton_ = CreateButton(L"Validate", kValidate);
        openButton_ = CreateButton(L"Open Source", kOpenSource);
        editorButton_ = CreateButton(L"Open Editor", kOpenInEditor);
        addPrimitiveButton_ = CreateButton(L"Add Primitive", kAddPrimitive);
        addGroupButton_ = CreateButton(L"Add Group", kAddGroup);
        bakeButton_ = CreateButton(L"Bake", kBakeModel);
        modelElement_ = CreateControl(
            L"LISTBOX",
            L"",
            LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS
                | WS_VSCROLL | WS_TABSTOP,
            kModelElement);
        transformMode_ = CreateControl(
            L"COMBOBOX",
            L"",
            CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
            kTransformMode);
        SendMessageW(transformMode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Position"));
        SendMessageW(transformMode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Rotation"));
        SendMessageW(transformMode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Scale"));
        SendMessageW(transformMode_, CB_SETCURSEL, 0, 0);
        transformX_ = CreateControl(L"EDIT", L"0", ES_AUTOHSCROLL | WS_TABSTOP, kTransformX, WS_EX_STATICEDGE);
        transformY_ = CreateControl(L"EDIT", L"0", ES_AUTOHSCROLL | WS_TABSTOP, kTransformY, WS_EX_STATICEDGE);
        transformZ_ = CreateControl(L"EDIT", L"0", ES_AUTOHSCROLL | WS_TABSTOP, kTransformZ, WS_EX_STATICEDGE);
        xLabel_ = CreateControl(L"STATIC", L"X", SS_CENTER, 0);
        yLabel_ = CreateControl(L"STATIC", L"Y", SS_CENTER, 0);
        zLabel_ = CreateControl(L"STATIC", L"Z", SS_CENTER, 0);
        applyTransformButton_ = CreateButton(L"Apply Transform", kApplyTransform);
        assetFilter_ = CreateControl(
            L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, kAssetFilter, WS_EX_STATICEDGE);
        SendMessageW(
            assetFilter_, EM_SETCUEBANNER, TRUE,
            reinterpret_cast<LPARAM>(L"Filter models, rigs, paths\u2026"));
        assetList_ = CreateControl(
            L"LISTBOX",
            L"",
            LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS
                | WS_VSCROLL | WS_TABSTOP,
            kAssetList);
        SendMessageW(assetList_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
        detail_ = CreateControl(
            L"EDIT",
            L"",
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_NOHIDESEL | WS_VSCROLL,
            0,
            WS_EX_STATICEDGE);
        status_ = CreateControl(
            L"STATIC",
            L"Ready. Build grouped primitives, cull internal faces, rig, bake, and hand off to the Editor.",
            SS_LEFT,
            0);

        const BOOL useDarkTitleBar = TRUE;
        constexpr DWORD kImmersiveDarkMode = 20;
        if (FAILED(DwmSetWindowAttribute(
                hwnd_, kImmersiveDarkMode, &useDarkTitleBar, sizeof(useDarkTitleBar)))) {
            constexpr DWORD kImmersiveDarkModeLegacy = 19;
            DwmSetWindowAttribute(
                hwnd_, kImmersiveDarkModeLegacy, &useDarkTitleBar, sizeof(useDarkTitleBar));
        }
        for (const HWND control :
             {assetFilter_, assetList_, modelElement_, transformMode_, transformX_, transformY_,
              transformZ_, detail_}) {
            ApplyDarkControlTheme(control);
        }
        SetTimer(hwnd_, kCatalogPollTimer, 50U, nullptr);
        RefreshCatalog();
    }

    void LayoutControls(int width, int height) {
        if (width < 200 || height < 200) {
            return;
        }
        constexpr int margin = 14;
        constexpr int gap = 10;
        constexpr int headerHeight = 66;
        constexpr int toolbarTop = 73;
        constexpr int toolbarHeight = 36;
        constexpr int contentTop = 123;
        constexpr int statusHeight = 32;
        const int contentBottom = (std::max)(contentTop + 120, height - statusHeight - 8);
        const int leftWidth = std::clamp(width * 22 / 100, 260, 330);
        const int rightWidth = std::clamp(width * 25 / 100, 310, 380);
        leftPane_ = RECT{margin, contentTop, margin + leftWidth, contentBottom};
        rightPane_ = RECT{width - margin - rightWidth, contentTop, width - margin, contentBottom};
        centerPane_ = RECT{leftPane_.right + gap, contentTop, rightPane_.left - gap, contentBottom};
        toolbarRect_ = RECT{0, headerHeight, width, contentTop - 5};

        MoveWindow(title_, margin, 11, width - margin * 2, 32, TRUE);
        MoveWindow(summary_, margin, 43, width - margin * 2, 19, TRUE);
        int buttonX = margin;
        const auto placeButton = [&](const HWND button, const int buttonWidth) {
            MoveWindow(button, buttonX, toolbarTop, buttonWidth, toolbarHeight, TRUE);
            buttonX += buttonWidth + 7;
        };
        placeButton(newModelButton_, 92);
        placeButton(addPrimitiveButton_, 108);
        placeButton(addGroupButton_, 88);
        placeButton(bakeButton_, 72);
        buttonX += 5;
        placeButton(newRigButton_, 78);
        placeButton(validateButton_, 82);
        placeButton(openButton_, 98);
        placeButton(editorButton_, 96);
        placeButton(refreshButton_, 76);

        constexpr int panePadding = 12;
        MoveWindow(
            assetHeading_, leftPane_.left + panePadding, leftPane_.top + 9,
            leftWidth - panePadding * 2, 20, TRUE);
        MoveWindow(
            assetFilter_, leftPane_.left + panePadding, leftPane_.top + 35,
            leftWidth - panePadding * 2, 30, TRUE);
        MoveWindow(
            assetList_, leftPane_.left + panePadding, leftPane_.top + 72,
            leftWidth - panePadding * 2, leftPane_.bottom - leftPane_.top - 84, TRUE);

        MoveWindow(
            viewportHeading_, centerPane_.left + panePadding, centerPane_.top + 9,
            centerPane_.right - centerPane_.left - panePadding * 2, 20, TRUE);
        const RECT newViewportBounds{
            centerPane_.left + 2,
            centerPane_.top + 36,
            centerPane_.right - 2,
            centerPane_.bottom - 2,
        };
        const bool viewportBoundsChanged = !EqualRect(&newViewportBounds, &viewportBounds_);
        viewportBounds_ = newViewportBounds;
        if (!viewportStarted_) {
            viewportStarted_ = viewport_.Start(hwnd_, viewportBounds_);
            if (!viewportStarted_) {
                SetWindowTextW(status_, Widen("3D viewport unavailable: " + viewport_.LastError()).c_str());
            }
        } else if (viewportBoundsChanged) {
            viewport_.SetBounds(viewportBounds_);
        }

        const int inspectorLeft = rightPane_.left + panePadding;
        const int inspectorWidth = rightPane_.right - rightPane_.left - panePadding * 2;
        const int paneHeight = rightPane_.bottom - rightPane_.top;
        const int elementHeight = std::clamp(paneHeight * 31 / 100, 150, 235);
        MoveWindow(modelHeading_, inspectorLeft, rightPane_.top + 9, inspectorWidth, 20, TRUE);
        MoveWindow(modelElement_, inspectorLeft, rightPane_.top + 35, inspectorWidth, elementHeight, TRUE);
        const int transformTop = rightPane_.top + 47 + elementHeight;
        MoveWindow(transformHeading_, inspectorLeft, transformTop, inspectorWidth, 20, TRUE);
        MoveWindow(transformMode_, inspectorLeft, transformTop + 25, inspectorWidth, 28, TRUE);
        const int axisTop = transformTop + 61;
        const int axisGap = 6;
        const int axisWidth = (inspectorWidth - axisGap * 2) / 3;
        const auto placeAxis = [&](const HWND label, const HWND edit, const int index) {
            const int left = inspectorLeft + index * (axisWidth + axisGap);
            MoveWindow(label, left, axisTop + 5, 15, 20, TRUE);
            MoveWindow(edit, left + 18, axisTop, axisWidth - 18, 28, TRUE);
        };
        placeAxis(xLabel_, transformX_, 0);
        placeAxis(yLabel_, transformY_, 1);
        placeAxis(zLabel_, transformZ_, 2);
        MoveWindow(applyTransformButton_, inspectorLeft, axisTop + 36, inspectorWidth, 30, TRUE);
        const int detailTop = axisTop + 79;
        MoveWindow(detailHeading_, inspectorLeft, detailTop, inspectorWidth, 20, TRUE);
        MoveWindow(
            detail_, inspectorLeft, detailTop + 27, inspectorWidth,
            (std::max)(30, static_cast<int>(rightPane_.bottom) - detailTop - 37), TRUE);
        MoveWindow(status_, margin, height - statusHeight + 5, width - margin * 2, 20, TRUE);
    }

    void RefreshCatalog(const fs::path& preferredSelection = {}) {
        fs::path selectedPath = preferredSelection;
        if (selectedPath.empty()) {
            if (const ri::forge::AssetEntry* selected = SelectedAsset(); selected != nullptr) {
                selectedPath = selected->absolutePath;
            }
        }
        catalogIndex_.Request(selectedPath);
        SetWindowTextW(status_, L"Updating the asset index in the background\u2026");
    }

    void PopulateAssetList(const fs::path& preferredSelection = {}) {
        fs::path selectedPath = preferredSelection;
        if (selectedPath.empty()) {
            if (const ri::forge::AssetEntry* selected = SelectedAsset(); selected != nullptr) {
                selectedPath = selected->absolutePath;
            }
        }
        visibleAssetIndices_ =
            ri::forge::FilterAssetCatalogIndices(catalog_, ReadControlTextUtf8(assetFilter_));
        SendMessageW(assetList_, WM_SETREDRAW, FALSE, 0);
        SendMessageW(assetList_, LB_RESETCONTENT, 0, 0);
        int preferredIndex = -1;
        for (std::size_t visibleIndex = 0; visibleIndex < visibleAssetIndices_.size(); ++visibleIndex) {
            const std::size_t catalogIndex = visibleAssetIndices_[visibleIndex];
            const ri::forge::AssetEntry& asset = catalog_.entries[catalogIndex];
            std::string prefix = "[MODEL] ";
            if (asset.kind == ri::forge::AssetKind::Rig) {
                prefix = asset.valid ? "[RIG]   " : "[RIG !] ";
            } else if (asset.kind == ri::forge::AssetKind::PrimitiveModel) {
                prefix = asset.valid ? "[FORGE] " : "[FORGE!] ";
            }
            const std::wstring label = Widen(prefix + asset.relativePath);
            SendMessageW(assetList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            if (!selectedPath.empty() && asset.absolutePath == selectedPath) {
                preferredIndex = static_cast<int>(visibleIndex);
            }
        }
        SendMessageW(assetList_, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(assetList_, nullptr, TRUE);
        if (!visibleAssetIndices_.empty()) {
            const int index = preferredIndex >= 0 ? preferredIndex : 0;
            SendMessageW(assetList_, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
        }

        const std::string summary = std::to_string(visibleAssetIndices_.size()) + " shown / "
            + std::to_string(catalog_.entries.size()) + " assets  ·  "
            + std::to_string(catalog_.primitiveModelCount) + " primitive models  ·  "
            + std::to_string(catalog_.modelCount) + " model sources  |  "
            + std::to_string(catalog_.rigCount) + " rigs  ·  "
            + std::to_string(catalog_.invalidPrimitiveModelCount + catalog_.invalidRigCount)
            + " invalid  ·  " + catalog_.sourceRoot.string();
        SetWindowTextW(summary_, Widen(summary).c_str());
        UpdateInspector();
    }

    void PollCatalogIndex() {
        std::optional<ri::forge::CatalogIndexResult> result = catalogIndex_.Poll();
        if (!result.has_value()) {
            return;
        }
        if (!result->error.empty()) {
            SetWindowTextW(
                status_,
                Widen("Asset index failed without replacing the current catalog: " + result->error)
                    .c_str());
            return;
        }
        catalog_ = std::move(result->catalog);
        PopulateAssetList(result->preferredSelection);
        const auto elapsed = static_cast<long long>(std::llround(result->elapsedMilliseconds));
        const std::string status = "Indexed " + std::to_string(catalog_.entries.size())
            + " Forge assets in " + std::to_string(elapsed)
            + " ms. Filtering is instant and does not rescan the workspace.";
        SetWindowTextW(status_, Widen(status).c_str());
    }

    const ri::forge::AssetEntry* SelectedAsset() const {
        if (assetList_ == nullptr) {
            return nullptr;
        }
        const LRESULT selection = SendMessageW(assetList_, LB_GETCURSEL, 0, 0);
        if (selection == LB_ERR || selection < 0
            || static_cast<std::size_t>(selection) >= visibleAssetIndices_.size()) {
            return nullptr;
        }
        const std::size_t catalogIndex = visibleAssetIndices_[static_cast<std::size_t>(selection)];
        return catalogIndex < catalog_.entries.size() ? &catalog_.entries[catalogIndex] : nullptr;
    }

    void UpdateInspector() {
        const ri::forge::AssetEntry* asset = SelectedAsset();
        if (asset == nullptr) {
            SetWindowTextW(detail_, L"No supported model or rig sources were found under Assets/Source.");
            EnableWindow(validateButton_, FALSE);
            EnableWindow(editorButton_, FALSE);
            EnableWindow(addPrimitiveButton_, FALSE);
            EnableWindow(addGroupButton_, FALSE);
            EnableWindow(bakeButton_, FALSE);
            RefreshModelControls(nullptr);
            Rebuild3DPreview(nullptr);
            return;
        }

        const char* kind = asset->kind == ri::forge::AssetKind::Rig
            ? "Raw Iron rig"
            : (asset->kind == ri::forge::AssetKind::PrimitiveModel
                   ? "Forge primitive model"
                   : "Model source");
        std::string text = std::string(kind) + "\r\n\r\n" + asset->relativePath + "\r\n\r\n" + asset->summary
            + "\r\n\r\nAbsolute path\r\n" + asset->absolutePath.string();
        if (asset->kind == ri::forge::AssetKind::ModelSource) {
            text += "\r\n\r\nForge keeps authoring sources separate from packaged runtime assets. "
                "Use Raw Iron's asset standardization path when the source is ready to ship.";
        } else if (asset->kind == ri::forge::AssetKind::Rig) {
            text += "\r\n\r\nRig documents define the portable rest skeleton used by the editor, import pipeline, "
                "animation retargeting, and runtime validation.";
        } else {
            text += "\r\n\r\nThis is an editable, grouped native primitive model. Add any primitive from the full "
                "engine catalog, group parts into pivots, bind groups or parts to rig bones, then bake a culled mesh "
                "or open the structured hierarchy in the Editor.";
        }
        SetWindowTextW(detail_, Widen(text).c_str());
        EnableWindow(validateButton_, TRUE);
        EnableWindow(editorButton_, TRUE);
        const BOOL modelSelected = asset->kind == ri::forge::AssetKind::PrimitiveModel && asset->valid;
        EnableWindow(addPrimitiveButton_, modelSelected);
        EnableWindow(addGroupButton_, modelSelected);
        EnableWindow(bakeButton_, modelSelected);
        RefreshModelControls(asset);
        Rebuild3DPreview(asset);
    }

    struct ModelElementRef {
        bool group = true;
        std::size_t index = 0;
    };

    static std::wstring FormatTransformValue(const float value) {
        wchar_t buffer[48]{};
        std::swprintf(buffer, std::size(buffer), L"%.5g", static_cast<double>(value));
        return buffer;
    }

    void RefreshModelControls(const ri::forge::AssetEntry* asset) {
        editableModel_.reset();
        editableModelPath_.clear();
        modelElements_.clear();
        SendMessageW(modelElement_, LB_RESETCONTENT, 0, 0);
        const bool enabled = asset != nullptr
            && asset->kind == ri::forge::AssetKind::PrimitiveModel
            && asset->valid;
        if (enabled) {
            editableModel_ = ri::content::LoadPrimitiveModelDocument(asset->absolutePath);
            editableModelPath_ = asset->absolutePath;
        }
        if (editableModel_.has_value()) {
            for (std::size_t index = 0; index < editableModel_->groups.size(); ++index) {
                const auto& group = editableModel_->groups[index];
                const std::wstring label = Widen("[GROUP] " + group.name + "  (" + group.id + ")");
                SendMessageW(modelElement_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
                modelElements_.push_back(ModelElementRef{.group = true, .index = index});
            }
            for (std::size_t index = 0; index < editableModel_->parts.size(); ++index) {
                const auto& part = editableModel_->parts[index];
                const std::wstring label =
                    Widen("[PART] " + part.name + "  <" + part.primitivePreset + ">");
                SendMessageW(modelElement_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
                modelElements_.push_back(ModelElementRef{.group = false, .index = index});
            }
            if (!modelElements_.empty()) {
                SendMessageW(modelElement_, LB_SETCURSEL, 0, 0);
            }
        }
        const BOOL controlsEnabled = editableModel_.has_value() ? TRUE : FALSE;
        EnableWindow(modelElement_, controlsEnabled);
        EnableWindow(transformMode_, controlsEnabled);
        EnableWindow(transformX_, controlsEnabled);
        EnableWindow(transformY_, controlsEnabled);
        EnableWindow(transformZ_, controlsEnabled);
        EnableWindow(applyTransformButton_, controlsEnabled);
        PopulateTransformFields();
    }

    ri::content::PrimitiveModelTransform* SelectedElementTransform() {
        if (!editableModel_.has_value()) {
            return nullptr;
        }
        const LRESULT selection = SendMessageW(modelElement_, LB_GETCURSEL, 0, 0);
        if (selection == LB_ERR || selection < 0
            || static_cast<std::size_t>(selection) >= modelElements_.size()) {
            return nullptr;
        }
        const ModelElementRef& element = modelElements_[static_cast<std::size_t>(selection)];
        if (element.group) {
            return element.index < editableModel_->groups.size()
                ? &editableModel_->groups[element.index].transform
                : nullptr;
        }
        return element.index < editableModel_->parts.size()
            ? &editableModel_->parts[element.index].transform
            : nullptr;
    }

    std::string SelectedTargetGroupId() const {
        if (!editableModel_.has_value()) {
            return "root";
        }
        const LRESULT selection = SendMessageW(modelElement_, LB_GETCURSEL, 0, 0);
        if (selection == LB_ERR || selection < 0
            || static_cast<std::size_t>(selection) >= modelElements_.size()) {
            return "root";
        }
        const ModelElementRef& element = modelElements_[static_cast<std::size_t>(selection)];
        if (element.group && element.index < editableModel_->groups.size()) {
            return editableModel_->groups[element.index].id;
        }
        if (!element.group && element.index < editableModel_->parts.size()
            && !editableModel_->parts[element.index].groupId.empty()) {
            return editableModel_->parts[element.index].groupId;
        }
        return "root";
    }

    void PopulateTransformFields() {
        const ri::content::PrimitiveModelTransform* transform = SelectedElementTransform();
        if (transform == nullptr) {
            SetWindowTextW(transformX_, L"");
            SetWindowTextW(transformY_, L"");
            SetWindowTextW(transformZ_, L"");
            return;
        }
        const LRESULT mode = SendMessageW(transformMode_, CB_GETCURSEL, 0, 0);
        const ri::content::DeclarativeVec3* value = &transform->translation;
        if (mode == 1) {
            value = &transform->rotationDegrees;
        } else if (mode == 2) {
            value = &transform->scale;
        }
        SetWindowTextW(transformX_, FormatTransformValue(value->x).c_str());
        SetWindowTextW(transformY_, FormatTransformValue(value->y).c_str());
        SetWindowTextW(transformZ_, FormatTransformValue(value->z).c_str());
    }

    static std::optional<float> ReadFiniteFloat(const HWND edit) {
        std::array<wchar_t, 96> text{};
        GetWindowTextW(edit, text.data(), static_cast<int>(text.size()));
        wchar_t* end = nullptr;
        const float value = std::wcstof(text.data(), &end);
        while (end != nullptr && *end == L' ') {
            ++end;
        }
        if (end == text.data() || (end != nullptr && *end != L'\0') || !std::isfinite(value)) {
            return std::nullopt;
        }
        return value;
    }

    void ApplySelectedTransform() {
        ri::content::PrimitiveModelTransform* transform = SelectedElementTransform();
        const auto x = ReadFiniteFloat(transformX_);
        const auto y = ReadFiniteFloat(transformY_);
        const auto z = ReadFiniteFloat(transformZ_);
        if (transform == nullptr || !x.has_value() || !y.has_value() || !z.has_value()) {
            MessageBoxW(
                hwnd_,
                L"Transform values must be finite numbers.",
                L"Forge transform",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const LRESULT mode = SendMessageW(transformMode_, CB_GETCURSEL, 0, 0);
        ri::content::DeclarativeVec3* value = &transform->translation;
        if (mode == 1) {
            value = &transform->rotationDegrees;
        } else if (mode == 2) {
            value = &transform->scale;
        }
        *value = {*x, *y, *z};
        if (!ri::content::SavePrimitiveModelDocument(editableModelPath_, *editableModel_)) {
            MessageBoxW(
                hwnd_,
                L"The transform would make the primitive model invalid. Scale components cannot be zero.",
                L"Forge transform",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const fs::path selection = editableModelPath_;
        SetWindowTextW(status_, L"Applied transform and refreshed the native 3D preview.");
        RefreshCatalog(selection);
    }

    void Rebuild3DPreview(const ri::forge::AssetEntry* asset, const bool force = false) {
        fs::path previewKey{};
        fs::file_time_type writeTime{};
        bool hasWriteTime = false;
        if (asset != nullptr) {
            previewKey = asset->absolutePath;
            std::error_code error{};
            writeTime = fs::last_write_time(previewKey, error);
            hasWriteTime = !error;
        }
        if (!force && previewInitialized_ && previewKey == previewedAssetPath_
            && hasWriteTime == previewHasWriteTime_
            && (!hasWriteTime || writeTime == previewedWriteTime_)) {
            return;
        }
        previewInitialized_ = true;
        previewedAssetPath_ = previewKey;
        previewedWriteTime_ = writeTime;
        previewHasWriteTime_ = hasWriteTime;
        if (asset == nullptr) {
            Apply3DPreview(ri::forge::BuildForgePreviewScene(
                {}, ri::forge::AssetKind::ModelSource));
            return;
        }
        previewBuilder_.Request(asset->absolutePath, asset->kind);
        SetWindowTextW(
            viewportHeading_,
            Widen("3D WORKSPACE  |  Loading " + asset->absolutePath.filename().string()
                  + " in the background...")
                .c_str());
    }

    void Apply3DPreview(ri::forge::ForgePreviewBuildResult result) {
        previewScene_ = std::move(result.scene);
        previewCamera_ = result.camera;
        previewOptions_.textureRoot = workspaceRoot_ / "Assets" / "Textures";
        previewOptions_.fogStrength = 0.2F;
        previewOptions_.orderedDither = false;
        viewport_.Publish(
            previewScene_,
            previewCamera_.cameraNode,
            previewOptions_,
            0.0,
            true);
        const auto elapsed = static_cast<long long>(std::llround(result.elapsedMilliseconds));
        const std::string heading = "3D WORKSPACE  |  " + result.status
            + (result.assetPath.empty()
                   ? std::string{}
                   : "  |  " + std::to_string(result.renderableNodeCount) + " renderables  |  "
                       + std::to_string(elapsed) + " ms")
            + "  |  RMB ORBIT  |  WHEEL ZOOM";
        SetWindowTextW(viewportHeading_, Widen(heading).c_str());
    }

    void PollPreviewBuilder() {
        std::optional<ri::forge::ForgePreviewBuildResult> result = previewBuilder_.Poll();
        if (!result.has_value() || result->assetPath != previewedAssetPath_) {
            return;
        }
        Apply3DPreview(std::move(*result));
    }

    void Orbit3DPreview(const int x, const int y) {
        if (!orbitDragging_) {
            return;
        }
        const int deltaX = x - lastOrbitPoint_.x;
        const int deltaY = y - lastOrbitPoint_.y;
        lastOrbitPoint_ = POINT{x, y};
        previewCamera_.orbit.yawDegrees += static_cast<float>(deltaX) * 0.45F;
        previewCamera_.orbit.pitchDegrees = std::clamp(
            previewCamera_.orbit.pitchDegrees + static_cast<float>(deltaY) * 0.35F,
            -85.0F,
            85.0F);
        ri::scene::SetOrbitCameraState(previewScene_, previewCamera_, previewCamera_.orbit);
        viewport_.Publish(
            previewScene_,
            previewCamera_.cameraNode,
            previewOptions_,
            0.0,
            false);
    }

    void Zoom3DPreview(const short wheelDelta) {
        const float scale = wheelDelta > 0 ? 0.88F : 1.14F;
        previewCamera_.orbit.distance =
            std::clamp(previewCamera_.orbit.distance * scale, 0.35F, 500.0F);
        ri::scene::SetOrbitCameraState(previewScene_, previewCamera_, previewCamera_.orbit);
        viewport_.Publish(
            previewScene_,
            previewCamera_.cameraNode,
            previewOptions_,
            0.0,
            false);
    }

    void CreatePrimitiveModel() {
        std::string error;
        const fs::path output = ri::forge::CreateUniquePrimitiveModel(workspaceRoot_, &error);
        if (output.empty()) {
            MessageBoxW(hwnd_, Widen(error).c_str(), L"Forge", MB_OK | MB_ICONERROR);
            return;
        }
        SetWindowTextW(status_, Widen("Created editable primitive model " + output.string()).c_str());
        RefreshCatalog(output);
    }

    void CreateHumanoidRig() {
        std::string error;
        const fs::path output = ri::forge::CreateUniqueHumanoidRig(workspaceRoot_, &error);
        if (output.empty()) {
            MessageBoxW(hwnd_, Widen(error).c_str(), L"Forge", MB_OK | MB_ICONERROR);
            return;
        }
        SetWindowTextW(status_, Widen("Created " + output.string()).c_str());
        RefreshCatalog(output);
    }

    void AddGroupToSelectedModel() {
        const ri::forge::AssetEntry* asset = SelectedAsset();
        if (asset == nullptr || asset->kind != ri::forge::AssetKind::PrimitiveModel) {
            return;
        }
        std::string groupId;
        std::string error;
        if (!ri::forge::AppendGroupToModel(
                asset->absolutePath,
                "Part Group",
                SelectedTargetGroupId(),
                {},
                &groupId,
                &error)) {
            MessageBoxW(hwnd_, Widen(error).c_str(), L"Forge", MB_OK | MB_ICONERROR);
            return;
        }
        SetWindowTextW(status_, Widen("Added group " + groupId + " to " + asset->relativePath).c_str());
        RefreshCatalog(asset->absolutePath);
    }

    void ShowPrimitiveMenuAndAdd() {
        const ri::forge::AssetEntry* asset = SelectedAsset();
        if (asset == nullptr || asset->kind != ri::forge::AssetKind::PrimitiveModel) {
            return;
        }
        HMENU menu = CreatePopupMenu();
        if (menu == nullptr) {
            return;
        }
        constexpr std::size_t kItemsPerMenu = 18U;
        for (std::size_t begin = 0; begin < ri::scene::kStructuralPrimitivePresets.size();
             begin += kItemsPerMenu) {
            HMENU submenu = CreatePopupMenu();
            const std::size_t end =
                (std::min)(begin + kItemsPerMenu, ri::scene::kStructuralPrimitivePresets.size());
            for (std::size_t index = begin; index < end; ++index) {
                const std::wstring label = Widen(ri::scene::kStructuralPrimitivePresets[index].label);
                AppendMenuW(
                    submenu,
                    MF_STRING,
                    static_cast<UINT_PTR>(kPrimitivePresetBase + static_cast<int>(index)),
                    label.c_str());
            }
            const std::string rangeLabel =
                std::string(ri::scene::kStructuralPrimitivePresets[begin].label) + " - "
                + ri::scene::kStructuralPrimitivePresets[end - 1U].label;
            AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(submenu), Widen(rangeLabel).c_str());
        }
        RECT buttonRect{};
        GetWindowRect(addPrimitiveButton_, &buttonRect);
        const int command = TrackPopupMenu(
            menu,
            TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
            buttonRect.left,
            buttonRect.bottom,
            0,
            hwnd_,
            nullptr);
        DestroyMenu(menu);
        if (command < kPrimitivePresetBase
            || static_cast<std::size_t>(command - kPrimitivePresetBase)
                >= ri::scene::kStructuralPrimitivePresets.size()) {
            return;
        }
        const auto& preset =
            ri::scene::kStructuralPrimitivePresets[static_cast<std::size_t>(command - kPrimitivePresetBase)];
        std::string partId;
        std::string error;
        if (!ri::forge::AppendPrimitiveToModel(
                asset->absolutePath,
                preset.label,
                SelectedTargetGroupId(),
                &partId,
                &error)) {
            MessageBoxW(hwnd_, Widen(error).c_str(), L"Forge", MB_OK | MB_ICONERROR);
            return;
        }
        SetWindowTextW(
            status_,
            Widen("Added " + std::string(preset.label) + " as " + partId).c_str());
        RefreshCatalog(asset->absolutePath);
    }

    void BakeSelectedModel() {
        const ri::forge::AssetEntry* asset = SelectedAsset();
        if (asset == nullptr || asset->kind != ri::forge::AssetKind::PrimitiveModel) {
            return;
        }
        const ri::forge::PrimitiveModelBakeSummary bake =
            ri::forge::BakePrimitiveModelAsset(asset->absolutePath);
        SetWindowTextW(status_, Widen(bake.summary).c_str());
        MessageBoxW(
            hwnd_,
            Widen(bake.summary + (bake.outputPath.empty()
                                     ? ""
                                     : "\r\n\r\nOutput: " + bake.outputPath.string())
                  + (bake.rigMapPath.empty()
                         ? ""
                         : "\r\nRig map: " + bake.rigMapPath.string())).c_str(),
            bake.valid ? L"Primitive model baked" : L"Primitive model bake failed",
            MB_OK | (bake.valid ? MB_ICONINFORMATION : MB_ICONWARNING));
        if (bake.valid) {
            RefreshCatalog(asset->absolutePath);
        }
    }

    void ValidateSelectedAsset() const {
        const ri::forge::AssetEntry* asset = SelectedAsset();
        if (asset == nullptr) {
            return;
        }
        bool valid = asset->valid;
        std::string summary = asset->summary;
        const wchar_t* title = valid ? L"Rig validation passed" : L"Rig validation failed";
        if (asset->kind == ri::forge::AssetKind::ModelSource) {
            const ri::forge::ModelSourceValidationReport report =
                ri::forge::ValidateModelSource(asset->absolutePath);
            valid = report.valid;
            summary = report.summary;
            title = report.valid ? L"Model import validation passed"
                                 : (report.runtimeImportable ? L"Model import validation failed"
                                                             : L"Model export required");
        }
        const std::wstring message = Widen(asset->relativePath + "\r\n\r\n" + summary);
        MessageBoxW(
            hwnd_,
            message.c_str(),
            title,
            MB_OK | (valid ? MB_ICONINFORMATION : MB_ICONWARNING));
    }

    void OpenSelectedSource() const {
        fs::path target = catalog_.sourceRoot;
        if (const ri::forge::AssetEntry* asset = SelectedAsset(); asset != nullptr) {
            target = asset->absolutePath;
        }
        const HINSTANCE result = ShellExecuteW(hwnd_, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32 && target != catalog_.sourceRoot) {
            ShellExecuteW(hwnd_, L"open", target.parent_path().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }

    void OpenSelectedInEditor() const {
        const ri::forge::AssetEntry* asset = SelectedAsset();
        if (asset == nullptr) {
            return;
        }
        const ri::content::AuthoringHandoffReport handoff = ri::content::BuildAuthoringHandoff({
            .workspaceRoot = workspaceRoot_,
            .assetPath = asset->absolutePath,
        });
        if (!handoff.valid) {
            const std::string issue = handoff.issues.empty() ? "Unknown handoff validation failure."
                                                             : handoff.issues.front();
            MessageBoxW(hwnd_, Widen(issue).c_str(), L"Forge to Editor", MB_OK | MB_ICONWARNING);
            return;
        }
        const fs::path editor = ResolveEditorExecutable();
        std::error_code error{};
        if (!fs::is_regular_file(editor, error)) {
            MessageBoxW(hwnd_, L"RawIron.Editor.exe was not found in this build configuration.",
                        L"Forge to Editor", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::wstring parameters = JoinArguments(handoff.editorArguments);
        const HINSTANCE result = ShellExecuteW(
            hwnd_, L"open", editor.c_str(), parameters.c_str(), workspaceRoot_.c_str(), SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32) {
            MessageBoxW(hwnd_, L"Windows could not launch RawIron.Editor.",
                        L"Forge to Editor", MB_OK | MB_ICONERROR);
        }
    }

    static std::wstring ReadListItemText(
        const HWND control,
        const UINT lengthMessage,
        const UINT textMessage,
        const std::size_t index) {
        const LRESULT length = SendMessageW(control, lengthMessage, index, 0);
        if (length <= 0) {
            return {};
        }
        std::wstring text(static_cast<std::size_t>(length + 1), L'\0');
        SendMessageW(
            control, textMessage, index, reinterpret_cast<LPARAM>(text.data()));
        text.resize(static_cast<std::size_t>(length));
        return text;
    }

    static bool IsPrimaryButton(const UINT id) {
        return id == kNewPrimitiveModel || id == kAddPrimitive || id == kBakeModel
            || id == kOpenInEditor;
    }

    void DrawOwnerDrawButton(const DRAWITEMSTRUCT& item) const {
        const bool enabled = (item.itemState & ODS_DISABLED) == 0U;
        const bool pressed = (item.itemState & ODS_SELECTED) != 0U;
        RECT bounds = item.rcItem;
        FillRect(item.hDC, &bounds, pressed ? listBrush_ : raisedBrush_);
        FrameRect(
            item.hDC,
            &bounds,
            IsPrimaryButton(item.CtlID) && enabled ? accentBrush_ : borderBrush_);
        if (IsPrimaryButton(item.CtlID) && enabled) {
            RECT marker{bounds.left + 1, bounds.top + 1, bounds.left + 4, bounds.bottom - 1};
            FillRect(item.hDC, &marker, accentBrush_);
        }

        std::array<wchar_t, 128> label{};
        GetWindowTextW(item.hwndItem, label.data(), static_cast<int>(label.size()));
        SetBkMode(item.hDC, TRANSPARENT);
        SetTextColor(item.hDC, enabled ? kTextColor : RGB(101, 107, 116));
        SelectObject(item.hDC, bodyFont_);
        RECT textBounds = bounds;
        if (pressed) {
            OffsetRect(&textBounds, 1, 1);
        }
        DrawTextW(
            item.hDC,
            label.data(),
            -1,
            &textBounds,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if ((item.itemState & ODS_FOCUS) != 0U) {
            InflateRect(&bounds, -4, -4);
            DrawFocusRect(item.hDC, &bounds);
        }
    }

    void DrawAssetListItem(const DRAWITEMSTRUCT& item) const {
        if (item.itemID == static_cast<UINT>(-1)
            || item.itemID >= visibleAssetIndices_.size()) {
            return;
        }
        const std::size_t catalogIndex = visibleAssetIndices_[item.itemID];
        if (catalogIndex >= catalog_.entries.size()) {
            return;
        }
        const ri::forge::AssetEntry& asset = catalog_.entries[catalogIndex];
        const bool selected = (item.itemState & ODS_SELECTED) != 0U;
        RECT bounds = item.rcItem;
        FillRect(item.hDC, &bounds, selected ? selectionBrush_ : listBrush_);
        RECT separator{bounds.left, bounds.bottom - 1, bounds.right, bounds.bottom};
        FillRect(item.hDC, &separator, borderBrush_);

        const wchar_t* tag = L"MODEL";
        HBRUSH tagBrush = modelBrush_;
        if (asset.kind == ri::forge::AssetKind::PrimitiveModel) {
            tag = L"FORGE";
            tagBrush = accentBrush_;
        } else if (asset.kind == ri::forge::AssetKind::Rig) {
            tag = L"RIG";
            tagBrush = rigBrush_;
        }
        if (!asset.valid) {
            tag = L"ERROR";
            tagBrush = errorBrush_;
        }
        RECT tagBounds{bounds.left + 7, bounds.top + 6, bounds.left + 60, bounds.bottom - 6};
        FillRect(item.hDC, &tagBounds, tagBrush);
        SetBkMode(item.hDC, TRANSPARENT);
        SetTextColor(item.hDC, RGB(245, 246, 248));
        SelectObject(item.hDC, sectionFont_);
        DrawTextW(
            item.hDC, tag, -1, &tagBounds,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT textBounds{bounds.left + 68, bounds.top, bounds.right - 7, bounds.bottom};
        SetTextColor(item.hDC, selected ? RGB(248, 239, 229) : kTextColor);
        SelectObject(item.hDC, monoFont_);
        const std::wstring path = Widen(asset.relativePath);
        DrawTextW(
            item.hDC,
            path.c_str(),
            static_cast<int>(path.size()),
            &textBounds,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        if ((item.itemState & ODS_FOCUS) != 0U) {
            InflateRect(&bounds, -2, -2);
            DrawFocusRect(item.hDC, &bounds);
        }
    }

    void DrawModelListItem(const DRAWITEMSTRUCT& item) const {
        if (item.itemID == static_cast<UINT>(-1)) {
            return;
        }
        const bool selected = (item.itemState & ODS_SELECTED) != 0U;
        RECT bounds = item.rcItem;
        FillRect(item.hDC, &bounds, selected ? selectionBrush_ : listBrush_);
        RECT separator{bounds.left, bounds.bottom - 1, bounds.right, bounds.bottom};
        FillRect(item.hDC, &separator, borderBrush_);
        RECT marker{bounds.left + 7, bounds.top + 7, bounds.left + 11, bounds.bottom - 7};
        const bool group =
            item.itemID < modelElements_.size() && modelElements_[item.itemID].group;
        FillRect(item.hDC, &marker, group ? accentBrush_ : modelBrush_);
        RECT textBounds{bounds.left + 18, bounds.top, bounds.right - 7, bounds.bottom};
        SetBkMode(item.hDC, TRANSPARENT);
        SetTextColor(item.hDC, selected ? RGB(248, 239, 229) : kTextColor);
        SelectObject(item.hDC, bodyFont_);
        const std::wstring text = ReadListItemText(
            item.hwndItem, LB_GETTEXTLEN, LB_GETTEXT, item.itemID);
        DrawTextW(
            item.hDC,
            text.c_str(),
            static_cast<int>(text.size()),
            &textBounds,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        if ((item.itemState & ODS_FOCUS) != 0U) {
            InflateRect(&bounds, -2, -2);
            DrawFocusRect(item.hDC, &bounds);
        }
    }

    void DrawTransformModeItem(const DRAWITEMSTRUCT& item) const {
        RECT bounds = item.rcItem;
        const bool selected = (item.itemState & ODS_SELECTED) != 0U;
        FillRect(item.hDC, &bounds, selected ? selectionBrush_ : inputBrush_);
        SetBkMode(item.hDC, TRANSPARENT);
        SetTextColor(item.hDC, kTextColor);
        SelectObject(item.hDC, bodyFont_);
        LRESULT index = item.itemID;
        if (index == static_cast<LRESULT>(-1)) {
            index = SendMessageW(item.hwndItem, CB_GETCURSEL, 0, 0);
        }
        const std::wstring text = index == CB_ERR
            ? std::wstring{}
            : ReadListItemText(
                  item.hwndItem, CB_GETLBTEXTLEN, CB_GETLBTEXT,
                  static_cast<std::size_t>(index));
        InflateRect(&bounds, -8, 0);
        DrawTextW(
            item.hDC,
            text.c_str(),
            static_cast<int>(text.size()),
            &bounds,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void DrawOwnerDrawItem(const DRAWITEMSTRUCT& item) const {
        if (item.CtlType == ODT_BUTTON) {
            DrawOwnerDrawButton(item);
        } else if (item.CtlID == kAssetList) {
            DrawAssetListItem(item);
        } else if (item.CtlID == kModelElement) {
            DrawModelListItem(item);
        } else if (item.CtlID == kTransformMode) {
            DrawTransformModeItem(item);
        }
    }

    void Paint() const {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd_, &paint);
        RECT client{};
        GetClientRect(hwnd_, &client);
        FillRect(dc, &client, backgroundBrush_);

        RECT header{0, 0, client.right, 66};
        FillRect(dc, &header, panelBrush_);
        RECT ember{0, 0, client.right, 4};
        FillRect(dc, &ember, accentBrush_);
        FillRect(dc, &toolbarRect_, raisedBrush_);
        RECT toolbarLine{toolbarRect_.left, toolbarRect_.bottom - 1, toolbarRect_.right, toolbarRect_.bottom};
        FillRect(dc, &toolbarLine, borderBrush_);

        for (const RECT pane : {leftPane_, centerPane_, rightPane_}) {
            RECT paneCopy = pane;
            FillRect(dc, &paneCopy, panelBrush_);
            FrameRect(dc, &paneCopy, borderBrush_);
        }
        RECT statusLine{0, client.bottom - 35, client.right, client.bottom - 34};
        FillRect(dc, &statusLine, borderBrush_);
        EndPaint(hwnd_, &paint);
    }

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
            case WM_CREATE:
                CreateControls();
                return 0;
            case WM_SIZE:
                LayoutControls(LOWORD(lParam), HIWORD(lParam));
                InvalidateRect(hwnd_, nullptr, TRUE);
                return 0;
            case WM_TIMER:
                if (wParam == kCatalogPollTimer) {
                    PollCatalogIndex();
                    PollPreviewBuilder();
                }
                return 0;
            case WM_RBUTTONDOWN: {
                const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                if (!viewportStarted_ || !PtInRect(&viewportBounds_, point)) {
                    return DefWindowProcW(hwnd_, message, wParam, lParam);
                }
                orbitDragging_ = true;
                lastOrbitPoint_ = point;
                SetCapture(hwnd_);
                return 0;
            }
            case WM_RBUTTONUP:
                orbitDragging_ = false;
                if (GetCapture() == hwnd_) {
                    ReleaseCapture();
                }
                return 0;
            case WM_CAPTURECHANGED:
                orbitDragging_ = false;
                return 0;
            case WM_MOUSEMOVE:
                Orbit3DPreview(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;
            case WM_MOUSEWHEEL: {
                POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ScreenToClient(hwnd_, &point);
                if (viewportStarted_ && PtInRect(&viewportBounds_, point)) {
                    Zoom3DPreview(GET_WHEEL_DELTA_WPARAM(wParam));
                    return 0;
                }
                return DefWindowProcW(hwnd_, message, wParam, lParam);
            }
            case WM_KEYDOWN:
                if (wParam == VK_F5) {
                    RefreshCatalog();
                    return 0;
                }
                if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && wParam == 'N') {
                    CreatePrimitiveModel();
                    return 0;
                }
                if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && wParam == 'S') {
                    if (IsWindowEnabled(applyTransformButton_)) {
                        ApplySelectedTransform();
                    }
                    return 0;
                }
                return DefWindowProcW(hwnd_, message, wParam, lParam);
            case WM_GETMINMAXINFO: {
                auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
                info->ptMinTrackSize.x = 1180;
                info->ptMinTrackSize.y = 720;
                return 0;
            }
            case WM_COMMAND: {
                const int id = LOWORD(wParam);
                const int notification = HIWORD(wParam);
                if (id == kRefresh) {
                    RefreshCatalog();
                } else if (id == kNewPrimitiveModel) {
                    CreatePrimitiveModel();
                } else if (id == kNewHumanoid) {
                    CreateHumanoidRig();
                } else if (id == kAddPrimitive) {
                    ShowPrimitiveMenuAndAdd();
                } else if (id == kAddGroup) {
                    AddGroupToSelectedModel();
                } else if (id == kBakeModel) {
                    BakeSelectedModel();
                } else if (id == kApplyTransform) {
                    if (IsWindowEnabled(applyTransformButton_)) {
                        ApplySelectedTransform();
                    }
                } else if (id == kFocusAssetFilter) {
                    SetFocus(assetFilter_);
                    SendMessageW(assetFilter_, EM_SETSEL, 0, -1);
                } else if (id == kModelElement && notification == LBN_SELCHANGE) {
                    PopulateTransformFields();
                } else if (id == kTransformMode && notification == CBN_SELCHANGE) {
                    PopulateTransformFields();
                } else if (id == kValidate) {
                    ValidateSelectedAsset();
                } else if (id == kOpenSource) {
                    OpenSelectedSource();
                } else if (id == kOpenInEditor) {
                    OpenSelectedInEditor();
                } else if (id == kAssetList && notification == LBN_SELCHANGE) {
                    UpdateInspector();
                } else if (id == kAssetList && notification == LBN_DBLCLK) {
                    OpenSelectedSource();
                } else if (id == kAssetFilter && notification == EN_CHANGE) {
                    PopulateAssetList();
                }
                return 0;
            }
            case WM_MEASUREITEM: {
                auto* item = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
                if (item->CtlID == kAssetList) {
                    item->itemHeight = 31;
                    return TRUE;
                }
                if (item->CtlID == kModelElement) {
                    item->itemHeight = 27;
                    return TRUE;
                }
                if (item->CtlID == kTransformMode) {
                    item->itemHeight = 25;
                    return TRUE;
                }
                break;
            }
            case WM_DRAWITEM: {
                const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
                DrawOwnerDrawItem(*item);
                return TRUE;
            }
            case WM_CTLCOLORSTATIC: {
                HDC dc = reinterpret_cast<HDC>(wParam);
                const HWND control = reinterpret_cast<HWND>(lParam);
                if (control == detail_) {
                    SetBkMode(dc, OPAQUE);
                    SetBkColor(dc, kInputColor);
                    SetTextColor(dc, kTextColor);
                    return reinterpret_cast<LRESULT>(inputBrush_);
                }
                SetBkMode(dc, TRANSPARENT);
                if (control == title_) {
                    SetTextColor(dc, RGB(238, 231, 220));
                } else if (control == summary_ || control == status_) {
                    SetTextColor(dc, kMutedTextColor);
                } else if (control == assetHeading_ || control == viewportHeading_
                           || control == modelHeading_ || control == transformHeading_
                           || control == detailHeading_ || control == xLabel_
                           || control == yLabel_ || control == zLabel_) {
                    SetTextColor(dc, control == xLabel_ || control == yLabel_ || control == zLabel_
                                         ? kAccentColor
                                         : RGB(190, 197, 206));
                } else {
                    SetTextColor(dc, kTextColor);
                }
                return reinterpret_cast<LRESULT>(
                    control == status_ ? backgroundBrush_ : panelBrush_);
            }
            case WM_CTLCOLORLISTBOX: {
                HDC dc = reinterpret_cast<HDC>(wParam);
                SetBkColor(dc, kWellColor);
                SetTextColor(dc, kTextColor);
                return reinterpret_cast<LRESULT>(listBrush_);
            }
            case WM_CTLCOLOREDIT: {
                HDC dc = reinterpret_cast<HDC>(wParam);
                SetBkColor(dc, kInputColor);
                SetTextColor(dc, kTextColor);
                return reinterpret_cast<LRESULT>(inputBrush_);
            }
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT:
                Paint();
                return 0;
            case WM_DESTROY:
                KillTimer(hwnd_, kCatalogPollTimer);
                viewport_.Stop();
                PostQuitMessage(0);
                return 0;
            case WM_NCDESTROY:
                if (bodyFont_ != nullptr) {
                    DeleteObject(bodyFont_);
                }
                if (titleFont_ != nullptr) {
                    DeleteObject(titleFont_);
                }
                if (sectionFont_ != nullptr) {
                    DeleteObject(sectionFont_);
                }
                if (monoFont_ != nullptr) {
                    DeleteObject(monoFont_);
                }
                if (panelBrush_ != nullptr) {
                    DeleteObject(panelBrush_);
                }
                if (listBrush_ != nullptr) {
                    DeleteObject(listBrush_);
                }
                if (inputBrush_ != nullptr) {
                    DeleteObject(inputBrush_);
                }
                if (raisedBrush_ != nullptr) {
                    DeleteObject(raisedBrush_);
                }
                if (borderBrush_ != nullptr) {
                    DeleteObject(borderBrush_);
                }
                if (accentBrush_ != nullptr) {
                    DeleteObject(accentBrush_);
                }
                if (selectionBrush_ != nullptr) {
                    DeleteObject(selectionBrush_);
                }
                if (modelBrush_ != nullptr) {
                    DeleteObject(modelBrush_);
                }
                if (rigBrush_ != nullptr) {
                    DeleteObject(rigBrush_);
                }
                if (errorBrush_ != nullptr) {
                    DeleteObject(errorBrush_);
                }
                if (backgroundBrush_ != nullptr) {
                    DeleteObject(backgroundBrush_);
                }
                SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
                return DefWindowProcW(hwnd_, message, wParam, lParam);
            default:
                return DefWindowProcW(hwnd_, message, wParam, lParam);
        }
        return DefWindowProcW(hwnd_, message, wParam, lParam);
    }

    fs::path workspaceRoot_;
    ri::forge::AsyncAssetCatalogIndex catalogIndex_;
    ri::forge::AsyncForgePreviewBuilder previewBuilder_;
    bool background_ = false;
    ri::forge::AssetCatalog catalog_{};
    std::vector<std::size_t> visibleAssetIndices_{};
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND title_ = nullptr;
    HWND summary_ = nullptr;
    HWND assetHeading_ = nullptr;
    HWND viewportHeading_ = nullptr;
    HWND modelHeading_ = nullptr;
    HWND transformHeading_ = nullptr;
    HWND refreshButton_ = nullptr;
    HWND newModelButton_ = nullptr;
    HWND newRigButton_ = nullptr;
    HWND validateButton_ = nullptr;
    HWND openButton_ = nullptr;
    HWND editorButton_ = nullptr;
    HWND addPrimitiveButton_ = nullptr;
    HWND addGroupButton_ = nullptr;
    HWND bakeButton_ = nullptr;
    HWND modelElement_ = nullptr;
    HWND transformMode_ = nullptr;
    HWND transformX_ = nullptr;
    HWND transformY_ = nullptr;
    HWND transformZ_ = nullptr;
    HWND xLabel_ = nullptr;
    HWND yLabel_ = nullptr;
    HWND zLabel_ = nullptr;
    HWND applyTransformButton_ = nullptr;
    HWND assetFilter_ = nullptr;
    HWND assetList_ = nullptr;
    HWND detailHeading_ = nullptr;
    HWND detail_ = nullptr;
    HWND status_ = nullptr;
    ri::editor::EditorVulkanViewport viewport_{};
    bool viewportStarted_ = false;
    RECT viewportBounds_{};
    RECT leftPane_{};
    RECT centerPane_{};
    RECT rightPane_{};
    RECT toolbarRect_{};
    ri::scene::Scene previewScene_{"Raw Iron Forge 3D Model"};
    ri::scene::OrbitCameraHandles previewCamera_{};
    ri::render::software::ScenePreviewOptions previewOptions_{};
    bool orbitDragging_ = false;
    POINT lastOrbitPoint_{};
    std::optional<ri::content::PrimitiveModelDocument> editableModel_{};
    fs::path editableModelPath_{};
    std::vector<ModelElementRef> modelElements_{};
    bool previewInitialized_ = false;
    fs::path previewedAssetPath_{};
    fs::file_time_type previewedWriteTime_{};
    bool previewHasWriteTime_ = false;
    HFONT bodyFont_ = nullptr;
    HFONT titleFont_ = nullptr;
    HFONT sectionFont_ = nullptr;
    HFONT monoFont_ = nullptr;
    HBRUSH panelBrush_ = nullptr;
    HBRUSH listBrush_ = nullptr;
    HBRUSH inputBrush_ = nullptr;
    HBRUSH raisedBrush_ = nullptr;
    HBRUSH borderBrush_ = nullptr;
    HBRUSH accentBrush_ = nullptr;
    HBRUSH selectionBrush_ = nullptr;
    HBRUSH modelBrush_ = nullptr;
    HBRUSH rigBrush_ = nullptr;
    HBRUSH errorBrush_ = nullptr;
    HBRUSH backgroundBrush_ = nullptr;
};

#endif

} // namespace

int main(int argc, char** argv) {
    try {
        const ri::core::CommandLine commandLine(argc, argv);
        const fs::path workspaceRoot = ResolveWorkspaceRoot(commandLine);
        if (commandLine.HasFlag("--list-primitives")) {
            for (const auto& preset : ri::scene::kStructuralPrimitivePresets) {
                std::cout << preset.label << "\n";
            }
            return 0;
        }
        if (commandLine.HasFlag("--create-primitive-model")) {
            std::string error;
            const fs::path output = ri::forge::CreateUniquePrimitiveModel(workspaceRoot, &error);
            if (output.empty()) {
                std::cerr << "Forge create failed: " << error << "\n";
                return 1;
            }
            std::cout << "Created primitive model: " << output.string() << "\n";
            return 0;
        }
        if (const auto model = commandLine.GetValue("--add-primitive");
            model.has_value() && !model->empty()) {
            const auto preset = commandLine.GetValue("--preset");
            if (!preset.has_value() || preset->empty()) {
                std::cerr << "Forge add failed: --preset is required.\n";
                return 1;
            }
            std::string partId;
            std::string error;
            if (!ri::forge::AppendPrimitiveToModel(
                    ResolveWorkspacePath(workspaceRoot, fs::path(*model)),
                    *preset,
                    commandLine.GetValue("--group").value_or("root"),
                    &partId,
                    &error)) {
                std::cerr << "Forge add failed: " << error << "\n";
                return 1;
            }
            std::cout << "Added primitive part: " << partId << "\n";
            return 0;
        }
        if (const auto model = commandLine.GetValue("--add-group");
            model.has_value() && !model->empty()) {
            std::string groupId;
            std::string error;
            if (!ri::forge::AppendGroupToModel(
                    ResolveWorkspacePath(workspaceRoot, fs::path(*model)),
                    commandLine.GetValue("--name").value_or("Part Group"),
                    commandLine.GetValue("--parent").value_or("root"),
                    commandLine.GetValue("--bone").value_or(""),
                    &groupId,
                    &error)) {
                std::cerr << "Forge group failed: " << error << "\n";
                return 1;
            }
            std::cout << "Added primitive group: " << groupId << "\n";
            return 0;
        }
        if (const auto model = commandLine.GetValue("--bake-primitive-model");
            model.has_value() && !model->empty()) {
            fs::path output{};
            if (const auto value = commandLine.GetValue("--output"); value.has_value() && !value->empty()) {
                output = ResolveWorkspacePath(workspaceRoot, fs::path(*value));
            }
            const ri::forge::PrimitiveModelBakeSummary bake =
                ri::forge::BakePrimitiveModelAsset(
                    ResolveWorkspacePath(workspaceRoot, fs::path(*model)),
                    output);
            std::cout << bake.summary << "\n";
        if (!bake.outputPath.empty()) {
            std::cout << "Output: " << bake.outputPath.string() << "\n";
        }
        if (!bake.rigMapPath.empty()) {
            std::cout << "Rig map: " << bake.rigMapPath.string() << "\n";
        }
            return bake.valid ? 0 : 1;
        }
        if (const std::optional<std::string> handoffAsset = commandLine.GetValue("--handoff-probe");
            handoffAsset.has_value() && !handoffAsset->empty()) {
            return PrintHandoffProbe(workspaceRoot, fs::path(*handoffAsset));
        }
        if (commandLine.HasFlag("--headless")) {
            const ri::forge::AssetCatalog catalog = ri::forge::ScanAssetCatalog(workspaceRoot);
            PrintHeadlessSummary(catalog);
            return 0;
        }

#if defined(_WIN32)
        if (HWND console = GetConsoleWindow(); console != nullptr) {
            ShowWindow(console, SW_HIDE);
        }
        ForgeWindow window(workspaceRoot, commandLine.HasFlag("--background"));
        return window.Run(GetModuleHandleW(nullptr));
#else
        PrintHeadlessSummary(catalog);
        return 0;
#endif
    } catch (const std::exception& exception) {
        std::cerr << "Forge failed: " << exception.what() << "\n";
        return 1;
    }
}
