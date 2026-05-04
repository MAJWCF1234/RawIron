#include "WorkshopForgeGpu.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace ri::shell {
namespace {

// Matches CPU `ComposeForgedBackground`; textures use DXGI_FORMAT_B8G8R8A8_UNORM (same byte order as GDI+ ARGB bitmaps).
static const char kForgeHlsl[] = R"(
Texture2D Albedo : register(t0);
Texture2D NormalMap : register(t1);
Texture2D Ambient : register(t2);
Texture2D Displacement : register(t3);
Texture2D Specular : register(t4);
SamplerState LinearSampler : register(s0);

cbuffer ForgeCB : register(b0) {
  float g_lightTime;
  float g_outW;
  float g_outH;
  uint g_hasAmbient;
  uint g_hasDisplacement;
  uint g_hasSpecular;
  float2 _padAlign;
};

float4 VSMain(uint id : SV_VertexID) : SV_POSITION {
  float2 uv = float2((id << 1) & 2, id & 2);
  return float4(uv * float2(2.f, -2.f) + float2(-1.f, 1.f), 0.f, 1.f);
}

struct PSInput {
  float4 pos : SV_POSITION;
};

float4 PSMain(PSInput pin) : SV_Target {
  // Match CPU ComposeForgedBackground: centre-sample each output pixel (integer pixel indices).
  float u = (floor(pin.pos.x) + 0.5f) / g_outW;
  float v = (floor(pin.pos.y) + 0.5f) / g_outH;

  static const float kStaticDisplacementStrength = 0.018f;

  float uu = u;
  float vv = v;
  if (g_hasDisplacement != 0u) {
    float4 disp = Displacement.Sample(LinearSampler, float2(u, v));
    float Dr = disp.z;
    float Dg = disp.y;
    uu = saturate(u + (Dr - 0.5f) * kStaticDisplacementStrength);
    vv = saturate(v + (Dg - 0.5f) * kStaticDisplacementStrength);
  }

  float4 Nsample = NormalMap.Sample(LinearSampler, float2(uu, vv));
  float Nr = Nsample.z * 255.f;
  float Ng = Nsample.y * 255.f;

  float nx = Nr / 127.5f - 1.f;
  float ny = Ng / 127.5f - 1.f;
  float nzSq = 1.f - nx * nx - ny * ny;
  float nz = nzSq > 0.f ? sqrt(nzSq) : 0.f;

  float lx = 0.45f * sin(g_lightTime * 0.31f) + 0.2f;
  float ly = 0.38f * cos(g_lightTime * 0.27f) + 0.15f;
  static const float lz = 0.74f;
  float invLLen = rsqrt(lx * lx + ly * ly + lz * lz);
  lx *= invLLen;
  ly *= invLLen;
  float lzNorm = lz * invLLen;

  static const float kx = 0.22f;
  static const float ky = -0.62f;
  static const float kz = 0.75f;
  float invKLen = rsqrt(kx * kx + ky * ky + kz * kz);

  float diff = nx * lx + ny * ly + nz * lzNorm;
  diff = diff < 0.f ? 0.f : diff;

  float diff2 = (nx * kx + ny * ky + nz * kz) * invKLen;
  diff2 = diff2 < 0.f ? 0.f : diff2;

  float hx = lx;
  float hy = ly;
  float hz = lzNorm + 1.f;
  float invHLen = rsqrt(hx * hx + hy * hy + hz * hz);
  hx *= invHLen;
  hy *= invHLen;
  hz *= invHLen;
  float specDot = nx * hx + ny * hy + nz * hz;
  specDot = specDot < 0.f ? 0.f : specDot;

  float du = u - 0.5f;
  float dv = v - 0.5f;
  float edgeDist = sqrt(du * du + dv * dv) * 1.41421356f;

  float lit = 0.14f + 0.58f * diff + 0.34f * diff2;
  lit *= 0.88f + 0.12f * (1.f - edgeDist);

  float4 Asamp = Albedo.Sample(LinearSampler, float2(uu, vv));
  float Ar = Asamp.z * 255.f;
  float Ag = Asamp.y * 255.f;
  float Ab = Asamp.x * 255.f;
  float Aa = Asamp.w * 255.f;

  float Mr = 255.f;
  float Mg = 255.f;
  float Mb = 255.f;
  if (g_hasAmbient != 0u) {
    float4 Msamp = Ambient.Sample(LinearSampler, float2(uu, vv));
    Mr = Msamp.z * 255.f;
    Mg = Msamp.y * 255.f;
    Mb = Msamp.x * 255.f;
  }

  float specMask = 0.f;
  if (g_hasSpecular != 0u) {
    float4 Ssamp = Specular.Sample(LinearSampler, float2(uu, vv));
    specMask = saturate((Ssamp.z * 0.2126f) + (Ssamp.y * 0.7152f) + (Ssamp.x * 0.0722f));
  }
  float spec = specMask * pow(specDot, 28.f) * (0.35f + 0.65f * diff);

  float fr = (Ar / 255.f) * (Mr / 255.f) * lit * 255.f + 18.f * diff2;
  float fg = (Ag / 255.f) * (Mg / 255.f) * lit * 255.f + 8.f * diff2;
  float fb = (Ab / 255.f) * (Mb / 255.f) * lit * 255.f + 2.f * diff2;

  fr += 110.f * spec;
  fg += 78.f * spec;
  fb += 42.f * spec;

  float Ob = clamp(fb, 0.f, 255.f);
  float Og = clamp(fg, 0.f, 255.f);
  float Or = clamp(fr, 0.f, 255.f);

  // Opaque plate for GDI+ compositing (PNG albedo alpha is often 0 → was washing out the whole backdrop).
  return float4(Ob / 255.f, Og / 255.f, Or / 255.f, 1.f);
}
)";

