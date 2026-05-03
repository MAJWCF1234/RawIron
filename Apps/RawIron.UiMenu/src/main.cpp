#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Ui/UiFlowSession.h"
#include "RawIron/Ui/UiJsonIO.h"
#include "RawIron/Ui/UiPaths.h"

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

#include "RawIron/Render/PreviewTexture.h"

#include <d3d11.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace fs = std::filesystem;

namespace {

fs::path DetectWorkspaceRoot(const fs::path& start) {
    fs::path current = fs::weakly_canonical(start);
    for (int guard = 0; guard < 40; ++guard) {
        std::error_code ec{};
        if (fs::exists(current / "CMakeLists.txt", ec) && fs::exists(current / "Assets", ec)
            && fs::exists(current / "Source", ec)) {
            return current;
        }
        const fs::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return fs::weakly_canonical(start);
}

static ID3D11Device* gDevice = nullptr;
static ID3D11DeviceContext* gContext = nullptr;
static IDXGISwapChain* gSwapChain = nullptr;
static ID3D11RenderTargetView* gRenderTarget = nullptr;
static UINT gResizeWidth = 0;
static UINT gResizeHeight = 0;

struct GpuUiTexture {
    ID3D11ShaderResourceView* srv = nullptr;
    int width = 0;
    int height = 0;
};

[[nodiscard]] bool CreateSrvFromRgba(const ri::render::software::RgbaImage& img, GpuUiTexture* out) {
    *out = GpuUiTexture{};
    if (!img.Valid() || gDevice == nullptr) {
        return false;
    }
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(img.width);
    desc.Height = static_cast<UINT>(img.height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sub{};
    sub.pSysMem = img.rgba.data();
    sub.SysMemPitch = static_cast<UINT>(img.width * 4);
    ID3D11Texture2D* tex = nullptr;
    if (FAILED(gDevice->CreateTexture2D(&desc, &sub, &tex)) || tex == nullptr) {
        return false;
    }
    HRESULT hr = gDevice->CreateShaderResourceView(tex, nullptr, &out->srv);
    tex->Release();
    if (FAILED(hr) || out->srv == nullptr) {
        return false;
    }
    out->width = img.width;
    out->height = img.height;
    return true;
}

class UiMenuTextureCache {
public:
    [[nodiscard]] const GpuUiTexture* TryGet(const fs::path& absolutePath) {
        if (absolutePath.empty()) {
            return nullptr;
        }
        std::error_code ec{};
        if (!fs::is_regular_file(absolutePath, ec)) {
            return nullptr;
        }
        const std::string key = absolutePath.generic_string();
        if (const auto it = map_.find(key); it != map_.end()) {
            return &it->second;
        }
        const ri::render::software::RgbaImage rgba = ri::render::software::LoadRgbaImageFile(absolutePath);
        GpuUiTexture gpu{};
        if (!CreateSrvFromRgba(rgba, &gpu)) {
            return nullptr;
        }
        const auto inserted = map_.emplace(key, gpu);
        return &inserted.first->second;
    }

    void Clear() {
        for (auto& e : map_) {
            if (e.second.srv != nullptr) {
                e.second.srv->Release();
                e.second.srv = nullptr;
            }
        }
        map_.clear();
    }

private:
    std::unordered_map<std::string, GpuUiTexture> map_{};
};

UiMenuTextureCache gUiTextures{};
static bool gShowUiBacklog = false;
static bool gUiBacklogScrollToBottomPending = false;
static std::string gUiAdvanceVisitSig;
static double gUiAdvanceTimerZero = 0.0;
static bool gUiAdvanceTimerConsumed = false;
static bool gUiHideVnDevHints = false;

void CreateRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    gSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (backBuffer != nullptr) {
        gDevice->CreateRenderTargetView(backBuffer, nullptr, &gRenderTarget);
        backBuffer->Release();
    }
}

void CleanupRenderTarget() {
    if (gRenderTarget != nullptr) {
        gRenderTarget->Release();
        gRenderTarget = nullptr;
    }
}

bool CreateDeviceD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    constexpr D3D_FEATURE_LEVEL levels[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    const HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0U,
        levels,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &gSwapChain,
        &gDevice,
        &featureLevel,
        &gContext);
    if (FAILED(hr)) {
        return false;
    }
    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (gSwapChain != nullptr) {
        gSwapChain->Release();
        gSwapChain = nullptr;
    }
    if (gContext != nullptr) {
        gContext->Release();
        gContext = nullptr;
    }
    if (gDevice != nullptr) {
        gDevice->Release();
        gDevice = nullptr;
    }
}

/// Ren'Py-style `"{variable}"` interpolation using the JSON manifest string store (`variables`, `setVar`, etc.).
[[nodiscard]] std::string SubstituteStoreVars(const ri::ui::UiFlowSession& session, std::string_view input) {
    std::string out;
    out.reserve(input.size() + 24U);
    for (std::size_t i = 0; i < input.size();) {
        if (input[i] == '$' && i + 1U < input.size() && input[i + 1U] == '{') {
            std::size_t j = i + 2U;
            while (j < input.size()) {
                const unsigned char ch = static_cast<unsigned char>(input[j]);
                if (std::isalnum(ch) != 0 || input[j] == '_') {
                    ++j;
                    continue;
                }
                break;
            }
            if (j < input.size() && input[j] == '}') {
                const std::string_view id = input.substr(i + 2U, j - (i + 2U));
                out.append(session.GetVariableValue(id));
                i = j + 1U;
                continue;
            }
        }
        out.push_back(input[i]);
        ++i;
    }
    return out;
}

[[nodiscard]] fs::path ResolveWorkspaceAssetPath(const ri::ui::UiFlowSession& session,
                                                 const fs::path& workspaceRoot,
                                                 const std::string& relativeRaw) {
    if (relativeRaw.empty()) {
        return {};
    }
    const std::string resolved = SubstituteStoreVars(session, relativeRaw);
    return (workspaceRoot / fs::path(resolved)).lexically_normal();
}

void ApplyEmit(std::string_view id, bool* quitOut, bool* playOut) {
    if (id == "app.quit") {
        *quitOut = true;
        PostQuitMessage(0);
        return;
    }
    if (id == "game.start") {
        *playOut = true;
        ri::core::LogInfo(std::string("[UiMenu] emit action: ") + std::string(id));
        return;
    }
    ri::core::LogInfo(std::string("[UiMenu] unhandled emit: ") + std::string(id));
}

void DrawAlignedParagraph(std::string_view text, std::string_view align, float wrapWidth) {
    ImVec2 pos = ImGui::GetCursorPos();
    const float region = ImGui::GetContentRegionAvail().x;
    if (align == "center") {
        const ImVec2 sz = ImGui::CalcTextSize(text.data(), text.data() + text.size(), false, wrapWidth);
        ImGui::SetCursorPosX(pos.x + std::max(0.0f, (region - sz.x) * 0.5f));
    } else if (align == "right") {
        const ImVec2 sz = ImGui::CalcTextSize(text.data(), text.data() + text.size(), false, wrapWidth);
        ImGui::SetCursorPosX(pos.x + std::max(0.0f, region - sz.x));
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrapWidth);
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
}

void DrawUiSession(ri::ui::UiFlowSession& session,
                   const fs::path& workspaceRoot,
                   bool* quitRequested,
                   bool* playRequested,
                   bool allowKeyboardShortcuts) {
    const ri::ui::UiScreen* screen = session.CurrentScreen();
    if (screen == nullptr) {
        ImGui::TextUnformatted("No active UI screen.");
        return;
    }

    const std::array<float, 4>& bg = screen->backgroundRgba;
    const fs::path bgPath = screen->backgroundImageRelative.empty()
        ? fs::path{}
        : ResolveWorkspaceAssetPath(session, workspaceRoot, screen->backgroundImageRelative);
    const GpuUiTexture* bgGpu = screen->backgroundImageRelative.empty() ? nullptr : gUiTextures.TryGet(bgPath);
    const bool drawBgPhoto = bgGpu != nullptr && bgGpu->srv != nullptr;

    if (drawBgPhoto) {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(bg[0], bg[1], bg[2], bg[3]));
    }
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("RawIronUiRoot",
                 nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                     | ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (drawBgPhoto) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p0 = ImGui::GetWindowPos();
        const ImVec2 p1 = ImVec2(p0.x + ImGui::GetWindowSize().x, p0.y + ImGui::GetWindowSize().y);
        dl->AddImage(
            reinterpret_cast<ImTextureID>(bgGpu->srv), p0, p1, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), IM_COL32_WHITE);
        const ImU32 overlay = IM_COL32(
            static_cast<int>(std::clamp(bg[0] * 255.0f, 0.0f, 255.0f)),
            static_cast<int>(std::clamp(bg[1] * 255.0f, 0.0f, 255.0f)),
            static_cast<int>(std::clamp(bg[2] * 255.0f, 0.0f, 255.0f)),
            static_cast<int>(std::clamp(bg[3] * 255.0f, 0.0f, 255.0f)));
        dl->AddRectFilled(p0, p1, overlay);
    }

    const float wrap = ImGui::GetContentRegionAvail().x - 24.0f;
    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    auto historyFingerprintBase = [&screen](std::size_t blockIndex) {
        std::string s = screen->id;
        s.push_back('#');
        s += std::to_string(blockIndex);
        return s;
    };
    auto tryHistorySay = [&](std::size_t blockIndex, const ri::ui::UiBlock& blk, const std::string& sp,
                             const std::string& tx, const std::string& voice) {
        if (!blk.rememberInHistory || tx.empty()) {
            return;
        }
        std::string fp = historyFingerprintBase(blockIndex);
        fp.push_back('|');
        fp.append(sp);
        fp.push_back('|');
        fp.append(tx);
        fp.push_back('|');
        fp.append(voice);
        fp.push_back('S');
        session.MaybeAppendHistory(
            fp,
            ri::ui::UiHistoryLine{.speaker = sp, .text = tx, .narration = false, .chapterMarker = false, .voiceCue = voice});
    };
    auto tryHistoryNarration = [&](std::size_t blockIndex, const ri::ui::UiBlock& blk, const std::string& tx) {
        if (!blk.rememberInHistory || tx.empty()) {
            return;
        }
        std::string fp = historyFingerprintBase(blockIndex);
        fp.push_back('|');
        fp.append(tx);
        fp.push_back('N');
        session.MaybeAppendHistory(fp, ri::ui::UiHistoryLine{.speaker = {}, .text = tx, .narration = true});
    };
    auto tryHistoryChapter = [&](std::size_t blockIndex, const ri::ui::UiBlock& blk, const std::string& tx) {
        if (!blk.rememberInHistory || tx.empty()) {
            return;
        }
        std::string fp = historyFingerprintBase(blockIndex);
        fp.push_back('|');
        fp.append(tx);
        fp.push_back('C');
        session.MaybeAppendHistory(
            fp,
            ri::ui::UiHistoryLine{.speaker = {}, .text = tx, .narration = false, .chapterMarker = true});
    };

    std::string visitSig;
    visitSig.reserve(session.Stack().size() * 24U);
    for (const std::string& sid : session.Stack()) {
        visitSig.append(sid);
        visitSig.push_back('\x1e');
    }
    if (visitSig != gUiAdvanceVisitSig) {
        gUiAdvanceVisitSig = std::move(visitSig);
        gUiAdvanceTimerZero = ImGui::GetTime();
        gUiAdvanceTimerConsumed = false;
    }

    for (std::size_t blockIndex = 0; blockIndex < screen->blocks.size(); ++blockIndex) {
        const ri::ui::UiBlock& block = screen->blocks[blockIndex];
        if (!session.IsBlockVisible(block)) {
            continue;
        }
        switch (block.kind) {
            case ri::ui::UiBlockKind::Heading: {
                const std::string headingText = SubstituteStoreVars(session, block.text);
                const char* t = headingText.c_str();
                const ImVec2 ts = ImGui::CalcTextSize(t);
                const float region = ImGui::GetContentRegionAvail().x;
                if (block.align == "center" || block.align.empty()) {
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (region - ts.x) * 0.5f));
                }
                ImGui::SetWindowFontScale(1.35f);
                ImGui::TextUnformatted(t);
                ImGui::SetWindowFontScale(1.0f);
                ImGui::Spacing();
                break;
            }
            case ri::ui::UiBlockKind::Paragraph:
            case ri::ui::UiBlockKind::Label: {
                const std::string body = SubstituteStoreVars(session, block.text);
                DrawAlignedParagraph(body,
                                     block.align.empty() ? "left" : block.align,
                                     std::max(100.0f, wrap));
                break;
            }
            case ri::ui::UiBlockKind::Spacer:
                ImGui::Dummy(ImVec2(1.0f, std::max(1.0f, block.spacerHeight)));
                break;
            case ri::ui::UiBlockKind::Separator:
                ImGui::Separator();
                break;
            case ri::ui::UiBlockKind::Button: {
                const std::string buttonLabel = SubstituteStoreVars(session, block.label);
                const float region = ImGui::GetContentRegionAvail().x;
                const float bw = std::min(280.0f, std::max(120.0f, region * 0.45f));
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (region - bw) * 0.5f));
                if (ImGui::Button(buttonLabel.c_str(), ImVec2(bw, 0.0f))) {
                    session.ApplyAction(
                        block.action,
                        [quitRequested, playRequested](std::string_view id) {
                            ApplyEmit(id, quitRequested, playRequested);
                        });
                }
                ImGui::Spacing();
                break;
            }
            case ri::ui::UiBlockKind::Say: {
                ImGui::PushID(&block);
                const std::string saySpeaker = SubstituteStoreVars(session, block.speaker);
                const std::string sayText = SubstituteStoreVars(session, block.text);
                const std::string sayVoice = SubstituteStoreVars(session, block.voiceHint);
                const fs::path portraitPath =
                    block.portraitRelativePath.empty() ? fs::path{}
                                                       : ResolveWorkspaceAssetPath(session,
                                                                                   workspaceRoot,
                                                                                   block.portraitRelativePath);
                const GpuUiTexture* portraitGpu =
                    block.portraitRelativePath.empty() ? nullptr : gUiTextures.TryGet(portraitPath);
                const bool portraitRight = (block.portraitSide == "right");
                const ImVec2 portraitSize(168.0f, 252.0f);
                const float portraitLane = portraitSize.x + 14.0f;
                const float sayBodyW = std::max(160.0f, wrap - portraitLane);

                auto drawSayBody = [&](float bodyW) {
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.08f, 0.11f, 0.94f));
                    ImGui::BeginChild("say_text",
                                      ImVec2(bodyW, 0.0f),
                                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
                    if (!saySpeaker.empty()) {
                        ImGui::TextColored(ImVec4(0.55f, 0.78f, 1.0f, 1.0f), "%s", saySpeaker.c_str());
                        ImGui::Spacing();
                    }
                    DrawAlignedParagraph(sayText,
                                         block.align.empty() ? "left" : block.align,
                                         std::max(80.0f, bodyW - 20.0f));
                    if (!sayVoice.empty()) {
                        ImGui::Spacing();
                        ImGui::TextDisabled("Voice: %s", sayVoice.c_str());
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar();
                };

                if (portraitGpu != nullptr && portraitGpu->srv != nullptr) {
                    if (portraitRight) {
                        drawSayBody(sayBodyW);
                        ImGui::SameLine();
                        ImGui::Image(reinterpret_cast<ImTextureID>(portraitGpu->srv), portraitSize);
                    } else {
                        ImGui::Image(reinterpret_cast<ImTextureID>(portraitGpu->srv), portraitSize);
                        ImGui::SameLine();
                        drawSayBody(sayBodyW);
                    }
                } else {
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.08f, 0.11f, 0.94f));
                    ImGui::BeginChild("say_panel",
                                      ImVec2(0.0f, 0.0f),
                                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
                    if (!saySpeaker.empty()) {
                        ImGui::TextColored(ImVec4(0.55f, 0.78f, 1.0f, 1.0f), "%s", saySpeaker.c_str());
                        ImGui::Spacing();
                    }
                    DrawAlignedParagraph(sayText,
                                         block.align.empty() ? "left" : block.align,
                                         std::max(100.0f, wrap));
                    if (!sayVoice.empty()) {
                        ImGui::Spacing();
                        ImGui::TextDisabled("Voice: %s", sayVoice.c_str());
                    }
                    if (!block.portraitRelativePath.empty()) {
                        ImGui::Spacing();
                        ImGui::TextDisabled("Portrait (%s): %s",
                                            block.portraitSide.empty() ? "side" : block.portraitSide.c_str(),
                                            portraitPath.generic_string().c_str());
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar();
                }
                ImGui::PopID();
                ImGui::Spacing();
                tryHistorySay(blockIndex, block, saySpeaker, sayText, sayVoice);
                break;
            }
            case ri::ui::UiBlockKind::Narration: {
                const std::string narration = SubstituteStoreVars(session, block.text);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.76f, 0.82f, 1.0f));
                DrawAlignedParagraph(narration,
                                     block.align.empty() ? "center" : block.align,
                                     std::max(100.0f, wrap));
                ImGui::PopStyleColor();
                tryHistoryNarration(blockIndex, block, narration);
                break;
            }
            case ri::ui::UiBlockKind::HistoryNote: {
                const std::string noteText = SubstituteStoreVars(session, block.text);
                tryHistoryChapter(blockIndex, block, noteText);
                if (!block.historyBacklogOnly) {
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.48f, 0.52f, 0.6f, 1.0f));
                    DrawAlignedParagraph(noteText,
                                         block.align.empty() ? "center" : block.align,
                                         std::max(100.0f, wrap));
                    ImGui::PopStyleColor();
                    ImGui::Separator();
                }
                break;
            }
            case ri::ui::UiBlockKind::Choices: {
                ImGui::Spacing();
                for (const ri::ui::UiChoiceItem& choice : block.choices) {
                    if (choice.label.empty() || !session.IsChoiceVisible(choice)) {
                        continue;
                    }
                    const std::string choiceLabel = SubstituteStoreVars(session, choice.label);
                    if (ImGui::Button(choiceLabel.c_str(), ImVec2(-FLT_MIN, 0.0f))) {
                        session.ApplyAction(
                            choice.action,
                            [quitRequested, playRequested](std::string_view id) {
                                ApplyEmit(id, quitRequested, playRequested);
                            });
                    }
                }
                ImGui::Spacing();
                break;
            }
            case ri::ui::UiBlockKind::Image: {
                ImGui::PushID(&block);
                const float h = block.imageHeightHint > 1.0f ? block.imageHeightHint : 140.0f;
                const fs::path imagePath =
                    block.imageRelativePath.empty()
                        ? fs::path{}
                        : ResolveWorkspaceAssetPath(session, workspaceRoot, block.imageRelativePath);
                const GpuUiTexture* imageGpu =
                    block.imageRelativePath.empty() ? nullptr : gUiTextures.TryGet(imagePath);
                if (imageGpu != nullptr && imageGpu->srv != nullptr && imageGpu->height > 0) {
                    const float aspect =
                        static_cast<float>(imageGpu->width) / static_cast<float>(imageGpu->height);
                    const float dispW = std::max(32.0f, h * aspect);
                    ImGui::Image(reinterpret_cast<ImTextureID>(imageGpu->srv), ImVec2(dispW, h));
                    if (!block.imageAnchor.empty()) {
                        ImGui::SameLine();
                        ImGui::TextDisabled("(%s)", block.imageAnchor.c_str());
                    }
                } else {
                    ImGui::BeginChild("image_panel", ImVec2(-FLT_MIN, h), ImGuiChildFlags_Borders);
                    ImGui::TextDisabled("Image");
                    if (!block.imageRelativePath.empty()) {
                        ImGui::TextWrapped("%s", block.imageRelativePath.c_str());
                    } else {
                        ImGui::TextDisabled("(no path)");
                    }
                    if (!block.imageAnchor.empty()) {
                        ImGui::TextDisabled("anchor: %s", block.imageAnchor.c_str());
                    }
                    ImGui::EndChild();
                }
                ImGui::PopID();
                ImGui::Spacing();
                break;
            }
            default:
                break;
        }
    }

    if (!gUiHideVnDevHints
        && (!screen->musicHint.empty() || (!screen->backgroundImageRelative.empty() && !drawBgPhoto))) {
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::Separator();
        if (!screen->backgroundImageRelative.empty() && !drawBgPhoto) {
            const std::string bgShown = SubstituteStoreVars(session, screen->backgroundImageRelative);
            ImGui::TextDisabled("Background image (missing or unloadable): %s", bgShown.c_str());
        }
        if (!screen->musicHint.empty()) {
            ImGui::TextDisabled("Music: %s", screen->musicHint.c_str());
        }
    }

    bool didLineAdvance = false;
    if (screen->advanceAction.kind != ri::ui::UiActionKind::None) {
        bool advance = false;
        if (screen->advanceOnSpace && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
            advance = true;
        }
        if (!advance && screen->advanceOnClick && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && !ImGui::IsAnyItemHovered()) {
            advance = true;
        }
        if (!advance && screen->advanceOnMouseWheel && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)
            && !ImGui::IsAnyItemHovered() && ImGui::GetIO().MouseWheel != 0.0f) {
            advance = true;
        }
        if (!advance && screen->advanceOnEnter && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && !ImGui::GetIO().WantTextInput
            && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
            advance = true;
        }
        if (!advance && !gUiAdvanceTimerConsumed && screen->advanceAfterSeconds > 0.0f) {
            const bool skipTimer = ImGui::GetIO().KeyCtrl;
            const double needSec =
                skipTimer ? std::min(0.12, static_cast<double>(screen->advanceAfterSeconds))
                          : static_cast<double>(screen->advanceAfterSeconds);
            if (ImGui::GetTime() - gUiAdvanceTimerZero >= needSec) {
                advance = true;
            }
        }
        if (advance) {
            gUiAdvanceTimerConsumed = true;
            session.ApplyAction(
                screen->advanceAction,
                [quitRequested, playRequested](std::string_view id) {
                    ApplyEmit(id, quitRequested, playRequested);
                });
            didLineAdvance = true;
        }
    }

    if (allowKeyboardShortcuts && ImGui::IsKeyPressed(ImGuiKey_B, false) && !ImGui::GetIO().WantTextInput) {
        gShowUiBacklog = !gShowUiBacklog;
        if (gShowUiBacklog) {
            gUiBacklogScrollToBottomPending = true;
        }
    }
    if (allowKeyboardShortcuts && ImGui::IsKeyPressed(ImGuiKey_H, false) && !ImGui::GetIO().WantTextInput) {
        gUiHideVnDevHints = !gUiHideVnDevHints;
    }

    if (allowKeyboardShortcuts && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            if (!session.GoBack()) {
                *quitRequested = true;
                PostQuitMessage(0);
            }
        }
        if (!didLineAdvance
            && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
            for (std::size_t bi = 0; bi < screen->blocks.size(); ++bi) {
                const ri::ui::UiBlock& block = screen->blocks[bi];
                if (!session.IsBlockVisible(block)) {
                    continue;
                }
                if (block.kind == ri::ui::UiBlockKind::Button
                    && SubstituteStoreVars(session, block.label) == "Continue") {
                    session.ApplyAction(
                        block.action,
                        [quitRequested, playRequested](std::string_view id) {
                            ApplyEmit(id, quitRequested, playRequested);
                        });
                    break;
                }
            }
        }
        if (!didLineAdvance && !ImGui::GetIO().WantTextInput) {
            int picked = -1;
            static const ImGuiKey kChoiceKeys[9] = {ImGuiKey_1,
                                                      ImGuiKey_2,
                                                      ImGuiKey_3,
                                                      ImGuiKey_4,
                                                      ImGuiKey_5,
                                                      ImGuiKey_6,
                                                      ImGuiKey_7,
                                                      ImGuiKey_8,
                                                      ImGuiKey_9};
            for (int n = 0; n < 9; ++n) {
                if (ImGui::IsKeyPressed(kChoiceKeys[n], false)) {
                    picked = n;
                    break;
                }
            }
            if (picked >= 0) {
                std::vector<const ri::ui::UiChoiceItem*> visibleChoices;
                visibleChoices.reserve(16U);
                for (const ri::ui::UiBlock& b : screen->blocks) {
                    if (!session.IsBlockVisible(b) || b.kind != ri::ui::UiBlockKind::Choices) {
                        continue;
                    }
                    for (const ri::ui::UiChoiceItem& ch : b.choices) {
                        if (!ch.label.empty() && session.IsChoiceVisible(ch)) {
                            visibleChoices.push_back(&ch);
                        }
                    }
                }
                if (picked < static_cast<int>(visibleChoices.size())) {
                    session.ApplyAction(
                        visibleChoices[static_cast<std::size_t>(picked)]->action,
                        [quitRequested, playRequested](std::string_view id) {
                            ApplyEmit(id, quitRequested, playRequested);
                        });
                }
            }
        }
    }

    ImGui::End();
    ImGui::PopStyleColor();

    if (gShowUiBacklog) {
        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(520.0f, 400.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Backlog",
                      &gShowUiBacklog,
                      ImGuiWindowFlags_None);
        ImGui::TextDisabled("Read lines so far (Ren'Py-style history). Press B to toggle.");
        ImGui::Separator();
        if (session.History().empty()) {
            ImGui::TextUnformatted("No remembered lines yet.");
        } else {
            ImGui::BeginChild("backlog_scroll",
                              ImVec2(0.0f, 0.0f),
                              ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_AlwaysVerticalScrollbar);
            const float backlogWrap = std::max(80.0f, ImGui::GetContentRegionAvail().x - 8.0f);
            for (const ri::ui::UiHistoryLine& line : session.History()) {
                if (line.chapterMarker) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.42f, 0.48f, 0.58f, 1.0f));
                    DrawAlignedParagraph(line.text, "center", backlogWrap);
                    ImGui::PopStyleColor();
                    ImGui::Separator();
                    continue;
                }
                if (line.narration) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.76f, 0.82f, 1.0f));
                    ImGui::TextWrapped("%s", line.text.c_str());
                    ImGui::PopStyleColor();
                } else {
                    ImGui::TextColored(ImVec4(0.55f, 0.78f, 1.0f, 1.0f), "%s", line.speaker.c_str());
                    ImGui::TextWrapped("%s", line.text.c_str());
                    if (!line.voiceCue.empty()) {
                        ImGui::TextDisabled("Voice: %s", line.voiceCue.c_str());
                    }
                }
                ImGui::Spacing();
            }
            if (gUiBacklogScrollToBottomPending) {
                ImGui::SetScrollY(ImGui::GetScrollMaxY());
                gUiBacklogScrollToBottomPending = false;
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
}

} // namespace

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

