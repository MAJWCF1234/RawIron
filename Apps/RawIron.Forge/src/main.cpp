#include "ForgeCatalog.h"
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
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <windowsx.h>
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
    kPrimitivePresetBase = 2000,
};

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
        newModelButton_ = CreateControl(L"BUTTON", L"New Primitive Model", BS_PUSHBUTTON, kNewPrimitiveModel);
        newRigButton_ = CreateControl(L"BUTTON", L"New Humanoid Rig", BS_PUSHBUTTON, kNewHumanoid);
        validateButton_ = CreateControl(L"BUTTON", L"Validate Asset", BS_PUSHBUTTON, kValidate);
        openButton_ = CreateControl(L"BUTTON", L"Open Source", BS_PUSHBUTTON, kOpenSource);
        editorButton_ = CreateControl(L"BUTTON", L"Open in Editor", BS_PUSHBUTTON, kOpenInEditor);
        addPrimitiveButton_ = CreateControl(L"BUTTON", L"Add Primitive", BS_PUSHBUTTON, kAddPrimitive);
        addGroupButton_ = CreateControl(L"BUTTON", L"Add Group", BS_PUSHBUTTON, kAddGroup);
        bakeButton_ = CreateControl(L"BUTTON", L"Bake Model", BS_PUSHBUTTON, kBakeModel);
        modelElement_ = CreateControl(
            L"COMBOBOX",
            L"",
            CBS_DROPDOWNLIST | WS_VSCROLL,
            kModelElement);
        transformMode_ = CreateControl(
            L"COMBOBOX",
            L"",
            CBS_DROPDOWNLIST,
            kTransformMode);
        SendMessageW(transformMode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Position"));
        SendMessageW(transformMode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Rotation"));
        SendMessageW(transformMode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Scale"));
        SendMessageW(transformMode_, CB_SETCURSEL, 0, 0);
        transformX_ = CreateControl(L"EDIT", L"0", ES_AUTOHSCROLL | WS_BORDER, kTransformX, WS_EX_CLIENTEDGE);
        transformY_ = CreateControl(L"EDIT", L"0", ES_AUTOHSCROLL | WS_BORDER, kTransformY, WS_EX_CLIENTEDGE);
        transformZ_ = CreateControl(L"EDIT", L"0", ES_AUTOHSCROLL | WS_BORDER, kTransformZ, WS_EX_CLIENTEDGE);
        applyTransformButton_ =
            CreateControl(L"BUTTON", L"Apply", BS_PUSHBUTTON, kApplyTransform);
        assetList_ = CreateControl(
            L"LISTBOX",
            L"",
            LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_BORDER,
            kAssetList,
            WS_EX_CLIENTEDGE);
        SendMessageW(assetList_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
        detailHeading_ = CreateControl(L"STATIC", L"ASSET INSPECTOR", SS_LEFT, 0);
        detail_ = CreateControl(L"STATIC", L"", SS_LEFT, 0);
        status_ = CreateControl(
            L"STATIC",
            L"Forge builds grouped primitive models, validates rigs, and hands editable assets to the Editor.",
            SS_LEFT,
            0);

        RefreshCatalog();
    }

    void LayoutControls(int width, int height) {
        constexpr int margin = 20;
        constexpr int gap = 12;
        constexpr int headerHeight = 72;
        constexpr int toolbarHeight = 32;
        constexpr int statusHeight = 30;
        const int secondToolbarY = headerHeight + toolbarHeight + 12;
        const int contentTop = secondToolbarY + toolbarHeight + 14;
        const int contentBottom = (std::max)(contentTop + 80, height - statusHeight - margin);
        const int listWidth = std::clamp(width * 45 / 100, 360, 540);
        const int detailLeft = margin + listWidth + gap;
        const int detailWidth = (std::max)(100, width - detailLeft - margin);

        MoveWindow(title_, margin, 14, width - margin * 2, 32, TRUE);
        MoveWindow(summary_, margin, 47, width - margin * 2, 22, TRUE);
        int buttonX = margin;
        MoveWindow(refreshButton_, buttonX, headerHeight + 4, 96, toolbarHeight, TRUE);
        buttonX += 104;
        MoveWindow(newModelButton_, buttonX, headerHeight + 4, 178, toolbarHeight, TRUE);
        buttonX += 186;
        MoveWindow(newRigButton_, buttonX, headerHeight + 4, 164, toolbarHeight, TRUE);
        buttonX += 172;
        MoveWindow(validateButton_, buttonX, headerHeight + 4, 122, toolbarHeight, TRUE);
        buttonX += 130;
        MoveWindow(openButton_, buttonX, headerHeight + 4, 116, toolbarHeight, TRUE);
        buttonX += 124;
        MoveWindow(editorButton_, buttonX, headerHeight + 4, 132, toolbarHeight, TRUE);

        buttonX = margin;
        MoveWindow(addPrimitiveButton_, buttonX, secondToolbarY, 142, toolbarHeight, TRUE);
        buttonX += 150;
        MoveWindow(addGroupButton_, buttonX, secondToolbarY, 116, toolbarHeight, TRUE);
        buttonX += 124;
        MoveWindow(bakeButton_, buttonX, secondToolbarY, 116, toolbarHeight, TRUE);

        MoveWindow(assetList_, margin, contentTop, listWidth, contentBottom - contentTop, TRUE);
        const int panelHeight = contentBottom - contentTop;
        const int viewportHeight = (std::max)(180, panelHeight * 58 / 100);
        viewportBounds_ = RECT{
            detailLeft + 2,
            contentTop + 2,
            detailLeft + detailWidth - 2,
            contentTop + viewportHeight,
        };
        if (!viewportStarted_) {
            viewportStarted_ = viewport_.Start(hwnd_, viewportBounds_);
            if (!viewportStarted_) {
                SetWindowTextW(status_, Widen("3D viewport unavailable: " + viewport_.LastError()).c_str());
            }
        } else {
            viewport_.SetBounds(viewportBounds_);
        }
        const int inspectorTop = contentTop + viewportHeight + 10;
        MoveWindow(detailHeading_, detailLeft + 16, inspectorTop, detailWidth - 32, 24, TRUE);
        MoveWindow(modelElement_, detailLeft + 16, inspectorTop + 28, detailWidth - 32, 220, TRUE);
        const int transformTop = inspectorTop + 62;
        const int editWidth = (std::max)(48, (detailWidth - 276) / 3);
        MoveWindow(transformMode_, detailLeft + 16, transformTop, 94, 180, TRUE);
        MoveWindow(transformX_, detailLeft + 118, transformTop, editWidth, 28, TRUE);
        MoveWindow(transformY_, detailLeft + 126 + editWidth, transformTop, editWidth, 28, TRUE);
        MoveWindow(transformZ_, detailLeft + 134 + editWidth * 2, transformTop, editWidth, 28, TRUE);
        MoveWindow(
            applyTransformButton_,
            detailLeft + 142 + editWidth * 3,
            transformTop,
            (std::max)(58, detailWidth - 158 - editWidth * 3),
            28,
            TRUE);
        MoveWindow(
            detail_,
            detailLeft + 16,
            inspectorTop + 98,
            detailWidth - 32,
            (std::max)(28, contentBottom - inspectorTop - 104),
            TRUE);
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
            std::string prefix = "[MODEL] ";
            if (asset.kind == ri::forge::AssetKind::Rig) {
                prefix = asset.valid ? "[RIG]   " : "[RIG !] ";
            } else if (asset.kind == ri::forge::AssetKind::PrimitiveModel) {
                prefix = asset.valid ? "[FORGE] " : "[FORGE!] ";
            }
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

        const std::string summary = std::to_string(catalog_.primitiveModelCount) + " primitive models  |  "
            + std::to_string(catalog_.modelCount) + " model sources  |  "
            + std::to_string(catalog_.rigCount) + " rigs  |  "
            + std::to_string(catalog_.invalidPrimitiveModelCount + catalog_.invalidRigCount)
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
        SendMessageW(modelElement_, CB_RESETCONTENT, 0, 0);
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
                SendMessageW(modelElement_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
                modelElements_.push_back(ModelElementRef{.group = true, .index = index});
            }
            for (std::size_t index = 0; index < editableModel_->parts.size(); ++index) {
                const auto& part = editableModel_->parts[index];
                const std::wstring label =
                    Widen("[PART] " + part.name + "  <" + part.primitivePreset + ">");
                SendMessageW(modelElement_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
                modelElements_.push_back(ModelElementRef{.group = false, .index = index});
            }
            if (!modelElements_.empty()) {
                SendMessageW(modelElement_, CB_SETCURSEL, 0, 0);
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
        const LRESULT selection = SendMessageW(modelElement_, CB_GETCURSEL, 0, 0);
        if (selection == CB_ERR || selection < 0
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
        const LRESULT selection = SendMessageW(modelElement_, CB_GETCURSEL, 0, 0);
        if (selection == CB_ERR || selection < 0
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

    void Rebuild3DPreview(const ri::forge::AssetEntry* asset) {
        previewScene_ = ri::scene::Scene{"Raw Iron Forge 3D Model"};
        const int previewRoot = previewScene_.CreateNode("Forge3DPreview");
        std::vector<int> frameNodes{};
        if (asset != nullptr && asset->kind == ri::forge::AssetKind::PrimitiveModel) {
            const auto document = ri::content::LoadPrimitiveModelDocument(asset->absolutePath);
            if (document.has_value()) {
                const ri::scene::PrimitiveModelInstantiationResult instantiated =
                    ri::scene::InstantiatePrimitiveModel(
                        previewScene_,
                        previewRoot,
                        *document,
                        asset->absolutePath.parent_path());
                if (instantiated.valid) {
                    frameNodes = instantiated.partNodes;
                }
            }
        }
        (void)ri::scene::AddGridHelper(
            previewScene_,
            ri::scene::GridHelperOptions{
                .nodeName = "ForgeGrid",
                .parent = previewRoot,
                .size = 12.0F,
            });
        (void)ri::scene::AddAxesHelper(
            previewScene_,
            ri::scene::AxesHelperOptions{
                .nodeName = "ForgeAxes",
                .parent = previewRoot,
                .axisLength = 1.5F,
            });
        (void)ri::scene::AddLightNode(
            previewScene_,
            ri::scene::LightNodeOptions{
                .nodeName = "ForgeKeyLight",
                .parent = previewRoot,
                .transform = ri::scene::Transform{
                    .rotationDegrees = {-35.0F, -35.0F, 0.0F},
                },
                .light = ri::scene::Light{
                    .name = "ForgeKeyLight",
                    .type = ri::scene::LightType::Directional,
                    .color = {1.0F, 0.92F, 0.82F},
                    .intensity = 2.0F,
                },
            });
        previewCamera_ = ri::scene::AddOrbitCamera(
            previewScene_,
            ri::scene::OrbitCameraOptions{
                .rigName = "ForgeOrbit",
                .parent = previewRoot,
                .camera = ri::scene::Camera{
                    .name = "ForgeCamera",
                    .fieldOfViewDegrees = 55.0F,
                    .nearClip = 0.02F,
                    .farClip = 500.0F,
                },
                .orbit = ri::scene::OrbitCameraState{
                    .distance = 6.0F,
                    .yawDegrees = 145.0F,
                    .pitchDegrees = -18.0F,
                },
            });
        if (!frameNodes.empty()) {
            (void)ri::scene::FrameNodesWithOrbitCamera(
                previewScene_, previewCamera_, frameNodes, 1.45F);
        }
        previewOptions_.textureRoot = workspaceRoot_ / "Assets" / "Textures";
        previewOptions_.fogStrength = 0.2F;
        previewOptions_.orderedDither = false;
        viewport_.Publish(
            previewScene_,
            previewCamera_.cameraNode,
            previewOptions_,
            0.0,
            true);
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
        RECT inspectorPanel{20 + listWidth + 12, 162, client.right - 20, client.bottom - 50};
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
            case WM_RBUTTONDOWN:
                orbitDragging_ = true;
                lastOrbitPoint_ = POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                SetCapture(hwnd_);
                return 0;
            case WM_RBUTTONUP:
                orbitDragging_ = false;
                if (GetCapture() == hwnd_) {
                    ReleaseCapture();
                }
                return 0;
            case WM_MOUSEMOVE:
                Orbit3DPreview(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;
            case WM_MOUSEWHEEL:
                Zoom3DPreview(GET_WHEEL_DELTA_WPARAM(wParam));
                return 0;
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
                    ApplySelectedTransform();
                } else if (id == kModelElement && notification == CBN_SELCHANGE) {
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
    HWND applyTransformButton_ = nullptr;
    HWND assetList_ = nullptr;
    HWND detailHeading_ = nullptr;
    HWND detail_ = nullptr;
    HWND status_ = nullptr;
    ri::editor::EditorVulkanViewport viewport_{};
    bool viewportStarted_ = false;
    RECT viewportBounds_{};
    ri::scene::Scene previewScene_{"Raw Iron Forge 3D Model"};
    ri::scene::OrbitCameraHandles previewCamera_{};
    ri::render::software::ScenePreviewOptions previewOptions_{};
    bool orbitDragging_ = false;
    POINT lastOrbitPoint_{};
    std::optional<ri::content::PrimitiveModelDocument> editableModel_{};
    fs::path editableModelPath_{};
    std::vector<ModelElementRef> modelElements_{};
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