struct ForgeConstants {
    float lightTime{};
    float outW{};
    float outH{};
    UINT hasAmbient{};
    UINT hasDisplacement{};
    UINT hasSpecular{};
    float padAlign[2]{};
};
static_assert(sizeof(ForgeConstants) == 32, "cbuffer must be 32 bytes for D3D11 constant buffers");

bool g_gpuOk = false;

ID3D11Device* g_dev = nullptr;
ID3D11DeviceContext* g_ctx = nullptr;

ID3D11VertexShader* g_vs = nullptr;
ID3D11PixelShader* g_ps = nullptr;
ID3D11Buffer* g_cb = nullptr;
ID3D11SamplerState* g_sampler = nullptr;
ID3D11RasterizerState* g_raster = nullptr;
ID3D11BlendState* g_blend = nullptr;
ID3D11DepthStencilState* g_depth = nullptr;

ID3D11Texture2D* g_rt = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
ID3D11Texture2D* g_staging = nullptr;

ID3D11Texture2D* g_texAlbedo = nullptr;
ID3D11Texture2D* g_texNormal = nullptr;
ID3D11Texture2D* g_texAmbient = nullptr;
ID3D11Texture2D* g_texDisp = nullptr;
ID3D11Texture2D* g_texSpec = nullptr;

ID3D11ShaderResourceView* g_srvAlbedo = nullptr;
ID3D11ShaderResourceView* g_srvNormal = nullptr;
ID3D11ShaderResourceView* g_srvAmbient = nullptr;
ID3D11ShaderResourceView* g_srvDisp = nullptr;
ID3D11ShaderResourceView* g_srvSpec = nullptr;

int g_cachedAw = -1;
int g_cachedAh = -1;
int g_cachedNw = -1;
int g_cachedNh = -1;
int g_cachedAmbW = -1;
int g_cachedAmbH = -1;
int g_cachedDw = -1;
int g_cachedDh = -1;
int g_cachedSw = -1;
int g_cachedSh = -1;

int g_cachedRtW = -1;
int g_cachedRtH = -1;

