#include "ForgeCatalog.h"
#include "ForgeCatalogIndex.h"
#include "ForgePreviewBuilder.h"
#include "EditorVulkanViewport.h"

#include "RawIron/Core/CommandLine.h"
#include "RawIron/Content/AuthoringHandoff.h"
#include "RawIron/Content/NativeAnimationDocument.h"
#include "RawIron/Content/NativeSculptDocument.h"
#include "RawIron/Content/PrimitiveModelDocument.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/NativeAnimation.h"
#include "RawIron/Scene/NativeSculpt.h"
#include "RawIron/Scene/Animation.h"
#include "RawIron/Scene/PrimitiveModelBake.h"
#include "RawIron/Scene/Raycast.h"
#include "RawIron/Scene/HumanoidRigNames.h"
#include "RawIron/Scene/RigAuthoring.h"
#include "RawIron/Scene/StructuralPrimitivePresets.h"
#include "RawIron/Scene/Transform.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Math/Vec3.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
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
    std::cout << "Sculpt assets: " << catalog.sculptCount << "\n";
    std::cout << "Rig assets: " << catalog.rigCount << "\n";
    std::cout << "Animation clips: " << catalog.animationCount << "\n";
    std::cout << "Invalid primitive models: " << catalog.invalidPrimitiveModelCount << "\n";
    std::cout << "Invalid sculpts: " << catalog.invalidSculptCount << "\n";
    std::cout << "Invalid rigs: " << catalog.invalidRigCount << "\n";
    std::cout << "Invalid animations: " << catalog.invalidAnimationCount << "\n";
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
    kNewSculpt = 118,
    kNewAnimation = 119,
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
    kBayStock = 130,
    kBayClay = 131,
    kBayLook = 132,
    kBayMotion = 133,
    kApplyLook = 134,
    kAlbedoR = 135,
    kAlbedoG = 136,
    kAlbedoB = 137,
    kRoughness = 138,
    kMetallic = 139,
    kAlbedoTexture = 140,
    kAnimPlay = 141,
    kAnimStop = 142,
    kAnimKey = 143,
    kAnimTime = 144,
    kBoneList = 145,
    kAnimScrub = 146,
    kAnimLoop = 147,
    kAnimKeyBone = 148,
    kBindRig = 149,
    kBindBone = 150,
    kFloodBone = 151,
    kClipList = 152,
    kAnimDuration = 153,
    kAnimTrim = 154,
    kAnimRootMotion = 155,
    kAnimIn = 156,
    kAnimOut = 157,
    kAnimStampIn = 158,
    kAnimStampOut = 159,
    kAnimEventName = 160,
    kAnimAddEvent = 161,
    kAnimDelEvent = 162,
    kAnimEventList = 163,
    kNormWeights = 164,
    kPruneWeights = 165,
    kMirrorWeights = 166,
    kMirrorRest = 167,
    kBoneRename = 168,
    kBoneRenameEdit = 169,
    kAddChildBone = 170,
    kDeleteBone = 171,
    kBoneParent = 172,
    kReparentBone = 173,
    kAddSlotBone = 174,
    kAuditWeights = 175,
    kDuplicatePart = 176,
    kDeletePart = 177,
    kDeleteAnimKey = 178,
    kDuplicateClip = 179,
    kClearWeights = 180,
    kUnbindRig = 181,
    kAnimPrevKey = 182,
    kAnimNextKey = 183,
    kResetBone = 184,
    kResetPose = 185,
    kMirrorPose = 186,
    kSnapAnimKey = 187,
    kDeleteClip = 188,
    kDuplicateSculpt = 189,
    kDeleteSculpt = 190,
    kRestAnimKey = 191,
    kMirrorAnimKeys = 192,
    kFloodUnbound = 193,
    kAnimHalfSpeed = 194,
    kAnimDoubleSpeed = 195,
    kDuplicateModel = 196,
    kDeleteModel = 197,
    kDuplicateRig = 198,
    kDeleteRig = 199,
    kAnimAlignStart = 201,
    kAnimFitDuration = 202,
    kAnimNudgeBack = 203,
    kAnimNudgeForward = 204,
    kCopyAnimTrack = 205,
    kPasteAnimTrack = 206,
    kRenameDisplay = 207,
    kDisplayNameEdit = 208,
    kAnimPlayheadToZero = 209,
    kClearAnimTrack = 210,
    kClearAnimKeys = 211,
    kTransferWeights = 212,
    kSwapWeights = 213,
    kDedupAnimKeys = 214,
    kAnimPrevEvent = 215,
    kAnimNextEvent = 216,
    kAnimRenameEvent = 217,
    kAnimEventToTime = 218,
    kInvertWeights = 219,
    kAnimDupEvent = 220,
    kQuantizeAnimKeys = 221,
    kHalveBoneWeights = 222,
    kDoubleBoneWeights = 223,
    kSmoothWeights = 224,
    kStripAnimTracks = 225,
    kClearAnimEvents = 226,
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
            .dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES | ICC_BAR_CLASSES,
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
            L"Raw Iron Forge",
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
    static constexpr COLORREF kBackgroundColor = RGB(10, 11, 10);
    static constexpr COLORREF kPanelColor = RGB(22, 21, 18);
    static constexpr COLORREF kWellColor = RGB(8, 9, 8);
    static constexpr COLORREF kInputColor = RGB(16, 17, 15);
    static constexpr COLORREF kRaisedColor = RGB(34, 33, 28);
    static constexpr COLORREF kBorderColor = RGB(48, 50, 42);
    static constexpr COLORREF kSelectionColor = RGB(32, 40, 24);
    static constexpr COLORREF kAccentColor = RGB(168, 186, 92);
    static constexpr COLORREF kTextColor = RGB(226, 220, 204);
    static constexpr COLORREF kMutedTextColor = RGB(126, 122, 108);

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
            -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        titleFont_ = CreateFontW(
            -18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Mono");
        sectionFont_ = CreateFontW(
            -11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        monoFont_ = CreateFontW(
            -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Mono");
        panelBrush_ = CreateSolidBrush(kPanelColor);
        listBrush_ = CreateSolidBrush(kWellColor);
        inputBrush_ = CreateSolidBrush(kInputColor);
        raisedBrush_ = CreateSolidBrush(kRaisedColor);
        borderBrush_ = CreateSolidBrush(kBorderColor);
        accentBrush_ = CreateSolidBrush(kAccentColor);
        selectionBrush_ = CreateSolidBrush(kSelectionColor);
        modelBrush_ = CreateSolidBrush(RGB(78, 118, 108));
        clayBrush_ = CreateSolidBrush(RGB(176, 92, 58));
        rigBrush_ = CreateSolidBrush(RGB(92, 118, 148));
        motionBrush_ = CreateSolidBrush(RGB(168, 186, 92));
        errorBrush_ = CreateSolidBrush(RGB(156, 58, 46));
        backgroundBrush_ = CreateSolidBrush(kBackgroundColor);

        title_ = CreateControl(L"STATIC", L"FORGE", SS_LEFT, 0);
        SendMessageW(title_, WM_SETFONT, reinterpret_cast<WPARAM>(titleFont_), TRUE);
        summary_ = CreateControl(L"STATIC", L"", SS_LEFT, 0);
        bayStock_ = CreateButton(L"STOCK", kBayStock);
        bayClay_ = CreateButton(L"CLAY", kBayClay);
        bayLook_ = CreateButton(L"LOOK", kBayLook);
        bayMotion_ = CreateButton(L"MOTION", kBayMotion);
        assetHeading_ = CreateControl(L"STATIC", L"STOCKPILE", SS_LEFT, 0);
        viewportHeading_ =
            CreateControl(L"STATIC", L"HEARTH    RMB ORBIT  ·  MMB PAN  ·  F FRAME", SS_LEFT, 0);
        modelHeading_ = CreateControl(L"STATIC", L"OUTLINER", SS_LEFT, 0);
        transformHeading_ = CreateControl(L"STATIC", L"PLACE", SS_LEFT, 0);
        lookHeading_ = CreateControl(L"STATIC", L"SURFACE", SS_LEFT, 0);
        motionHeading_ = CreateControl(L"STATIC", L"MOTION", SS_LEFT, 0);
        clipHeading_ = CreateControl(L"STATIC", L"CLIP", SS_LEFT, 0);
        detailHeading_ = CreateControl(L"STATIC", L"DOSSIER", SS_LEFT, 0);
        displayNameLabel_ = CreateControl(L"STATIC", L"Name", SS_LEFT, 0);
        displayNameEdit_ = CreateControl(
            L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, kDisplayNameEdit, WS_EX_STATICEDGE);
        renameDisplayButton_ = CreateButton(L"Rename", kRenameDisplay);
        for (const HWND heading :
             {assetHeading_, viewportHeading_, modelHeading_, transformHeading_, lookHeading_,
              motionHeading_, clipHeading_, detailHeading_}) {
            SendMessageW(heading, WM_SETFONT, reinterpret_cast<WPARAM>(sectionFont_), TRUE);
        }
        refreshButton_ = CreateButton(L"Refresh", kRefresh);
        newModelButton_ = CreateButton(L"New Stock", kNewPrimitiveModel);
        duplicateModelButton_ = CreateButton(L"Dup Stock", kDuplicateModel);
        deleteModelButton_ = CreateButton(L"Del Stock", kDeleteModel);
        newSculptButton_ = CreateButton(L"New Clay", kNewSculpt);
        duplicateSculptButton_ = CreateButton(L"Dup Clay", kDuplicateSculpt);
        deleteSculptButton_ = CreateButton(L"Del Clay", kDeleteSculpt);
        newRigButton_ = CreateButton(L"New Rig", kNewHumanoid);
        duplicateRigButton_ = CreateButton(L"Dup Rig", kDuplicateRig);
        deleteRigButton_ = CreateButton(L"Del Rig", kDeleteRig);
        newAnimButton_ = CreateButton(L"New Clip", kNewAnimation);
        validateButton_ = CreateButton(L"Check", kValidate);
        openButton_ = CreateButton(L"Reveal", kOpenSource);
        editorButton_ = CreateButton(L"To Editor", kOpenInEditor);
        addPrimitiveButton_ = CreateButton(L"Add Solid", kAddPrimitive);
        addGroupButton_ = CreateButton(L"Add Pivot", kAddGroup);
        duplicatePartButton_ = CreateButton(L"Dup", kDuplicatePart);
        deletePartButton_ = CreateButton(L"Del", kDeletePart);
        bakeButton_ = CreateButton(L"Bake", kBakeModel);
        bindRigButton_ = CreateButton(L"Bind Rig", kBindRig);
        unbindRigButton_ = CreateButton(L"Unbind", kUnbindRig);
        bindBoneButton_ = CreateButton(L"Bind Bone", kBindBone);
        floodBoneButton_ = CreateButton(L"Flood Bone", kFloodBone);
        floodUnboundButton_ = CreateButton(L"Flood Free", kFloodUnbound);
        transferWeightsButton_ = CreateButton(L"Xfer→", kTransferWeights);
        swapWeightsButton_ = CreateButton(L"Swap↔", kSwapWeights);
        normWeightsButton_ = CreateButton(L"Norm", kNormWeights);
        pruneWeightsButton_ = CreateButton(L"Prune", kPruneWeights);
        mirrorWeightsButton_ = CreateButton(L"Mir X", kMirrorWeights);
        auditWeightsButton_ = CreateButton(L"Audit", kAuditWeights);
        invertWeightsButton_ = CreateButton(L"Inv", kInvertWeights);
        halveWeightsButton_ = CreateButton(L"½W", kHalveBoneWeights);
        doubleWeightsButton_ = CreateButton(L"×2W", kDoubleBoneWeights);
        softWeightsButton_ = CreateButton(L"Soft", kSmoothWeights);
        clearWeightsButton_ = CreateButton(L"Clear", kClearWeights);
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
        applyTransformButton_ = CreateButton(L"Set Place", kApplyTransform);
        mirrorRestButton_ = CreateButton(L"Mir Rest", kMirrorRest);
        boneRenameLabel_ = CreateControl(L"STATIC", L"Name", SS_LEFT, 0);
        boneRenameEdit_ = CreateControl(
            L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, kBoneRenameEdit, WS_EX_STATICEDGE);
        boneRenameButton_ = CreateButton(L"Rename", kBoneRename);
        addChildBoneButton_ = CreateButton(L"Add Child", kAddChildBone);
        deleteBoneButton_ = CreateButton(L"Del Bone", kDeleteBone);
        boneParentLabel_ = CreateControl(L"STATIC", L"Parent", SS_LEFT, 0);
        boneParentCombo_ = CreateControl(
            L"COMBOBOX",
            L"",
            CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
            kBoneParent);
        reparentBoneButton_ = CreateButton(L"Reparent", kReparentBone);
        addSlotBoneButton_ = CreateButton(L"Add Slot", kAddSlotBone);
        albedoR_ = CreateControl(L"EDIT", L"0.62", ES_AUTOHSCROLL | WS_TABSTOP, kAlbedoR, WS_EX_STATICEDGE);
        albedoG_ = CreateControl(L"EDIT", L"0.55", ES_AUTOHSCROLL | WS_TABSTOP, kAlbedoG, WS_EX_STATICEDGE);
        albedoB_ = CreateControl(L"EDIT", L"0.46", ES_AUTOHSCROLL | WS_TABSTOP, kAlbedoB, WS_EX_STATICEDGE);
        roughnessEdit_ = CreateControl(L"EDIT", L"0.62", ES_AUTOHSCROLL | WS_TABSTOP, kRoughness, WS_EX_STATICEDGE);
        metallicEdit_ = CreateControl(L"EDIT", L"0.04", ES_AUTOHSCROLL | WS_TABSTOP, kMetallic, WS_EX_STATICEDGE);
        albedoTexture_ = CreateControl(
            L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, kAlbedoTexture, WS_EX_STATICEDGE);
        albedoLabel_ = CreateControl(L"STATIC", L"RGB", SS_LEFT, 0);
        roughnessLabel_ = CreateControl(L"STATIC", L"ROUGH", SS_LEFT, 0);
        metallicLabel_ = CreateControl(L"STATIC", L"METAL", SS_LEFT, 0);
        textureLabel_ = CreateControl(L"STATIC", L"MAP", SS_LEFT, 0);
        applyLookButton_ = CreateButton(L"Stamp Look", kApplyLook);
        boneList_ = CreateControl(
            L"LISTBOX",
            L"",
            LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
            kBoneList);
        clipList_ = CreateControl(
            L"COMBOBOX",
            L"",
            CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
            kClipList);
        duplicateClipButton_ = CreateButton(L"Dup", kDuplicateClip);
        deleteClipButton_ = CreateButton(L"Del", kDeleteClip);
        animTime_ = CreateControl(L"EDIT", L"0.00", ES_AUTOHSCROLL | WS_TABSTOP, kAnimTime, WS_EX_STATICEDGE);
        animTimeLabel_ = CreateControl(L"STATIC", L"T", SS_CENTER, 0);
        animDuration_ = CreateControl(
            L"EDIT", L"1", ES_AUTOHSCROLL | WS_TABSTOP, kAnimDuration, WS_EX_STATICEDGE);
        animDurationLabel_ = CreateControl(L"STATIC", L"DUR", SS_CENTER, 0);
        animInButton_ = CreateButton(L"IN", kAnimStampIn);
        animOutButton_ = CreateButton(L"OUT", kAnimStampOut);
        animIn_ = CreateControl(L"EDIT", L"0", ES_AUTOHSCROLL | WS_TABSTOP, kAnimIn, WS_EX_STATICEDGE);
        animOut_ = CreateControl(L"EDIT", L"1", ES_AUTOHSCROLL | WS_TABSTOP, kAnimOut, WS_EX_STATICEDGE);
        animEventLabel_ = CreateControl(L"STATIC", L"EVT", SS_LEFT, 0);
        animEventName_ = CreateControl(
            L"EDIT", L"event", ES_AUTOHSCROLL | WS_TABSTOP, kAnimEventName, WS_EX_STATICEDGE);
        SendMessageW(
            animEventName_, EM_SETCUEBANNER, TRUE,
            reinterpret_cast<LPARAM>(L"Event name"));
        animAddEventButton_ = CreateButton(L"Add", kAnimAddEvent);
        animDelEventButton_ = CreateButton(L"Del", kAnimDelEvent);
        animRenameEventButton_ = CreateButton(L"Ren", kAnimRenameEvent);
        animEventToTimeButton_ = CreateButton(L"→T", kAnimEventToTime);
        animDupEventButton_ = CreateButton(L"Dup→", kAnimDupEvent);
        animClearEventsButton_ = CreateButton(L"ClrEvt", kClearAnimEvents);
        animPrevEventButton_ = CreateButton(L"<Evt", kAnimPrevEvent);
        animNextEventButton_ = CreateButton(L"Evt>", kAnimNextEvent);
        animEventList_ = CreateControl(
            L"LISTBOX",
            L"",
            LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
            kAnimEventList);
        animPlayButton_ = CreateButton(L"Play", kAnimPlay);
        animStopButton_ = CreateButton(L"Stop", kAnimStop);
        animLoopButton_ = CreateButton(L"Loop", kAnimLoop);
        animRootButton_ = CreateButton(L"Root", kAnimRootMotion);
        animKeyBoneButton_ = CreateButton(L"Key Bone", kAnimKeyBone);
        animKeyButton_ = CreateButton(L"Key Pose", kAnimKey);
        animDeleteKeyButton_ = CreateButton(L"Del Key", kDeleteAnimKey);
        animClearTrackButton_ = CreateButton(L"Clr Track", kClearAnimTrack);
        animClearKeysButton_ = CreateButton(L"Clr Keys", kClearAnimKeys);
        animDedupKeysButton_ = CreateButton(L"Clean Keys", kDedupAnimKeys);
        animQuantizeKeysButton_ = CreateButton(L"Q30", kQuantizeAnimKeys);
        animStripTracksButton_ = CreateButton(L"Strip", kStripAnimTracks);
        animPrevKeyButton_ = CreateButton(L"< Key", kAnimPrevKey);
        animNextKeyButton_ = CreateButton(L"Key >", kAnimNextKey);
        animSnapKeyButton_ = CreateButton(L"Snap", kSnapAnimKey);
        animResetBoneButton_ = CreateButton(L"Rst Bone", kResetBone);
        animResetPoseButton_ = CreateButton(L"Rst Pose", kResetPose);
        animMirrorPoseButton_ = CreateButton(L"Mir Pose", kMirrorPose);
        animRestKeyButton_ = CreateButton(L"Rest Key", kRestAnimKey);
        animMirrorKeysButton_ = CreateButton(L"Mir Keys", kMirrorAnimKeys);
        animHalfSpeedButton_ = CreateButton(L"½ Speed", kAnimHalfSpeed);
        animDoubleSpeedButton_ = CreateButton(L"×2 Speed", kAnimDoubleSpeed);
        animAlignStartButton_ = CreateButton(L"Align 0", kAnimAlignStart);
        animFitDurationButton_ = CreateButton(L"Fit End", kAnimFitDuration);
        animPlayheadZeroButton_ = CreateButton(L"Play→0", kAnimPlayheadToZero);
        animNudgeBackButton_ = CreateButton(L"←0.1", kAnimNudgeBack);
        animNudgeForwardButton_ = CreateButton(L"0.1→", kAnimNudgeForward);
        animCopyTrackButton_ = CreateButton(L"Copy Keys", kCopyAnimTrack);
        animPasteTrackButton_ = CreateButton(L"Paste Keys", kPasteAnimTrack);
        animTrimButton_ = CreateButton(L"Trim", kAnimTrim);
        animScrub_ = CreateControl(
            TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, kAnimScrub);
        SendMessageW(animScrub_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 1000));
        SendMessageW(animScrub_, TBM_SETPAGESIZE, 0, 50);
        assetFilter_ = CreateControl(
            L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, kAssetFilter, WS_EX_STATICEDGE);
        SendMessageW(
            assetFilter_, EM_SETCUEBANNER, TRUE,
            reinterpret_cast<LPARAM>(L"Filter stock, clay, rigs, clips"));
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
            L"Mill ready. STOCK builds solids, CLAY sculpts, LOOK stamps albedo, MOTION poses the rig.",
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
              transformZ_, albedoR_, albedoG_, albedoB_, roughnessEdit_, metallicEdit_, albedoTexture_,
              boneList_, clipList_, animTime_, animDuration_, animIn_, animOut_, animEventName_,
              animEventList_, animScrub_, detail_}) {
            ApplyDarkControlTheme(control);
        }
        SetTimer(hwnd_, kCatalogPollTimer, 50U, nullptr);
        RefreshCatalog();
    }

    void LayoutControls(int width, int height) {
        if (width < 200 || height < 200) {
            return;
        }
        constexpr int margin = 8;
        constexpr int gap = 6;
        constexpr int headerHeight = 44;
        constexpr int toolbarTop = 46;
        constexpr int toolbarHeight = 26;
        constexpr int contentTop = 78;
        constexpr int statusHeight = 22;
        const int contentBottom = (std::max)(contentTop + 120, height - statusHeight - 4);
        const int leftWidth = std::clamp(width * 20 / 100, 220, 280);
        const int rightWidth = std::clamp(width * 24 / 100, 250, 320);
        leftPane_ = RECT{margin, contentTop, margin + leftWidth, contentBottom};
        rightPane_ = RECT{width - margin - rightWidth, contentTop, width - margin, contentBottom};
        centerPane_ = RECT{leftPane_.right + gap, contentTop, rightPane_.left - gap, contentBottom};
        toolbarRect_ = RECT{0, headerHeight, width, contentTop - 4};

        MoveWindow(title_, margin, 8, 90, 22, TRUE);
        MoveWindow(summary_, margin + 96, 12, width - margin * 2 - 96, 16, TRUE);
        int buttonX = margin;
        const auto placeButton = [&](const HWND button, const int buttonWidth) {
            MoveWindow(button, buttonX, toolbarTop, buttonWidth, toolbarHeight, TRUE);
            buttonX += buttonWidth + 4;
        };
        placeButton(bayStock_, 52);
        placeButton(bayClay_, 48);
        placeButton(bayLook_, 48);
        placeButton(bayMotion_, 60);
        buttonX += 8;
        placeButton(newModelButton_, 72);
        placeButton(duplicateModelButton_, 68);
        placeButton(deleteModelButton_, 64);
        placeButton(newSculptButton_, 68);
        placeButton(duplicateSculptButton_, 64);
        placeButton(deleteSculptButton_, 60);
        placeButton(addPrimitiveButton_, 72);
        placeButton(addGroupButton_, 72);
        placeButton(bakeButton_, 48);
        placeButton(bindRigButton_, 64);
        placeButton(unbindRigButton_, 56);
        buttonX += 6;
        placeButton(newRigButton_, 60);
        placeButton(duplicateRigButton_, 56);
        placeButton(deleteRigButton_, 52);
        placeButton(newAnimButton_, 64);
        placeButton(validateButton_, 52);
        placeButton(openButton_, 54);
        placeButton(editorButton_, 72);
        placeButton(refreshButton_, 58);

        constexpr int panePadding = 8;
        MoveWindow(
            assetHeading_, leftPane_.left + panePadding, leftPane_.top + 6,
            leftWidth - panePadding * 2, 16, TRUE);
        MoveWindow(
            assetFilter_, leftPane_.left + panePadding, leftPane_.top + 24,
            leftWidth - panePadding * 2, 22, TRUE);
        MoveWindow(
            assetList_, leftPane_.left + panePadding, leftPane_.top + 50,
            leftWidth - panePadding * 2, leftPane_.bottom - leftPane_.top - 58, TRUE);

        MoveWindow(
            viewportHeading_, centerPane_.left + panePadding, centerPane_.top + 6,
            centerPane_.right - centerPane_.left - panePadding * 2, 16, TRUE);
        const RECT newViewportBounds{
            centerPane_.left + 2,
            centerPane_.top + 26,
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
        const int elementHeight = std::clamp(paneHeight * 26 / 100, 110, 180);
        MoveWindow(modelHeading_, inspectorLeft, rightPane_.top + 6, inspectorWidth, 16, TRUE);
        MoveWindow(modelElement_, inspectorLeft, rightPane_.top + 24, inspectorWidth, elementHeight, TRUE);
        const int stockToolsTop = rightPane_.top + 26 + elementHeight;
        const int stockToolHalf = (inspectorWidth - 4) / 2;
        MoveWindow(duplicatePartButton_, inspectorLeft, stockToolsTop, stockToolHalf, 22, TRUE);
        MoveWindow(
            deletePartButton_, inspectorLeft + stockToolHalf + 4, stockToolsTop, stockToolHalf, 22, TRUE);
        const int stockTransformTop = stockToolsTop + 26;

        const int lookTop = rightPane_.top + 6;
        MoveWindow(lookHeading_, inspectorLeft, lookTop, inspectorWidth, 16, TRUE);
        MoveWindow(albedoLabel_, inspectorLeft, lookTop + 20, inspectorWidth, 14, TRUE);
        const int axisGap = 4;
        const int channelWidth = (inspectorWidth - axisGap * 2) / 3;
        MoveWindow(albedoR_, inspectorLeft, lookTop + 36, channelWidth, 22, TRUE);
        MoveWindow(albedoG_, inspectorLeft + channelWidth + axisGap, lookTop + 36, channelWidth, 22, TRUE);
        MoveWindow(albedoB_, inspectorLeft + (channelWidth + axisGap) * 2, lookTop + 36, channelWidth, 22, TRUE);
        const int half = (inspectorWidth - axisGap) / 2;
        MoveWindow(roughnessLabel_, inspectorLeft, lookTop + 64, half, 14, TRUE);
        MoveWindow(metallicLabel_, inspectorLeft + half + axisGap, lookTop + 64, half, 14, TRUE);
        MoveWindow(roughnessEdit_, inspectorLeft, lookTop + 78, half, 22, TRUE);
        MoveWindow(metallicEdit_, inspectorLeft + half + axisGap, lookTop + 78, half, 22, TRUE);
        MoveWindow(textureLabel_, inspectorLeft, lookTop + 106, inspectorWidth, 14, TRUE);
        MoveWindow(albedoTexture_, inspectorLeft, lookTop + 122, inspectorWidth, 22, TRUE);
        MoveWindow(applyLookButton_, inspectorLeft, lookTop + 150, inspectorWidth, 24, TRUE);

        const int boneListHeight = std::clamp(paneHeight / 6, 56, 88);
        const bool clayBay = bay_ == kBayClay;
        const bool stockBay = bay_ == kBayStock;
        const bool motionBay = bay_ == kBayMotion;
        constexpr int motionClipBlock = 42;
        const int motionShift = motionBay ? motionClipBlock : 0;
        MoveWindow(clipHeading_, inspectorLeft, lookTop, inspectorWidth, 16, TRUE);
        MoveWindow(clipList_, inspectorLeft, lookTop + 18, inspectorWidth - 92, 22, TRUE);
        MoveWindow(duplicateClipButton_, inspectorLeft + inspectorWidth - 88, lookTop + 18, 40, 22, TRUE);
        MoveWindow(deleteClipButton_, inspectorLeft + inspectorWidth - 44, lookTop + 18, 40, 22, TRUE);
        const int poseTop = motionBay ? lookTop + 24 + boneListHeight + 30 + motionShift : stockTransformTop;
        MoveWindow(transformHeading_, inspectorLeft, poseTop, inspectorWidth, 16, TRUE);
        MoveWindow(transformMode_, inspectorLeft, poseTop + 18, inspectorWidth, 22, TRUE);
        const int axisTop = poseTop + 46;
        const int axisWidth = (inspectorWidth - axisGap * 2) / 3;
        const auto placeAxis = [&](const HWND label, const HWND edit, const int index) {
            const int left = inspectorLeft + index * (axisWidth + axisGap);
            MoveWindow(label, left, axisTop + 4, 12, 16, TRUE);
            MoveWindow(edit, left + 14, axisTop, axisWidth - 14, 22, TRUE);
        };
        placeAxis(xLabel_, transformX_, 0);
        placeAxis(yLabel_, transformY_, 1);
        placeAxis(zLabel_, transformZ_, 2);
        const int applyTop = axisTop + 28;
        const int applyHalf = (inspectorWidth - 4) / 2;
        if (motionBay && ShouldPersistBoneRest()) {
            MoveWindow(applyTransformButton_, inspectorLeft, applyTop, applyHalf, 24, TRUE);
            MoveWindow(mirrorRestButton_, inspectorLeft + applyHalf + 4, applyTop, applyHalf, 24, TRUE);
        } else {
            MoveWindow(applyTransformButton_, inspectorLeft, applyTop, inspectorWidth, 24, TRUE);
        }

        const int renameTop = axisTop + 56;
        const bool rigRestEdit = motionBay && ShouldPersistBoneRest()
            && !previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_;
        if (rigRestEdit) {
            MoveWindow(boneRenameLabel_, inspectorLeft, renameTop + 4, 36, 16, TRUE);
            MoveWindow(
                boneRenameEdit_,
                inspectorLeft + 38,
                renameTop,
                inspectorWidth - 38 - 56 - 4,
                22,
                TRUE);
            MoveWindow(boneRenameButton_, inspectorLeft + inspectorWidth - 56, renameTop, 56, 22, TRUE);
            const int parentTop = renameTop + 28;
            MoveWindow(boneParentLabel_, inspectorLeft, parentTop + 4, 40, 16, TRUE);
            MoveWindow(
                boneParentCombo_,
                inspectorLeft + 42,
                parentTop,
                inspectorWidth - 42 - 64 - 4,
                120,
                TRUE);
            MoveWindow(reparentBoneButton_, inspectorLeft + inspectorWidth - 64, parentTop, 64, 22, TRUE);
            const int hierarchyTop = parentTop + 28;
            const int hierarchyHalf = (inspectorWidth - 4) / 2;
            MoveWindow(addChildBoneButton_, inspectorLeft, hierarchyTop, hierarchyHalf, 22, TRUE);
            MoveWindow(
                deleteBoneButton_,
                inspectorLeft + hierarchyHalf + 4,
                hierarchyTop,
                hierarchyHalf,
                22,
                TRUE);
            const bool hasMissingHumanoidSlots = editableRig_.has_value()
                && editableRig_->profile == ri::scene::RigProfile::Humanoid
                && !ri::scene::MissingHumanoidBoneKeys(*editableRig_).empty();
            if (hasMissingHumanoidSlots) {
                MoveWindow(
                    addSlotBoneButton_,
                    inspectorLeft,
                    hierarchyTop + 28,
                    inspectorWidth,
                    22,
                    TRUE);
            }
        }
        const int rigEditRows = rigRestEdit
            ? (86
                + ((editableRig_.has_value()
                       && editableRig_->profile == ri::scene::RigProfile::Humanoid
                       && !ri::scene::MissingHumanoidBoneKeys(*editableRig_).empty())
                      ? 28
                      : 0))
            : 0;
        const int motionKeysTop = axisTop + 56 + rigEditRows;
        const int keyBtn = (inspectorWidth - 12) / 4;
        MoveWindow(animKeyBoneButton_, inspectorLeft, motionKeysTop, keyBtn, 24, TRUE);
        MoveWindow(animKeyButton_, inspectorLeft + keyBtn + 4, motionKeysTop, keyBtn, 24, TRUE);
        MoveWindow(
            animDeleteKeyButton_, inspectorLeft + (keyBtn + 4) * 2, motionKeysTop, keyBtn, 24, TRUE);
        MoveWindow(
            animClearTrackButton_, inspectorLeft + (keyBtn + 4) * 3, motionKeysTop, keyBtn, 24, TRUE);
        const int poseToolsTop = motionKeysTop + 28;
        MoveWindow(animResetBoneButton_, inspectorLeft, poseToolsTop, keyBtn, 24, TRUE);
        MoveWindow(animResetPoseButton_, inspectorLeft + keyBtn + 4, poseToolsTop, keyBtn, 24, TRUE);
        MoveWindow(
            animMirrorPoseButton_, inspectorLeft + (keyBtn + 4) * 2, poseToolsTop, keyBtn, 24, TRUE);
        MoveWindow(animSnapKeyButton_, inspectorLeft + (keyBtn + 4) * 3, poseToolsTop, keyBtn, 24, TRUE);
        const int keyToolsTop = poseToolsTop + 28;
        MoveWindow(animRestKeyButton_, inspectorLeft, keyToolsTop, keyBtn, 24, TRUE);
        MoveWindow(
            animMirrorKeysButton_, inspectorLeft + keyBtn + 4, keyToolsTop, keyBtn, 24, TRUE);
        MoveWindow(
            animCopyTrackButton_, inspectorLeft + (keyBtn + 4) * 2, keyToolsTop, keyBtn, 24, TRUE);
        MoveWindow(
            animPasteTrackButton_, inspectorLeft + (keyBtn + 4) * 3, keyToolsTop, keyBtn, 24, TRUE);
        const int speedTop = keyToolsTop + 28;
        MoveWindow(animHalfSpeedButton_, inspectorLeft, speedTop, keyBtn, 24, TRUE);
        MoveWindow(animDoubleSpeedButton_, inspectorLeft + keyBtn + 4, speedTop, keyBtn, 24, TRUE);
        MoveWindow(
            animNudgeBackButton_, inspectorLeft + (keyBtn + 4) * 2, speedTop, keyBtn, 24, TRUE);
        MoveWindow(
            animNudgeForwardButton_, inspectorLeft + (keyBtn + 4) * 3, speedTop, keyBtn, 24, TRUE);
        const int alignTop = speedTop + 28;
        MoveWindow(animAlignStartButton_, inspectorLeft, alignTop, keyBtn, 24, TRUE);
        MoveWindow(animFitDurationButton_, inspectorLeft + keyBtn + 4, alignTop, keyBtn, 24, TRUE);
        MoveWindow(
            animPlayheadZeroButton_, inspectorLeft + (keyBtn + 4) * 2, alignTop, keyBtn, 24, TRUE);
        MoveWindow(animTrimButton_, inspectorLeft + (keyBtn + 4) * 3, alignTop, keyBtn, 24, TRUE);
        const int clearTop = alignTop + 28;
        const int clearQuarter = (inspectorWidth - 12) / 4;
        MoveWindow(animClearKeysButton_, inspectorLeft, clearTop, clearQuarter, 24, TRUE);
        MoveWindow(
            animDedupKeysButton_, inspectorLeft + clearQuarter + 4, clearTop, clearQuarter, 24, TRUE);
        MoveWindow(
            animQuantizeKeysButton_,
            inspectorLeft + (clearQuarter + 4) * 2,
            clearTop,
            clearQuarter,
            24,
            TRUE);
        MoveWindow(
            animStripTracksButton_,
            inspectorLeft + (clearQuarter + 4) * 3,
            clearTop,
            clearQuarter,
            24,
            TRUE);
        const int scrubRow = clearTop + 28;
        const int scrubLeft = inspectorLeft + 48;
        const int scrubWidth = inspectorWidth - 96;
        MoveWindow(animPrevKeyButton_, inspectorLeft, scrubRow, 44, 22, TRUE);
        MoveWindow(animScrub_, scrubLeft, scrubRow, scrubWidth, 22, TRUE);
        MoveWindow(animNextKeyButton_, scrubLeft + scrubWidth + 4, scrubRow, 44, 22, TRUE);
        const int timeHalf = (inspectorWidth - axisGap) / 2;
        MoveWindow(animTimeLabel_, inspectorLeft, scrubRow + 28, 14, 16, TRUE);
        MoveWindow(animTime_, inspectorLeft + 18, scrubRow + 24, timeHalf - 18, 22, TRUE);
        MoveWindow(animDurationLabel_, inspectorLeft + timeHalf + axisGap, scrubRow + 28, 28, 16, TRUE);
        MoveWindow(
            animDuration_,
            inspectorLeft + timeHalf + axisGap + 32,
            scrubRow + 24,
            timeHalf - 32,
            22,
            TRUE);
        const int windowTop = scrubRow + 52;
        const int windowHalf = (inspectorWidth - axisGap) / 2;
        MoveWindow(animInButton_, inspectorLeft, windowTop, 36, 22, TRUE);
        MoveWindow(animIn_, inspectorLeft + 40, windowTop, windowHalf - 40, 22, TRUE);
        MoveWindow(animOutButton_, inspectorLeft + windowHalf + axisGap, windowTop, 40, 22, TRUE);
        MoveWindow(
            animOut_,
            inspectorLeft + windowHalf + axisGap + 44,
            windowTop,
            windowHalf - 44,
            22,
            TRUE);
        const int eventTop = windowTop + 28;
        const int eventBtn = 26;
        MoveWindow(animEventLabel_, inspectorLeft, eventTop + 4, 28, 16, TRUE);
        MoveWindow(
            animEventName_,
            inspectorLeft + 30,
            eventTop,
            inspectorWidth - 30 - eventBtn * 6 - 12,
            22,
            TRUE);
        MoveWindow(
            animAddEventButton_,
            inspectorLeft + inspectorWidth - eventBtn * 6 - 12,
            eventTop,
            eventBtn,
            22,
            TRUE);
        MoveWindow(
            animRenameEventButton_,
            inspectorLeft + inspectorWidth - eventBtn * 5 - 10,
            eventTop,
            eventBtn,
            22,
            TRUE);
        MoveWindow(
            animEventToTimeButton_,
            inspectorLeft + inspectorWidth - eventBtn * 4 - 8,
            eventTop,
            eventBtn,
            22,
            TRUE);
        MoveWindow(
            animDupEventButton_,
            inspectorLeft + inspectorWidth - eventBtn * 3 - 6,
            eventTop,
            eventBtn,
            22,
            TRUE);
        MoveWindow(
            animClearEventsButton_,
            inspectorLeft + inspectorWidth - eventBtn * 2 - 4,
            eventTop,
            eventBtn,
            22,
            TRUE);
        MoveWindow(
            animDelEventButton_,
            inspectorLeft + inspectorWidth - eventBtn,
            eventTop,
            eventBtn,
            22,
            TRUE);
        MoveWindow(animEventList_, inspectorLeft + 40, eventTop + 26, inspectorWidth - 80, 48, TRUE);
        MoveWindow(animPrevEventButton_, inspectorLeft, eventTop + 26, 36, 48, TRUE);
        MoveWindow(
            animNextEventButton_, inspectorLeft + inspectorWidth - 36, eventTop + 26, 36, 48, TRUE);

        const int stockBindTop = axisTop + 56;
        const int boneHeadingTop = stockBay ? stockBindTop : (motionBay ? lookTop + motionShift : lookTop);
        const int boneListTop = boneHeadingTop + 18;
        MoveWindow(motionHeading_, inspectorLeft, boneHeadingTop, inspectorWidth, 16, TRUE);
        MoveWindow(boneList_, inspectorLeft, boneListTop, inspectorWidth, boneListHeight, TRUE);
        const int motionButtonsTop = lookTop + 24 + boneListHeight + motionShift;
        const int motionBtn = (inspectorWidth - 12) / 4;
        MoveWindow(animPlayButton_, inspectorLeft, motionButtonsTop, motionBtn, 24, TRUE);
        MoveWindow(animStopButton_, inspectorLeft + motionBtn + 4, motionButtonsTop, motionBtn, 24, TRUE);
        MoveWindow(animLoopButton_, inspectorLeft + (motionBtn + 4) * 2, motionButtonsTop, motionBtn, 24, TRUE);
        MoveWindow(animRootButton_, inspectorLeft + (motionBtn + 4) * 3, motionButtonsTop, motionBtn, 24, TRUE);
        MoveWindow(
            bindBoneButton_, inspectorLeft, boneListTop + boneListHeight + 4, inspectorWidth, 24, TRUE);
        const int floodHalf = (inspectorWidth - 12) / 4;
        MoveWindow(
            floodBoneButton_, inspectorLeft, boneListTop + boneListHeight + 4, floodHalf, 24, TRUE);
        MoveWindow(
            floodUnboundButton_,
            inspectorLeft + floodHalf + 4,
            boneListTop + boneListHeight + 4,
            floodHalf,
            24,
            TRUE);
        MoveWindow(
            transferWeightsButton_,
            inspectorLeft + (floodHalf + 4) * 2,
            boneListTop + boneListHeight + 4,
            floodHalf,
            24,
            TRUE);
        MoveWindow(
            swapWeightsButton_,
            inspectorLeft + (floodHalf + 4) * 3,
            boneListTop + boneListHeight + 4,
            floodHalf,
            24,
            TRUE);
        const int weightToolsTop = boneListTop + boneListHeight + 32;
        const int weightBtn = (inspectorWidth - 12) / 4;
        MoveWindow(normWeightsButton_, inspectorLeft, weightToolsTop, weightBtn, 24, TRUE);
        MoveWindow(pruneWeightsButton_, inspectorLeft + weightBtn + 4, weightToolsTop, weightBtn, 24, TRUE);
        MoveWindow(
            mirrorWeightsButton_, inspectorLeft + (weightBtn + 4) * 2, weightToolsTop, weightBtn, 24, TRUE);
        MoveWindow(
            clearWeightsButton_, inspectorLeft + (weightBtn + 4) * 3, weightToolsTop, weightBtn, 24, TRUE);
        const int softBtn = (inspectorWidth - 16) / 5;
        MoveWindow(softWeightsButton_, inspectorLeft, weightToolsTop + 28, softBtn, 24, TRUE);
        MoveWindow(
            auditWeightsButton_, inspectorLeft + softBtn + 4, weightToolsTop + 28, softBtn, 24, TRUE);
        MoveWindow(
            invertWeightsButton_,
            inspectorLeft + (softBtn + 4) * 2,
            weightToolsTop + 28,
            softBtn,
            24,
            TRUE);
        MoveWindow(
            halveWeightsButton_,
            inspectorLeft + (softBtn + 4) * 3,
            weightToolsTop + 28,
            softBtn,
            24,
            TRUE);
        MoveWindow(
            doubleWeightsButton_,
            inspectorLeft + (softBtn + 4) * 4,
            weightToolsTop + 28,
            softBtn,
            24,
            TRUE);

        const int detailTop = bay_ == kBayLook ? lookTop + 186
            : (motionBay ? eventTop + 80
                : (stockBay ? boneListTop + boneListHeight + 34
                    : clayBay ? boneListTop + boneListHeight + 90
                    : axisTop + 58));
        MoveWindow(detailHeading_, inspectorLeft, detailTop, inspectorWidth, 16, TRUE);
        const int nameTop = detailTop + 18;
        MoveWindow(displayNameLabel_, inspectorLeft, nameTop + 4, 36, 16, TRUE);
        MoveWindow(
            displayNameEdit_,
            inspectorLeft + 40,
            nameTop,
            inspectorWidth - 40 - 64,
            22,
            TRUE);
        MoveWindow(
            renameDisplayButton_,
            inspectorLeft + inspectorWidth - 60,
            nameTop,
            60,
            22,
            TRUE);
        MoveWindow(
            detail_, inspectorLeft, nameTop + 28, inspectorWidth,
            (std::max)(28, static_cast<int>(rightPane_.bottom) - nameTop - 34), TRUE);
        MoveWindow(status_, margin, height - statusHeight + 3, width - margin * 2, 16, TRUE);
        ApplyBayVisibility();
    }

    void ApplyBayVisibility() {
        const bool stock = bay_ == kBayStock;
        const bool clay = bay_ == kBayClay;
        const bool look = bay_ == kBayLook;
        const bool motion = bay_ == kBayMotion;
        const bool place = stock || motion;
        const bool bones = stock || clay || motion;
        const auto show = [](const HWND control, const bool visible) {
            ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
        };
        show(modelHeading_, stock);
        show(modelElement_, stock);
        show(duplicatePartButton_, stock);
        show(deletePartButton_, stock);
        show(transformHeading_, place);
        show(transformMode_, place);
        show(xLabel_, place);
        show(yLabel_, place);
        show(zLabel_, place);
        show(transformX_, place);
        show(transformY_, place);
        show(transformZ_, place);
        show(applyTransformButton_, place);
        show(lookHeading_, look);
        show(albedoLabel_, look);
        show(albedoR_, look);
        show(albedoG_, look);
        show(albedoB_, look);
        show(roughnessLabel_, look);
        show(metallicLabel_, look);
        show(roughnessEdit_, look);
        show(metallicEdit_, look);
        show(textureLabel_, look);
        show(albedoTexture_, look);
        show(applyLookButton_, look);
        SetWindowTextW(motionHeading_, L"BONES");
        show(motionHeading_, bones);
        show(clipHeading_, motion);
        show(clipList_, motion);
        show(duplicateClipButton_, motion);
        show(deleteClipButton_, motion);
        show(boneList_, bones);
        show(bindBoneButton_, stock);
        show(floodBoneButton_, clay);
        show(floodUnboundButton_, clay);
        show(transferWeightsButton_, clay);
        show(swapWeightsButton_, clay);
        show(normWeightsButton_, clay);
        show(pruneWeightsButton_, clay);
        show(mirrorWeightsButton_, clay);
        show(clearWeightsButton_, clay);
        show(softWeightsButton_, clay);
        show(auditWeightsButton_, clay);
        show(invertWeightsButton_, clay);
        show(halveWeightsButton_, clay);
        show(doubleWeightsButton_, clay);
        show(mirrorRestButton_, motion && ShouldPersistBoneRest());
        show(
            boneRenameLabel_,
            motion && !previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_);
        show(
            boneRenameEdit_,
            motion && !previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_);
        show(
            boneRenameButton_,
            motion && !previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_);
        show(
            addChildBoneButton_,
            motion && !previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_);
        show(
            deleteBoneButton_,
            motion && !previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_);
        show(
            addSlotBoneButton_,
            motion && !previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_
                && editableRig_.has_value()
                && editableRig_->profile == ri::scene::RigProfile::Humanoid
                && !ri::scene::MissingHumanoidBoneKeys(*editableRig_).empty());
        show(
            boneParentLabel_,
            motion && !previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_);
        show(
            boneParentCombo_,
            motion && !previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_);
        show(
            reparentBoneButton_,
            motion && !previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_);
        show(animPlayButton_, motion);
        show(animStopButton_, motion);
        show(animLoopButton_, motion);
        show(animRootButton_, motion);
        show(animKeyBoneButton_, motion);
        show(animKeyButton_, motion);
        show(animDeleteKeyButton_, motion);
        show(animClearTrackButton_, motion);
        show(animClearKeysButton_, motion);
        show(animDedupKeysButton_, motion);
        show(animQuantizeKeysButton_, motion);
        show(animStripTracksButton_, motion);
        show(animResetBoneButton_, motion);
        show(animResetPoseButton_, motion);
        show(animMirrorPoseButton_, motion);
        show(animSnapKeyButton_, motion);
        show(animRestKeyButton_, motion);
        show(animMirrorKeysButton_, motion);
        show(animCopyTrackButton_, motion);
        show(animPasteTrackButton_, motion);
        show(animHalfSpeedButton_, motion);
        show(animDoubleSpeedButton_, motion);
        show(animNudgeBackButton_, motion);
        show(animNudgeForwardButton_, motion);
        show(animAlignStartButton_, motion);
        show(animFitDurationButton_, motion);
        show(animPlayheadZeroButton_, motion);
        show(animPrevKeyButton_, motion);
        show(animNextKeyButton_, motion);
        show(animTrimButton_, motion);
        show(animScrub_, motion);
        show(animTimeLabel_, motion);
        show(animTime_, motion);
        show(animDurationLabel_, motion);
        show(animDuration_, motion);
        show(animInButton_, motion);
        show(animOutButton_, motion);
        show(animIn_, motion);
        show(animOut_, motion);
        show(animEventLabel_, motion);
        show(animEventName_, motion);
        show(animAddEventButton_, motion);
        show(animDelEventButton_, motion);
        show(animRenameEventButton_, motion);
        show(animEventToTimeButton_, motion);
        show(animDupEventButton_, motion);
        show(animClearEventsButton_, motion);
        show(animPrevEventButton_, motion);
        show(animNextEventButton_, motion);
        show(animEventList_, motion);
        SyncAuthoringEnable();
    }

    void RefreshCatalog(const fs::path& preferredSelection = {}) {
        fs::path selectedPath = preferredSelection;
        if (selectedPath.empty()) {
            if (const ri::forge::AssetEntry* selected = SelectedAsset(); selected != nullptr) {
                selectedPath = selected->absolutePath;
            }
        }
        catalogIndex_.Request(selectedPath);
        if (!sculptDragging_ && !sculptDirty_) {
            SetWindowTextW(status_, L"Updating the asset index in the background\u2026");
        }
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
            } else if (asset.kind == ri::forge::AssetKind::Animation) {
                prefix = asset.valid ? "[CLIP]  " : "[CLIP!] ";
            } else if (asset.kind == ri::forge::AssetKind::Sculpt) {
                prefix = asset.valid ? "[CLAY]  " : "[CLAY!] ";
            } else if (asset.kind == ri::forge::AssetKind::PrimitiveModel) {
                prefix = asset.valid ? "[STOCK] " : "[STOCK!]";
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
            + std::to_string(catalog_.entries.size()) + "  ·  "
            + std::to_string(catalog_.primitiveModelCount) + " stock  ·  "
            + std::to_string(catalog_.sculptCount) + " clay  ·  "
            + std::to_string(catalog_.modelCount) + " import  ·  "
            + std::to_string(catalog_.rigCount) + " rigs  ·  "
            + std::to_string(catalog_.animationCount) + " clips  ·  "
            + std::to_string(catalog_.invalidPrimitiveModelCount + catalog_.invalidSculptCount
                             + catalog_.invalidRigCount + catalog_.invalidAnimationCount)
            + " bad  ·  " + catalog_.sourceRoot.string();
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
        if (!sculptDragging_ && !sculptDirty_) {
            const auto elapsed = static_cast<long long>(std::llround(result->elapsedMilliseconds));
            const std::string status = "Indexed " + std::to_string(catalog_.entries.size())
                + " Forge assets in " + std::to_string(elapsed)
                + " ms. Filtering is instant and does not rescan the workspace.";
            SetWindowTextW(status_, Widen(status).c_str());
        }
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

    bool RestoreAssetSelection(const fs::path& path) {
        if (path.empty() || assetList_ == nullptr) {
            return false;
        }
        for (std::size_t visibleIndex = 0; visibleIndex < visibleAssetIndices_.size(); ++visibleIndex) {
            const std::size_t catalogIndex = visibleAssetIndices_[visibleIndex];
            if (catalogIndex < catalog_.entries.size()
                && catalog_.entries[catalogIndex].absolutePath == path) {
                SendMessageW(assetList_, LB_SETCURSEL, static_cast<WPARAM>(visibleIndex), 0);
                return true;
            }
        }
        return false;
    }

    void UpdateInspector() {
        const ri::forge::AssetEntry* asset = SelectedAsset();
        const bool leavingSculpt = !editableSculptPath_.empty()
            && (asset == nullptr || asset->absolutePath != editableSculptPath_);
        if (leavingSculpt && !FlushLiveSculpt()) {
            MessageBoxW(
                hwnd_,
                L"Could not save the open clay. Stay on this sculpt until it writes, or copy the work out.",
                L"Forge clay",
                MB_OK | MB_ICONWARNING);
            if (!RestoreAssetSelection(editableSculptPath_)) {
                SetWindowTextW(status_, L"Unsaved clay is still open. Catalog selection was not changed.");
            }
            return;
        }
        if (asset == nullptr) {
            SetWindowTextW(detail_, L"No supported model or rig sources were found under Assets/Source.");
            EnableWindow(validateButton_, FALSE);
            EnableWindow(editorButton_, FALSE);
            EnableWindow(addPrimitiveButton_, FALSE);
            EnableWindow(addGroupButton_, FALSE);
            EnableWindow(duplicatePartButton_, FALSE);
            EnableWindow(deletePartButton_, FALSE);
            EnableWindow(duplicateSculptButton_, FALSE);
            EnableWindow(deleteSculptButton_, FALSE);
            EnableWindow(duplicateModelButton_, FALSE);
            EnableWindow(deleteModelButton_, FALSE);
            EnableWindow(duplicateRigButton_, FALSE);
            EnableWindow(deleteRigButton_, FALSE);
            EnableWindow(bakeButton_, FALSE);
            EnableWindow(bindRigButton_, FALSE);
            EnableWindow(unbindRigButton_, FALSE);
            EnableWindow(newAnimButton_, !PreferredRigPath().empty() ? TRUE : FALSE);
            PopulateDisplayNameField();
            RefreshModelControls(nullptr);
            Rebuild3DPreview(nullptr);
            PopulateClipList();
            return;
        }

        const char* kind = "Imported source";
        if (asset->kind == ri::forge::AssetKind::Rig) {
            kind = "Rig";
        } else if (asset->kind == ri::forge::AssetKind::PrimitiveModel) {
            kind = "Stock model";
        } else if (asset->kind == ri::forge::AssetKind::Sculpt) {
            kind = "Clay";
        } else if (asset->kind == ri::forge::AssetKind::Animation) {
            kind = "Motion clip";
        }
        std::string text = std::string(kind) + "\r\n\r\n" + asset->relativePath + "\r\n\r\n" + asset->summary
            + "\r\n\r\n" + asset->absolutePath.string();
        if (asset->kind == ri::forge::AssetKind::ModelSource) {
            text += "\r\n\r\nBridge file only. Author in STOCK or CLAY, then export interchange if a vendor tool needs it.";
        } else if (asset->kind == ri::forge::AssetKind::Rig) {
            text += "\r\n\r\nRest skeleton for bake weights and MOTION clips. MOTION bay: Set Rest, Mir Rest, Rename, Add/Del/Reparent updates .ri_rig.json and open sidecars.";
            if (const std::optional<ri::scene::RigDefinition> rig =
                    ri::scene::LoadRigDefinition(asset->absolutePath)) {
                const ri::scene::RigValidationReport report = ri::scene::ValidateRigDefinition(*rig);
                if (!report.warnings.empty()) {
                    text += "\r\n\r\nValidation warnings:";
                    for (const std::string& warning : report.warnings) {
                        text += "\r\n- " + warning;
                    }
                }
            }
        } else if (asset->kind == ri::forge::AssetKind::Sculpt) {
            text += "\r\n\r\nNative clay. LMB build, Shift smooth, Ctrl inflate, E extrude, X/Y/Z mirror. Bind Rig nearest-weights the cage.";
            text += " B paint weights (blend/assign/smooth/clear). Norm/Prune/Mir X fix the bind list.";
            if (const std::optional<ri::content::NativeSculptDocument> sculpt =
                    ri::content::LoadNativeSculptDocument(asset->absolutePath)) {
                const ri::scene::NativeSculptWeightAudit audit =
                    ri::scene::AuditNativeSculptWeights(*sculpt);
                if (audit.vertexCount > 0U) {
                    text += "\r\n\r\nWeights: " + std::to_string(audit.boundCount) + " bound / "
                        + std::to_string(audit.vertexCount) + " verts";
                    if (audit.unboundCount > 0U) {
                        text += " | " + std::to_string(audit.unboundCount) + " unbound";
                    }
                    if (audit.blendedCount > 0U) {
                        text += " | " + std::to_string(audit.blendedCount) + " blended";
                    }
                    if (audit.nonNormalizedCount > 0U) {
                        text += " | " + std::to_string(audit.nonNormalizedCount) + " non-normalized";
                    }
                }
            }
        } else if (asset->kind == ri::forge::AssetKind::Animation) {
            text += "\r\n\r\nBone-name clip. MOTION CLIP list plays it on the open clay or stock. Pick a bone, Set Pose, then Key Bone.";
            if (const std::optional<ri::content::NativeAnimationDocument> clip =
                    ri::content::LoadNativeAnimationDocument(asset->absolutePath)) {
                text += "\r\n\r\nTracks: " + std::to_string(clip->tracks.size())
                    + "  |  Events: " + std::to_string(clip->events.size())
                    + "  |  Duration: " + std::to_string(clip->durationSeconds) + "s";
                if (!clip->rigPath.empty()) {
                    text += "\r\nRig: " + clip->rigPath;
                }
                if (!previewBoneNodes_.empty()
                    && editableAnim_.has_value()
                    && editableAnimPath_ == asset->absolutePath) {
                    const ri::scene::NativeAnimationBindReport bindReport =
                        ri::scene::DiagnoseNativeAnimationBind(
                            *editableAnim_, previewScene_, previewBoneNodes_);
                    text += "\r\n\r\nSkeleton bind: " + bindReport.summary;
                    if (!bindReport.missingBones.empty()) {
                        text += "\r\nUnbound track bones:";
                        for (const std::string& boneName : bindReport.missingBones) {
                            text += "\r\n- " + boneName;
                        }
                        if (bindReport.missingBoneCount > bindReport.missingBones.size()) {
                            text += "\r\n- ...";
                        }
                    }
                }
            }
        } else {
            text += "\r\n\r\nGrouped native solids. Bind Rig overlays the skeleton. Bind Bone assigns the selected part. LOOK stamps albedo. Bake flattens for runtime.";
        }
        SetWindowTextW(detail_, Widen(text).c_str());
        PopulateDisplayNameField();
        EnableWindow(validateButton_, TRUE);
        EnableWindow(editorButton_, TRUE);
        const BOOL modelSelected = asset->kind == ri::forge::AssetKind::PrimitiveModel && asset->valid;
        EnableWindow(addPrimitiveButton_, modelSelected);
        EnableWindow(addGroupButton_, modelSelected);
        EnableWindow(duplicatePartButton_, modelSelected);
        EnableWindow(deletePartButton_, modelSelected);
        EnableWindow(bakeButton_, modelSelected);
        EnableWindow(duplicateModelButton_, modelSelected);
        EnableWindow(deleteModelButton_, modelSelected);
        const BOOL sculptSelected = asset->kind == ri::forge::AssetKind::Sculpt && asset->valid;
        EnableWindow(duplicateSculptButton_, sculptSelected);
        EnableWindow(deleteSculptButton_, sculptSelected);
        const BOOL rigSelected = asset->kind == ri::forge::AssetKind::Rig && asset->valid;
        EnableWindow(duplicateRigButton_, rigSelected);
        EnableWindow(deleteRigButton_, rigSelected);
        if (asset->kind == ri::forge::AssetKind::Rig && asset->valid) {
            lastRigPath_ = asset->absolutePath;
        }
        if ((asset->kind == ri::forge::AssetKind::PrimitiveModel
                || asset->kind == ri::forge::AssetKind::Sculpt)
            && asset->valid) {
            lastBindTargetPath_ = asset->absolutePath;
            lastBindTargetKind_ = asset->kind;
        }
        const bool canBind = (asset->kind == ri::forge::AssetKind::PrimitiveModel && asset->valid)
            || (asset->kind == ri::forge::AssetKind::Sculpt && asset->valid)
            || ((lastBindTargetKind_ == ri::forge::AssetKind::PrimitiveModel
                    || lastBindTargetKind_ == ri::forge::AssetKind::Sculpt)
                && !lastBindTargetPath_.empty());
        EnableWindow(bindRigButton_, canBind ? TRUE : FALSE);
        const bool canUnbind = (editableSculpt_.has_value() && !editableSculpt_->rigPath.empty())
            || (editableModel_.has_value() && !editableModel_->rigPath.empty())
            || (asset->kind == ri::forge::AssetKind::Sculpt && asset->valid)
            || (asset->kind == ri::forge::AssetKind::PrimitiveModel && asset->valid);
        EnableWindow(unbindRigButton_, canUnbind ? TRUE : FALSE);
        EnableWindow(newAnimButton_, !PreferredRigPath().empty() ? TRUE : FALSE);
        if (asset->kind == ri::forge::AssetKind::Animation && asset->valid
            && CanBindClipOntoLivePreview(asset->absolutePath)) {
            EnableWindow(addPrimitiveButton_, editableModel_.has_value() ? TRUE : FALSE);
            EnableWindow(addGroupButton_, editableModel_.has_value() ? TRUE : FALSE);
            EnableWindow(bakeButton_, editableModel_.has_value() ? TRUE : FALSE);
            BindClipOntoPreview(asset->absolutePath);
            SetForgeBay(kBayMotion);
            return;
        }
        RefreshModelControls(asset);
        Rebuild3DPreview(asset);
        PopulateClipList();
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

    [[nodiscard]] std::string CurrentModelElementId() const {
        if (!editableModel_.has_value()) {
            return {};
        }
        const LRESULT selection = SendMessageW(modelElement_, LB_GETCURSEL, 0, 0);
        if (selection == LB_ERR || selection < 0
            || static_cast<std::size_t>(selection) >= modelElements_.size()) {
            return {};
        }
        const ModelElementRef& element = modelElements_[static_cast<std::size_t>(selection)];
        if (element.group) {
            return element.index < editableModel_->groups.size()
                ? editableModel_->groups[element.index].id
                : std::string{};
        }
        return element.index < editableModel_->parts.size()
            ? editableModel_->parts[element.index].id
            : std::string{};
    }

    [[nodiscard]] int FindModelElementIndex(const std::string& id) const {
        if (!editableModel_.has_value() || id.empty()) {
            return -1;
        }
        for (std::size_t index = 0; index < modelElements_.size(); ++index) {
            const ModelElementRef& element = modelElements_[index];
            if (element.group && element.index < editableModel_->groups.size()
                && editableModel_->groups[element.index].id == id) {
                return static_cast<int>(index);
            }
            if (!element.group && element.index < editableModel_->parts.size()
                && editableModel_->parts[element.index].id == id) {
                return static_cast<int>(index);
            }
        }
        return -1;
    }

    void RebuildModelElementList(const std::string& preferredId) {
        modelElements_.clear();
        SendMessageW(modelElement_, LB_RESETCONTENT, 0, 0);
        if (!editableModel_.has_value()) {
            return;
        }
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
        if (modelElements_.empty()) {
            return;
        }
        const int preferred = FindModelElementIndex(preferredId);
        SendMessageW(
            modelElement_,
            LB_SETCURSEL,
            static_cast<WPARAM>(preferred >= 0 ? preferred : 0),
            0);
    }

    bool SelectModelElementById(const std::string& id) {
        const int index = FindModelElementIndex(id);
        if (index < 0) {
            return false;
        }
        SendMessageW(modelElement_, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
        PopulateTransformFields();
        SyncAuthoringEnable();
        SyncTransformGizmo();
        PublishPreview();
        return true;
    }

    void RefreshModelControls(const ri::forge::AssetEntry* asset) {
        const bool enabled = asset != nullptr
            && asset->kind == ri::forge::AssetKind::PrimitiveModel
            && asset->valid;
        if (!enabled) {
            editableModel_.reset();
            editableModelPath_.clear();
            editableModelHasWriteTime_ = false;
            pendingModelElementId_.clear();
            modelElements_.clear();
            SendMessageW(modelElement_, LB_RESETCONTENT, 0, 0);
            EnableWindow(modelElement_, FALSE);
            PopulateTransformFields();
            SyncAuthoringEnable();
            return;
        }

        const bool sameDocument =
            editableModel_.has_value() && editableModelPath_ == asset->absolutePath;
        std::error_code writeError{};
        const fs::file_time_type writeTime = fs::last_write_time(asset->absolutePath, writeError);
        const bool hasWriteTime = !writeError;
        if (sameDocument && hasWriteTime == editableModelHasWriteTime_
            && (!hasWriteTime || writeTime == editableModelWriteTime_)) {
            EnableWindow(modelElement_, TRUE);
            SyncAuthoringEnable();
            return;
        }

        const std::string keepId =
            !pendingModelElementId_.empty() ? pendingModelElementId_ : CurrentModelElementId();
        pendingModelElementId_.clear();
        editableModel_ = ri::content::LoadPrimitiveModelDocument(asset->absolutePath);
        editableModelPath_ = asset->absolutePath;
        StampWriteTime(editableModelPath_, editableModelWriteTime_, editableModelHasWriteTime_);
        RebuildModelElementList(keepId);
        EnableWindow(modelElement_, editableModel_.has_value() ? TRUE : FALSE);
        PopulateTransformFields();
        SyncAuthoringEnable();
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
        if (bay_ == kBayMotion) {
            PopulateBonePoseFields();
            return;
        }
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
        PopulateLookFields();
    }

    ri::content::PrimitiveModelPart* SelectedPart() {
        if (!editableModel_.has_value()) {
            return nullptr;
        }
        const LRESULT selection = SendMessageW(modelElement_, LB_GETCURSEL, 0, 0);
        if (selection == LB_ERR || selection < 0
            || static_cast<std::size_t>(selection) >= modelElements_.size()) {
            return nullptr;
        }
        const ModelElementRef& element = modelElements_[static_cast<std::size_t>(selection)];
        if (element.group || element.index >= editableModel_->parts.size()) {
            return nullptr;
        }
        return &editableModel_->parts[element.index];
    }

    void PopulateLookFields() {
        const ri::content::PrimitiveModelPart* part = SelectedPart();
        const BOOL enabled = part != nullptr ? TRUE : FALSE;
        EnableWindow(albedoR_, enabled);
        EnableWindow(albedoG_, enabled);
        EnableWindow(albedoB_, enabled);
        EnableWindow(roughnessEdit_, enabled);
        EnableWindow(metallicEdit_, enabled);
        EnableWindow(albedoTexture_, enabled);
        EnableWindow(applyLookButton_, enabled);
        if (part == nullptr) {
            return;
        }
        SetWindowTextW(albedoR_, FormatTransformValue(part->albedoColor.x).c_str());
        SetWindowTextW(albedoG_, FormatTransformValue(part->albedoColor.y).c_str());
        SetWindowTextW(albedoB_, FormatTransformValue(part->albedoColor.z).c_str());
        SetWindowTextW(roughnessEdit_, FormatTransformValue(part->roughness).c_str());
        SetWindowTextW(metallicEdit_, FormatTransformValue(part->metallic).c_str());
        SetWindowTextW(albedoTexture_, Widen(part->albedoTexture).c_str());
    }

    void ApplySelectedLook() {
        ri::content::PrimitiveModelPart* part = SelectedPart();
        const auto r = ReadFiniteFloat(albedoR_);
        const auto g = ReadFiniteFloat(albedoG_);
        const auto b = ReadFiniteFloat(albedoB_);
        const auto roughness = ReadFiniteFloat(roughnessEdit_);
        const auto metallic = ReadFiniteFloat(metallicEdit_);
        if (part == nullptr || !r.has_value() || !g.has_value() || !b.has_value()
            || !roughness.has_value() || !metallic.has_value()) {
            MessageBoxW(hwnd_, L"Select a part and enter finite RGB / rough / metal values.",
                        L"Forge look", MB_OK | MB_ICONWARNING);
            return;
        }
        part->albedoColor = {*r, *g, *b};
        part->roughness = std::clamp(*roughness, 0.0F, 1.0F);
        part->metallic = std::clamp(*metallic, 0.0F, 1.0F);
        part->albedoTexture = ReadControlTextUtf8(albedoTexture_);
        if (!ri::content::SavePrimitiveModelDocument(editableModelPath_, *editableModel_)) {
            MessageBoxW(hwnd_, L"Could not save the stamped look.", L"Forge look", MB_OK | MB_ICONERROR);
            return;
        }
        StampWriteTime(editableModelPath_, editableModelWriteTime_, editableModelHasWriteTime_);
        SetWindowTextW(status_, Widen("Stamped look on " + part->name).c_str());
        RefreshCatalog(editableModelPath_);
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
        if (bay_ == kBayMotion) {
            ApplySelectedBonePose();
            return;
        }
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
        StampWriteTime(editableModelPath_, editableModelWriteTime_, editableModelHasWriteTime_);
        if (editableModelPath_ == previewedAssetPath_) {
            StampWriteTime(editableModelPath_, previewedWriteTime_, previewHasWriteTime_);
        }
        ApplySelectedTransformToPreview();
        SyncTransformGizmo();
        PublishPreview();
        SetWindowTextW(status_, L"Applied transform to the live preview.");
    }

    void SyncEditableRig() {
        editableRig_.reset();
        editableRigPath_.clear();
        const fs::path rigPath = PreferredRigPath();
        if (rigPath.empty()) {
            return;
        }
        if (const auto loaded = ri::scene::LoadRigDefinition(rigPath)) {
            editableRig_ = *loaded;
            editableRigPath_ = rigPath;
        }
    }

    [[nodiscard]] bool ShouldPersistBoneRest() const {
        if (!editableRig_.has_value() || editableRigPath_.empty()) {
            return false;
        }
        if (!previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_) {
            return true;
        }
        return !editableAnim_.has_value() || boundClip_.nodeTracks.empty();
    }

    void SyncMotionPoseControls() {
        if (bay_ != kBayMotion) {
            return;
        }
        const bool restEdit = ShouldPersistBoneRest();
        SetWindowTextW(applyTransformButton_, restEdit ? L"Set Rest" : L"Set Pose");
        ShowWindow(mirrorRestButton_, restEdit ? SW_SHOW : SW_HIDE);
        RECT client{};
        GetClientRect(hwnd_, &client);
        LayoutControls(client.right, client.bottom);
    }

    void ApplyEditableRigRestToPreview() {
        if (!editableRig_.has_value()) {
            return;
        }
        const std::size_t count =
            (std::min)(editableRig_->bones.size(), previewBoneNodes_.size());
        for (std::size_t index = 0; index < count; ++index) {
            const int node = previewBoneNodes_[index];
            if (node == ri::scene::kInvalidHandle
                || node < 0
                || static_cast<std::size_t>(node) >= previewScene_.NodeCount()) {
                continue;
            }
            previewScene_.GetNode(node).localTransform = editableRig_->bones[index].restLocal;
        }
        CaptureRestBoneWorld();
    }

    [[nodiscard]] bool SaveEditableRig(const bool showErrors = true, const bool syncPreview = true) {
        if (!editableRig_.has_value() || editableRigPath_.empty()) {
            return false;
        }
        const ri::scene::RigValidationReport validation = ri::scene::ValidateRigDefinition(*editableRig_);
        if (!validation.valid) {
            if (const auto reloaded = ri::scene::LoadRigDefinition(editableRigPath_)) {
                editableRig_ = *reloaded;
            }
            if (showErrors) {
                MessageBoxW(
                    hwnd_,
                    validation.errors.empty()
                        ? L"The rig would become invalid."
                        : Widen(validation.errors.front()).c_str(),
                    L"Forge rig",
                    MB_OK | MB_ICONWARNING);
            }
            return false;
        }
        if (!ri::scene::SaveRigDefinition(editableRigPath_, *editableRig_)) {
            if (const auto reloaded = ri::scene::LoadRigDefinition(editableRigPath_)) {
                editableRig_ = *reloaded;
            }
            if (showErrors) {
                MessageBoxW(hwnd_, L"Could not save the rig.", L"Forge rig", MB_OK | MB_ICONERROR);
            }
            return false;
        }
        if (syncPreview) {
            ApplyEditableRigRestToPreview();
            ApplySculptDisplayMesh();
        }
        return true;
    }

    void MirrorSelectedBoneRest() {
        if (!ShouldPersistBoneRest()) {
            MessageBoxW(
                hwnd_,
                L"Open a rig or bound clay without an active clip before mirroring rest.",
                L"Forge rig",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string sourceName = SelectedBoneName();
        if (sourceName.empty()) {
            MessageBoxW(
                hwnd_, L"Select a left/right bone to mirror.", L"Forge rig", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::optional<std::string> partnerName = ri::scene::MirrorPartnerBoneName(sourceName);
        if (!partnerName.has_value()) {
            MessageBoxW(
                hwnd_,
                L"Selected bone has no left/right partner to mirror onto.",
                L"Forge rig",
                MB_OK | MB_ICONWARNING);
            return;
        }
        if (!ri::scene::MirrorRigBoneRestAcrossX(*editableRig_, sourceName)) {
            MessageBoxW(
                hwnd_,
                Widen("Could not mirror " + sourceName + " onto " + *partnerName).c_str(),
                L"Forge rig",
                MB_OK | MB_ICONWARNING);
            return;
        }
        if (!SaveEditableRig()) {
            return;
        }
        for (std::size_t index = 0; index < listedBoneNodes_.size(); ++index) {
            const int node = listedBoneNodes_[index];
            if (node == ri::scene::kInvalidHandle) {
                continue;
            }
            if (node >= 0 && static_cast<std::size_t>(node) < previewScene_.NodeCount()
                && previewScene_.GetNode(node).name == *partnerName) {
                SendMessageW(boneList_, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
                PopulateBonePoseFields();
                SyncTransformGizmo();
                break;
            }
        }
        PublishPreview();
        SetWindowTextW(
            status_,
            Widen("Mirrored " + sourceName + " rest onto " + *partnerName + ".").c_str());
    }

    [[nodiscard]] bool PersistSelectedBoneRest(const bool showErrors = true) {
        if (!ShouldPersistBoneRest()) {
            return false;
        }
        const int node = SelectedBoneNode();
        if (node == ri::scene::kInvalidHandle
            || node < 0
            || static_cast<std::size_t>(node) >= previewScene_.NodeCount()) {
            return false;
        }
        const std::string& name = previewScene_.GetNode(node).name;
        const ri::scene::Transform& local = previewScene_.GetNode(node).localTransform;
        if (!ri::scene::SetRigBoneRestLocal(*editableRig_, name, local)) {
            if (showErrors) {
                MessageBoxW(
                    hwnd_,
                    L"Could not write the selected bone into the rig document.",
                    L"Forge rig",
                    MB_OK | MB_ICONWARNING);
            }
            return false;
        }
        restBoneLocal_[name] = local;
        if (!SaveEditableRig(showErrors)) {
            return false;
        }
        return true;
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
        const bool keepLiveSculpt =
            sculptDragging_
            || (editableSculpt_.has_value() && !editableSculptPath_.empty()
                && previewKey == editableSculptPath_);
        if (!force && previewInitialized_
            && ri::forge::ShouldReuseForgePreview(
                previewedAssetPath_,
                previewHasWriteTime_,
                previewedWriteTime_,
                previewKey,
                hasWriteTime,
                writeTime,
                keepLiveSculpt)) {
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
            Widen("HEARTH  |  loading " + asset->absolutePath.filename().string())
                .c_str());
    }

    [[nodiscard]] std::string ViewportControlHint() const {
        return std::string("RMB ORBIT  |  MMB/SHIFT+RMB PAN  |  WHEEL ZOOM  |  F FRAME  |  G GRID")
            + (showGrid_ ? " ON" : " OFF")
            + "  |  A AXES" + (showAxes_ ? " ON" : " OFF")
            + (editableModel_.has_value() && bay_ != kBayMotion ? "  |  LMB PICK / DRAG AXIS" : "")
            + (editableSculpt_.has_value() && bay_ != kBayMotion ? "  |  LMB CLAY" : "")
            + (!listedBoneNodes_.empty() && bay_ == kBayMotion ? "  |  LMB PICK / DRAG BONE" : "")
            + (!listedBoneNodes_.empty() && bay_ == kBayStock && editableModel_.has_value()
                ? "  |  LMB PICK BONE"
                : "")
            + (BoundSculptSkinnable() ? "  |  MOTION SKINS CLAY" : "")
            + (!restStockParts_.empty() ? "  |  MOTION SKINS PARTS" : "")
            + (editableSculpt_.has_value() && bay_ == kBayClay ? "  |  B PAINT" : "")
            + "  |  H WGHT" + (showWeights_ ? " ON" : " OFF")
            + "  |  W WIRE" + (showWireframe_ ? " ON" : " OFF")
            + "  |  N NRML" + (showNormals_ ? " ON" : " OFF")
            + "  |  C COLL" + (showCollision_ ? " ON" : " OFF")
            + "  |  X/Y/Z MIRROR"
            + (sculptMirrorX_ || sculptMirrorY_ || sculptMirrorZ_
                ? std::string(" ")
                    + (sculptMirrorX_ ? "X" : "")
                    + (sculptMirrorY_ ? "Y" : "")
                    + (sculptMirrorZ_ ? "Z" : "")
                : std::string(" OFF"));
    }

    void SyncPreviewOverlays(const bool republish) {
        previewOptions_.hiddenNodeHandles.clear();
        if (!showGrid_ && previewGridNode_ != ri::scene::kInvalidHandle) {
            previewOptions_.hiddenNodeHandles.push_back(previewGridNode_);
        }
        if (!showAxes_ && previewAxesNode_ != ri::scene::kInvalidHandle) {
            previewOptions_.hiddenNodeHandles.push_back(previewAxesNode_);
        }
        if (!showWireframe_ && previewWireframeNode_ != ri::scene::kInvalidHandle) {
            previewOptions_.hiddenNodeHandles.push_back(previewWireframeNode_);
        }
        if (!showNormals_ && previewNormalsNode_ != ri::scene::kInvalidHandle) {
            previewOptions_.hiddenNodeHandles.push_back(previewNormalsNode_);
        }
        if (!showCollision_ && previewCollisionNode_ != ri::scene::kInvalidHandle) {
            previewOptions_.hiddenNodeHandles.push_back(previewCollisionNode_);
        }
        if ((!showWeights_ || SelectedBoneName().empty() || !CanShowSculptWeights())
            && previewWeightNode_ != ri::scene::kInvalidHandle) {
            previewOptions_.hiddenNodeHandles.push_back(previewWeightNode_);
        }
        if (!TransformGizmoVisible() && transformGizmo_.root != ri::scene::kInvalidHandle) {
            previewOptions_.hiddenNodeHandles.push_back(transformGizmo_.root);
            previewOptions_.hiddenNodeHandles.push_back(transformGizmo_.xAxis);
            previewOptions_.hiddenNodeHandles.push_back(transformGizmo_.yAxis);
            previewOptions_.hiddenNodeHandles.push_back(transformGizmo_.zAxis);
        }
        if (republish && previewCamera_.cameraNode != ri::scene::kInvalidHandle) {
            viewport_.Publish(
                previewScene_,
                previewCamera_.cameraNode,
                previewOptions_,
                0.0,
                true);
        }
    }

    void RefreshViewportHeading() {
        const auto elapsed = static_cast<long long>(std::llround(previewElapsedMilliseconds_));
        std::string heading = "HEARTH  |  " + previewStatus_;
        if (!previewedAssetPath_.empty()) {
            heading += "  |  " + std::to_string(previewRenderableCount_) + " renderables  |  "
                + std::to_string(elapsed) + " ms";
        }
        heading += "  |  " + ViewportControlHint();
        SetWindowTextW(viewportHeading_, Widen(heading).c_str());
    }

    void Apply3DPreview(ri::forge::ForgePreviewBuildResult result) {
        if (sculptDirty_ && editableSculpt_.has_value() && !editableSculptPath_.empty()
            && result.assetPath != editableSculptPath_) {
            return;
        }
        const bool keepView = previewCamera_.cameraNode != ri::scene::kInvalidHandle
            && !result.assetPath.empty()
            && result.assetPath == appliedPreviewAssetPath_;
        const ri::scene::OrbitCameraState keptOrbit = previewCamera_.orbit;
        const bool keepLiveSculpt = editableSculpt_.has_value()
            && !editableSculptPath_.empty()
            && result.assetPath == editableSculptPath_;
        const bool incomingHasBones = !result.boneNodes.empty();
        const bool keepLiveAnim = editableAnim_.has_value()
            && !editableAnimPath_.empty()
            && (result.assetPath == editableAnimPath_
                || (!ri::forge::IsAnimationPath(result.assetPath) && incomingHasBones));
        const double keptAnimTime = animPlayer_.TimeSeconds();
        std::string keptBoneName;
        if (!pendingBoneSelect_.empty()) {
            keptBoneName = pendingBoneSelect_;
            pendingBoneSelect_.clear();
        } else if (const int bone = SelectedBoneNode(); bone != ri::scene::kInvalidHandle) {
            keptBoneName = previewScene_.GetNode(bone).name;
        }

        previewScene_ = std::move(result.scene);
        previewCamera_ = result.camera;
        previewFrameNodes_ = std::move(result.frameNodes);
        previewPartIds_ = std::move(result.partIds);
        previewGroupNodes_ = std::move(result.groupNodes);
        previewGroupIds_ = std::move(result.groupIds);
        transformGizmo_ = {};
        previewBoneNodes_ = std::move(result.boneNodes);
        CaptureRestBoneWorld();
        previewGridNode_ = result.gridNode;
        previewAxesNode_ = result.axesNode;
        previewSculptNode_ = result.sculptNode;
        previewWireframeNode_ = result.sculptWireframeNode;
        previewNormalsNode_ = result.sculptNormalsNode;
        previewCollisionNode_ = result.sculptCollisionNode;
        previewBrushCursorNode_ = result.sculptBrushCursorNode;
        previewWeightNode_ = ri::scene::kInvalidHandle;
        previewStatus_ = std::move(result.status);
        previewRenderableCount_ = result.renderableNodeCount;
        previewElapsedMilliseconds_ = result.elapsedMilliseconds;
        previewOptions_.textureRoot = workspaceRoot_ / "Assets" / "Textures";
        previewOptions_.fogStrength = 0.2F;
        previewOptions_.orderedDither = false;
        appliedPreviewAssetPath_ = result.assetPath;
        if (!keepLiveSculpt) {
            editableSculpt_.reset();
            editableSculptPath_.clear();
            sculptUndo_.Clear();
            sculptStrokeCaptured_ = false;
            sculptWeightStroke_ = false;
            sculptDirty_ = false;
            if (previewSculptNode_ != ri::scene::kInvalidHandle && !result.assetPath.empty()) {
                editableSculpt_ = ri::content::LoadNativeSculptDocument(result.assetPath);
                editableSculptPath_ = result.assetPath;
            }
        } else if (previewSculptNode_ != ri::scene::kInvalidHandle) {
            ri::scene::WriteNativeSculptMesh(
                previewScene_, previewSculptNode_, editableSculpt_->mesh);
            UpdateVisibleSculptOverlays(editableSculpt_->mesh);
        }
        animPlayer_.Stop();
        boundClip_ = {};
        if (!keepLiveAnim) {
            editableAnim_.reset();
            editableAnimPath_.clear();
            if (!result.assetPath.empty() && ri::forge::IsAnimationPath(result.assetPath)) {
                editableAnim_ = ri::content::LoadNativeAnimationDocument(result.assetPath);
                editableAnimPath_ = result.assetPath;
            }
        }
        RefreshBoneList(keptBoneName);
        if (editableAnim_.has_value() && !previewBoneNodes_.empty()) {
            boundClip_ = ri::scene::BindNativeAnimationClip(
                *editableAnim_, previewScene_, previewBoneNodes_);
            animPlayer_.SetClip(&boundClip_);
            animPlayer_.SetLooping(editableAnim_->looping);
            animPlayer_.SetTimeSeconds(keepLiveAnim ? keptAnimTime : 0.0);
            ApplyClipPoseToPreview();
        }
        PopulateClipList();
        if (previewSculptNode_ != ri::scene::kInvalidHandle) {
            const int parent = previewScene_.GetNode(previewSculptNode_).parent;
            previewWeightNode_ =
                ri::scene::InstantiateNativeSculptWeightOverlay(previewScene_, parent);
        }
        CaptureStockPartRest();
        ApplySculptDisplayMesh();
        if (keepView) {
            ri::scene::SetOrbitCameraState(previewScene_, previewCamera_, keptOrbit);
        }
        UpdateLoopButton();
        SyncAnimScrubFromPlayer();
        SyncAnimDurationFromClip();
        PopulateTransformFields();
        SyncEditableRig();
        SyncAuthoringEnable();
        SyncMotionPoseControls();
        SyncTransformGizmo();
        SyncPreviewOverlays(false);
        viewport_.Publish(
            previewScene_,
            previewCamera_.cameraNode,
            previewOptions_,
            0.0,
            true);
        RefreshViewportHeading();
        SyncForgeTitle();
    }

    void PollPreviewBuilder() {
        std::optional<ri::forge::ForgePreviewBuildResult> result = previewBuilder_.Poll();
        if (!result.has_value() || result->assetPath != previewedAssetPath_) {
            return;
        }
        if (sculptDirty_ && editableSculpt_.has_value() && !editableSculptPath_.empty()
            && result->assetPath != editableSculptPath_) {
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

    void Pan3DPreview(const int x, const int y) {
        if (!panDragging_) {
            return;
        }
        const int deltaX = x - lastOrbitPoint_.x;
        const int deltaY = y - lastOrbitPoint_.y;
        lastOrbitPoint_ = POINT{x, y};
        const float yawRad = previewCamera_.orbit.yawDegrees * (3.14159265F / 180.0F);
        const float pitchRad = previewCamera_.orbit.pitchDegrees * (3.14159265F / 180.0F);
        const float cosPitch = std::cos(pitchRad);
        const float sinPitch = std::sin(pitchRad);
        const float sinYaw = std::sin(yawRad);
        const float cosYaw = std::cos(yawRad);
        const ri::math::Vec3 forward{cosPitch * sinYaw, sinPitch, cosPitch * cosYaw};
        const ri::math::Vec3 worldUp{0.0F, 1.0F, 0.0F};
        ri::math::Vec3 right = ri::math::Cross(forward, worldUp);
        if (ri::math::LengthSquared(right) < 1.0e-6F) {
            right = ri::math::Vec3{1.0F, 0.0F, 0.0F};
        } else {
            right = ri::math::Normalize(right);
        }
        const ri::math::Vec3 up = ri::math::Normalize(ri::math::Cross(right, forward));
        const float scale = std::max(0.0015F, previewCamera_.orbit.distance * 0.0018F);
        previewCamera_.orbit.target = previewCamera_.orbit.target
            + right * (-static_cast<float>(deltaX) * scale)
            + up * (static_cast<float>(deltaY) * scale);
        ri::scene::SetOrbitCameraState(previewScene_, previewCamera_, previewCamera_.orbit);
        viewport_.Publish(
            previewScene_,
            previewCamera_.cameraNode,
            previewOptions_,
            0.0,
            false);
    }

    void Frame3DPreview() {
        if (previewCamera_.cameraNode == ri::scene::kInvalidHandle) {
            return;
        }
        std::vector<int> frameTargets = previewFrameNodes_;
        if (bay_ == kBayMotion) {
            const int bone = SelectedBoneNode();
            if (bone != ri::scene::kInvalidHandle) {
                frameTargets = {bone};
            } else if (previewSculptNode_ != ri::scene::kInvalidHandle) {
                frameTargets = {previewSculptNode_};
            }
        } else if (bay_ == kBayClay && previewSculptNode_ != ri::scene::kInvalidHandle) {
            frameTargets = {previewSculptNode_};
        }
        if (frameTargets.empty()) {
            return;
        }
        if (ri::scene::FrameNodesWithOrbitCamera(
                previewScene_, previewCamera_, frameTargets, 1.45F)) {
            viewport_.Publish(
                previewScene_,
                previewCamera_.cameraNode,
                previewOptions_,
                0.0,
                false);
        }
    }

    [[nodiscard]] bool FocusIsTextEntry() const {
        const HWND focus = GetFocus();
        if (focus == nullptr) {
            return false;
        }
        wchar_t className[32]{};
        if (GetClassNameW(focus, className, 32) <= 0) {
            return false;
        }
        return _wcsicmp(className, L"EDIT") == 0 || _wcsicmp(className, L"ComboBox") == 0;
    }

    void TogglePreviewGrid() {
        showGrid_ = !showGrid_;
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
    }

    void TogglePreviewAxes() {
        showAxes_ = !showAxes_;
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
    }

    void TogglePreviewWireframe() {
        showWireframe_ = !showWireframe_;
        if (showWireframe_ && editableSculpt_.has_value()
            && previewWireframeNode_ != ri::scene::kInvalidHandle) {
            ri::scene::UpdateNativeSculptWireframe(
                previewScene_, previewWireframeNode_, editableSculpt_->mesh);
        }
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
    }

    void TogglePreviewNormals() {
        showNormals_ = !showNormals_;
        if (showNormals_ && editableSculpt_.has_value()
            && previewNormalsNode_ != ri::scene::kInvalidHandle) {
            ri::scene::UpdateNativeSculptNormals(
                previewScene_, previewNormalsNode_, editableSculpt_->mesh);
        }
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
    }

    void TogglePreviewCollision() {
        showCollision_ = !showCollision_;
        if (showCollision_ && editableSculpt_.has_value()
            && previewCollisionNode_ != ri::scene::kInvalidHandle) {
            ri::scene::UpdateNativeSculptCollisionBounds(
                previewScene_, previewCollisionNode_, editableSculpt_->mesh);
        }
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
    }

    void TogglePreviewWeights() {
        showWeights_ = !showWeights_;
        ApplySculptDisplayMesh();
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
        SetWindowTextW(status_, showWeights_ ? L"Weight highlight on selected bone." : L"Weight highlight off.");
    }

    void ToggleSculptMirror(const char axis) {
        const char* label = "X";
        bool* flag = &sculptMirrorX_;
        if (axis == 'y') {
            label = "Y";
            flag = &sculptMirrorY_;
        } else if (axis == 'z') {
            label = "Z";
            flag = &sculptMirrorZ_;
        }
        *flag = !*flag;
        SetWindowTextW(
            status_,
            Widen(std::string(label) + "-mirror " + (*flag ? "on" : "off")
                + ". Strokes copy across that plane.").c_str());
        RefreshViewportHeading();
    }

    void UpdateVisibleSculptOverlays(const ri::scene::Mesh& mesh) {
        if (showWireframe_ && previewWireframeNode_ != ri::scene::kInvalidHandle) {
            ri::scene::UpdateNativeSculptWireframe(previewScene_, previewWireframeNode_, mesh);
        }
        if (showNormals_ && previewNormalsNode_ != ri::scene::kInvalidHandle) {
            ri::scene::UpdateNativeSculptNormals(previewScene_, previewNormalsNode_, mesh);
        }
        if (showCollision_ && previewCollisionNode_ != ri::scene::kInvalidHandle) {
            ri::scene::UpdateNativeSculptCollisionBounds(previewScene_, previewCollisionNode_, mesh);
        }
    }

    void ChangeSculptDensity(const int step) {
        if (!editableSculpt_.has_value() || previewSculptNode_ == ri::scene::kInvalidHandle) {
            return;
        }
        const ri::content::NativeSculptDocument previous = *editableSculpt_;
        const int around = editableSculpt_->segmentsAround + step * 8;
        const int down = editableSculpt_->segmentsDown + step * 4;
        if (!ri::scene::RebuildNativeSculptDensity(*editableSculpt_, around, down)) {
            SetWindowTextW(
                status_,
                L"Density unchanged. Extruded clay densifies with 0; 9 will not coarsen it.");
            return;
        }
        sculptUndo_.Capture(previous);
        sculptDirty_ = true;
        RebindLiveSculptIfNeeded(true);
        ApplySculptMeshToScene();
        SaveActiveSculpt();
        SetWindowTextW(
            status_,
            Widen("Sculpt density " + std::to_string(editableSculpt_->segmentsAround) + " x "
                + std::to_string(editableSculpt_->segmentsDown) + " ("
                + std::to_string(editableSculpt_->mesh.positions.size()) + " verts)").c_str());
    }

    void PublishPreview() {
        if (previewCamera_.cameraNode == ri::scene::kInvalidHandle) {
            return;
        }
        viewport_.Publish(
            previewScene_,
            previewCamera_.cameraNode,
            previewOptions_,
            0.0,
            true);
    }

    void ApplySculptMeshToScene() {
        if (!editableSculpt_.has_value() || previewSculptNode_ == ri::scene::kInvalidHandle) {
            return;
        }
        ri::scene::WriteNativeSculptMesh(previewScene_, previewSculptNode_, editableSculpt_->mesh);
        UpdateVisibleSculptOverlays(editableSculpt_->mesh);
        UpdateWeightOverlay(editableSculpt_->mesh);
        PublishPreview();
    }

    void UndoSculptStroke() {
        if (!editableSculpt_.has_value() || !sculptUndo_.Undo(*editableSculpt_)) {
            return;
        }
        sculptDirty_ = true;
        RebindLiveSculptIfNeeded();
        ApplySculptDisplayMesh();
        SyncPreviewOverlays(false);
        PublishPreview();
        SyncForgeTitle();
        SetWindowTextW(status_, L"Undid clay edit. Ctrl+Y redo. Ctrl+S saves.");
    }

    void RedoSculptStroke() {
        if (!editableSculpt_.has_value() || !sculptUndo_.Redo(*editableSculpt_)) {
            return;
        }
        sculptDirty_ = true;
        RebindLiveSculptIfNeeded();
        ApplySculptDisplayMesh();
        SyncPreviewOverlays(false);
        PublishPreview();
        SyncForgeTitle();
        SetWindowTextW(status_, L"Redid clay edit.");
    }

    void CreateNativeSculpt(const std::string_view cage = "sphere") {
        std::string error;
        const fs::path output = ri::forge::CreateUniqueNativeSculpt(workspaceRoot_, cage, &error);
        if (output.empty()) {
            MessageBoxW(hwnd_, Widen(error).c_str(), L"Forge", MB_OK | MB_ICONERROR);
            return;
        }
        SetWindowTextW(status_, Widen("Created native sculpt " + output.string()).c_str());
        RefreshCatalog(output);
    }

    static bool StampWriteTime(
        const fs::path& path,
        fs::file_time_type& writeTime,
        bool& hasWriteTime) {
        std::error_code error{};
        const fs::file_time_type stamped = fs::last_write_time(path, error);
        hasWriteTime = !error;
        if (!error) {
            writeTime = stamped;
        }
        return !error;
    }

    void SyncForgeTitle() {
        std::wstring title = L"Raw Iron Forge";
        fs::path shown = previewedAssetPath_;
        if (!editableSculptPath_.empty()) {
            shown = editableSculptPath_;
        } else if (!editableModelPath_.empty()) {
            shown = editableModelPath_;
        }
        if (!shown.empty()) {
            title += L"  —  ";
            title += shown.filename().wstring();
            if (sculptDirty_) {
                title += L"  •  unsaved clay";
            }
        }
        SetWindowTextW(hwnd_, title.c_str());
    }

    [[nodiscard]] bool FlushLiveSculpt() {
        if (!sculptDirty_) {
            return true;
        }
        SaveActiveSculpt();
        return !sculptDirty_;
    }

    bool SaveActiveSculpt() {
        if (!editableSculpt_.has_value() || editableSculptPath_.empty()) {
            return true;
        }
        if (!ri::content::SaveNativeSculptDocument(editableSculptPath_, *editableSculpt_)) {
            SetWindowTextW(status_, L"Could not save the native sculpt.");
            return false;
        }
        sculptDirty_ = false;
        if (editableSculptPath_ == previewedAssetPath_) {
            StampWriteTime(editableSculptPath_, previewedWriteTime_, previewHasWriteTime_);
        }
        SyncForgeTitle();
        SetWindowTextW(status_, Widen("Saved native sculpt " + editableSculptPath_.filename().string()).c_str());
        return true;
    }

    void RebindLiveSculptIfNeeded(const bool replaceAssigned = false) {
        if (!editableSculpt_.has_value() || editableSculpt_->rigPath.empty()) {
            return;
        }
        if (!replaceAssigned
            && editableSculpt_->vertexBoneNames.size() == editableSculpt_->mesh.positions.size()) {
            return;
        }
        const auto rig = ri::forge::LoadSidecarRig(editableSculpt_->rigPath, editableSculptPath_);
        if (!rig.has_value()) {
            return;
        }
        (void)ri::scene::BindNativeSculptToRig(
            *editableSculpt_, *rig, editableSculpt_->rigPath, replaceAssigned);
    }

    [[nodiscard]] bool BoundSculptSkinnable() const {
        return CanShowSculptWeights()
            && !restBoneWorld_.empty()
            && !listedBoneNodes_.empty();
    }

    [[nodiscard]] bool CanShowSculptWeights() const {
        return editableSculpt_.has_value()
            && !editableSculpt_->vertexBoneNames.empty()
            && editableSculpt_->vertexBoneNames.size() == editableSculpt_->mesh.positions.size();
    }

    void CaptureRestBoneWorld() {
        restBoneWorld_.clear();
        restBoneLocal_.clear();
        for (const int node : previewBoneNodes_) {
            if (node == ri::scene::kInvalidHandle
                || node < 0
                || static_cast<std::size_t>(node) >= previewScene_.NodeCount()) {
                continue;
            }
            const std::string& name = previewScene_.GetNode(node).name;
            if (!name.empty()) {
                restBoneWorld_[name] = previewScene_.ComputeWorldMatrix(node);
                restBoneLocal_[name] = previewScene_.GetNode(node).localTransform;
            }
        }
    }

    void RestoreRestBoneLocals() {
        for (const int node : previewBoneNodes_) {
            if (node == ri::scene::kInvalidHandle
                || node < 0
                || static_cast<std::size_t>(node) >= previewScene_.NodeCount()) {
                continue;
            }
            const std::string& name = previewScene_.GetNode(node).name;
            const auto rest = restBoneLocal_.find(name);
            if (!name.empty() && rest != restBoneLocal_.end()) {
                previewScene_.GetNode(node).localTransform = rest->second;
            }
        }
    }

    void ApplyClipPoseToPreview() {
        animPlayer_.Apply(previewScene_);
        if (!editableAnim_.has_value() || editableAnim_->rootMotion) {
            return;
        }
        const int root = ri::scene::FindNativeAnimationRootMotionNode(
            previewScene_, previewBoneNodes_);
        if (root == ri::scene::kInvalidHandle) {
            return;
        }
        ri::scene::Transform rest{};
        const std::string& name = previewScene_.GetNode(root).name;
        if (const auto found = restBoneLocal_.find(name); found != restBoneLocal_.end()) {
            rest = found->second;
        }
        ri::scene::HoldNativeAnimationRootInPlace(previewScene_, root, rest);
    }

    [[nodiscard]] std::unordered_map<std::string, ri::math::Mat4> CollectPosedBoneWorld() const {
        std::unordered_map<std::string, ri::math::Mat4> posed{};
        for (const int node : listedBoneNodes_) {
            if (node == ri::scene::kInvalidHandle
                || node < 0
                || static_cast<std::size_t>(node) >= previewScene_.NodeCount()) {
                continue;
            }
            const std::string& name = previewScene_.GetNode(node).name;
            if (!name.empty()) {
                posed[name] = previewScene_.ComputeWorldMatrix(node);
            }
        }
        return posed;
    }

    void UpdateWeightOverlay(const ri::scene::Mesh& displayed) {
        if (previewWeightNode_ == ri::scene::kInvalidHandle || !editableSculpt_.has_value()) {
            return;
        }
        const std::string selected = showWeights_ ? SelectedBoneName() : std::string{};
            ri::scene::UpdateNativeSculptWeightOverlay(
            previewScene_,
            previewWeightNode_,
            displayed,
            editableSculpt_->vertexBoneNames,
            selected,
            editableSculpt_->vertexInfluences);
    }

    void ApplySculptDisplayMesh() {
        if (editableSculpt_.has_value() && previewSculptNode_ != ri::scene::kInvalidHandle) {
            ri::scene::Mesh displayed = editableSculpt_->mesh;
            if (bay_ == kBayMotion && BoundSculptSkinnable()) {
                (void)ri::scene::SkinRigidMesh(
                    displayed,
                    editableSculpt_->vertexBoneNames,
                    editableSculpt_->mesh.positions,
                    editableSculpt_->mesh.normals,
                    restBoneWorld_,
                    CollectPosedBoneWorld(),
                    editableSculpt_->vertexInfluences);
            }
            ri::scene::WriteNativeSculptMesh(previewScene_, previewSculptNode_, displayed);
            UpdateVisibleSculptOverlays(displayed);
            UpdateWeightOverlay(displayed);
        }
        ApplyStockPartPose();
    }

    void CaptureStockPartRest() {
        restStockParts_.clear();
        if (!editableModel_.has_value()) {
            return;
        }
        const std::size_t count =
            (std::min)(previewPartIds_.size(), previewFrameNodes_.size());
        const std::vector<int> partNodes(
            previewFrameNodes_.begin(),
            previewFrameNodes_.begin() + static_cast<std::vector<int>::difference_type>(count));
        restStockParts_ = ri::scene::CaptureBoundPrimitivePartRest(
            previewScene_, partNodes, previewPartIds_, *editableModel_);
    }

    void ApplyStockPartPose() {
        if (restStockParts_.empty()) {
            return;
        }
        if (bay_ == kBayMotion && !restBoneWorld_.empty()) {
            (void)ri::scene::PoseBoundPrimitiveParts(
                previewScene_, restStockParts_, restBoneWorld_, CollectPosedBoneWorld());
        } else {
            ri::scene::RestoreBoundPrimitiveParts(previewScene_, restStockParts_);
        }
    }

    [[nodiscard]] fs::path PreferredRigPath() const {
        if (const ri::forge::AssetEntry* selected = SelectedAsset();
            selected != nullptr && selected->kind == ri::forge::AssetKind::Rig && selected->valid) {
            return selected->absolutePath;
        }
        const auto resolveSidecar = [this](const std::string_view rigPath, const fs::path& document) {
            return ri::forge::ResolveCatalogRigPath(catalog_, rigPath, document);
        };
        if (editableSculpt_.has_value() && !editableSculpt_->rigPath.empty()) {
            if (const fs::path resolved = resolveSidecar(editableSculpt_->rigPath, editableSculptPath_);
                !resolved.empty()) {
                return resolved;
            }
        }
        if (editableModel_.has_value() && !editableModel_->rigPath.empty()) {
            if (const fs::path resolved = resolveSidecar(editableModel_->rigPath, editableModelPath_);
                !resolved.empty()) {
                return resolved;
            }
        }
        if (editableAnim_.has_value() && !editableAnim_->rigPath.empty()) {
            if (const fs::path resolved = resolveSidecar(editableAnim_->rigPath, editableAnimPath_);
                !resolved.empty()) {
                return resolved;
            }
        }
        std::error_code error{};
        if (!lastRigPath_.empty() && fs::is_regular_file(lastRigPath_, error)) {
            return lastRigPath_;
        }
        for (auto iterator = catalog_.entries.rbegin(); iterator != catalog_.entries.rend(); ++iterator) {
            if (iterator->kind == ri::forge::AssetKind::Rig && iterator->valid) {
                return iterator->absolutePath;
            }
        }
        return {};
    }

    [[nodiscard]] fs::path PreviewDocumentPath() const {
        if (!editableSculptPath_.empty()) {
            return editableSculptPath_;
        }
        if (!editableModelPath_.empty()) {
            return editableModelPath_;
        }
        if (!editableAnimPath_.empty()) {
            return editableAnimPath_;
        }
        return previewedAssetPath_;
    }

    [[nodiscard]] std::string CurrentPreviewRigRelative() const {
        if (editableSculpt_.has_value() && !editableSculpt_->rigPath.empty()) {
            return editableSculpt_->rigPath;
        }
        if (editableModel_.has_value() && !editableModel_->rigPath.empty()) {
            return editableModel_->rigPath;
        }
        if (editableAnim_.has_value() && !editableAnim_->rigPath.empty()) {
            return editableAnim_->rigPath;
        }
        if (const ri::forge::AssetEntry* selected = SelectedAsset();
            selected != nullptr && selected->kind == ri::forge::AssetKind::Rig) {
            return selected->relativePath;
        }
        if (!lastRigPath_.empty()) {
            return ri::forge::RelativeSourcePath(workspaceRoot_, lastRigPath_);
        }
        if (!previewedAssetPath_.empty() && ri::forge::IsRigPath(previewedAssetPath_)) {
            return ri::forge::RelativeSourcePath(workspaceRoot_, previewedAssetPath_);
        }
        return {};
    }

    [[nodiscard]] bool CanBindClipOntoLivePreview(const fs::path& clipPath) const {
        if (clipPath.empty() || previewBoneNodes_.empty()) {
            return false;
        }
        const std::vector<std::size_t> indices = ri::forge::AnimationIndicesForRig(
            catalog_, CurrentPreviewRigRelative(), PreviewDocumentPath());
        for (const std::size_t index : indices) {
            if (index < catalog_.entries.size()
                && catalog_.entries[index].absolutePath == clipPath) {
                return true;
            }
        }
        return false;
    }

    void PopulateClipList() {
        if (clipList_ == nullptr) {
            return;
        }
        clipListSyncing_ = true;
        SendMessageW(clipList_, CB_RESETCONTENT, 0, 0);
        clipListPaths_.clear();
        clipListPaths_.emplace_back();
        SendMessageW(clipList_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"(no clip)"));
        const std::vector<std::size_t> indices = ri::forge::AnimationIndicesForRig(
            catalog_, CurrentPreviewRigRelative(), PreviewDocumentPath());
        int selected = 0;
        for (const std::size_t index : indices) {
            if (index >= catalog_.entries.size()) {
                continue;
            }
            const ri::forge::AssetEntry& entry = catalog_.entries[index];
            clipListPaths_.push_back(entry.absolutePath);
            const std::wstring label = Widen(entry.absolutePath.filename().string());
            SendMessageW(clipList_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            if (!editableAnimPath_.empty() && entry.absolutePath == editableAnimPath_) {
                selected = static_cast<int>(clipListPaths_.size() - 1);
            }
        }
        SendMessageW(clipList_, CB_SETCURSEL, static_cast<WPARAM>(selected), 0);
        clipListSyncing_ = false;
    }

    void BindClipOntoPreview(const fs::path& clipPath) {
        animPlayer_.Stop();
        if (clipPath.empty()) {
            editableAnim_.reset();
            editableAnimPath_.clear();
            boundClip_ = {};
            animPlayer_.SetClip(nullptr);
            RestoreRestBoneLocals();
            ApplySculptDisplayMesh();
            PublishPreview();
            PopulateClipList();
            UpdateLoopButton();
            SyncAnimScrubFromPlayer();
            SyncAnimDurationFromClip();
            SyncAnimWindowFromClip();
            PopulateBonePoseFields();
            PopulateEventList();
            SyncAuthoringEnable();
            RefreshViewportHeading();
            SetWindowTextW(status_, L"Rest pose. MOTION CLIP list plays a clip on this mesh.");
            return;
        }
        auto loaded = ri::content::LoadNativeAnimationDocument(clipPath);
        if (!loaded.has_value()) {
            SetWindowTextW(status_, Widen("Could not load motion clip " + clipPath.string()).c_str());
            return;
        }
        editableAnim_ = std::move(loaded);
        editableAnimPath_ = clipPath;
        if (const fs::path resolved =
                ri::forge::ResolveCatalogRigPath(catalog_, editableAnim_->rigPath, editableAnimPath_);
            !resolved.empty()) {
            lastRigPath_ = resolved;
        }
        boundClip_ = {};
        if (!previewBoneNodes_.empty()) {
            boundClip_ = ri::scene::BindNativeAnimationClip(
                *editableAnim_, previewScene_, previewBoneNodes_);
            animPlayer_.SetClip(&boundClip_);
            animPlayer_.SetLooping(editableAnim_->looping);
            animPlayer_.SetTimeSeconds(0.0);
            ApplyClipPoseToPreview();
        }
        ApplySculptDisplayMesh();
        PublishPreview();
        PopulateClipList();
        UpdateLoopButton();
        SyncAnimScrubFromPlayer();
        SyncAnimDurationFromClip();
        SyncAnimWindowFromClip();
        SetWindowTextW(animTime_, FormatTransformValue(static_cast<float>(animPlayer_.TimeSeconds())).c_str());
        PopulateBonePoseFields();
        PopulateEventList();
        SyncAuthoringEnable();
        RefreshViewportHeading();
        std::string status = "Motion clip " + clipPath.filename().string() + " "
            + (boundClip_.nodeTracks.empty() ? "loaded" : "bound on the open mesh") + ".";
        if (!previewBoneNodes_.empty()) {
            const ri::scene::NativeAnimationBindReport report = ri::scene::DiagnoseNativeAnimationBind(
                *editableAnim_, previewScene_, previewBoneNodes_);
            status += " " + report.summary;
        }
        SetWindowTextW(status_, Widen(status).c_str());
        SyncMotionPoseControls();
        UpdateInspector();
    }

    void OnClipListSelChange() {
        if (clipListSyncing_ || clipList_ == nullptr) {
            return;
        }
        const LRESULT selection = SendMessageW(clipList_, CB_GETCURSEL, 0, 0);
        if (selection == CB_ERR || selection < 0
            || static_cast<std::size_t>(selection) >= clipListPaths_.size()) {
            return;
        }
        BindClipOntoPreview(clipListPaths_[static_cast<std::size_t>(selection)]);
    }

    [[nodiscard]] std::string SelectedBoneName() const {
        const int node = SelectedBoneNode();
        if (node == ri::scene::kInvalidHandle
            || node < 0
            || static_cast<std::size_t>(node) >= previewScene_.NodeCount()) {
            return {};
        }
        return previewScene_.GetNode(node).name;
    }

    void ApplySculptAt(const int x, const int y) {
        if (!editableSculpt_.has_value() || previewSculptNode_ == ri::scene::kInvalidHandle
            || previewCamera_.cameraNode == ri::scene::kInvalidHandle) {
            return;
        }
        const float width = static_cast<float>(viewportBounds_.right - viewportBounds_.left);
        const float height = static_cast<float>(viewportBounds_.bottom - viewportBounds_.top);
        if (width < 1.0F || height < 1.0F) {
            return;
        }
        const float u = (static_cast<float>(x - viewportBounds_.left) + 0.5F) / width;
        const float v = (static_cast<float>(y - viewportBounds_.top) + 0.5F) / height;
        const auto ray = ri::scene::BuildPerspectiveCameraRay(
            previewScene_, previewCamera_.cameraNode, u, v, width / height);
        if (!ray.has_value()) {
            ri::scene::UpdateNativeSculptBrushCursor(
                previewScene_, previewBrushCursorNode_, {}, sculptRadius_, false);
            return;
        }
        const auto hit = ri::scene::RaycastNode(previewScene_, previewSculptNode_, *ray);
        if (!hit.has_value()) {
            ri::scene::UpdateNativeSculptBrushCursor(
                previewScene_, previewBrushCursorNode_, {}, sculptRadius_, false);
            if (!sculptDragging_) {
                PublishPreview();
            }
            return;
        }
        ri::scene::UpdateNativeSculptBrushCursor(
            previewScene_, previewBrushCursorNode_, hit->position, sculptRadius_, true);
        if (!sculptDragging_) {
            PublishPreview();
            return;
        }
        ri::scene::Node& node = previewScene_.GetNode(previewSculptNode_);
        if (node.mesh == ri::scene::kInvalidHandle) {
            return;
        }
        ri::scene::Mesh& mesh = previewScene_.GetMesh(node.mesh);
        const bool holdingE = (GetKeyState('E') & 0x8000) != 0;
        const bool holdingShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const bool holdingCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool holdingAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        if (holdingE && sculptStrokeCaptured_ && !sculptWeightStroke_) {
            PublishPreview();
            return;
        }
        if (sculptWeightStroke_) {
            const std::string boneName = SelectedBoneName();
            ri::scene::NativeSculptWeightPaint mode = ri::scene::NativeSculptWeightPaint::Add;
            if (holdingShift) {
                mode = ri::scene::NativeSculptWeightPaint::Smooth;
            } else if (holdingAlt) {
                mode = ri::scene::NativeSculptWeightPaint::Clear;
            } else if (holdingCtrl) {
                mode = ri::scene::NativeSculptWeightPaint::Assign;
            }
            if ((mode == ri::scene::NativeSculptWeightPaint::Assign
                    || mode == ri::scene::NativeSculptWeightPaint::Add)
                && boneName.empty()) {
                SetWindowTextW(
                    status_,
                    L"Select a bone, then hold B and paint. B blends, B+Ctrl replaces, B+Shift smooths, B+Alt clears.");
                PublishPreview();
                return;
            }
            if (!sculptStrokeCaptured_) {
                sculptUndo_.Capture(*editableSculpt_);
                sculptStrokeCaptured_ = true;
            }
            const std::size_t painted = ri::scene::PaintNativeSculptWeights(
                *editableSculpt_,
                hit->position,
                sculptRadius_,
                boneName,
                mode,
                sculptMirrorX_,
                sculptMirrorY_,
                sculptMirrorZ_,
                std::clamp(sculptStrength_ * 6.0F, 0.15F, 1.0F));
            if (painted == 0U) {
                PublishPreview();
                return;
            }
            if (!showWeights_) {
                showWeights_ = true;
                RefreshViewportHeading();
            }
            if (!sculptDirty_) {
                sculptDirty_ = true;
                SyncForgeTitle();
            }
            ApplySculptDisplayMesh();
            SyncPreviewOverlays(false);
            PublishPreview();
            const char* verb = "Blended";
            std::string detail = " verts onto " + boneName;
            if (mode == ri::scene::NativeSculptWeightPaint::Assign) {
                verb = "Assigned";
            } else if (mode == ri::scene::NativeSculptWeightPaint::Smooth) {
                verb = "Smoothed";
                detail = " verts toward neighbors";
            } else if (mode == ri::scene::NativeSculptWeightPaint::Clear) {
                verb = "Cleared";
                detail = " verts";
            }
            SetWindowTextW(
                status_,
                Widen(std::string(verb) + " " + std::to_string(painted) + detail).c_str());
            return;
        }
        if (!sculptStrokeCaptured_) {
            sculptUndo_.Capture(*editableSculpt_);
            sculptStrokeCaptured_ = true;
        }
        ri::scene::NativeSculptStroke stroke{};
        stroke.worldPosition = hit->position;
        stroke.worldNormal = hit->normal;
        stroke.radius = sculptRadius_;
        stroke.strength = sculptStrength_;
        if (holdingE) {
            stroke.brush = ri::scene::NativeSculptBrush::Extrude;
        } else if (holdingShift && holdingCtrl) {
            stroke.brush = ri::scene::NativeSculptBrush::Flatten;
        } else if (holdingShift) {
            stroke.brush = ri::scene::NativeSculptBrush::Smooth;
        } else if (holdingCtrl) {
            stroke.brush = ri::scene::NativeSculptBrush::Inflate;
        } else {
            stroke.brush = ri::scene::NativeSculptBrush::Clay;
        }
        stroke.invert = holdingAlt;
        stroke.mirrorX = sculptMirrorX_;
        stroke.mirrorY = sculptMirrorY_;
        stroke.mirrorZ = sculptMirrorZ_;
        const bool applied = stroke.brush == ri::scene::NativeSculptBrush::Extrude
            ? ri::scene::ApplyNativeSculptFaceExtrude(
                mesh, stroke, &editableSculpt_->vertexBoneNames, &editableSculpt_->vertexInfluences)
            : ri::scene::ApplyNativeSculptStroke(mesh, stroke);
        if (!applied) {
            PublishPreview();
            return;
        }
        editableSculpt_->mesh = mesh;
        if (!sculptDirty_) {
            sculptDirty_ = true;
            SyncForgeTitle();
        }
        RebindLiveSculptIfNeeded();
        ApplySculptDisplayMesh();
        SyncPreviewOverlays(false);
        PublishPreview();
        SetWindowTextW(
            status_,
            Widen(std::string(ri::scene::NativeSculptBrushName(stroke.brush)) + " stroke").c_str());
    }

    [[nodiscard]] std::optional<ri::scene::Ray> ViewportRay(const int x, const int y) const {
        if (previewCamera_.cameraNode == ri::scene::kInvalidHandle) {
            return std::nullopt;
        }
        const float width = static_cast<float>(viewportBounds_.right - viewportBounds_.left);
        const float height = static_cast<float>(viewportBounds_.bottom - viewportBounds_.top);
        if (width < 1.0F || height < 1.0F) {
            return std::nullopt;
        }
        const float u = (static_cast<float>(x - viewportBounds_.left) + 0.5F) / width;
        const float v = (static_cast<float>(y - viewportBounds_.top) + 0.5F) / height;
        return ri::scene::BuildPerspectiveCameraRay(
            previewScene_, previewCamera_.cameraNode, u, v, width / height);
    }

    bool PickViewportAuthoringTarget(const int x, const int y) {
        if (editableSculpt_.has_value() && bay_ != kBayMotion) {
            return false;
        }
        const auto ray = ViewportRay(x, y);
        if (!ray.has_value()) {
            return false;
        }
        const std::vector<ri::scene::RaycastHit> hits = ri::scene::RaycastSceneAll(previewScene_, *ray);
        const auto skipOverlay = [&](const int node) {
            return node == previewGridNode_ || node == previewAxesNode_
                || node == previewBrushCursorNode_ || node == previewWireframeNode_
                || node == previewNormalsNode_ || node == previewCollisionNode_
                || node == previewWeightNode_
                || node == transformGizmo_.root || node == transformGizmo_.xAxis
                || node == transformGizmo_.yAxis || node == transformGizmo_.zAxis
                || (bay_ == kBayMotion && node == previewSculptNode_);
        };
        if (bay_ == kBayMotion) {
            for (const ri::scene::RaycastHit& hit : hits) {
                int node = hit.node;
                while (node != ri::scene::kInvalidHandle
                    && node >= 0
                    && static_cast<std::size_t>(node) < previewScene_.NodeCount()) {
                    if (skipOverlay(node)) {
                        break;
                    }
                    for (std::size_t index = 0; index < listedBoneNodes_.size(); ++index) {
                        if (listedBoneNodes_[index] != node) {
                            continue;
                        }
                        SendMessageW(boneList_, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
                        PopulateBonePoseFields();
                        SyncAuthoringEnable();
                        SyncTransformGizmo();
                        ApplySculptDisplayMesh();
                        SyncPreviewOverlays(false);
                        PublishPreview();
                        SetWindowTextW(
                            status_,
                            Widen("Picked bone " + previewScene_.GetNode(node).name
                                + ". Drag the RGB axes or Set Pose.")
                                .c_str());
                        return true;
                    }
                    node = previewScene_.GetNode(node).parent;
                }
            }
        }
        if (editableSculpt_.has_value()) {
            return false;
        }
        for (const ri::scene::RaycastHit& hit : hits) {
            int node = hit.node;
            while (node != ri::scene::kInvalidHandle
                && node >= 0
                && static_cast<std::size_t>(node) < previewScene_.NodeCount()) {
                if (node == previewGridNode_ || node == previewAxesNode_
                    || node == previewBrushCursorNode_ || node == previewWireframeNode_
                    || node == previewNormalsNode_ || node == previewCollisionNode_
                    || node == previewWeightNode_
                    || node == transformGizmo_.root || node == transformGizmo_.xAxis
                    || node == transformGizmo_.yAxis || node == transformGizmo_.zAxis) {
                    break;
                }
                for (std::size_t index = 0; index < previewFrameNodes_.size(); ++index) {
                    if (previewFrameNodes_[index] != node || index >= previewPartIds_.size()) {
                        continue;
                    }
                    if (SelectModelElementById(previewPartIds_[index])) {
                        SetWindowTextW(
                            status_,
                            Widen("Picked part " + previewPartIds_[index]
                                + ". Drag the RGB axes, or Set Place / Stamp Look.")
                                .c_str());
                        return true;
                    }
                }
                for (std::size_t index = 0; index < listedBoneNodes_.size(); ++index) {
                    if (listedBoneNodes_[index] != node) {
                        continue;
                    }
                    SendMessageW(boneList_, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
                    PopulateBonePoseFields();
                    SyncAuthoringEnable();
                    SyncTransformGizmo();
                    ApplySculptDisplayMesh();
                    SyncPreviewOverlays(false);
                    PublishPreview();
                    SetWindowTextW(
                        status_,
                        Widen("Picked bone " + previewScene_.GetNode(node).name + ".").c_str());
                    return true;
                }
                node = previewScene_.GetNode(node).parent;
            }
        }
        return false;
    }

    [[nodiscard]] bool TransformGizmoVisible() const {
        if (bay_ == kBayMotion) {
            return SelectedBoneNode() != ri::scene::kInvalidHandle;
        }
        return editableModel_.has_value() && !editableSculpt_.has_value()
            && SelectedPreviewNode() != ri::scene::kInvalidHandle;
    }

    [[nodiscard]] int SelectedGizmoNode() const {
        if (bay_ == kBayMotion) {
            return SelectedBoneNode();
        }
        return SelectedPreviewNode();
    }

    [[nodiscard]] int SelectedPreviewNode() const {
        const std::string id = CurrentModelElementId();
        if (id.empty()) {
            return ri::scene::kInvalidHandle;
        }
        for (std::size_t index = 0; index < previewPartIds_.size() && index < previewFrameNodes_.size();
             ++index) {
            if (previewPartIds_[index] == id) {
                return previewFrameNodes_[index];
            }
        }
        for (std::size_t index = 0; index < previewGroupIds_.size() && index < previewGroupNodes_.size();
             ++index) {
            if (previewGroupIds_[index] == id) {
                return previewGroupNodes_[index];
            }
        }
        return ri::scene::kInvalidHandle;
    }

    static float& DeclarativeComponent(ri::content::DeclarativeVec3& value, const int axis) {
        return axis == 0 ? value.x : (axis == 1 ? value.y : value.z);
    }

    [[nodiscard]] static std::optional<float> AxisRayParameter(
        const ri::scene::Ray& ray,
        const ri::math::Vec3& origin,
        const ri::math::Vec3& axis) {
        const ri::math::Vec3 w = ray.origin - origin;
        const float a = ri::math::Dot(ray.direction, ray.direction);
        const float b = ri::math::Dot(ray.direction, axis);
        const float c = ri::math::Dot(axis, axis);
        const float d = ri::math::Dot(ray.direction, w);
        const float e = ri::math::Dot(axis, w);
        const float denom = a * c - b * b;
        if (c < 1.0e-10F || std::abs(denom) < 1.0e-8F) {
            return std::nullopt;
        }
        return (a * e - b * d) / denom;
    }

    [[nodiscard]] ri::math::Vec3 ParentLocalAxisInWorld(const int node, const int axis) const {
        const int parent = previewScene_.GetNode(node).parent;
        const ri::math::Mat4 parentWorld = parent == ri::scene::kInvalidHandle
            ? ri::math::IdentityMatrix()
            : previewScene_.ComputeWorldMatrix(parent);
        const ri::math::Vec3 local =
            axis == 0 ? ri::math::Vec3{1.0F, 0.0F, 0.0F}
                      : (axis == 1 ? ri::math::Vec3{0.0F, 1.0F, 0.0F} : ri::math::Vec3{0.0F, 0.0F, 1.0F});
        return ri::math::TransformVector(parentWorld, local);
    }

    void ApplySelectedTransformToPreview() {
        const int node = SelectedPreviewNode();
        const ri::content::PrimitiveModelTransform* transform = SelectedElementTransform();
        if (node == ri::scene::kInvalidHandle || transform == nullptr) {
            return;
        }
        ri::scene::Transform& local = previewScene_.GetNode(node).localTransform;
        local.position = {transform->translation.x, transform->translation.y, transform->translation.z};
        local.rotationDegrees = {
            transform->rotationDegrees.x, transform->rotationDegrees.y, transform->rotationDegrees.z};
        local.scale = {transform->scale.x, transform->scale.y, transform->scale.z};
        if (bay_ != kBayMotion) {
            CaptureStockPartRest();
        }
    }

    void EnsureTransformGizmo() {
        if (transformGizmo_.root != ri::scene::kInvalidHandle) {
            return;
        }
        ri::scene::AxesHelperOptions options{};
        options.nodeName = "ForgeTransformGizmo";
        options.axisLength = 0.9F;
        options.axisThickness = 0.06F;
        transformGizmo_ = ri::scene::AddAxesHelper(previewScene_, options);
    }

    void SyncTransformGizmo() {
        if (!TransformGizmoVisible()) {
            return;
        }
        EnsureTransformGizmo();
        const int node = SelectedGizmoNode();
        if (node == ri::scene::kInvalidHandle || transformGizmo_.root == ri::scene::kInvalidHandle) {
            return;
        }
        const int parent = previewScene_.GetNode(node).parent;
        (void)previewScene_.SetParent(transformGizmo_.root, parent);
        ri::scene::Transform& gizmo = previewScene_.GetNode(transformGizmo_.root).localTransform;
        gizmo.position = previewScene_.GetNode(node).localTransform.position;
        gizmo.rotationDegrees = {};
        const float distance = std::max(previewCamera_.orbit.distance, 0.35F);
        const float visual = std::clamp(distance * 0.16F, 0.22F, 2.4F);
        gizmo.scale = {visual, visual, visual};
    }

    [[nodiscard]] int GizmoAxisFromNode(int node) const {
        while (node != ri::scene::kInvalidHandle
            && node >= 0
            && static_cast<std::size_t>(node) < previewScene_.NodeCount()) {
            if (node == transformGizmo_.xAxis) {
                return 0;
            }
            if (node == transformGizmo_.yAxis) {
                return 1;
            }
            if (node == transformGizmo_.zAxis) {
                return 2;
            }
            if (node == transformGizmo_.root) {
                return -1;
            }
            node = previewScene_.GetNode(node).parent;
        }
        return -1;
    }

    static float& Vec3Component(ri::math::Vec3& value, const int axis) {
        return axis == 0 ? value.x : (axis == 1 ? value.y : value.z);
    }

    static ri::math::Vec3* BonePoseVector(ri::scene::Transform& transform, const int mode) {
        if (mode == 1) {
            return &transform.rotationDegrees;
        }
        if (mode == 2) {
            return &transform.scale;
        }
        return &transform.position;
    }

    bool BeginGizmoDrag(const int x, const int y) {
        if (!TransformGizmoVisible()) {
            return false;
        }
        const auto ray = ViewportRay(x, y);
        if (!ray.has_value()) {
            return false;
        }
        const std::vector<ri::scene::RaycastHit> hits = ri::scene::RaycastSceneAll(previewScene_, *ray);
        int axis = -1;
        for (const ri::scene::RaycastHit& hit : hits) {
            axis = GizmoAxisFromNode(hit.node);
            if (axis >= 0) {
                break;
            }
        }
        const int node = SelectedGizmoNode();
        if (axis < 0 || node == ri::scene::kInvalidHandle) {
            return false;
        }
        gizmoBone_ = bay_ == kBayMotion;
        ri::content::PrimitiveModelTransform* transform =
            gizmoBone_ ? nullptr : SelectedElementTransform();
        if (!gizmoBone_ && transform == nullptr) {
            return false;
        }
        const LRESULT mode = SendMessageW(transformMode_, CB_GETCURSEL, 0, 0);
        gizmoDragging_ = true;
        gizmoDirty_ = false;
        gizmoAxis_ = axis;
        if (gizmoBone_) {
            gizmoStartValue_ = Vec3Component(
                *BonePoseVector(previewScene_.GetNode(node).localTransform, static_cast<int>(mode)),
                axis);
        } else {
            ri::content::DeclarativeVec3* value = &transform->translation;
            if (mode == 1) {
                value = &transform->rotationDegrees;
            } else if (mode == 2) {
                value = &transform->scale;
            }
            gizmoStartValue_ = DeclarativeComponent(*value, axis);
        }
        gizmoStartMouseX_ = x;
        gizmoStartParam_ = 0.0F;
        if (mode != 1) {
            if (const auto param = AxisRayParameter(
                    *ray, previewScene_.ComputeWorldPosition(node), ParentLocalAxisInWorld(node, axis));
                param.has_value()) {
                gizmoStartParam_ = *param;
            }
        }
        lastOrbitPoint_ = POINT{x, y};
        SetCapture(hwnd_);
        SetWindowTextW(
            status_,
            mode == 1 ? L"Rotating on gizmo axis."
                      : (mode == 2 ? L"Scaling on gizmo axis." : L"Moving on gizmo axis."));
        return true;
    }

    void DragGizmo(const int x, const int y) {
        if (!gizmoDragging_ || gizmoAxis_ < 0) {
            return;
        }
        const int node = SelectedGizmoNode();
        if (node == ri::scene::kInvalidHandle) {
            return;
        }
        const LRESULT mode = SendMessageW(transformMode_, CB_GETCURSEL, 0, 0);
        float next = gizmoStartValue_;
        if (mode == 1) {
            next = gizmoStartValue_ + static_cast<float>(x - gizmoStartMouseX_) * 0.4F;
        } else if (const auto ray = ViewportRay(x, y); ray.has_value()) {
            if (const auto param = AxisRayParameter(
                    *ray, previewScene_.ComputeWorldPosition(node), ParentLocalAxisInWorld(node, gizmoAxis_));
                param.has_value()) {
                next = gizmoStartValue_ + (*param - gizmoStartParam_);
            }
        }
        if (mode == 2) {
            next = std::max(next, 0.05F);
        }
        if (gizmoBone_) {
            Vec3Component(
                *BonePoseVector(previewScene_.GetNode(node).localTransform, static_cast<int>(mode)),
                gizmoAxis_) = next;
        } else {
            ri::content::PrimitiveModelTransform* transform = SelectedElementTransform();
            if (transform == nullptr) {
                return;
            }
            ri::content::DeclarativeVec3* value = &transform->translation;
            if (mode == 1) {
                value = &transform->rotationDegrees;
            } else if (mode == 2) {
                value = &transform->scale;
            }
            DeclarativeComponent(*value, gizmoAxis_) = next;
            ApplySelectedTransformToPreview();
        }
        gizmoDirty_ = true;
        ApplySculptDisplayMesh();
        SyncTransformGizmo();
        PopulateTransformFields();
        PublishPreview();
    }

    void EndGizmoDrag() {
        if (!gizmoDragging_) {
            return;
        }
        gizmoDragging_ = false;
        if (gizmoBone_) {
            const bool dirty = gizmoDirty_;
            ApplySculptDisplayMesh();
            PublishPreview();
            gizmoDirty_ = false;
            gizmoBone_ = false;
            if (dirty && PersistSelectedBoneRest(false)) {
                SetWindowTextW(status_, L"Saved dragged bone rest to the rig.");
            } else if (editableAnim_.has_value() && !boundClip_.nodeTracks.empty()) {
                SetWindowTextW(status_, L"Posed bone. Key Bone saves it on a clip.");
            } else {
                SetWindowTextW(status_, L"Posed bone.");
            }
            return;
        }
        if (!gizmoDirty_ || !editableModel_.has_value() || editableModelPath_.empty()) {
            gizmoDirty_ = false;
            return;
        }
        if (!ri::content::SavePrimitiveModelDocument(editableModelPath_, *editableModel_)) {
            SetWindowTextW(status_, L"Could not save the dragged transform.");
            return;
        }
        StampWriteTime(editableModelPath_, editableModelWriteTime_, editableModelHasWriteTime_);
        if (editableModelPath_ == previewedAssetPath_) {
            StampWriteTime(editableModelPath_, previewedWriteTime_, previewHasWriteTime_);
        }
        gizmoDirty_ = false;
        SetWindowTextW(status_, L"Saved gizmo transform.");
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

    void CreateNativeAnimation() {
        const fs::path rigPath = PreferredRigPath();
        std::string error;
        const fs::path output = ri::forge::CreateUniqueNativeAnimation(workspaceRoot_, rigPath, &error);
        if (output.empty()) {
            MessageBoxW(hwnd_, Widen(error).c_str(), L"Forge", MB_OK | MB_ICONERROR);
            return;
        }
        const fs::path keepPreview =
            !previewedAssetPath_.empty() && !ri::forge::IsAnimationPath(previewedAssetPath_)
            ? previewedAssetPath_
            : output;
        SetWindowTextW(status_, Widen("Created motion clip " + output.string()).c_str());
        BindClipOntoPreview(output);
        SetForgeBay(kBayMotion);
        RefreshCatalog(keepPreview);
    }

    void SetForgeBay(const int id) {
        if (bay_ == id) {
            return;
        }
        bay_ = id;
        SetWindowTextW(transformHeading_, bay_ == kBayMotion ? L"POSE" : L"PLACE");
        if (bay_ == kBayMotion) {
            SyncMotionPoseControls();
        } else {
            SetWindowTextW(applyTransformButton_, L"Set Place");
            ShowWindow(mirrorRestButton_, SW_HIDE);
        }
        RECT client{};
        GetClientRect(hwnd_, &client);
        LayoutControls(client.right, client.bottom);
        PopulateTransformFields();
        ApplySculptDisplayMesh();
        SyncTransformGizmo();
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
        InvalidateRect(hwnd_, nullptr, TRUE);
    }

    void SyncAuthoringEnable() {
        if (bay_ == kBayMotion) {
            const bool posed = SelectedBoneNode() != ri::scene::kInvalidHandle;
            const bool clip = editableAnim_.has_value();
            const bool bound = !boundClip_.nodeTracks.empty();
            const bool restEdit = ShouldPersistBoneRest();
            EnableWindow(transformMode_, posed ? TRUE : FALSE);
            EnableWindow(transformX_, posed ? TRUE : FALSE);
            EnableWindow(transformY_, posed ? TRUE : FALSE);
            EnableWindow(transformZ_, posed ? TRUE : FALSE);
            EnableWindow(applyTransformButton_, posed ? TRUE : FALSE);
            EnableWindow(animPlayButton_, bound ? TRUE : FALSE);
            EnableWindow(animStopButton_, bound ? TRUE : FALSE);
            EnableWindow(animLoopButton_, clip ? TRUE : FALSE);
            EnableWindow(animRootButton_, clip ? TRUE : FALSE);
            EnableWindow(animKeyButton_, clip && !listedBoneNodes_.empty() ? TRUE : FALSE);
            EnableWindow(animKeyBoneButton_, clip && posed ? TRUE : FALSE);
            EnableWindow(animDeleteKeyButton_, clip && posed ? TRUE : FALSE);
            EnableWindow(animClearTrackButton_, clip && posed ? TRUE : FALSE);
            EnableWindow(animClearKeysButton_, clip ? TRUE : FALSE);
            EnableWindow(animDedupKeysButton_, clip ? TRUE : FALSE);
            EnableWindow(animQuantizeKeysButton_, clip ? TRUE : FALSE);
            EnableWindow(
                animStripTracksButton_,
                clip && !previewBoneNodes_.empty() ? TRUE : FALSE);
            EnableWindow(animResetBoneButton_, posed && editableRig_.has_value() ? TRUE : FALSE);
            EnableWindow(
                animResetPoseButton_,
                editableRig_.has_value() && !previewBoneNodes_.empty() ? TRUE : FALSE);
            EnableWindow(animMirrorPoseButton_, posed && !restEdit ? TRUE : FALSE);
            EnableWindow(animSnapKeyButton_, clip ? TRUE : FALSE);
            EnableWindow(animRestKeyButton_, clip && editableRig_.has_value() ? TRUE : FALSE);
            EnableWindow(animMirrorKeysButton_, clip && posed ? TRUE : FALSE);
            EnableWindow(animCopyTrackButton_, clip && posed ? TRUE : FALSE);
            EnableWindow(animPasteTrackButton_, clip && posed && !copiedAnimTrack_.keys.empty() ? TRUE : FALSE);
            EnableWindow(animHalfSpeedButton_, clip ? TRUE : FALSE);
            EnableWindow(animDoubleSpeedButton_, clip ? TRUE : FALSE);
            EnableWindow(animNudgeBackButton_, clip ? TRUE : FALSE);
            EnableWindow(animNudgeForwardButton_, clip ? TRUE : FALSE);
            EnableWindow(animAlignStartButton_, clip ? TRUE : FALSE);
            EnableWindow(animFitDurationButton_, clip ? TRUE : FALSE);
            EnableWindow(animPlayheadZeroButton_, clip ? TRUE : FALSE);
            EnableWindow(animPrevKeyButton_, clip ? TRUE : FALSE);
            EnableWindow(animNextKeyButton_, clip ? TRUE : FALSE);
            EnableWindow(animTrimButton_, clip ? TRUE : FALSE);
            EnableWindow(duplicateClipButton_, clip ? TRUE : FALSE);
            EnableWindow(deleteClipButton_, clip ? TRUE : FALSE);
            EnableWindow(animScrub_, bound ? TRUE : FALSE);
            EnableWindow(animTime_, clip ? TRUE : FALSE);
            EnableWindow(animDuration_, clip ? TRUE : FALSE);
            EnableWindow(animInButton_, clip ? TRUE : FALSE);
            EnableWindow(animOutButton_, clip ? TRUE : FALSE);
            EnableWindow(animIn_, clip ? TRUE : FALSE);
            EnableWindow(animOut_, clip ? TRUE : FALSE);
            EnableWindow(animEventName_, clip ? TRUE : FALSE);
            EnableWindow(animAddEventButton_, clip ? TRUE : FALSE);
            EnableWindow(animDelEventButton_, clip ? TRUE : FALSE);
            EnableWindow(animRenameEventButton_, clip ? TRUE : FALSE);
            EnableWindow(animEventToTimeButton_, clip ? TRUE : FALSE);
            EnableWindow(animDupEventButton_, clip ? TRUE : FALSE);
            EnableWindow(animClearEventsButton_, clip ? TRUE : FALSE);
            EnableWindow(animPrevEventButton_, clip ? TRUE : FALSE);
            EnableWindow(animNextEventButton_, clip ? TRUE : FALSE);
            EnableWindow(animEventList_, clip ? TRUE : FALSE);
            EnableWindow(clipList_, TRUE);
            EnableWindow(bindBoneButton_, FALSE);
            EnableWindow(floodBoneButton_, FALSE);
            EnableWindow(floodUnboundButton_, FALSE);
            EnableWindow(transferWeightsButton_, FALSE);
            EnableWindow(swapWeightsButton_, FALSE);
            EnableWindow(
                mirrorRestButton_,
                ShouldPersistBoneRest() && posed ? TRUE : FALSE);
            EnableWindow(
                boneRenameEdit_,
                !previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_ && posed
                    ? TRUE
                    : FALSE);
            EnableWindow(
                boneRenameButton_,
                !previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_ && posed
                    ? TRUE
                    : FALSE);
            EnableWindow(
                addChildBoneButton_,
                !previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_ && posed
                    ? TRUE
                    : FALSE);
            EnableWindow(
                deleteBoneButton_,
                !previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_ && posed
                    ? TRUE
                    : FALSE);
            EnableWindow(
                boneParentCombo_,
                !previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_ && posed
                    ? TRUE
                    : FALSE);
            EnableWindow(
                reparentBoneButton_,
                !previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_ && posed
                    ? TRUE
                    : FALSE);
            EnableWindow(
                addSlotBoneButton_,
                !previewedAssetPath_.empty() && previewedAssetPath_ == editableRigPath_
                        && editableRig_.has_value()
                        && editableRig_->profile == ri::scene::RigProfile::Humanoid
                        && !ri::scene::MissingHumanoidBoneKeys(*editableRig_).empty()
                    ? TRUE
                    : FALSE);
            return;
        }
        const BOOL enabled = editableModel_.has_value() ? TRUE : FALSE;
        EnableWindow(transformMode_, enabled);
        EnableWindow(transformX_, enabled);
        EnableWindow(transformY_, enabled);
        EnableWindow(transformZ_, enabled);
        EnableWindow(applyTransformButton_, enabled);
        EnableWindow(duplicatePartButton_, enabled);
        EnableWindow(deletePartButton_, enabled);
        EnableWindow(
            bindBoneButton_,
            enabled && !listedBoneNodes_.empty() && SelectedBoneNode() != ri::scene::kInvalidHandle
                ? TRUE
                : FALSE);
        EnableWindow(
            floodBoneButton_,
            editableSculpt_.has_value() && !SelectedBoneName().empty() ? TRUE : FALSE);
        EnableWindow(
            floodUnboundButton_,
            editableSculpt_.has_value() && !SelectedBoneName().empty() ? TRUE : FALSE);
        EnableWindow(
            transferWeightsButton_,
            editableSculpt_.has_value() && !SelectedBoneName().empty() ? TRUE : FALSE);
        EnableWindow(
            swapWeightsButton_,
            editableSculpt_.has_value() && !SelectedBoneName().empty() ? TRUE : FALSE);
        EnableWindow(auditWeightsButton_, editableSculpt_.has_value() ? TRUE : FALSE);
        EnableWindow(softWeightsButton_, editableSculpt_.has_value() ? TRUE : FALSE);
        EnableWindow(
            invertWeightsButton_,
            editableSculpt_.has_value() && !SelectedBoneName().empty() ? TRUE : FALSE);
        EnableWindow(
            halveWeightsButton_,
            editableSculpt_.has_value() && !SelectedBoneName().empty() ? TRUE : FALSE);
        EnableWindow(
            doubleWeightsButton_,
            editableSculpt_.has_value() && !SelectedBoneName().empty() ? TRUE : FALSE);
        EnableWindow(clearWeightsButton_, editableSculpt_.has_value() ? TRUE : FALSE);
    }

    [[nodiscard]] int SelectedBoneNode() const {
        const LRESULT selection = SendMessageW(boneList_, LB_GETCURSEL, 0, 0);
        if (selection == LB_ERR || selection < 0
            || static_cast<std::size_t>(selection) >= listedBoneNodes_.size()) {
            return ri::scene::kInvalidHandle;
        }
        return listedBoneNodes_[static_cast<std::size_t>(selection)];
    }

    void PopulateBonePoseFields() {
        const int node = SelectedBoneNode();
        if (node == ri::scene::kInvalidHandle
            || node < 0
            || static_cast<std::size_t>(node) >= previewScene_.NodeCount()) {
            SetWindowTextW(transformX_, L"");
            SetWindowTextW(transformY_, L"");
            SetWindowTextW(transformZ_, L"");
            return;
        }
        const ri::scene::Transform& transform = previewScene_.GetNode(node).localTransform;
        const LRESULT mode = SendMessageW(transformMode_, CB_GETCURSEL, 0, 0);
        const ri::math::Vec3* value = &transform.position;
        if (mode == 1) {
            value = &transform.rotationDegrees;
        } else if (mode == 2) {
            value = &transform.scale;
        }
        SetWindowTextW(transformX_, FormatTransformValue(value->x).c_str());
        SetWindowTextW(transformY_, FormatTransformValue(value->y).c_str());
        SetWindowTextW(transformZ_, FormatTransformValue(value->z).c_str());
        if (boneRenameEdit_ != nullptr
            && !previewedAssetPath_.empty()
            && previewedAssetPath_ == editableRigPath_) {
            SetWindowTextW(boneRenameEdit_, Widen(previewScene_.GetNode(node).name).c_str());
        }
        PopulateBoneParentCombo();
    }

    void PopulateBoneParentCombo() {
        if (boneParentCombo_ == nullptr || !editableRig_.has_value()
            || previewedAssetPath_ != editableRigPath_) {
            return;
        }
        const std::string boneName = SelectedBoneName();
        SendMessageW(boneParentCombo_, CB_RESETCONTENT, 0, 0);
        boneParentNames_.clear();
        if (boneName.empty()) {
            return;
        }
        const std::optional<std::size_t> boneIndex = ri::scene::FindRigBoneIndex(*editableRig_, boneName);
        if (!boneIndex.has_value()) {
            return;
        }
        std::string currentParent{};
        const int currentParentIndex = editableRig_->bones[*boneIndex].parentIndex;
        if (currentParentIndex >= 0
            && static_cast<std::size_t>(currentParentIndex) < editableRig_->bones.size()) {
            currentParent = editableRig_->bones[static_cast<std::size_t>(currentParentIndex)].name;
        }
        int selected = -1;
        for (std::size_t index = 0; index < editableRig_->bones.size(); ++index) {
            if (index == *boneIndex) {
                continue;
            }
            bool descendant = false;
            std::size_t walk = index;
            while (true) {
                if (walk == *boneIndex) {
                    descendant = true;
                    break;
                }
                const int parent = editableRig_->bones[walk].parentIndex;
                if (parent < 0 || static_cast<std::size_t>(parent) >= editableRig_->bones.size()) {
                    break;
                }
                walk = static_cast<std::size_t>(parent);
            }
            if (descendant) {
                continue;
            }
            const std::string& candidate = editableRig_->bones[index].name;
            boneParentNames_.push_back(candidate);
            const int comboIndex = static_cast<int>(boneParentNames_.size()) - 1;
            SendMessageW(
                boneParentCombo_,
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(Widen(candidate).c_str()));
            if (candidate == currentParent) {
                selected = comboIndex;
            }
        }
        if (selected >= 0) {
            SendMessageW(boneParentCombo_, CB_SETCURSEL, static_cast<WPARAM>(selected), 0);
        } else if (!boneParentNames_.empty()) {
            SendMessageW(boneParentCombo_, CB_SETCURSEL, 0, 0);
        }
    }

    [[nodiscard]] static std::wstring FormatBoneListLabel(const std::string& boneName, const int depth) {
        std::wstring label;
        label.reserve(static_cast<std::size_t>(depth) * 2U + boneName.size() + 4U);
        for (int indent = 0; indent < depth; ++indent) {
            label += L"  ";
        }
        const std::string humanoidKey = ri::scene::CanonicalHumanoidBoneKey(boneName);
        if (ri::scene::IsHumanoidRequiredBoneKey(humanoidKey)) {
            label += L"* ";
        }
        label += Widen(boneName);
        return label;
    }

    [[nodiscard]] static std::wstring FormatMissingHumanoidSlotLabel(const std::string& slotKey) {
        return L"? " + Widen(slotKey) + L" (missing)";
    }

    void SyncBoneListHeading() {
        if (!editableRig_.has_value() || previewedAssetPath_ != editableRigPath_) {
            SetWindowTextW(motionHeading_, L"BONES");
            return;
        }
        if (editableRig_->profile != ri::scene::RigProfile::Humanoid) {
            SetWindowTextW(motionHeading_, L"BONES");
            return;
        }
        const ri::scene::RigValidationReport report = ri::scene::ValidateRigDefinition(*editableRig_);
        const std::vector<std::string> missing = ri::scene::MissingHumanoidBoneKeys(*editableRig_);
        std::string heading = "BONES  |  humanoid " + std::to_string(report.humanoidMatchedBoneCount) + "/"
            + std::to_string(report.humanoidRequiredBoneCount) + "  (* mapped";
        if (!missing.empty()) {
            heading += ", ? missing";
        }
        heading += ")";
        SetWindowTextW(motionHeading_, Widen(heading).c_str());
    }

    [[nodiscard]] std::optional<std::string> SelectedMissingHumanoidSlot() const {
        const LRESULT selection = SendMessageW(boneList_, LB_GETCURSEL, 0, 0);
        if (selection == LB_ERR || selection < 0
            || static_cast<std::size_t>(selection) >= listedBoneNodes_.size()) {
            return std::nullopt;
        }
        if (listedBoneNodes_[static_cast<std::size_t>(selection)] != ri::scene::kInvalidHandle) {
            return std::nullopt;
        }
        if (static_cast<std::size_t>(selection) >= listedBoneMissingSlots_.size()) {
            return std::nullopt;
        }
        return listedBoneMissingSlots_[static_cast<std::size_t>(selection)];
    }

    void RefreshBoneList(const std::string& selectName = {}) {
        const std::string keepName = selectName.empty() ? SelectedBoneName() : selectName;
        const std::optional<std::string> keepMissingSlot = SelectedMissingHumanoidSlot();
        listedBoneNodes_.clear();
        listedBoneMissingSlots_.clear();
        SendMessageW(boneList_, LB_RESETCONTENT, 0, 0);
        int selected = -1;
        const auto appendBone = [&](const int node, const int depth) {
            if (node == ri::scene::kInvalidHandle
                || node < 0
                || static_cast<std::size_t>(node) >= previewScene_.NodeCount()) {
                return;
            }
            const std::string& name = previewScene_.GetNode(node).name;
            if (name == keepName) {
                selected = static_cast<int>(listedBoneNodes_.size());
            }
            listedBoneNodes_.push_back(node);
            listedBoneMissingSlots_.push_back(std::nullopt);
            const std::wstring label = FormatBoneListLabel(name, depth);
            SendMessageW(boneList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        };
        if (editableRig_.has_value() && previewedAssetPath_ == editableRigPath_) {
            for (const ri::scene::RigBoneTreeEntry& entry :
                ri::scene::BuildRigBoneDepthFirstOrder(*editableRig_)) {
                if (entry.boneIndex >= previewBoneNodes_.size()) {
                    continue;
                }
                appendBone(previewBoneNodes_[entry.boneIndex], entry.depth);
            }
            if (editableRig_->profile == ri::scene::RigProfile::Humanoid) {
                for (const std::string& missingSlot :
                    ri::scene::MissingHumanoidBoneKeys(*editableRig_)) {
                    if (missingSlot == keepMissingSlot) {
                        selected = static_cast<int>(listedBoneNodes_.size());
                    }
                    listedBoneNodes_.push_back(ri::scene::kInvalidHandle);
                    listedBoneMissingSlots_.push_back(missingSlot);
                    SendMessageW(
                        boneList_,
                        LB_ADDSTRING,
                        0,
                        reinterpret_cast<LPARAM>(FormatMissingHumanoidSlotLabel(missingSlot).c_str()));
                }
            }
        } else {
            for (const int boneNode : previewBoneNodes_) {
                appendBone(boneNode, 0);
            }
        }
        if (!listedBoneNodes_.empty()) {
            SendMessageW(
                boneList_,
                LB_SETCURSEL,
                static_cast<WPARAM>(selected >= 0 ? selected : 0),
                0);
        }
        SyncBoneListHeading();
    }

    void ResyncBoneList(const std::string& selectName = {}) {
        RefreshBoneList(selectName);
    }

    [[nodiscard]] bool SidecarUsesEditableRig(
        const std::string& rigPath,
        const fs::path& documentPath) const {
        if (rigPath.empty() || editableRigPath_.empty()) {
            return false;
        }
        const fs::path resolved = ri::forge::ResolveCatalogRigPath(catalog_, rigPath, documentPath);
        std::error_code error{};
        return !resolved.empty()
            && fs::equivalent(resolved, editableRigPath_, error);
    }

    void RenameSelectedBone() {
        if (editableRigPath_.empty() || previewedAssetPath_ != editableRigPath_) {
            MessageBoxW(
                hwnd_,
                L"Open a rig document before renaming bones.",
                L"Forge rig",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string oldName = SelectedBoneName();
        const std::string newName = ReadControlTextUtf8(boneRenameEdit_);
        if (oldName.empty()) {
            MessageBoxW(
                hwnd_, L"Select a bone to rename.", L"Forge rig", MB_OK | MB_ICONWARNING);
            return;
        }
        if (newName.empty()) {
            MessageBoxW(
                hwnd_, L"Enter a new bone name.", L"Forge rig", MB_OK | MB_ICONWARNING);
            return;
        }
        if (newName == oldName) {
            SetWindowTextW(status_, L"Bone name unchanged.");
            return;
        }
        const ri::scene::RigBoneRenameResult renamed =
            ri::scene::RenameRigBone(*editableRig_, oldName, newName);
        if (!renamed.valid) {
            MessageBoxW(hwnd_, Widen(renamed.summary).c_str(), L"Forge rig", MB_OK | MB_ICONWARNING);
            return;
        }
        const int node = SelectedBoneNode();
        if (node != ri::scene::kInvalidHandle
            && node >= 0
            && static_cast<std::size_t>(node) < previewScene_.NodeCount()) {
            previewScene_.GetNode(node).name = newName;
        }
        if (const auto rest = restBoneLocal_.extract(oldName)) {
            restBoneLocal_.insert({newName, rest.mapped()});
        }
        std::string sidecars{};
        if (editableSculpt_.has_value()
            && SidecarUsesEditableRig(editableSculpt_->rigPath, editableSculptPath_)) {
            if (ri::scene::RenameNativeSculptBone(*editableSculpt_, oldName, newName) > 0U) {
                if (ri::content::SaveNativeSculptDocument(editableSculptPath_, *editableSculpt_)) {
                    sidecars += " clay";
                }
            }
        }
        if (editableModel_.has_value()
            && SidecarUsesEditableRig(editableModel_->rigPath, editableModelPath_)) {
            if (ri::content::RenamePrimitiveModelBoneReferences(*editableModel_, oldName, newName) > 0U) {
                if (ri::content::SavePrimitiveModelDocument(editableModelPath_, *editableModel_)) {
                    sidecars += " stock";
                }
            }
        }
        if (editableAnim_.has_value()
            && SidecarUsesEditableRig(editableAnim_->rigPath, editableAnimPath_)) {
            if (ri::scene::RenameNativeAnimationBone(*editableAnim_, oldName, newName)) {
                if (ri::content::SaveNativeAnimationDocument(editableAnimPath_, *editableAnim_)) {
                    sidecars += " clip";
                }
            }
        }
        if (!SaveEditableRig()) {
            return;
        }
        ResyncBoneList(newName);
        PopulateBonePoseFields();
        SyncTransformGizmo();
        if (editableAnim_.has_value() && !previewBoneNodes_.empty()) {
            boundClip_ = ri::scene::BindNativeAnimationClip(
                *editableAnim_, previewScene_, previewBoneNodes_);
            animPlayer_.SetClip(&boundClip_);
        }
        ApplySculptDisplayMesh();
        PublishPreview();
        RefreshCatalog(editableRigPath_);
        SetWindowTextW(
            status_,
            Widen(renamed.summary + (sidecars.empty() ? "" : " Updated" + sidecars + ".")).c_str());
    }

    void AddChildToSelectedBone() {
        if (editableRigPath_.empty() || previewedAssetPath_ != editableRigPath_) {
            MessageBoxW(
                hwnd_,
                L"Open a rig document before adding bones.",
                L"Forge rig",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string parentName = SelectedBoneName();
        if (parentName.empty()) {
            MessageBoxW(
                hwnd_, L"Select a parent bone first.", L"Forge rig", MB_OK | MB_ICONWARNING);
            return;
        }
        std::string desiredName = ReadControlTextUtf8(boneRenameEdit_);
        if (desiredName == parentName) {
            desiredName.clear();
        }
        const ri::scene::RigAddBoneResult added = ri::scene::AddRigChildBone(
            *editableRig_, parentName, desiredName);
        if (!added.valid) {
            MessageBoxW(hwnd_, Widen(added.summary).c_str(), L"Forge rig", MB_OK | MB_ICONWARNING);
            return;
        }
        if (!SaveEditableRig(true, false)) {
            return;
        }
        pendingBoneSelect_ = added.boneName;
        Rebuild3DPreview(SelectedAsset(), true);
        RefreshCatalog(editableRigPath_);
        SetWindowTextW(status_, Widen(added.summary).c_str());
    }

    void DeleteSelectedBone() {
        if (editableRigPath_.empty() || previewedAssetPath_ != editableRigPath_) {
            MessageBoxW(
                hwnd_,
                L"Open a rig document before deleting bones.",
                L"Forge rig",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string boneName = SelectedBoneName();
        if (boneName.empty()) {
            MessageBoxW(
                hwnd_, L"Select a bone to delete.", L"Forge rig", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::optional<std::size_t> boneIndex = ri::scene::FindRigBoneIndex(*editableRig_, boneName);
        std::string parentName{};
        if (boneIndex.has_value() && editableRig_->bones[*boneIndex].parentIndex >= 0) {
            const int parentIndex = editableRig_->bones[*boneIndex].parentIndex;
            if (static_cast<std::size_t>(parentIndex) < editableRig_->bones.size()) {
                parentName = editableRig_->bones[static_cast<std::size_t>(parentIndex)].name;
            }
        }
        const ri::scene::RigDeleteBoneResult deleted =
            ri::scene::DeleteRigBone(*editableRig_, boneName);
        if (!deleted.valid) {
            MessageBoxW(hwnd_, Widen(deleted.summary).c_str(), L"Forge rig", MB_OK | MB_ICONWARNING);
            return;
        }
        std::string sidecars{};
        if (editableSculpt_.has_value()
            && SidecarUsesEditableRig(editableSculpt_->rigPath, editableSculptPath_)) {
            if (ri::scene::RemoveNativeSculptBone(*editableSculpt_, boneName) > 0U) {
                if (ri::content::SaveNativeSculptDocument(editableSculptPath_, *editableSculpt_)) {
                    sidecars += " clay";
                }
            }
        }
        if (editableModel_.has_value()
            && SidecarUsesEditableRig(editableModel_->rigPath, editableModelPath_)) {
            if (ri::content::RemovePrimitiveModelBoneReferences(*editableModel_, boneName) > 0U) {
                if (ri::content::SavePrimitiveModelDocument(editableModelPath_, *editableModel_)) {
                    sidecars += " stock";
                }
            }
        }
        if (editableAnim_.has_value()
            && SidecarUsesEditableRig(editableAnim_->rigPath, editableAnimPath_)) {
            if (ri::scene::RemoveNativeAnimationBone(*editableAnim_, boneName)) {
                if (ri::content::SaveNativeAnimationDocument(editableAnimPath_, *editableAnim_)) {
                    sidecars += " clip";
                }
            }
        }
        if (!SaveEditableRig(true, false)) {
            return;
        }
        pendingBoneSelect_ = parentName;
        Rebuild3DPreview(SelectedAsset(), true);
        RefreshCatalog(editableRigPath_);
        SetWindowTextW(
            status_,
            Widen(deleted.summary + (sidecars.empty() ? "" : " Updated" + sidecars + ".")).c_str());
    }

    void AddSelectedHumanoidSlot() {
        if (editableRigPath_.empty() || previewedAssetPath_ != editableRigPath_) {
            MessageBoxW(
                hwnd_,
                L"Open a humanoid rig document before adding slots.",
                L"Forge rig",
                MB_OK | MB_ICONWARNING);
            return;
        }
        std::string slotKey = SelectedMissingHumanoidSlot().value_or("");
        if (slotKey.empty()) {
            const std::vector<std::string> missing =
                ri::scene::MissingHumanoidBoneKeys(*editableRig_);
            if (missing.empty()) {
                MessageBoxW(
                    hwnd_, L"No humanoid slots are missing.", L"Forge rig", MB_OK | MB_ICONWARNING);
                return;
            }
            slotKey = missing.front();
        }
        const ri::scene::RigAddBoneResult added =
            ri::scene::AddHumanoidSlotBone(*editableRig_, slotKey);
        if (!added.valid) {
            MessageBoxW(hwnd_, Widen(added.summary).c_str(), L"Forge rig", MB_OK | MB_ICONWARNING);
            return;
        }
        if (!SaveEditableRig(true, false)) {
            return;
        }
        pendingBoneSelect_ = added.boneName;
        Rebuild3DPreview(SelectedAsset(), true);
        RefreshCatalog(editableRigPath_);
        SetWindowTextW(status_, Widen(added.summary).c_str());
    }

    void AuditSelectedSculptWeights() {
        if (!editableSculpt_.has_value()) {
            MessageBoxW(
                hwnd_, L"Open clay before auditing weights.", L"Forge clay", MB_OK | MB_ICONWARNING);
            return;
        }
        const ri::scene::NativeSculptWeightAudit audit =
            ri::scene::AuditNativeSculptWeights(*editableSculpt_);
        std::string summary = std::to_string(audit.boundCount) + " bound / "
            + std::to_string(audit.vertexCount) + " verts";
        if (audit.unboundCount > 0U) {
            summary += " | " + std::to_string(audit.unboundCount) + " unbound";
        }
        if (audit.blendedCount > 0U) {
            summary += " | " + std::to_string(audit.blendedCount) + " blended";
        }
        if (audit.nonNormalizedCount > 0U) {
            summary += " | " + std::to_string(audit.nonNormalizedCount) + " non-normalized";
        }
        SetWindowTextW(status_, Widen("Weight audit: " + summary).c_str());
    }

    void SoftenSelectedSculptWeights() {
        if (!editableSculpt_.has_value()) {
            MessageBoxW(
                hwnd_, L"Open clay before softening weights.", L"Forge weights", MB_OK | MB_ICONWARNING);
            return;
        }
        const ri::content::NativeSculptDocument previous = *editableSculpt_;
        const std::size_t changed = ri::scene::SmoothNativeSculptWeights(*editableSculpt_);
        if (changed == 0U) {
            SetWindowTextW(status_, L"Weights were already smooth.");
            return;
        }
        sculptUndo_.Capture(previous);
        sculptDirty_ = true;
        showWeights_ = true;
        ApplySculptDisplayMesh();
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
        SyncForgeTitle();
        if (!SaveActiveSculpt()) {
            return;
        }
        SetWindowTextW(
            status_, Widen("Softened weights on " + std::to_string(changed) + " verts.").c_str());
    }

    void InvertSelectedBoneWeights() {
        if (!editableSculpt_.has_value()) {
            MessageBoxW(
                hwnd_, L"Open clay before inverting weights.", L"Forge weights", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string boneName = SelectedBoneName();
        if (boneName.empty()) {
            MessageBoxW(
                hwnd_,
                L"Select a bone in BONES, then Inv.",
                L"Forge weights",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const ri::content::NativeSculptDocument previous = *editableSculpt_;
        const std::size_t changed =
            ri::scene::InvertNativeSculptBoneWeights(*editableSculpt_, boneName);
        if (changed == 0U) {
            SetWindowTextW(status_, Widen("No weights on " + boneName + " to invert.").c_str());
            return;
        }
        sculptUndo_.Capture(previous);
        sculptDirty_ = true;
        showWeights_ = true;
        ApplySculptDisplayMesh();
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
        SyncForgeTitle();
        if (!SaveActiveSculpt()) {
            return;
        }
        SetWindowTextW(
            status_,
            Widen("Inverted " + boneName + " on " + std::to_string(changed) + " verts.").c_str());
    }

    void ScaleSelectedBoneWeights(const float factor) {
        if (!editableSculpt_.has_value()) {
            MessageBoxW(
                hwnd_, L"Open clay before scaling weights.", L"Forge weights", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string boneName = SelectedBoneName();
        if (boneName.empty()) {
            MessageBoxW(
                hwnd_,
                L"Select a bone in BONES, then ½W or ×2W.",
                L"Forge weights",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const ri::content::NativeSculptDocument previous = *editableSculpt_;
        const std::size_t changed =
            ri::scene::ScaleNativeSculptBoneWeights(*editableSculpt_, boneName, factor);
        if (changed == 0U) {
            SetWindowTextW(status_, Widen("No weights on " + boneName + " to scale.").c_str());
            return;
        }
        sculptUndo_.Capture(previous);
        sculptDirty_ = true;
        showWeights_ = true;
        ApplySculptDisplayMesh();
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
        SyncForgeTitle();
        if (!SaveActiveSculpt()) {
            return;
        }
        SetWindowTextW(
            status_,
            Widen(std::string(factor < 1.0f ? "Halved" : "Doubled") + " " + boneName + " on "
                + std::to_string(changed) + " verts.").c_str());
    }

    void ClearSelectedSculptWeights() {
        if (!editableSculpt_.has_value()) {
            MessageBoxW(
                hwnd_, L"Open clay before clearing weights.", L"Forge weights", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string boneName = SelectedBoneName();
        const ri::content::NativeSculptDocument previous = *editableSculpt_;
        const std::size_t changed = boneName.empty()
            ? ri::scene::ClearNativeSculptWeights(*editableSculpt_)
            : ri::scene::RemoveNativeSculptBone(*editableSculpt_, boneName);
        if (changed == 0U) {
            if (boneName.empty()) {
                SetWindowTextW(status_, L"No vertex weights to clear.");
            } else {
                SetWindowTextW(status_, Widen("No weights used " + boneName + ".").c_str());
            }
            return;
        }
        sculptUndo_.Capture(previous);
        sculptDirty_ = true;
        showWeights_ = true;
        ApplySculptDisplayMesh();
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
        SyncForgeTitle();
        if (!SaveActiveSculpt()) {
            return;
        }
        if (boneName.empty()) {
            SetWindowTextW(
                status_,
                Widen("Cleared weights on " + std::to_string(changed) + " verts.").c_str());
        } else {
            SetWindowTextW(
                status_,
                Widen("Cleared " + boneName + " from " + std::to_string(changed) + " verts.")
                    .c_str());
        }
    }

    void UnbindSelectedTarget() {
        fs::path targetPath = lastBindTargetPath_;
        ri::forge::AssetKind targetKind = lastBindTargetKind_;
        if (const ri::forge::AssetEntry* selected = SelectedAsset();
            selected != nullptr
            && (selected->kind == ri::forge::AssetKind::Sculpt
                || selected->kind == ri::forge::AssetKind::PrimitiveModel)
            && selected->valid) {
            targetPath = selected->absolutePath;
            targetKind = selected->kind;
        }
        if (targetPath.empty() && editableSculpt_.has_value()) {
            targetPath = editableSculptPath_;
            targetKind = ri::forge::AssetKind::Sculpt;
        }
        if (targetPath.empty() && editableModel_.has_value()) {
            targetPath = editableModelPath_;
            targetKind = ri::forge::AssetKind::PrimitiveModel;
        }
        if (targetPath.empty()) {
            MessageBoxW(
                hwnd_,
                L"Open clay or stock before unbinding a rig.",
                L"Forge unbind",
                MB_OK | MB_ICONWARNING);
            return;
        }

        if (targetKind == ri::forge::AssetKind::Sculpt
            && editableSculpt_.has_value()
            && editableSculptPath_ == targetPath) {
            if (editableSculpt_->rigPath.empty()
                && ri::scene::AuditNativeSculptWeights(*editableSculpt_).boundCount == 0U) {
                SetWindowTextW(status_, L"Clay has no rig binding to clear.");
                return;
            }
            const ri::content::NativeSculptDocument previous = *editableSculpt_;
            const std::size_t changed = ri::scene::UnbindNativeSculptFromRig(*editableSculpt_);
            sculptUndo_.Capture(previous);
            sculptDirty_ = true;
            ApplySculptDisplayMesh();
            SyncPreviewOverlays(true);
            RefreshViewportHeading();
            SyncForgeTitle();
            if (!SaveActiveSculpt()) {
                return;
            }
            SyncEditableRig();
            SyncMotionPoseControls();
            RefreshCatalog(targetPath);
            SetWindowTextW(
                status_,
                Widen("Unbound clay (" + std::to_string(changed) + " cleared).").c_str());
            return;
        }

        if (targetKind == ri::forge::AssetKind::PrimitiveModel
            && editableModel_.has_value()
            && editableModelPath_ == targetPath) {
            const std::size_t changed = ri::content::UnbindPrimitiveModelFromRig(*editableModel_);
            if (changed == 0U) {
                SetWindowTextW(status_, L"Stock has no rig binding to clear.");
                return;
            }
            if (!ri::content::SavePrimitiveModelDocument(editableModelPath_, *editableModel_)) {
                MessageBoxW(hwnd_, L"Could not save unbound stock.", L"Forge unbind", MB_OK | MB_ICONERROR);
                return;
            }
            SyncEditableRig();
            SyncMotionPoseControls();
            RefreshCatalog(targetPath);
            Rebuild3DPreview(SelectedAsset(), true);
            SetWindowTextW(status_, L"Unbound stock from its rig.");
            return;
        }

        if (targetKind == ri::forge::AssetKind::Sculpt) {
            auto sculpt = ri::content::LoadNativeSculptDocument(targetPath);
            if (!sculpt.has_value()) {
                MessageBoxW(hwnd_, L"Could not load clay to unbind.", L"Forge unbind", MB_OK | MB_ICONERROR);
                return;
            }
            if (ri::scene::UnbindNativeSculptFromRig(*sculpt) == 0U) {
                SetWindowTextW(status_, L"Clay has no rig binding to clear.");
                return;
            }
            if (!ri::content::SaveNativeSculptDocument(targetPath, *sculpt)) {
                MessageBoxW(hwnd_, L"Could not save unbound clay.", L"Forge unbind", MB_OK | MB_ICONERROR);
                return;
            }
        } else {
            auto model = ri::content::LoadPrimitiveModelDocument(targetPath);
            if (!model.has_value()) {
                MessageBoxW(hwnd_, L"Could not load stock to unbind.", L"Forge unbind", MB_OK | MB_ICONERROR);
                return;
            }
            if (ri::content::UnbindPrimitiveModelFromRig(*model) == 0U) {
                SetWindowTextW(status_, L"Stock has no rig binding to clear.");
                return;
            }
            if (!ri::content::SavePrimitiveModelDocument(targetPath, *model)) {
                MessageBoxW(hwnd_, L"Could not save unbound stock.", L"Forge unbind", MB_OK | MB_ICONERROR);
                return;
            }
        }
        RefreshCatalog(targetPath);
        Rebuild3DPreview(SelectedAsset(), true);
        SetWindowTextW(status_, L"Unbound selected asset from its rig.");
    }

    void JumpToNearestAnimKey(const bool previous) {
        if (!editableAnim_.has_value()) {
            MessageBoxW(hwnd_, L"Open a motion clip before jumping keys.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string boneName = SelectedBoneName();
        const std::optional<double> next = ri::scene::FindNearestNativeAnimationKeyTime(
            *editableAnim_, ReadCurrentAnimTime(), previous, boneName);
        if (!next.has_value()) {
            SetWindowTextW(
                status_,
                previous ? L"No earlier key." : L"No later key.");
            return;
        }
        animPlayer_.Stop();
        if (!boundClip_.nodeTracks.empty()) {
            animPlayer_.SetClip(&boundClip_);
        }
        animPlayer_.SetTimeSeconds(*next);
        if (!boundClip_.nodeTracks.empty()) {
            ApplyClipPoseToPreview();
            ApplySculptDisplayMesh();
            PublishPreview();
        }
        SetWindowTextW(animTime_, FormatTransformValue(static_cast<float>(*next)).c_str());
        SyncAnimScrubFromPlayer();
        PopulateBonePoseFields();
        std::wstring status = previous ? L"Previous key @ " : L"Next key @ ";
        status += FormatTransformValue(static_cast<float>(*next));
        status += L"s";
        if (!boneName.empty()) {
            status += L" (";
            status += Widen(boneName);
            status += L")";
        }
        SetWindowTextW(status_, status.c_str());
    }

    void SeekAnimTime(const double timeSeconds) {
        animPlayer_.Stop();
        if (!boundClip_.nodeTracks.empty()) {
            animPlayer_.SetClip(&boundClip_);
        }
        animPlayer_.SetTimeSeconds(timeSeconds);
        if (!boundClip_.nodeTracks.empty()) {
            ApplyClipPoseToPreview();
            ApplySculptDisplayMesh();
            PublishPreview();
        }
        SetWindowTextW(animTime_, FormatTransformValue(static_cast<float>(timeSeconds)).c_str());
        SyncAnimScrubFromPlayer();
        PopulateBonePoseFields();
    }

    void SnapToClosestAnimKey() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(hwnd_, L"Open a motion clip before snapping.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string boneName = SelectedBoneName();
        const std::optional<double> closest = ri::scene::FindClosestNativeAnimationKeyTime(
            *editableAnim_, ReadCurrentAnimTime(), boneName);
        if (!closest.has_value()) {
            SetWindowTextW(status_, L"No keys to snap to.");
            return;
        }
        SeekAnimTime(*closest);
        std::wstring status = L"Snapped to ";
        status += FormatTransformValue(static_cast<float>(*closest));
        status += L"s";
        if (!boneName.empty()) {
            status += L" (";
            status += Widen(boneName);
            status += L")";
        }
        SetWindowTextW(status_, status.c_str());
    }

    [[nodiscard]] bool ApplyRigRestToBoneNode(const int node) {
        if (!editableRig_.has_value()
            || node == ri::scene::kInvalidHandle
            || node < 0
            || static_cast<std::size_t>(node) >= previewScene_.NodeCount()) {
            return false;
        }
        const std::string& name = previewScene_.GetNode(node).name;
        const std::optional<std::size_t> index = ri::scene::FindRigBoneIndex(*editableRig_, name);
        if (!index.has_value()) {
            return false;
        }
        previewScene_.GetNode(node).localTransform = editableRig_->bones[*index].restLocal;
        return true;
    }

    void ResetSelectedBonePose() {
        const int node = SelectedBoneNode();
        if (!ApplyRigRestToBoneNode(node)) {
            MessageBoxW(
                hwnd_,
                L"Select a bone that exists on the bound rig to reset.",
                L"Forge pose",
                MB_OK | MB_ICONWARNING);
            return;
        }
        CaptureRestBoneWorld();
        ApplySculptDisplayMesh();
        PublishPreview();
        PopulateBonePoseFields();
        SyncTransformGizmo();
        SetWindowTextW(
            status_,
            Widen("Reset " + previewScene_.GetNode(node).name + " to rest.").c_str());
    }

    void ResetAllBonePoses() {
        if (!editableRig_.has_value() || previewBoneNodes_.empty()) {
            MessageBoxW(
                hwnd_, L"Bind a rig before resetting the pose.", L"Forge pose", MB_OK | MB_ICONWARNING);
            return;
        }
        animPlayer_.Stop();
        ApplyEditableRigRestToPreview();
        ApplySculptDisplayMesh();
        PublishPreview();
        PopulateBonePoseFields();
        SyncTransformGizmo();
        SetWindowTextW(status_, L"Reset all bones to rest pose.");
    }

    void MirrorSelectedBonePose() {
        if (ShouldPersistBoneRest()) {
            MirrorSelectedBoneRest();
            return;
        }
        const int node = SelectedBoneNode();
        if (node == ri::scene::kInvalidHandle
            || node < 0
            || static_cast<std::size_t>(node) >= previewScene_.NodeCount()) {
            MessageBoxW(
                hwnd_, L"Select a bone to mirror across X.", L"Forge pose", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string sourceName = previewScene_.GetNode(node).name;
        const std::optional<std::string> partnerName = ri::scene::MirrorPartnerBoneName(sourceName);
        if (!partnerName.has_value()) {
            MessageBoxW(
                hwnd_,
                Widen(sourceName + " has no left/right partner.").c_str(),
                L"Forge pose",
                MB_OK | MB_ICONWARNING);
            return;
        }
        int partnerNode = ri::scene::kInvalidHandle;
        for (const int candidate : listedBoneNodes_) {
            if (candidate != ri::scene::kInvalidHandle
                && candidate >= 0
                && static_cast<std::size_t>(candidate) < previewScene_.NodeCount()
                && previewScene_.GetNode(candidate).name == *partnerName) {
                partnerNode = candidate;
                break;
            }
        }
        if (partnerNode == ri::scene::kInvalidHandle) {
            MessageBoxW(
                hwnd_,
                Widen("Partner bone " + *partnerName + " is not in the preview skeleton.").c_str(),
                L"Forge pose",
                MB_OK | MB_ICONWARNING);
            return;
        }
        previewScene_.GetNode(partnerNode).localTransform =
            ri::scene::MirrorTransformAcrossX(previewScene_.GetNode(node).localTransform);
        CaptureRestBoneWorld();
        ApplySculptDisplayMesh();
        PublishPreview();
        for (std::size_t index = 0; index < listedBoneNodes_.size(); ++index) {
            if (listedBoneNodes_[index] == partnerNode) {
                SendMessageW(boneList_, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
                break;
            }
        }
        PopulateBonePoseFields();
        SyncTransformGizmo();
        SetWindowTextW(
            status_,
            Widen("Mirrored " + sourceName + " pose onto " + *partnerName + ".").c_str());
    }

    void DeleteSelectedClip() {
        fs::path clipPath = editableAnimPath_;
        if (clipPath.empty()) {
            if (const ri::forge::AssetEntry* asset = SelectedAsset();
                asset != nullptr && asset->kind == ri::forge::AssetKind::Animation) {
                clipPath = asset->absolutePath;
            }
        }
        if (clipPath.empty()) {
            MessageBoxW(hwnd_, L"Select a motion clip to delete.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::wstring prompt =
            L"Delete motion clip\n" + Widen(clipPath.filename().string()) + L"?\nThis cannot be undone.";
        if (MessageBoxW(hwnd_, prompt.c_str(), L"Forge", MB_YESNO | MB_ICONWARNING) != IDYES) {
            return;
        }
        const bool clearingLive = editableAnimPath_ == clipPath;
        std::string error;
        if (!ri::forge::DeleteNativeAnimation(clipPath, &error)) {
            MessageBoxW(
                hwnd_,
                error.empty() ? L"Could not delete the clip." : Widen(error).c_str(),
                L"Forge",
                MB_OK | MB_ICONERROR);
            return;
        }
        if (clearingLive) {
            editableAnim_.reset();
            editableAnimPath_.clear();
            boundClip_ = {};
            animPlayer_.Stop();
            animPlayer_.SetClip(nullptr);
        }
        SetWindowTextW(status_, Widen("Deleted " + clipPath.filename().string()).c_str());
        RefreshCatalog({});
        PopulateClipList();
        SyncAuthoringEnable();
    }

    void ReparentSelectedBone() {
        if (editableRigPath_.empty() || previewedAssetPath_ != editableRigPath_) {
            MessageBoxW(
                hwnd_,
                L"Open a rig document before reparenting bones.",
                L"Forge rig",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string boneName = SelectedBoneName();
        if (boneName.empty()) {
            MessageBoxW(
                hwnd_, L"Select a bone to reparent.", L"Forge rig", MB_OK | MB_ICONWARNING);
            return;
        }
        const LRESULT parentSelection = SendMessageW(boneParentCombo_, CB_GETCURSEL, 0, 0);
        if (parentSelection == CB_ERR || parentSelection < 0
            || static_cast<std::size_t>(parentSelection) >= boneParentNames_.size()) {
            MessageBoxW(
                hwnd_, L"Choose a new parent bone.", L"Forge rig", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string newParent = boneParentNames_[static_cast<std::size_t>(parentSelection)];
        const ri::scene::RigReparentBoneResult reparented =
            ri::scene::ReparentRigBone(*editableRig_, boneName, newParent);
        if (!reparented.valid) {
            MessageBoxW(hwnd_, Widen(reparented.summary).c_str(), L"Forge rig", MB_OK | MB_ICONWARNING);
            return;
        }
        if (!SaveEditableRig(true, false)) {
            return;
        }
        pendingBoneSelect_ = boneName;
        Rebuild3DPreview(SelectedAsset(), true);
        RefreshCatalog(editableRigPath_);
        SetWindowTextW(status_, Widen(reparented.summary).c_str());
    }

    void ApplySelectedBonePose() {
        const int node = SelectedBoneNode();
        const auto x = ReadFiniteFloat(transformX_);
        const auto y = ReadFiniteFloat(transformY_);
        const auto z = ReadFiniteFloat(transformZ_);
        if (node == ri::scene::kInvalidHandle || !x.has_value() || !y.has_value() || !z.has_value()) {
            MessageBoxW(hwnd_, L"Select a bone and enter finite pose values.", L"Forge pose",
                        MB_OK | MB_ICONWARNING);
            return;
        }
        ri::scene::Transform& transform = previewScene_.GetNode(node).localTransform;
        const LRESULT mode = SendMessageW(transformMode_, CB_GETCURSEL, 0, 0);
        if (mode == 1) {
            transform.rotationDegrees = {*x, *y, *z};
        } else if (mode == 2) {
            transform.scale = {*x, *y, *z};
        } else {
            transform.position = {*x, *y, *z};
        }
        ApplySculptDisplayMesh();
        PublishPreview();
        const std::string boneName = previewScene_.GetNode(node).name;
        if (PersistSelectedBoneRest(false)) {
            SetWindowTextW(
                status_,
                Widen("Saved rest pose for " + boneName + " in "
                      + editableRigPath_.filename().string())
                    .c_str());
        } else if (editableAnim_.has_value() && !boundClip_.nodeTracks.empty()) {
            SetWindowTextW(status_, Widen("Posed " + boneName + ". Key Bone saves clip pose.").c_str());
        } else {
            SetWindowTextW(status_, Widen("Posed " + boneName).c_str());
        }
    }

    [[nodiscard]] double ClipDurationSeconds() const {
        if (boundClip_.durationSeconds > 0.0) {
            return boundClip_.durationSeconds;
        }
        if (editableAnim_.has_value() && editableAnim_->durationSeconds > 0.0) {
            return editableAnim_->durationSeconds;
        }
        return 1.0;
    }

    [[nodiscard]] double ReadCurrentAnimTime() const {
        if (const auto typed = ReadFiniteFloat(animTime_); typed.has_value() && *typed >= 0.0F) {
            return static_cast<double>(*typed);
        }
        return animPlayer_.TimeSeconds();
    }

    void SyncAnimScrubFromPlayer() {
        if (animScrub_ == nullptr) {
            return;
        }
        animScrubSyncing_ = true;
        const double duration = ClipDurationSeconds();
        const int pos = duration <= 0.0
            ? 0
            : static_cast<int>(std::lround(
                  std::clamp(animPlayer_.TimeSeconds() / duration, 0.0, 1.0) * 1000.0));
        SendMessageW(animScrub_, TBM_SETPOS, TRUE, pos);
        animScrubSyncing_ = false;
    }

    void ScrubClipFromTrackbar() {
        if (animScrubSyncing_ || boundClip_.nodeTracks.empty()) {
            return;
        }
        animPlayer_.Stop();
        const int pos = static_cast<int>(SendMessageW(animScrub_, TBM_GETPOS, 0, 0));
        const double time = ClipDurationSeconds() * (static_cast<double>(std::clamp(pos, 0, 1000)) / 1000.0);
        animPlayer_.SetClip(&boundClip_);
        animPlayer_.SetTimeSeconds(time);
        ApplyClipPoseToPreview();
        ApplySculptDisplayMesh();
        PublishPreview();
        SetWindowTextW(animTime_, FormatTransformValue(static_cast<float>(animPlayer_.TimeSeconds())).c_str());
        PopulateBonePoseFields();
    }

    void ApplyAnimTimeFromEdit() {
        if (boundClip_.nodeTracks.empty()) {
            return;
        }
        if (const auto typed = ReadFiniteFloat(animTime_); typed.has_value() && *typed >= 0.0F) {
            animPlayer_.Stop();
            animPlayer_.SetClip(&boundClip_);
            animPlayer_.SetTimeSeconds(static_cast<double>(*typed));
            ApplyClipPoseToPreview();
            ApplySculptDisplayMesh();
            PublishPreview();
            SyncAnimScrubFromPlayer();
            PopulateBonePoseFields();
        }
    }

    void SyncAnimDurationFromClip() {
        if (animDuration_ == nullptr) {
            return;
        }
        animDurationSyncing_ = true;
        if (!editableAnim_.has_value()) {
            SetWindowTextW(animDuration_, L"");
        } else {
            SetWindowTextW(
                animDuration_,
                FormatTransformValue(static_cast<float>(editableAnim_->durationSeconds)).c_str());
        }
        animDurationSyncing_ = false;
    }

    void SyncAnimWindowFromClip() {
        if (animIn_ == nullptr || animOut_ == nullptr) {
            return;
        }
        animWindowSyncing_ = true;
        if (!editableAnim_.has_value()) {
            SetWindowTextW(animIn_, L"");
            SetWindowTextW(animOut_, L"");
        } else {
            SetWindowTextW(animIn_, L"0");
            SetWindowTextW(
                animOut_,
                FormatTransformValue(static_cast<float>(editableAnim_->durationSeconds)).c_str());
        }
        animWindowSyncing_ = false;
    }

    void PopulateEventList(const int preferredIndex = -1) {
        if (animEventList_ == nullptr) {
            return;
        }
        animEventSyncing_ = true;
        SendMessageW(animEventList_, LB_RESETCONTENT, 0, 0);
        int selected = preferredIndex;
        if (editableAnim_.has_value()) {
            for (const ri::content::NativeAnimationEvent& event : editableAnim_->events) {
                const std::wstring label =
                    FormatTransformValue(static_cast<float>(event.timeSeconds)) + L"  " + Widen(event.name);
                SendMessageW(animEventList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            }
            if (selected < 0 && !editableAnim_->events.empty()) {
                const double time = ReadCurrentAnimTime();
                double bestDelta = 1.0e9;
                for (std::size_t index = 0; index < editableAnim_->events.size(); ++index) {
                    const double delta = std::abs(editableAnim_->events[index].timeSeconds - time);
                    if (delta < bestDelta) {
                        bestDelta = delta;
                        selected = static_cast<int>(index);
                    }
                }
                if (bestDelta > 1.0 / 120.0) {
                    selected = -1;
                }
            }
        }
        if (selected >= 0) {
            SendMessageW(animEventList_, LB_SETCURSEL, static_cast<WPARAM>(selected), 0);
        }
        animEventSyncing_ = false;
    }

    void JumpToSelectedEvent() {
        if (animEventSyncing_ || !editableAnim_.has_value() || animEventList_ == nullptr) {
            return;
        }
        const LRESULT selection = SendMessageW(animEventList_, LB_GETCURSEL, 0, 0);
        if (selection == LB_ERR || selection < 0
            || static_cast<std::size_t>(selection) >= editableAnim_->events.size()) {
            return;
        }
        const ri::content::NativeAnimationEvent& event =
            editableAnim_->events[static_cast<std::size_t>(selection)];
        SetWindowTextW(animEventName_, Widen(event.name).c_str());
        if (boundClip_.nodeTracks.empty()) {
            SetWindowTextW(animTime_, FormatTransformValue(static_cast<float>(event.timeSeconds)).c_str());
            return;
        }
        animPlayer_.Stop();
        animPlayer_.SetClip(&boundClip_);
        animPlayer_.SetTimeSeconds(event.timeSeconds);
        ApplyClipPoseToPreview();
        ApplySculptDisplayMesh();
        PublishPreview();
        SetWindowTextW(animTime_, FormatTransformValue(static_cast<float>(animPlayer_.TimeSeconds())).c_str());
        SyncAnimScrubFromPlayer();
        PopulateBonePoseFields();
    }

    void AddEventAtPlayhead() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(hwnd_, L"Open a motion clip before adding events.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        std::string name = ReadControlTextUtf8(animEventName_);
        while (!name.empty() && (name.front() == ' ' || name.front() == '\t')) {
            name.erase(name.begin());
        }
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
            name.pop_back();
        }
        if (name.empty()) {
            name = "event";
        }
        const double time = ReadCurrentAnimTime();
        if (!ri::scene::UpsertNativeAnimationEvent(*editableAnim_, time, name)) {
            MessageBoxW(
                hwnd_,
                L"Event name must be non-empty (max 64) and the clip can hold at most 256 events.",
                L"Forge clip",
                MB_OK | MB_ICONWARNING);
            return;
        }
        if (!ri::content::SaveNativeAnimationDocument(editableAnimPath_, *editableAnim_)) {
            MessageBoxW(hwnd_, L"Could not save clip events.", L"Forge", MB_OK | MB_ICONERROR);
            return;
        }
        int selected = 0;
        for (std::size_t index = 0; index < editableAnim_->events.size(); ++index) {
            const ri::content::NativeAnimationEvent& event = editableAnim_->events[index];
            if (event.name == name && std::abs(event.timeSeconds - time) <= 1.0 / 120.0) {
                selected = static_cast<int>(index);
                break;
            }
        }
        PopulateEventList(selected);
        SetWindowTextW(status_, Widen("Event " + name + " at " + std::to_string(time) + "s.").c_str());
    }

    void DeleteSelectedEvent() {
        if (!editableAnim_.has_value()) {
            return;
        }
        const LRESULT selection = SendMessageW(animEventList_, LB_GETCURSEL, 0, 0);
        if (selection == LB_ERR || selection < 0
            || static_cast<std::size_t>(selection) >= editableAnim_->events.size()) {
            MessageBoxW(hwnd_, L"Select an event to delete.", L"Forge clip", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string name = editableAnim_->events[static_cast<std::size_t>(selection)].name;
        if (!ri::scene::RemoveNativeAnimationEvent(*editableAnim_, static_cast<std::size_t>(selection))) {
            return;
        }
        if (!ri::content::SaveNativeAnimationDocument(editableAnimPath_, *editableAnim_)) {
            MessageBoxW(hwnd_, L"Could not save clip events.", L"Forge", MB_OK | MB_ICONERROR);
            return;
        }
        PopulateEventList();
        SetWindowTextW(status_, Widen("Removed event " + name + ".").c_str());
    }

    void RenameSelectedEvent() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(hwnd_, L"Open a motion clip before renaming events.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const LRESULT selection = SendMessageW(animEventList_, LB_GETCURSEL, 0, 0);
        if (selection == LB_ERR || selection < 0
            || static_cast<std::size_t>(selection) >= editableAnim_->events.size()) {
            MessageBoxW(hwnd_, L"Select an event to rename.", L"Forge clip", MB_OK | MB_ICONWARNING);
            return;
        }
        std::string name = ReadControlTextUtf8(animEventName_);
        while (!name.empty() && (name.front() == ' ' || name.front() == '\t')) {
            name.erase(name.begin());
        }
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
            name.pop_back();
        }
        if (name.empty()) {
            MessageBoxW(hwnd_, L"Enter a non-empty event name.", L"Forge clip", MB_OK | MB_ICONWARNING);
            return;
        }
        if (!ri::scene::RenameNativeAnimationEvent(
                *editableAnim_, static_cast<std::size_t>(selection), name)) {
            MessageBoxW(hwnd_, L"Could not rename that event.", L"Forge clip", MB_OK | MB_ICONWARNING);
            return;
        }
        if (!ri::content::SaveNativeAnimationDocument(editableAnimPath_, *editableAnim_)) {
            MessageBoxW(hwnd_, L"Could not save clip events.", L"Forge", MB_OK | MB_ICONERROR);
            return;
        }
        int selected = static_cast<int>(selection);
        for (std::size_t index = 0; index < editableAnim_->events.size(); ++index) {
            if (editableAnim_->events[index].name == name) {
                selected = static_cast<int>(index);
                break;
            }
        }
        PopulateEventList(selected);
        SetWindowTextW(status_, Widen("Renamed event to " + name + ".").c_str());
    }

    void StepSelectedEvent(const bool previous) {
        if (!editableAnim_.has_value() || editableAnim_->events.empty()) {
            return;
        }
        const LRESULT selection = SendMessageW(animEventList_, LB_GETCURSEL, 0, 0);
        int index = 0;
        if (selection != LB_ERR && selection >= 0
            && static_cast<std::size_t>(selection) < editableAnim_->events.size()) {
            index = static_cast<int>(selection);
            if (previous) {
                index = (std::max)(0, index - 1);
            } else {
                index = (std::min)(
                    static_cast<int>(editableAnim_->events.size()) - 1, index + 1);
            }
        } else {
            const double time = ReadCurrentAnimTime();
            if (previous) {
                index = 0;
                for (std::size_t i = 0; i < editableAnim_->events.size(); ++i) {
                    if (editableAnim_->events[i].timeSeconds <= time + 1.0 / 120.0) {
                        index = static_cast<int>(i);
                    }
                }
            } else {
                index = static_cast<int>(editableAnim_->events.size()) - 1;
                for (std::size_t i = 0; i < editableAnim_->events.size(); ++i) {
                    if (editableAnim_->events[i].timeSeconds + 1.0 / 120.0 >= time) {
                        index = static_cast<int>(i);
                        break;
                    }
                }
            }
        }
        SendMessageW(animEventList_, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
        JumpToSelectedEvent();
    }

    void MoveSelectedEventToPlayhead() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(
                hwnd_, L"Open a motion clip before moving events.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const LRESULT selection = SendMessageW(animEventList_, LB_GETCURSEL, 0, 0);
        if (selection == LB_ERR || selection < 0
            || static_cast<std::size_t>(selection) >= editableAnim_->events.size()) {
            MessageBoxW(hwnd_, L"Select an event to move to the playhead.", L"Forge clip", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string name = editableAnim_->events[static_cast<std::size_t>(selection)].name;
        const double time = ReadCurrentAnimTime();
        if (!ri::scene::SetNativeAnimationEventTime(
                *editableAnim_, static_cast<std::size_t>(selection), time)) {
            MessageBoxW(hwnd_, L"Could not move that event.", L"Forge clip", MB_OK | MB_ICONWARNING);
            return;
        }
        if (!ri::content::SaveNativeAnimationDocument(editableAnimPath_, *editableAnim_)) {
            MessageBoxW(hwnd_, L"Could not save clip events.", L"Forge", MB_OK | MB_ICONERROR);
            return;
        }
        int selected = 0;
        for (std::size_t index = 0; index < editableAnim_->events.size(); ++index) {
            const ri::content::NativeAnimationEvent& event = editableAnim_->events[index];
            if (event.name == name && std::abs(event.timeSeconds - time) <= 1.0 / 120.0) {
                selected = static_cast<int>(index);
                break;
            }
        }
        PopulateEventList(selected);
        SyncAnimDurationFromClip();
        SetWindowTextW(
            status_, Widen("Moved " + name + " to " + std::to_string(time) + "s.").c_str());
    }

    void DuplicateSelectedEventAtPlayhead() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(
                hwnd_, L"Open a motion clip before duplicating events.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const LRESULT selection = SendMessageW(animEventList_, LB_GETCURSEL, 0, 0);
        if (selection == LB_ERR || selection < 0
            || static_cast<std::size_t>(selection) >= editableAnim_->events.size()) {
            MessageBoxW(
                hwnd_, L"Select an event to duplicate onto the playhead.", L"Forge clip",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string name = editableAnim_->events[static_cast<std::size_t>(selection)].name;
        const double time = ReadCurrentAnimTime();
        if (!ri::scene::DuplicateNativeAnimationEventAt(
                *editableAnim_, static_cast<std::size_t>(selection), time)) {
            MessageBoxW(hwnd_, L"Could not duplicate that event.", L"Forge clip", MB_OK | MB_ICONWARNING);
            return;
        }
        if (!ri::content::SaveNativeAnimationDocument(editableAnimPath_, *editableAnim_)) {
            MessageBoxW(hwnd_, L"Could not save clip events.", L"Forge", MB_OK | MB_ICONERROR);
            return;
        }
        int selected = 0;
        for (std::size_t index = 0; index < editableAnim_->events.size(); ++index) {
            const ri::content::NativeAnimationEvent& event = editableAnim_->events[index];
            if (event.name == name && std::abs(event.timeSeconds - time) <= 1.0 / 120.0) {
                selected = static_cast<int>(index);
                break;
            }
        }
        PopulateEventList(selected);
        SyncAnimDurationFromClip();
        SetWindowTextW(
            status_, Widen("Duplicated " + name + " at " + std::to_string(time) + "s.").c_str());
    }

    [[nodiscard]] bool ReadAnimTrimWindow(double& startSeconds, double& endSeconds) const {
        startSeconds = 0.0;
        endSeconds = ClipDurationSeconds();
        if (const auto typedIn = ReadFiniteFloat(animIn_); typedIn.has_value() && *typedIn >= 0.0F) {
            startSeconds = static_cast<double>(*typedIn);
        }
        if (const auto typedOut = ReadFiniteFloat(animOut_); typedOut.has_value() && *typedOut > 0.0F) {
            endSeconds = static_cast<double>(*typedOut);
        }
        return endSeconds > startSeconds;
    }

    void StampAnimWindowFromPlayhead(const bool stampOut) {
        if (!editableAnim_.has_value()) {
            return;
        }
        const double time = ReadCurrentAnimTime();
        animWindowSyncing_ = true;
        SetWindowTextW(
            stampOut ? animOut_ : animIn_,
            FormatTransformValue(static_cast<float>(time)).c_str());
        animWindowSyncing_ = false;
        SetWindowTextW(
            status_,
            stampOut ? Widen("OUT " + std::to_string(time) + "s. Trim crops IN..OUT and shifts to 0.").c_str()
                     : Widen("IN " + std::to_string(time) + "s. Trim crops IN..OUT and shifts to 0.").c_str());
    }

    void ApplyAnimDurationFromEdit() {
        if (animDurationSyncing_ || !editableAnim_.has_value()) {
            return;
        }
        const auto typed = ReadFiniteFloat(animDuration_);
        if (!typed.has_value()
            || !ri::scene::SetNativeAnimationDuration(*editableAnim_, static_cast<double>(*typed))) {
            SyncAnimDurationFromClip();
            MessageBoxW(
                hwnd_,
                L"Duration must be greater than 0 and at most 600 seconds.",
                L"Forge clip",
                MB_OK | MB_ICONWARNING);
            return;
        }
        if (!ri::content::SaveNativeAnimationDocument(editableAnimPath_, *editableAnim_)) {
            MessageBoxW(hwnd_, L"Could not save clip duration.", L"Forge", MB_OK | MB_ICONERROR);
            return;
        }
        boundClip_.durationSeconds = editableAnim_->durationSeconds;
        const double time = std::min(animPlayer_.TimeSeconds(), ClipDurationSeconds());
        animPlayer_.SetClip(boundClip_.nodeTracks.empty() ? nullptr : &boundClip_);
        animPlayer_.SetTimeSeconds(time);
        if (!boundClip_.nodeTracks.empty()) {
            ApplyClipPoseToPreview();
            ApplySculptDisplayMesh();
            PublishPreview();
        }
        SetWindowTextW(animTime_, FormatTransformValue(static_cast<float>(animPlayer_.TimeSeconds())).c_str());
        SyncAnimScrubFromPlayer();
        SyncAnimDurationFromClip();
        SyncAnimWindowFromClip();
        PopulateEventList();
        SetWindowTextW(
            status_,
            Widen("Clip duration " + std::to_string(editableAnim_->durationSeconds)
                  + "s. Trim crops IN..OUT and shifts remaining keys to 0.")
                .c_str());
    }

    void TrimSelectedClip() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(hwnd_, L"Open a motion clip before trimming.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        double startSeconds = 0.0;
        double endSeconds = editableAnim_->durationSeconds;
        if (!ReadAnimTrimWindow(startSeconds, endSeconds)) {
            MessageBoxW(
                hwnd_,
                L"IN must be >= 0 and OUT must be greater than IN.",
                L"Forge clip",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const ri::scene::NativeAnimationTrimResult trimmed = ri::scene::TrimNativeAnimation(
            *editableAnim_, startSeconds, endSeconds, true);
        if (!trimmed.valid) {
            MessageBoxW(hwnd_, Widen(trimmed.summary).c_str(), L"Forge clip", MB_OK | MB_ICONWARNING);
            return;
        }
        if (!ri::content::SaveNativeAnimationDocument(editableAnimPath_, *editableAnim_)) {
            MessageBoxW(hwnd_, L"Could not save the trimmed clip.", L"Forge", MB_OK | MB_ICONERROR);
            return;
        }
        if (!previewBoneNodes_.empty()) {
            boundClip_ = ri::scene::BindNativeAnimationClip(
                *editableAnim_, previewScene_, previewBoneNodes_);
            animPlayer_.SetClip(&boundClip_);
        } else {
            boundClip_.durationSeconds = editableAnim_->durationSeconds;
        }
        const double time = std::min(animPlayer_.TimeSeconds(), ClipDurationSeconds());
        animPlayer_.SetTimeSeconds(time);
        if (!boundClip_.nodeTracks.empty()) {
            ApplyClipPoseToPreview();
            ApplySculptDisplayMesh();
            PublishPreview();
        }
        SetWindowTextW(animTime_, FormatTransformValue(static_cast<float>(animPlayer_.TimeSeconds())).c_str());
        SyncAnimScrubFromPlayer();
        SyncAnimDurationFromClip();
        SyncAnimWindowFromClip();
        PopulateEventList();
        SetWindowTextW(status_, Widen(trimmed.summary).c_str());
    }

    void UpdateLoopButton() {
        const bool looping = editableAnim_.has_value() ? editableAnim_->looping : true;
        SetWindowTextW(animLoopButton_, looping ? L"Loop" : L"Once");
        InvalidateRect(animLoopButton_, nullptr, TRUE);
        UpdateRootMotionButton();
    }

    void UpdateRootMotionButton() {
        const bool travel = !editableAnim_.has_value() || editableAnim_->rootMotion;
        SetWindowTextW(animRootButton_, travel ? L"Root" : L"Place");
        InvalidateRect(animRootButton_, nullptr, TRUE);
    }

    void ToggleRootMotion() {
        if (!editableAnim_.has_value()) {
            return;
        }
        editableAnim_->rootMotion = !editableAnim_->rootMotion;
        if (!ri::content::SaveNativeAnimationDocument(editableAnimPath_, *editableAnim_)) {
            editableAnim_->rootMotion = !editableAnim_->rootMotion;
            MessageBoxW(hwnd_, L"Could not save root-motion mode.", L"Forge", MB_OK | MB_ICONERROR);
            UpdateRootMotionButton();
            return;
        }
        UpdateRootMotionButton();
        if (!boundClip_.nodeTracks.empty()) {
            ApplyClipPoseToPreview();
            ApplySculptDisplayMesh();
            PublishPreview();
        }
        SetWindowTextW(
            status_,
            editableAnim_->rootMotion ? L"Root motion travels with the clip."
                                      : L"In-place: root translation stays at rest.");
    }

    void ToggleAnimLoop() {
        if (!editableAnim_.has_value()) {
            return;
        }
        editableAnim_->looping = !editableAnim_->looping;
        boundClip_.looping = editableAnim_->looping;
        animPlayer_.SetLooping(editableAnim_->looping);
        if (!ri::content::SaveNativeAnimationDocument(editableAnimPath_, *editableAnim_)) {
            editableAnim_->looping = !editableAnim_->looping;
            boundClip_.looping = editableAnim_->looping;
            animPlayer_.SetLooping(editableAnim_->looping);
            MessageBoxW(hwnd_, L"Could not save loop mode.", L"Forge", MB_OK | MB_ICONERROR);
            UpdateLoopButton();
            return;
        }
        UpdateLoopButton();
        SetWindowTextW(status_, editableAnim_->looping ? L"Clip loops." : L"Clip plays once.");
    }

    void TickAnimation() {
        if (!animPlayer_.IsPlaying()) {
            animClockInit_ = false;
            return;
        }
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        if (!animClockInit_) {
            animQpc_ = now;
            animClockInit_ = true;
            return;
        }
        LARGE_INTEGER freq{};
        QueryPerformanceFrequency(&freq);
        const double delta = freq.QuadPart == 0
            ? 0.0
            : static_cast<double>(now.QuadPart - animQpc_.QuadPart) / static_cast<double>(freq.QuadPart);
        animQpc_ = now;
        const double previousTime = animPlayer_.TimeSeconds();
        animPlayer_.AdvanceSeconds(delta);
        NotifyClipEventsBetween(previousTime, animPlayer_.TimeSeconds());
        ApplyClipPoseToPreview();
        ApplySculptDisplayMesh();
        PublishPreview();
        SetWindowTextW(animTime_, FormatTransformValue(static_cast<float>(animPlayer_.TimeSeconds())).c_str());
        SyncAnimScrubFromPlayer();
    }

    void PlaySelectedClip() {
        if (boundClip_.nodeTracks.empty()) {
            SetWindowTextW(status_, L"Select a motion clip to play.");
            return;
        }
        animPlayer_.SetClip(&boundClip_);
        animPlayer_.Play(true);
        animClockInit_ = false;
        SetForgeBay(kBayMotion);
        SyncAnimScrubFromPlayer();
        SetWindowTextW(status_, L"Playing clip.");
    }

    void StopSelectedClip() {
        animPlayer_.Stop();
        if (!boundClip_.nodeTracks.empty()) {
            animPlayer_.SetTimeSeconds(0.0);
            ApplyClipPoseToPreview();
            ApplySculptDisplayMesh();
            PublishPreview();
        }
        SetWindowTextW(animTime_, L"0");
        SyncAnimScrubFromPlayer();
        PopulateBonePoseFields();
        SetWindowTextW(status_, L"Clip stopped.");
    }

    void KeyCurrentPose() {
        if (!editableAnim_.has_value() || previewBoneNodes_.empty()) {
            MessageBoxW(hwnd_, L"Open a motion clip before keying a pose.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        double time = ReadCurrentAnimTime();
        ri::scene::CaptureNativeAnimationPose(*editableAnim_, previewScene_, previewBoneNodes_, time);
        if (!ri::content::SaveNativeAnimationDocument(editableAnimPath_, *editableAnim_)) {
            MessageBoxW(hwnd_, L"Could not save the keyed pose.", L"Forge", MB_OK | MB_ICONERROR);
            return;
        }
        boundClip_ = ri::scene::BindNativeAnimationClip(*editableAnim_, previewScene_, previewBoneNodes_);
        animPlayer_.SetClip(&boundClip_);
        animPlayer_.SetTimeSeconds(time);
        SyncAnimScrubFromPlayer();
        SyncAnimDurationFromClip();
        SetWindowTextW(status_, Widen("Keyed pose at " + std::to_string(time) + "s").c_str());
    }

    void KeySelectedBone() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(hwnd_, L"Open a motion clip before keying a bone.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const int node = SelectedBoneNode();
        if (node == ri::scene::kInvalidHandle) {
            MessageBoxW(hwnd_, L"Select a bone to key.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const ri::scene::Node& bone = previewScene_.GetNode(node);
        const double time = ReadCurrentAnimTime();
        ri::scene::UpsertNativeAnimationKey(*editableAnim_, bone.name, time, bone.localTransform);
        if (!ri::content::SaveNativeAnimationDocument(editableAnimPath_, *editableAnim_)) {
            MessageBoxW(hwnd_, L"Could not save the keyed bone.", L"Forge", MB_OK | MB_ICONERROR);
            return;
        }
        boundClip_ = ri::scene::BindNativeAnimationClip(*editableAnim_, previewScene_, previewBoneNodes_);
        animPlayer_.SetClip(&boundClip_);
        animPlayer_.SetTimeSeconds(time);
        SyncAnimScrubFromPlayer();
        SyncAnimDurationFromClip();
        SetWindowTextW(status_, Widen("Keyed " + bone.name + " at " + std::to_string(time) + "s").c_str());
    }

    [[nodiscard]] bool ReloadBoundClipAfterEdit(const double timeSeconds) {
        if (!editableAnim_.has_value() || editableAnimPath_.empty()) {
            return false;
        }
        if (!ri::content::SaveNativeAnimationDocument(editableAnimPath_, *editableAnim_)) {
            MessageBoxW(hwnd_, L"Could not save the motion clip.", L"Forge", MB_OK | MB_ICONERROR);
            return false;
        }
        if (!previewBoneNodes_.empty()) {
            boundClip_ = ri::scene::BindNativeAnimationClip(
                *editableAnim_, previewScene_, previewBoneNodes_);
            animPlayer_.SetClip(&boundClip_);
            animPlayer_.SetTimeSeconds(timeSeconds);
            ApplyClipPoseToPreview();
            ApplySculptDisplayMesh();
            PublishPreview();
        }
        SyncAnimScrubFromPlayer();
        SyncAnimDurationFromClip();
        PopulateBonePoseFields();
        return true;
    }

    void KeyRestAtPlayhead() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(hwnd_, L"Open a motion clip before keying rest.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        if (!editableRig_.has_value()) {
            MessageBoxW(
                hwnd_,
                L"Bind a rig before inserting rest keys.",
                L"Forge",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string boneName = SelectedBoneName();
        const double time = ReadCurrentAnimTime();
        const std::size_t changed = ri::scene::InsertNativeAnimationRestKeys(
            *editableAnim_, *editableRig_, time, boneName);
        if (changed == 0U) {
            SetWindowTextW(status_, L"No rest keys were written.");
            return;
        }
        if (!ReloadBoundClipAfterEdit(time)) {
            return;
        }
        if (boneName.empty()) {
            SetWindowTextW(
                status_,
                Widen("Rest-keyed " + std::to_string(changed) + " bones at "
                    + std::to_string(time) + "s").c_str());
        } else {
            SetWindowTextW(
                status_,
                Widen("Rest-keyed " + boneName + " at " + std::to_string(time) + "s").c_str());
        }
    }

    void MirrorSelectedAnimKeys() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(
                hwnd_, L"Open a motion clip before mirroring keys.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string boneName = SelectedBoneName();
        if (boneName.empty()) {
            MessageBoxW(
                hwnd_, L"Select a bone whose keys should mirror across X.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::size_t changed =
            ri::scene::MirrorNativeAnimationTrackAcrossX(*editableAnim_, boneName);
        if (changed == 0U) {
            MessageBoxW(
                hwnd_,
                Widen("No keys to mirror from " + boneName
                    + " (needs a left/right partner and keyed track).").c_str(),
                L"Forge",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const double time = ReadCurrentAnimTime();
        if (!ReloadBoundClipAfterEdit(time)) {
            return;
        }
        const std::optional<std::string> partner = ri::scene::MirrorPartnerBoneName(boneName);
        SetWindowTextW(
            status_,
            Widen("Mirrored " + std::to_string(changed) + " keys from " + boneName
                + (partner.has_value() ? " onto " + *partner : "") + ".").c_str());
    }

    void DuplicateSelectedSculpt() {
        fs::path sculptPath = editableSculptPath_;
        if (sculptPath.empty()) {
            if (const ri::forge::AssetEntry* asset = SelectedAsset();
                asset != nullptr && asset->kind == ri::forge::AssetKind::Sculpt) {
                sculptPath = asset->absolutePath;
            }
        }
        if (sculptPath.empty()) {
            MessageBoxW(hwnd_, L"Select clay to duplicate.", L"Forge clay", MB_OK | MB_ICONWARNING);
            return;
        }
        if (!FlushLiveSculpt()) {
            return;
        }
        std::string error;
        const fs::path output =
            ri::forge::DuplicateNativeSculpt(workspaceRoot_, sculptPath, &error);
        if (output.empty()) {
            MessageBoxW(
                hwnd_,
                error.empty() ? L"Could not duplicate clay." : Widen(error).c_str(),
                L"Forge clay",
                MB_OK | MB_ICONERROR);
            return;
        }
        SetWindowTextW(status_, Widen("Duplicated clay as " + output.filename().string()).c_str());
        SetForgeBay(kBayClay);
        RefreshCatalog(output);
    }

    void DeleteSelectedSculpt() {
        fs::path sculptPath = editableSculptPath_;
        if (sculptPath.empty()) {
            if (const ri::forge::AssetEntry* asset = SelectedAsset();
                asset != nullptr && asset->kind == ri::forge::AssetKind::Sculpt) {
                sculptPath = asset->absolutePath;
            }
        }
        if (sculptPath.empty()) {
            MessageBoxW(hwnd_, L"Select clay to delete.", L"Forge clay", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::wstring prompt =
            L"Delete clay\n" + Widen(sculptPath.filename().string()) + L"?\nThis cannot be undone.";
        if (MessageBoxW(hwnd_, prompt.c_str(), L"Forge clay", MB_YESNO | MB_ICONWARNING) != IDYES) {
            return;
        }
        const bool clearingLive = editableSculptPath_ == sculptPath;
        if (clearingLive) {
            sculptDirty_ = false;
            editableSculpt_.reset();
            editableSculptPath_.clear();
            sculptUndo_.Clear();
        }
        std::string error;
        if (!ri::forge::DeleteNativeSculpt(sculptPath, &error)) {
            MessageBoxW(
                hwnd_,
                error.empty() ? L"Could not delete clay." : Widen(error).c_str(),
                L"Forge clay",
                MB_OK | MB_ICONERROR);
            return;
        }
        SetWindowTextW(status_, Widen("Deleted " + sculptPath.filename().string()).c_str());
        RefreshCatalog({});
        SyncAuthoringEnable();
        Rebuild3DPreview(SelectedAsset(), true);
    }

    void DeleteSelectedBoneKey() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(hwnd_, L"Open a motion clip before deleting a key.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const int node = SelectedBoneNode();
        if (node == ri::scene::kInvalidHandle) {
            MessageBoxW(hwnd_, L"Select a bone whose key you want to delete.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const ri::scene::Node& bone = previewScene_.GetNode(node);
        const double time = ReadCurrentAnimTime();
        if (!ri::scene::RemoveNativeAnimationKey(*editableAnim_, bone.name, time)) {
            MessageBoxW(
                hwnd_,
                Widen("No key on " + bone.name + " at " + std::to_string(time) + "s.").c_str(),
                L"Forge",
                MB_OK | MB_ICONWARNING);
            return;
        }
        if (!ri::content::SaveNativeAnimationDocument(editableAnimPath_, *editableAnim_)) {
            MessageBoxW(hwnd_, L"Could not save after deleting the key.", L"Forge", MB_OK | MB_ICONERROR);
            return;
        }
        boundClip_ = ri::scene::BindNativeAnimationClip(*editableAnim_, previewScene_, previewBoneNodes_);
        animPlayer_.SetClip(&boundClip_);
        animPlayer_.SetTimeSeconds(time);
        ApplyClipPoseToPreview();
        ApplySculptDisplayMesh();
        PublishPreview();
        SyncAnimScrubFromPlayer();
        SyncAnimDurationFromClip();
        SetWindowTextW(status_, Widen("Deleted " + bone.name + " key at " + std::to_string(time) + "s").c_str());
    }

    void ClearSelectedAnimTrack() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(hwnd_, L"Open a motion clip before clearing a track.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string boneName = SelectedBoneName();
        if (boneName.empty()) {
            MessageBoxW(hwnd_, L"Select a bone whose track to clear.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::size_t removed = ri::scene::ClearNativeAnimationTrack(*editableAnim_, boneName);
        if (removed == 0U) {
            SetWindowTextW(status_, Widen("No keys on " + boneName + ".").c_str());
            return;
        }
        const double time = ReadCurrentAnimTime();
        if (!ReloadBoundClipAfterEdit(time)) {
            return;
        }
        SetWindowTextW(
            status_,
            Widen("Cleared " + std::to_string(removed) + " keys from " + boneName + ".").c_str());
    }

    void ClearAllAnimKeys() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(hwnd_, L"Open a motion clip before clearing keys.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        if (MessageBoxW(
                hwnd_,
                L"Clear every keyframe on this clip?\nEvents and duration stay.",
                L"Forge",
                MB_YESNO | MB_ICONWARNING)
            != IDYES) {
            return;
        }
        const std::size_t removed = ri::scene::ClearNativeAnimationKeys(*editableAnim_);
        if (removed == 0U) {
            SetWindowTextW(status_, L"Clip already had no keys.");
            return;
        }
        if (!ReloadBoundClipAfterEdit(0.0)) {
            return;
        }
        SetWindowTextW(
            status_, Widen("Cleared " + std::to_string(removed) + " keys from the clip.").c_str());
    }

    void DeduplicateAnimKeys() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(
                hwnd_, L"Open a motion clip before cleaning keys.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::size_t removed = ri::scene::DeduplicateNativeAnimationKeys(*editableAnim_);
        if (removed == 0U) {
            SetWindowTextW(status_, L"No duplicate keys to clean.");
            return;
        }
        if (!ReloadBoundClipAfterEdit(ReadCurrentAnimTime())) {
            return;
        }
        SetWindowTextW(
            status_,
            Widen("Removed " + std::to_string(removed) + " duplicate keys.").c_str());
    }

    void QuantizeAnimKeys() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(
                hwnd_, L"Open a motion clip before quantizing.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::size_t changed = ri::scene::QuantizeNativeAnimationTimes(*editableAnim_, 30.0);
        if (changed == 0U) {
            SetWindowTextW(status_, L"Clip already sits on the 30fps grid.");
            return;
        }
        if (!ReloadBoundClipAfterEdit(ReadCurrentAnimTime())) {
            return;
        }
        SyncAnimWindowFromClip();
        PopulateEventList();
        SetWindowTextW(
            status_,
            Widen("Quantized " + std::to_string(changed) + " stamps to 30fps.").c_str());
    }

    void StripMissingAnimTracks() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(
                hwnd_, L"Open a motion clip before stripping tracks.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        if (previewBoneNodes_.empty()) {
            MessageBoxW(
                hwnd_,
                L"Open a skinned mesh or rig so Strip can see which bones exist.",
                L"Forge",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const std::size_t removed = ri::scene::StripNativeAnimationMissingTracks(
            *editableAnim_, previewScene_, previewBoneNodes_);
        if (removed == 0U) {
            SetWindowTextW(status_, L"No unbound tracks to strip.");
            return;
        }
        if (!ReloadBoundClipAfterEdit(ReadCurrentAnimTime())) {
            return;
        }
        SetWindowTextW(
            status_, Widen("Stripped " + std::to_string(removed) + " unbound tracks.").c_str());
    }

    void ClearAllAnimEvents() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(
                hwnd_, L"Open a motion clip before clearing events.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        if (editableAnim_->events.empty()) {
            SetWindowTextW(status_, L"Clip already has no events.");
            return;
        }
        if (MessageBoxW(
                hwnd_,
                L"Clear every event marker on this clip?",
                L"Forge",
                MB_YESNO | MB_ICONWARNING)
            != IDYES) {
            return;
        }
        const std::size_t removed = ri::scene::ClearNativeAnimationEvents(*editableAnim_);
        if (!ri::content::SaveNativeAnimationDocument(editableAnimPath_, *editableAnim_)) {
            MessageBoxW(hwnd_, L"Could not save clip events.", L"Forge", MB_OK | MB_ICONERROR);
            return;
        }
        PopulateEventList();
        SetWindowTextW(
            status_, Widen("Cleared " + std::to_string(removed) + " events.").c_str());
    }

    void ShiftClipPlayheadToZero() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(hwnd_, L"Open a motion clip before shifting.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const double playhead = ReadCurrentAnimTime();
        if (playhead <= 1.0 / 120.0) {
            SetWindowTextW(status_, L"Playhead is already at the start.");
            return;
        }
        const double endSeconds = (std::max)(editableAnim_->durationSeconds, playhead);
        const ri::scene::NativeAnimationTrimResult trimmed =
            ri::scene::TrimNativeAnimation(*editableAnim_, playhead, endSeconds, true);
        if (!trimmed.valid) {
            MessageBoxW(hwnd_, Widen(trimmed.summary).c_str(), L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        if (!ReloadBoundClipAfterEdit(0.0)) {
            return;
        }
        SyncAnimWindowFromClip();
        PopulateEventList();
        SetWindowTextW(status_, Widen(trimmed.summary).c_str());
    }

    void DuplicateSelectedClip() {
        fs::path clipPath = editableAnimPath_;
        if (clipPath.empty()) {
            if (const ri::forge::AssetEntry* asset = SelectedAsset();
                asset != nullptr && asset->kind == ri::forge::AssetKind::Animation) {
                clipPath = asset->absolutePath;
            }
        }
        if (clipPath.empty()) {
            MessageBoxW(hwnd_, L"Open a motion clip before duplicating it.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        std::string error;
        const fs::path output = ri::forge::DuplicateNativeAnimation(workspaceRoot_, clipPath, &error);
        if (output.empty()) {
            MessageBoxW(
                hwnd_,
                error.empty() ? L"Could not duplicate the clip." : Widen(error).c_str(),
                L"Forge",
                MB_OK | MB_ICONERROR);
            return;
        }
        SetWindowTextW(status_, Widen("Duplicated clip as " + output.filename().string()).c_str());
        BindClipOntoPreview(output);
        SetForgeBay(kBayMotion);
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
        pendingModelElementId_ = groupId;
        RefreshCatalog(asset->absolutePath);
    }

    void DuplicateSelectedStockElement() {
        const ri::forge::AssetEntry* asset = SelectedAsset();
        if (asset == nullptr || asset->kind != ri::forge::AssetKind::PrimitiveModel) {
            return;
        }
        const std::string sourceId = CurrentModelElementId();
        if (sourceId.empty()) {
            MessageBoxW(
                hwnd_, L"Select a part or pivot to duplicate.", L"Forge stock", MB_OK | MB_ICONWARNING);
            return;
        }
        std::string insertedId;
        std::string error;
        if (!ri::forge::DuplicatePrimitiveModelElement(
                asset->absolutePath, sourceId, &insertedId, &error)) {
            MessageBoxW(hwnd_, Widen(error).c_str(), L"Forge stock", MB_OK | MB_ICONERROR);
            return;
        }
        pendingModelElementId_ = insertedId;
        SetWindowTextW(status_, Widen("Duplicated " + sourceId + " as " + insertedId).c_str());
        RefreshCatalog(asset->absolutePath);
    }

    void DeleteSelectedStockElement() {
        const ri::forge::AssetEntry* asset = SelectedAsset();
        if (asset == nullptr || asset->kind != ri::forge::AssetKind::PrimitiveModel) {
            return;
        }
        const std::string sourceId = CurrentModelElementId();
        if (sourceId.empty()) {
            MessageBoxW(
                hwnd_, L"Select a part or pivot to delete.", L"Forge stock", MB_OK | MB_ICONWARNING);
            return;
        }
        std::string error;
        if (!ri::forge::RemovePrimitiveModelElement(asset->absolutePath, sourceId, &error)) {
            MessageBoxW(hwnd_, Widen(error).c_str(), L"Forge stock", MB_OK | MB_ICONERROR);
            return;
        }
        pendingModelElementId_.clear();
        SetWindowTextW(status_, Widen("Deleted " + sourceId).c_str());
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
        pendingModelElementId_ = partId;
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

    void BindSelectedRig() {
        const fs::path rigPath = PreferredRigPath();
        if (rigPath.empty()) {
            MessageBoxW(
                hwnd_,
                L"Create or select a rig, then Bind Rig on clay or stock.",
                L"Forge bind",
                MB_OK | MB_ICONWARNING);
            return;
        }

        fs::path targetPath = lastBindTargetPath_;
        ri::forge::AssetKind targetKind = lastBindTargetKind_;
        if (const ri::forge::AssetEntry* selected = SelectedAsset();
            selected != nullptr
            && (selected->kind == ri::forge::AssetKind::Sculpt
                || selected->kind == ri::forge::AssetKind::PrimitiveModel)
            && selected->valid) {
            targetPath = selected->absolutePath;
            targetKind = selected->kind;
        }
        if (targetPath.empty() && editableSculpt_.has_value()) {
            targetPath = editableSculptPath_;
            targetKind = ri::forge::AssetKind::Sculpt;
        }
        if (targetPath.empty() && editableModel_.has_value()) {
            targetPath = editableModelPath_;
            targetKind = ri::forge::AssetKind::PrimitiveModel;
        }
        if (targetPath.empty()) {
            MessageBoxW(
                hwnd_,
                L"Open clay or stock before binding a rig.",
                L"Forge bind",
                MB_OK | MB_ICONWARNING);
            return;
        }

        std::string error;
        bool bound = false;
        if (targetKind == ri::forge::AssetKind::Sculpt
            && editableSculpt_.has_value()
            && editableSculptPath_ == targetPath) {
            const auto rig = ri::scene::LoadRigDefinition(rigPath);
            if (!rig.has_value() || !ri::scene::ValidateRigDefinition(*rig).valid) {
                MessageBoxW(hwnd_, L"Bind needs a valid rig document.", L"Forge bind",
                            MB_OK | MB_ICONWARNING);
                return;
            }
            const ri::content::NativeSculptDocument previous = *editableSculpt_;
            const ri::scene::NativeSculptBindResult result = ri::scene::BindNativeSculptToRig(
                *editableSculpt_,
                *rig,
                ri::forge::RelativeSourcePath(workspaceRoot_, rigPath));
            if (!result.valid) {
                MessageBoxW(hwnd_, Widen(result.summary).c_str(), L"Forge bind", MB_OK | MB_ICONWARNING);
                return;
            }
            const bool keptPaint = previous.vertexBoneNames == editableSculpt_->vertexBoneNames
                && previous.rigPath == editableSculpt_->rigPath;
            if (!keptPaint) {
                sculptUndo_.Capture(previous);
                sculptDirty_ = true;
                if (!SaveActiveSculpt()) {
                    return;
                }
            }
            error = keptPaint ? result.summary + " (kept painted weights)" : result.summary;
            bound = true;
        } else if (targetKind == ri::forge::AssetKind::Sculpt) {
            bound = ri::forge::BindSculptToRig(targetPath, rigPath, workspaceRoot_, &error);
        } else if (targetKind == ri::forge::AssetKind::PrimitiveModel
            && editableModel_.has_value()
            && editableModelPath_ == targetPath) {
            const auto rig = ri::scene::LoadRigDefinition(rigPath);
            if (!rig.has_value() || !ri::scene::ValidateRigDefinition(*rig).valid) {
                MessageBoxW(hwnd_, L"Bind needs a valid rig document.", L"Forge bind",
                            MB_OK | MB_ICONWARNING);
                return;
            }
            editableModel_->rigPath = ri::forge::RelativeSourcePath(workspaceRoot_, rigPath);
            if (!ri::content::SavePrimitiveModelDocument(editableModelPath_, *editableModel_)) {
                MessageBoxW(hwnd_, L"Could not save the bound stock model.", L"Forge bind",
                            MB_OK | MB_ICONERROR);
                return;
            }
            StampWriteTime(editableModelPath_, editableModelWriteTime_, editableModelHasWriteTime_);
            error = "Attached rig " + editableModel_->rigPath;
            bound = true;
        } else if (targetKind == ri::forge::AssetKind::PrimitiveModel) {
            bound = ri::forge::BindPrimitiveModelToRig(targetPath, rigPath, workspaceRoot_, &error);
            if (bound && editableModel_.has_value() && editableModelPath_ == targetPath) {
                editableModel_ = ri::content::LoadPrimitiveModelDocument(targetPath);
            }
        }
        if (!bound) {
            MessageBoxW(hwnd_, Widen(error).c_str(), L"Forge bind", MB_OK | MB_ICONWARNING);
            return;
        }
        lastRigPath_ = rigPath;
        lastBindTargetPath_ = targetPath;
        lastBindTargetKind_ = targetKind;
        SetWindowTextW(status_, Widen(error).c_str());
        RefreshCatalog(targetPath);
        if (const ri::forge::AssetEntry* selected = SelectedAsset(); selected != nullptr) {
            Rebuild3DPreview(selected, true);
        }
    }

    void BindSelectedBone() {
        if (!editableModel_.has_value() || editableModelPath_.empty()) {
            MessageBoxW(
                hwnd_,
                L"Open stock and Bind Rig before assigning a bone.",
                L"Forge bind",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string boneName = SelectedBoneName();
        const std::string elementId = CurrentModelElementId();
        if (boneName.empty() || elementId.empty()) {
            MessageBoxW(
                hwnd_,
                L"Select a stock part or pivot and a bone, then Bind Bone.",
                L"Forge bind",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const LRESULT selection = SendMessageW(modelElement_, LB_GETCURSEL, 0, 0);
        if (selection == LB_ERR || selection < 0
            || static_cast<std::size_t>(selection) >= modelElements_.size()) {
            return;
        }
        const ModelElementRef& element = modelElements_[static_cast<std::size_t>(selection)];
        if (element.group && element.index < editableModel_->groups.size()) {
            editableModel_->groups[element.index].boneName = boneName;
        } else if (!element.group && element.index < editableModel_->parts.size()) {
            editableModel_->parts[element.index].boneName = boneName;
        } else {
            return;
        }
        if (!ri::content::SavePrimitiveModelDocument(editableModelPath_, *editableModel_)) {
            MessageBoxW(hwnd_, L"Could not save the bone binding.", L"Forge bind", MB_OK | MB_ICONERROR);
            return;
        }
        StampWriteTime(editableModelPath_, editableModelWriteTime_, editableModelHasWriteTime_);
        CaptureStockPartRest();
        SetWindowTextW(status_, Widen("Bound " + elementId + " to " + boneName).c_str());
    }

    void FloodSelectedBone() {
        if (!editableSculpt_.has_value()) {
            MessageBoxW(
                hwnd_,
                L"Open clay and Bind Rig before flooding a bone.",
                L"Forge weights",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string boneName = SelectedBoneName();
        if (boneName.empty()) {
            MessageBoxW(
                hwnd_,
                L"Select a bone in BONES, then Flood Bone.",
                L"Forge weights",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const ri::content::NativeSculptDocument previous = *editableSculpt_;
        const std::size_t painted = ri::scene::FloodNativeSculptWeights(*editableSculpt_, boneName);
        if (painted == 0U) {
            SetWindowTextW(status_, Widen("All verts already on " + boneName).c_str());
            return;
        }
        sculptUndo_.Capture(previous);
        sculptDirty_ = true;
        showWeights_ = true;
        ApplySculptDisplayMesh();
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
        SyncForgeTitle();
        if (!SaveActiveSculpt()) {
            return;
        }
        SetWindowTextW(
            status_,
            Widen("Flooded " + std::to_string(painted) + " verts onto " + boneName).c_str());
    }

    void FloodUnboundSelectedBone() {
        if (!editableSculpt_.has_value()) {
            MessageBoxW(
                hwnd_,
                L"Open clay and Bind Rig before flooding unbound verts.",
                L"Forge weights",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string boneName = SelectedBoneName();
        if (boneName.empty()) {
            MessageBoxW(
                hwnd_,
                L"Select a bone in BONES, then Flood Free.",
                L"Forge weights",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const ri::content::NativeSculptDocument previous = *editableSculpt_;
        const std::size_t painted =
            ri::scene::FloodUnboundNativeSculptWeights(*editableSculpt_, boneName);
        if (painted == 0U) {
            SetWindowTextW(status_, L"No unbound verts to flood.");
            return;
        }
        sculptUndo_.Capture(previous);
        sculptDirty_ = true;
        showWeights_ = true;
        ApplySculptDisplayMesh();
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
        SyncForgeTitle();
        if (!SaveActiveSculpt()) {
            return;
        }
        SetWindowTextW(
            status_,
            Widen("Flooded " + std::to_string(painted) + " free verts onto " + boneName).c_str());
    }

    void TransferSelectedBoneWeights() {
        if (!editableSculpt_.has_value()) {
            MessageBoxW(
                hwnd_,
                L"Open clay before transferring weights.",
                L"Forge weights",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string boneName = SelectedBoneName();
        if (boneName.empty()) {
            MessageBoxW(
                hwnd_,
                L"Select the source bone, then Xfer→.",
                L"Forge weights",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const std::optional<std::string> partner = ri::scene::MirrorPartnerBoneName(boneName);
        if (!partner.has_value()) {
            MessageBoxW(
                hwnd_,
                Widen(boneName + " has no left/right partner to transfer onto.").c_str(),
                L"Forge weights",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const ri::content::NativeSculptDocument previous = *editableSculpt_;
        const std::size_t changed =
            ri::scene::TransferNativeSculptWeights(*editableSculpt_, boneName, *partner);
        if (changed == 0U) {
            SetWindowTextW(status_, Widen("No weights on " + boneName + " to transfer.").c_str());
            return;
        }
        sculptUndo_.Capture(previous);
        sculptDirty_ = true;
        showWeights_ = true;
        ApplySculptDisplayMesh();
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
        SyncForgeTitle();
        if (!SaveActiveSculpt()) {
            return;
        }
        SetWindowTextW(
            status_,
            Widen("Transferred " + std::to_string(changed) + " verts from " + boneName + " onto "
                + *partner + ".").c_str());
    }

    void SwapSelectedBoneWeights() {
        if (!editableSculpt_.has_value()) {
            MessageBoxW(
                hwnd_,
                L"Open clay before swapping weights.",
                L"Forge weights",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string boneName = SelectedBoneName();
        if (boneName.empty()) {
            MessageBoxW(
                hwnd_,
                L"Select a left/right bone, then Swap↔.",
                L"Forge weights",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const std::optional<std::string> partner = ri::scene::MirrorPartnerBoneName(boneName);
        if (!partner.has_value()) {
            MessageBoxW(
                hwnd_,
                Widen(boneName + " has no left/right partner to swap with.").c_str(),
                L"Forge weights",
                MB_OK | MB_ICONWARNING);
            return;
        }
        const ri::content::NativeSculptDocument previous = *editableSculpt_;
        const std::size_t changed =
            ri::scene::SwapNativeSculptWeights(*editableSculpt_, boneName, *partner);
        if (changed == 0U) {
            SetWindowTextW(
                status_,
                Widen("No weights on " + boneName + " / " + *partner + " to swap.").c_str());
            return;
        }
        sculptUndo_.Capture(previous);
        sculptDirty_ = true;
        showWeights_ = true;
        ApplySculptDisplayMesh();
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
        SyncForgeTitle();
        if (!SaveActiveSculpt()) {
            return;
        }
        SetWindowTextW(
            status_,
            Widen("Swapped " + std::to_string(changed) + " verts between " + boneName + " and "
                + *partner + ".").c_str());
    }

    void ScaleSelectedClipTime(const double factor) {
        if (!editableAnim_.has_value()) {
            MessageBoxW(hwnd_, L"Open a motion clip before retiming.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        if (!ri::scene::ScaleNativeAnimationTime(*editableAnim_, factor)) {
            MessageBoxW(hwnd_, L"Could not scale clip timing.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const double time = std::min(ReadCurrentAnimTime() * factor, editableAnim_->durationSeconds);
        if (!ReloadBoundClipAfterEdit(time)) {
            return;
        }
        SyncAnimWindowFromClip();
        PopulateEventList();
        SetWindowTextW(
            status_,
            Widen(std::string("Scaled clip timing by ")
                + (factor < 1.0 ? "0.5" : "2.0") + "x.").c_str());
    }

    void NudgeSelectedClipTime(const double deltaSeconds) {
        if (!editableAnim_.has_value()) {
            MessageBoxW(hwnd_, L"Open a motion clip before nudging.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        if (!ri::scene::OffsetNativeAnimationTimes(*editableAnim_, deltaSeconds)) {
            SetWindowTextW(status_, L"Clip timing was unchanged.");
            return;
        }
        const double time = std::clamp(
            ReadCurrentAnimTime() + deltaSeconds, 0.0, editableAnim_->durationSeconds);
        if (!ReloadBoundClipAfterEdit(time)) {
            return;
        }
        SyncAnimWindowFromClip();
        PopulateEventList();
        SetWindowTextW(
            status_,
            Widen(std::string("Nudged clip by ") + (deltaSeconds < 0.0 ? "-" : "+") + "0.1s.").c_str());
    }

    void CopySelectedAnimTrack() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(hwnd_, L"Open a motion clip before copying keys.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string boneName = SelectedBoneName();
        if (boneName.empty()) {
            MessageBoxW(hwnd_, L"Select a bone whose keys to copy.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        copiedAnimTrack_ = {};
        for (const ri::content::NativeAnimationTrack& track : editableAnim_->tracks) {
            if (track.boneName == boneName) {
                copiedAnimTrack_ = track;
                break;
            }
        }
        if (copiedAnimTrack_.keys.empty()) {
            SetWindowTextW(status_, Widen("No keys on " + boneName + " to copy.").c_str());
            SyncAuthoringEnable();
            return;
        }
        SyncAuthoringEnable();
        SetWindowTextW(
            status_,
            Widen("Copied " + std::to_string(copiedAnimTrack_.keys.size()) + " keys from " + boneName
                + ".").c_str());
    }

    void PasteSelectedAnimTrack() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(hwnd_, L"Open a motion clip before pasting keys.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        if (copiedAnimTrack_.keys.empty() || copiedAnimTrack_.boneName.empty()) {
            MessageBoxW(hwnd_, L"Copy Keys first.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::string boneName = SelectedBoneName();
        if (boneName.empty()) {
            MessageBoxW(hwnd_, L"Select a destination bone for Paste Keys.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        // Ensure clipboard source track exists, then copy onto the destination bone.
        bool hasSource = false;
        for (ri::content::NativeAnimationTrack& track : editableAnim_->tracks) {
            if (track.boneName == copiedAnimTrack_.boneName) {
                track.keys = copiedAnimTrack_.keys;
                hasSource = true;
                break;
            }
        }
        if (!hasSource) {
            editableAnim_->tracks.push_back(copiedAnimTrack_);
        }
        const std::size_t changed = ri::scene::CopyNativeAnimationTrack(
            *editableAnim_, copiedAnimTrack_.boneName, boneName);
        if (changed == 0U) {
            SetWindowTextW(status_, L"Paste Keys wrote nothing.");
            return;
        }
        const double time = ReadCurrentAnimTime();
        if (!ReloadBoundClipAfterEdit(time)) {
            return;
        }
        SetWindowTextW(
            status_,
            Widen("Pasted " + std::to_string(changed) + " keys onto " + boneName + ".").c_str());
    }

    void PopulateDisplayNameField() {
        if (displayNameEdit_ == nullptr) {
            return;
        }
        std::string name{};
        if (editableSculpt_.has_value() && !editableSculptPath_.empty()) {
            name = editableSculpt_->displayName;
        } else if (editableModel_.has_value() && !editableModelPath_.empty()) {
            name = editableModel_->displayName;
        } else if (editableAnim_.has_value() && !editableAnimPath_.empty()) {
            name = editableAnim_->displayName;
        } else if (editableRig_.has_value() && !editableRigPath_.empty()) {
            name = editableRig_->displayName;
        }
        SetWindowTextW(displayNameEdit_, Widen(name).c_str());
        const bool canRename = !name.empty()
            || editableSculpt_.has_value()
            || editableModel_.has_value()
            || editableAnim_.has_value()
            || editableRig_.has_value();
        EnableWindow(displayNameEdit_, canRename ? TRUE : FALSE);
        EnableWindow(renameDisplayButton_, canRename ? TRUE : FALSE);
    }

    void RenameSelectedDisplayName() {
        std::string name = ReadControlTextUtf8(displayNameEdit_);
        while (!name.empty() && (name.front() == ' ' || name.front() == '\t')) {
            name.erase(name.begin());
        }
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
            name.pop_back();
        }
        if (name.empty()) {
            MessageBoxW(hwnd_, L"Enter a display name.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        if (editableSculpt_.has_value() && !editableSculptPath_.empty()) {
            editableSculpt_->displayName = name;
            if (!SaveActiveSculpt()) {
                return;
            }
            RefreshCatalog(editableSculptPath_);
        } else if (editableModel_.has_value() && !editableModelPath_.empty()) {
            editableModel_->displayName = name;
            if (!ri::content::SavePrimitiveModelDocument(editableModelPath_, *editableModel_)) {
                MessageBoxW(hwnd_, L"Could not save stock name.", L"Forge", MB_OK | MB_ICONERROR);
                return;
            }
            StampWriteTime(editableModelPath_, editableModelWriteTime_, editableModelHasWriteTime_);
            RefreshCatalog(editableModelPath_);
        } else if (editableAnim_.has_value() && !editableAnimPath_.empty()) {
            editableAnim_->displayName = name;
            if (!ri::content::SaveNativeAnimationDocument(editableAnimPath_, *editableAnim_)) {
                MessageBoxW(hwnd_, L"Could not save clip name.", L"Forge", MB_OK | MB_ICONERROR);
                return;
            }
            RefreshCatalog(editableAnimPath_);
        } else if (editableRig_.has_value() && !editableRigPath_.empty()) {
            editableRig_->displayName = name;
            if (!SaveEditableRig(true, false)) {
                return;
            }
            RefreshCatalog(editableRigPath_);
        } else {
            MessageBoxW(hwnd_, L"Open a native asset before renaming.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        SetWindowTextW(status_, Widen("Renamed to \"" + name + "\".").c_str());
    }

    void AlignSelectedClipStart() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(hwnd_, L"Open a motion clip before aligning.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        if (!ri::scene::AlignNativeAnimationStart(*editableAnim_)) {
            SetWindowTextW(status_, L"No keys or events to align.");
            return;
        }
        const double time = std::min(ReadCurrentAnimTime(), editableAnim_->durationSeconds);
        if (!ReloadBoundClipAfterEdit(time)) {
            return;
        }
        SyncAnimWindowFromClip();
        PopulateEventList();
        SetWindowTextW(status_, L"Aligned clip so the first key/event is at 0s.");
    }

    void FitSelectedClipDuration() {
        if (!editableAnim_.has_value()) {
            MessageBoxW(hwnd_, L"Open a motion clip before fitting duration.", L"Forge", MB_OK | MB_ICONWARNING);
            return;
        }
        if (!ri::scene::FitNativeAnimationDuration(*editableAnim_)) {
            SetWindowTextW(status_, L"No keys or events to fit.");
            return;
        }
        const double time = std::min(ReadCurrentAnimTime(), editableAnim_->durationSeconds);
        if (!ReloadBoundClipAfterEdit(time)) {
            return;
        }
        SyncAnimWindowFromClip();
        PopulateEventList();
        SetWindowTextW(
            status_,
            Widen("Fit duration to " + std::to_string(editableAnim_->durationSeconds) + "s.").c_str());
    }

    void DuplicateSelectedRig() {
        fs::path rigPath = editableRigPath_;
        if (rigPath.empty()) {
            if (const ri::forge::AssetEntry* asset = SelectedAsset();
                asset != nullptr && asset->kind == ri::forge::AssetKind::Rig) {
                rigPath = asset->absolutePath;
            }
        }
        if (rigPath.empty()) {
            MessageBoxW(hwnd_, L"Select a rig to duplicate.", L"Forge rig", MB_OK | MB_ICONWARNING);
            return;
        }
        std::string error;
        const fs::path output = ri::forge::DuplicateRig(workspaceRoot_, rigPath, &error);
        if (output.empty()) {
            MessageBoxW(
                hwnd_,
                error.empty() ? L"Could not duplicate the rig." : Widen(error).c_str(),
                L"Forge rig",
                MB_OK | MB_ICONERROR);
            return;
        }
        SetWindowTextW(status_, Widen("Duplicated rig as " + output.filename().string()).c_str());
        SetForgeBay(kBayMotion);
        RefreshCatalog(output);
    }

    void DeleteSelectedRig() {
        fs::path rigPath = editableRigPath_;
        if (rigPath.empty()) {
            if (const ri::forge::AssetEntry* asset = SelectedAsset();
                asset != nullptr && asset->kind == ri::forge::AssetKind::Rig) {
                rigPath = asset->absolutePath;
            }
        }
        if (rigPath.empty()) {
            MessageBoxW(hwnd_, L"Select a rig to delete.", L"Forge rig", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::wstring prompt =
            L"Delete rig\n" + Widen(rigPath.filename().string())
            + L"?\nSidecar clay/stock/clips keep their paths until rebound.";
        if (MessageBoxW(hwnd_, prompt.c_str(), L"Forge rig", MB_YESNO | MB_ICONWARNING) != IDYES) {
            return;
        }
        const bool clearingLive = editableRigPath_ == rigPath;
        std::string error;
        if (!ri::forge::DeleteRig(rigPath, &error)) {
            MessageBoxW(
                hwnd_,
                error.empty() ? L"Could not delete the rig." : Widen(error).c_str(),
                L"Forge rig",
                MB_OK | MB_ICONERROR);
            return;
        }
        if (clearingLive) {
            editableRig_.reset();
            editableRigPath_.clear();
        }
        if (lastRigPath_ == rigPath) {
            lastRigPath_.clear();
        }
        SetWindowTextW(status_, Widen("Deleted " + rigPath.filename().string()).c_str());
        RefreshCatalog({});
        SyncAuthoringEnable();
        Rebuild3DPreview(SelectedAsset(), true);
    }

    void DuplicateSelectedModel() {
        fs::path modelPath = editableModelPath_;
        if (modelPath.empty()) {
            if (const ri::forge::AssetEntry* asset = SelectedAsset();
                asset != nullptr && asset->kind == ri::forge::AssetKind::PrimitiveModel) {
                modelPath = asset->absolutePath;
            }
        }
        if (modelPath.empty()) {
            MessageBoxW(hwnd_, L"Select stock to duplicate.", L"Forge stock", MB_OK | MB_ICONWARNING);
            return;
        }
        std::string error;
        const fs::path output =
            ri::forge::DuplicatePrimitiveModel(workspaceRoot_, modelPath, &error);
        if (output.empty()) {
            MessageBoxW(
                hwnd_,
                error.empty() ? L"Could not duplicate stock." : Widen(error).c_str(),
                L"Forge stock",
                MB_OK | MB_ICONERROR);
            return;
        }
        SetWindowTextW(status_, Widen("Duplicated stock as " + output.filename().string()).c_str());
        SetForgeBay(kBayStock);
        RefreshCatalog(output);
    }

    void DeleteSelectedModel() {
        fs::path modelPath = editableModelPath_;
        if (modelPath.empty()) {
            if (const ri::forge::AssetEntry* asset = SelectedAsset();
                asset != nullptr && asset->kind == ri::forge::AssetKind::PrimitiveModel) {
                modelPath = asset->absolutePath;
            }
        }
        if (modelPath.empty()) {
            MessageBoxW(hwnd_, L"Select stock to delete.", L"Forge stock", MB_OK | MB_ICONWARNING);
            return;
        }
        const std::wstring prompt =
            L"Delete stock model\n" + Widen(modelPath.filename().string())
            + L"?\nThis cannot be undone.";
        if (MessageBoxW(hwnd_, prompt.c_str(), L"Forge stock", MB_YESNO | MB_ICONWARNING) != IDYES) {
            return;
        }
        const bool clearingLive = editableModelPath_ == modelPath;
        if (clearingLive) {
            editableModel_.reset();
            editableModelPath_.clear();
        }
        std::string error;
        if (!ri::forge::DeletePrimitiveModel(modelPath, &error)) {
            MessageBoxW(
                hwnd_,
                error.empty() ? L"Could not delete stock." : Widen(error).c_str(),
                L"Forge stock",
                MB_OK | MB_ICONERROR);
            return;
        }
        SetWindowTextW(status_, Widen("Deleted " + modelPath.filename().string()).c_str());
        RefreshCatalog({});
        SyncAuthoringEnable();
        Rebuild3DPreview(SelectedAsset(), true);
    }

    void NormalizeSelectedSculptWeights() {
        if (!editableSculpt_.has_value()) {
            MessageBoxW(
                hwnd_, L"Open clay before normalizing weights.", L"Forge weights", MB_OK | MB_ICONWARNING);
            return;
        }
        const ri::content::NativeSculptDocument previous = *editableSculpt_;
        const std::size_t changed = ri::scene::NormalizeAllNativeSculptWeights(*editableSculpt_);
        if (changed == 0U) {
            SetWindowTextW(status_, L"All vertex weights are already normalized.");
            return;
        }
        sculptUndo_.Capture(previous);
        sculptDirty_ = true;
        showWeights_ = true;
        ApplySculptDisplayMesh();
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
        SyncForgeTitle();
        if (!SaveActiveSculpt()) {
            return;
        }
        SetWindowTextW(
            status_, Widen("Normalized " + std::to_string(changed) + " vertices.").c_str());
    }

    void PruneSelectedSculptWeights() {
        if (!editableSculpt_.has_value()) {
            MessageBoxW(
                hwnd_, L"Open clay before pruning weights.", L"Forge weights", MB_OK | MB_ICONWARNING);
            return;
        }
        const ri::content::NativeSculptDocument previous = *editableSculpt_;
        const std::size_t changed = ri::scene::PruneAllNativeSculptWeights(*editableSculpt_);
        if (changed == 0U) {
            SetWindowTextW(status_, L"No weights were above the prune threshold.");
            return;
        }
        sculptUndo_.Capture(previous);
        sculptDirty_ = true;
        showWeights_ = true;
        ApplySculptDisplayMesh();
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
        SyncForgeTitle();
        if (!SaveActiveSculpt()) {
            return;
        }
        SetWindowTextW(status_, Widen("Pruned " + std::to_string(changed) + " vertices.").c_str());
    }

    void MirrorSelectedSculptWeights() {
        if (!editableSculpt_.has_value()) {
            MessageBoxW(
                hwnd_, L"Open clay before mirroring weights.", L"Forge weights", MB_OK | MB_ICONWARNING);
            return;
        }
        const ri::content::NativeSculptDocument previous = *editableSculpt_;
        const std::size_t changed = ri::scene::MirrorNativeSculptWeights(
            *editableSculpt_, true, sculptMirrorY_, sculptMirrorZ_);
        if (changed == 0U) {
            SetWindowTextW(status_, L"No mirrored weight partners were updated.");
            return;
        }
        sculptUndo_.Capture(previous);
        sculptDirty_ = true;
        showWeights_ = true;
        ApplySculptDisplayMesh();
        SyncPreviewOverlays(true);
        RefreshViewportHeading();
        SyncForgeTitle();
        if (!SaveActiveSculpt()) {
            return;
        }
        SetWindowTextW(
            status_, Widen("Mirrored weights onto " + std::to_string(changed) + " vertices.").c_str());
    }

    void NotifyClipEventsBetween(const double fromTime, const double toTime) {
        if (!editableAnim_.has_value() || editableAnim_->events.empty()) {
            return;
        }
        const double duration = ClipDurationSeconds();
        const auto fireRange = [&](const double start, const double end) {
            if (!(end > start)) {
                return;
            }
            for (const ri::content::NativeAnimationEvent& event : editableAnim_->events) {
                if (event.timeSeconds > start && event.timeSeconds <= end) {
                    SetWindowTextW(
                        status_,
                        (L"Event @" + FormatTransformValue(static_cast<float>(event.timeSeconds)) + L"s: "
                            + Widen(event.name))
                            .c_str());
                }
            }
        };
        if (toTime >= fromTime) {
            fireRange(fromTime, toTime);
        } else {
            fireRange(fromTime, duration);
            fireRange(0.0, toTime);
        }
    }

    void ValidateSelectedAsset() const {
        const ri::forge::AssetEntry* asset = SelectedAsset();
        if (asset == nullptr) {
            return;
        }
        bool valid = asset->valid;
        std::string summary = asset->summary;
        const wchar_t* title = valid ? L"Validation passed" : L"Validation failed";
        if (asset->kind == ri::forge::AssetKind::ModelSource) {
            const ri::forge::ModelSourceValidationReport report =
                ri::forge::ValidateModelSource(asset->absolutePath);
            valid = report.valid;
            summary = report.summary;
            title = report.valid ? L"Model import validation passed"
                                 : (report.runtimeImportable ? L"Model import validation failed"
                                                             : L"Model export required");
        } else if (asset->kind == ri::forge::AssetKind::PrimitiveModel) {
            title = valid ? L"Primitive model validation passed" : L"Primitive model validation failed";
        } else if (asset->kind == ri::forge::AssetKind::Sculpt) {
            title = valid ? L"Native sculpt validation passed" : L"Native sculpt validation failed";
        } else if (asset->kind == ri::forge::AssetKind::Rig) {
            title = valid ? L"Rig validation passed" : L"Rig validation failed";
        } else if (asset->kind == ri::forge::AssetKind::Animation) {
            title = valid ? L"Clip validation passed" : L"Clip validation failed";
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

    void OpenSelectedInEditor() {
        if (!FlushLiveSculpt()) {
            MessageBoxW(
                hwnd_,
                L"Could not save the open clay before handing it to the editor.",
                L"Forge to Editor",
                MB_OK | MB_ICONWARNING);
            return;
        }
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
        return id == kNewPrimitiveModel || id == kNewSculpt || id == kNewAnimation || id == kAddPrimitive
            || id == kBakeModel || id == kBindRig || id == kOpenInEditor || id == kApplyLook || id == kAnimPlay;
    }

    void DrawOwnerDrawButton(const DRAWITEMSTRUCT& item) const {
        const bool enabled = (item.itemState & ODS_DISABLED) == 0U;
        const bool pressed = (item.itemState & ODS_SELECTED) != 0U;
        const bool bayOn = item.CtlID == static_cast<UINT>(bay_);
        RECT bounds = item.rcItem;
        FillRect(item.hDC, &bounds, bayOn ? accentBrush_ : (pressed ? listBrush_ : raisedBrush_));
        FrameRect(
            item.hDC,
            &bounds,
            IsPrimaryButton(item.CtlID) && enabled ? accentBrush_ : borderBrush_);
        if (IsPrimaryButton(item.CtlID) && enabled && !bayOn) {
            RECT marker{bounds.left + 1, bounds.top + 1, bounds.left + 3, bounds.bottom - 1};
            FillRect(item.hDC, &marker, accentBrush_);
        }

        std::array<wchar_t, 128> label{};
        GetWindowTextW(item.hwndItem, label.data(), static_cast<int>(label.size()));
        SetBkMode(item.hDC, TRANSPARENT);
        SetTextColor(item.hDC, enabled ? (bayOn ? kBackgroundColor : kTextColor) : RGB(90, 88, 78));
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

        const wchar_t* tag = L"SRC";
        HBRUSH tagBrush = modelBrush_;
        if (asset.kind == ri::forge::AssetKind::PrimitiveModel) {
            tag = L"STOCK";
            tagBrush = accentBrush_;
        } else if (asset.kind == ri::forge::AssetKind::Sculpt) {
            tag = L"CLAY";
            tagBrush = clayBrush_;
        } else if (asset.kind == ri::forge::AssetKind::Rig) {
            tag = L"RIG";
            tagBrush = rigBrush_;
        } else if (asset.kind == ri::forge::AssetKind::Animation) {
            tag = L"CLIP";
            tagBrush = motionBrush_;
        }
        if (!asset.valid) {
            tag = L"ERROR";
            tagBrush = errorBrush_;
        }
        RECT tagBounds{bounds.left + 6, bounds.top + 4, bounds.left + 54, bounds.bottom - 4};
        FillRect(item.hDC, &tagBounds, tagBrush);
        SetBkMode(item.hDC, TRANSPARENT);
        SetTextColor(item.hDC, RGB(245, 246, 248));
        SelectObject(item.hDC, sectionFont_);
        DrawTextW(
            item.hDC, tag, -1, &tagBounds,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT textBounds{bounds.left + 60, bounds.top, bounds.right - 6, bounds.bottom};
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
        } else if (item.CtlID == kTransformMode || item.CtlID == kClipList) {
            DrawTransformModeItem(item);
        }
    }

    void Paint() const {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd_, &paint);
        RECT client{};
        GetClientRect(hwnd_, &client);
        FillRect(dc, &client, backgroundBrush_);

        RECT header{0, 0, client.right, 44};
        FillRect(dc, &header, panelBrush_);
        RECT ember{0, 0, 6, 44};
        FillRect(dc, &ember, accentBrush_);
        RECT hairline{0, 43, client.right, 44};
        FillRect(dc, &hairline, borderBrush_);
        FillRect(dc, &toolbarRect_, raisedBrush_);
        RECT toolbarLine{toolbarRect_.left, toolbarRect_.bottom - 1, toolbarRect_.right, toolbarRect_.bottom};
        FillRect(dc, &toolbarLine, borderBrush_);

        for (const RECT pane : {leftPane_, centerPane_, rightPane_}) {
            RECT paneCopy = pane;
            FillRect(dc, &paneCopy, panelBrush_);
            FrameRect(dc, &paneCopy, borderBrush_);
        }
        RECT statusLine{0, client.bottom - 24, client.right, client.bottom - 23};
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
                    TickAnimation();
                }
                return 0;
            case WM_LBUTTONDOWN: {
                const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                if (!viewportStarted_ || !PtInRect(&viewportBounds_, point)) {
                    return DefWindowProcW(hwnd_, message, wParam, lParam);
                }
                if (editableSculpt_.has_value() && previewSculptNode_ != ri::scene::kInvalidHandle
                    && bay_ != kBayMotion) {
                    sculptDragging_ = true;
                    sculptWeightStroke_ = bay_ == kBayClay && (GetKeyState('B') & 0x8000) != 0;
                    lastOrbitPoint_ = point;
                    SetCapture(hwnd_);
                    ApplySculptAt(point.x, point.y);
                    return 0;
                }
                if (BeginGizmoDrag(point.x, point.y)) {
                    return 0;
                }
                if (PickViewportAuthoringTarget(point.x, point.y)) {
                    return 0;
                }
                return DefWindowProcW(hwnd_, message, wParam, lParam);
            }
            case WM_LBUTTONUP:
                if (gizmoDragging_) {
                    EndGizmoDrag();
                    if (GetCapture() == hwnd_ && !orbitDragging_ && !panDragging_) {
                        ReleaseCapture();
                    }
                    return 0;
                }
                if (sculptDragging_) {
                    sculptDragging_ = false;
                    if (sculptDirty_) {
                        SaveActiveSculpt();
                    }
                    sculptStrokeCaptured_ = false;
                    sculptWeightStroke_ = false;
                    if (GetCapture() == hwnd_ && !orbitDragging_ && !panDragging_) {
                        ReleaseCapture();
                    }
                    return 0;
                }
                return DefWindowProcW(hwnd_, message, wParam, lParam);
            case WM_RBUTTONDOWN: {
                const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                if (!viewportStarted_ || !PtInRect(&viewportBounds_, point)) {
                    return DefWindowProcW(hwnd_, message, wParam, lParam);
                }
                lastOrbitPoint_ = point;
                panDragging_ = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                orbitDragging_ = !panDragging_;
                SetCapture(hwnd_);
                return 0;
            }
            case WM_RBUTTONUP:
                orbitDragging_ = false;
                panDragging_ = false;
                if (GetCapture() == hwnd_) {
                    ReleaseCapture();
                }
                return 0;
            case WM_MBUTTONDOWN: {
                const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                if (!viewportStarted_ || !PtInRect(&viewportBounds_, point)) {
                    return DefWindowProcW(hwnd_, message, wParam, lParam);
                }
                panDragging_ = true;
                orbitDragging_ = false;
                lastOrbitPoint_ = point;
                SetCapture(hwnd_);
                return 0;
            }
            case WM_MBUTTONUP:
                panDragging_ = false;
                if (GetCapture() == hwnd_) {
                    ReleaseCapture();
                }
                return 0;
            case WM_CAPTURECHANGED:
                EndGizmoDrag();
                if (sculptDragging_ && sculptDirty_) {
                    SaveActiveSculpt();
                }
                orbitDragging_ = false;
                panDragging_ = false;
                sculptDragging_ = false;
                sculptStrokeCaptured_ = false;
                sculptWeightStroke_ = false;
                return 0;
            case WM_MOUSEMOVE: {
                const int mouseX = GET_X_LPARAM(lParam);
                const int mouseY = GET_Y_LPARAM(lParam);
                if (gizmoDragging_) {
                    DragGizmo(mouseX, mouseY);
                    return 0;
                }
                if (sculptDragging_) {
                    ApplySculptAt(mouseX, mouseY);
                    return 0;
                }
                Orbit3DPreview(mouseX, mouseY);
                Pan3DPreview(mouseX, mouseY);
                if (!orbitDragging_ && !panDragging_ && editableSculpt_.has_value()
                    && bay_ != kBayMotion) {
                    const POINT point{mouseX, mouseY};
                    if (viewportStarted_ && PtInRect(&viewportBounds_, point)) {
                        ApplySculptAt(mouseX, mouseY);
                    }
                }
                return 0;
            }
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
                    if (editableSculpt_.has_value()) {
                        SaveActiveSculpt();
                        return 0;
                    }
                    if (IsWindowEnabled(applyTransformButton_)) {
                        ApplySelectedTransform();
                    }
                    return 0;
                }
                if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && !FocusIsTextEntry()) {
                    if (wParam == 'Z') {
                        UndoSculptStroke();
                        return 0;
                    }
                    if (wParam == 'Y') {
                        RedoSculptStroke();
                        return 0;
                    }
                }
                if (!FocusIsTextEntry()) {
                    if (wParam == 'F') {
                        Frame3DPreview();
                        return 0;
                    }
                    if (wParam == 'G') {
                        TogglePreviewGrid();
                        return 0;
                    }
                    if (wParam == 'A') {
                        TogglePreviewAxes();
                        return 0;
                    }
                    if (wParam == 'W') {
                        TogglePreviewWireframe();
                        return 0;
                    }
                    if (wParam == 'H') {
                        TogglePreviewWeights();
                        return 0;
                    }
                    if ((GetKeyState(VK_CONTROL) & 0x8000) == 0) {
                        if (wParam == 'N') {
                            TogglePreviewNormals();
                            return 0;
                        }
                        if (wParam == 'C') {
                            TogglePreviewCollision();
                            return 0;
                        }
                        if (wParam == 'X') {
                            ToggleSculptMirror('x');
                            return 0;
                        }
                        if (wParam == 'Y') {
                            ToggleSculptMirror('y');
                            return 0;
                        }
                        if (wParam == 'Z') {
                            ToggleSculptMirror('z');
                            return 0;
                        }
                    }
                    if (wParam == '9' || wParam == VK_NEXT) {
                        ChangeSculptDensity(-1);
                        return 0;
                    }
                    if (wParam == '0' || wParam == VK_PRIOR) {
                        ChangeSculptDensity(1);
                        return 0;
                    }
                    if (wParam == VK_OEM_4) {
                        sculptRadius_ = std::clamp(sculptRadius_ * 0.85F, 0.04F, 1.5F);
                        SetWindowTextW(status_, Widen("Brush radius " + std::to_string(sculptRadius_)).c_str());
                        return 0;
                    }
                    if (wParam == VK_OEM_6) {
                        sculptRadius_ = std::clamp(sculptRadius_ * 1.15F, 0.04F, 1.5F);
                        SetWindowTextW(status_, Widen("Brush radius " + std::to_string(sculptRadius_)).c_str());
                        return 0;
                    }
                    if (wParam == VK_OEM_MINUS) {
                        sculptStrength_ = std::clamp(sculptStrength_ * 0.85F, 0.01F, 0.35F);
                        SetWindowTextW(status_, Widen("Brush strength " + std::to_string(sculptStrength_)).c_str());
                        return 0;
                    }
                    if (wParam == VK_OEM_PLUS) {
                        sculptStrength_ = std::clamp(sculptStrength_ * 1.15F, 0.01F, 0.35F);
                        SetWindowTextW(status_, Widen("Brush strength " + std::to_string(sculptStrength_)).c_str());
                        return 0;
                    }
                }
                return DefWindowProcW(hwnd_, message, wParam, lParam);
            case WM_GETMINMAXINFO: {
                auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
                info->ptMinTrackSize.x = 1100;
                info->ptMinTrackSize.y = 640;
                return 0;
            }
            case WM_HSCROLL:
                if (reinterpret_cast<HWND>(lParam) == animScrub_) {
                    ScrubClipFromTrackbar();
                    return 0;
                }
                break;
            case WM_COMMAND: {
                const int id = LOWORD(wParam);
                const int notification = HIWORD(wParam);
                if (id == kRefresh) {
                    RefreshCatalog();
                } else if (id == kNewPrimitiveModel) {
                    CreatePrimitiveModel();
                } else if (id == kDuplicateModel) {
                    DuplicateSelectedModel();
                } else if (id == kDeleteModel) {
                    DeleteSelectedModel();
                } else if (id == kNewSculpt) {
                    CreateNativeSculpt();
                } else if (id == kDuplicateSculpt) {
                    DuplicateSelectedSculpt();
                } else if (id == kDeleteSculpt) {
                    DeleteSelectedSculpt();
                } else if (id == kNewHumanoid) {
                    CreateHumanoidRig();
                } else if (id == kDuplicateRig) {
                    DuplicateSelectedRig();
                } else if (id == kDeleteRig) {
                    DeleteSelectedRig();
                } else if (id == kNewAnimation) {
                    CreateNativeAnimation();
                } else if (id == kBayStock || id == kBayClay || id == kBayLook || id == kBayMotion) {
                    SetForgeBay(id);
                } else if (id == kApplyLook) {
                    ApplySelectedLook();
                } else if (id == kAnimPlay) {
                    PlaySelectedClip();
                } else if (id == kAnimStop) {
                    StopSelectedClip();
                } else if (id == kAnimLoop) {
                    ToggleAnimLoop();
                } else if (id == kAnimRootMotion) {
                    ToggleRootMotion();
                } else if (id == kAnimKey) {
                    KeyCurrentPose();
                } else if (id == kAnimKeyBone) {
                    KeySelectedBone();
                } else if (id == kDeleteAnimKey) {
                    DeleteSelectedBoneKey();
                } else if (id == kClearAnimTrack) {
                    ClearSelectedAnimTrack();
                } else if (id == kClearAnimKeys) {
                    ClearAllAnimKeys();
                } else if (id == kDedupAnimKeys) {
                    DeduplicateAnimKeys();
                } else if (id == kQuantizeAnimKeys) {
                    QuantizeAnimKeys();
                } else if (id == kStripAnimTracks) {
                    StripMissingAnimTracks();
                } else if (id == kDuplicateClip) {
                    DuplicateSelectedClip();
                } else if (id == kAnimTrim) {
                    TrimSelectedClip();
                } else if (id == kAnimStampIn) {
                    StampAnimWindowFromPlayhead(false);
                } else if (id == kAnimStampOut) {
                    StampAnimWindowFromPlayhead(true);
                } else if (id == kAnimAddEvent) {
                    AddEventAtPlayhead();
                } else if (id == kAnimDelEvent) {
                    DeleteSelectedEvent();
                } else if (id == kAnimRenameEvent) {
                    RenameSelectedEvent();
                } else if (id == kAnimEventToTime) {
                    MoveSelectedEventToPlayhead();
                } else if (id == kAnimDupEvent) {
                    DuplicateSelectedEventAtPlayhead();
                } else if (id == kClearAnimEvents) {
                    ClearAllAnimEvents();
                } else if (id == kAnimPrevEvent) {
                    StepSelectedEvent(true);
                } else if (id == kAnimNextEvent) {
                    StepSelectedEvent(false);
                } else if (id == kAnimEventList && notification == LBN_SELCHANGE) {
                    JumpToSelectedEvent();
                } else if (id == kAddPrimitive) {
                    ShowPrimitiveMenuAndAdd();
                } else if (id == kAddGroup) {
                    AddGroupToSelectedModel();
                } else if (id == kDuplicatePart) {
                    DuplicateSelectedStockElement();
                } else if (id == kDeletePart) {
                    DeleteSelectedStockElement();
                } else if (id == kBakeModel) {
                    BakeSelectedModel();
                } else if (id == kBindRig) {
                    BindSelectedRig();
                } else if (id == kBindBone) {
                    BindSelectedBone();
                } else if (id == kFloodBone) {
                    FloodSelectedBone();
                } else if (id == kFloodUnbound) {
                    FloodUnboundSelectedBone();
                } else if (id == kTransferWeights) {
                    TransferSelectedBoneWeights();
                } else if (id == kSwapWeights) {
                    SwapSelectedBoneWeights();
                } else if (id == kNormWeights) {
                    NormalizeSelectedSculptWeights();
                } else if (id == kPruneWeights) {
                    PruneSelectedSculptWeights();
                } else if (id == kMirrorWeights) {
                    MirrorSelectedSculptWeights();
                } else if (id == kMirrorRest) {
                    MirrorSelectedBoneRest();
                } else if (id == kBoneRename) {
                    RenameSelectedBone();
                } else if (id == kAddChildBone) {
                    AddChildToSelectedBone();
                } else if (id == kDeleteBone) {
                    DeleteSelectedBone();
                } else if (id == kReparentBone) {
                    ReparentSelectedBone();
                } else if (id == kAddSlotBone) {
                    AddSelectedHumanoidSlot();
                } else if (id == kAuditWeights) {
                    AuditSelectedSculptWeights();
                } else if (id == kSmoothWeights) {
                    SoftenSelectedSculptWeights();
                } else if (id == kInvertWeights) {
                    InvertSelectedBoneWeights();
                } else if (id == kHalveBoneWeights) {
                    ScaleSelectedBoneWeights(0.5f);
                } else if (id == kDoubleBoneWeights) {
                    ScaleSelectedBoneWeights(2.0f);
                } else if (id == kClearWeights) {
                    ClearSelectedSculptWeights();
                } else if (id == kUnbindRig) {
                    UnbindSelectedTarget();
                } else if (id == kAnimPrevKey) {
                    JumpToNearestAnimKey(true);
                } else if (id == kAnimNextKey) {
                    JumpToNearestAnimKey(false);
                } else if (id == kSnapAnimKey) {
                    SnapToClosestAnimKey();
                } else if (id == kResetBone) {
                    ResetSelectedBonePose();
                } else if (id == kResetPose) {
                    ResetAllBonePoses();
                } else if (id == kMirrorPose) {
                    MirrorSelectedBonePose();
                } else if (id == kDeleteClip) {
                    DeleteSelectedClip();
                } else if (id == kRestAnimKey) {
                    KeyRestAtPlayhead();
                } else if (id == kMirrorAnimKeys) {
                    MirrorSelectedAnimKeys();
                } else if (id == kAnimHalfSpeed) {
                    ScaleSelectedClipTime(0.5);
                } else if (id == kAnimDoubleSpeed) {
                    ScaleSelectedClipTime(2.0);
                } else if (id == kAnimNudgeBack) {
                    NudgeSelectedClipTime(-0.1);
                } else if (id == kAnimNudgeForward) {
                    NudgeSelectedClipTime(0.1);
                } else if (id == kCopyAnimTrack) {
                    CopySelectedAnimTrack();
                } else if (id == kPasteAnimTrack) {
                    PasteSelectedAnimTrack();
                } else if (id == kRenameDisplay) {
                    RenameSelectedDisplayName();
                } else if (id == kAnimAlignStart) {
                    AlignSelectedClipStart();
                } else if (id == kAnimFitDuration) {
                    FitSelectedClipDuration();
                } else if (id == kAnimPlayheadToZero) {
                    ShiftClipPlayheadToZero();
                } else if (id == kApplyTransform) {
                    if (IsWindowEnabled(applyTransformButton_)) {
                        ApplySelectedTransform();
                    }
                } else if (id == kFocusAssetFilter) {
                    SetFocus(assetFilter_);
                    SendMessageW(assetFilter_, EM_SETSEL, 0, -1);
                } else if (id == kModelElement && notification == LBN_SELCHANGE) {
                    PopulateTransformFields();
                    SyncTransformGizmo();
                    PublishPreview();
                } else if (id == kBoneList && notification == LBN_SELCHANGE) {
                    PopulateBonePoseFields();
                    SyncAuthoringEnable();
                    SyncTransformGizmo();
                    ApplySculptDisplayMesh();
                    SyncPreviewOverlays(true);
                } else if (id == kClipList && notification == CBN_SELCHANGE) {
                    OnClipListSelChange();
                } else if (id == kTransformMode && notification == CBN_SELCHANGE) {
                    PopulateTransformFields();
                } else if (id == kAnimTime && notification == EN_KILLFOCUS) {
                    ApplyAnimTimeFromEdit();
                } else if (id == kAnimDuration && notification == EN_KILLFOCUS) {
                    ApplyAnimDurationFromEdit();
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
                    item->itemHeight = 24;
                    return TRUE;
                }
                if (item->CtlID == kModelElement) {
                    item->itemHeight = 22;
                    return TRUE;
                }
                if (item->CtlID == kTransformMode || item->CtlID == kClipList) {
                    item->itemHeight = 20;
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
                           || control == lookHeading_ || control == motionHeading_
                           || control == clipHeading_ || control == detailHeading_ || control == xLabel_
                           || control == yLabel_ || control == zLabel_
                           || control == albedoLabel_ || control == roughnessLabel_
                           || control == metallicLabel_                            || control == textureLabel_
                           || control == animTimeLabel_ || control == animDurationLabel_
                           || control == animEventLabel_) {
                    SetTextColor(dc, control == xLabel_ || control == yLabel_ || control == zLabel_
                                         ? kAccentColor
                                         : RGB(168, 164, 148));
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
            case WM_CLOSE:
                if (!FlushLiveSculpt()) {
                    const int choice = MessageBoxW(
                        hwnd_,
                        L"Clay could not be saved. Close Forge anyway and lose unsaved strokes?",
                        L"Forge clay",
                        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
                    if (choice != IDYES) {
                        return 0;
                    }
                    sculptDirty_ = false;
                    SyncForgeTitle();
                }
                DestroyWindow(hwnd_);
                return 0;
            case WM_DESTROY:
                KillTimer(hwnd_, kCatalogPollTimer);
                (void)FlushLiveSculpt();
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
                if (clayBrush_ != nullptr) {
                    DeleteObject(clayBrush_);
                }
                if (rigBrush_ != nullptr) {
                    DeleteObject(rigBrush_);
                }
                if (motionBrush_ != nullptr) {
                    DeleteObject(motionBrush_);
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
    HWND lookHeading_ = nullptr;
    HWND motionHeading_ = nullptr;
    HWND clipHeading_ = nullptr;
    HWND refreshButton_ = nullptr;
    HWND newModelButton_ = nullptr;
    HWND duplicateModelButton_ = nullptr;
    HWND deleteModelButton_ = nullptr;
    HWND newSculptButton_ = nullptr;
    HWND duplicateSculptButton_ = nullptr;
    HWND deleteSculptButton_ = nullptr;
    HWND newRigButton_ = nullptr;
    HWND duplicateRigButton_ = nullptr;
    HWND deleteRigButton_ = nullptr;
    HWND newAnimButton_ = nullptr;
    HWND bayStock_ = nullptr;
    HWND bayClay_ = nullptr;
    HWND bayLook_ = nullptr;
    HWND bayMotion_ = nullptr;
    HWND validateButton_ = nullptr;
    HWND openButton_ = nullptr;
    HWND editorButton_ = nullptr;
    HWND addPrimitiveButton_ = nullptr;
    HWND addGroupButton_ = nullptr;
    HWND duplicatePartButton_ = nullptr;
    HWND deletePartButton_ = nullptr;
    HWND bakeButton_ = nullptr;
    HWND bindRigButton_ = nullptr;
    HWND unbindRigButton_ = nullptr;
    HWND bindBoneButton_ = nullptr;
    HWND floodBoneButton_ = nullptr;
    HWND floodUnboundButton_ = nullptr;
    HWND transferWeightsButton_ = nullptr;
    HWND swapWeightsButton_ = nullptr;
    HWND normWeightsButton_ = nullptr;
    HWND pruneWeightsButton_ = nullptr;
    HWND mirrorWeightsButton_ = nullptr;
    HWND mirrorRestButton_ = nullptr;
    HWND boneRenameLabel_ = nullptr;
    HWND boneRenameEdit_ = nullptr;
    HWND boneRenameButton_ = nullptr;
    HWND addChildBoneButton_ = nullptr;
    HWND deleteBoneButton_ = nullptr;
    HWND boneParentLabel_ = nullptr;
    HWND boneParentCombo_ = nullptr;
    HWND reparentBoneButton_ = nullptr;
    HWND addSlotBoneButton_ = nullptr;
    std::vector<std::string> boneParentNames_{};
    std::vector<std::optional<std::string>> listedBoneMissingSlots_{};
    std::string pendingBoneSelect_{};
    HWND auditWeightsButton_ = nullptr;
    HWND invertWeightsButton_ = nullptr;
    HWND softWeightsButton_ = nullptr;
    HWND halveWeightsButton_ = nullptr;
    HWND doubleWeightsButton_ = nullptr;
    HWND clearWeightsButton_ = nullptr;
    HWND modelElement_ = nullptr;
    HWND transformMode_ = nullptr;
    HWND transformX_ = nullptr;
    HWND transformY_ = nullptr;
    HWND transformZ_ = nullptr;
    HWND xLabel_ = nullptr;
    HWND yLabel_ = nullptr;
    HWND zLabel_ = nullptr;
    HWND applyTransformButton_ = nullptr;
    HWND albedoLabel_ = nullptr;
    HWND roughnessLabel_ = nullptr;
    HWND metallicLabel_ = nullptr;
    HWND textureLabel_ = nullptr;
    HWND albedoR_ = nullptr;
    HWND albedoG_ = nullptr;
    HWND albedoB_ = nullptr;
    HWND roughnessEdit_ = nullptr;
    HWND metallicEdit_ = nullptr;
    HWND albedoTexture_ = nullptr;
    HWND applyLookButton_ = nullptr;
    HWND boneList_ = nullptr;
    HWND clipList_ = nullptr;
    HWND duplicateClipButton_ = nullptr;
    HWND deleteClipButton_ = nullptr;
    HWND animTimeLabel_ = nullptr;
    HWND animTime_ = nullptr;
    HWND animDurationLabel_ = nullptr;
    HWND animDuration_ = nullptr;
    HWND animInButton_ = nullptr;
    HWND animOutButton_ = nullptr;
    HWND animIn_ = nullptr;
    HWND animOut_ = nullptr;
    HWND animEventLabel_ = nullptr;
    HWND animEventName_ = nullptr;
    HWND animAddEventButton_ = nullptr;
    HWND animDelEventButton_ = nullptr;
    HWND animRenameEventButton_ = nullptr;
    HWND animEventToTimeButton_ = nullptr;
    HWND animDupEventButton_ = nullptr;
    HWND animClearEventsButton_ = nullptr;
    HWND animPrevEventButton_ = nullptr;
    HWND animNextEventButton_ = nullptr;
    HWND animEventList_ = nullptr;
    HWND animPlayButton_ = nullptr;
    HWND animStopButton_ = nullptr;
    HWND animLoopButton_ = nullptr;
    HWND animRootButton_ = nullptr;
    HWND animKeyBoneButton_ = nullptr;
    HWND animKeyButton_ = nullptr;
    HWND animDeleteKeyButton_ = nullptr;
    HWND animClearTrackButton_ = nullptr;
    HWND animClearKeysButton_ = nullptr;
    HWND animDedupKeysButton_ = nullptr;
    HWND animQuantizeKeysButton_ = nullptr;
    HWND animStripTracksButton_ = nullptr;
    HWND animPrevKeyButton_ = nullptr;
    HWND animNextKeyButton_ = nullptr;
    HWND animSnapKeyButton_ = nullptr;
    HWND animResetBoneButton_ = nullptr;
    HWND animResetPoseButton_ = nullptr;
    HWND animMirrorPoseButton_ = nullptr;
    HWND animRestKeyButton_ = nullptr;
    HWND animMirrorKeysButton_ = nullptr;
    HWND animCopyTrackButton_ = nullptr;
    HWND animPasteTrackButton_ = nullptr;
    HWND animHalfSpeedButton_ = nullptr;
    HWND animDoubleSpeedButton_ = nullptr;
    HWND animNudgeBackButton_ = nullptr;
    HWND animNudgeForwardButton_ = nullptr;
    HWND animAlignStartButton_ = nullptr;
    HWND animFitDurationButton_ = nullptr;
    HWND animPlayheadZeroButton_ = nullptr;
    HWND animTrimButton_ = nullptr;
    HWND animScrub_ = nullptr;
    HWND assetFilter_ = nullptr;
    HWND assetList_ = nullptr;
    HWND detailHeading_ = nullptr;
    HWND displayNameLabel_ = nullptr;
    HWND displayNameEdit_ = nullptr;
    HWND renameDisplayButton_ = nullptr;
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
    std::vector<int> previewFrameNodes_{};
    std::vector<std::string> previewPartIds_{};
    std::vector<int> previewGroupNodes_{};
    std::vector<std::string> previewGroupIds_{};
    ri::scene::AxesHelperHandles transformGizmo_{};
    bool gizmoDragging_ = false;
    bool gizmoDirty_ = false;
    int gizmoAxis_ = -1;
    float gizmoStartValue_ = 0.0F;
    float gizmoStartParam_ = 0.0F;
    int gizmoStartMouseX_ = 0;
    std::vector<int> previewBoneNodes_{};
    std::vector<int> listedBoneNodes_{};
    int previewGridNode_ = ri::scene::kInvalidHandle;
    int previewAxesNode_ = ri::scene::kInvalidHandle;
    int previewSculptNode_ = ri::scene::kInvalidHandle;
    int previewWireframeNode_ = ri::scene::kInvalidHandle;
    int previewNormalsNode_ = ri::scene::kInvalidHandle;
    int previewCollisionNode_ = ri::scene::kInvalidHandle;
    int previewBrushCursorNode_ = ri::scene::kInvalidHandle;
    int previewWeightNode_ = ri::scene::kInvalidHandle;
    std::unordered_map<std::string, ri::math::Mat4> restBoneWorld_{};
    std::unordered_map<std::string, ri::scene::Transform> restBoneLocal_{};
    std::vector<ri::scene::BoundPrimitivePartRest> restStockParts_{};
    std::string previewStatus_{};
    std::size_t previewRenderableCount_ = 0;
    double previewElapsedMilliseconds_ = 0.0;
    bool showGrid_ = true;
    bool showAxes_ = true;
    bool showWireframe_ = false;
    bool showNormals_ = false;
    bool showCollision_ = false;
    bool showWeights_ = true;
    bool gizmoBone_ = false;
    bool sculptMirrorX_ = false;
    bool sculptMirrorY_ = false;
    bool sculptMirrorZ_ = false;
    bool orbitDragging_ = false;
    bool panDragging_ = false;
    bool sculptDragging_ = false;
    bool sculptDirty_ = false;
    bool sculptStrokeCaptured_ = false;
    bool sculptWeightStroke_ = false;
    float sculptRadius_ = 0.18F;
    float sculptStrength_ = 0.06F;
    POINT lastOrbitPoint_{};
    std::optional<ri::content::PrimitiveModelDocument> editableModel_{};
    fs::path editableModelPath_{};
    fs::file_time_type editableModelWriteTime_{};
    bool editableModelHasWriteTime_ = false;
    std::string pendingModelElementId_{};
    fs::path lastRigPath_{};
    fs::path lastBindTargetPath_{};
    ri::forge::AssetKind lastBindTargetKind_ = ri::forge::AssetKind::ModelSource;
    std::optional<ri::content::NativeSculptDocument> editableSculpt_{};
    fs::path editableSculptPath_{};
    std::optional<ri::content::NativeAnimationDocument> editableAnim_{};
    fs::path editableAnimPath_{};
    ri::content::NativeAnimationTrack copiedAnimTrack_{};
    std::optional<ri::scene::RigDefinition> editableRig_{};
    fs::path editableRigPath_{};
    ri::scene::AnimationClip boundClip_{};
    ri::scene::AnimationPlayer animPlayer_{};
    LARGE_INTEGER animQpc_{};
    bool animClockInit_ = false;
    bool animScrubSyncing_ = false;
    bool animDurationSyncing_ = false;
    bool animWindowSyncing_ = false;
    bool animEventSyncing_ = false;
    bool clipListSyncing_ = false;
    std::vector<fs::path> clipListPaths_{};
    int bay_ = kBayStock;
    ri::scene::NativeSculptUndoStack sculptUndo_{};
    std::vector<ModelElementRef> modelElements_{};
    bool previewInitialized_ = false;
    fs::path previewedAssetPath_{};
    fs::path appliedPreviewAssetPath_{};
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
    HBRUSH clayBrush_ = nullptr;
    HBRUSH rigBrush_ = nullptr;
    HBRUSH motionBrush_ = nullptr;
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
        if (commandLine.HasFlag("--create-sculpt")) {
            std::string error;
            const fs::path output = ri::forge::CreateUniqueNativeSculpt(
                workspaceRoot, commandLine.GetValue("--cage").value_or("sphere"), &error);
            if (output.empty()) {
                std::cerr << "Forge sculpt create failed: " << error << "\n";
                return 1;
            }
            std::cout << "Created native sculpt: " << output.string() << "\n";
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
        const ri::forge::AssetCatalog catalog = ri::forge::ScanAssetCatalog(workspaceRoot);
        PrintHeadlessSummary(catalog);
        return 0;
#endif
    } catch (const std::exception& exception) {
        std::cerr << "Forge failed: " << exception.what() << "\n";
        return 1;
    }
}
