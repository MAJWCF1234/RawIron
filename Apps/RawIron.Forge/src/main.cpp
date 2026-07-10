#include "ForgeCatalog.h"

#include "RawIron/Core/CommandLine.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
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

void PrintHeadlessSummary(const ri::forge::AssetCatalog& catalog) {
    std::cout << "[Forge Ready]\n";
    std::cout << "Workspace: " << catalog.workspaceRoot.string() << "\n";
    std::cout << "Asset source: " << catalog.sourceRoot.string() << "\n";
    std::cout << "Model sources: " << catalog.modelCount << "\n";
    std::cout << "Rig assets: " << catalog.rigCount << "\n";
    std::cout << "Invalid rigs: " << catalog.invalidRigCount << "\n";
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

enum ControlId : int {
    kAssetList = 100,
    kRefresh = 101,
    kNewHumanoid = 102,
    kValidate = 103,
    kOpenSource = 104,
};

class ForgeWindow {
public:
    explicit ForgeWindow(fs::path workspaceRoot)
        : workspaceRoot_(std::move(workspaceRoot)) {}

    int Run(HINSTANCE instance) {
        instance_ = instance;
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
            0,
            windowClass.lpszClassName,
            L"Raw Iron Forge - Model & Rig Workbench",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1120,
            720,
            nullptr,
            nullptr,
            instance,
            this);
        if (hwnd_ == nullptr) {
            return 1;
        }

        ShowWindow(hwnd_, SW_SHOWDEFAULT);
        UpdateWindow(hwnd_);
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return static_cast<int>(message.wParam);
    }

private:
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

    void CreateControls() {
        bodyFont_ = CreateFontW(
            -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        titleFont_ = CreateFontW(
            -25, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        monoFont_ = CreateFontW(
            -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Mono");
        panelBrush_ = CreateSolidBrush(RGB(28, 30, 31));
        listBrush_ = CreateSolidBrush(RGB(17, 19, 20));
        backgroundBrush_ = CreateSolidBrush(RGB(12, 14, 15));

        title_ = CreateControl(L"STATIC", L"RAW IRON FORGE", SS_LEFT, 0);
        SendMessageW(title_, WM_SETFONT, reinterpret_cast<WPARAM>(titleFont_), TRUE);
        summary_ = CreateControl(L"STATIC", L"", SS_LEFT, 0);
        refreshButton_ = CreateControl(L"BUTTON", L"Refresh", BS_PUSHBUTTON, kRefresh);
        newRigButton_ = CreateControl(L"BUTTON", L"New Humanoid Rig", BS_PUSHBUTTON, kNewHumanoid);
        validateButton_ = CreateControl(L"BUTTON", L"Validate Asset", BS_PUSHBUTTON, kValidate);
        openButton_ = CreateControl(L"BUTTON", L"Open Source", BS_PUSHBUTTON, kOpenSource);
        assetList_ = CreateControl(
            L"LISTBOX",
            L"",
            LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_BORDER,
            kAssetList,
            WS_EX_CLIENTEDGE);
        SendMessageW(assetList_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
        detailHeading_ = CreateControl(L"STATIC", L"ASSET INSPECTOR", SS_LEFT, 0);
        detail_ = CreateControl(L"STATIC", L"", SS_LEFT, 0);
        status_ = CreateControl(L"STATIC", L"Forge indexes engine-supported model sources and Raw Iron rig assets.", SS_LEFT, 0);

        RefreshCatalog();
    }

    void LayoutControls(int width, int height) const {
        constexpr int margin = 20;
        constexpr int gap = 12;
        constexpr int headerHeight = 72;
        constexpr int toolbarHeight = 32;
        constexpr int statusHeight = 30;
        const int contentTop = headerHeight + toolbarHeight + 22;
        const int contentBottom = (std::max)(contentTop + 80, height - statusHeight - margin);
        const int listWidth = std::clamp(width * 45 / 100, 360, 540);
        const int detailLeft = margin + listWidth + gap;
        const int detailWidth = (std::max)(100, width - detailLeft - margin);

        MoveWindow(title_, margin, 14, width - margin * 2, 32, TRUE);
        MoveWindow(summary_, margin, 47, width - margin * 2, 22, TRUE);
        int buttonX = margin;
        MoveWindow(refreshButton_, buttonX, headerHeight + 4, 96, toolbarHeight, TRUE);
        buttonX += 104;
        MoveWindow(newRigButton_, buttonX, headerHeight + 4, 164, toolbarHeight, TRUE);
        buttonX += 172;
        MoveWindow(validateButton_, buttonX, headerHeight + 4, 122, toolbarHeight, TRUE);
        buttonX += 130;
        MoveWindow(openButton_, buttonX, headerHeight + 4, 116, toolbarHeight, TRUE);

        MoveWindow(assetList_, margin, contentTop, listWidth, contentBottom - contentTop, TRUE);
        MoveWindow(detailHeading_, detailLeft + 16, contentTop + 14, detailWidth - 32, 24, TRUE);
        MoveWindow(detail_, detailLeft + 16, contentTop + 48, detailWidth - 32, contentBottom - contentTop - 64, TRUE);
        MoveWindow(status_, margin, height - statusHeight, width - margin * 2, 22, TRUE);
    }

    void RefreshCatalog(const fs::path& preferredSelection = {}) {
        fs::path selectedPath = preferredSelection;
        if (selectedPath.empty()) {
            if (const ri::forge::AssetEntry* selected = SelectedAsset(); selected != nullptr) {
                selectedPath = selected->absolutePath;
            }
        }

        catalog_ = ri::forge::ScanAssetCatalog(workspaceRoot_);
        SendMessageW(assetList_, WM_SETREDRAW, FALSE, 0);
        SendMessageW(assetList_, LB_RESETCONTENT, 0, 0);
        int preferredIndex = -1;
        for (std::size_t index = 0; index < catalog_.entries.size(); ++index) {
            const ri::forge::AssetEntry& asset = catalog_.entries[index];
            const std::string prefix = asset.kind == ri::forge::AssetKind::Rig
                ? (asset.valid ? "[RIG]   " : "[RIG !] ")
                : "[MODEL] ";
            const std::wstring label = Widen(prefix + asset.relativePath);
            SendMessageW(assetList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            if (!selectedPath.empty() && asset.absolutePath == selectedPath) {
                preferredIndex = static_cast<int>(index);
            }
        }
        SendMessageW(assetList_, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(assetList_, nullptr, TRUE);
        if (!catalog_.entries.empty()) {
            const int index = preferredIndex >= 0 ? preferredIndex : 0;
            SendMessageW(assetList_, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
        }

        const std::string summary = std::to_string(catalog_.modelCount) + " model sources  |  "
            + std::to_string(catalog_.rigCount) + " rigs  |  " + std::to_string(catalog_.invalidRigCount)
            + " invalid  |  " + catalog_.sourceRoot.string();
        SetWindowTextW(summary_, Widen(summary).c_str());
        UpdateInspector();
    }

    const ri::forge::AssetEntry* SelectedAsset() const {
        if (assetList_ == nullptr) {
            return nullptr;
        }
        const LRESULT selection = SendMessageW(assetList_, LB_GETCURSEL, 0, 0);
        if (selection == LB_ERR || selection < 0 || static_cast<std::size_t>(selection) >= catalog_.entries.size()) {
            return nullptr;
        }
        return &catalog_.entries[static_cast<std::size_t>(selection)];
    }

    void UpdateInspector() const {
        const ri::forge::AssetEntry* asset = SelectedAsset();
        if (asset == nullptr) {
            SetWindowTextW(detail_, L"No supported model or rig sources were found under Assets/Source.");
            EnableWindow(validateButton_, FALSE);
            return;
        }

        const char* kind = asset->kind == ri::forge::AssetKind::Rig ? "Raw Iron rig" : "Model source";
        std::string text = std::string(kind) + "\r\n\r\n" + asset->relativePath + "\r\n\r\n" + asset->summary
            + "\r\n\r\nAbsolute path\r\n" + asset->absolutePath.string();
        if (asset->kind == ri::forge::AssetKind::ModelSource) {
            text += "\r\n\r\nForge keeps authoring sources separate from packaged runtime assets. "
                "Use Raw Iron's asset standardization path when the source is ready to ship.";
        } else {
            text += "\r\n\r\nRig documents define the portable rest skeleton used by the editor, import pipeline, "
                "animation retargeting, and runtime validation.";
        }
        SetWindowTextW(detail_, Widen(text).c_str());
        EnableWindow(validateButton_, TRUE);
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

    void Paint() const {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd_, &paint);
        RECT client{};
        GetClientRect(hwnd_, &client);
        FillRect(dc, &client, backgroundBrush_);

        RECT header{0, 0, client.right, 72};
        FillRect(dc, &header, panelBrush_);
        RECT ember{0, 0, client.right, 4};
        HBRUSH emberBrush = CreateSolidBrush(RGB(220, 119, 42));
        FillRect(dc, &ember, emberBrush);
        DeleteObject(emberBrush);

        const int listWidth = std::clamp(static_cast<int>(client.right) * 45 / 100, 360, 540);
        RECT inspectorPanel{20 + listWidth + 12, 126, client.right - 20, client.bottom - 50};
        FillRect(dc, &inspectorPanel, panelBrush_);
        FrameRect(dc, &inspectorPanel, GetSysColorBrush(COLOR_3DSHADOW));
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
            case WM_COMMAND: {
                const int id = LOWORD(wParam);
                const int notification = HIWORD(wParam);
                if (id == kRefresh) {
                    RefreshCatalog();
                } else if (id == kNewHumanoid) {
                    CreateHumanoidRig();
                } else if (id == kValidate) {
                    ValidateSelectedAsset();
                } else if (id == kOpenSource) {
                    OpenSelectedSource();
                } else if (id == kAssetList && notification == LBN_SELCHANGE) {
                    UpdateInspector();
                } else if (id == kAssetList && notification == LBN_DBLCLK) {
                    OpenSelectedSource();
                }
                return 0;
            }
            case WM_CTLCOLORSTATIC: {
                HDC dc = reinterpret_cast<HDC>(wParam);
                SetBkMode(dc, TRANSPARENT);
                SetTextColor(dc, RGB(225, 219, 199));
                return reinterpret_cast<LRESULT>(backgroundBrush_);
            }
            case WM_CTLCOLORLISTBOX: {
                HDC dc = reinterpret_cast<HDC>(wParam);
                SetBkColor(dc, RGB(17, 19, 20));
                SetTextColor(dc, RGB(220, 216, 200));
                return reinterpret_cast<LRESULT>(listBrush_);
            }
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT:
                Paint();
                return 0;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            case WM_NCDESTROY:
                if (bodyFont_ != nullptr) {
                    DeleteObject(bodyFont_);
                }
                if (titleFont_ != nullptr) {
                    DeleteObject(titleFont_);
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
                if (backgroundBrush_ != nullptr) {
                    DeleteObject(backgroundBrush_);
                }
                SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
                return DefWindowProcW(hwnd_, message, wParam, lParam);
            default:
                return DefWindowProcW(hwnd_, message, wParam, lParam);
        }
    }

    fs::path workspaceRoot_;
    ri::forge::AssetCatalog catalog_{};
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND title_ = nullptr;
    HWND summary_ = nullptr;
    HWND refreshButton_ = nullptr;
    HWND newRigButton_ = nullptr;
    HWND validateButton_ = nullptr;
    HWND openButton_ = nullptr;
    HWND assetList_ = nullptr;
    HWND detailHeading_ = nullptr;
    HWND detail_ = nullptr;
    HWND status_ = nullptr;
    HFONT bodyFont_ = nullptr;
    HFONT titleFont_ = nullptr;
    HFONT monoFont_ = nullptr;
    HBRUSH panelBrush_ = nullptr;
    HBRUSH listBrush_ = nullptr;
    HBRUSH backgroundBrush_ = nullptr;
};

#endif

} // namespace

int main(int argc, char** argv) {
    try {
        const ri::core::CommandLine commandLine(argc, argv);
        const fs::path workspaceRoot = ResolveWorkspaceRoot(commandLine);
        const ri::forge::AssetCatalog catalog = ri::forge::ScanAssetCatalog(workspaceRoot);
        if (commandLine.HasFlag("--headless")) {
            PrintHeadlessSummary(catalog);
            return 0;
        }

#if defined(_WIN32)
        if (HWND console = GetConsoleWindow(); console != nullptr) {
            ShowWindow(console, SW_HIDE);
        }
        ForgeWindow window(workspaceRoot);
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