namespace {

LRESULT WINAPI ProcessWnd(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp) != 0) {
        return true;
    }
    switch (msg) {
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) {
                gResizeWidth = LOWORD(lp);
                gResizeHeight = HIWORD(lp);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

int main(int argc, char** argv) {
    ri::core::CommandLine commandLine(argc, argv);
    if (commandLine.HasFlag("--help") || commandLine.HasFlag("-h")) {
        std::fprintf(stderr,
                     "RawIron.UiMenu — JSON-driven title / main menu / credits (Dear ImGui + Direct3D 11).\n"
                     "  --workspace=<path>     Repository root (default: auto-detect)\n"
                     "  --manifest=<path>      UI JSON file (default: <workspace>/Assets/UI/default_menu.ui.json)\n"
                     "  --demo-vn              Load branching dialogue sample (Assets/UI/visual_novel_demo.ui.json)\n"
                     "  --headless             Load manifest and exit (CI)\n"
                     "Text: use ${varName} in JSON strings; values come from manifest \"variables\" + setVar actions.\n"
                     "In the window: B backlog, H hide music/bg dev strip, 1-9 activate visible choices in order.\n"
                     "advance.onMouseWheel; hold Ctrl to shorten delaySeconds; ${...} in portrait/image/bg paths.\n"
                     "say.voice: voice-line cue (shown in UI + backlog; playback is engine TBD).\n");
        return 0;
    }

    const fs::path workspace =
        commandLine.GetValue("--workspace").has_value()
            ? fs::weakly_canonical(fs::path(*commandLine.GetValue("--workspace")))
            : DetectWorkspaceRoot(fs::current_path());

    fs::path manifestPath = ri::ui::DefaultUiManifestPath(workspace);
    if (commandLine.HasFlag("--demo-vn")) {
        manifestPath = ri::ui::VisualNovelDemoManifestPath(workspace);
    }
    if (const auto m = commandLine.GetValue("--manifest"); m.has_value() && !m->empty()) {
        const fs::path userPath(*m);
        manifestPath = userPath.is_absolute() ? userPath : workspace / userPath;
    }

    ri::ui::UiManifest manifest{};
    std::string err;
    if (!ri::ui::TryLoadUiManifestFromJsonFile(manifestPath, manifest, &err)) {
        ri::core::LogInfo("[UiMenu] Failed to load manifest: " + err);
        return 1;
    }

    ri::core::LogInfo("[UiMenu] manifest loaded: " + manifestPath.string() + " screens=" +
                      std::to_string(manifest.screens.size()));

    if (commandLine.HasFlag("--headless")) {
        return 0;
    }

    ri::ui::UiFlowSession session{};
    session.Reset(manifest);

    constexpr int width = 960;
    constexpr int height = 540;
    const bool demoVn = commandLine.HasFlag("--demo-vn");

    const HINSTANCE inst = GetModuleHandleW(nullptr);
    const wchar_t* wc = L"RawIronUiMenuWnd";
    WNDCLASSEXW wcDesc{};
    wcDesc.cbSize = sizeof(wcDesc);
    wcDesc.style = CS_CLASSDC;
    wcDesc.lpfnWndProc = ProcessWnd;
    wcDesc.hInstance = inst;
    wcDesc.lpszClassName = wc;
    RegisterClassExW(&wcDesc);

    HWND hwnd = CreateWindowExW(
        0,
        wc,
        demoVn ? L"RawIron — VN demo (JSON UI)" : L"RawIron — Menu (JSON UI)",
        WS_OVERLAPPEDWINDOW,
        100,
        100,
        width,
        height,
        nullptr,
        nullptr,
        inst,
        nullptr);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        DestroyWindow(hwnd);
        UnregisterClassW(wc, inst);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(gDevice, gContext);

    bool quitRequested = false;
    bool playRequested = false;

    bool done = false;
    while (!done) {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                done = true;
            }
        }
        if (done) {
            break;
        }

        if (gResizeWidth != 0 && gResizeHeight != 0) {
            CleanupRenderTarget();
            gSwapChain->ResizeBuffers(0, gResizeWidth, gResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            gResizeWidth = gResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        DrawUiSession(session, workspace, &quitRequested, &playRequested, true);

        ImGui::Render();
        const float clear[4] = {0.08f, 0.08f, 0.1f, 1.0f};
        gContext->OMSetRenderTargets(1, &gRenderTarget, nullptr);
        gContext->ClearRenderTargetView(gRenderTarget, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        gSwapChain->Present(1, 0);

        if (quitRequested) {
            done = true;
        }
        if (playRequested) {
            done = true;
        }
    }

    gUiTextures.Clear();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc, inst);

    if (playRequested) {
        ri::core::LogInfo("[UiMenu] Play selected — hook game bootstrap here.");
    }

    return 0;
}