void ReleaseSrvPair(ID3D11Texture2D** tex, ID3D11ShaderResourceView** srv) {
    if (srv != nullptr && *srv != nullptr) {
        (*srv)->Release();
        *srv = nullptr;
    }
    if (tex != nullptr && *tex != nullptr) {
        (*tex)->Release();
        *tex = nullptr;
    }
}

void GpuReleaseTextures() {
    ReleaseSrvPair(&g_texAlbedo, &g_srvAlbedo);
    ReleaseSrvPair(&g_texNormal, &g_srvNormal);
    ReleaseSrvPair(&g_texAmbient, &g_srvAmbient);
    ReleaseSrvPair(&g_texDisp, &g_srvDisp);
    ReleaseSrvPair(&g_texSpec, &g_srvSpec);
    g_cachedAw = g_cachedAh = -1;
    g_cachedNw = g_cachedNh = -1;
    g_cachedAmbW = g_cachedAmbH = -1;
    g_cachedDw = g_cachedDh = -1;
    g_cachedSw = g_cachedSh = -1;
}

void GpuReleaseRt() {
    if (g_rtv != nullptr) {
        g_rtv->Release();
        g_rtv = nullptr;
    }
    if (g_rt != nullptr) {
        g_rt->Release();
        g_rt = nullptr;
    }
    if (g_staging != nullptr) {
        g_staging->Release();
        g_staging = nullptr;
    }
    g_cachedRtW = g_cachedRtH = -1;
}

HRESULT CompileShader(const char* src,
                      size_t len,
                      const char* entry,
                      const char* profile,
                      ID3DBlob** blob) {
    ID3DBlob* errors = nullptr;
    const HRESULT hr =
        D3DCompile(src, len, nullptr, nullptr, nullptr, entry, profile,
                   D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR, 0, blob, &errors);
    if (errors != nullptr) {
        if (FAILED(hr)) {
            OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
        }
        errors->Release();
    }
    return hr;
}

