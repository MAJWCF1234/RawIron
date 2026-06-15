#include "RawIron/Render/PreviewTexture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>
#endif

namespace ri::render::software {

namespace {

[[nodiscard]] std::string LowerExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext;
}

[[nodiscard]] RgbaImage LoadRgbaImageFileStb(const std::filesystem::path& path) {
    RgbaImage out{};
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        return out;
    }

    const std::streamsize size = stream.tellg();
    if (size <= 0) {
        return out;
    }

    stream.seekg(0, std::ios::beg);
    std::vector<stbi_uc> buffer(static_cast<std::size_t>(size));
    if (!stream.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return out;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(
        buffer.data(),
        static_cast<int>(buffer.size()),
        &width,
        &height,
        &channels,
        4);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        stbi_image_free(pixels);
        return out;
    }

    out.width = width;
    out.height = height;
    out.rgba.assign(pixels, pixels + static_cast<std::size_t>(width * height * 4));
    stbi_image_free(pixels);
    return out;
}

#if defined(_WIN32)

[[nodiscard]] RgbaImage LoadRgbaImageFileWic(const std::filesystem::path& path) {
    RgbaImage out{};
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool coInitialized = hr == S_OK || hr == S_FALSE;
    if (hr == RPC_E_CHANGED_MODE) {
        hr = S_OK;
    }
    if (FAILED(hr)) {
        return out;
    }

    IWICImagingFactory* factory = nullptr;
    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr) || factory == nullptr) {
        if (coInitialized) {
            CoUninitialize();
        }
        return out;
    }

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromFilename(
        path.wstring().c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (FAILED(hr) || decoder == nullptr) {
        factory->Release();
        if (coInitialized) {
            CoUninitialize();
        }
        return out;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr) || frame == nullptr) {
        decoder->Release();
        factory->Release();
        if (coInitialized) {
            CoUninitialize();
        }
        return out;
    }

    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr) || converter == nullptr) {
        frame->Release();
        decoder->Release();
        factory->Release();
        if (coInitialized) {
            CoUninitialize();
        }
        return out;
    }

    hr = converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        if (coInitialized) {
            CoUninitialize();
        }
        return out;
    }

    UINT width = 0;
    UINT height = 0;
    converter->GetSize(&width, &height);
    if (width == 0U || height == 0U) {
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        if (coInitialized) {
            CoUninitialize();
        }
        return out;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    out.width = static_cast<int>(width);
    out.height = static_cast<int>(height);
    out.rgba.resize(pixelCount * 4U);
    const UINT stride = static_cast<UINT>(width * 4U);
    hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(out.rgba.size()), out.rgba.data());

    converter->Release();
    frame->Release();
    decoder->Release();
    factory->Release();
    if (coInitialized) {
        CoUninitialize();
    }

    if (FAILED(hr)) {
        out = {};
    }
    return out;
}

#endif

} // namespace

RgbaImage LoadRgbaImageFile(const std::filesystem::path& path) {
    const std::string extension = LowerExtension(path);
#if defined(_WIN32)
    if (extension == ".tif" || extension == ".tiff") {
        RgbaImage wicImage = LoadRgbaImageFileWic(path);
        if (wicImage.Valid()) {
            return wicImage;
        }
    }
#endif

    RgbaImage stbImage = LoadRgbaImageFileStb(path);
    if (stbImage.Valid()) {
        return stbImage;
    }

#if defined(_WIN32)
    return LoadRgbaImageFileWic(path);
#else
    return {};
#endif
}

} // namespace ri::render::software
