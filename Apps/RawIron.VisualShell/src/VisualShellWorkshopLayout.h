#pragma once

#include <algorithm>
#include <array>
#include <windows.h>

namespace ri::vshell::layout {

inline int LayoutRectWidth(const RECT& rect) {
    return static_cast<int>(rect.right - rect.left);
}

inline int LayoutRectHeight(const RECT& rect) {
    return static_cast<int>(rect.bottom - rect.top);
}

inline RECT LayoutInsetRect(const RECT& rect, int inset) {
    return RECT{
        rect.left + inset,
        rect.top + inset,
        rect.right - inset,
        rect.bottom - inset,
    };
}

inline RECT LayoutInsetRect(const RECT& rect, int xInset, int yInset) {
    return RECT{
        rect.left + xInset,
        rect.top + yInset,
        rect.right - xInset,
        rect.bottom - yInset,
    };
}

inline RECT LayoutOffsetRect(RECT rect, int dx, int dy) {
    rect.left += dx;
    rect.right += dx;
    rect.top += dy;
    rect.bottom += dy;
    return rect;
}

constexpr int kOuterMargin = 22;
constexpr int kGutter = 14;

constexpr int kTaskbarH = 48;
constexpr int kCanvasTopInset = 16;

/// Minimum height must fit slogan + Build + Source rows in DrawHeader (see VisualShellDesktop.cpp).
constexpr int kHeaderMin = 94;
constexpr int kHeaderMax = 102;

constexpr int kDesktopIconCols = 3;
constexpr int kDesktopIconRows = 4;
constexpr int kDesktopIconCount = kDesktopIconCols * kDesktopIconRows;

constexpr int kDesktopIconW = 92;
constexpr int kDesktopIconH = 84;
constexpr int kDesktopIconGapX = 18;
constexpr int kDesktopIconGapY = 14;

constexpr int kDesktopIconImage = 52;
constexpr int kDesktopIconLabelH = 26;

constexpr int kWidgetTitleBarH = 24;
constexpr int kWidgetPad = 12;

constexpr int kRecentPanelMinW = 300;
constexpr int kRecentPanelMaxW = 420;
constexpr int kRecentPanelMinH = 84;
constexpr int kRecentPanelMaxH = 124;

constexpr int kProjectPanelMinH = 118;
constexpr int kProjectPanelMaxH = 160;

constexpr int kGamePanelMinH = 132;
constexpr int kGamePanelMaxH = 184;

constexpr int kBuildPanelMinH = 92;
constexpr int kBuildPanelMaxH = 128;

constexpr int kPreviewPanelMinH = 128;
constexpr int kPreviewPanelMaxH = 228;

constexpr int kStartMenuW = 292;
constexpr int kStartMenuH = 326;

constexpr int kTaskbarStartW = 112;
constexpr int kTaskbarConsoleW = 126;
constexpr int kTaskbarRecentW = 210;
constexpr int kTaskbarStatusW = 176;
constexpr int kTaskbarVersionW = 132;
constexpr int kTaskbarMinRecentW = 108;
constexpr int kTaskbarMinStatusW = 112;
constexpr int kTaskbarMinVersionW = 92;

struct WorkshopTileSpec {
    const char* label;
    const char* actionTitleSubstring;
};

inline constexpr WorkshopTileSpec kWorkshopTiles[kDesktopIconCount] = {
    {"Editor", "Level Editor"},
    {"Forge", "Preview Window"},
    {"Examples", "Scene Kit Targets"},

    {"Tests", "Core Tests"},
    {"Games", "Liminal Hall Game"},
    {"Projects", "Workspace Check"},

    {"Plugins", "Vulkan Diagnostics"},
    {"Console", "Scene Kit Checks"},
    {"Settings", "Open Documentation"},

    {"Plugin Forge", "Engine Import Tests"},
    {"Mod Manager", "Open Previews Folder"},
    {"Performance Lab", "Liminal Vulkan FPS Sample"},
};

[[nodiscard]] inline int DesktopIconGridPixelWidth() {
    return kDesktopIconCols * kDesktopIconW + (kDesktopIconCols - 1) * kDesktopIconGapX;
}

[[nodiscard]] inline int DesktopIconGridPixelHeight() {
    return kDesktopIconRows * kDesktopIconH + (kDesktopIconRows - 1) * kDesktopIconGapY;
}

struct TaskbarLayout {
    RECT startButton{};
    RECT consoleButton{};
    RECT recentEditorButton{};
    RECT spacer{};
    RECT buildStatus{};
    RECT engineVersion{};
};

struct DesktopIconLayout {
    RECT hitRect{};
    RECT iconRect{};
    RECT labelRect{};
};

struct WorkshopFrameLayout {
    RECT taskbar{};
    RECT canvas{};