HRESULT CreateTextureFromBgra(ID3D11Device* device,
                              const BYTE* rowPtr,
                              int strideBytes,
                              int width,
                              int height,
                              ID3D11Texture2D** outTex,
                              ID3D11ShaderResourceView** outSrv) {
    std::vector<BYTE> packed(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    for (int y = 0; y < height; ++y) {
        std::memcpy(packed.data() + static_cast<size_t>(y) * width * 4,
                    rowPtr + static_cast<size_t>(y) * strideBytes,
                    static_cast<size_t>(width) * 4);
    }

    D3D11_TEXTURE2D_DESC td{};
    td.Width = static_cast<UINT>(width);
    td.Height = static_cast<UINT>(height);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = packed.data();
    sd.SysMemPitch = static_cast<UINT>(width * 4);

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = device->CreateTexture2D(&td, &sd, &tex);
    if (FAILED(hr)) {
        return hr;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
    sv.Format = td.Format;
    sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sv.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView* srv = nullptr;
    hr = device->CreateShaderResourceView(tex, &sv, &srv);
    if (FAILED(hr)) {
        tex->Release();
        return hr;
    }

    *outTex = tex;
    *outSrv = srv;
    return S_OK;
}

HRESULT CreateSolidBgra(ID3D11Device* device,
                        BYTE b,
                        BYTE g,
                        BYTE r,
                        BYTE a,
                        ID3D11Texture2D** outTex,
                        ID3D11ShaderResourceView** outSrv) {
    BYTE px[4] = {b, g, r, a};
    return CreateTextureFromBgra(device, px, 4, 1, 1, outTex, outSrv);
}

HRESULT EnsureRenderTarget(ID3D11Device* device, int w, int h) {
    if (w == g_cachedRtW && h == g_cachedRtH && g_rt != nullptr && g_rtv != nullptr && g_staging != nullptr) {
        return S_OK;
    }

    GpuReleaseRt();

    D3D11_TEXTURE2D_DESC td{};
    td.Width = static_cast<UINT>(w);
    td.Height = static_cast<UINT>(h);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;

    HRESULT hr = device->CreateTexture2D(&td, nullptr, &g_rt);
    if (FAILED(hr)) {
        return hr;
    }

    hr = device->CreateRenderTargetView(g_rt, nullptr, &g_rtv);
    if (FAILED(hr)) {
        return hr;
    }

    td.BindFlags = 0;
    td.Usage = D3D11_USAGE_STAGING;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    hr = device->CreateTexture2D(&td, nullptr, &g_staging);
    if (FAILED(hr)) {
        return hr;
    }

    g_cachedRtW = w;
    g_cachedRtH = h;
    return S_OK;
}

} // namespace

bool WorkshopForgeGpuTryInit() noexcept {
    if (g_gpuOk) {
        return true;
    }

    const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL got{};

    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, 1, D3D11_SDK_VERSION,
                                   &g_dev, &got, &g_ctx);
    if (FAILED(hr) || g_dev == nullptr || g_ctx == nullptr) {
        return false;
    }

    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    hr = CompileShader(kForgeHlsl, std::strlen(kForgeHlsl), "VSMain", "vs_5_0", &vsBlob);
    if (FAILED(hr)) {
        WorkshopForgeGpuShutdown();
        return false;
    }
    hr = CompileShader(kForgeHlsl, std::strlen(kForgeHlsl), "PSMain", "ps_5_0", &psBlob);
    if (FAILED(hr)) {
        if (vsBlob != nullptr) {
            vsBlob->Release();
        }
        WorkshopForgeGpuShutdown();
        return false;
    }

    hr = g_dev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_vs);
    vsBlob->Release();
    vsBlob = nullptr;
    if (FAILED(hr)) {
        if (psBlob != nullptr) {
            psBlob->Release();
        }
        WorkshopForgeGpuShutdown();
        return false;
    }
    hr = g_dev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_ps);
    psBlob->Release();
    psBlob = nullptr;
    if (FAILED(hr)) {
        WorkshopForgeGpuShutdown();
        return false;
    }

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(ForgeConstants);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = g_dev->CreateBuffer(&bd, nullptr, &g_cb);
    if (FAILED(hr)) {
        WorkshopForgeGpuShutdown();
        return false;
    }

    D3D11_SAMPLER_DESC samp{};
    samp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.MaxAnisotropy = 1;
    samp.ComparisonFunc = D3D11_COMPARISON_NEVER;

    hr = g_dev->CreateSamplerState(&samp, &g_sampler);
    if (FAILED(hr)) {
        WorkshopForgeGpuShutdown();
        return false;
    }

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;

    hr = g_dev->CreateRasterizerState(&rd, &g_raster);
    if (FAILED(hr)) {
        WorkshopForgeGpuShutdown();
        return false;
    }

    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = g_dev->CreateBlendState(&blend, &g_blend);
    if (FAILED(hr)) {
        WorkshopForgeGpuShutdown();
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC ds{};
    ds.DepthEnable = FALSE;
    hr = g_dev->CreateDepthStencilState(&ds, &g_depth);
    if (FAILED(hr)) {
        WorkshopForgeGpuShutdown();
        return false;
    }

    g_gpuOk = true;
    return true;
}

void WorkshopForgeGpuShutdown() noexcept {
    GpuReleaseTextures();
    GpuReleaseRt();

    if (g_vs != nullptr) {
        g_vs->Release();
        g_vs = nullptr;
    }
    if (g_ps != nullptr) {
        g_ps->Release();
        g_ps = nullptr;
    }
    if (g_cb != nullptr) {
        g_cb->Release();
        g_cb = nullptr;
    }
    if (g_sampler != nullptr) {
        g_sampler->Release();
        g_sampler = nullptr;
    }
    if (g_raster != nullptr) {
        g_raster->Release();
        g_raster = nullptr;
    }
    if (g_blend != nullptr) {
        g_blend->Release();
        g_blend = nullptr;
    }
    if (g_depth != nullptr) {
        g_depth->Release();
        g_depth = nullptr;
    }
    if (g_ctx != nullptr) {
        g_ctx->Release();
        g_ctx = nullptr;
    }
    if (g_dev != nullptr) {
        g_dev->Release();
        g_dev = nullptr;
    }
    g_gpuOk = false;
}

