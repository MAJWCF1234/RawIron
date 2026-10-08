#include "Apps/RawIron.Editor/src/EditorStructuralPicker.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include "Apps/RawIron.Editor/src/EditorRenderer.h"
#include "RawIron/Render/PreviewTexture.h"
#include <stdexcept>

// Optional artifact mode uses the actual GDI palette renderer without touching a user's editor window.
void CapturePalette(const char* path) {
    using namespace ri::editor;
    constexpr int width = 1160, height = 300;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void* pixels = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!dc || !bitmap || !pixels) throw std::runtime_error("palette capture allocation failed");
    const auto oldBitmap = SelectObject(dc, bitmap);
    HFONT small = CreateFontW(-12,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");
    HFONT header = CreateFontW(-15,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,L"Bahnschrift SemiBold");
    StructuralThumbnailCache cache;
    const auto layout = ComputeStructuralPickerLayout(RECT{0,0,width,height}, AuthoringCatalogSection::Structural, 0, "arch");
    for (const auto& cell : layout.cells) cache.Prewarm(AuthoringCatalogSection::Structural,cell.presetIndex,{});
    EditorRenderer::FillRectColor(dc,RECT{0,0,width,height},RGB(22,26,32));
    StructuralPickerModel model;
    model.visible = true;
    model.searchQuery = "arch";
    model.searchActive = true;
    model.selectedPresetIndex = layout.cells.front().presetIndex;
    RenderStructuralPickerOverlay(dc,layout,model,cache,{},StructuralPickerTheme{header,small,small},
        [small](HDC d,const RECT& r,const std::string& label,bool active) {EditorRenderer::DrawToolbarButton(d,r,label,active,small);});
    GdiFlush();
    ri::render::software::SoftwareImage image;
    image.width=width; image.height=height; image.pixels.resize(width*height*3);
    const auto* bgra=static_cast<const unsigned char*>(pixels);
    for (int i=0;i<width*height;++i) {
        image.pixels[i*3]=bgra[i*4+2]; image.pixels[i*3+1]=bgra[i*4+1]; image.pixels[i*3+2]=bgra[i*4];
    }
    SelectObject(dc,oldBitmap); DeleteObject(bitmap); DeleteDC(dc); DeleteObject(small); DeleteObject(header);
    if (!ri::render::software::SaveBmp(image,path)) throw std::runtime_error("palette capture save failed");
}

int main(int argc,char** argv) {
    try {
        using namespace ri::editor;
        for(const int width:{160,640,1040,1680}) {
            const RECT viewport{20,140,20+width,760};
            for(const auto section:{AuthoringCatalogSection::Structural,AuthoringCatalogSection::Volumes,
                    AuthoringCatalogSection::Logic}) {
                for(const int scroll:{0,10000}) {
                    const auto layout=ComputeStructuralPickerLayout(viewport,section,scroll);
                    if(layout.cells.empty() || layout.columns<1) throw std::runtime_error("empty catalog layout");
                    for(const auto& cell:layout.cells) {
                        if(cell.labelRect.left<layout.contentRect.left || cell.labelRect.right>layout.contentRect.right
                            || cell.labelRect.bottom>layout.contentRect.bottom)
                            throw std::runtime_error("catalog card leaves its panel");
                        // The label extends beyond the thumbnail. Every visible label edge must work.
                        const POINT labelEdge{cell.labelRect.left+2,cell.labelRect.top+2};
                        const auto hit=HitTestStructuralPicker(layout,labelEdge);
                        if(hit.kind!=StructuralPickerHitKind::Preset || hit.presetIndex!=cell.presetIndex)
                            throw std::runtime_error("catalog label is not clickable");
                    }
                }
            }
        }
        for (const auto section : {AuthoringCatalogSection::Structural, AuthoringCatalogSection::Volumes,
                AuthoringCatalogSection::Logic}) {
            const auto count = ActiveCatalogPresetCount(section);
            if (MatchingCatalogPresets(section, "   ").size() != count)
                throw std::runtime_error("blank search should retain every preset");
            for (std::size_t index = 0; index < count; ++index) {
                auto query = ActiveCatalogPresetLabel(section, index);
                std::replace(query.begin(), query.end(), '_', ' ');
                std::transform(query.begin(), query.end(), query.begin(), [](unsigned char c){return static_cast<char>(std::toupper(c));});
                const auto matches = MatchingCatalogPresets(section, query);
                if (std::find(matches.begin(), matches.end(), index) == matches.end())
                    throw std::runtime_error("case and separator normalized search lost a preset");
                const auto layout = ComputeStructuralPickerLayout(RECT{0,0,1040,760}, section, 10000, query);
                for (const auto& cell : layout.cells) {
                    if (std::find(matches.begin(), matches.end(), cell.presetIndex) == matches.end())
                        throw std::runtime_error("filtered card lost its original preset identity");
                }
            }
            const auto empty = ComputeStructuralPickerLayout(RECT{0,0,640,760}, section,10000,"zzzz_no_such_piece");
            if (!empty.cells.empty() || empty.totalRows != 0 || empty.scrollTopRow != 0)
                throw std::runtime_error("empty search failed to clamp pagination");
            if (HitTestStructuralPicker(empty, POINT{empty.placeBtn.left+2, empty.placeBtn.top+2}).kind == StructuralPickerHitKind::Place)
                throw std::runtime_error("empty catalog allows accidental placement");
            if (HitTestStructuralPicker(empty, POINT{empty.searchRect.left+2, empty.searchRect.top+2}).kind != StructuralPickerHitKind::Search)
                throw std::runtime_error("catalog search is not clickable");
        }
        if (argc == 2) CapturePalette(argv[1]);
        std::cout<<"Catalog: narrow/wide layouts, all sections, final-page clamping and label hit targets passed\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