    RECT header{};
    RECT logoPlate{};
    RECT workspaceTitle{};
    RECT headerActions{};

    RECT desktop{};
    RECT iconGrid{};
    RECT desktopIconsArea{};

    RECT widgetColumn{};
    RECT recentPanel{};
    RECT projectPanel{};
    RECT gameCardsPanel{};
    RECT buildPanel{};
    RECT previewPanel{};

    RECT startMenuPanel{};

    TaskbarLayout taskbarItems{};
    std::array<DesktopIconLayout, kDesktopIconCount> desktopIcons{};
};

[[nodiscard]] inline TaskbarLayout ComputeTaskbarLayout(const RECT& taskbar) {
    TaskbarLayout out{};

    const int yPad = 7;
    const int xPad = 8;
    const int itemH = LayoutRectHeight(taskbar) - yPad * 2;
    const int innerLeft = taskbar.left + xPad;
    const int innerRight = taskbar.right - xPad;
    const int innerW = (std::max)(0, innerRight - innerLeft);

    int versionW = (std::min)(kTaskbarVersionW, (std::max)(kTaskbarMinVersionW, innerW / 7));
    int statusW = (std::min)(kTaskbarStatusW, (std::max)(kTaskbarMinStatusW, innerW / 6));

    int x = innerLeft;

    out.startButton = RECT{
        x,
        taskbar.top + yPad,
        (std::min)(x + kTaskbarStartW, innerRight),
        taskbar.top + yPad + itemH,
    };
    x = out.startButton.right + xPad;

    out.consoleButton = RECT{
        x,
        taskbar.top + yPad,
        (std::min)(x + kTaskbarConsoleW, innerRight),
        taskbar.top + yPad + itemH,
    };
    x = out.consoleButton.right + xPad;

    out.engineVersion = RECT{
        (std::max)(innerLeft, static_cast<int>(innerRight - versionW)),
        taskbar.top + yPad,
        innerRight,
        taskbar.top + yPad + itemH,
    };

    out.buildStatus = RECT{
        (std::max)(innerLeft, static_cast<int>(out.engineVersion.left - xPad - statusW)),
        taskbar.top + yPad,
        (std::max)(innerLeft, static_cast<int>(out.engineVersion.left - xPad)),
        taskbar.top + yPad + itemH,
    };

    const int recentRightLimit = (std::max)(x, static_cast<int>(out.buildStatus.left - xPad));
    const int availableRecent = recentRightLimit - x;
    const int recentW = (std::min)(kTaskbarRecentW, availableRecent);
    out.recentEditorButton = RECT{
        x,
        taskbar.top + yPad,
        x + (std::max)(0, recentW),
        taskbar.top + yPad + itemH,
    };

    out.spacer = RECT{
        out.recentEditorButton.right + xPad,
        taskbar.top + yPad,
        out.buildStatus.left - xPad,
        taskbar.top + yPad + itemH,
    };

    if (out.spacer.right <= out.spacer.left || availableRecent < kTaskbarMinRecentW) {
        SetRectEmpty(&out.spacer);
    }
    if (availableRecent < kTaskbarMinRecentW) {
        SetRectEmpty(&out.recentEditorButton);
    }
    if (out.buildStatus.right <= out.buildStatus.left) {
        SetRectEmpty(&out.buildStatus);
    }
    if (out.engineVersion.right <= out.engineVersion.left) {
        SetRectEmpty(&out.engineVersion);
    }

    return out;
}

[[nodiscard]] inline std::array<DesktopIconLayout, kDesktopIconCount>
ComputeDesktopIcons(const RECT& iconGrid) {
    std::array<DesktopIconLayout, kDesktopIconCount> icons{};

    int index = 0;

    for (int row = 0; row < kDesktopIconRows; ++row) {
        for (int col = 0; col < kDesktopIconCols; ++col) {
            const int x = iconGrid.left + col * (kDesktopIconW + kDesktopIconGapX);
            const int y = iconGrid.top + row * (kDesktopIconH + kDesktopIconGapY);

            RECT hitRect{
                x,
                y,
                x + kDesktopIconW,
                y + kDesktopIconH,
            };

            const int iconX = x + (kDesktopIconW - kDesktopIconImage) / 2;
            RECT iconRect{
                iconX,
                y + 8,
                iconX + kDesktopIconImage,
                y + 8 + kDesktopIconImage,
            };

            RECT labelRect{
                x + 4,
                iconRect.bottom + 4,
                x + kDesktopIconW - 4,
                iconRect.bottom + 4 + kDesktopIconLabelH,
            };

            icons[static_cast<std::size_t>(index++)] = DesktopIconLayout{
                hitRect,
                iconRect,
                labelRect,
            };
        }
    }

    return icons;
}

[[nodiscard]] inline WorkshopFrameLayout ComputeWorkshopFrameLayout(
    const RECT& client,
    bool showStartMenu
) {
    WorkshopFrameLayout out{};

    const int clientW = LayoutRectWidth(client);

    const int outerMargin = std::clamp(clientW / 64, 16, kOuterMargin);
    const int gutter = std::clamp(clientW / 110, 10, kGutter);

    out.taskbar = RECT{
        client.left + outerMargin,
        client.bottom - outerMargin - kTaskbarH,
        client.right - outerMargin,
        client.bottom - outerMargin,
    };

    out.canvas = RECT{
        client.left + outerMargin,
        client.top + kCanvasTopInset,
        client.right - outerMargin,
        out.taskbar.top - gutter,
    };

    const int canvasH = LayoutRectHeight(out.canvas);
    const int headerH = std::clamp(canvasH / 9, kHeaderMin, kHeaderMax);

    out.header = RECT{
        out.canvas.left,
        out.canvas.top,
        out.canvas.right,
        out.canvas.top + headerH,
    };

    out.logoPlate = RECT{
        out.header.left + 14,
        out.header.top + 10,
        out.header.left + 156,
        out.header.bottom - 10,
    };

    out.workspaceTitle = RECT{
        out.logoPlate.right + 12,
        out.header.top + 10,
        out.header.right - 286,
        out.header.bottom - 10,
    };

    out.headerActions = RECT{
        out.header.right - 270,
        out.header.top + 12,
        out.header.right - 16,
        out.header.bottom - 12,
    };

    const int headerToDesktopGap = (std::max)(gutter + 10, 22);
    out.desktop = RECT{
        out.canvas.left,
        out.header.bottom + headerToDesktopGap,
        out.canvas.right,
        out.canvas.bottom,
    };

    const int widgetW = std::clamp(
        LayoutRectWidth(out.desktop) * 30 / 100,
        kRecentPanelMinW,
        kRecentPanelMaxW
    );

    out.widgetColumn = RECT{
        out.desktop.right - widgetW,
        out.desktop.top,
        out.desktop.right,
        out.desktop.bottom,
    };

    out.desktopIconsArea = RECT{
        out.desktop.left,
        out.desktop.top,
        out.widgetColumn.left - gutter,
        out.desktop.bottom,
    };

    const int gridW = DesktopIconGridPixelWidth();
    const int gridH = DesktopIconGridPixelHeight();

    const int gridX = out.desktopIconsArea.left + 16;
    const int gridY = out.desktopIconsArea.top + 28;

    out.iconGrid = RECT{
        gridX,
        gridY,
        gridX + gridW,
        gridY + gridH,
    };

    out.desktopIcons = ComputeDesktopIcons(out.iconGrid);

    const int wy0 = out.widgetColumn.top;
    const int colH = LayoutRectHeight(out.widgetColumn);
    int recentH = std::clamp(colH * 15 / 100, kRecentPanelMinH, kRecentPanelMaxH);
    int gamesH = std::clamp(colH * 18 / 100, 96, kGamePanelMaxH);
    int projectH = std::clamp(colH * 22 / 100, 104, kProjectPanelMaxH);
    int buildH = std::clamp(colH * 18 / 100, 90, kBuildPanelMaxH);
    constexpr int kInterPanelGutters = 4;
    const int guttersTotal = kInterPanelGutters * gutter;

    auto fixedStackHeight = [&]() {
        return recentH + gamesH + projectH + buildH + guttersTotal;
    };

    // Minimum column height can make (recent+games+project+build+gutters) exceed colH — panels then
    // overlap vertically and spill onto the taskbar strip; shrink until the fixed stack fits.
    if (fixedStackHeight() > colH) {
        int need = fixedStackHeight() - colH;
        auto shaveOverflow = [](int& h, int minH, int amount) {
            const int cut = (std::min)((std::max)(0, amount), (std::max)(0, h - minH));
            h -= cut;
            return amount - cut;
        };
        int guard = 0;
        while (need > 0 && guard < 2048) {
            ++guard;
            const int before = need;
            need = shaveOverflow(buildH, 52, need);
            need = shaveOverflow(gamesH, 52, need);
            need = shaveOverflow(projectH, 72, need);
            need = shaveOverflow(recentH, 52, need);
            if (need == before) {
                break;
            }
        }
    }
    for (int k = 0; k < 3000 && fixedStackHeight() > colH; ++k) {
        if (gamesH > 40) {
            --gamesH;
        } else if (buildH > 40) {
            --buildH;
        } else if (projectH > 64) {
            --projectH;
        } else if (recentH > 40) {
            --recentH;
        } else {
            break;
        }
    }

    // Fit the stack top-to-bottom. If the window is short, shrink diagnostics first,
    // then hide the preview rather than letting panels collide with the taskbar.
    int previewH = colH - fixedStackHeight();
    auto shave = [](int& h, int minH, int amount) {
        const int cut = (std::min)((std::max)(0, amount), (std::max)(0, h - minH));
        h -= cut;
        return amount - cut;
    };
    if (previewH < kPreviewPanelMinH) {
        int need = kPreviewPanelMinH - previewH;
        need = shave(buildH, 72, need);
        need = shave(gamesH, 78, need);
        need = shave(projectH, 92, need);
        need = shave(recentH, 72, need);
        previewH = colH - fixedStackHeight();
    }
    if (previewH < 92) {
        previewH = 0;
    } else {
        previewH = (std::min)(previewH, kPreviewPanelMaxH);
    }

    int wy = wy0;

    out.recentPanel = RECT{
        out.widgetColumn.left,
        wy,
        out.widgetColumn.right,
        wy + recentH,
    };
    wy = out.recentPanel.bottom + gutter;

    out.gameCardsPanel = RECT{
        out.widgetColumn.left,
        wy,
        out.widgetColumn.right,
        wy + gamesH,
    };
    wy = out.gameCardsPanel.bottom + gutter;

    out.projectPanel = RECT{
        out.widgetColumn.left,
        wy,
        out.widgetColumn.right,
        wy + projectH,
    };
    wy = out.projectPanel.bottom + gutter;

    out.buildPanel = RECT{
        out.widgetColumn.left,
        wy,
        out.widgetColumn.right,
        wy + buildH,
    };
    wy = out.buildPanel.bottom + gutter;

    if (previewH > 0) {
        out.previewPanel = RECT{
            out.widgetColumn.left,
            wy,
            out.widgetColumn.right,
            (std::min)(wy + previewH, static_cast<int>(out.widgetColumn.bottom)),
        };
    } else {
        SetRectEmpty(&out.previewPanel);
    }

    out.taskbarItems = ComputeTaskbarLayout(out.taskbar);

    if (showStartMenu) {
        out.startMenuPanel = RECT{
            out.taskbar.left + 8,
            out.taskbar.top - kStartMenuH - 8,
            out.taskbar.left + 8 + kStartMenuW,
            out.taskbar.top - 8,
        };
    } else {
        SetRectEmpty(&out.startMenuPanel);
    }

    return out;
}

} // namespace ri::vshell::layout