bool WorkshopForgeGpuAvailable() noexcept {
    return g_gpuOk;
}

std::unique_ptr<Gdiplus::Bitmap> WorkshopForgeGpuCompose(int outW,
                                                         int outH,
                                                         const Gdiplus::BitmapData& albedo,
                                                         int aw,
                                                         int ah,
                                                         const Gdiplus::BitmapData* ambient,
                                                         int ambW,
                                                         int ambH,
                                                         const Gdiplus::BitmapData& normal,
                                                         int nw,
                                                         int nh,
                                                         const Gdiplus::BitmapData* displacement,
                                                         int dw,
                                                         int dh,
                                                         const Gdiplus::BitmapData* specular,
                                                         int sw,
                                                         int sh,
                                                         float lightingTime) {
    if (!g_gpuOk || g_dev == nullptr || g_ctx == nullptr || outW <= 0 || outH <= 0) {
        return nullptr;
    }

    const bool hasAmb = ambient != nullptr && ambW > 0 && ambH > 0;
    const bool hasDisp = displacement != nullptr && dw > 0 && dh > 0;
    const bool hasSpec = specular != nullptr && sw > 0 && sh > 0;

    HRESULT hr = EnsureRenderTarget(g_dev, outW, outH);
    if (FAILED(hr)) {
        return nullptr;
    }

    auto uploadIfNeeded = [&](ID3D11Texture2D** tex,
                              ID3D11ShaderResourceView** srv,
                              const Gdiplus::BitmapData& data,
                              int w,
                              int h,
                              int* cw,
                              int* ch) -> bool {
        if (w == *cw && h == *ch && *tex != nullptr && *srv != nullptr) {
            return true;
        }
        ReleaseSrvPair(tex, srv);
        const BYTE* scan = static_cast<const BYTE*>(data.Scan0);
        const int stride = static_cast<int>(data.Stride);
        hr = CreateTextureFromBgra(g_dev, scan, stride, w, h, tex, srv);
        if (FAILED(hr)) {
            return false;
        }
        *cw = w;
        *ch = h;
        return true;
    };

    if (!uploadIfNeeded(&g_texAlbedo, &g_srvAlbedo, albedo, aw, ah, &g_cachedAw, &g_cachedAh)) {
        return nullptr;
    }
    if (!uploadIfNeeded(&g_texNormal, &g_srvNormal, normal, nw, nh, &g_cachedNw, &g_cachedNh)) {
        return nullptr;
    }

    if (hasAmb) {
        if (!uploadIfNeeded(&g_texAmbient, &g_srvAmbient, *ambient, ambW, ambH, &g_cachedAmbW, &g_cachedAmbH)) {
            return nullptr;
        }
    } else {
        if (g_srvAmbient == nullptr || g_cachedAmbW != 1) {
            ReleaseSrvPair(&g_texAmbient, &g_srvAmbient);
            hr = CreateSolidBgra(g_dev, 255, 255, 255, 255, &g_texAmbient, &g_srvAmbient);
            if (FAILED(hr)) {
                return nullptr;
            }
            g_cachedAmbW = g_cachedAmbH = 1;
        }
    }

    if (hasDisp) {
        if (!uploadIfNeeded(&g_texDisp, &g_srvDisp, *displacement, dw, dh, &g_cachedDw, &g_cachedDh)) {
            return nullptr;
        }
    } else {
        if (g_srvDisp == nullptr || g_cachedDw != 1) {
            ReleaseSrvPair(&g_texDisp, &g_srvDisp);
            hr = CreateSolidBgra(g_dev, 128, 128, 128, 255, &g_texDisp, &g_srvDisp);
            if (FAILED(hr)) {
                return nullptr;
            }
            g_cachedDw = g_cachedDh = 1;
        }
    }

    if (hasSpec) {
        if (!uploadIfNeeded(&g_texSpec, &g_srvSpec, *specular, sw, sh, &g_cachedSw, &g_cachedSh)) {
            return nullptr;
        }
    } else {
        if (g_srvSpec == nullptr || g_cachedSw != 1) {
            ReleaseSrvPair(&g_texSpec, &g_srvSpec);
            hr = CreateSolidBgra(g_dev, 0, 0, 0, 255, &g_texSpec, &g_srvSpec);
            if (FAILED(hr)) {
                return nullptr;
            }
            g_cachedSw = g_cachedSh = 1;
        }
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = g_ctx->Map(g_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) {
        return nullptr;
    }
    auto* cb = static_cast<ForgeConstants*>(mapped.pData);
    cb->lightTime = lightingTime;
    cb->outW = static_cast<float>(outW);
    cb->outH = static_cast<float>(outH);
    cb->hasAmbient = hasAmb ? 1u : 0u;
    cb->hasDisplacement = hasDisp ? 1u : 0u;
    cb->hasSpecular = hasSpec ? 1u : 0u;
    cb->padAlign[0] = cb->padAlign[1] = 0.f;
    g_ctx->Unmap(g_cb, 0);

    ID3D11ShaderResourceView* srvs[5] = {g_srvAlbedo, g_srvNormal, g_srvAmbient, g_srvDisp, g_srvSpec};
    g_ctx->PSSetShaderResources(0, 5, srvs);
    g_ctx->PSSetSamplers(0, 1, &g_sampler);
    g_ctx->PSSetConstantBuffers(0, 1, &g_cb);
    g_ctx->PSSetShader(g_ps, nullptr, 0);
    g_ctx->VSSetShader(g_vs, nullptr, 0);
    g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_ctx->IASetInputLayout(nullptr);

    float blendFactor[4] = {0, 0, 0, 0};
    g_ctx->OMSetBlendState(g_blend, blendFactor, 0xFFFFFFFF);
    g_ctx->OMSetDepthStencilState(g_depth, 0);
    g_ctx->RSSetState(g_raster);

    ID3D11RenderTargetView* rtv = g_rtv;
    g_ctx->OMSetRenderTargets(1, &rtv, nullptr);

    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(outW);
    vp.Height = static_cast<float>(outH);
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    g_ctx->RSSetViewports(1, &vp);

    // RGBA order for ClearRenderTargetView (logical R,G,B,A); opaque — alpha 0 was erasing the plate in GDI+.
    const float clear[4] = {
        22.f / 255.f,
        24.f / 255.f,
        28.f / 255.f,
        1.f,
    };
    g_ctx->ClearRenderTargetView(g_rtv, clear);
    g_ctx->Draw(3, 0);

    ID3D11ShaderResourceView* nullSrv[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    g_ctx->PSSetShaderResources(0, 5, nullSrv);

    g_ctx->CopyResource(g_staging, g_rt);
    g_ctx->Flush();

    hr = g_ctx->Map(g_staging, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        return nullptr;
    }

    auto result = std::make_unique<Gdiplus::Bitmap>(outW, outH, PixelFormat32bppARGB);
    Gdiplus::Rect bounds(0, 0, outW, outH);
    Gdiplus::BitmapData lock{};
    if (result->LockBits(&bounds, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &lock) != Gdiplus::Ok) {
        g_ctx->Unmap(g_staging, 0);
        return nullptr;
    }

    const BYTE* src = static_cast<const BYTE*>(mapped.pData);
    BYTE* dst = static_cast<BYTE*>(lock.Scan0);
    const UINT srcPitch = mapped.RowPitch;
    const int dstStride = lock.Stride;
    for (int y = 0; y < outH; ++y) {
        std::memcpy(dst + static_cast<size_t>(y) * dstStride,
                    src + static_cast<size_t>(y) * srcPitch,
                    static_cast<size_t>(outW) * 4);
    }

    result->UnlockBits(&lock);
    g_ctx->Unmap(g_staging, 0);

    return result;
}

} // namespace ri::shell
