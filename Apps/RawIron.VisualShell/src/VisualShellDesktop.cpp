#include "VisualShellTypes.h"
#include "ShellWorkshop.h"
#include "VisualShellUiDraw.h"
#include "VisualShellLauncherArt.h"
#include "VisualShellWorkshopLayout.h"
#include "WorkshopForgeGpu.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace {

#if defined(RAWIRON_BUILD_CONFIGURATION)
constexpr const char* kBuildConfiguration = RAWIRON_BUILD_CONFIGURATION;
#else
constexpr const char* kBuildConfiguration = "";
#endif

std::string MakeDisplaySceneId(std::string slug) {
    constexpr std::string_view kPrefixes[] = {"scene_", "misc_"};
    for (const std::string_view prefix : kPrefixes) {
        if (slug.rfind(prefix.data(), 0) == 0) {
            slug.erase(0, prefix.size());
            break;
        }
    }
    return slug;
}

namespace pal = ri::vshell::colors;
namespace wl = ri::vshell::layout;

[[nodiscard]] float WorkshopAnimationTimeSeconds() {
    static LARGE_INTEGER freq{};
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return static_cast<float>(static_cast<double>(counter.QuadPart) / static_cast<double>(freq.QuadPart));
}

[[nodiscard]] std::wstring Utf8BytesToWide(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int nbytes = static_cast<int>(utf8.size());
    const int wideChars =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), nbytes, nullptr, 0);
    if (wideChars <= 0) {
        std::wstring fallback;
        fallback.reserve(utf8.size());
        for (unsigned char ch : utf8) {
            fallback.push_back(static_cast<wchar_t>(ch));
        }
        return fallback;
    }
    std::wstring out(static_cast<std::size_t>(wideChars), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), nbytes, out.data(), wideChars);
    return out;
}

[[nodiscard]] std::wstring FormatRecentSessionLabelW(const std::vector<std::string>& recentUtf8Paths) {
    if (recentUtf8Paths.empty()) {
        return L"Recent: --";
    }
    const fs::path path(Utf8BytesToWide(recentUtf8Paths.front()));
    std::wstring line = L"Recent: " + path.filename().wstring();
    constexpr std::size_t kMaxChars = 56;
    if (line.size() > kMaxChars) {
        line.resize(kMaxChars - 3);
        line += L"...";
    }
    return line;
}

std::string ActionKindLabel(const Action& action) {
    switch (action.kind) {
        case Action::Kind::RunCaptured:
            return "Captured output";
        case Action::Kind::LaunchDetached:
            return "Launch app";
        case Action::Kind::OpenFolder:
            return "Open folder";
    }
    return "Action";
}

enum class WorkshopHitKind {
    None = 0,
    StartOrb,
    DesktopTile,
    GameOpenFolder,
    GameLaunchEditor,
    TaskConsoleAction,
    TaskRecentOpen,
    StartMenuItem,
    /// Fills the taskbar behind RI / CLI / Recent so clicks don’t fall through to widgets drawn below.
    TaskbarBackground,
};

struct WorkshopHitZone {
    RECT rect{};
    WorkshopHitKind kind = WorkshopHitKind::None;
    int payload = 0;
};

class VisualShellWindow {
public:
    static constexpr UINT_PTR kWorkshopBgAnimTimerId = 1;

    explicit VisualShellWindow(ShellState& shell)
        : shell_(shell) {}

    int Run(HINSTANCE instance) {
        const wchar_t* className = L"RawIronVisualShellWindow";

        WNDCLASSW windowClass{};
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = &VisualShellWindow::WindowProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = className;
        windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);

        RegisterClassW(&windowClass);

        hwnd_ = CreateWindowExW(
            0,
            className,
            L"Raw Iron Workspace",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1280,
            1040,
            nullptr,
            nullptr,
            instance,
            this);
        if (hwnd_ == nullptr) {
            return 1;
        }

        {
            RECT work{};
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
            constexpr int kPreferredWinW = 1280;
            constexpr int kPreferredWinH = 1040;
            constexpr int kMinUsableWinW = 980;
            constexpr int kMinUsableWinH = 680;
            const int areaW = static_cast<int>(work.right - work.left);
            const int areaH = static_cast<int>(work.bottom - work.top);
            minTrackW_ = (std::min)(kMinUsableWinW, (std::max)(1, areaW));
            minTrackH_ = (std::min)(kMinUsableWinH, (std::max)(1, areaH));
            const int winW = std::clamp(kPreferredWinW, minTrackW_, (std::max)(minTrackW_, areaW));
            const int winH = std::clamp(kPreferredWinH, minTrackH_, (std::max)(minTrackH_, areaH));
            const int px = work.left + (std::max)(0, (areaW - winW) / 2);
            const int py = work.top + (std::max)(0, (areaH - winH) / 2);
            SetWindowPos(hwnd_, HWND_TOP, px, py, winW, winH, SWP_SHOWWINDOW);
        }

        gdiplusToken_ = ri::shell::StartupGdiplus();
        {
            const fs::path shellAssets = shell_.SourceRoot() / "Assets" / "Shell";
            const fs::path bgPath = shellAssets / "background.png";
            const fs::path bgAltPath = shellAssets / "ribackground.png";
            const fs::path logoPath = shellAssets / "RILOGO.png";
            const fs::path logoAltPath = shellAssets / "RItransparent.png";
            const fs::path ambientPath = shellAssets / "background_ambient.png";
            const fs::path displacementPath = shellAssets / "background_displacement.png";
            const fs::path specularPath = shellAssets / "background_specular.png";
            const fs::path normalPrimary = shellAssets / "background_normal.png";
            const fs::path normalAlt = shellAssets / "background_normal (1).png";

            fs::path bgResolved = bgPath;
            bool bgOk = backgroundImage_.Load(bgPath);
            if (!bgOk) {
                bgResolved = bgAltPath;
                bgOk = backgroundImage_.Load(bgAltPath);
            }
            fs::path logoResolved = logoPath;
            bool logoOk = logoImage_.Load(logoPath);
            if (!logoOk) {
                logoResolved = logoAltPath;
                logoOk = logoImage_.Load(logoAltPath);
            }
            const bool ambientOk = ambientImage_.Load(ambientPath);
            const bool dispOk = displacementImage_.Load(displacementPath);
            const bool specularOk = specularImage_.Load(specularPath);
            bool normalOk = normalImage_.Load(normalPrimary);
            if (!normalOk) {
                normalOk = normalImage_.Load(normalAlt);
            }

            shell_.AppendLog(bgOk ? "Workshop background loaded (GDI+): " + Narrow(bgResolved)
                                  : "Workshop background missing; using procedural metal: " + Narrow(bgPath));
            shell_.AppendLog(logoOk ? "Workshop logo loaded (GDI+): " + Narrow(logoResolved)
                                    : "Workshop logo missing; title uses text only: " + Narrow(logoPath));
            shell_.AppendLog(ambientOk ? "Background ambient/occlusion layer: " + Narrow(ambientPath)
                                       : "Ambient layer missing (optional): " + Narrow(ambientPath));
            shell_.AppendLog(normalOk ? "Background normal map loaded for lit workshop backdrop."
                                      : "Normal map missing - flat shading until you add background_normal.png.");
            shell_.AppendLog(dispOk ? "Displacement map loaded (parallax wobble)." : "Displacement map optional; skipped.");
            shell_.AppendLog(specularOk ? "Specular map loaded for forged highlights: " + Narrow(specularPath)
                                        : "Specular map optional; skipped.");
        }

        if (ri::shell::WorkshopForgeGpuTryInit()) {
            shell_.AppendLog("Workshop forged lighting: GPU (Direct3D 11).");
        } else {
            shell_.AppendLog("Workshop forged lighting: CPU (D3D11 unavailable or failed to initialize).");
        }

        /// ~24 fps ceiling for ember/spark overlay; forged plate uses cached CPU compose between lighting keyframes.
        SetTimer(hwnd_, kWorkshopBgAnimTimerId, 41, nullptr);

        titleFont_ = CreateUiFont(-20, FW_BOLD, L"Trebuchet MS");
        menuFont_ = CreateUiFont(-16, FW_SEMIBOLD, L"Tahoma");
        bodyFont_ = CreateUiFont(-13, FW_NORMAL, L"Tahoma");
        smallFont_ = CreateUiFont(-12, FW_NORMAL, L"Tahoma");
        monoFont_ = CreateUiFont(-12, FW_NORMAL, L"Consolas");
        OnResize();

        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
        BringWindowToTop(hwnd_);
        SetForegroundWindow(hwnd_);
        SetActiveWindow(hwnd_);
        shell_.AppendLog("Shell window shown; if hidden, use Alt+Tab or the taskbar (Raw Iron Workspace).");

        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        DeleteObject(titleFont_);
        DeleteObject(menuFont_);
        DeleteObject(bodyFont_);
        DeleteObject(smallFont_);
        DeleteObject(monoFont_);

        backgroundImage_ = {};
        ambientImage_ = {};
        normalImage_ = {};
        displacementImage_ = {};
        specularImage_ = {};
        logoImage_ = {};
        ri::shell::WorkshopForgeGpuShutdown();
        ri::shell::ShutdownGdiplus(gdiplusToken_);

        return static_cast<int>(message.wParam);
    }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        VisualShellWindow* self = nullptr;
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<LPCREATESTRUCTW>(lParam);
            self = static_cast<VisualShellWindow*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->hwnd_ = hwnd;
        } else {
            self = reinterpret_cast<VisualShellWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (self == nullptr) {
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }

        switch (message) {
            case WM_KEYDOWN:
                return self->OnKeyDown(wParam);
            case WM_SIZE:
                self->OnResize();
                return 0;
            case WM_GETMINMAXINFO:
                self->OnGetMinMaxInfo(reinterpret_cast<MINMAXINFO*>(lParam));
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_LBUTTONDOWN: {
                const int x = static_cast<short>(LOWORD(lParam));
                const int y = static_cast<short>(HIWORD(lParam));
                self->OnMouseLeftDown(x, y);
                return 0;
            }
            case WM_TIMER:
                if (wParam == VisualShellWindow::kWorkshopBgAnimTimerId) {
                    if (!IsIconic(hwnd) && IsWindowVisible(hwnd)) {
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                }
                return 0;
            case WM_PAINT:
                self->Paint();
                return 0;
            case WM_DESTROY:
                KillTimer(hwnd, VisualShellWindow::kWorkshopBgAnimTimerId);
                PostQuitMessage(0);
                return 0;
            default:
                return DefWindowProcW(hwnd, message, wParam, lParam);
        }
    }

    LRESULT OnKeyDown(WPARAM key) {
        switch (key) {
            case VK_UP:
                shell_.MoveSelection(-1);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            case VK_DOWN:
                shell_.MoveSelection(1);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            case VK_LEFT:
                shell_.CycleExample(-1);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            case VK_RIGHT:
                shell_.CycleExample(1);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            case VK_HOME:
                while (shell_.SelectedIndex() > 0) {
                    shell_.MoveSelection(-1);
                }
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            case VK_END:
                while (shell_.SelectedIndex() + 1 < shell_.Actions().size()) {
                    shell_.MoveSelection(1);
                }
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            case VK_RETURN:
            case VK_F5:
                shell_.LaunchSelected(hwnd_);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            case VK_ESCAPE:
                DestroyWindow(hwnd_);
                return 0;
            default:
                return 0;
        }
    }

    void OnResize() {
        if (hwnd_ == nullptr) {
            return;
        }
        InvalidateRect(hwnd_, nullptr, TRUE);
    }

    void OnGetMinMaxInfo(MINMAXINFO* minMaxInfo) {
        if (minMaxInfo == nullptr) {
            return;
        }
        minMaxInfo->ptMinTrackSize.x = (std::max)(1, minTrackW_);
        minMaxInfo->ptMinTrackSize.y = (std::max)(1, minTrackH_);
    }

    void ClearHitZones() {
        hitZones_.clear();
    }

    void PushHitZone(const RECT& rect, WorkshopHitKind kind, int payload = 0) {
        if (RectWidth(rect) <= 0 || RectHeight(rect) <= 0) {
            return;
        }
        hitZones_.push_back(WorkshopHitZone{rect, kind, payload});
    }

    RECT DrawSourceToolWindowChrome(HDC dc, const RECT& outer, const char* title) {
        DrawPanelDropShadow(dc, outer, 1);

        RECT titleBar{outer.left, outer.top, outer.right, outer.top + wl::kWidgetTitleBarH};
        RECT client{outer.left, titleBar.bottom, outer.right, outer.bottom};

        FillRectColor(dc, outer, pal::kShadow);
        DrawEdgeLines(dc, outer, pal::kHighlight, pal::kDarkShadow);

        FillRectVerticalGradient(dc, titleBar, pal::kSourceTitleBarBg, pal::kTitleBar);
        RECT titleStripe{titleBar.left, titleBar.top, titleBar.right, titleBar.top + 2};
        FillRectColor(dc, titleStripe, pal::kAccentEmber);
        RECT titleLeftRail{titleBar.left, titleBar.top, titleBar.left + 4, titleBar.bottom};
        FillRectColor(dc, titleLeftRail, RGB(92, 64, 34));
        DrawEdgeLines(dc, titleBar, pal::kHighlight, pal::kDarkShadow);
        DrawTextLine(dc,
                     InsetRectBy(titleBar, 12, 3),
                     title,
                     pal::kTitleBarText,
                     monoFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

        FillRectVerticalGradient(dc, client, pal::kPanel, pal::kInset);
        DrawEdgeLines(dc, client, pal::kDarkShadow, pal::kHighlight);

        RECT inner = InsetRectBy(client, 2, 2);
        FillRectVerticalGradient(dc, inner, pal::kInset, pal::kMetalSunkenTop);
        DrawEdgeLines(dc, inner, pal::kDarkShadow, pal::kLightShadow);
        return inner;
    }

    void DrawDesktopShortcutTile(HDC dc,
                                 const wl::DesktopIconLayout& cell,
                                 const char* label,
                                 int iconIndex,
                                 bool ready) {
        const RECT& tile = cell.hitRect;
        RECT shadow = tile;
        OffsetRect(&shadow, 2, 2);
        FillRectColor(dc, shadow, RGB(5, 6, 7));

        FillRectVerticalGradient(dc, tile, pal::kDesktopShortcutPaperTop, pal::kDesktopShortcutPaperBottom);
        RECT topWear{tile.left + 1, tile.top + 1, tile.right - 1, tile.top + 3};
        FillRectColor(dc, topWear, ready ? pal::kAccentEmber : RGB(82, 78, 68));
        DrawEdgeLines(dc, tile, pal::kHighlight, pal::kDarkShadow);

        FillLauncherIconWell(dc, cell.iconRect, iconIndex, ready);
        DrawLauncherGlyph(dc, cell.iconRect, iconIndex, ready);

        RECT lamp{tile.right - 11, tile.top + 8, tile.right - 6, tile.top + 13};
        FillRectColor(dc, lamp, ready ? RGB(115, 164, 98) : RGB(91, 54, 42));
        DrawEdgeLines(dc, lamp, RGB(160, 154, 128), RGB(9, 10, 11));

        RECT labelPlate{cell.labelRect.left - 1,
                        cell.labelRect.top - 1,
                        cell.labelRect.right + 1,
                        cell.labelRect.bottom + 1};
        FillRectVerticalGradient(dc, labelPlate, RGB(31, 33, 35), RGB(17, 19, 21));
        DrawEdgeLines(dc, labelPlate, RGB(75, 75, 69), RGB(6, 7, 8));
        DrawTextLine(dc,
                     cell.labelRect,
                     label,
                     pal::kText,
                     smallFont_,
                     DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
    }

    void DrawDesktopIconGrid(HDC dc, const wl::WorkshopFrameLayout& lay) {
        const int clipSave = SaveDC(dc);
        IntersectClipRect(dc,
                          lay.desktopIconsArea.left,
                          lay.desktopIconsArea.top,
                          lay.desktopIconsArea.right,
                          lay.desktopIconsArea.bottom);

        DrawTextLine(dc,
                     RECT{lay.desktopIconsArea.left + 6,
                          lay.desktopIconsArea.top + 6,
                          lay.desktopIconsArea.right - 6,
                          lay.desktopIconsArea.top + 24},
                     "LAUNCHERS",
                     pal::kToolbarSpacerText,
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        for (int i = 0; i < wl::kDesktopIconCount; ++i) {
            const auto& cell = lay.desktopIcons[static_cast<std::size_t>(i)];
            const auto idx =
                FindActionIndex(shell_.Actions(), wl::kWorkshopTiles[static_cast<std::size_t>(i)].actionTitleSubstring);
            const bool ready = idx.has_value() && TargetReady(shell_.Actions().at(*idx));

            DrawDesktopShortcutTile(
                dc, cell, wl::kWorkshopTiles[static_cast<std::size_t>(i)].label, i, ready);
            PushHitZone(cell.hitRect, WorkshopHitKind::DesktopTile, i);
        }

        RestoreDC(dc, clipSave);
    }

    void DrawWorkshopGameStrip(HDC dc, const RECT& strip) {
        const auto& games = shell_.GameProjects();
        if (games.empty()) {
            DrawTextLine(dc,
                         InsetRectBy(strip, 10, 12),
                         "No game manifests found. Add Games/*/manifest.json to list projects here.",
                         pal::kMuted,
                         smallFont_,
                         DT_LEFT | DT_WORDBREAK);
            return;
        }

        const int clipSave = SaveDC(dc);
        IntersectClipRect(dc, strip.left, strip.top, strip.right, strip.bottom);

        constexpr int maxCards = 4;
        const int cardCount = (std::min)(maxCards, static_cast<int>(games.size()));
        constexpr int slotGap = 12;
        const int stripInnerW = RectWidth(strip) - 24;
        const int slotW =
            cardCount > 0 ? (stripInnerW - (cardCount - 1) * slotGap) / cardCount : stripInnerW;
        const bool stackVertically =
            cardCount >= 2 && (RectWidth(strip) < 380 || slotW < 168);

        constexpr int kCardPad = 8;
        constexpr int kThumbWMax = 76;
        constexpr int kMinTextReserve = 132;
        constexpr int btnH = 22;

        auto drawOneCard = [&](const RECT& card, int gameIndex) {
            const auto& game = games[static_cast<std::size_t>(gameIndex)];
            const std::size_t gi = static_cast<std::size_t>(gameIndex);

            FillRectVerticalGradient(dc, card, pal::kGameCardTop, pal::kGameCardBottom);
            DrawEdgeLines(dc, card, pal::kHighlight, pal::kDarkShadow);

            if (RectHeight(card) < 48) {
                constexpr int kTinyPad = 5;
                const int btnGap = 6;
                const int tinyBtnH = (std::max)(16, (std::min)(20, RectHeight(card) - 8));
                const int btnTop = card.top + (RectHeight(card) - tinyBtnH) / 2;
                const int actionW =
                    std::clamp(RectWidth(card) * 48 / 100, 126, (std::max)(126, RectWidth(card) - 96));
                const int actionsLeft = (std::max)(card.left + 96, card.right - kTinyPad - actionW);
                const int btnW =
                    (std::max)(0, static_cast<int>(card.right - kTinyPad - actionsLeft - btnGap) / 2);

                DrawTextLine(dc,
                             RECT{card.left + kTinyPad,
                                  card.top + 1,
                                  actionsLeft - 8,
                                  card.bottom - 1},
                             game.name,
                             pal::kText,
                             smallFont_,
                             DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);

                RECT openBtn{actionsLeft, btnTop, actionsLeft + btnW, btnTop + tinyBtnH};
                RECT editBtn{openBtn.right + btnGap, btnTop, card.right - kTinyPad, btnTop + tinyBtnH};
                DrawTag(dc, openBtn, "Folder", pal::kTagOpenFolder, pal::kText, smallFont_, true);
                DrawTag(dc, editBtn, "Editor", pal::kTagEditor, pal::kTitleBarText, smallFont_, true);
                PushHitZone(openBtn, WorkshopHitKind::GameOpenFolder, gameIndex);
                PushHitZone(editBtn, WorkshopHitKind::GameLaunchEditor, gameIndex);
                return;
            }

            const bool compactCard = RectHeight(card) < 68;
            const int cardPad = compactCard ? 5 : kCardPad;
            const int localBtnH = compactCard ? 19 : btnH;
            const int btnTop = card.bottom - localBtnH - (compactCard ? 4 : 6);
            const int innerW = RectWidth(card) - cardPad * 2;
            const int thumbW =
                (!compactCard && !stackVertically && innerW > kMinTextReserve + 52)
                    ? (std::min)(kThumbWMax, innerW - kMinTextReserve)
                    : 0;
            int textLeft = card.left + cardPad;
            if (thumbW >= 52) {
                RECT thumb{
                    card.left + cardPad,
                    card.top + cardPad,
                    card.left + cardPad + thumbW,
                    btnTop - cardPad,
                };
                if (RectHeight(thumb) > 12) {
                    FillGameThumbnailWell(dc, thumb, gi);
                    DrawEdgeLines(dc, thumb, RGB(102, 110, 124), RGB(30, 32, 38));
                    DrawGamePackageGlyph(dc, thumb, gi);
                    textLeft = thumb.right + kCardPad;
                }
            }

            DrawTextLine(dc,
                         RECT{textLeft, card.top + (compactCard ? 4 : 6), card.right - cardPad, card.top + 22},
                         game.name,
                         pal::kText,
                         bodyFont_,
                         DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
            if (!compactCard && btnTop - (card.top + 22) >= 14) {
                DrawTextLine(dc,
                             RECT{textLeft, card.top + 22, card.right - cardPad, card.top + 38},
                             game.id,
                             pal::kMuted,
                             smallFont_,
                             DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
            }

            const int btnGap = 8;
            const int pairInner = innerW - btnGap;
            // Equal split so Open + gap + Editor never overlaps (max(56,…) was wider than half on narrow cards).
            const int btnW = pairInner > 0 ? pairInner / 2 : 0;
            RECT openBtn{
                card.left + cardPad,
                btnTop,
                card.left + cardPad + btnW,
                btnTop + localBtnH,
            };
            RECT editBtn{
                openBtn.right + btnGap,
                btnTop,
                card.right - cardPad,
                btnTop + localBtnH,
            };

            DrawTag(dc,
                    openBtn,
                    compactCard ? "Folder" : "Open folder",
                    pal::kTagOpenFolder,
                    pal::kText,
                    smallFont_,
                    true);
            DrawTag(dc,
                    editBtn,
                    "Editor",
                    pal::kTagEditor,
                    pal::kTitleBarText,
                    smallFont_,
                    true);

            PushHitZone(openBtn, WorkshopHitKind::GameOpenFolder, gameIndex);
            PushHitZone(editBtn, WorkshopHitKind::GameLaunchEditor, gameIndex);
        };

        if (stackVertically) {
            const int gap = 8;
            const int usableH = RectHeight(strip) - 20;
            const int cardH =
                (std::max)(76, (usableH - (cardCount - 1) * gap) / (std::max)(1, cardCount));
            const int stripBottomInset = static_cast<int>(strip.bottom) - 10;
            int y = strip.top + 10;
            for (int i = 0; i < cardCount; ++i) {
                RECT card{
                    strip.left + 10,
                    y,
                    strip.right - 10,
                    static_cast<LONG>((std::min)(y + cardH, stripBottomInset)),
                };
                drawOneCard(card, i);
                y = card.bottom + gap;
                if (y >= strip.bottom - 12) {
                    break;
                }
            }
        } else {
            int x = strip.left + 12;
            const int cardTop = strip.top + 10;
            const int cardBottom = strip.bottom - 10;
            for (int i = 0; i < cardCount; ++i) {
                RECT card{x, cardTop, x + slotW, cardBottom};
                x += slotW + slotGap;
                drawOneCard(card, i);
            }
        }

        if (static_cast<int>(games.size()) > maxCards) {
            DrawTextLine(dc,
                         RECT{strip.right - 200, strip.top + 6, strip.right - 12, strip.top + 22},
                         "+" + std::to_string(games.size() - maxCards) + " more in workspace",
                         pal::kMuted,
                         smallFont_,
                         DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        }

        RestoreDC(dc, clipSave);
    }

    void DrawRecentEditorInstances(HDC dc, const RECT& panel) {
        const auto& recent = shell_.RecentSessions();
        const int lineH = 22;
        int y = panel.top + 8;
        const std::size_t maxLines = 5;
        if (recent.empty()) {
            DrawTextLine(dc,
                         RECT{panel.left + 6, y, panel.right - 6, y + 40},
                         "No recent sessions yet. Launch a game in the editor to populate this list.",
                         pal::kMuted,
                         smallFont_,
                         DT_LEFT | DT_WORDBREAK);
            return;
        }

        for (std::size_t i = 0; i < recent.size() && i < maxLines; ++i) {
            const std::wstring w = Utf8BytesToWide(recent[i]);
            fs::path p(w.empty() ? fs::path(recent[i]) : fs::path(w));
            std::wstring line = p.filename().wstring();
            if (line.empty()) {
                line = L"(session)";
            }
            DrawTextLineW(dc,
                          RECT{panel.left + 10, y, panel.right - 10, y + lineH},
                          L"• " + line,
                          pal::kText,
                          smallFont_,
                          DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
            y += lineH;
        }
    }

    void DrawTaskbarLamp(HDC dc, const RECT& lamp, COLORREF glow, bool active) {
        if (RectWidth(lamp) <= 0 || RectHeight(lamp) <= 0) {
            return;
        }

        HBRUSH dark = CreateSolidBrush(RGB(8, 10, 10));
        HBRUSH lit = CreateSolidBrush(active ? glow : RGB(52, 55, 50));
        HPEN rim = CreatePen(PS_SOLID, 1, active ? RGB(180, 164, 120) : RGB(76, 76, 68));
        HPEN oldPen = static_cast<HPEN>(SelectObject(dc, rim));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(dc, dark));
        Ellipse(dc, lamp.left, lamp.top, lamp.right, lamp.bottom);
        SelectObject(dc, lit);
        RECT inner = InsetRectBy(lamp, 2, 2);
        Ellipse(dc, inner.left, inner.top, inner.right, inner.bottom);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(rim);
        DeleteObject(lit);
        DeleteObject(dark);
    }

    void DrawTaskbarCellFrame(HDC dc, const RECT& cell, COLORREF rail, bool active) {
        if (RectWidth(cell) <= 0 || RectHeight(cell) <= 0) {
            return;
        }

        FillRectVerticalGradient(dc,
                                 cell,
                                 active ? RGB(55, 50, 40) : RGB(37, 40, 41),
                                 active ? RGB(24, 21, 17) : RGB(16, 18, 20));
        RECT topRail{cell.left + 1, cell.top + 1, cell.right - 1, cell.top + 3};
        FillRectColor(dc, topRail, rail);
        RECT leftGroove{cell.left + 3, cell.top + 5, cell.left + 5, cell.bottom - 5};
        FillRectColor(dc, leftGroove, active ? RGB(128, 83, 38) : RGB(62, 61, 54));
        DrawEdgeLines(dc, cell, RGB(104, 102, 91), RGB(5, 6, 7));

        HPEN grain = CreatePen(PS_SOLID, 1, RGB(32, 34, 34));
        HPEN oldPen = static_cast<HPEN>(SelectObject(dc, grain));
        for (int y = cell.top + 9; y < cell.bottom - 5; y += 7) {
            MoveToEx(dc, cell.left + 8, y, nullptr);
            LineTo(dc, cell.right - 6, y);
        }
        SelectObject(dc, oldPen);
        DeleteObject(grain);
    }

    void DrawTaskbarCommandDeck(HDC dc, const RECT& deck) {
        if (RectWidth(deck) < 72 || RectHeight(deck) < 14) {
            return;
        }

        DrawTaskbarCellFrame(dc, deck, RGB(87, 73, 48), false);

        struct CommandSlot {
            const char* label;
            COLORREF rail;
        };
        static constexpr CommandSlot kSlots[] = {
            {"console", RGB(167, 116, 52)},
            {"editor", RGB(143, 134, 104)},
            {"forge", RGB(214, 128, 42)},
            {"materials", RGB(120, 150, 132)},
        };

        constexpr int count = static_cast<int>(sizeof(kSlots) / sizeof(kSlots[0]));
        constexpr int gap = 6;
        const int innerLeft = deck.left + 10;
        const int innerRight = deck.right - 10;
        const int innerW = (std::max)(0, innerRight - innerLeft);
        const int slotW = (innerW - (count - 1) * gap) / count;
        if (slotW < 58) {
            DrawTextLine(dc,
                         InsetRectBy(deck, 12, 4),
                         "> console    > editor    > forge    > materials",
                         pal::kToolbarSpacerText,
                         monoFont_,
                         DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
            return;
        }

        int x = innerLeft;
        for (const CommandSlot& slot : kSlots) {
            RECT command{x, deck.top + 6, x + slotW, deck.bottom - 6};
            FillRectVerticalGradient(dc, command, RGB(31, 33, 33), RGB(17, 19, 20));
            RECT rail{command.left, command.top, command.left + 2, command.bottom};
            FillRectColor(dc, rail, slot.rail);
            DrawEdgeLines(dc, command, RGB(78, 78, 71), RGB(8, 9, 10));

            DrawTextLine(dc,
                         RECT{command.left + 8, command.top, command.right - 5, command.bottom},
                         slot.label,
                         pal::kToolbarSpacerText,
                         monoFont_,
                         DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
            x = command.right + gap;
        }
    }

    void DrawWorkshopTaskbar(HDC dc, const RECT& taskbar, const wl::TaskbarLayout& tb) {
        PushHitZone(taskbar, WorkshopHitKind::TaskbarBackground, 0);

        const int clipSave = SaveDC(dc);
        IntersectClipRect(dc, taskbar.left, taskbar.top, taskbar.right, taskbar.bottom);

        FillRectVerticalGradient(dc, taskbar, pal::kTaskbarFaceTop, pal::kTaskbarFaceBottom);
        RECT rim{taskbar.left, taskbar.top, taskbar.right, taskbar.top + 2};
        FillRectColor(dc, rim, pal::kTaskbarRim);
        RECT upperInset{taskbar.left + 3, taskbar.top + 4, taskbar.right - 3, taskbar.top + 5};
        FillRectColor(dc, upperInset, RGB(74, 69, 55));
        RECT lowerLip{taskbar.left, taskbar.bottom - 2, taskbar.right, taskbar.bottom};
        FillRectColor(dc, lowerLip, pal::kDarkShadow);
        DrawEdgeLines(dc, taskbar, pal::kHighlight, pal::kDarkShadow);

        RECT orb = tb.startButton;
        DrawTaskbarCellFrame(dc, orb, pal::kAccentEmber, true);
        RECT orbStripe{orb.left + 1, orb.top + 1, orb.right - 1, orb.top + 3};
        FillRectColor(dc, orbStripe, pal::kAccentEmber);
        RECT orbLamp{orb.left + 10, orb.top + 10, orb.left + 18, orb.top + 18};
        DrawTaskbarLamp(dc, orbLamp, RGB(215, 145, 55), true);
        DrawTextLine(dc,
                     RECT{orb.left + 22, orb.top, orb.right - 8, orb.bottom},
                     "RI",
                     pal::kTitleBarText,
                     menuFont_,
                     DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        taskbarStartOrbRect_ = orb;
        PushHitZone(orb, WorkshopHitKind::StartOrb, 0);

        if (!IsRectEmpty(&tb.spacer)) {
            DrawTaskbarCommandDeck(dc, tb.spacer);
        }

        if (!IsRectEmpty(&tb.consoleButton)) {
            DrawTaskbarCellFrame(dc, tb.consoleButton, RGB(104, 100, 82), false);
            RECT cliLamp{tb.consoleButton.left + 9,
                         tb.consoleButton.top + 11,
                         tb.consoleButton.left + 16,
                         tb.consoleButton.top + 18};
            DrawTaskbarLamp(dc, cliLamp, pal::kReady, true);
            DrawTextLine(dc,
                         RECT{tb.consoleButton.left + 20,
                              tb.consoleButton.top,
                              tb.consoleButton.right - 8,
                              tb.consoleButton.bottom},
                         "CLI",
                         pal::kToolbarSpacerText,
                         smallFont_,
                         DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            PushHitZone(tb.consoleButton, WorkshopHitKind::TaskConsoleAction, 0);
        }

        const auto& recent = shell_.RecentSessions();
        if (!IsRectEmpty(&tb.recentEditorButton)) {
            DrawTaskbarCellFrame(dc,
                                 tb.recentEditorButton,
                                 recent.empty() ? RGB(80, 74, 60) : RGB(119, 146, 112),
                                 !recent.empty());
            const std::wstring recentLabel = FormatRecentSessionLabelW(recent);
            DrawTextLineW(dc,
                          RECT{tb.recentEditorButton.left + 10,
                               tb.recentEditorButton.top,
                               tb.recentEditorButton.right - 8,
                               tb.recentEditorButton.bottom},
                          recentLabel,
                          recent.empty() ? pal::kMuted : pal::kText,
                          smallFont_,
                          DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
            if (!recent.empty()) {
                PushHitZone(tb.recentEditorButton, WorkshopHitKind::TaskRecentOpen, 0);
            }
        }

        const char* cfg = std::string_view(kBuildConfiguration).empty() ? "build" : kBuildConfiguration;
        std::string statusLine =
            std::string(shell_.Busy() ? "busy" : "idle") + " | " + std::to_string(shell_.Actions().size()) +
            " actions";
        if (!IsRectEmpty(&tb.buildStatus)) {
            DrawTaskbarCellFrame(dc, tb.buildStatus, shell_.Busy() ? pal::kAccentEmber : RGB(91, 132, 85), false);
            RECT statusLamp{tb.buildStatus.left + 9,
                            tb.buildStatus.top + 10,
                            tb.buildStatus.left + 18,
                            tb.buildStatus.top + 19};
            DrawTaskbarLamp(dc, statusLamp, shell_.Busy() ? pal::kAccentEmber : pal::kReady, true);
            DrawTextLine(dc,
                         RECT{tb.buildStatus.left + 24, tb.buildStatus.top, tb.buildStatus.right - 8, tb.buildStatus.bottom},
                         statusLine,
                         shell_.Busy() ? pal::kAccentEmber : pal::kReady,
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
        }

        std::string verLine = std::string("Vulkan | ") + cfg;
        if (!IsRectEmpty(&tb.engineVersion)) {
            DrawTaskbarCellFrame(dc, tb.engineVersion, RGB(90, 94, 90), false);
            DrawTextLine(dc,
                         RECT{tb.engineVersion.left + 8, tb.engineVersion.top, tb.engineVersion.right - 8, tb.engineVersion.bottom},
                         verLine,
                         pal::kMuted,
                         smallFont_,
                         DT_RIGHT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
        }

        RestoreDC(dc, clipSave);
    }

    void DrawWorkshopStartMenu(HDC dc) {
        if (!showStartMenu_) {
            return;
        }

        RECT panel = startMenuPanelRect_;
        FillRectVerticalGradient(dc, panel, pal::kStartMenuFillTop, pal::kStartMenuFillBottom);
        RECT menuRim{panel.left, panel.top, panel.right, panel.top + 3};
        FillRectColor(dc, menuRim, pal::kAccentEmber);
        DrawEdgeLines(dc, panel, pal::kHighlight, pal::kDarkShadow);

        DrawTextLine(dc,
                     RECT{panel.left + 14, panel.top + 10, panel.right - 14, panel.top + 30},
                     "RawIron",
                     pal::kText,
                     menuFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        struct Item {
            const char* title;
            int payload;
        };
        const Item items[] = {
            {"New project...", 0},
            {"Open Games folder", 1},
            {"Forge (preview)", 2},
            {"Editor", 3},
            {"Workspace CLI check", 4},
            {"Documentation", 5},
            {"Close", 6},
        };

        int y = panel.top + 42;
        for (const Item& item : items) {
            RECT row{panel.left + 8, y, panel.right - 8, y + 30};
            FillRectVerticalGradient(dc, row, RGB(43, 45, 47), pal::kStartMenuRow);
            RECT rail{row.left, row.top, row.left + 3, row.bottom};
            FillRectColor(dc, rail, RGB(105, 72, 36));
            DrawEdgeLines(dc, row, pal::kLightShadow, pal::kDarkShadow);
            DrawTextLine(dc,
                         RECT{row.left + 12, row.top, row.right - 10, row.bottom},
                         item.title,
                         pal::kText,
                         bodyFont_,
                         DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            PushHitZone(row, WorkshopHitKind::StartMenuItem, item.payload);
            y += 32;
        }
    }

    void OnMouseLeftDown(int x, int y) {
        const POINT pt{x, y};
        for (auto it = hitZones_.rbegin(); it != hitZones_.rend(); ++it) {
            if (!PtInRect(&it->rect, pt)) {
                continue;
            }

            switch (it->kind) {
                case WorkshopHitKind::StartOrb:
                    showStartMenu_ = !showStartMenu_;
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    return;
                case WorkshopHitKind::DesktopTile: {
                    showStartMenu_ = false;
                    const int tileIndex = it->payload;
                    if (tileIndex < 0 || tileIndex >= wl::kDesktopIconCount) {
                        return;
                    }
                    const char* sub = wl::kWorkshopTiles[static_cast<std::size_t>(tileIndex)].actionTitleSubstring;
                    const auto idx = FindActionIndex(shell_.Actions(), sub);
                    if (!idx) {
                        shell_.AppendLog("No launcher mapped for this tile.");
                        InvalidateRect(hwnd_, nullptr, FALSE);
                        return;
                    }
                    shell_.SetSelectedIndex(*idx);
                    shell_.LaunchSelected(hwnd_);
                    return;
                }
                case WorkshopHitKind::GameOpenFolder: {
                    showStartMenu_ = false;
                    const auto& games = shell_.GameProjects();
                    if (it->payload >= 0 && it->payload < static_cast<int>(games.size())) {
                        shell_.OpenGameFolder(games[static_cast<std::size_t>(it->payload)], hwnd_);
                    }
                    return;
                }
                case WorkshopHitKind::GameLaunchEditor: {
                    showStartMenu_ = false;
                    const auto& games = shell_.GameProjects();
                    if (it->payload >= 0 && it->payload < static_cast<int>(games.size())) {
                        shell_.LaunchEditorForGame(games[static_cast<std::size_t>(it->payload)], hwnd_);
                    }
                    return;
                }
                case WorkshopHitKind::TaskConsoleAction: {
                    showStartMenu_ = false;
                    const auto idx = FindActionIndex(shell_.Actions(), "Workspace Check");
                    if (idx) {
                        shell_.SetSelectedIndex(*idx);
                        shell_.LaunchSelected(hwnd_);
                    }
                    return;
                }
                case WorkshopHitKind::TaskRecentOpen: {
                    showStartMenu_ = false;
                    const auto& recent = shell_.RecentSessions();
                    if (!recent.empty()) {
                        fs::path p(recent.front());
                        if (OpenFolder(p)) {
                            shell_.AppendLog("Opened recent path.");
                        }
                    }
                    return;
                }
                case WorkshopHitKind::TaskbarBackground:
                    showStartMenu_ = false;
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    return;
                case WorkshopHitKind::StartMenuItem: {
                    showStartMenu_ = false;
                    switch (it->payload) {
                        case 0:
                            shell_.AppendLog("New project: coming soon.");
                            break;
                        case 1:
                            OpenFolder(shell_.SourceRoot() / "Games");
                            shell_.AppendLog("Opened Games folder.");
                            break;
                        case 2: {
                            const auto idx = FindActionIndex(shell_.Actions(), "Preview Window");
                            if (idx) {
                                shell_.SetSelectedIndex(*idx);
                                shell_.LaunchSelected(hwnd_);
                            }
                            break;
                        }
                        case 3: {
                            const auto idx = FindActionIndex(shell_.Actions(), "Level Editor");
                            if (idx) {
                                shell_.SetSelectedIndex(*idx);
                                shell_.LaunchSelected(hwnd_);
                            }
                            break;
                        }
                        case 4: {
                            const auto idx = FindActionIndex(shell_.Actions(), "Workspace Check");
                            if (idx) {
                                shell_.SetSelectedIndex(*idx);
                                shell_.LaunchSelected(hwnd_);
                            }
                            break;
                        }
                        case 5: {
                            const auto idx = FindActionIndex(shell_.Actions(), "Open Documentation");
                            if (idx) {
                                shell_.SetSelectedIndex(*idx);
                                shell_.LaunchSelected(hwnd_);
                            }
                            break;
                        }
                        default:
                            break;
                    }
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    return;
                }
                case WorkshopHitKind::None:
                    break;
            }
            return;
        }

        if (showStartMenu_) {
            const bool inMenu = !IsRectEmpty(&startMenuPanelRect_) && PtInRect(&startMenuPanelRect_, pt);
            const bool onOrb = !IsRectEmpty(&taskbarStartOrbRect_) && PtInRect(&taskbarStartOrbRect_, pt);
            if (!inMenu && !onOrb) {
                showStartMenu_ = false;
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
        }
    }

    void Paint() {
        PAINTSTRUCT paint{};
        HDC windowDc = BeginPaint(hwnd_, &paint);

        RECT client{};
        GetClientRect(hwnd_, &client);
        const int clientWidth = std::max(1L, client.right - client.left);
        const int clientHeight = std::max(1L, client.bottom - client.top);

        HDC dc = CreateCompatibleDC(windowDc);
        HBITMAP backBuffer = CreateCompatibleBitmap(windowDc, clientWidth, clientHeight);
        HGDIOBJ oldBitmap = SelectObject(dc, backBuffer);
        // Memory DC inherits BeginPaint's update-region clip; partial clips leave stale pixels → ember trails.
        SelectClipRgn(dc, nullptr);
        IntersectClipRect(dc, 0, 0, clientWidth, clientHeight);

        RECT clearRc{0, 0, clientWidth, clientHeight};
        HBRUSH clearBrush = CreateSolidBrush(pal::kMetalClear);
        FillRect(dc, &clearRc, clearBrush);
        DeleteObject(clearBrush);

        ClearHitZones();

        const wl::WorkshopFrameLayout lay = wl::ComputeWorkshopFrameLayout(client, showStartMenu_);
        startMenuPanelRect_ = lay.startMenuPanel;

        const RECT& taskbar = lay.taskbar;
        const RECT& header = lay.header;
        const RECT& recentPanel = lay.recentPanel;
        const RECT& gameCardsPanel = lay.gameCardsPanel;
        const RECT& projectPanel = lay.projectPanel;
        const RECT& buildPanel = lay.buildPanel;
        const RECT& previewPanel = lay.previewPanel;

        const ri::shell::WorkshopImage* workshopBg =
            backgroundImage_.Valid() ? &backgroundImage_ : nullptr;
        const ri::shell::WorkshopImage* workshopAmbient =
            ambientImage_.Valid() ? &ambientImage_ : nullptr;
        const ri::shell::WorkshopImage* workshopNormal =
            normalImage_.Valid() ? &normalImage_ : nullptr;
        const ri::shell::WorkshopImage* workshopDisp =
            displacementImage_.Valid() ? &displacementImage_ : nullptr;
        const ri::shell::WorkshopImage* workshopSpec =
            specularImage_.Valid() ? &specularImage_ : nullptr;
        const ri::shell::WorkshopImage* workshopLogo = logoImage_.Valid() ? &logoImage_ : nullptr;
        const float workshopAnimSec =
            std::fmod(WorkshopAnimationTimeSeconds(), 2048.0f);
        ri::shell::DrawWorkshopChrome(dc,
                                      client,
                                      workshopBg,
                                      workshopAmbient,
                                      workshopNormal,
                                      workshopDisp,
                                      workshopSpec,
                                      workshopLogo,
                                      header,
                                      L"Raw Iron Workspace",
                                      workshopAnimSec);
        SelectClipRgn(dc, nullptr);
        IntersectClipRect(dc, 0, 0, clientWidth, clientHeight);

        auto visiblePanel = [](const RECT& r) {
            return RectWidth(r) > 8 && RectHeight(r) > 8;
        };

        const int desktopClip = SaveDC(dc);
        IntersectClipRect(dc, lay.desktop.left, lay.desktop.top, lay.desktop.right, lay.desktop.bottom);

        DrawDesktopIconGrid(dc, lay);

        RECT recentClient{};
        RECT gameClient{};
        if (visiblePanel(recentPanel)) {
            recentClient = DrawSourceToolWindowChrome(dc, recentPanel, "RECENT EDITOR INSTANCES");
        }
        if (visiblePanel(gameCardsPanel)) {
            gameClient = DrawSourceToolWindowChrome(dc, gameCardsPanel, "GAMES / PROJECTS");
        }

        if (visiblePanel(projectPanel)) {
            DrawPanelDropShadow(dc, projectPanel, 2);
            DrawBeveledPanel(dc, projectPanel);
        }
        if (visiblePanel(buildPanel)) {
            DrawPanelDropShadow(dc, buildPanel, 2);
            DrawBeveledPanel(dc, buildPanel);
        }
        if (visiblePanel(previewPanel)) {
            DrawPanelDropShadow(dc, previewPanel, 2);
            DrawBeveledPanel(dc, previewPanel);
        }

        if (visiblePanel(recentClient)) {
            DrawRecentEditorInstances(dc, recentClient);
        }
        if (visiblePanel(gameClient)) {
            DrawWorkshopGameStrip(dc, gameClient);
        }
        if (visiblePanel(projectPanel)) {
            DrawSelectionPanel(dc, projectPanel);
        }
        if (visiblePanel(buildPanel)) {
            DrawLogPanel(dc, buildPanel);
        }
        if (visiblePanel(previewPanel)) {
            DrawPreviewPanel(dc, previewPanel);
        }

        RestoreDC(dc, desktopClip);

        DrawHeader(dc, header);

        DrawWorkshopTaskbar(dc, taskbar, lay.taskbarItems);
        DrawWorkshopStartMenu(dc);

        BitBlt(windowDc, 0, 0, clientWidth, clientHeight, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(backBuffer);
        DeleteDC(dc);
        EndPaint(hwnd_, &paint);
    }

    void DrawHeader(HDC dc, const RECT& headerRect) {
        const int t = headerRect.top;
        DrawTag(dc,
                RECT{headerRect.right - 128, t + 8, headerRect.right - 12, t + 28},
                shell_.Busy() ? "BUSY" : "READY",
                shell_.Busy() ? RGB(94, 59, 30) : RGB(42, 72, 45),
                pal::kTitleBarText,
                smallFont_);

        const int left = headerRect.left + 14;
        const int right = headerRect.right - 14;
        const int statsLeft = headerRect.right - 318;

        const int rowSloganTop = t + 34;
        const int rowSloganBot = t + 50;
        DrawTextLine(dc,
                     RECT{left, rowSloganTop, statsLeft - 10, rowSloganBot},
                     "RAWIRON // BUILD FORGE PLAY",
                     pal::kToolbarSpacerText,
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
        DrawTextLine(dc,
                     RECT{statsLeft, rowSloganTop, right, rowSloganBot},
                     std::to_string(shell_.Actions().size()) + " actions | " + std::to_string(shell_.ExampleCount()) +
                         " Scene Kit targets",
                     pal::kText,
                     smallFont_,
                     DT_RIGHT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);

        const int rowBuildTop = t + 50;
        const int rowBuildBot = t + 68;
        DrawTextLine(dc,
                     RECT{left, rowBuildTop, right, rowBuildBot},
                     "Build: " + shell_.BuildRoot().string(),
                     pal::kMuted,
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_PATH_ELLIPSIS | DT_VCENTER);

        const int rowSourceTop = t + 68;
        const int rowSourceBot = headerRect.bottom - 8;
        if (rowSourceBot > rowSourceTop) {
            DrawTextLine(dc,
                         RECT{left, rowSourceTop, right, rowSourceBot},
                         "Source: " + shell_.SourceRoot().string(),
                         pal::kMuted,
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_PATH_ELLIPSIS | DT_VCENTER);
        }
    }

    void DrawActionsPanel(HDC dc, const RECT& panelRect) {
        const auto& actions = shell_.Actions();
        const int headerTop = panelRect.top + 14;
        DrawTextLine(dc,
                     RECT{panelRect.left + 18, headerTop, panelRect.right - 18, headerTop + 18},
                     "ACTIONS",
                     pal::kMuted,
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        DrawTextLine(dc,
                     RECT{panelRect.left + 18, headerTop + 18, panelRect.right - 18, headerTop + 46},
                     "Launchers and checks",
                     pal::kText,
                     menuFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        RECT listFrame{panelRect.left + 16, panelRect.top + 56, panelRect.right - 16, panelRect.bottom - 42};
        DrawBeveledPanel(dc, listFrame, true);
        RECT rowsArea = InsetRectBy(listFrame, 12, 12);

        const int rowHeight = 38;
        const int rowGap = 6;
        const int visibleRows = std::max(1, (RectHeight(rowsArea) + rowGap) / (rowHeight + rowGap));
        const int totalRows = static_cast<int>(actions.size());
        const int selectedIndex = static_cast<int>(shell_.SelectedIndex());
        const int maxFirstVisible = std::max(0, totalRows - visibleRows);
        const int firstVisible = std::clamp(selectedIndex - std::max(0, visibleRows / 2), 0, maxFirstVisible);
        const int lastVisible = std::min(totalRows, firstVisible + visibleRows);

        int rowTop = rowsArea.top;
        for (int rowIndex = firstVisible; rowIndex < lastVisible; ++rowIndex) {
            RECT row{rowsArea.left, rowTop, rowsArea.right, rowTop + rowHeight};
            const bool isSelected = rowIndex == selectedIndex;
            if (isSelected) {
                FillRectColor(dc, row, pal::kSelection);
                DrawEdgeLines(dc, row, RGB(214, 157, 74), RGB(60, 36, 18));
                RECT inner = row;
                InflateRectCopy(inner, 1);
                DrawEdgeLines(dc, inner, RGB(169, 111, 50), RGB(42, 27, 16));
            } else {
                DrawBeveledPanel(dc, row, true);
            }

            const Action& action = actions.at(static_cast<std::size_t>(rowIndex));
            DrawTextLine(dc,
                         RECT{row.left + 12, row.top, row.right - 118, row.bottom},
                         std::to_string(rowIndex + 1) + ". " + action.title,
                         isSelected ? pal::kSelectionText : pal::kText,
                         bodyFont_,
                         DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

            DrawTag(dc,
                    RECT{row.right - 96, row.top + 8, row.right - 12, row.bottom - 8},
                    TargetReady(action) ? "READY" : "MISSING",
                    isSelected ? RGB(85, 63, 36) : (TargetReady(action) ? RGB(35, 55, 38) : RGB(78, 42, 32)),
                    TargetReady(action) ? pal::kReady : pal::kMissing,
                    smallFont_,
                    !isSelected);
            rowTop += rowHeight + rowGap;
        }

        DrawTextLine(dc,
                     RECT{panelRect.left + 18, panelRect.bottom - 30, panelRect.right - 18, panelRect.bottom - 12},
                     "Showing " + std::to_string(firstVisible + 1) + "-" + std::to_string(lastVisible) +
                         " of " + std::to_string(totalRows) +
                         "  |  Up/Down to move, Enter to run",
                     pal::kMuted,
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    }

    void DrawPreviewPanel(HDC dc, const RECT& targetRect) {
        const int pvClip = SaveDC(dc);
        IntersectClipRect(dc, targetRect.left, targetRect.top, targetRect.right, targetRect.bottom);

        RECT titleRect{targetRect.left + 18, targetRect.top + 14, targetRect.right - 18, targetRect.top + 34};
        const auto* example = shell_.CurrentExample();
        DrawTextLine(dc, titleRect, "SCENE KIT TARGET", pal::kMuted, smallFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        RECT frame{targetRect.left + 18, targetRect.top + 46, targetRect.right - 18, targetRect.bottom - 18};
        DrawBeveledPanel(dc, frame, true);
        const int frameH = RectHeight(frame);

        if (example == nullptr) {
            DrawTextLine(dc,
                         RECT{frame.left + 18, frame.top + 12, frame.right - 18, frame.bottom - 12},
                         "Scene Kit examples were not available when the shell started.",
                         pal::kText,
                         bodyFont_,
                         DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
            RestoreDC(dc, pvClip);
            return;
        }

        constexpr int kNestedMinFrameH = 152;
        const bool useNestedCard = frameH >= kNestedMinFrameH;

        if (!useNestedCard) {
            const int pad = 10;
            const int innerL = frame.left + pad;
            const int innerR = frame.right - pad;
            int y = frame.top + pad;
            const int bottomLimit = frame.bottom - pad;
            const int tagW = std::clamp(RectWidth(frame) / 3, 72, 118);

            HFONT titleF = frameH >= 72 ? titleFont_ : bodyFont_;

            DrawTextLine(dc,
                         RECT{innerL, y, innerR - tagW - 8, y + 22},
                         example->title,
                         pal::kText,
                         titleF,
                         DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
            DrawTag(dc,
                    RECT{innerR - tagW, y - 2, innerR, y + 24},
                    example->statusLabel,
                    RGB(39, 57, 42),
                    pal::kReady,
                    smallFont_,
                    true);
            y += 24;

            if (y + 14 <= bottomLimit) {
                DrawTextLine(dc,
                             RECT{innerL, y, innerR, y + 15},
                             "Example " + std::to_string(shell_.SelectedExampleIndex() + 1) + "/" +
                                 std::to_string(shell_.ExampleCount()),
                             pal::kMuted,
                             smallFont_,
                             DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
                y += 16;
            }

            auto lineIfFit = [&](const std::string& text, COLORREF color, HFONT font) {
                constexpr int lh = 16;
                if (y + lh > bottomLimit) {
                    return;
                }
                DrawTextLine(dc,
                             RECT{innerL, y, innerR, y + lh},
                             text,
                             color,
                             font,
                             DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
                y += lh + 2;
            };

            lineIfFit(std::string("Scene ID  ") + MakeDisplaySceneId(example->slug), pal::kText, bodyFont_);
            lineIfFit(std::string("Track  ") + example->rawIronTrack, pal::kText, bodyFont_);

            RestoreDC(dc, pvClip);
            return;
        }

        DrawTextLine(dc,
                     RECT{frame.left + 18, frame.top + 18, frame.right - 120, frame.top + 44},
                     example->title,
                     pal::kText,
                     titleFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        DrawTag(dc,
                RECT{frame.right - 138, frame.top + 18, frame.right - 18, frame.top + 46},
                example->statusLabel,
                RGB(39, 57, 42),
                pal::kReady,
                smallFont_,
                true);
        DrawTextLine(dc,
                     RECT{frame.left + 18, frame.top + 52, frame.right - 18, frame.top + 74},
                     "Example " + std::to_string(shell_.SelectedExampleIndex() + 1) + "/" +
                         std::to_string(shell_.ExampleCount()),
                     pal::kMuted,
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        RECT sceneCard{frame.left + 18, frame.top + 88, frame.right - 18, frame.bottom - 18};
        DrawBeveledPanel(dc, sceneCard);
        DrawTextLine(dc,
                     RECT{sceneCard.left + 16, sceneCard.top + 16, sceneCard.right - 16, sceneCard.top + 38},
                     "Scene ID  " + MakeDisplaySceneId(example->slug),
                     pal::kText,
                     bodyFont_,
                     DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
        DrawTextLine(dc,
                     RECT{sceneCard.left + 16, sceneCard.top + 48, sceneCard.right - 16, sceneCard.top + 68},
                     "Track",
                     pal::kMuted,
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        DrawTextLine(dc,
                     RECT{sceneCard.left + 16, sceneCard.top + 68, sceneCard.right - 16, sceneCard.top + 90},
                     example->rawIronTrack,
                     pal::kText,
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);

        RestoreDC(dc, pvClip);
    }

    void DrawSelectionPanel(HDC dc, const RECT& panelRect) {
        const auto& actions = shell_.Actions();
        if (actions.empty()) {
            return;
        }

        const int clipSave = SaveDC(dc);
        IntersectClipRect(dc, panelRect.left, panelRect.top, panelRect.right, panelRect.bottom);

        const Action& action = actions.at(shell_.SelectedIndex());
        const int innerLeft = panelRect.left + 14;
        const int innerRight = panelRect.right - 14;
        constexpr int lineH = 17;
        constexpr int headerEnd = 52;
        const int statusH = 18;
        const int statusTop = panelRect.bottom - 10 - statusH;

        DrawTextLine(dc,
                     RECT{innerLeft, panelRect.top + 10, innerRight, panelRect.top + 26},
                     "SELECTED ACTION",
                     pal::kMuted,
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        DrawTextLine(dc,
                     RECT{innerLeft, panelRect.top + 26, innerRight - 108, panelRect.top + 48},
                     action.title,
                     pal::kText,
                     menuFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

        DrawTag(dc,
                RECT{innerRight - 102, panelRect.top + 24, innerRight, panelRect.top + 50},
                TargetReady(action) ? "READY" : "MISSING",
                TargetReady(action) ? RGB(39, 57, 42) : RGB(78, 42, 32),
                TargetReady(action) ? pal::kReady : pal::kMissing,
                smallFont_,
                true);

        int y = panelRect.top + headerEnd;
        int yBodyEnd = statusTop - 8;
        if (yBodyEnd <= y) {
            DrawTextLine(dc,
                         RECT{innerLeft, statusTop, innerRight, panelRect.bottom - 6},
                         shell_.Busy() ? "Shell is processing..." : "Shell idle.",
                         shell_.Busy() ? RGB(146, 91, 22) : pal::kReady,
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
            RestoreDC(dc, clipSave);
            return;
        }

        auto nextLine = [&](const std::string& text, COLORREF color, HFONT font, UINT extra = 0) -> bool {
            if (y + lineH > yBodyEnd) {
                return false;
            }
            DrawTextLine(dc,
                         RECT{innerLeft, y, innerRight, y + lineH},
                         text,
                         color,
                         font,
                         DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER | extra);
            y += lineH + 2;
            return true;
        };

        nextLine(action.detail, pal::kText, bodyFont_);
        nextLine(std::string("Mode: ") + ActionKindLabel(action), pal::kText, smallFont_);
        nextLine(std::string("Target: ") + action.target.string(), pal::kText, smallFont_, DT_PATH_ELLIPSIS);
        nextLine(action.injectCurrentExample ? "Example: Scene Kit on" : "Example: Scene Kit off",
                 pal::kMuted,
                 smallFont_);

        DrawTextLine(dc,
                     RECT{innerLeft, statusTop, innerRight, panelRect.bottom - 6},
                     shell_.Busy() ? "Shell is processing the current request." :
                                     "Shell is idle and ready for the next action.",
                     shell_.Busy() ? RGB(146, 91, 22) : pal::kReady,
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);

        RestoreDC(dc, clipSave);
    }

    void DrawLogPanel(HDC dc, const RECT& panelRect) {
        const int clipSave = SaveDC(dc);
        IntersectClipRect(dc, panelRect.left, panelRect.top, panelRect.right, panelRect.bottom);

        const std::deque<std::string> logs = shell_.SnapshotLogs();
        if (RectHeight(panelRect) < 92) {
            const std::string latest = logs.empty() ? "No recent shell output." : logs.back();
            DrawTextLine(dc,
                         RECT{panelRect.left + 14, panelRect.top + 8, panelRect.right - 14, panelRect.top + 24},
                         "ACTIVITY",
                         pal::kMuted,
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            DrawTextLine(dc,
                         RECT{panelRect.left + 14, panelRect.top + 26, panelRect.right - 14, panelRect.bottom - 8},
                         latest,
                         pal::kLog,
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
            RestoreDC(dc, clipSave);
            return;
        }

        DrawTextLine(dc,
                     RECT{panelRect.left + 18, panelRect.top + 14, panelRect.right - 18, panelRect.top + 34},
                     "ACTIVITY LOG",
                     pal::kMuted,
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        DrawTextLine(dc,
                     RECT{panelRect.left + 18, panelRect.top + 34, panelRect.right - 18, panelRect.top + 54},
                     "Recent shell output",
                     pal::kText,
                     bodyFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        RECT logFrame{panelRect.left + 16, panelRect.top + 60, panelRect.right - 16, panelRect.bottom - 16};
        DrawBeveledPanel(dc, logFrame, true);
        const int lineHeight = 21;
        const int visibleLines = std::max(1, (RectHeight(logFrame) - 18) / lineHeight);
        const std::size_t firstVisible = logs.size() > static_cast<std::size_t>(visibleLines)
            ? logs.size() - static_cast<std::size_t>(visibleLines)
            : 0;

        int lineTop = logFrame.top + 10;
        for (std::size_t index = firstVisible; index < logs.size(); ++index) {
            DrawTextLine(dc,
                         RECT{logFrame.left + 12, lineTop, logFrame.right - 12, lineTop + lineHeight},
                         logs[index],
                         pal::kLog,
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
            lineTop += lineHeight;
        }

        RestoreDC(dc, clipSave);
    }

    ShellState& shell_;
    HWND hwnd_ = nullptr;
    HFONT titleFont_ = nullptr;
    HFONT menuFont_ = nullptr;
    HFONT bodyFont_ = nullptr;
    HFONT smallFont_ = nullptr;
    HFONT monoFont_ = nullptr;

    std::uintptr_t gdiplusToken_ = 0;
    ri::shell::WorkshopImage backgroundImage_;
    ri::shell::WorkshopImage ambientImage_;
    ri::shell::WorkshopImage normalImage_;
    ri::shell::WorkshopImage displacementImage_;
    ri::shell::WorkshopImage specularImage_;
    ri::shell::WorkshopImage logoImage_;
    bool showStartMenu_ = false;
    int minTrackW_ = 980;
    int minTrackH_ = 680;
    RECT startMenuPanelRect_{};
    RECT taskbarStartOrbRect_{};
    std::vector<WorkshopHitZone> hitZones_;
};

} // namespace

int RunVisualShellDesktopUi(ShellState& shell, HINSTANCE instance) {
    VisualShellWindow window(shell);
    return window.Run(instance);
}

#endif
