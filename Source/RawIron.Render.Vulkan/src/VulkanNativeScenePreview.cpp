#include "RawIron/Render/VulkanPreviewPresenter.h"
#include "RawIron/Render/HybridPresentationTargets.h"
#include "RawIron/Render/VulkanScenePreviewBridge.h"

#if defined(_WIN32)
#include "RawIron/Core/Log.h"
#include "RawIron/Core/RenderRecorder.h"
#include "RawIron/Core/RenderSubmissionPlan.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Scene/Components.h"
#include "RawIron/Render/VulkanCommandList.h"
#include "RawIron/Render/VulkanWarmupCache.h"
#include "RawIron/Math/Vec2.h"
#include "RawIron/Render/PreviewTexture.h"

#include "ProceduralMaterialMaps.h"
#include "RawIron/Scene/SceneKit.h"
#include "RawIron/Scene/SceneRenderSubmission.h"
#include "RawIron/Scene/SceneUtils.h"
#define VK_USE_PLATFORM_WIN32_KHR 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef RAWIRON_VULKAN_NATIVE_PREVIEW_ENABLED
#define RAWIRON_VULKAN_NATIVE_PREVIEW_ENABLED 0
#endif

namespace ri::render::vulkan {

namespace {

namespace fs = std::filesystem;

struct NativeSceneVertex {
    float position[3]{};
    float normal[3]{};
    float uv[2]{};
};

struct NativeSceneDraw {
    std::int32_t meshHandle = -1;
    std::int32_t materialHandle = -1;
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t instanceCount = 1;
    std::array<float, 16> model{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    std::array<float, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
    std::array<float, 3> emissiveColor{0.0f, 0.0f, 0.0f};
    float metallic = 0.0f;
    float roughness = 1.0f;
    std::array<float, 2> textureTiling{1.0f, 1.0f};
    bool useTexture = false;
    bool litShadingModel = false;
    std::int32_t materialStyleFlags = 0;
    bool alphaCutout = false;
    bool doubleSided = false;
    /// Resolved path under `textureRoot` for this frame (animated sequences); empty falls back to material lookup.
    std::string resolvedAlbedoRelPath{};
    bool nativeWaterUvMotion = false;
    bool additiveBlend = false;
    bool transparent = false;
    float alphaCutoff = 1.0f;
    float sortDepthSq = 0.0f;
};

constexpr std::int32_t kNativeMaterialStyleRetro = 1 << 5;
constexpr std::int32_t kNativeMaterialStyleLayered = 1 << 6;
constexpr std::int32_t kNativeMaterialStyleMixedMedia = 1 << 7;
constexpr std::int32_t kNativeMaterialStyleCrystal = 1 << 8;
constexpr std::int32_t kNativeMaterialWorkflowSpecGloss = 1 << 9;
constexpr std::int32_t kNativeMaterialStyleMetalLookup = 1 << 10;
constexpr std::int32_t kNativeMaterialHasNormalMap = 1 << 11;
constexpr std::int32_t kNativeMaterialHasOrmMap = 1 << 12;
constexpr std::int32_t kNativeMaterialWorldTileUv = 1 << 13;
constexpr std::int32_t kNativeMaterialAlbedoAlphaSmoothness = 1 << 14;

// When a lit, textured material does not author its own normal/ORM maps, the engine
// derives them from the albedo (Sobel-based relief + cavity occlusion) so flat
// materials gain depth instead of rendering glitchy/flat. Metal-lookup palette
// materials are skipped because their "albedo" is a lookup gradient, not a surface.
constexpr bool kNativeGenerateMissingMaterialMaps = true;

[[nodiscard]] constexpr std::uint32_t NativeShadowMapResolutionForTier(const int tier) noexcept {
    switch (std::clamp(tier, 0, 2)) {
    case 0:
        return 1536U;
    case 1:
        return 2048U;
    default:
        return 4096U;
    }
}

struct NativeScenePreviewData {
    const ri::scene::Scene* scene = nullptr;
    fs::path textureRoot{};
    std::array<float, 16> viewProjection{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    std::array<float, 4> clearColor{0.05f, 0.07f, 0.10f, 1.0f};
    std::vector<NativeSceneDraw> draws{};
    float sceneAnimationTimeSeconds = 0.0f;
    std::array<float, 4> cameraWorldPosition{{0.0f, 0.0f, 0.0f, 1.0f}};
    /// x=exposure, y=contrast, z=saturation, w=fog density
    std::array<float, 4> renderTuning{{1.0f, 1.0f, 1.0f, 0.0095f}};
    /// x=noise amount, y=scanline amount, z=barrel distortion, w=chromatic aberration
    std::array<float, 4> postProcessPrimary{{0.0f, 0.0f, 0.0f, 0.0f}};
    /// rgb=tint color, a=tint strength
    std::array<float, 4> postProcessTint{{1.0f, 1.0f, 1.0f, 0.0f}};
    /// x=blur, y=static fade, z=time seconds, w=non-zero: hybrid HDR linear radiance (composite tonemap/post).
    std::array<float, 4> postProcessSecondary{{0.0f, 0.0f, 0.0f, 0.0f}};
    int renderQualityTier = 1;
    std::array<float, 16> lightViewProjection{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    /// xyz=world-space light direction, w=sun intensity
    std::array<float, 4> lightDirectionIntensity{{0.34f, 0.86f, 0.31f, 1.65f}};
    /// xyz=world-space local light position, w=range
    std::array<float, 4> localLightPositionRange{{0.0f, 1.8f, 0.0f, 20.0f}};
    /// rgb=local light color, w=intensity multiplier
    std::array<float, 4> localLightColorIntensity{{1.0f, 0.92f, 0.82f, 2.0f}};
    /// rgb=directional light color * intensity (linear); w reserved (keep 1 for future sun scale use).
    std::array<float, 4> directionalLightColorIntensity{{1.0f, 0.98f, 0.94f, 1.0f}};
    /// x=width px, y=height px, z=1/width, w=1/height (post radial / vignette).
    std::array<float, 4> viewportMetrics{{1920.0f, 1080.0f, 1.0f / 1920.0f, 1.0f / 1080.0f}};
    /// x=CAS sharpen mix, y=CAS contrast adaptation, z=bloom intensity, w=bloom threshold (linear HDR).
    std::array<float, 4> presentationTuning{{0.0f, 0.0f, 0.0f, 0.65f}};
    /// x=tone curve, y=TriDither output, z=deband, w=SweetFX vignette (type 0) strength.
    std::array<float, 4> presentationColorGrading{{0.0f, 0.0f, 0.0f, 0.0f}};
    /// x=film grain (SweetFX-style), yzw reserved.
    std::array<float, 4> presentationExtra{{0.0f, 0.0f, 0.0f, 0.0f}};
    /// SweetFX LiftGammaGain: xyz=RGB_Lift, w=mix (0..1). Gamma/Gain in following vec4s.
    std::array<float, 4> lggLiftMix{{1.0f, 1.0f, 1.0f, 0.0f}};
    std::array<float, 4> lggGammaRgb{{1.0f, 1.0f, 1.0f, 0.0f}};
    std::array<float, 4> lggGainRgb{{1.0f, 1.0f, 1.0f, 0.0f}};
    /// xyz=VibranceRGBBalance, w=Vibrance amount (-1..1).
    std::array<float, 4> vibranceBalanceAmount{{1.0f, 1.0f, 1.0f, 0.0f}};
    /// Technicolor v1: x=Power, y=Strength, zw=RGBNegative.rg; second vec4 x=B negative.
    std::array<float, 4> technicolor1PowStrNegRg{{4.0f, 0.0f, 0.88f, 0.88f}};
    std::array<float, 4> technicolor1NegBPad{{0.88f, 0.0f, 0.0f, 0.0f}};
    /// Technicolor2: xyz=ColorStrength, w=Brightness; second vec4 xy=Saturation,Strength.
    std::array<float, 4> technicolor2ColBright{{0.2f, 0.2f, 0.2f, 1.0f}};
    std::array<float, 4> technicolor2SatStrPad{{1.0f, 0.0f, 0.0f, 0.0f}};
    /// SweetFX Sepia.fx (`Tint`): xyz=tint, w=strength.
    std::array<float, 4> sepiaTintXyzStrength{{0.55f, 0.43f, 0.42f, 0.0f}};
    /// x=preset index (0–17 as float), y=color saturation (1=off), zw=0.
    std::array<float, 4> monochromePresetSat{{0.0f, 1.0f, 0.0f, 0.0f}};
    /// xyz=custom conversion coefficients when preset==0.
    std::array<float, 4> monochromeCustomCoeff{{0.21f, 0.72f, 0.07f, 0.0f}};
    /// SweetFX DPX.fx: xyz=RGB_Curve.
    std::array<float, 4> dpxRgbCurvePad{{8.0f, 8.0f, 8.0f, 0.0f}};
    /// xyz=RGB_C.
    std::array<float, 4> dpxRgbCPad{{0.36f, 0.36f, 0.34f, 0.0f}};
    /// x=Contrast, y=Saturation, z=Colorfulness, w=Strength.
    std::array<float, 4> dpxContrastSatColorStr{{0.1f, 3.0f, 2.5f, 0.0f}};
    /// SweetFX ColorMatrix.fx row R (new red channel mix).
    std::array<float, 4> colorMatrixRowR{{0.817f, 0.183f, 0.0f, 0.0f}};
    std::array<float, 4> colorMatrixRowG{{0.333f, 0.667f, 0.0f, 0.0f}};
    /// xyz=row B, w=Strength.
    std::array<float, 4> colorMatrixRowBStr{{0.0f, 0.125f, 0.875f, 0.0f}};
    /// SweetFX FakeHDR.fx: x=HDRPower, y=radius1, z=radius2, w=strength (0=off).
    std::array<float, 4> fakeHdrPowerR1R2Str{{1.30f, 0.793f, 0.87f, 0.0f}};
    /// SweetFX Levels.fx: x=black (0–255), y=white (0–255), z=strength, w=clip highlight (0/1).
    std::array<float, 4> levelsBlackWhiteStrClip{{16.0f, 235.0f, 0.0f, 0.0f}};
    /// SweetFX LumaSharpen.fx: xyz=strength,clamp,offset_bias; w=pattern(0–3)+4*show_debug.
    std::array<float, 4> lumaSharpenPack{{0.0f, 0.035f, 1.0f, 1.0f}};
    /// SweetFX Curves.fx: x=Contrast, y=Mode(0-2), z=Formula(0-10), w=strength.
    std::array<float, 4> sweetFxCurvesPack{{0.65f, 0.0f, 4.0f, 0.0f}};
    /// SweetFX ChromaticAberration.fx: xy=Shift (pixels), z=strength, w=0.
    std::array<float, 4> sweetFxChromaticAberrationPack{{2.5f, -0.5f, 0.0f, 0.0f}};
    /// SweetFX Border.fx: x=widthX px, y=widthY px, z=border_ratio, w=strength (0=off).
    std::array<float, 4> sweetFxBorderPack{{0.0f, 0.0f, 2.35f, 0.0f}};
    /// SweetFX Border.fx: xyz=border color, w=0.
    std::array<float, 4> sweetFxBorderColorPad{{0.0f, 0.0f, 0.0f, 0.0f}};
    /// SweetFX Cartoon.fx: x=Power, y=EdgeSlope, z=strength, w=0.
    std::array<float, 4> sweetFxCartoonPack{{1.5f, 1.5f, 0.0f, 0.0f}};
    /// SweetFX Tonemap.fx v1.1: xyzw = Gamma, Exposure, Saturation, Bleach.
    std::array<float, 4> sweetFxTonemapGammaExpSatBleach{{1.0f, 0.0f, 0.0f, 0.0f}};
    /// xyz=FogColor, w=Defog.
    std::array<float, 4> sweetFxTonemapFogColorDefog{{0.0f, 0.0f, 1.0f, 0.0f}};
    /// x=strength (0=skip), yzw=0.
    std::array<float, 4> sweetFxTonemapStrengthPad{{0.0f, 0.0f, 0.0f, 0.0f}};
    /// x=mode (0..6 as float), y=strength, zw=0.
    std::array<float, 4> sweetFxSplitscreenModeStrength{{0.0f, 0.0f, 0.0f, 0.0f}};
    /// x=palette 0..14, y=scanlines 0..2, z=dither, w=strength.
    std::array<float, 4> sweetFxNostalgiaPack{{1.0f, 1.0f, 0.0f, 0.0f}};
    /// x=mode 0..8, y=difference_scale, z=strength, w=0.
    std::array<float, 4> sweetFxComparePack{{7.0f, 5.0f, 0.0f, 0.0f}};
    /// SweetFX Layer.fx: xy=Layer_Pos, z=Layer_Scale, w=Layer_Blend.
    std::array<float, 4> sweetFxLayerPosScaleBlend{{0.5f, 0.5f, 1.0f, 0.0f}};
    /// x=LAYER_SIZE_X, y=LAYER_SIZE_Y, zw=0.
    std::array<float, 4> sweetFxLayerTexSizePad{{1280.0f, 720.0f, 0.0f, 0.0f}};
    /// SweetFX FXAA 3.11: x=Subpix, y=EdgeThreshold, z=EdgeThresholdMin, w=Strength.
    std::array<float, 4> sweetFxFxaaPack{{0.25f, 0.125f, 0.0f, 0.0f}};
    /// SweetFX CRT.fx: x=Amount, y=Resolution, z=Gamma, w=MonitorGamma.
    std::array<float, 4> sweetFxCrtPack0{{0.0f, 1.15f, 2.4f, 2.2f}};
    /// x=Brightness, y=ScanlineIntensity, z=ScanlineGaussian, w=Curvature.
    std::array<float, 4> sweetFxCrtPack1{{0.9f, 2.0f, 1.0f, 0.0f}};
    /// x=CurvatureRadius, y=CornerSize, z=ViewerDistance, w=Overscan.
    std::array<float, 4> sweetFxCrtPack2{{1.5f, 0.01f, 2.0f, 1.01f}};
    /// xy=Angle, z=Oversample, w=0.
    std::array<float, 4> sweetFxCrtPack3{{0.0f, 0.0f, 1.0f, 0.0f}};
    /// SweetFX ASCII.fx: x=spacing, y=font (0:3x5,1:5x5), z=colorMode, w=strength.
    std::array<float, 4> sweetFxAsciiPack0{{1.0f, 1.0f, 1.0f, 0.0f}};
    /// x=swapColors, y=invertBrightness, z=dithering, w=ditheringIntensity.
    std::array<float, 4> sweetFxAsciiPack1{{0.0f, 0.0f, 1.0f, 2.0f}};
    /// x=ditherDebugGradient, yzw=0.
    std::array<float, 4> sweetFxAsciiPack2{{0.0f, 0.0f, 0.0f, 0.0f}};
    /// xyz=fontColor, w=0.
    std::array<float, 4> sweetFxAsciiFontColorPad{{1.0f, 1.0f, 1.0f, 0.0f}};
    /// xyz=backgroundColor, w=0.
    std::array<float, 4> sweetFxAsciiBackgroundColorPad{{0.0f, 0.0f, 0.0f, 0.0f}};
    /// SweetFX SMAA.fx: x=edgeType, y=edgeThreshold, z=depthThreshold, w=strength.
    std::array<float, 4> sweetFxSmaaPack0{{1.0f, 0.10f, 0.01f, 0.0f}};
    /// x=maxSearch, y=maxSearchDiag, z=cornerRounding, w=debugOutput.
    std::array<float, 4> sweetFxSmaaPack1{{32.0f, 16.0f, 25.0f, 0.0f}};
    /// x=type, y=strength, zw=0.
    std::array<float, 4> reshadeDaltonizePack{{0.0f, 0.0f, 0.0f, 0.0f}};
    /// x=presentType, y=strength, zw=0.
    std::array<float, 4> reshadeDisplayDepthPack{{2.0f, 0.0f, 0.0f, 0.0f}};
    /// x=amountChroma, y=amountLuma, z=strength, w=0.
    std::array<float, 4> reshadeLutPack{{1.0f, 1.0f, 0.0f, 0.0f}};
    /// PD80_04_Technicolor.fx packs (see `NativeComposite.frag`).
    std::array<float, 4> pd80TcRedStrPad{{1.0f, 0.098f, 0.0f, 0.0f}};
    std::array<float, 4> pd80TcCyanPad{{0.0f, 0.988f, 1.0f, 0.0f}};
    std::array<float, 4> pd80TcKeySat2Pad{{1.0f, 1.0f, 1.0f, 1.5f}};
    std::array<float, 4> pd80Tc3ColBrightPad{{0.2f, 0.2f, 0.2f, 1.0f}};
    std::array<float, 4> pd80Tc3SatStrEnPad{{1.0f, 1.0f, 0.0f, 0.0f}};
    /// PD80_04_Color_Temperature.fx (see `NativeComposite.frag`).
    std::array<float, 4> pd80ColorTempKelvinLumMixStr{{6500.0f, 1.0f, 1.0f, 0.0f}};
    /// PD80_04_Saturation_Limit.fx: x=limit, y=strength.
    std::array<float, 4> pd80SatLimitCapStr{{1.0f, 0.0f, 0.0f, 0.0f}};
    /// PD80_04_Color_Balance.fx (see `NativeComposite.frag`).
    std::array<float, 4> pd80ColorBalanceShadowPad{{0.0f, 0.0f, 0.0f, 0.0f}};
    std::array<float, 4> pd80ColorBalanceMidPad{{0.0f, 0.0f, 0.0f, 0.0f}};
    std::array<float, 4> pd80ColorBalanceHighPad{{0.0f, 0.0f, 0.0f, 0.0f}};
    std::array<float, 4> pd80ColorBalanceOptStr{{1.0f, 0.0f, 0.0f, 0.0f}};
    /// PD80_04_Color_Isolation.fx (see `NativeComposite.frag`).
    std::array<float, 4> pd80ColorIsolationHueRangeSatMix{{0.0f, 0.167f, 1.0f, 1.0f}};
    std::array<float, 4> pd80ColorIsolationStrPad{{0.0f, 0.0f, 0.0f, 0.0f}};
    /// PD80_03_Levels.fx (see `NativeComposite.frag`).
    std::array<float, 4> pd80LevelsIbPad{{0.0f, 0.0f, 0.0f, 0.0f}};
    std::array<float, 4> pd80LevelsIwPad{{1.0f, 1.0f, 1.0f, 0.0f}};
    std::array<float, 4> pd80LevelsObPad{{0.0f, 0.0f, 0.0f, 0.0f}};
    std::array<float, 4> pd80LevelsOwPad{{1.0f, 1.0f, 1.0f, 0.0f}};
    std::array<float, 4> pd80LevelsGammaDitherStr{{1.0f, 1.0f, 1.0f, 0.0f}};
    /// PD80_04_BlacknWhite.fx (see `NativeComposite.frag`).
    std::array<float, 4> pd80BwPack0{{13.0f, 1.5f, 1.0f, 1.0f}};
    std::array<float, 4> pd80BwPack1{{0.2f, 0.4f, 0.6f, 0.0f}};
    std::array<float, 4> pd80BwPack2{{-0.6f, -0.2f, 0.0f, 0.0f}};
    std::array<float, 4> pd80BwPack3{{0.0f, 0.083f, 0.12f, 0.0f}};
    /// PD80_04_Contrast_Brightness_Saturation.fx (see `NativeComposite.frag`).
    std::array<float, 4> pd80CbsPack0{{1.0f, 1.0f, 0.0f, 0.0f}};
    std::array<float, 4> pd80CbsPack1{};
    std::array<float, 4> pd80CbsPack2{{0.0f, 0.167f, 0.0f, 0.0f}};
    std::array<float, 4> pd80CbsPack3{};
    std::array<float, 4> pd80CbsPack4{};
    std::array<float, 4> pd80CbsPack5{{0.0f, 0.0f, 0.1f, 1.0f}};
    std::array<float, 4> pd80CbsPack6{};
    std::array<float, 4> pd80CbsPack7{};
    /// PD80_06_Chromatic_Aberration.fx (see `NativeComposite.frag`).
    std::array<float, 4> pd80CaPack0{{0.0f, 1.0f, -12.0f, 24.0f}};
    std::array<float, 4> pd80CaPack1{{0.0f, 135.0f, 1.0f, 1.0f}};
    std::array<float, 4> pd80CaPack2{{0.0f, 0.0f, 1.0f, 1.0f}};
    std::array<float, 4> pd80CaPack3{};
    std::array<float, 4> pd80CaPack4{};
    std::array<float, 4> pd80CaPack5{{0.0f, 0.1f, 1.0f, 0.0f}};
    /// PD80_05_Sharpening.fx (see `NativeComposite.frag`).
    std::array<float, 4> pd80LsPack0{{0.0f, 0.45f, 1.7f, 0.0f}};
    std::array<float, 4> pd80LsPack1{{0.03f, 0.0f, 0.0f, 0.0f}};
    std::array<float, 4> pd80LsPack2{{0.0f, 0.0f, 0.1f, 1.0f}};
    /// PD80_06_Film_Grain.fx (see `NativeComposite.frag`).
    std::array<float, 4> pd80FgPack0{{0.0f, 1.0f, 1.0f, 1.0f}};
    std::array<float, 4> pd80FgPack1{{1.0f, 0.0f, 1.0f, 0.333f}};
    std::array<float, 4> pd80FgPack2{{0.65f, 10.0f, 1.0f, 1.0f}};
    std::array<float, 4> pd80FgPack3{};
    std::array<float, 4> pd80FgPack4{{0.0f, 0.1f, 1.0f, 0.0f}};
    /// PD80_06_Depth_Slicer.fx (see `NativeComposite.frag`).
    std::array<float, 4> pd80DsPack0{{0.0f, 0.0f, 0.015f, 0.0f}};
    std::array<float, 4> pd80DsPack1{{0.005f, 0.0f, 0.083f, 0.0f}};
    std::array<float, 4> pd80DsPack2{{0.0f, 1.0f, 0.0f, 0.0f}};
    /// PD80_01_Color_Gamut.fx (see `NativeComposite.frag`).
    std::array<float, 4> pd80CgPack0{};
    /// PD80_03_Color_Space_Curves.fx (see `NativeComposite.frag`).
    std::array<float, 4> pd80CscPack0{{0.0f, 1.0f, 1.0f, 1.0f}};
    std::array<float, 4> pd80CscPack1{{0.2f, 0.2f, 0.8f, 0.8f}};
    std::array<float, 4> pd80CscPack2{};
    /// PD80_03_Shadows_Midtones_Highlights.fx (see `NativeComposite.frag`).
    std::array<float, 4> pd80SmhPack0{{0.0f, 2.0f, 0.0f, 1.0f}};
    std::array<float, 4> pd80SmhPack1{{2.0f, 0.0f, 0.0f, 0.0f}};
    std::array<float, 4> pd80SmhPack2{};
    std::array<float, 4> pd80SmhPack3{};
    std::array<float, 4> pd80SmhPack4{};
    std::array<float, 4> pd80SmhPack5{};
    std::array<float, 4> pd80SmhPack6{};
    std::array<float, 4> pd80SmhPack7{};
    std::array<float, 4> pd80SmhPack8{};
    std::array<float, 4> pd80SmhPack9{};
    std::array<float, 4> pd80SmhPack10{};
    /// PD80_03_Curved_Levels.fx (see `NativeComposite.frag`).
    std::array<float, 4> pd80ClPack0{{0.0f, 1.0f, 1.0f, 0.0f}};
    /// Grey: black_in, white_in, black_out, white_out (0–255 as float).
    std::array<float, 4> pd80ClPack1{{0.0f, 255.0f, 0.0f, 255.0f}};
    /// Grey: pos0_shoulder, pos1_shoulder, pos0_toe, pos1_toe.
    std::array<float, 4> pd80ClPack2{{0.75f, 0.75f, 0.25f, 0.25f}};
    std::array<float, 4> pd80ClPack3{{0.0f, 255.0f, 0.0f, 255.0f}};
    std::array<float, 4> pd80ClPack4{{0.75f, 0.75f, 0.25f, 0.25f}};
    std::array<float, 4> pd80ClPack5{{0.0f, 255.0f, 0.0f, 255.0f}};
    std::array<float, 4> pd80ClPack6{{0.75f, 0.75f, 0.25f, 0.25f}};
    std::array<float, 4> pd80ClPack7{{0.0f, 255.0f, 0.0f, 255.0f}};
    std::array<float, 4> pd80ClPack8{{0.75f, 0.75f, 0.25f, 0.25f}};
    /// PD80_04_Selective_Color.fx (see `NativeComposite.frag`).
    std::array<float, 4> pd80ScPack0{{0.0f, 1.0f, 1.0f, 0.0f}};
    std::array<float, 4> pd80ScPack1{};
    std::array<float, 4> pd80ScPack2{};
    std::array<float, 4> pd80ScPack3{};
    std::array<float, 4> pd80ScPack4{};
    std::array<float, 4> pd80ScPack5{};
    std::array<float, 4> pd80ScPack6{};
    std::array<float, 4> pd80ScPack7{};
    std::array<float, 4> pd80ScPack8{};
    std::array<float, 4> pd80ScPack9{};
    std::array<float, 4> pd80ScPack10{};
    std::array<float, 4> pd80ScPack11{};
    std::array<float, 4> pd80ScPack12{};
    std::array<float, 4> pd80ScPack13{};
    std::array<float, 4> pd80ScPack14{};
    std::array<float, 4> pd80ScPack15{};
    std::array<float, 4> pd80ScPack16{};
    std::array<float, 4> pd80ScPack17{};
    std::array<float, 4> pd80ScPack18{};
    std::array<float, 4> pd80PpPack0{};
    std::array<float, 4> pd80PpPack1{};
    std::array<float, 4> pd80MrPack0{};
    std::array<float, 4> pd80MrPack1{};
    std::array<float, 4> pd80MrPack2{};
    std::array<float, 4> pd80MrPack3{};
    std::array<float, 4> pd80MrPack4{};
    std::array<float, 4> pd80MrPack5{};
    std::array<float, 4> pd80MrPack6{};
    std::array<float, 4> pd80MrPack7{};
    std::array<float, 4> pd80BlpPack0{};
    std::array<float, 4> pd80BlpPack1{};
    std::array<float, 4> pd80BlpPack2{};
    std::array<float, 4> pd80BlpPack3{};
    std::array<float, 4> pd80BlpPack4{};
    std::array<float, 4> pd80BlpPack5{};
    std::array<float, 4> pd80CltPack0{};
    std::array<float, 4> pd80CltPack1{};
    std::array<float, 4> pd80CltPack2{};
    std::array<float, 4> pd80CltPack3{};
    std::array<float, 4> pd80CltPack4{};
    std::array<float, 4> pd80CltPack5{};
    std::array<float, 4> pd80LcPack0{};
    std::array<float, 4> pd80LfPack0{};
    std::array<float, 4> pd80Cg4Pack0{};
    std::array<float, 4> pd80Cg4Pack1{};
    std::array<float, 4> pd80Cg4Pack2{};
    std::array<float, 4> pd80Cg4Pack3{};
    std::array<float, 4> pd80Cg4Pack4{};
    std::array<float, 4> pd80Cg4Pack5{};
    std::array<float, 4> pd80Cg4Pack6{};
    std::array<float, 4> pd80Cg4Pack7{};
    std::array<float, 4> pd80Cg4Pack8{};
    std::array<float, 4> pd80CcPack0{};
    std::array<float, 4> pd80RccPack0{};
    std::array<float, 4> pd80RccPack1{};
    std::array<float, 4> pd80RccPack2{};
    std::array<float, 4> pd80RccPack3{};
    std::array<float, 4> pd80RccPack4{};
    std::array<float, 4> pd80FaPack0{};
    std::array<float, 4> pd80HbPack0{};
    std::array<float, 4> pd80HbPack1{};
    std::array<float, 4> pd80HbPack2{};
    std::array<float, 4> pd80Sc2Pack0{};
    /// Colourfulness.fx: x=colourfulness, y=limit luma, zw unused.
    std::array<float, 4> creatorColourfulnessPack{};
    /// FilmicPass.fx: x=strength, y=fade, z=bleach, w=saturation.
    std::array<float, 4> creatorFilmicPassPack{};
    /// FilmGrain2.fx: x=amount, y=color amount, z=luminance amount, w=grain size.
    std::array<float, 4> creatorFilmGrain2Pack{};
    /// Denoise.fx KNN: x=strength, y=noise level, z=lerp coefficient, w=weight threshold.
    std::array<float, 4> creatorDenoisePack{};
    /// Denoise.fx KNN: x=counter threshold, y=gaussian sigma, zw unused.
    std::array<float, 4> creatorDenoisePack2{};
    /// AdaptiveSharpen.fx: x=curve_height, y=curve_slope, z=L_overshoot, w=D_overshoot.
    std::array<float, 4> creatorAdaptiveSharpenPack0{};
    /// AdaptiveSharpen.fx: x=L_compr_low, y=L_compr_high, z=D_compr_low, w=D_compr_high.
    std::array<float, 4> creatorAdaptiveSharpenPack1{};
    /// AdaptiveSharpen.fx: x=scale_lim, y=scale_cs, z=pm_p, w unused.
    std::array<float, 4> creatorAdaptiveSharpenPack2{};
    /// GaussianBlur.fx: x=strength, y=offset, z=radius (0–4), w unused.
    std::array<float, 4> creatorGaussianBlurPack{};
    /// FineSharp.fx: x=sstr, y=cstr, z=xstr, w=xrep.
    std::array<float, 4> creatorFineSharpPack0{};
    /// FineSharp.fx: x=lstr, y=pstr, z=mode (0–2), w unused.
    std::array<float, 4> creatorFineSharpPack1{};
    /// Bloom.fx Marty McFly: x=threshold, y=amount, z=saturation, w=mix mode (0–3).
    std::array<float, 4> creatorMartyBloomPack0{};
    /// Bloom.fx Marty McFly: xyz=tint, w unused.
    std::array<float, 4> creatorMartyBloomPack1{};
    /// DOF.fx RingDOF: x=strength, y=autoFocus, z=manualFocusDepth, w=infiniteFocus.
    std::array<float, 4> creatorDofPack0{};
    /// DOF.fx RingDOF: xy=focusPoint, z=focusRadius, w=focusSamples.
    std::array<float, 4> creatorDofPack1{};
    /// DOF.fx RingDOF: x=nearBlurCurve, y=farBlurCurve, z=blurRadius, w=ringSamples.
    std::array<float, 4> creatorDofPack2{};
    /// DOF.fx RingDOF: x=ringRings, y=ringThreshold, z=ringGain, w=ringFringe.
    std::array<float, 4> creatorDofPack3{};
    /// DOF.fx RingDOF: x=ringBias, yzw unused.
    std::array<float, 4> creatorDofPack4{};
    /// AmbientLight.fx: x=intensity, y=threshold, z=adapt, w=adaptBaseMult.
    std::array<float, 4> creatorAmbientLightPack0{};
    /// AmbientLight.fx: x=adaptBlackLevel, y=dither, z=dirt, w=adaptiveMode.
    std::array<float, 4> creatorAmbientLightPack1{};
    /// AmbientLight.fx: x=dirtInt, y=dirtOvrInt, z=timePhase, w unused.
    std::array<float, 4> creatorAmbientLightPack2{};
    /// FakeMotionBlur.fx: x=recall, y=softness, zw unused.
    std::array<float, 4> creatorFakeMotionBlurPack0{};
    /// ReflectiveBumpMapping.fx: x=strength, y=blurWidthPixels, z=reliefHeight, w=fresnelReflectance.
    std::array<float, 4> creatorReflectiveBumpMappingPack0{};
    /// ReflectiveBumpMapping.fx: x=fresnelMult, y=lowerThreshold, z=upperThreshold, w=sampleCount.
    std::array<float, 4> creatorReflectiveBumpMappingPack1{};
    /// ReflectiveBumpMapping.fx: rgba color masks for red/orange/yellow/green.
    std::array<float, 4> creatorReflectiveBumpMappingPack2{};
    /// ReflectiveBumpMapping.fx: rgb color masks for cyan/blue/magenta, w=depthFarPlane.
    std::array<float, 4> creatorReflectiveBumpMappingPack3{};
    /// Native CropResize: xy=content size pixels, zw=intermediate size pixels.
    std::array<float, 4> cropScaleContentIntermediate{};
    /// Native CropResize: xy=final size pixels, z=filter (0 point/1 linear), w=strength.
    std::array<float, 4> cropScaleFinalFilterStrength{};
    /// Barbatos uFakeHDR: x=preset atlas row (0..2), y=strength (0..2), zw unused.
    std::array<float, 4> barbatosFakeHdrPack{};
    /// Raw Iron native post capabilities.
    std::array<float, 4> riAdaptiveDebandPack{};
    std::array<float, 4> riLocalSharpenPack{};
    std::array<float, 4> riOutlinePack0{};
    std::array<float, 4> riOutlineColorMethod{};
    std::array<float, 4> riOutlineWobbleDebug{};
    std::array<float, 4> riSignalGlitchPack{};
    std::array<float, 4> riNightVisionPack{};
    std::array<float, 4> riHq4xPack0{};
    std::array<float, 4> riHq4xPack1{};
    std::array<float, 4> riHslAnchor0{};
    std::array<float, 4> riHslAnchor1{};
    std::array<float, 4> riHslAnchor2{};
    std::array<float, 4> riHslAnchor3{};
    std::array<float, 4> riHslAnchor4{};
    std::array<float, 4> riHslAnchor5{};
    std::array<float, 4> riHslAnchor6{};
    std::array<float, 4> riHslAnchor7{};
    std::array<float, 4> riLevelsPlusPack0{};
    std::array<float, 4> riLevelsPlusPack1{};
    std::array<float, 4> riLevelsPlusPack2{};
    std::array<float, 4> riLevelsPlusPack3{};
    std::array<float, 4> riLevelsPlusPack4{};
    std::array<float, 4> riLevelsPlusPack5{};
    std::array<float, 4> riLevelsPlusPack6{};
    std::array<float, 4> riLightDofPack0{};
    std::array<float, 4> riLightDofPack1{};
    std::array<float, 4> riLightDofPack2{};
    std::array<float, 4> riMagicBloomPack0{};
    std::array<float, 4> riMagicBloomPack1{};
    std::array<float, 4> riUiMaskPack0{};
    std::array<float, 4> riUiMaskPack1{};
    std::array<float, 4> riLuminanceThresholdPack{};
    std::array<float, 4> riColorQuantizePack0{};
    std::array<float, 4> riColorQuantizePack1{};
    std::array<float, 4> riColorQuantizePack2{};
    std::array<float, 4> riKaleidoscopePack0{};
    std::array<float, 4> riKaleidoscopePack1{};
    /// Column-major `mat4` for `NativeSkybox.vert` (`projection * skyRotation`).
    std::array<float, 16> skyClipFromLocal{};
    /// Column-major `mat4`; upper 3x3 maps eye-space directions to world for equirect sampling.
    std::array<float, 16> skyEyeToWorld{};
    std::int32_t skyUseTextureFile = 0;
    std::int32_t skyUseAuthoredGradient = 0;
    std::array<float, 4> skyHorizonColor{{0.82f, 0.82f, 0.80f, 1.0f}};
    std::array<float, 4> skyZenithColor{{0.54f, 0.56f, 0.57f, 1.0f}};
    fs::path skyEquirectAbsolute{};
};

struct alignas(16) SkyUniformStd140 {
    std::int32_t hasSkyTexture = 0;
    std::int32_t useAuthoredGradient = 0;
    std::int32_t pad0 = 0;
    std::int32_t pad1 = 0;
    float clipFromLocal[16]{};
    float eyeToWorldRotation[16]{};
    /// xyz = world-space direction toward the sun; w = visible sun strength.
    float sunDirection[4]{};
    /// rgb = sun disc tint (normalized); w unused.
    float sunColor[4]{1.0f, 0.94f, 0.82f, 1.0f};
    /// rgb = horizon / `clear_bottom`; w unused.
    float horizonColor[4]{0.82f, 0.82f, 0.80f, 1.0f};
    /// rgb = zenith / `clear_top`; w unused.
    float zenithColor[4]{0.54f, 0.56f, 0.57f, 1.0f};
};

static_assert(sizeof(SkyUniformStd140) == 208, "Must match NativeSkybox.{vert,frag} std140 layout.");

struct alignas(16) CameraUniformStd140 {
    float viewProjection[16]{};
    float cameraWorldPosition[4]{};
    float renderTuning[4]{};
    float postProcessPrimary[4]{};
    float postProcessTint[4]{};
    float postProcessSecondary[4]{};
    float lightViewProjection[16]{};
    float lightDirectionIntensity[4]{};
    float localLightPositionRange[4]{};
    float localLightColorIntensity[4]{};
    float directionalLightColorIntensity[4]{};
    float viewportMetrics[4]{};
    float presentationTuning[4]{};
    float presentationColorGrading[4]{};
    float presentationExtra[4]{};
    float lggLiftMix[4]{};
    float lggGammaRgb[4]{};
    float lggGainRgb[4]{};
    float vibranceBalanceAmount[4]{};
    float technicolor1PowStrNegRg[4]{};
    float technicolor1NegBPad[4]{};
    float technicolor2ColBright[4]{};
    float technicolor2SatStrPad[4]{};
    float sepiaTintXyzStrength[4]{};
    float monochromePresetSat[4]{};
    float monochromeCustomCoeff[4]{};
    float dpxRgbCurvePad[4]{};
    float dpxRgbCPad[4]{};
    float dpxContrastSatColorStr[4]{};
    float colorMatrixRowR[4]{};
    float colorMatrixRowG[4]{};
    float colorMatrixRowBStr[4]{};
    float fakeHdrPowerR1R2Str[4]{};
    float levelsBlackWhiteStrClip[4]{};
    float lumaSharpenPack[4]{};
    float sweetFxCurvesPack[4]{};
    float sweetFxChromaticAberrationPack[4]{};
    float sweetFxBorderPack[4]{};
    float sweetFxBorderColorPad[4]{};
    float sweetFxCartoonPack[4]{};
    float sweetFxTonemapGammaExpSatBleach[4]{};
    float sweetFxTonemapFogColorDefog[4]{};
    float sweetFxTonemapStrengthPad[4]{};
    float sweetFxSplitscreenModeStrength[4]{};
    float sweetFxNostalgiaPack[4]{};
    float sweetFxComparePack[4]{};
    float sweetFxLayerPosScaleBlend[4]{};
    float sweetFxLayerTexSizePad[4]{};
    float sweetFxFxaaPack[4]{};
    float sweetFxCrtPack0[4]{};
    float sweetFxCrtPack1[4]{};
    float sweetFxCrtPack2[4]{};
    float sweetFxCrtPack3[4]{};
    float sweetFxAsciiPack0[4]{};
    float sweetFxAsciiPack1[4]{};
    float sweetFxAsciiPack2[4]{};
    float sweetFxAsciiFontColorPad[4]{};
    float sweetFxAsciiBackgroundColorPad[4]{};
    float sweetFxSmaaPack0[4]{};
    float sweetFxSmaaPack1[4]{};
    float reshadeDaltonizePack[4]{};
    float reshadeDisplayDepthPack[4]{};
    float reshadeLutPack[4]{};
    float pd80TcRedStrPad[4]{};
    float pd80TcCyanPad[4]{};
    float pd80TcKeySat2Pad[4]{};
    float pd80Tc3ColBrightPad[4]{};
    float pd80Tc3SatStrEnPad[4]{};
    float pd80ColorTempKelvinLumMixStr[4]{};
    float pd80SatLimitCapStr[4]{};
    float pd80ColorBalanceShadowPad[4]{};
    float pd80ColorBalanceMidPad[4]{};
    float pd80ColorBalanceHighPad[4]{};
    float pd80ColorBalanceOptStr[4]{};
    float pd80ColorIsolationHueRangeSatMix[4]{};
    float pd80ColorIsolationStrPad[4]{};
    float pd80LevelsIbPad[4]{};
    float pd80LevelsIwPad[4]{};
    float pd80LevelsObPad[4]{};
    float pd80LevelsOwPad[4]{};
    float pd80LevelsGammaDitherStr[4]{};
    float pd80BwPack0[4]{};
    float pd80BwPack1[4]{};
    float pd80BwPack2[4]{};
    float pd80BwPack3[4]{};
    float pd80CbsPack0[4]{};
    float pd80CbsPack1[4]{};
    float pd80CbsPack2[4]{};
    float pd80CbsPack3[4]{};
    float pd80CbsPack4[4]{};
    float pd80CbsPack5[4]{};
    float pd80CbsPack6[4]{};
    float pd80CbsPack7[4]{};
    float pd80CaPack0[4]{};
    float pd80CaPack1[4]{};
    float pd80CaPack2[4]{};
    float pd80CaPack3[4]{};
    float pd80CaPack4[4]{};
    float pd80CaPack5[4]{};
    float pd80LsPack0[4]{};
    float pd80LsPack1[4]{};
    float pd80LsPack2[4]{};
    float pd80FgPack0[4]{};
    float pd80FgPack1[4]{};
    float pd80FgPack2[4]{};
    float pd80FgPack3[4]{};
    float pd80FgPack4[4]{};
    float pd80DsPack0[4]{};
    float pd80DsPack1[4]{};
    float pd80DsPack2[4]{};
    float pd80CgPack0[4]{};
    float pd80CscPack0[4]{};
    float pd80CscPack1[4]{};
    float pd80CscPack2[4]{};
    float pd80SmhPack0[4]{};
    float pd80SmhPack1[4]{};
    float pd80SmhPack2[4]{};
    float pd80SmhPack3[4]{};
    float pd80SmhPack4[4]{};
    float pd80SmhPack5[4]{};
    float pd80SmhPack6[4]{};
    float pd80SmhPack7[4]{};
    float pd80SmhPack8[4]{};
    float pd80SmhPack9[4]{};
    float pd80SmhPack10[4]{};
    float pd80ClPack0[4]{};
    float pd80ClPack1[4]{};
    float pd80ClPack2[4]{};
    float pd80ClPack3[4]{};
    float pd80ClPack4[4]{};
    float pd80ClPack5[4]{};
    float pd80ClPack6[4]{};
    float pd80ClPack7[4]{};
    float pd80ClPack8[4]{};
    float pd80ScPack0[4]{};
    float pd80ScPack1[4]{};
    float pd80ScPack2[4]{};
    float pd80ScPack3[4]{};
    float pd80ScPack4[4]{};
    float pd80ScPack5[4]{};
    float pd80ScPack6[4]{};
    float pd80ScPack7[4]{};
    float pd80ScPack8[4]{};
    float pd80ScPack9[4]{};
    float pd80ScPack10[4]{};
    float pd80ScPack11[4]{};
    float pd80ScPack12[4]{};
    float pd80ScPack13[4]{};
    float pd80ScPack14[4]{};
    float pd80ScPack15[4]{};
    float pd80ScPack16[4]{};
    float pd80ScPack17[4]{};
    float pd80ScPack18[4]{};
    float pd80PpPack0[4]{};
    float pd80PpPack1[4]{};
    float pd80MrPack0[4]{};
    float pd80MrPack1[4]{};
    float pd80MrPack2[4]{};
    float pd80MrPack3[4]{};
    float pd80MrPack4[4]{};
    float pd80MrPack5[4]{};
    float pd80MrPack6[4]{};
    float pd80MrPack7[4]{};
    float pd80BlpPack0[4]{};
    float pd80BlpPack1[4]{};
    float pd80BlpPack2[4]{};
    float pd80BlpPack3[4]{};
    float pd80BlpPack4[4]{};
    float pd80BlpPack5[4]{};
    float pd80CltPack0[4]{};
    float pd80CltPack1[4]{};
    float pd80CltPack2[4]{};
    float pd80CltPack3[4]{};
    float pd80CltPack4[4]{};
    float pd80CltPack5[4]{};
    float pd80LcPack0[4]{};
    float pd80LfPack0[4]{};
    float pd80Cg4Pack0[4]{};
    float pd80Cg4Pack1[4]{};
    float pd80Cg4Pack2[4]{};
    float pd80Cg4Pack3[4]{};
    float pd80Cg4Pack4[4]{};
    float pd80Cg4Pack5[4]{};
    float pd80Cg4Pack6[4]{};
    float pd80Cg4Pack7[4]{};
    float pd80Cg4Pack8[4]{};
    float pd80CcPack0[4]{};
    float pd80RccPack0[4]{};
    float pd80RccPack1[4]{};
    float pd80RccPack2[4]{};
    float pd80RccPack3[4]{};
    float pd80RccPack4[4]{};
    float pd80FaPack0[4]{};
    float pd80HbPack0[4]{};
    float pd80HbPack1[4]{};
    float pd80HbPack2[4]{};
    float pd80Sc2Pack0[4]{};
    float creatorColourfulnessPack[4]{};
    float creatorFilmicPassPack[4]{};
    float creatorFilmGrain2Pack[4]{};
    float creatorDenoisePack[4]{};
    float creatorDenoisePack2[4]{};
    float creatorAdaptiveSharpenPack0[4]{};
    float creatorAdaptiveSharpenPack1[4]{};
    float creatorAdaptiveSharpenPack2[4]{};
    float creatorGaussianBlurPack[4]{};
    float creatorFineSharpPack0[4]{};
    float creatorFineSharpPack1[4]{};
    float creatorMartyBloomPack0[4]{};
    float creatorMartyBloomPack1[4]{};
    float creatorDofPack0[4]{};
    float creatorDofPack1[4]{};
    float creatorDofPack2[4]{};
    float creatorDofPack3[4]{};
    float creatorDofPack4[4]{};
    float creatorAmbientLightPack0[4]{};
    float creatorAmbientLightPack1[4]{};
    float creatorAmbientLightPack2[4]{};
    float creatorFakeMotionBlurPack0[4]{};
    float creatorReflectiveBumpMappingPack0[4]{};
    float creatorReflectiveBumpMappingPack1[4]{};
    float creatorReflectiveBumpMappingPack2[4]{};
    float creatorReflectiveBumpMappingPack3[4]{};
    float cropScaleContentIntermediate[4]{};
    float cropScaleFinalFilterStrength[4]{};
    float barbatosFakeHdrPack[4]{};
    float riAdaptiveDebandPack[4]{};
    float riLocalSharpenPack[4]{};
    float riOutlinePack0[4]{};
    float riOutlineColorMethod[4]{};
    float riOutlineWobbleDebug[4]{};
    float riSignalGlitchPack[4]{};
    float riNightVisionPack[4]{};
    float riHq4xPack0[4]{};
    float riHq4xPack1[4]{};
    float riHslAnchor0[4]{};
    float riHslAnchor1[4]{};
    float riHslAnchor2[4]{};
    float riHslAnchor3[4]{};
    float riHslAnchor4[4]{};
    float riHslAnchor5[4]{};
    float riHslAnchor6[4]{};
    float riHslAnchor7[4]{};
    float riLevelsPlusPack0[4]{};
    float riLevelsPlusPack1[4]{};
    float riLevelsPlusPack2[4]{};
    float riLevelsPlusPack3[4]{};
    float riLevelsPlusPack4[4]{};
    float riLevelsPlusPack5[4]{};
    float riLevelsPlusPack6[4]{};
    float riLightDofPack0[4]{};
    float riLightDofPack1[4]{};
    float riLightDofPack2[4]{};
    float riMagicBloomPack0[4]{};
    float riMagicBloomPack1[4]{};
    float riUiMaskPack0[4]{};
    float riUiMaskPack1[4]{};
    float riLuminanceThresholdPack[4]{};
    float riColorQuantizePack0[4]{};
    float riColorQuantizePack1[4]{};
    float riColorQuantizePack2[4]{};
    float riKaleidoscopePack0[4]{};
    float riKaleidoscopePack1[4]{};
};

static_assert(sizeof(CameraUniformStd140) == 4304, "Must match NativeScenePreview shader CameraData std140 layout.");

void StoreMat4ColumnMajorGlsl(const ri::math::Mat4& matrix, float destination[16]) {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            destination[column * 4 + row] = matrix.m[row][column];
        }
    }
}

void StoreMat4ColumnMajorGlsl(const ri::math::Mat4& matrix, std::array<float, 16>& destination) {
    StoreMat4ColumnMajorGlsl(matrix, destination.data());
}

struct WindowState {
    HWND hwnd = nullptr;
    bool running = true;
    void* messageUserData = nullptr;
    VulkanPreviewWindowOptions::Win32MessageHook onWin32Message = nullptr;
};

struct ScopedWindowClass {
    HINSTANCE instance = nullptr;
    const wchar_t* className = L"RawIronVulkanNativeScenePreviewWindow";
    ATOM atom = 0;

    explicit ScopedWindowClass(WNDPROC windowProc) {
        instance = GetModuleHandleW(nullptr);
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = windowProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = className;
        windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        atom = RegisterClassW(&windowClass);
        if (atom == 0) {
            throw std::runtime_error("RegisterClassW failed for Vulkan native scene preview window.");
        }
    }

    ~ScopedWindowClass() {
        if (atom != 0) {
            UnregisterClassW(className, instance);
        }
    }
};

struct DeviceSelection {
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    std::uint32_t graphicsQueueFamily = 0;
    std::uint32_t presentQueueFamily = 0;
};

struct BufferResource {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

struct CachedGpuMesh {
    BufferResource vertexBuffer{};
    BufferResource indexBuffer{};
    std::uint32_t indexCount = 0;
};

struct NativeDrawPushConstants {
    float model[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float tiling[2] = {1.0f, 1.0f};
    std::int32_t useTexture = 0;
    std::int32_t nativeWaterUvMotion = 0;
    float nativeWaterTime = 0.0f;
    std::int32_t litShadingModel = 0;
    float metallic = 0.0f;
    float roughness = 1.0f;
    float emissiveColor[3] = {0.0f, 0.0f, 0.0f};
    float qualityTier = 1.0f;
    float alphaCutoff = 1.0f;
};
static_assert(sizeof(NativeDrawPushConstants) == 132, "Must match NativeScenePreview.{vert,frag} push_constant layout.");
static_assert(offsetof(NativeDrawPushConstants, useTexture) == 88);
static_assert(offsetof(NativeDrawPushConstants, nativeWaterUvMotion) == 92);
static_assert(offsetof(NativeDrawPushConstants, nativeWaterTime) == 96);
static_assert(offsetof(NativeDrawPushConstants, litShadingModel) == 100);
static_assert(offsetof(NativeDrawPushConstants, metallic) == 104);
static_assert(offsetof(NativeDrawPushConstants, roughness) == 108);
static_assert(offsetof(NativeDrawPushConstants, emissiveColor) == 112);
static_assert(offsetof(NativeDrawPushConstants, qualityTier) == 124);
static_assert(offsetof(NativeDrawPushConstants, alphaCutoff) == 128);
static_assert(sizeof(NativeSceneVertex) == 32);

struct CpuMeshGeometry {
    std::vector<NativeSceneVertex> vertices{};
    std::vector<std::uint32_t> indices{};
};

struct ImageResource {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

BufferResource CreateBuffer(VkPhysicalDevice physicalDevice,
                            VkDevice device,
                            VkDeviceSize size,
                            VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags memoryFlags);

constexpr std::array<ri::math::Vec3, 8> kCubeVertices = {{
    {-0.5f, -0.5f, -0.5f},
    {0.5f, -0.5f, -0.5f},
    {0.5f, 0.5f, -0.5f},
    {-0.5f, 0.5f, -0.5f},
    {-0.5f, -0.5f, 0.5f},
    {0.5f, -0.5f, 0.5f},
    {0.5f, 0.5f, 0.5f},
    {-0.5f, 0.5f, 0.5f},
}};

constexpr std::array<int, 36> kCubeIndices = {{
    4, 5, 6, 4, 6, 7,
    1, 0, 3, 1, 3, 2,
    0, 4, 7, 0, 7, 3,
    5, 1, 2, 5, 2, 6,
    3, 7, 6, 3, 6, 2,
    0, 1, 5, 0, 5, 4,
}};

constexpr std::array<ri::math::Vec3, 4> kPlaneVertices = {{
    {-0.5f, 0.0f, -0.5f},
    {0.5f, 0.0f, -0.5f},
    {-0.5f, 0.0f, 0.5f},
    {0.5f, 0.0f, 0.5f},
}};

constexpr std::array<int, 6> kPlaneIndices = {{
    0, 2, 1,
    1, 2, 3,
}};

constexpr std::array<std::array<int, 4>, 6> kCubeFaces = {{
    {4, 5, 6, 7},
    {1, 0, 3, 2},
    {0, 4, 7, 3},
    {5, 1, 2, 6},
    {3, 7, 6, 2},
    {0, 1, 5, 4},
}};

constexpr std::array<std::array<ri::math::Vec2, 4>, 6> kCubeFaceCornerUv = {{
    {ri::math::Vec2{0.0f, 0.0f},
     ri::math::Vec2{1.0f, 0.0f},
     ri::math::Vec2{1.0f, 1.0f},
     ri::math::Vec2{0.0f, 1.0f}},
    {ri::math::Vec2{0.0f, 0.0f},
     ri::math::Vec2{1.0f, 0.0f},
     ri::math::Vec2{1.0f, 1.0f},
     ri::math::Vec2{0.0f, 1.0f}},
    {ri::math::Vec2{0.0f, 0.0f},
     ri::math::Vec2{1.0f, 0.0f},
     ri::math::Vec2{1.0f, 1.0f},
     ri::math::Vec2{0.0f, 1.0f}},
    {ri::math::Vec2{0.0f, 0.0f},
     ri::math::Vec2{1.0f, 0.0f},
     ri::math::Vec2{1.0f, 1.0f},
     ri::math::Vec2{0.0f, 1.0f}},
    {ri::math::Vec2{0.0f, 0.0f},
     ri::math::Vec2{1.0f, 0.0f},
     ri::math::Vec2{1.0f, 1.0f},
     ri::math::Vec2{0.0f, 1.0f}},
    {ri::math::Vec2{0.0f, 0.0f},
     ri::math::Vec2{1.0f, 0.0f},
     ri::math::Vec2{1.0f, 1.0f},
     ri::math::Vec2{0.0f, 1.0f}},
}};

void ExpectVk(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(operation) + " failed with VkResult=" + std::to_string(static_cast<int>(result)));
    }
}

LRESULT CALLBACK NativePreviewWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    WindowState* state = reinterpret_cast<WindowState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<LPCREATESTRUCTW>(lParam);
        state = static_cast<WindowState*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        if (state != nullptr) {
            state->hwnd = hwnd;
        }
    }

    if (message != WM_NCCREATE && state != nullptr && state->onWin32Message != nullptr) {
        state->onWin32Message(state->messageUserData,
                              hwnd,
                              static_cast<unsigned int>(message),
                              static_cast<std::uint64_t>(wParam),
                              static_cast<std::int64_t>(lParam));
    }

    switch (message) {
    case WM_CLOSE:
        if (state != nullptr) {
            state->running = false;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (state != nullptr) {
            state->running = false;
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

std::wstring Widen(std::string_view text) {
    return std::wstring(text.begin(), text.end());
}

VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const VkSurfaceFormatKHR& format : formats) {
        if ((format.format == VK_FORMAT_B8G8R8A8_SRGB || format.format == VK_FORMAT_R8G8B8A8_SRGB) &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    for (const VkSurfaceFormatKHR& format : formats) {
        if ((format.format == VK_FORMAT_B8G8R8A8_UNORM || format.format == VK_FORMAT_R8G8B8A8_UNORM) &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    for (const VkSurfaceFormatKHR& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB || format.format == VK_FORMAT_B8G8R8A8_UNORM ||
            format.format == VK_FORMAT_R8G8B8A8_SRGB || format.format == VK_FORMAT_R8G8B8A8_UNORM) {
            return format;
        }
    }
    return formats.front();
}

const char* PresentModeName(const VkPresentModeKHR mode) {
    switch (mode) {
    case VK_PRESENT_MODE_MAILBOX_KHR:
        return "mailbox";
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
        return "immediate";
    case VK_PRESENT_MODE_FIFO_KHR:
        return "fifo";
    default:
        return "other";
    }
}

VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes,
                                   const VulkanPresentModePreference preference) {
    const auto hasMode = [&presentModes](const VkPresentModeKHR mode) {
        return std::find(presentModes.begin(), presentModes.end(), mode) != presentModes.end();
    };
    if (preference == VulkanPresentModePreference::Mailbox && hasMode(VK_PRESENT_MODE_MAILBOX_KHR)) {
        return VK_PRESENT_MODE_MAILBOX_KHR;
    }
    if (preference == VulkanPresentModePreference::Immediate && hasMode(VK_PRESENT_MODE_IMMEDIATE_KHR)) {
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
    if (preference == VulkanPresentModePreference::Fifo && hasMode(VK_PRESENT_MODE_FIFO_KHR)) {
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    for (const VkPresentModeKHR mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            // Prefer low-latency uncapped presentation for high-FPS native benchmarking.
            return mode;
        }
    }
    for (const VkPresentModeKHR mode : presentModes) {
        if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            return mode;
        }
    }
    for (const VkPresentModeKHR mode : presentModes) {
        if (mode == VK_PRESENT_MODE_FIFO_KHR) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

DeviceSelection PickDevice(VkInstance instance, VkSurfaceKHR surface) {
    std::uint32_t deviceCount = 0;
    ExpectVk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices(count)");
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan devices were found.");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    ExpectVk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), "vkEnumeratePhysicalDevices(list)");

    int bestScore = std::numeric_limits<int>::min();
    DeviceSelection best{};
    for (VkPhysicalDevice device : devices) {
        std::uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        std::optional<std::uint32_t> graphicsFamily;
        std::optional<std::uint32_t> presentFamily;
        for (std::uint32_t familyIndex = 0; familyIndex < queueFamilyCount; ++familyIndex) {
            if ((queueFamilies[familyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U && !graphicsFamily.has_value()) {
                graphicsFamily = familyIndex;
            }
            VkBool32 presentSupport = VK_FALSE;
            ExpectVk(vkGetPhysicalDeviceSurfaceSupportKHR(device, familyIndex, surface, &presentSupport),
                     "vkGetPhysicalDeviceSurfaceSupportKHR");
            if (presentSupport == VK_TRUE && !presentFamily.has_value()) {
                presentFamily = familyIndex;
            }
        }

        if (!graphicsFamily.has_value() || !presentFamily.has_value()) {
            continue;
        }

        std::uint32_t extensionCount = 0;
        ExpectVk(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr),
                 "vkEnumerateDeviceExtensionProperties(count)");
        std::vector<VkExtensionProperties> extensions(extensionCount);
        if (extensionCount > 0U) {
            ExpectVk(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data()),
                     "vkEnumerateDeviceExtensionProperties(list)");
        }

        bool hasSwapchain = false;
        for (const VkExtensionProperties& extension : extensions) {
            if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchain = true;
                break;
            }
        }
        if (!hasSwapchain) {
            continue;
        }

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);
        int score = 0;
        switch (properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: score += 400; break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score += 250; break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: score += 125; break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU: score += 50; break;
        default: score += 25; break;
        }
        score += static_cast<int>(properties.limits.maxImageDimension2D);
        if (score > bestScore) {
            bestScore = score;
            best = DeviceSelection{
                .physicalDevice = device,
                .graphicsQueueFamily = *graphicsFamily,
                .presentQueueFamily = *presentFamily,
            };
        }
    }

    if (best.physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("No Vulkan device with graphics, present, and swapchain support was found.");
    }
    return best;
}

std::uint32_t FindMemoryType(VkPhysicalDevice physicalDevice,
                             std::uint32_t typeBits,
                             VkMemoryPropertyFlags requiredFlags) {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    for (std::uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index) {
        const bool supported = (typeBits & (1U << index)) != 0U;
        const bool hasFlags = (memoryProperties.memoryTypes[index].propertyFlags & requiredFlags) == requiredFlags;
        if (supported && hasFlags) {
            return index;
        }
    }
    throw std::runtime_error("No suitable Vulkan memory type was found.");
}

VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice) {
    const std::array<VkFormat, 3> formats = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (const VkFormat format : formats) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
        constexpr VkFormatFeatureFlags required = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                  | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if ((properties.optimalTilingFeatures & required) == required) {
            return format;
        }
    }
    throw std::runtime_error("No supported Vulkan depth format (attachment + shader sampling) was found.");
}

VkFormat FindHdrSceneColorFormat(VkPhysicalDevice physicalDevice) {
    static_cast<void>(HybridPresentationFormats::kSceneHdrColor);
    const std::array<VkFormat, 2> formats = {
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
    };
    for (const VkFormat format : formats) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
        constexpr VkFormatFeatureFlags required =
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if ((properties.optimalTilingFeatures & required) == required) {
            return format;
        }
    }
    throw std::runtime_error("No Vulkan HDR scene color format (RGBA16F or RGBA32F with attachment + sampling).");
}

VkFormat FindShadowDepthFormat(VkPhysicalDevice physicalDevice) {
    const std::array<VkFormat, 3> formats = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (const VkFormat format : formats) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
        const VkFormatFeatureFlags required =
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if ((properties.optimalTilingFeatures & required) == required) {
            return format;
        }
    }
    throw std::runtime_error("No supported Vulkan shadow depth format was found.");
}

ri::math::Mat4 BuildLookAtMatrix(const ri::math::Vec3& eye, const ri::math::Vec3& target, const ri::math::Vec3& upHint) {
    const ri::math::Vec3 forward = ri::math::Normalize(target - eye);
    const ri::math::Vec3 right = ri::math::Normalize(ri::math::Cross(forward, upHint));
    const ri::math::Vec3 up = ri::math::Cross(right, forward);
    ri::math::Mat4 view = ri::math::IdentityMatrix();
    view.m[0][0] = right.x;
    view.m[0][1] = right.y;
    view.m[0][2] = right.z;
    view.m[0][3] = -ri::math::Dot(right, eye);
    view.m[1][0] = up.x;
    view.m[1][1] = up.y;
    view.m[1][2] = up.z;
    view.m[1][3] = -ri::math::Dot(up, eye);
    // Match the engine camera convention: objects in front of the camera live at positive view-space Z.
    view.m[2][0] = forward.x;
    view.m[2][1] = forward.y;
    view.m[2][2] = forward.z;
    view.m[2][3] = -ri::math::Dot(forward, eye);
    view.m[3][0] = 0.0f;
    view.m[3][1] = 0.0f;
    view.m[3][2] = 0.0f;
    view.m[3][3] = 1.0f;
    return view;
}

ri::math::Mat4 BuildOrthographicMatrix(const float left,
                                       const float right,
                                       const float bottom,
                                       const float top,
                                       const float nearPlane,
                                       const float farPlane) {
    ri::math::Mat4 projection{};
    projection.m[0][0] = 2.0f / std::max(right - left, 0.001f);
    projection.m[1][1] = 2.0f / std::max(top - bottom, 0.001f);
    // Vulkan uses clip-space depth in [0, 1], the same convention as the main preview camera.
    projection.m[2][2] = 1.0f / std::max(farPlane - nearPlane, 0.001f);
    projection.m[3][3] = 1.0f;
    projection.m[0][3] = -(right + left) / std::max(right - left, 0.001f);
    projection.m[1][3] = -(top + bottom) / std::max(top - bottom, 0.001f);
    projection.m[2][3] = -nearPlane / std::max(farPlane - nearPlane, 0.001f);
    return projection;
}

[[nodiscard]] bool NativeShaderBundleMarkerExists(const fs::path& directory) {
    std::error_code ec{};
    return fs::exists(directory / "NativeScenePreview.vert.spv", ec);
}

[[nodiscard]] bool NativePostTextureBundleMarkerExists(const fs::path& directory) {
    std::error_code ec{};
    return fs::exists(directory / "Layer.png", ec) && fs::exists(directory / "AreaTex.png", ec)
        && fs::exists(directory / "SearchTex.png", ec) && fs::exists(directory / "lut.png", ec);
}

[[nodiscard]] fs::path ExecutableDirectory() {
#if defined(_WIN32)
    std::wstring module(4096, L'\0');
    const DWORD n = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size() - 1U));
    if (n == 0U) {
        return {};
    }
    module.resize(n);
    return fs::path(module).parent_path();
#else
    return {};
#endif
}

[[nodiscard]] std::optional<fs::path> EnvironmentPath(const char* name) {
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t len = 0;
    if (_dupenv_s(&value, &len, name) != 0 || value == nullptr) {
        return std::nullopt;
    }
    const std::string owned(value);
    std::free(value);
    if (owned.empty()) {
        return std::nullopt;
    }
    return fs::path(owned);
#else
    if (const char* value = std::getenv(name); value != nullptr && value[0] != '\0') {
        return fs::path(value);
    }
    return std::nullopt;
#endif
}

template <typename Predicate>
[[nodiscard]] std::optional<fs::path> FindFirstMatchingDirectory(fs::path start, Predicate&& matches) {
    if (start.empty()) {
        return std::nullopt;
    }
    if (fs::is_regular_file(start)) {
        start = start.parent_path();
    }
    start = start.lexically_normal();
    for (int depth = 0; depth < 28; ++depth) {
        if (const fs::path sourceTreeCandidate = start / "Source" / "RawIron.Render.Vulkan";
            matches(sourceTreeCandidate)) {
            return sourceTreeCandidate;
        }
        if (!start.has_parent_path()) {
            break;
        }
        const fs::path parent = start.parent_path();
        if (parent == start) {
            break;
        }
        start = parent;
    }
    return std::nullopt;
}

/// SPIR-V was historically loaded only from the compile-time `RAWIRON_VULKAN_SHADER_DIR` (absolute).
/// That breaks when the build tree moves drives/machines. Prefer env / next to exe / walk up to
/// `Source/RawIron.Render.Vulkan/shaders` under the same CMake build prefix.
[[nodiscard]] fs::path ResolveVulkanNativeShaderDirectory() {
    if (const std::optional<fs::path> fromEnv = EnvironmentPath("RAWIRON_VULKAN_SHADER_DIR"); fromEnv.has_value()) {
        if (NativeShaderBundleMarkerExists(*fromEnv)) {
            return *fromEnv;
        }
    }
#if defined(RAWIRON_VULKAN_SHADER_DIR)
    {
        const fs::path compileTime(RAWIRON_VULKAN_SHADER_DIR);
        if (NativeShaderBundleMarkerExists(compileTime)) {
            return compileTime;
        }
    }
#endif
#if defined(_WIN32)
    {
        const fs::path exeDir = ExecutableDirectory();
        const fs::path besideVulkan = exeDir / "vulkan" / "shaders";
        if (NativeShaderBundleMarkerExists(besideVulkan)) {
            return besideVulkan;
        }
        const fs::path beside = exeDir / "shaders";
        if (NativeShaderBundleMarkerExists(beside)) {
            return beside;
        }
        if (const std::optional<fs::path> sourceTree = FindFirstMatchingDirectory(
                exeDir, [](const fs::path& candidateRoot) { return NativeShaderBundleMarkerExists(candidateRoot / "shaders"); });
            sourceTree.has_value()) {
            return *sourceTree / "shaders";
        }
    }
#endif
    if (const std::optional<fs::path> sourceTree = FindFirstMatchingDirectory(
            fs::current_path(), [](const fs::path& candidateRoot) { return NativeShaderBundleMarkerExists(candidateRoot / "shaders"); });
        sourceTree.has_value()) {
        return *sourceTree / "shaders";
    }
#if defined(RAWIRON_VULKAN_SHADER_DIR)
    return fs::path(RAWIRON_VULKAN_SHADER_DIR);
#else
    return {};
#endif
}

/// Post-process textures are part of the compiled native shader bundle. This keeps runtime behavior
/// independent from the migration-only ReferenceShaders tree and works for portable staged builds.
[[nodiscard]] fs::path ResolveVulkanNativePostTextureDirectory() {
    const fs::path nativeShaderRoot = ResolveVulkanNativeShaderDirectory();
    if (nativeShaderRoot.empty()) {
        return {};
    }
    const fs::path bundled = nativeShaderRoot / "NativeTextures";
    return NativePostTextureBundleMarkerExists(bundled) ? bundled : fs::path{};
}

std::vector<char> ReadBinaryFile(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw std::runtime_error("Unable to open shader file: " + path.string());
    }

    const std::streamsize size = stream.tellg();
    if (size <= 0) {
        throw std::runtime_error("Shader file is empty: " + path.string());
    }

    stream.seekg(0, std::ios::beg);
    std::vector<char> data(static_cast<std::size_t>(size));
    if (!stream.read(data.data(), size)) {
        throw std::runtime_error("Unable to read shader file: " + path.string());
    }
    return data;
}

VkShaderModule CreateShaderModule(VkDevice device, const fs::path& path) {
    const std::vector<char> bytes = ReadBinaryFile(path);
    const VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = bytes.size(),
        .pCode = reinterpret_cast<const std::uint32_t*>(bytes.data()),
    };
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    ExpectVk(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule), "vkCreateShaderModule");
    return shaderModule;
}

ri::math::Vec3 ClampColor(const ri::math::Vec3& color) {
    const auto safe = [](const float value) { return std::isfinite(value) ? value : 0.0f; };
    return ri::math::Vec3{
        std::clamp(safe(color.x), 0.0f, 1.0f),
        std::clamp(safe(color.y), 0.0f, 1.0f),
        std::clamp(safe(color.z), 0.0f, 1.0f),
    };
}

void PopulateNativeGpuLightingFromScene(const ri::scene::Scene& scene,
                                       const ri::math::Vec3& cameraWorldPos,
                                       NativeScenePreviewData& data,
                                       const std::uint32_t shadowMapResolution) {
    constexpr float kDirectionalRgbScale = 1.35f;
    constexpr float kEngineDefaultSunIntensity = 1.65f;
    constexpr float kFillRgbScale = 0.32f;
    constexpr float kFillSwitchMargin = 1.14f;

    const ri::math::Vec3 fillAnchor{
        cameraWorldPos.x,
        cameraWorldPos.y + 1.6f,
        cameraWorldPos.z,
    };

    ri::math::Vec3 sunToSurface = ri::math::Normalize(ri::math::Vec3{0.34f, 0.86f, 0.31f});
    ri::math::Vec3 sunRgb = ClampColor(ri::math::Vec3{1.0f, 0.98f, 0.94f}) * kDirectionalRgbScale;
    float sunIntensityMultiplier = kEngineDefaultSunIntensity;
    bool foundSun = false;

    ri::math::Vec3 bestFillPos = ri::math::Vec3{cameraWorldPos.x, cameraWorldPos.y + 0.15f, cameraWorldPos.z};
    ri::math::Vec3 bestFillRgb = ri::math::Vec3{1.0f, 0.93f, 0.84f};
    float bestFillRange = 24.0f;
    float bestFillStrength = -1.0f;
    int bestFillNode = ri::scene::kInvalidHandle;

    ri::math::Vec3 accumulatedRgb{0.0f, 0.0f, 0.0f};
    ri::math::Vec3 weightedFillPos{0.0f, 0.0f, 0.0f};
    float accumulatedWeight = 0.0f;
    float accumulatedRange = 8.0f;

    static int lockedFillNode = ri::scene::kInvalidHandle;
    ri::math::Vec3 lockedFillPos = bestFillPos;
    ri::math::Vec3 lockedFillRgb = bestFillRgb;
    float lockedFillRange = bestFillRange;
    float lockedFillStrength = -1.0f;

    const std::vector<int> lightNodes = ri::scene::CollectLightNodes(scene);
    for (const int nodeHandle : lightNodes) {
        const ri::scene::Node& node = scene.GetNode(nodeHandle);
        if (node.light == ri::scene::kInvalidHandle) {
            continue;
        }
        const ri::scene::Light& light = scene.GetLight(node.light);
        const ri::math::Mat4 world = scene.ComputeWorldMatrix(nodeHandle);
        const ri::math::Vec3 position = ri::math::ExtractTranslation(world);
        const ri::math::Vec3 forward = ri::math::ExtractForward(world);

        if (light.type == ri::scene::LightType::Directional) {
            if (!foundSun) {
                sunToSurface = ri::math::Normalize(forward * -1.0f);
                const float directionalIntensity = std::max(light.intensity, 0.05f);
                sunIntensityMultiplier =
                    std::clamp(0.45f + directionalIntensity * 0.72f, 0.85f, 2.75f);
                sunRgb = ClampColor(ri::math::Vec3{
                    light.color.x * directionalIntensity * kDirectionalRgbScale,
                    light.color.y * directionalIntensity * kDirectionalRgbScale,
                    light.color.z * directionalIntensity * kDirectionalRgbScale,
                });
                foundSun = true;
            }
            continue;
        }

        if (light.type != ri::scene::LightType::Point && light.type != ri::scene::LightType::Spot) {
            continue;
        }

        ri::math::Vec3 offsetLightMinusAnchor = position - fillAnchor;
        const float distance = ri::math::Length(offsetLightMinusAnchor);
        if (distance <= 1.0e-4f) {
            continue;
        }
        ri::math::Vec3 toLight = offsetLightMinusAnchor / distance;

        float attenuation = 1.0f;
        if (light.range > 0.001f) {
            attenuation = std::clamp(1.0f - (distance / light.range), 0.0f, 1.0f);
            attenuation *= attenuation;
        }

        if (light.type == ri::scene::LightType::Spot) {
            const ri::math::Vec3 spotForward = ri::math::Normalize(forward);
            const float cone = ri::math::Dot(spotForward * -1.0f, toLight);
            const float coneCutoff =
                std::cos(ri::math::DegreesToRadians(light.spotAngleDegrees * 0.5f));
            if (cone <= coneCutoff) {
                attenuation = 0.0f;
            } else {
                const float edge = std::max(1.0f - coneCutoff, 0.001f);
                attenuation *= std::pow(std::clamp((cone - coneCutoff) / edge, 0.0f, 1.0f), 2.0f);
            }
        }

        const float strength = attenuation * std::max(light.intensity, 0.001f);
        const ri::math::Vec3 lightRgb = ClampColor(ri::math::Vec3{
            light.color.x * light.intensity * kFillRgbScale,
            light.color.y * light.intensity * kFillRgbScale,
            light.color.z * light.intensity * kFillRgbScale,
        });
        if (nodeHandle == lockedFillNode) {
            lockedFillStrength = strength;
            lockedFillPos = position;
            lockedFillRgb = lightRgb;
            lockedFillRange = std::max(light.range, 6.0f);
        }
        if (strength > 0.06f) {
            accumulatedRgb = accumulatedRgb + (lightRgb * strength);
            weightedFillPos = weightedFillPos + (position * strength);
            accumulatedWeight += strength;
            accumulatedRange = std::max(accumulatedRange, std::max(light.range, 6.0f));
        }
        if (strength > bestFillStrength) {
            bestFillStrength = strength;
            bestFillNode = nodeHandle;
            bestFillPos = position;
            bestFillRgb = lightRgb;
            bestFillRange = std::max(light.range, 6.0f);
        }
    }

    if (accumulatedWeight > 0.08f) {
        bestFillPos = weightedFillPos / accumulatedWeight;
        bestFillRgb = accumulatedRgb / accumulatedWeight;
        bestFillStrength = std::min(accumulatedWeight, 3.8f);
        bestFillRange = accumulatedRange;
    }

    if (lockedFillNode != ri::scene::kInvalidHandle && lockedFillStrength > 0.0f &&
        lockedFillStrength * kFillSwitchMargin >= bestFillStrength * 0.85f) {
        bestFillPos = ri::math::Lerp(bestFillPos, lockedFillPos, 0.35f);
        bestFillRgb = ri::math::Lerp(bestFillRgb, lockedFillRgb, 0.25f);
        bestFillRange = std::max(bestFillRange, lockedFillRange);
        bestFillNode = lockedFillNode;
    } else if (bestFillNode != ri::scene::kInvalidHandle) {
        lockedFillNode = bestFillNode;
    }

    if (!foundSun) {
        sunRgb = ClampColor(ri::math::Vec3{1.0f, 0.98f, 0.94f}) * kDirectionalRgbScale;
        sunIntensityMultiplier = kEngineDefaultSunIntensity;
    }

    const ri::math::Vec3 shadowCenter = cameraWorldPos + ri::math::Vec3{0.0f, 3.0f, 0.0f};
    // sunToSurface points toward the sun; place the virtual light above the follow center.
    const ri::math::Vec3 lightEye = shadowCenter + sunToSurface * 120.0f;
    const ri::math::Mat4 lightView =
        BuildLookAtMatrix(lightEye, shadowCenter, ri::math::Vec3{0.0f, 1.0f, 0.0f});
    constexpr float orthoRadius = 90.0f;
    ri::math::Mat4 lightProjection =
        BuildOrthographicMatrix(-orthoRadius, orthoRadius, -orthoRadius, orthoRadius, 6.0f, 180.0f);

    // Stabilise the camera-following shadow map by snapping its projection to the
    // shadow-map texel grid at the follow center (not world origin). Without this,
    // the ortho slides continuously with the camera and shadow texels crawl across
    // surfaces, which reads as geometry shifting when the view moves.
    const float shadowMapResolutionF = static_cast<float>(std::max(shadowMapResolution, 1U));
    const float halfResolution = shadowMapResolutionF * 0.5f;
    {
        const ri::math::Mat4 unsnapped = ri::math::Multiply(lightProjection, lightView);
        const ri::math::Vec3 shadowNdc = ri::math::TransformPoint(unsnapped, shadowCenter);
        const float snappedX = std::round(shadowNdc.x * halfResolution) / halfResolution;
        const float snappedY = std::round(shadowNdc.y * halfResolution) / halfResolution;
        lightProjection.m[0][3] += (snappedX - shadowNdc.x);
        lightProjection.m[1][3] += (snappedY - shadowNdc.y);
    }

    const ri::math::Mat4 lightViewProjection = ri::math::Multiply(lightProjection, lightView);
    StoreMat4ColumnMajorGlsl(lightViewProjection, data.lightViewProjection.data());

    data.lightDirectionIntensity = {
        sunToSurface.x,
        sunToSurface.y,
        sunToSurface.z,
        sunIntensityMultiplier,
    };
    data.directionalLightColorIntensity = {
        sunRgb.x,
        sunRgb.y,
        sunRgb.z,
        1.0f,
    };

    data.localLightPositionRange = {bestFillPos.x, bestFillPos.y, bestFillPos.z, bestFillRange};
    const float fillW = std::max(bestFillStrength, 0.25f);
    data.localLightColorIntensity = {bestFillRgb.x, bestFillRgb.y, bestFillRgb.z, fillW};
}

void SetNativeVertex(NativeSceneVertex& vertex,
                     const ri::math::Vec3& position,
                     const ri::math::Vec3& normal,
                     const ri::math::Vec2& uv) {
    vertex.position[0] = position.x;
    vertex.position[1] = position.y;
    vertex.position[2] = position.z;
    vertex.normal[0] = normal.x;
    vertex.normal[1] = normal.y;
    vertex.normal[2] = normal.z;
    vertex.uv[0] = uv.x;
    vertex.uv[1] = uv.y;
}

CpuMeshGeometry BuildCubeMeshGeometryExpanded() {
    CpuMeshGeometry geometry{};
    geometry.vertices.reserve(36U);
    geometry.indices.reserve(36U);
    for (std::size_t face = 0; face < kCubeFaces.size(); ++face) {
        const std::array<int, 4>& faceIdx = kCubeFaces[face];
        const ri::math::Vec3& p0 = kCubeVertices[static_cast<std::size_t>(faceIdx[0])];
        const ri::math::Vec3& p1 = kCubeVertices[static_cast<std::size_t>(faceIdx[1])];
        const ri::math::Vec3& p2 = kCubeVertices[static_cast<std::size_t>(faceIdx[2])];
        const ri::math::Vec3 faceNormal = ri::math::Normalize(ri::math::Cross(p1 - p0, p2 - p0));
        const auto emitCorner = [&](int cornerIndex) {
            const ri::math::Vec3& p = kCubeVertices[static_cast<std::size_t>(faceIdx[static_cast<std::size_t>(cornerIndex)])];
            const ri::math::Vec2& uv = kCubeFaceCornerUv[face][static_cast<std::size_t>(cornerIndex)];
            NativeSceneVertex vertex{};
            SetNativeVertex(vertex, p, faceNormal, uv);
            geometry.vertices.push_back(vertex);
            geometry.indices.push_back(static_cast<std::uint32_t>(geometry.vertices.size() - 1U));
        };
        emitCorner(0);
        emitCorner(1);
        emitCorner(2);
        emitCorner(0);
        emitCorner(2);
        emitCorner(3);
    }
    return geometry;
}

CpuMeshGeometry BuildPlaneMeshGeometryUv() {
    CpuMeshGeometry geometry{};
    geometry.vertices.reserve(4U);
    geometry.indices.reserve(6U);
    std::vector<ri::math::Vec3> normals(4U, ri::math::Vec3{0.0f, 1.0f, 0.0f});
    for (std::size_t i = 0; i < kPlaneVertices.size(); ++i) {
        const ri::math::Vec3& p = kPlaneVertices[i];
        const ri::math::Vec2 uv{p.x + 0.5f, p.z + 0.5f};
        NativeSceneVertex vertex{};
        SetNativeVertex(vertex, p, normals[i], uv);
        geometry.vertices.push_back(vertex);
    }
    for (const int index : kPlaneIndices) {
        geometry.indices.push_back(static_cast<std::uint32_t>(index));
    }
    return geometry;
}

CpuMeshGeometry BuildIndexedMeshGeometryUv(const std::vector<ri::math::Vec3>& positions,
                                           const std::vector<ri::math::Vec3>* explicitNormals,
                                           const std::vector<ri::math::Vec2>& texCoords,
                                           const std::vector<std::uint32_t>& indices,
                                           bool hasUv,
                                           bool flatShaded) {
    CpuMeshGeometry geometry{};
    if (positions.empty() || indices.empty() || (indices.size() % 3U) != 0U) {
        return geometry;
    }

    std::vector<ri::math::Vec3> vertexNormals(positions.size(), ri::math::Vec3{0.0f, 0.0f, 0.0f});
    if (explicitNormals != nullptr && explicitNormals->size() == positions.size()) {
        for (std::size_t index = 0; index < positions.size(); ++index) {
            ri::math::Vec3 normal = (*explicitNormals)[index];
            if (ri::math::Length(normal) <= 0.0001f) {
                normal = ri::math::Vec3{0.0f, 1.0f, 0.0f};
            } else {
                normal = ri::math::Normalize(normal);
            }
            vertexNormals[index] = normal;
        }
    } else if (!flatShaded) {
        for (std::size_t triangleIndex = 0; triangleIndex + 2U < indices.size(); triangleIndex += 3U) {
            const std::uint32_t ia = indices[triangleIndex + 0U];
            const std::uint32_t ib = indices[triangleIndex + 1U];
            const std::uint32_t ic = indices[triangleIndex + 2U];
            if (ia >= positions.size() || ib >= positions.size() || ic >= positions.size()) {
                continue;
            }
            const ri::math::Vec3 faceNormal =
                ri::math::Cross(positions[ib] - positions[ia], positions[ic] - positions[ia]);
            vertexNormals[ia] = vertexNormals[ia] + faceNormal;
            vertexNormals[ib] = vertexNormals[ib] + faceNormal;
            vertexNormals[ic] = vertexNormals[ic] + faceNormal;
        }
        for (std::size_t index = 0; index < positions.size(); ++index) {
            ri::math::Vec3 normal = vertexNormals[index];
            if (ri::math::Length(normal) <= 0.0001f) {
                normal = ri::math::Vec3{0.0f, 1.0f, 0.0f};
            } else {
                normal = ri::math::Normalize(normal);
            }
            vertexNormals[index] = normal;
        }
    }

    geometry.vertices.reserve(indices.size());
    geometry.indices.reserve(indices.size());
    for (std::size_t triangleIndex = 0; triangleIndex + 2U < indices.size(); triangleIndex += 3U) {
        const std::uint32_t ia = indices[triangleIndex + 0U];
        const std::uint32_t ib = indices[triangleIndex + 1U];
        const std::uint32_t ic = indices[triangleIndex + 2U];
        if (ia >= positions.size() || ib >= positions.size() || ic >= positions.size()) {
            continue;
        }
        const auto cornerUv = [&](std::uint32_t vertexIndex) -> ri::math::Vec2 {
            if (hasUv && vertexIndex < texCoords.size()) {
                return texCoords[vertexIndex];
            }
            return ri::math::Vec2{0.0f, 0.0f};
        };
        ri::math::Vec3 faceNormal{0.0f, 1.0f, 0.0f};
        if (flatShaded && explicitNormals == nullptr) {
            faceNormal = ri::math::Cross(positions[ib] - positions[ia], positions[ic] - positions[ia]);
            if (ri::math::Length(faceNormal) <= 0.0001f) {
                faceNormal = ri::math::Vec3{0.0f, 1.0f, 0.0f};
            } else {
                faceNormal = ri::math::Normalize(faceNormal);
            }
        }
        for (const std::uint32_t corner : {ia, ib, ic}) {
            NativeSceneVertex vertex{};
            SetNativeVertex(vertex,
                            positions[corner],
                            flatShaded && explicitNormals == nullptr ? faceNormal : vertexNormals[corner],
                            cornerUv(corner));
            geometry.vertices.push_back(vertex);
            geometry.indices.push_back(static_cast<std::uint32_t>(geometry.vertices.size() - 1U));
        }
    }
    return geometry;
}

std::vector<std::uint32_t> BuildSequentialIndices(const std::vector<ri::math::Vec3>& positions) {
    std::vector<std::uint32_t> indices;
    indices.reserve(positions.size());
    for (std::size_t index = 0; index < positions.size(); ++index) {
        indices.push_back(static_cast<std::uint32_t>(index));
    }
    return indices;
}

CpuMeshGeometry BuildNativeMeshGeometry(const ri::scene::Mesh& mesh) {
    switch (mesh.primitive) {
    case ri::scene::PrimitiveType::Cube:
        return BuildCubeMeshGeometryExpanded();
    case ri::scene::PrimitiveType::Plane:
        return BuildPlaneMeshGeometryUv();
    case ri::scene::PrimitiveType::Sphere:
    case ri::scene::PrimitiveType::Custom: {
        const bool hasUv = mesh.texCoords.size() == mesh.positions.size();
        const std::vector<ri::math::Vec3>* explicitNormals =
            mesh.normals.size() == mesh.positions.size() ? &mesh.normals : nullptr;
        const bool flatShaded = mesh.primitive == ri::scene::PrimitiveType::Custom && explicitNormals == nullptr;
        if (!mesh.indices.empty()) {
            std::vector<std::uint32_t> indices;
            indices.reserve(mesh.indices.size());
            for (const int index : mesh.indices) {
                if (index >= 0) {
                    indices.push_back(static_cast<std::uint32_t>(index));
                }
            }
            return BuildIndexedMeshGeometryUv(mesh.positions,
                                              explicitNormals,
                                              mesh.texCoords,
                                              indices,
                                              hasUv,
                                              flatShaded);
        }
        return BuildIndexedMeshGeometryUv(
            mesh.positions,
            explicitNormals,
            mesh.texCoords,
            BuildSequentialIndices(mesh.positions),
            hasUv,
            flatShaded);
    }
    }
    return {};
}

[[nodiscard]] std::string MaterialNameLowerAscii(const ri::scene::Material& material) {
    std::string out = material.name;
    for (char& character : out) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return out;
}

[[nodiscard]] bool NativePreviewIsWaterLikeMaterial(const ri::scene::Material& material) {
    const std::string lower = MaterialNameLowerAscii(material);
    if (lower.find("water") != std::string::npos) {
        return true;
    }
    if (material.baseColorTextureFrames.size() >= 8U && material.baseColorTextureFramesPerSecond >= 4.0f) {
        return true;
    }
    return false;
}

[[nodiscard]] bool NativePreviewPathUsesMetalLookupPalette(const std::string& path) {
    std::string lower = path;
    for (char& character : lower) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return lower.find("metalcolormap.png") != std::string::npos;
}

[[nodiscard]] bool NativePreviewUsesMetalLookupPalette(const ri::scene::Material& material,
                                                       const std::string& resolvedAlbedoPath) {
    return NativePreviewPathUsesMetalLookupPalette(material.baseColorTexture) ||
           NativePreviewPathUsesMetalLookupPalette(resolvedAlbedoPath);
}

[[nodiscard]] std::string ResolveNativePreviewAlbedoRelPath(const ri::scene::Material& material,
                                                            double animationTimeSeconds) {
    if (!material.baseColorTexture.empty() && fs::path(material.baseColorTexture).is_absolute()) {
        return material.baseColorTexture;
    }
    if (!material.baseColorTextureFrames.empty()) {
        std::size_t frameIndex = 0;
        if (material.baseColorTextureFramesPerSecond > 0.0f && material.baseColorTextureFrames.size() > 1U) {
            const double frameCursor =
                std::floor(std::max(0.0, animationTimeSeconds)
                            * static_cast<double>(material.baseColorTextureFramesPerSecond));
            frameIndex = static_cast<std::size_t>(
                static_cast<long long>(frameCursor) % static_cast<long long>(material.baseColorTextureFrames.size()));
        }
        const std::string& framePath = material.baseColorTextureFrames[frameIndex];
        if (!framePath.empty()) {
            return framePath;
        }
    }
    return material.baseColorTexture;
}

bool BuildNativeScenePreviewData(const ri::scene::Scene& scene,
                                 int cameraNode,
                                 int width,
                                 int height,
                                 const ri::scene::PhotoModeCameraOverrides* photoMode,
                                 const fs::path& textureRoot,
                                 const fs::path& skyEquirectRelativeToTextureRoot,
                                 double animationTimeSeconds,
                                 const std::optional<ri::math::Vec3>& environmentClearColor,
                                 const ri::math::Mat4* cameraWorldOverride,
                                 NativeScenePreviewData* outData,
                                 std::string* error,
                                 const std::uint32_t shadowMapResolution) {
    try {
        if (outData == nullptr) {
            throw std::runtime_error("Native scene preview output was null.");
        }
        auto absolutePathExistsCached = [](const fs::path& path) {
            static std::unordered_map<std::string, bool> cache;
            const std::string key = path.lexically_normal().string();
            const auto found = cache.find(key);
            if (found != cache.end()) {
                return found->second;
            }
            const bool exists = fs::is_regular_file(path);
            cache.emplace(key, exists);
            return exists;
        };

        ri::scene::SceneRenderSubmissionOptions submissionOptions{};
        submissionOptions.viewportWidth = std::max(width, 1);
        submissionOptions.viewportHeight = std::max(height, 1);
        if (environmentClearColor.has_value()) {
            submissionOptions.clearColor = *environmentClearColor;
        }
        // Native Vulkan draws the full authored scene each frame; distance/occlusion LOD from the software
        // preview path causes visible popping on large void levels, so keep those toggles off here.
        submissionOptions.enableFarHorizon = false;
        submissionOptions.enableCoarseOcclusion = false;
        submissionOptions.enableFrustumCulling = false;
        if (photoMode != nullptr && ri::scene::PhotoModeFieldOfViewActive(*photoMode)) {
            submissionOptions.photoMode = *photoMode;
        }

        const ri::scene::SceneRenderSubmission submission = ri::scene::BuildSceneRenderSubmission(
            scene,
            cameraNode,
            submissionOptions);
        if (submission.stats.cameraNodeHandle == ri::scene::kInvalidHandle) {
            throw std::runtime_error("Scene preview does not expose a valid camera for native Vulkan rendering.");
        }

        const ri::core::RenderSubmissionPlan plan = ri::core::BuildRenderSubmissionPlan(submission.commands);
        VulkanCommandListSink sink{};
        ri::core::RenderRecorderStats recorderStats{};
        if (!ri::core::ExecuteRenderSubmissionPlan(submission.commands, plan, sink, &recorderStats)) {
            throw std::runtime_error("Failed to execute render submission plan for native Vulkan rendering.");
        }

        NativeScenePreviewData& data = *outData;
        data.scene = &scene;
        data.textureRoot = textureRoot;
        data.viewProjection = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
        data.clearColor = {0.05f, 0.07f, 0.10f, 1.0f};
        data.sceneAnimationTimeSeconds = static_cast<float>(animationTimeSeconds);
        data.cameraWorldPosition = {{0.0f, 0.0f, 0.0f, 1.0f}};
        data.skyClipFromLocal.fill(0.0f);
        data.skyEyeToWorld.fill(0.0f);
        data.skyUseTextureFile = 0;
        data.skyUseAuthoredGradient = 0;
        data.skyHorizonColor = {0.82f, 0.82f, 0.80f, 1.0f};
        data.skyZenithColor = {0.54f, 0.56f, 0.57f, 1.0f};
        data.skyEquirectAbsolute.clear();
        data.draws.clear();
        const int submissionCamera = submission.stats.cameraNodeHandle;
        if (submissionCamera != ri::scene::kInvalidHandle) {
            const ri::math::Vec3 cameraWorld = cameraWorldOverride != nullptr
                ? ri::math::ExtractTranslation(*cameraWorldOverride)
                : scene.ComputeWorldPosition(submissionCamera);
            data.cameraWorldPosition = {{cameraWorld.x, cameraWorld.y, cameraWorld.z, 1.0f}};
            const float aspectRatio = static_cast<float>(std::max(width, 1))
                / static_cast<float>(std::max(height, 1));
            const ri::math::Mat4 projection = ri::scene::BuildCameraProjectionMatrix(
                scene,
                submissionCamera,
                aspectRatio,
                photoMode);
            ri::math::Mat4 skyRotation{};
            if (cameraWorldOverride != nullptr) {
                if (!ri::math::TryInvertAffineMat4(*cameraWorldOverride, skyRotation)) {
                    throw std::runtime_error("Native Vulkan camera override was singular.");
                }
                skyRotation.m[0][3] = 0.0f;
                skyRotation.m[1][3] = 0.0f;
                skyRotation.m[2][3] = 0.0f;
            } else {
                skyRotation = ri::scene::BuildCameraSkyRotationMatrix(scene, submissionCamera);
            }
            const ri::math::Mat4 clipSky = ri::math::Multiply(projection, skyRotation);
            ri::math::Mat4 eyeToWorld = ri::math::IdentityMatrix();
            for (int row = 0; row < 3; ++row) {
                for (int column = 0; column < 3; ++column) {
                    eyeToWorld.m[row][column] = skyRotation.m[column][row];
                }
            }
            StoreMat4ColumnMajorGlsl(clipSky, data.skyClipFromLocal);
            StoreMat4ColumnMajorGlsl(eyeToWorld, data.skyEyeToWorld);
            if (!skyEquirectRelativeToTextureRoot.empty()) {
                fs::path absoluteSky = skyEquirectRelativeToTextureRoot.lexically_normal();
                if (!skyEquirectRelativeToTextureRoot.is_absolute() && !textureRoot.empty()) {
                    absoluteSky = (textureRoot / skyEquirectRelativeToTextureRoot).lexically_normal();
                }
                if (absolutePathExistsCached(absoluteSky)) {
                    data.skyUseTextureFile = 1;
                    data.skyEquirectAbsolute = std::move(absoluteSky);
                }
            }
        }
        for (const VulkanRenderOp& operation : sink.Operations()) {
            switch (operation.type) {
            case VulkanRenderOpType::ClearColor:
                data.clearColor = {
                    operation.clearColor[0],
                    operation.clearColor[1],
                    operation.clearColor[2],
                    operation.clearColor[3],
                };
                break;
            case VulkanRenderOpType::SetViewProjection:
                std::copy(std::begin(operation.viewProjection), std::end(operation.viewProjection), data.viewProjection.begin());
                break;
            case VulkanRenderOpType::DrawMesh: {
                if (operation.meshHandle < 0) {
                    break;
                }
                const ri::scene::Material material = operation.materialHandle >= 0
                    ? scene.GetMaterial(operation.materialHandle)
                    : ri::scene::Material{};
                const ri::math::Vec3 color = ClampColor(material.baseColor);
                NativeSceneDraw draw{};
                draw.meshHandle = operation.meshHandle;
                draw.materialHandle = operation.materialHandle;
                draw.firstIndex = operation.firstIndex;
                draw.indexCount = operation.indexCount;
                draw.instanceCount = std::max(operation.instanceCount, 1U);
                std::copy(std::begin(operation.model), std::end(operation.model), draw.model.begin());
                draw.color = {
                    color.x,
                    color.y,
                    color.z,
                    material.transparent ? std::clamp(material.opacity, 0.0f, 1.0f) : 1.0f,
                };
                if (material.alphaCutoff > 0.01f && material.alphaCutoff < 0.99f && !material.transparent) {
                    draw.alphaCutout = true;
                    draw.alphaCutoff = std::clamp(material.alphaCutoff, 0.01f, 0.99f);
                }
                draw.transparent = material.transparent;
                draw.emissiveColor = {
                    std::clamp(material.emissiveColor.x, 0.0f, 16.0f),
                    std::clamp(material.emissiveColor.y, 0.0f, 16.0f),
                    std::clamp(material.emissiveColor.z, 0.0f, 16.0f),
                };
                draw.metallic = std::clamp(material.metallic, 0.0f, 1.0f);
                draw.roughness = std::clamp(material.roughness, 0.04f, 1.0f);
                draw.textureTiling = {material.textureTiling.x, material.textureTiling.y};
                {
                    const bool hasNamedTexture = !material.baseColorTexture.empty();
                    const bool hasFrameTexture =
                        !material.baseColorTextureFrames.empty() && !material.baseColorTextureFrames.front().empty();
                    draw.useTexture = hasNamedTexture || hasFrameTexture || !material.normalTexture.empty()
                                      || !material.ormTexture.empty() || !material.emissiveTexture.empty()
                                      || !material.opacityTexture.empty() || !material.detailTexture.empty();
                }
                if (draw.useTexture) {
                    draw.resolvedAlbedoRelPath = ResolveNativePreviewAlbedoRelPath(material, animationTimeSeconds);
                    draw.nativeWaterUvMotion = NativePreviewIsWaterLikeMaterial(material);
                }
                draw.litShadingModel = material.shadingModel == ri::scene::ShadingModel::Lit;
                switch (material.materialStyle) {
                    case ri::scene::MaterialStyle::Retro:
                        draw.materialStyleFlags |= kNativeMaterialStyleRetro;
                        break;
                    case ri::scene::MaterialStyle::Layered:
                        draw.materialStyleFlags |= kNativeMaterialStyleLayered;
                        break;
                    case ri::scene::MaterialStyle::MixedMedia:
                        draw.materialStyleFlags |= kNativeMaterialStyleMixedMedia;
                        break;
                    case ri::scene::MaterialStyle::Crystal:
                        draw.materialStyleFlags |= kNativeMaterialStyleCrystal;
                        break;
                    case ri::scene::MaterialStyle::Standard:
                    default:
                        break;
                }
                if (material.materialWorkflow == ri::scene::MaterialWorkflow::SpecGloss) {
                    draw.materialStyleFlags |= kNativeMaterialWorkflowSpecGloss;
                }
                const bool metalLookupMaterial =
                    NativePreviewUsesMetalLookupPalette(material, draw.resolvedAlbedoRelPath);
                if (metalLookupMaterial) {
                    draw.materialStyleFlags |= kNativeMaterialStyleMetalLookup;
                }
                if (material.bakedWorldTileUv) {
                    draw.materialStyleFlags |= kNativeMaterialWorldTileUv;
                }
                if (material.albedoAlphaIsSmoothness) {
                    draw.materialStyleFlags |= kNativeMaterialAlbedoAlphaSmoothness;
                }
                const bool hasRealAlbedo =
                    !draw.resolvedAlbedoRelPath.empty()
                    && (!material.baseColorTexture.empty()
                        || (!material.baseColorTextureFrames.empty()
                            && !material.baseColorTextureFrames.front().empty()));
                const bool canGenerateMaps = kNativeGenerateMissingMaterialMaps && draw.litShadingModel
                                             && draw.useTexture && hasRealAlbedo && !metalLookupMaterial
                                             && !material.albedoAlphaIsSmoothness;
                const bool specGlossWorkflow =
                    material.materialWorkflow == ri::scene::MaterialWorkflow::SpecGloss;
                // Normal maps are workflow-agnostic, but the generated map is ORM-packed
                // (R=AO,G=rough,B=metal); SpecGloss materials would misread it as a spec
                // colour, so only hydrate ORM for the MetalRough path.
                if (!material.bakedWorldTileUv
                    && (!material.normalTexture.empty() || canGenerateMaps)) {
                    draw.materialStyleFlags |= kNativeMaterialHasNormalMap;
                }
                if (!material.ormTexture.empty() || (canGenerateMaps && !specGlossWorkflow)) {
                    draw.materialStyleFlags |= kNativeMaterialHasOrmMap;
                }
                draw.doubleSided = material.doubleSided;
                draw.additiveBlend = material.additiveBlend;
                {
                    const float dx = draw.model[12] - data.cameraWorldPosition[0];
                    const float dy = draw.model[13] - data.cameraWorldPosition[1];
                    const float dz = draw.model[14] - data.cameraWorldPosition[2];
                    draw.sortDepthSq = (dx * dx) + (dy * dy) + (dz * dz);
                }
                data.draws.push_back(draw);
                break;
            }
            default:
                break;
            }
        }

        if (submissionCamera != ri::scene::kInvalidHandle) {
            const float aspectRatio = static_cast<float>(std::max(width, 1))
                / static_cast<float>(std::max(height, 1));
            ri::math::Mat4 view{};
            if (cameraWorldOverride != nullptr) {
                if (!ri::math::TryInvertAffineMat4(*cameraWorldOverride, view)) {
                    throw std::runtime_error("Native Vulkan camera override was singular.");
                }
            } else {
                view = ri::scene::BuildCameraViewMatrix(scene, submissionCamera);
            }
            const ri::math::Mat4 projection = ri::scene::BuildCameraProjectionMatrix(
                scene,
                submissionCamera,
                aspectRatio,
                photoMode);
            StoreMat4ColumnMajorGlsl(ri::math::Multiply(projection, view), data.viewProjection);
        }

        {
            const ri::math::Vec3 cameraPos{
                data.cameraWorldPosition[0],
                data.cameraWorldPosition[1],
                data.cameraWorldPosition[2],
            };
            PopulateNativeGpuLightingFromScene(scene, cameraPos, data, shadowMapResolution);
        }

        return true;
    } catch (const std::exception& exception) {
        if (error != nullptr) {
            *error = exception.what();
        }
        return false;
    }
}

bool BuildNativeScenePreviewData(const VulkanNativeSceneFrame& frame,
                                 int width,
                                 int height,
                                 NativeScenePreviewData* outData,
                                 std::string* error,
                                 const std::uint32_t shadowMapResolution) {
    if (frame.scene == nullptr) {
        if (error != nullptr) {
            *error = "Native Vulkan scene callback did not provide a scene.";
        }
        return false;
    }
    if (frame.cameraNode == ri::scene::kInvalidHandle) {
        if (error != nullptr) {
            *error = "Native Vulkan scene callback did not provide a valid camera node.";
        }
        return false;
    }
    const ri::scene::PhotoModeCameraOverrides* const photoMode =
        frame.photoModeEnabled && ri::scene::PhotoModeFieldOfViewActive(frame.photoMode) ? &frame.photoMode : nullptr;
    const fs::path textureRoot = !frame.textureRoot.empty() ? frame.textureRoot : fs::path{};
    std::optional<ri::math::Vec3> environmentClearColor{};
    if (frame.useEnvironmentClear) {
        environmentClearColor = (frame.environmentClearTop + frame.environmentClearBottom) * 0.5f;
    }
    const bool ok = BuildNativeScenePreviewData(*frame.scene,
                                                frame.cameraNode,
                                                width,
                                                height,
                                                photoMode,
                                                textureRoot,
                                                frame.skyEquirectTextureRelative,
                                                frame.animationTimeSeconds,
                                                environmentClearColor,
                                                frame.cameraWorldOverrideEnabled ? &frame.cameraWorldOverride : nullptr,
                                                outData,
                                                error,
                                                shadowMapResolution);
    if (!ok) {
        return false;
    }
    const VulkanNativeSceneResolvedTuning resolvedTuning = ResolveVulkanNativeSceneTuning(frame);
    if (frame.useEnvironmentClear) {
        const ri::math::Vec3 horizon = resolvedTuning.environmentBottom;
        const ri::math::Vec3 zenith = resolvedTuning.environmentTop;
        outData->skyUseAuthoredGradient = 1;
        outData->skyHorizonColor = {horizon.x, horizon.y, horizon.z, 1.0f};
        outData->skyZenithColor = {zenith.x, zenith.y, zenith.z, 1.0f};
    } else {
        outData->skyUseAuthoredGradient = 0;
        outData->skyHorizonColor = {0.82f, 0.82f, 0.80f, 1.0f};
        outData->skyZenithColor = {0.54f, 0.56f, 0.57f, 1.0f};
    }
    const float fogStart = resolvedTuning.fogStart;
    const float fogEnd = resolvedTuning.fogEnd;
    const bool linearFog = resolvedTuning.linearFog;
    outData->renderTuning = {
        resolvedTuning.exposure,
        resolvedTuning.contrast,
        resolvedTuning.saturation,
        resolvedTuning.fogAmount,
    };
    const ri::math::Vec3 fogNear = resolvedTuning.fogColorNear;
    const ri::math::Vec3 fogFar = resolvedTuning.fogColorFar;
    const ri::math::Vec3 ambient = resolvedTuning.ambientLight;
    const ri::render::PostProcessParameters sanitizedPost = ri::render::SanitizePostProcessParameters(frame.postProcess);
    outData->postProcessPrimary = {
        sanitizedPost.noiseAmount,
        sanitizedPost.scanlineAmount,
        sanitizedPost.barrelDistortion,
        sanitizedPost.chromaticAberration,
    };
    outData->postProcessTint = {
        sanitizedPost.tintColor.x,
        sanitizedPost.tintColor.y,
        sanitizedPost.tintColor.z,
        sanitizedPost.tintStrength,
    };
    outData->postProcessSecondary = {
        sanitizedPost.blurAmount,
        sanitizedPost.staticFadeAmount,
        sanitizedPost.timeSeconds,
        0.0f,
    };
    outData->presentationTuning = {
        sanitizedPost.casSharpenAmount,
        sanitizedPost.casContrastAdaptation,
        sanitizedPost.bloomIntensity,
        sanitizedPost.bloomThreshold,
    };
    outData->presentationColorGrading = {
        sanitizedPost.toneCurveStrength,
        sanitizedPost.outputDitherStrength,
        sanitizedPost.debandStrength,
        sanitizedPost.vignetteStrength,
    };
    outData->presentationExtra = {
        sanitizedPost.filmGrainIntensity,
        ambient.x,
        ambient.y,
        ambient.z,
    };
    outData->lggLiftMix = {
        sanitizedPost.liftRgb.x,
        sanitizedPost.liftRgb.y,
        sanitizedPost.liftRgb.z,
        sanitizedPost.liftGammaGainMix,
    };
    outData->lggGammaRgb = {
        sanitizedPost.gammaRgb.x,
        sanitizedPost.gammaRgb.y,
        sanitizedPost.gammaRgb.z,
        0.0f,
    };
    outData->lggGainRgb = {
        sanitizedPost.gainRgb.x,
        sanitizedPost.gainRgb.y,
        sanitizedPost.gainRgb.z,
        0.0f,
    };
    outData->vibranceBalanceAmount = {
        sanitizedPost.vibranceRgbBalance.x,
        sanitizedPost.vibranceRgbBalance.y,
        sanitizedPost.vibranceRgbBalance.z,
        sanitizedPost.vibrance,
    };
    outData->technicolor1PowStrNegRg = {
        sanitizedPost.technicolorPower,
        sanitizedPost.technicolorStrength,
        sanitizedPost.technicolorRgbNegative.x,
        sanitizedPost.technicolorRgbNegative.y,
    };
    outData->technicolor1NegBPad = {
        sanitizedPost.technicolorRgbNegative.z,
        0.0f,
        0.0f,
        0.0f,
    };
    outData->technicolor2ColBright = {
        sanitizedPost.technicolor2ColorStrength.x,
        sanitizedPost.technicolor2ColorStrength.y,
        sanitizedPost.technicolor2ColorStrength.z,
        sanitizedPost.technicolor2Brightness,
    };
    outData->technicolor2SatStrPad = {
        sanitizedPost.technicolor2Saturation,
        sanitizedPost.technicolor2Strength,
        0.0f,
        0.0f,
    };
    outData->sepiaTintXyzStrength = {
        sanitizedPost.sepiaTint.x,
        sanitizedPost.sepiaTint.y,
        sanitizedPost.sepiaTint.z,
        sanitizedPost.sepiaStrength,
    };
    outData->monochromePresetSat = {
        static_cast<float>(sanitizedPost.monochromePreset),
        sanitizedPost.monochromeColorSaturation,
        0.0f,
        0.0f,
    };
    outData->monochromeCustomCoeff = {
        sanitizedPost.monochromeCustomCoeff.x,
        sanitizedPost.monochromeCustomCoeff.y,
        sanitizedPost.monochromeCustomCoeff.z,
        0.0f,
    };
    outData->dpxRgbCurvePad = {
        sanitizedPost.dpxRgbCurve.x,
        sanitizedPost.dpxRgbCurve.y,
        sanitizedPost.dpxRgbCurve.z,
        0.0f,
    };
    outData->dpxRgbCPad = {
        sanitizedPost.dpxRgbC.x,
        sanitizedPost.dpxRgbC.y,
        sanitizedPost.dpxRgbC.z,
        0.0f,
    };
    outData->dpxContrastSatColorStr = {
        sanitizedPost.dpxContrast,
        sanitizedPost.dpxSaturation,
        sanitizedPost.dpxColorfulness,
        sanitizedPost.dpxStrength,
    };
    outData->colorMatrixRowR = {
        sanitizedPost.colorMatrixRed.x,
        sanitizedPost.colorMatrixRed.y,
        sanitizedPost.colorMatrixRed.z,
        0.0f,
    };
    outData->colorMatrixRowG = {
        sanitizedPost.colorMatrixGreen.x,
        sanitizedPost.colorMatrixGreen.y,
        sanitizedPost.colorMatrixGreen.z,
        0.0f,
    };
    outData->colorMatrixRowBStr = {
        sanitizedPost.colorMatrixBlue.x,
        sanitizedPost.colorMatrixBlue.y,
        sanitizedPost.colorMatrixBlue.z,
        sanitizedPost.colorMatrixStrength,
    };
    outData->fakeHdrPowerR1R2Str = {
        sanitizedPost.fakeHdrPower,
        sanitizedPost.fakeHdrRadius1,
        sanitizedPost.fakeHdrRadius2,
        sanitizedPost.fakeHdrStrength,
    };
    outData->levelsBlackWhiteStrClip = {
        sanitizedPost.levelsBlackPoint,
        sanitizedPost.levelsWhitePoint,
        sanitizedPost.levelsStrength,
        sanitizedPost.levelsClipHighlight,
    };
    {
        const int pat = std::clamp(sanitizedPost.lumaSharpenPattern, 0, 3);
        const int showPacked =
            (sanitizedPost.lumaSharpenShowPattern >= 0.5f && std::isfinite(sanitizedPost.lumaSharpenShowPattern))
            ? 1
            : 0;
        outData->lumaSharpenPack = {
            sanitizedPost.lumaSharpenStrength,
            sanitizedPost.lumaSharpenClamp,
            sanitizedPost.lumaSharpenOffsetBias,
            static_cast<float>(pat + 4 * showPacked),
        };
    }
    outData->sweetFxCurvesPack = {
        sanitizedPost.sweetFxCurvesContrast,
        static_cast<float>(sanitizedPost.sweetFxCurvesMode),
        static_cast<float>(sanitizedPost.sweetFxCurvesFormula),
        sanitizedPost.sweetFxCurvesStrength,
    };
    outData->sweetFxChromaticAberrationPack = {
        sanitizedPost.sweetFxChromaticAberrationShiftX,
        sanitizedPost.sweetFxChromaticAberrationShiftY,
        sanitizedPost.sweetFxChromaticAberrationStrength,
        0.0f,
    };
    outData->sweetFxBorderPack = {
        sanitizedPost.sweetFxBorderWidthX,
        sanitizedPost.sweetFxBorderWidthY,
        sanitizedPost.sweetFxBorderRatio,
        sanitizedPost.sweetFxBorderStrength,
    };
    outData->sweetFxBorderColorPad = {
        sanitizedPost.sweetFxBorderColor.x,
        sanitizedPost.sweetFxBorderColor.y,
        sanitizedPost.sweetFxBorderColor.z,
        0.0f,
    };
    outData->sweetFxCartoonPack = {
        sanitizedPost.sweetFxCartoonPower,
        sanitizedPost.sweetFxCartoonEdgeSlope,
        sanitizedPost.sweetFxCartoonStrength,
        0.0f,
    };
    outData->sweetFxTonemapGammaExpSatBleach = {
        sanitizedPost.sweetFxTonemapGamma,
        sanitizedPost.sweetFxTonemapExposure,
        sanitizedPost.sweetFxTonemapSaturation,
        sanitizedPost.sweetFxTonemapBleach,
    };
    outData->sweetFxTonemapFogColorDefog = {
        fogNear.x,
        fogNear.y,
        fogNear.z,
        sanitizedPost.sweetFxTonemapDefog,
    };
    outData->sweetFxTonemapStrengthPad = {
        sanitizedPost.sweetFxTonemapStrength,
        static_cast<float>(std::clamp(frame.renderQualityTier, 0, 2)),
        fogFar.x,
        fogFar.y,
    };
    outData->sweetFxSplitscreenModeStrength = {
        static_cast<float>(sanitizedPost.sweetFxSplitscreenMode),
        sanitizedPost.sweetFxSplitscreenStrength,
        linearFog ? fogStart : 0.0f,
        linearFog ? fogEnd : 0.0f,
    };
    outData->sweetFxNostalgiaPack = {
        static_cast<float>(sanitizedPost.sweetFxNostalgiaPalette),
        static_cast<float>(sanitizedPost.sweetFxNostalgiaScanlines),
        sanitizedPost.sweetFxNostalgiaDither,
        sanitizedPost.sweetFxNostalgiaStrength,
    };
    outData->sweetFxComparePack = {
        static_cast<float>(sanitizedPost.sweetFxCompareMode),
        sanitizedPost.sweetFxCompareDifferenceScale,
        sanitizedPost.sweetFxCompareStrength,
        fogFar.z,
    };
    outData->sweetFxLayerPosScaleBlend = {
        sanitizedPost.sweetFxLayerPosition.x,
        sanitizedPost.sweetFxLayerPosition.y,
        sanitizedPost.sweetFxLayerScale,
        sanitizedPost.sweetFxLayerBlend,
    };
    outData->sweetFxLayerTexSizePad = {
        sanitizedPost.sweetFxLayerTexWidth,
        sanitizedPost.sweetFxLayerTexHeight,
        0.0f,
        0.0f,
    };
    outData->sweetFxFxaaPack = {
        sanitizedPost.sweetFxFxaaSubpix,
        sanitizedPost.sweetFxFxaaEdgeThreshold,
        sanitizedPost.sweetFxFxaaEdgeThresholdMin,
        sanitizedPost.sweetFxFxaaStrength,
    };
    outData->sweetFxCrtPack0 = {
        sanitizedPost.sweetFxCrtAmount,
        sanitizedPost.sweetFxCrtResolution,
        sanitizedPost.sweetFxCrtGamma,
        sanitizedPost.sweetFxCrtMonitorGamma,
    };
    outData->sweetFxCrtPack1 = {
        sanitizedPost.sweetFxCrtBrightness,
        static_cast<float>(sanitizedPost.sweetFxCrtScanlineIntensity),
        sanitizedPost.sweetFxCrtScanlineGaussian,
        sanitizedPost.sweetFxCrtCurvature,
    };
    outData->sweetFxCrtPack2 = {
        sanitizedPost.sweetFxCrtCurvatureRadius,
        sanitizedPost.sweetFxCrtCornerSize,
        sanitizedPost.sweetFxCrtViewerDistance,
        sanitizedPost.sweetFxCrtOverscan,
    };
    outData->sweetFxCrtPack3 = {
        sanitizedPost.sweetFxCrtAngle.x,
        sanitizedPost.sweetFxCrtAngle.y,
        sanitizedPost.sweetFxCrtOversample,
        0.0f,
    };
    outData->sweetFxAsciiPack0 = {
        static_cast<float>(sanitizedPost.sweetFxAsciiSpacing),
        static_cast<float>(sanitizedPost.sweetFxAsciiFont),
        static_cast<float>(sanitizedPost.sweetFxAsciiFontColorMode),
        sanitizedPost.sweetFxAsciiStrength,
    };
    outData->sweetFxAsciiPack1 = {
        sanitizedPost.sweetFxAsciiSwapColors,
        sanitizedPost.sweetFxAsciiInvertBrightness,
        sanitizedPost.sweetFxAsciiDithering,
        sanitizedPost.sweetFxAsciiDitheringIntensity,
    };
    outData->sweetFxAsciiPack2 = {
        sanitizedPost.sweetFxAsciiDitheringDebugGradient,
        0.0f,
        0.0f,
        0.0f,
    };
    outData->sweetFxAsciiFontColorPad = {
        sanitizedPost.sweetFxAsciiFontColor.x,
        sanitizedPost.sweetFxAsciiFontColor.y,
        sanitizedPost.sweetFxAsciiFontColor.z,
        0.0f,
    };
    outData->sweetFxAsciiBackgroundColorPad = {
        sanitizedPost.sweetFxAsciiBackgroundColor.x,
        sanitizedPost.sweetFxAsciiBackgroundColor.y,
        sanitizedPost.sweetFxAsciiBackgroundColor.z,
        0.0f,
    };
    outData->sweetFxSmaaPack0 = {
        static_cast<float>(sanitizedPost.sweetFxSmaaEdgeDetectionType),
        sanitizedPost.sweetFxSmaaEdgeThreshold,
        sanitizedPost.sweetFxSmaaDepthThreshold,
        sanitizedPost.sweetFxSmaaStrength,
    };
    outData->sweetFxSmaaPack1 = {
        static_cast<float>(sanitizedPost.sweetFxSmaaMaxSearchSteps),
        static_cast<float>(sanitizedPost.sweetFxSmaaMaxSearchStepsDiagonal),
        static_cast<float>(sanitizedPost.sweetFxSmaaCornerRounding),
        sanitizedPost.sweetFxSmaaDebugOutput,
    };
    outData->reshadeDaltonizePack = {
        static_cast<float>(sanitizedPost.reshadeDaltonizeType),
        sanitizedPost.reshadeDaltonizeStrength,
        0.0f,
        0.0f,
    };
    outData->reshadeDisplayDepthPack = {
        static_cast<float>(sanitizedPost.reshadeDisplayDepthPresentType),
        sanitizedPost.reshadeDisplayDepthStrength,
        0.0f,
        0.0f,
    };
    outData->reshadeLutPack = {
        sanitizedPost.reshadeLutAmountChroma,
        sanitizedPost.reshadeLutAmountLuma,
        sanitizedPost.reshadeLutStrength,
        0.0f,
    };
    outData->pd80TcRedStrPad = {
        sanitizedPost.pd80TechnicolorRed2strip.x,
        sanitizedPost.pd80TechnicolorRed2strip.y,
        sanitizedPost.pd80TechnicolorRed2strip.z,
        sanitizedPost.pd80TechnicolorStrength,
    };
    outData->pd80TcCyanPad = {
        sanitizedPost.pd80TechnicolorCyan2strip.x,
        sanitizedPost.pd80TechnicolorCyan2strip.y,
        sanitizedPost.pd80TechnicolorCyan2strip.z,
        0.0f,
    };
    outData->pd80TcKeySat2Pad = {
        sanitizedPost.pd80TechnicolorColorKey.x,
        sanitizedPost.pd80TechnicolorColorKey.y,
        sanitizedPost.pd80TechnicolorColorKey.z,
        sanitizedPost.pd80TechnicolorSaturation2,
    };
    outData->pd80Tc3ColBrightPad = {
        sanitizedPost.pd80Technicolor3ColorStrength.x,
        sanitizedPost.pd80Technicolor3ColorStrength.y,
        sanitizedPost.pd80Technicolor3ColorStrength.z,
        sanitizedPost.pd80Technicolor3Brightness,
    };
    outData->pd80Tc3SatStrEnPad = {
        sanitizedPost.pd80Technicolor3Saturation,
        sanitizedPost.pd80Technicolor3Strength,
        sanitizedPost.pd80TechnicolorEnable3strip,
        0.0f,
    };
    outData->pd80ColorTempKelvinLumMixStr = {
        sanitizedPost.pd80ColorTemperatureKelvin,
        sanitizedPost.pd80ColorTemperatureLuminancePreservation,
        sanitizedPost.pd80ColorTemperatureMix,
        sanitizedPost.pd80ColorTemperatureStrength,
    };
    outData->pd80SatLimitCapStr = {
        sanitizedPost.pd80SaturationLimit,
        sanitizedPost.pd80SaturationLimitStrength,
        0.0f,
        0.0f,
    };
    outData->pd80ColorBalanceShadowPad = {
        sanitizedPost.pd80ColorBalanceShadow.x,
        sanitizedPost.pd80ColorBalanceShadow.y,
        sanitizedPost.pd80ColorBalanceShadow.z,
        0.0f,
    };
    outData->pd80ColorBalanceMidPad = {
        sanitizedPost.pd80ColorBalanceMid.x,
        sanitizedPost.pd80ColorBalanceMid.y,
        sanitizedPost.pd80ColorBalanceMid.z,
        0.0f,
    };
    outData->pd80ColorBalanceHighPad = {
        sanitizedPost.pd80ColorBalanceHigh.x,
        sanitizedPost.pd80ColorBalanceHigh.y,
        sanitizedPost.pd80ColorBalanceHigh.z,
        0.0f,
    };
    outData->pd80ColorBalanceOptStr = {
        sanitizedPost.pd80ColorBalancePreserveLuma,
        sanitizedPost.pd80ColorBalanceSeparationMode,
        sanitizedPost.pd80ColorBalanceStrength,
        0.0f,
    };
    outData->pd80ColorIsolationHueRangeSatMix = {
        sanitizedPost.pd80ColorIsolationHueMid,
        sanitizedPost.pd80ColorIsolationHueRange,
        sanitizedPost.pd80ColorIsolationSatLimit,
        sanitizedPost.pd80ColorIsolationFxMix,
    };
    outData->pd80ColorIsolationStrPad = {
        sanitizedPost.pd80ColorIsolationStrength,
        0.0f,
        0.0f,
        0.0f,
    };
    outData->pd80LevelsIbPad = {
        sanitizedPost.pd80LevelsBlackIn.x,
        sanitizedPost.pd80LevelsBlackIn.y,
        sanitizedPost.pd80LevelsBlackIn.z,
        0.0f,
    };
    outData->pd80LevelsIwPad = {
        sanitizedPost.pd80LevelsWhiteIn.x,
        sanitizedPost.pd80LevelsWhiteIn.y,
        sanitizedPost.pd80LevelsWhiteIn.z,
        0.0f,
    };
    outData->pd80LevelsObPad = {
        sanitizedPost.pd80LevelsBlackOut.x,
        sanitizedPost.pd80LevelsBlackOut.y,
        sanitizedPost.pd80LevelsBlackOut.z,
        0.0f,
    };
    outData->pd80LevelsOwPad = {
        sanitizedPost.pd80LevelsWhiteOut.x,
        sanitizedPost.pd80LevelsWhiteOut.y,
        sanitizedPost.pd80LevelsWhiteOut.z,
        0.0f,
    };
    outData->pd80LevelsGammaDitherStr = {
        sanitizedPost.pd80LevelsGamma,
        sanitizedPost.pd80LevelsEnableDither,
        sanitizedPost.pd80LevelsDitherStrength,
        sanitizedPost.pd80LevelsStrength,
    };
    outData->pd80BwPack0 = {
        sanitizedPost.pd80BlackWhiteMode,
        sanitizedPost.pd80BlackWhiteCurveStr,
        sanitizedPost.pd80BlackWhiteEnableDither,
        sanitizedPost.pd80BlackWhiteDitherStrength,
    };
    outData->pd80BwPack1 = {
        sanitizedPost.pd80BlackWhiteRedChannel,
        sanitizedPost.pd80BlackWhiteYellowChannel,
        sanitizedPost.pd80BlackWhiteGreenChannel,
        sanitizedPost.pd80BlackWhiteCyanChannel,
    };
    outData->pd80BwPack2 = {
        sanitizedPost.pd80BlackWhiteBlueChannel,
        sanitizedPost.pd80BlackWhiteMagentaChannel,
        sanitizedPost.pd80BlackWhiteStrength,
        sanitizedPost.pd80BlackWhiteShowClip,
    };
    outData->pd80BwPack3 = {
        sanitizedPost.pd80BlackWhiteUseTint,
        sanitizedPost.pd80BlackWhiteTintHue,
        sanitizedPost.pd80BlackWhiteTintSat,
        0.0f,
    };
    outData->pd80CbsPack0 = {
        sanitizedPost.pd80CbsEnableDither,
        sanitizedPost.pd80CbsDitherStrength,
        sanitizedPost.pd80CbsTint,
        sanitizedPost.pd80CbsExposure,
    };
    outData->pd80CbsPack1 = {
        sanitizedPost.pd80CbsContrast,
        sanitizedPost.pd80CbsBrightness,
        sanitizedPost.pd80CbsSaturation,
        sanitizedPost.pd80CbsVibrance,
    };
    outData->pd80CbsPack2 = {
        sanitizedPost.pd80CbsHueMid,
        sanitizedPost.pd80CbsHueRange,
        sanitizedPost.pd80CbsSatCustom,
        sanitizedPost.pd80CbsStrength,
    };
    outData->pd80CbsPack3 = {
        sanitizedPost.pd80CbsSatR,
        sanitizedPost.pd80CbsSatY,
        sanitizedPost.pd80CbsSatG,
        sanitizedPost.pd80CbsSatA,
    };
    outData->pd80CbsPack4 = {
        sanitizedPost.pd80CbsSatB,
        sanitizedPost.pd80CbsSatP,
        sanitizedPost.pd80CbsSatM,
        sanitizedPost.pd80CbsEnableDepth,
    };
    outData->pd80CbsPack5 = {
        sanitizedPost.pd80CbsDisplayDepth,
        sanitizedPost.pd80CbsDepthStart,
        sanitizedPost.pd80CbsDepthEnd,
        sanitizedPost.pd80CbsDepthCurve,
    };
    outData->pd80CbsPack6 = {
        sanitizedPost.pd80CbsExposureFar,
        sanitizedPost.pd80CbsContrastFar,
        sanitizedPost.pd80CbsBrightnessFar,
        sanitizedPost.pd80CbsSaturationFar,
    };
    outData->pd80CbsPack7 = {sanitizedPost.pd80CbsVibranceFar, 0.0f, 0.0f, 0.0f};
    outData->pd80CaPack0 = {
        sanitizedPost.pd80CaMasterStrength,
        sanitizedPost.pd80CaEffectStrength,
        sanitizedPost.pd80CaGlobalWidth,
        sanitizedPost.pd80CaSampleSteps,
    };
    outData->pd80CaPack1 = {
        sanitizedPost.pd80CaType,
        sanitizedPost.pd80CaDegrees,
        sanitizedPost.pd80CaWidth,
        sanitizedPost.pd80CaCurve,
    };
    outData->pd80CaPack2 = {
        sanitizedPost.pd80CaOX,
        sanitizedPost.pd80CaOY,
        sanitizedPost.pd80CaShapeX,
        sanitizedPost.pd80CaShapeY,
    };
    outData->pd80CaPack3 = {
        sanitizedPost.pd80CaVignetteColor.x,
        sanitizedPost.pd80CaVignetteColor.y,
        sanitizedPost.pd80CaVignetteColor.z,
        sanitizedPost.pd80CaShowCa,
    };
    outData->pd80CaPack4 = {
        sanitizedPost.pd80CaEnableDepthInt,
        sanitizedPost.pd80CaEnableDepthWidth,
        sanitizedPost.pd80CaDisplayDepth,
        0.0f,
    };
    outData->pd80CaPack5 = {
        sanitizedPost.pd80CaDepthStart,
        sanitizedPost.pd80CaDepthEnd,
        sanitizedPost.pd80CaDepthCurve,
        0.0f,
    };
    outData->pd80LsPack0 = {
        sanitizedPost.pd80LsMasterStrength,
        sanitizedPost.pd80LsBlurSigma,
        sanitizedPost.pd80LsSharpening,
        sanitizedPost.pd80LsThreshold,
    };
    outData->pd80LsPack1 = {
        sanitizedPost.pd80LsLimiter,
        sanitizedPost.pd80LsShowEdges,
        sanitizedPost.pd80LsEnableDepth,
        sanitizedPost.pd80LsEnableReverse,
    };
    outData->pd80LsPack2 = {
        sanitizedPost.pd80LsDisplayDepth,
        sanitizedPost.pd80LsDepthStart,
        sanitizedPost.pd80LsDepthEnd,
        sanitizedPost.pd80LsDepthCurve,
    };
    outData->pd80FgPack0 = {
        sanitizedPost.pd80FgMasterStrength,
        sanitizedPost.pd80FgGrainAdjust,
        sanitizedPost.pd80FgGrainSize,
        sanitizedPost.pd80FgGrainMotion,
    };
    outData->pd80FgPack1 = {
        sanitizedPost.pd80FgGrainOrigColor,
        sanitizedPost.pd80FgUseNegnoise,
        sanitizedPost.pd80FgGrainColor,
        sanitizedPost.pd80FgGrainAmount,
    };
    outData->pd80FgPack2 = {
        sanitizedPost.pd80FgGrainIntensity,
        sanitizedPost.pd80FgGrainDensity,
        sanitizedPost.pd80FgGrainIntHigh,
        sanitizedPost.pd80FgGrainIntLow,
    };
    outData->pd80FgPack3 = {
        sanitizedPost.pd80FgEnableTest,
        sanitizedPost.pd80FgEnableDepth,
        sanitizedPost.pd80FgDisplayDepth,
        0.0f,
    };
    outData->pd80FgPack4 = {
        sanitizedPost.pd80FgDepthStart,
        sanitizedPost.pd80FgDepthEnd,
        sanitizedPost.pd80FgDepthCurve,
        0.0f,
    };
    outData->pd80DsPack0 = {
        sanitizedPost.pd80DsMasterStrength,
        sanitizedPost.pd80DsDepthNear,
        sanitizedPost.pd80DsDepthPos,
        sanitizedPost.pd80DsDepthFar,
    };
    outData->pd80DsPack1 = {
        sanitizedPost.pd80DsDepthSmoothing,
        sanitizedPost.pd80DsIntensity,
        sanitizedPost.pd80DsHue,
        sanitizedPost.pd80DsSaturation,
    };
    outData->pd80DsPack2 = {
        sanitizedPost.pd80DsBlendMode,
        sanitizedPost.pd80DsOpacity,
        0.0f,
        0.0f,
    };
    outData->pd80CgPack0 = {
        sanitizedPost.pd80CgMasterStrength,
        sanitizedPost.pd80ColorGamut,
        0.0f,
        0.0f,
    };
    outData->pd80CscPack0 = {
        sanitizedPost.pd80CscMasterStrength,
        sanitizedPost.pd80CscEnableDither,
        sanitizedPost.pd80CscDitherStrength,
        sanitizedPost.pd80CscColorSpace,
    };
    outData->pd80CscPack1 = {
        sanitizedPost.pd80CscPos0ToeGrey,
        sanitizedPost.pd80CscPos1ToeGrey,
        sanitizedPost.pd80CscPos0ShoulderGrey,
        sanitizedPost.pd80CscPos1ShoulderGrey,
    };
    outData->pd80CscPack2 = {
        sanitizedPost.pd80CscColorSat,
        0.0f,
        0.0f,
        0.0f,
    };
    outData->pd80SmhPack0 = {
        sanitizedPost.pd80SmhMasterStrength,
        sanitizedPost.pd80SmhLumaMode,
        sanitizedPost.pd80SmhSeparationMode,
        sanitizedPost.pd80SmhEnableDither,
    };
    outData->pd80SmhPack1 = {
        sanitizedPost.pd80SmhDitherStrength,
        0.0f,
        0.0f,
        0.0f,
    };
    outData->pd80SmhPack2 = {
        sanitizedPost.pd80SmhShadowExposure,
        sanitizedPost.pd80SmhShadowContrast,
        sanitizedPost.pd80SmhShadowBrightness,
        sanitizedPost.pd80SmhShadowOpacity,
    };
    outData->pd80SmhPack3 = {
        sanitizedPost.pd80SmhBlendColorShadow.x,
        sanitizedPost.pd80SmhBlendColorShadow.y,
        sanitizedPost.pd80SmhBlendColorShadow.z,
        sanitizedPost.pd80SmhShadowBlendMode,
    };
    outData->pd80SmhPack4 = {
        sanitizedPost.pd80SmhShadowTint,
        sanitizedPost.pd80SmhShadowSaturation,
        sanitizedPost.pd80SmhShadowVibrance,
        0.0f,
    };
    outData->pd80SmhPack5 = {
        sanitizedPost.pd80SmhMidExposure,
        sanitizedPost.pd80SmhMidContrast,
        sanitizedPost.pd80SmhMidBrightness,
        sanitizedPost.pd80SmhMidOpacity,
    };
    outData->pd80SmhPack6 = {
        sanitizedPost.pd80SmhBlendColorMid.x,
        sanitizedPost.pd80SmhBlendColorMid.y,
        sanitizedPost.pd80SmhBlendColorMid.z,
        sanitizedPost.pd80SmhMidBlendMode,
    };
    outData->pd80SmhPack7 = {
        sanitizedPost.pd80SmhMidTint,
        sanitizedPost.pd80SmhMidSaturation,
        sanitizedPost.pd80SmhMidVibrance,
        0.0f,
    };
    outData->pd80SmhPack8 = {
        sanitizedPost.pd80SmhHighlightExposure,
        sanitizedPost.pd80SmhHighlightContrast,
        sanitizedPost.pd80SmhHighlightBrightness,
        sanitizedPost.pd80SmhHighlightOpacity,
    };
    outData->pd80SmhPack9 = {
        sanitizedPost.pd80SmhBlendColorHighlight.x,
        sanitizedPost.pd80SmhBlendColorHighlight.y,
        sanitizedPost.pd80SmhBlendColorHighlight.z,
        sanitizedPost.pd80SmhHighlightBlendMode,
    };
    outData->pd80SmhPack10 = {
        sanitizedPost.pd80SmhHighlightTint,
        sanitizedPost.pd80SmhHighlightSaturation,
        sanitizedPost.pd80SmhHighlightVibrance,
        0.0f,
    };
    outData->pd80ClPack0 = {
        sanitizedPost.pd80ClMasterStrength,
        sanitizedPost.pd80ClEnableDither,
        sanitizedPost.pd80ClDitherStrength,
        sanitizedPost.pd80ClEnableRgb,
    };
    outData->pd80ClPack1 = {
        sanitizedPost.pd80ClGreyBlackIn,
        sanitizedPost.pd80ClGreyWhiteIn,
        sanitizedPost.pd80ClGreyBlackOut,
        sanitizedPost.pd80ClGreyWhiteOut,
    };
    outData->pd80ClPack2 = {
        sanitizedPost.pd80ClGreyPos0Shoulder,
        sanitizedPost.pd80ClGreyPos1Shoulder,
        sanitizedPost.pd80ClGreyPos0Toe,
        sanitizedPost.pd80ClGreyPos1Toe,
    };
    outData->pd80ClPack3 = {
        sanitizedPost.pd80ClRedBlackIn,
        sanitizedPost.pd80ClRedWhiteIn,
        sanitizedPost.pd80ClRedBlackOut,
        sanitizedPost.pd80ClRedWhiteOut,
    };
    outData->pd80ClPack4 = {
        sanitizedPost.pd80ClRedPos0Shoulder,
        sanitizedPost.pd80ClRedPos1Shoulder,
        sanitizedPost.pd80ClRedPos0Toe,
        sanitizedPost.pd80ClRedPos1Toe,
    };
    outData->pd80ClPack5 = {
        sanitizedPost.pd80ClGreenBlackIn,
        sanitizedPost.pd80ClGreenWhiteIn,
        sanitizedPost.pd80ClGreenBlackOut,
        sanitizedPost.pd80ClGreenWhiteOut,
    };
    outData->pd80ClPack6 = {
        sanitizedPost.pd80ClGreenPos0Shoulder,
        sanitizedPost.pd80ClGreenPos1Shoulder,
        sanitizedPost.pd80ClGreenPos0Toe,
        sanitizedPost.pd80ClGreenPos1Toe,
    };
    outData->pd80ClPack7 = {
        sanitizedPost.pd80ClBlueBlackIn,
        sanitizedPost.pd80ClBlueWhiteIn,
        sanitizedPost.pd80ClBlueBlackOut,
        sanitizedPost.pd80ClBlueWhiteOut,
    };
    outData->pd80ClPack8 = {
        sanitizedPost.pd80ClBluePos0Shoulder,
        sanitizedPost.pd80ClBluePos1Shoulder,
        sanitizedPost.pd80ClBluePos0Toe,
        sanitizedPost.pd80ClBluePos1Toe,
    };
    outData->pd80ScPack0 = {
        sanitizedPost.pd80ScMasterStrength,
        sanitizedPost.pd80ScCorrectionMethod,
        sanitizedPost.pd80ScCorrectionMethodSaturation,
        0.0f,
    };
    outData->pd80ScPack1 = {
        sanitizedPost.pd80ScRedsCyan,
        sanitizedPost.pd80ScRedsMagenta,
        sanitizedPost.pd80ScRedsYellow,
        sanitizedPost.pd80ScRedsBlack,
    };
    outData->pd80ScPack2 = {sanitizedPost.pd80ScRedsSaturation, sanitizedPost.pd80ScRedsVibrance, 0.0f, 0.0f};
    outData->pd80ScPack3 = {
        sanitizedPost.pd80ScYellowsCyan,
        sanitizedPost.pd80ScYellowsMagenta,
        sanitizedPost.pd80ScYellowsYellow,
        sanitizedPost.pd80ScYellowsBlack,
    };
    outData->pd80ScPack4 = {sanitizedPost.pd80ScYellowsSaturation, sanitizedPost.pd80ScYellowsVibrance, 0.0f, 0.0f};
    outData->pd80ScPack5 = {
        sanitizedPost.pd80ScGreensCyan,
        sanitizedPost.pd80ScGreensMagenta,
        sanitizedPost.pd80ScGreensYellow,
        sanitizedPost.pd80ScGreensBlack,
    };
    outData->pd80ScPack6 = {sanitizedPost.pd80ScGreensSaturation, sanitizedPost.pd80ScGreensVibrance, 0.0f, 0.0f};
    outData->pd80ScPack7 = {
        sanitizedPost.pd80ScCyansCyan,
        sanitizedPost.pd80ScCyansMagenta,
        sanitizedPost.pd80ScCyansYellow,
        sanitizedPost.pd80ScCyansBlack,
    };
    outData->pd80ScPack8 = {sanitizedPost.pd80ScCyansSaturation, sanitizedPost.pd80ScCyansVibrance, 0.0f, 0.0f};
    outData->pd80ScPack9 = {
        sanitizedPost.pd80ScBluesCyan,
        sanitizedPost.pd80ScBluesMagenta,
        sanitizedPost.pd80ScBluesYellow,
        sanitizedPost.pd80ScBluesBlack,
    };
    outData->pd80ScPack10 = {sanitizedPost.pd80ScBluesSaturation, sanitizedPost.pd80ScBluesVibrance, 0.0f, 0.0f};
    outData->pd80ScPack11 = {
        sanitizedPost.pd80ScMagentasCyan,
        sanitizedPost.pd80ScMagentasMagenta,
        sanitizedPost.pd80ScMagentasYellow,
        sanitizedPost.pd80ScMagentasBlack,
    };
    outData->pd80ScPack12 = {
        sanitizedPost.pd80ScMagentasSaturation,
        sanitizedPost.pd80ScMagentasVibrance,
        0.0f,
        0.0f,
    };
    outData->pd80ScPack13 = {
        sanitizedPost.pd80ScWhitesCyan,
        sanitizedPost.pd80ScWhitesMagenta,
        sanitizedPost.pd80ScWhitesYellow,
        sanitizedPost.pd80ScWhitesBlack,
    };
    outData->pd80ScPack14 = {sanitizedPost.pd80ScWhitesSaturation, sanitizedPost.pd80ScWhitesVibrance, 0.0f, 0.0f};
    outData->pd80ScPack15 = {
        sanitizedPost.pd80ScNeutralsCyan,
        sanitizedPost.pd80ScNeutralsMagenta,
        sanitizedPost.pd80ScNeutralsYellow,
        sanitizedPost.pd80ScNeutralsBlack,
    };
    outData->pd80ScPack16 = {
        sanitizedPost.pd80ScNeutralsSaturation,
        sanitizedPost.pd80ScNeutralsVibrance,
        0.0f,
        0.0f,
    };
    outData->pd80ScPack17 = {
        sanitizedPost.pd80ScBlacksCyan,
        sanitizedPost.pd80ScBlacksMagenta,
        sanitizedPost.pd80ScBlacksYellow,
        sanitizedPost.pd80ScBlacksBlack,
    };
    outData->pd80ScPack18 = {sanitizedPost.pd80ScBlacksSaturation, sanitizedPost.pd80ScBlacksVibrance, 0.0f, 0.0f};
    outData->pd80PpPack0 = {sanitizedPost.pd80PpMasterStrength,
                            sanitizedPost.pd80PpNumberOfLevels,
                            sanitizedPost.pd80PpPixelSize,
                            sanitizedPost.pd80PpBorderStrength};
    outData->pd80PpPack1 = {
        sanitizedPost.pd80PpEnableDither,
        sanitizedPost.pd80PpDitherMotion,
        sanitizedPost.pd80PpDitherStrength,
        0.0f,
    };
    outData->pd80MrPack0 = {
        sanitizedPost.pd80MrShape,
        sanitizedPost.pd80MrInvertShape,
        sanitizedPost.pd80MrRotation,
        sanitizedPost.pd80MrCenter.x,
    };
    outData->pd80MrPack1 = {
        sanitizedPost.pd80MrCenter.y,
        sanitizedPost.pd80MrSizeX,
        sanitizedPost.pd80MrSizeY,
        sanitizedPost.pd80MrDepthPosition,
    };
    outData->pd80MrPack2 = {
        sanitizedPost.pd80MrSmoothing,
        sanitizedPost.pd80MrDepthSmoothing,
        sanitizedPost.pd80MrDitherStrength,
        sanitizedPost.pd80MrExposure,
    };
    outData->pd80MrPack3 = {
        sanitizedPost.pd80MrContrast,
        sanitizedPost.pd80MrBrightness,
        sanitizedPost.pd80MrHue,
        sanitizedPost.pd80MrSaturation,
    };
    outData->pd80MrPack4 = {
        sanitizedPost.pd80MrVibrance,
        sanitizedPost.pd80MrEnableGradient,
        sanitizedPost.pd80MrGradientType,
        sanitizedPost.pd80MrGradientCurve,
    };
    outData->pd80MrPack5 = {
        sanitizedPost.pd80MrIntensityBoost,
        sanitizedPost.pd80MrBlendMode,
        sanitizedPost.pd80MrOpacity,
        0.0f,
    };
    outData->pd80MrPack6 = {
        sanitizedPost.pd80MrColor.x,
        sanitizedPost.pd80MrColor.y,
        sanitizedPost.pd80MrColor.z,
        0.0f,
    };
    outData->pd80MrPack7 = {0.0f, 0.0f, 0.0f, 0.0f};
    outData->pd80BlpPack0 = {
        sanitizedPost.pd80BlpMasterStrength,
        sanitizedPost.pd80BlpEnableDither,
        sanitizedPost.pd80BlpDitherStrength,
        sanitizedPost.pd80BlpLutSelector,
    };
    outData->pd80BlpPack1 = {
        sanitizedPost.pd80BlpMixChroma,
        sanitizedPost.pd80BlpMixLuma,
        sanitizedPost.pd80BlpGamma,
        0.0f,
    };
    outData->pd80BlpPack2 = {
        sanitizedPost.pd80BlpBlackIn.x,
        sanitizedPost.pd80BlpBlackIn.y,
        sanitizedPost.pd80BlpBlackIn.z,
        0.0f,
    };
    outData->pd80BlpPack3 = {
        sanitizedPost.pd80BlpWhiteIn.x,
        sanitizedPost.pd80BlpWhiteIn.y,
        sanitizedPost.pd80BlpWhiteIn.z,
        0.0f,
    };
    outData->pd80BlpPack4 = {
        sanitizedPost.pd80BlpBlackOut.x,
        sanitizedPost.pd80BlpBlackOut.y,
        sanitizedPost.pd80BlpBlackOut.z,
        sanitizedPost.pd80BlpWhiteOut.x,
    };
    outData->pd80BlpPack5 = {sanitizedPost.pd80BlpWhiteOut.y, sanitizedPost.pd80BlpWhiteOut.z, 0.0f, 0.0f};
    outData->pd80CltPack0 = {
        sanitizedPost.pd80CltMasterStrength,
        sanitizedPost.pd80CltEnableDither,
        sanitizedPost.pd80CltDitherStrength,
        sanitizedPost.pd80CltLutSelector,
    };
    outData->pd80CltPack1 = {
        sanitizedPost.pd80CltMixChroma,
        sanitizedPost.pd80CltMixLuma,
        sanitizedPost.pd80CltGamma,
        0.0f,
    };
    outData->pd80CltPack2 = {
        sanitizedPost.pd80CltBlackIn.x,
        sanitizedPost.pd80CltBlackIn.y,
        sanitizedPost.pd80CltBlackIn.z,
        0.0f,
    };
    outData->pd80CltPack3 = {
        sanitizedPost.pd80CltWhiteIn.x,
        sanitizedPost.pd80CltWhiteIn.y,
        sanitizedPost.pd80CltWhiteIn.z,
        0.0f,
    };
    outData->pd80CltPack4 = {
        sanitizedPost.pd80CltBlackOut.x,
        sanitizedPost.pd80CltBlackOut.y,
        sanitizedPost.pd80CltBlackOut.z,
        sanitizedPost.pd80CltWhiteOut.x,
    };
    outData->pd80CltPack5 = {sanitizedPost.pd80CltWhiteOut.y, sanitizedPost.pd80CltWhiteOut.z, 0.0f, 0.0f};
    outData->pd80LcPack0 = {
        sanitizedPost.pd80LcMasterStrength,
        sanitizedPost.pd80LcTextureWidth,
        sanitizedPost.pd80LcTextureHeight,
        0.0f,
    };
    outData->pd80LfPack0 = {
        sanitizedPost.pd80LfMasterStrength,
        sanitizedPost.pd80LfTransitionSpeed,
        sanitizedPost.pd80LfMinLevel,
        sanitizedPost.pd80LfMaxLevel,
    };
    outData->pd80Cg4Pack0 = {
        sanitizedPost.pd80Cg4MasterStrength,
        sanitizedPost.pd80Cg4LumaMode,
        sanitizedPost.pd80Cg4SeparationMode,
        sanitizedPost.pd80Cg4EnableDither,
    };
    outData->pd80Cg4Pack1 = {
        sanitizedPost.pd80Cg4DitherStrength,
        sanitizedPost.pd80Cg4DesaturateBase,
        sanitizedPost.pd80Cg4FinalMix,
        sanitizedPost.pd80Cg4LightSceneMidBlendMode,
    };
    outData->pd80Cg4Pack2 = {
        sanitizedPost.pd80Cg4LightSceneMidOpacity,
        sanitizedPost.pd80Cg4LightSceneShadowBlendMode,
        sanitizedPost.pd80Cg4LightSceneShadowOpacity,
        sanitizedPost.pd80Cg4EnableDarkScene,
    };
    outData->pd80Cg4Pack3 = {
        sanitizedPost.pd80Cg4DarkSceneMidBlendMode,
        sanitizedPost.pd80Cg4DarkSceneMidOpacity,
        sanitizedPost.pd80Cg4DarkSceneShadowBlendMode,
        sanitizedPost.pd80Cg4DarkSceneShadowOpacity,
    };
    outData->pd80Cg4Pack4 = {
        sanitizedPost.pd80Cg4MinLevel,
        sanitizedPost.pd80Cg4MaxLevel,
        0.0f,
        0.0f,
    };
    outData->pd80Cg4Pack5 = {
        sanitizedPost.pd80Cg4LightSceneMidColor.x,
        sanitizedPost.pd80Cg4LightSceneMidColor.y,
        sanitizedPost.pd80Cg4LightSceneMidColor.z,
        0.0f,
    };
    outData->pd80Cg4Pack6 = {
        sanitizedPost.pd80Cg4LightSceneShadowColor.x,
        sanitizedPost.pd80Cg4LightSceneShadowColor.y,
        sanitizedPost.pd80Cg4LightSceneShadowColor.z,
        0.0f,
    };
    outData->pd80Cg4Pack7 = {
        sanitizedPost.pd80Cg4DarkSceneMidColor.x,
        sanitizedPost.pd80Cg4DarkSceneMidColor.y,
        sanitizedPost.pd80Cg4DarkSceneMidColor.z,
        0.0f,
    };
    outData->pd80Cg4Pack8 = {
        sanitizedPost.pd80Cg4DarkSceneShadowColor.x,
        sanitizedPost.pd80Cg4DarkSceneShadowColor.y,
        sanitizedPost.pd80Cg4DarkSceneShadowColor.z,
        0.0f,
    };
    outData->pd80CcPack0 = {
        sanitizedPost.pd80CcMasterStrength,
        sanitizedPost.pd80CcEnableWhitepoint,
        sanitizedPost.pd80CcWhitepointStrength,
        sanitizedPost.pd80CcEnableBlackpoint * sanitizedPost.pd80CcBlackpointStrength,
    };
    outData->pd80RccPack0 = {
        sanitizedPost.pd80RccMasterStrength,
        sanitizedPost.pd80RccEnableDither,
        sanitizedPost.pd80RccDitherStrength,
        sanitizedPost.pd80RccEnableWhitepoint,
    };
    outData->pd80RccPack1 = {
        sanitizedPost.pd80RccWhitepointRespectLuma,
        sanitizedPost.pd80RccWhitepointMethod,
        sanitizedPost.pd80RccWhitepointStrength,
        sanitizedPost.pd80RccWhitepointLumaStrength,
    };
    outData->pd80RccPack2 = {
        sanitizedPost.pd80RccEnableBlackpoint,
        sanitizedPost.pd80RccBlackpointRespectLuma,
        sanitizedPost.pd80RccBlackpointMethod,
        sanitizedPost.pd80RccBlackpointStrength,
    };
    outData->pd80RccPack3 = {
        sanitizedPost.pd80RccBlackpointLumaStrength,
        sanitizedPost.pd80RccEnableMidpoint,
        sanitizedPost.pd80RccMidpointRespectLuma,
        sanitizedPost.pd80RccMidUseAltMethod,
    };
    outData->pd80RccPack4 = {
        sanitizedPost.pd80RccMidScale,
        0.0f,
        0.0f,
        0.0f,
    };
    outData->pd80FaPack0 = {
        sanitizedPost.pd80FaMasterStrength,
        sanitizedPost.pd80FaAdjustShoulder,
        sanitizedPost.pd80FaAdjustLinear,
        sanitizedPost.pd80FaAdjustToe,
    };
    outData->pd80HbPack0 = {
        sanitizedPost.pd80HbMasterStrength,
        sanitizedPost.pd80HbDebugBloom,
        sanitizedPost.pd80HbDitherStrength,
        sanitizedPost.pd80HbMix,
    };
    outData->pd80HbPack1 = {
        sanitizedPost.pd80HbThreshold,
        sanitizedPost.pd80HbGreyValue,
        sanitizedPost.pd80HbExposure,
        sanitizedPost.pd80HbBlurSigma,
    };
    outData->pd80HbPack2 = {
        sanitizedPost.pd80HbSaturation,
        0.0f,
        0.0f,
        0.0f,
    };
    outData->pd80Sc2Pack0 = {
        sanitizedPost.pd80Sc2MasterStrength,
        sanitizedPost.pd80Sc2CorrectionMethod,
        sanitizedPost.pd80Sc2SaturationScale,
        sanitizedPost.pd80Sc2LightnessScale,
    };
    outData->creatorColourfulnessPack = {
        sanitizedPost.colourfulness,
        sanitizedPost.colourfulnessLimitLuma,
        0.0f,
        0.0f,
    };
    outData->creatorFilmicPassPack = {
        sanitizedPost.filmicPassStrength,
        sanitizedPost.filmicPassFade,
        sanitizedPost.filmicPassBleach,
        sanitizedPost.filmicPassSaturation,
    };
    outData->creatorFilmGrain2Pack = {
        sanitizedPost.filmGrain2Amount,
        sanitizedPost.filmGrain2ColorAmount,
        sanitizedPost.filmGrain2LuminanceAmount,
        sanitizedPost.filmGrain2Size,
    };
    outData->creatorDenoisePack = {
        sanitizedPost.denoiseStrength,
        sanitizedPost.denoiseNoiseLevel,
        sanitizedPost.denoiseLerpCoefficient,
        sanitizedPost.denoiseWeightThreshold,
    };
    outData->creatorDenoisePack2 = {
        sanitizedPost.denoiseCounterThreshold,
        sanitizedPost.denoiseGaussianSigma,
        0.0f,
        0.0f,
    };
    outData->creatorAdaptiveSharpenPack0 = {
        sanitizedPost.adaptiveSharpenStrength,
        sanitizedPost.adaptiveSharpenCurveSlope,
        sanitizedPost.adaptiveSharpenLightOvershoot,
        sanitizedPost.adaptiveSharpenDarkOvershoot,
    };
    outData->creatorAdaptiveSharpenPack1 = {
        sanitizedPost.adaptiveSharpenLightComprLow,
        sanitizedPost.adaptiveSharpenLightComprHigh,
        sanitizedPost.adaptiveSharpenDarkComprLow,
        sanitizedPost.adaptiveSharpenDarkComprHigh,
    };
    outData->creatorAdaptiveSharpenPack2 = {
        sanitizedPost.adaptiveSharpenScaleLim,
        sanitizedPost.adaptiveSharpenScaleCs,
        sanitizedPost.adaptiveSharpenPmP,
        0.0f,
    };
    outData->creatorGaussianBlurPack = {
        sanitizedPost.gaussianBlurStrength,
        sanitizedPost.gaussianBlurOffset,
        static_cast<float>(sanitizedPost.gaussianBlurRadius),
        0.0f,
    };
    outData->creatorFineSharpPack0 = {
        sanitizedPost.fineSharpStrength,
        sanitizedPost.fineSharpEqualization,
        sanitizedPost.fineSharpXStrength,
        sanitizedPost.fineSharpXRepair,
    };
    outData->creatorFineSharpPack1 = {
        sanitizedPost.fineSharpLStrength,
        sanitizedPost.fineSharpPStrength,
        static_cast<float>(sanitizedPost.fineSharpMode),
        0.0f,
    };
    outData->creatorMartyBloomPack0 = {
        sanitizedPost.martyBloomThreshold,
        sanitizedPost.martyBloomAmount,
        sanitizedPost.martyBloomSaturation,
        static_cast<float>(sanitizedPost.martyBloomMixMode),
    };
    outData->creatorMartyBloomPack1 = {
        sanitizedPost.martyBloomTint.x,
        sanitizedPost.martyBloomTint.y,
        sanitizedPost.martyBloomTint.z,
        0.0f,
    };
    outData->creatorDofPack0 = {
        sanitizedPost.creatorDofStrength,
        sanitizedPost.creatorDofAutoFocus ? 1.0f : 0.0f,
        sanitizedPost.creatorDofManualFocusDepth,
        sanitizedPost.creatorDofInfiniteFocus,
    };
    outData->creatorDofPack1 = {
        sanitizedPost.creatorDofFocusPoint.x,
        sanitizedPost.creatorDofFocusPoint.y,
        sanitizedPost.creatorDofFocusRadius,
        static_cast<float>(sanitizedPost.creatorDofFocusSamples),
    };
    outData->creatorDofPack2 = {
        sanitizedPost.creatorDofNearBlurCurve,
        sanitizedPost.creatorDofFarBlurCurve,
        sanitizedPost.creatorDofBlurRadius,
        static_cast<float>(sanitizedPost.creatorDofRingSamples),
    };
    outData->creatorDofPack3 = {
        static_cast<float>(sanitizedPost.creatorDofRingRings),
        sanitizedPost.creatorDofRingThreshold,
        sanitizedPost.creatorDofRingGain,
        sanitizedPost.creatorDofRingFringe,
    };
    outData->creatorDofPack4 = {
        sanitizedPost.creatorDofRingBias,
        0.0f,
        0.0f,
        0.0f,
    };
    outData->creatorAmbientLightPack0 = {
        sanitizedPost.ambientLightIntensity,
        sanitizedPost.ambientLightThreshold,
        sanitizedPost.ambientLightAdapt,
        sanitizedPost.ambientLightAdaptBaseMult,
    };
    outData->creatorAmbientLightPack1 = {
        static_cast<float>(sanitizedPost.ambientLightAdaptBlackLevel),
        sanitizedPost.ambientLightDither ? 1.0f : 0.0f,
        sanitizedPost.ambientLightDirt ? 1.0f : 0.0f,
        static_cast<float>(sanitizedPost.ambientLightAdaptiveMode),
    };
    outData->creatorAmbientLightPack2 = {
        sanitizedPost.ambientLightDirtInt,
        sanitizedPost.ambientLightDirtOvrInt,
        sanitizedPost.timeSeconds,
        sanitizedPost.ambientLightAdaptation ? 1.0f : 0.0f,
    };
    outData->creatorFakeMotionBlurPack0 = {
        sanitizedPost.fakeMotionBlurRecall,
        sanitizedPost.fakeMotionBlurSoftness,
        0.0f,
        0.0f,
    };
    outData->creatorReflectiveBumpMappingPack0 = {
        sanitizedPost.reflectiveBumpMappingStrength,
        sanitizedPost.reflectiveBumpMappingBlurWidthPixels,
        sanitizedPost.reflectiveBumpMappingReliefHeight,
        sanitizedPost.reflectiveBumpMappingFresnelReflectance,
    };
    outData->creatorReflectiveBumpMappingPack1 = {
        sanitizedPost.reflectiveBumpMappingFresnelMult,
        sanitizedPost.reflectiveBumpMappingLowerThreshold,
        sanitizedPost.reflectiveBumpMappingUpperThreshold,
        static_cast<float>(sanitizedPost.reflectiveBumpMappingSampleCount),
    };
    outData->creatorReflectiveBumpMappingPack2 = {
        sanitizedPost.reflectiveBumpMappingColorMaskRed,
        sanitizedPost.reflectiveBumpMappingColorMaskOrange,
        sanitizedPost.reflectiveBumpMappingColorMaskYellow,
        sanitizedPost.reflectiveBumpMappingColorMaskGreen,
    };
    outData->creatorReflectiveBumpMappingPack3 = {
        sanitizedPost.reflectiveBumpMappingColorMaskCyan,
        sanitizedPost.reflectiveBumpMappingColorMaskBlue,
        sanitizedPost.reflectiveBumpMappingColorMaskMagenta,
        sanitizedPost.reflectiveBumpMappingDepthFarPlane,
    };
    outData->cropScaleContentIntermediate = {
        sanitizedPost.cropScaleContentWidth,
        sanitizedPost.cropScaleContentHeight,
        sanitizedPost.cropScaleIntermediateWidth,
        sanitizedPost.cropScaleIntermediateHeight,
    };
    outData->cropScaleFinalFilterStrength = {
        sanitizedPost.cropScaleFinalWidth,
        sanitizedPost.cropScaleFinalHeight,
        static_cast<float>(sanitizedPost.cropScaleFilter),
        sanitizedPost.cropScaleStrength,
    };
    outData->barbatosFakeHdrPack = {
        static_cast<float>(sanitizedPost.barbatosFakeHdrPreset),
        sanitizedPost.barbatosFakeHdrStrength,
        0.0f,
        0.0f,
    };
    outData->riAdaptiveDebandPack = {
        sanitizedPost.riAdaptiveDebandStrength,
        sanitizedPost.riAdaptiveDebandRadius,
        sanitizedPost.riAdaptiveDebandThreshold,
        static_cast<float>(sanitizedPost.riAdaptiveDebandIterations),
    };
    outData->riLocalSharpenPack = {
        sanitizedPost.riLocalSharpenStrength,
        sanitizedPost.riLocalSharpenRadius,
        sanitizedPost.riLocalSharpenClamp,
        sanitizedPost.riLocalSharpenEdgeLimit,
    };
    outData->riOutlinePack0 = {
        sanitizedPost.riOutlineStrength,
        sanitizedPost.riOutlineThickness,
        sanitizedPost.riOutlineDepthSensitivity,
        sanitizedPost.riOutlineColorSensitivity,
    };
    outData->riOutlineColorMethod = {
        sanitizedPost.riOutlineColor.x,
        sanitizedPost.riOutlineColor.y,
        sanitizedPost.riOutlineColor.z,
        static_cast<float>(sanitizedPost.riOutlineMethod),
    };
    outData->riOutlineWobbleDebug = {
        sanitizedPost.riOutlineWobbleAmount,
        sanitizedPost.riOutlineWobbleSpeed,
        sanitizedPost.riOutlineWobbleFrequency,
        sanitizedPost.riOutlineDebug,
    };
    outData->riSignalGlitchPack = {
        sanitizedPost.riSignalGlitchStrength,
        sanitizedPost.riSignalGlitchBlockSize,
        sanitizedPost.riSignalGlitchColorShiftPixels,
        sanitizedPost.riSignalGlitchSpeed,
    };
    outData->riNightVisionPack = {
        sanitizedPost.riNightVisionStrength,
        sanitizedPost.riNightVisionGain,
        sanitizedPost.riNightVisionNoise,
        sanitizedPost.riNightVisionVignette,
    };
    outData->riHq4xPack0 = {
        sanitizedPost.riHq4xStrength,
        sanitizedPost.riHq4xRadiusPixels,
        sanitizedPost.riHq4xSmoothing,
        sanitizedPost.riHq4xEdgeHardness,
    };
    outData->riHq4xPack1 = {
        sanitizedPost.riHq4xMinWeight,
        sanitizedPost.riHq4xMaxWeight,
        sanitizedPost.riHq4xLumaBias,
        0.0f,
    };
    const auto packHslAnchor = [](const ri::math::Vec3& anchor, float w) {
        return std::array<float, 4>{anchor.x, anchor.y, anchor.z, w};
    };
    outData->riHslAnchor0 = packHslAnchor(sanitizedPost.riHslRed, sanitizedPost.riHslShiftStrength);
    outData->riHslAnchor1 = packHslAnchor(sanitizedPost.riHslOrange, 0.0f);
    outData->riHslAnchor2 = packHslAnchor(sanitizedPost.riHslYellow, 0.0f);
    outData->riHslAnchor3 = packHslAnchor(sanitizedPost.riHslGreen, 0.0f);
    outData->riHslAnchor4 = packHslAnchor(sanitizedPost.riHslCyan, 0.0f);
    outData->riHslAnchor5 = packHslAnchor(sanitizedPost.riHslBlue, 0.0f);
    outData->riHslAnchor6 = packHslAnchor(sanitizedPost.riHslPurple, 0.0f);
    outData->riHslAnchor7 = packHslAnchor(sanitizedPost.riHslMagenta, 0.0f);
    outData->riLevelsPlusPack0 = packHslAnchor(sanitizedPost.riLevelsPlusInputBlack, sanitizedPost.riLevelsPlusStrength);
    outData->riLevelsPlusPack1 = packHslAnchor(sanitizedPost.riLevelsPlusInputWhite, sanitizedPost.riLevelsPlusColorShiftDirection);
    outData->riLevelsPlusPack2 = packHslAnchor(sanitizedPost.riLevelsPlusGamma, static_cast<float>(sanitizedPost.riLevelsPlusAcesMode));
    outData->riLevelsPlusPack3 = packHslAnchor(sanitizedPost.riLevelsPlusOutputBlack, sanitizedPost.riLevelsPlusClipDebug);
    outData->riLevelsPlusPack4 = packHslAnchor(sanitizedPost.riLevelsPlusOutputWhite, 0.0f);
    outData->riLevelsPlusPack5 = packHslAnchor(sanitizedPost.riLevelsPlusColorShift, 0.0f);
    outData->riLevelsPlusPack6 = packHslAnchor(sanitizedPost.riLevelsPlusAcesLuminance, 0.0f);
    outData->riLightDofPack0 = {
        sanitizedPost.riLightDofStrength,
        sanitizedPost.riLightDofWidthPixels,
        sanitizedPost.riLightDofAmount,
        sanitizedPost.riLightDofManualFocus,
    };
    outData->riLightDofPack1 = {
        sanitizedPost.riLightDofAutoFocus,
        sanitizedPost.riLightDofFocusPoint.x,
        sanitizedPost.riLightDofFocusPoint.y,
        sanitizedPost.riLightDofFarChromatic,
    };
    outData->riLightDofPack2 = {sanitizedPost.riLightDofNearChromatic, 0.0f, 0.0f, 0.0f};
    outData->riMagicBloomPack0 = {
        sanitizedPost.riMagicBloomStrength,
        sanitizedPost.riMagicBloomIntensity,
        sanitizedPost.riMagicBloomThresholdPower,
        sanitizedPost.riMagicBloomExposure,
    };
    outData->riMagicBloomPack1 = {
        sanitizedPost.riMagicBloomDirtIntensity,
        sanitizedPost.riMagicBloomRadiusPixels,
        sanitizedPost.riMagicBloomAdaptSensitivity,
        0.0f,
    };
    outData->riUiMaskPack0 = {
        sanitizedPost.riUiMaskStrength,
        sanitizedPost.riUiMaskIntensity,
        sanitizedPost.riUiMaskRed,
        sanitizedPost.riUiMaskGreen,
    };
    outData->riUiMaskPack1 = {sanitizedPost.riUiMaskBlue, sanitizedPost.riUiMaskDisplay, 0.0f, 0.0f};
    outData->riLuminanceThresholdPack = {
        sanitizedPost.riLuminanceThresholdStrength,
        sanitizedPost.riLuminanceThreshold,
        sanitizedPost.riLuminanceThresholdSoftness,
        0.0f,
    };
    outData->riColorQuantizePack0 = {
        sanitizedPost.riColorQuantizeStrength,
        sanitizedPost.riColorQuantizePixelate,
        sanitizedPost.riColorQuantizeResolution.x,
        sanitizedPost.riColorQuantizeResolution.y,
    };
    outData->riColorQuantizePack1 = {
        sanitizedPost.riColorQuantizeDither,
        static_cast<float>(sanitizedPost.riColorQuantizeDitherMode),
        sanitizedPost.riColorQuantizeLevels.x,
        sanitizedPost.riColorQuantizeLevels.y,
    };
    outData->riColorQuantizePack2 = {sanitizedPost.riColorQuantizeLevels.z, 0.0f, 0.0f, 0.0f};
    outData->riKaleidoscopePack0 = {
        sanitizedPost.riKaleidoscopeStrength,
        sanitizedPost.riKaleidoscopeSegments,
        sanitizedPost.riKaleidoscopeRotation,
        sanitizedPost.riKaleidoscopeSymmetry,
    };
    outData->riKaleidoscopePack1 = {sanitizedPost.riKaleidoscopeZoom, 0.0f, 0.0f, 0.0f};
    outData->renderQualityTier = std::clamp(frame.renderQualityTier, 0, 2);
    if (outData->renderQualityTier >= 2) {
        outData->localLightColorIntensity[3] =
            std::min(outData->localLightColorIntensity[3] * 1.14f, 4.2f);
    }
    const float vw = static_cast<float>(std::max(width, 1));
    const float vh = static_cast<float>(std::max(height, 1));
    outData->viewportMetrics = {vw, vh, 1.0f / vw, 1.0f / vh};
    return true;
}

void DestroyBuffer(VkDevice device, BufferResource& resource) {
    if (resource.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, resource.buffer, nullptr);
        resource.buffer = VK_NULL_HANDLE;
    }
    if (resource.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, resource.memory, nullptr);
        resource.memory = VK_NULL_HANDLE;
    }
}

void EnsureMappedBufferCapacity(VkPhysicalDevice physicalDevice,
                                VkDevice device,
                                VkBufferUsageFlags usage,
                                VkDeviceSize requiredSize,
                                BufferResource& resource,
                                VkDeviceSize& capacity,
                                void*& mappedMemory,
                                const char* label) {
    const VkDeviceSize targetSize = std::max<VkDeviceSize>(requiredSize, 1U);
    if (resource.buffer != VK_NULL_HANDLE && capacity >= targetSize && mappedMemory != nullptr) {
        return;
    }

    if (mappedMemory != nullptr && resource.memory != VK_NULL_HANDLE) {
        vkUnmapMemory(device, resource.memory);
        mappedMemory = nullptr;
    }
    DestroyBuffer(device, resource);

    resource = CreateBuffer(physicalDevice,
                            device,
                            targetSize,
                            usage,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    capacity = targetSize;
    ExpectVk(vkMapMemory(device, resource.memory, 0, VK_WHOLE_SIZE, 0, &mappedMemory), label);
}

void DestroyCachedGpuMesh(VkDevice device, CachedGpuMesh& mesh) {
    DestroyBuffer(device, mesh.vertexBuffer);
    DestroyBuffer(device, mesh.indexBuffer);
    mesh.indexCount = 0;
}

void ClearGpuMeshCache(VkDevice device, std::unordered_map<int, CachedGpuMesh>& cache) {
    for (auto& [meshHandle, mesh] : cache) {
        (void)meshHandle;
        DestroyCachedGpuMesh(device, mesh);
    }
    cache.clear();
}

void EnsureGpuMeshCached(VkPhysicalDevice physicalDevice,
                         VkDevice device,
                         const ri::scene::Scene& scene,
                         int meshHandle,
                         std::unordered_map<int, CachedGpuMesh>& cache) {
    if (cache.contains(meshHandle)) {
        return;
    }

    const CpuMeshGeometry geometry = BuildNativeMeshGeometry(scene.GetMesh(meshHandle));
    if (geometry.vertices.empty() || geometry.indices.empty()) {
        throw std::runtime_error("Unable to build Vulkan mesh geometry for mesh handle " + std::to_string(meshHandle) + ".");
    }

    CachedGpuMesh gpuMesh{};
    const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(geometry.vertices.size() * sizeof(NativeSceneVertex));
    gpuMesh.vertexBuffer = CreateBuffer(physicalDevice,
                                        device,
                                        vertexBytes,
                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* mappedVertexMemory = nullptr;
    ExpectVk(vkMapMemory(device, gpuMesh.vertexBuffer.memory, 0, VK_WHOLE_SIZE, 0, &mappedVertexMemory), "vkMapMemory(cached-vertex)");
    std::memcpy(mappedVertexMemory, geometry.vertices.data(), static_cast<std::size_t>(vertexBytes));
    vkUnmapMemory(device, gpuMesh.vertexBuffer.memory);

    const VkDeviceSize indexBytes = static_cast<VkDeviceSize>(geometry.indices.size() * sizeof(std::uint32_t));
    gpuMesh.indexBuffer = CreateBuffer(physicalDevice,
                                       device,
                                       indexBytes,
                                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* mappedIndexMemory = nullptr;
    ExpectVk(vkMapMemory(device, gpuMesh.indexBuffer.memory, 0, VK_WHOLE_SIZE, 0, &mappedIndexMemory), "vkMapMemory(cached-index)");
    std::memcpy(mappedIndexMemory, geometry.indices.data(), static_cast<std::size_t>(indexBytes));
    vkUnmapMemory(device, gpuMesh.indexBuffer.memory);

    gpuMesh.indexCount = static_cast<std::uint32_t>(geometry.indices.size());
    cache.emplace(meshHandle, std::move(gpuMesh));
}

CachedGpuMesh CreateStaticUnitCubeGpuMesh(VkPhysicalDevice physicalDevice, VkDevice device) {
    const CpuMeshGeometry geometry = BuildCubeMeshGeometryExpanded();
    if (geometry.vertices.empty() || geometry.indices.empty()) {
        throw std::runtime_error("Unable to build static unit cube mesh for Vulkan skybox.");
    }

    CachedGpuMesh gpuMesh{};
    const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(geometry.vertices.size() * sizeof(NativeSceneVertex));
    gpuMesh.vertexBuffer = CreateBuffer(physicalDevice,
                                        device,
                                        vertexBytes,
                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* mappedVertexMemory = nullptr;
    ExpectVk(vkMapMemory(device, gpuMesh.vertexBuffer.memory, 0, VK_WHOLE_SIZE, 0, &mappedVertexMemory),
             "vkMapMemory(skybox-vertex)");
    std::memcpy(mappedVertexMemory, geometry.vertices.data(), static_cast<std::size_t>(vertexBytes));
    vkUnmapMemory(device, gpuMesh.vertexBuffer.memory);

    const VkDeviceSize indexBytes = static_cast<VkDeviceSize>(geometry.indices.size() * sizeof(std::uint32_t));
    gpuMesh.indexBuffer = CreateBuffer(physicalDevice,
                                       device,
                                       indexBytes,
                                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* mappedIndexMemory = nullptr;
    ExpectVk(vkMapMemory(device, gpuMesh.indexBuffer.memory, 0, VK_WHOLE_SIZE, 0, &mappedIndexMemory),
             "vkMapMemory(skybox-index)");
    std::memcpy(mappedIndexMemory, geometry.indices.data(), static_cast<std::size_t>(indexBytes));
    vkUnmapMemory(device, gpuMesh.indexBuffer.memory);

    gpuMesh.indexCount = static_cast<std::uint32_t>(geometry.indices.size());
    return gpuMesh;
}

BufferResource CreateBuffer(VkPhysicalDevice physicalDevice,
                            VkDevice device,
                            VkDeviceSize size,
                            VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags memoryFlags) {
    BufferResource resource{};
    const VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = std::max<VkDeviceSize>(size, 1U),
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    ExpectVk(vkCreateBuffer(device, &bufferInfo, nullptr, &resource.buffer), "vkCreateBuffer");

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, resource.buffer, &requirements);
    const VkMemoryAllocateInfo allocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = FindMemoryType(physicalDevice, requirements.memoryTypeBits, memoryFlags),
    };
    ExpectVk(vkAllocateMemory(device, &allocateInfo, nullptr, &resource.memory), "vkAllocateMemory(buffer)");
    ExpectVk(vkBindBufferMemory(device, resource.buffer, resource.memory, 0), "vkBindBufferMemory");
    return resource;
}

ImageResource CreateDepthImage(VkPhysicalDevice physicalDevice,
                               VkDevice device,
                               VkFormat format,
                               std::uint32_t width,
                               std::uint32_t height,
                               VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    ImageResource resource{};
    const VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ExpectVk(vkCreateImage(device, &imageInfo, nullptr, &resource.image), "vkCreateImage(depth)");

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, resource.image, &requirements);
    const VkMemoryAllocateInfo allocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = FindMemoryType(physicalDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    ExpectVk(vkAllocateMemory(device, &allocateInfo, nullptr, &resource.memory), "vkAllocateMemory(depth)");
    ExpectVk(vkBindImageMemory(device, resource.image, resource.memory, 0), "vkBindImageMemory(depth)");

    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = resource.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    ExpectVk(vkCreateImageView(device, &viewInfo, nullptr, &resource.view), "vkCreateImageView(depth)");
    return resource;
}

ImageResource CreateHdrSceneColorImage(VkPhysicalDevice physicalDevice,
                                       VkDevice device,
                                       VkFormat format,
                                       std::uint32_t width,
                                       std::uint32_t height) {
    ImageResource resource{};
    const VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ExpectVk(vkCreateImage(device, &imageInfo, nullptr, &resource.image), "vkCreateImage(hdr-scene)");

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, resource.image, &requirements);
    const VkMemoryAllocateInfo allocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = FindMemoryType(physicalDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    ExpectVk(vkAllocateMemory(device, &allocateInfo, nullptr, &resource.memory), "vkAllocateMemory(hdr-scene)");
    ExpectVk(vkBindImageMemory(device, resource.image, resource.memory, 0), "vkBindImageMemory(hdr-scene)");

    const VkImageViewCreateInfo hdrViewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = resource.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    ExpectVk(vkCreateImageView(device, &hdrViewInfo, nullptr, &resource.view), "vkCreateImageView(hdr-scene)");
    return resource;
}

ImageResource CreateRgba8HistoryImage(VkPhysicalDevice physicalDevice,
                                      VkDevice device,
                                      VkFormat format,
                                      const std::uint32_t width,
                                      const std::uint32_t height) {
    ImageResource resource{};
    const VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ExpectVk(vkCreateImage(device, &imageInfo, nullptr, &resource.image), "vkCreateImage(fake-motion-blur-history)");

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, resource.image, &requirements);
    const VkMemoryAllocateInfo allocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = FindMemoryType(physicalDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    ExpectVk(vkAllocateMemory(device, &allocateInfo, nullptr, &resource.memory), "vkAllocateMemory(fake-motion-blur-history)");
    ExpectVk(vkBindImageMemory(device, resource.image, resource.memory, 0), "vkBindImageMemory(fake-motion-blur-history)");

    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = resource.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    ExpectVk(vkCreateImageView(device, &viewInfo, nullptr, &resource.view), "vkCreateImageView(fake-motion-blur-history)");
    return resource;
}

void TransitionImageLayout(VkCommandBuffer commandBuffer,
                           VkImage image,
                           VkImageLayout oldLayout,
                           VkImageLayout newLayout,
                           VkAccessFlags srcAccessMask,
                           VkAccessFlags dstAccessMask,
                           VkPipelineStageFlags srcStage,
                           VkPipelineStageFlags dstStage) {
    const VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void RecordFakeMotionBlurHistoryCopy(VkCommandBuffer commandBuffer,
                                     VkImage swapchainImage,
                                     VkImage historyImage,
                                     VkExtent2D extent) {
    TransitionImageLayout(commandBuffer,
                          swapchainImage,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_ACCESS_MEMORY_READ_BIT,
                          VK_ACCESS_TRANSFER_READ_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT);
    TransitionImageLayout(commandBuffer,
                          historyImage,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT);

    const VkImageCopy copyRegion{
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcOffset = {0, 0, 0},
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .dstOffset = {0, 0, 0},
        .extent = {extent.width, extent.height, 1},
    };
    vkCmdCopyImage(commandBuffer,
                   swapchainImage,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   historyImage,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &copyRegion);

    TransitionImageLayout(commandBuffer,
                          swapchainImage,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                          VK_ACCESS_TRANSFER_READ_BIT,
                          VK_ACCESS_MEMORY_READ_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    TransitionImageLayout(commandBuffer,
                          historyImage,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

struct GpuAlbedoImage {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

void DestroyGpuAlbedoImage(VkDevice device, GpuAlbedoImage& image) {
    if (image.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, image.view, nullptr);
        image.view = VK_NULL_HANDLE;
    }
    if (image.image != VK_NULL_HANDLE) {
        vkDestroyImage(device, image.image, nullptr);
        image.image = VK_NULL_HANDLE;
    }
    if (image.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, image.memory, nullptr);
        image.memory = VK_NULL_HANDLE;
    }
}

void SubmitOneTimeCommands(VkDevice device,
                           VkCommandPool commandPool,
                           VkQueue queue,
                           const std::function<void(VkCommandBuffer)>& recorder) {
    const VkCommandBufferAllocateInfo allocateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    ExpectVk(vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer), "vkAllocateCommandBuffers(upload)");

    const VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    ExpectVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer(upload)");
    recorder(commandBuffer);
    ExpectVk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer(upload)");

    const VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
    };
    ExpectVk(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit(upload)");
    ExpectVk(vkQueueWaitIdle(queue), "vkQueueWaitIdle(upload)");
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

// Uploads an RGBA8 texture. `format` selects the colour-space interpretation: colour
// maps (albedo/emissive/detail) use VK_FORMAT_R8G8B8A8_SRGB so they are gamma-decoded on
// sample, while data maps (normal/ORM/opacity) MUST use VK_FORMAT_R8G8B8A8_UNORM so their
// raw bytes are read verbatim -- an sRGB normal/ORM is silently warped by the hardware.
GpuAlbedoImage CreateGpuRgba8Image(VkPhysicalDevice physicalDevice,
                                   VkDevice device,
                                   VkCommandPool commandPool,
                                   VkQueue queue,
                                   const int width,
                                   const int height,
                                   const std::uint8_t* rgbaPixels,
                                   const VkFormat format) {
    if (width <= 0 || height <= 0 || rgbaPixels == nullptr) {
        return {};
    }

    const auto mipLevelsFor = [](const int w, const int h) -> std::uint32_t {
        const int largest = std::max(w, h);
        return largest > 0 ? static_cast<std::uint32_t>(std::floor(std::log2(static_cast<float>(largest)))) + 1U : 1U;
    };
    VkFormatProperties formatProperties{};
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
    const bool canBlitMipmaps =
        (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0
        && (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0
        && (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
    const std::uint32_t mipLevels = canBlitMipmaps ? mipLevelsFor(width, height) : 1U;
    const VkDeviceSize pixelBytes =
        static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4ULL;
    BufferResource staging = CreateBuffer(physicalDevice,
                                            device,
                                            pixelBytes,
                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* mapped = nullptr;
    ExpectVk(vkMapMemory(device, staging.memory, 0, VK_WHOLE_SIZE, 0, &mapped), "vkMapMemory(staging-tex)");
    std::memcpy(mapped, rgbaPixels, static_cast<std::size_t>(pixelBytes));
    vkUnmapMemory(device, staging.memory);

    GpuAlbedoImage out{};
    const VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1},
        .mipLevels = mipLevels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ExpectVk(vkCreateImage(device, &imageInfo, nullptr, &out.image), "vkCreateImage(albedo)");

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, out.image, &requirements);
    const VkMemoryAllocateInfo allocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = FindMemoryType(physicalDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    ExpectVk(vkAllocateMemory(device, &allocateInfo, nullptr, &out.memory), "vkAllocateMemory(albedo)");
    ExpectVk(vkBindImageMemory(device, out.image, out.memory, 0), "vkBindImageMemory(albedo)");

    SubmitOneTimeCommands(device, commandPool, queue, [&](VkCommandBuffer cmd) {
        const VkImageMemoryBarrier undefToDst{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = out.image,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &undefToDst);

        const VkBufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .imageExtent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1},
        };
        vkCmdCopyBufferToImage(cmd, staging.buffer, out.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        VkImageMemoryBarrier mipBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = out.image,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };
        int mipWidth = width;
        int mipHeight = height;
        for (std::uint32_t level = 1; level < mipLevels; ++level) {
            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &mipBarrier);

            const VkImageBlit blit{
                .srcSubresource =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = level - 1U,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                .srcOffsets =
                    {
                        VkOffset3D{0, 0, 0},
                        VkOffset3D{mipWidth, mipHeight, 1},
                    },
                .dstSubresource =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = level,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                .dstOffsets =
                    {
                        VkOffset3D{0, 0, 0},
                        VkOffset3D{std::max(1, mipWidth / 2), std::max(1, mipHeight / 2), 1},
                    },
            };
            vkCmdBlitImage(cmd,
                           out.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           out.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &blit,
                           VK_FILTER_LINEAR);

            VkImageMemoryBarrier srcToShader = mipBarrier;
            srcToShader.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            srcToShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcToShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            srcToShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            srcToShader.subresourceRange.baseMipLevel = level - 1U;
            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &srcToShader);

            mipBarrier.subresourceRange.baseMipLevel = level;
            mipWidth = std::max(1, mipWidth / 2);
            mipHeight = std::max(1, mipHeight / 2);
        }

        VkImageMemoryBarrier finalToShader{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = static_cast<VkAccessFlags>(VK_ACCESS_TRANSFER_WRITE_BIT),
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = out.image,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = mipLevels - 1U,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };
        if (mipLevels == 1U) {
            finalToShader.subresourceRange.baseMipLevel = 0;
        }
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &finalToShader);
    });

    DestroyBuffer(device, staging);

    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = out.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = mipLevels,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    ExpectVk(vkCreateImageView(device, &viewInfo, nullptr, &out.view), "vkCreateImageView(albedo)");
    return out;
}

// ---------------------------------------------------------------------------
// On-disk cache for generated material maps.
//
// Generated normal/ORM maps are derived from the source albedo. To avoid paying
// the generation cost on every launch, results are persisted as a tiny raw RGBA
// blob keyed by source path + parameters + generator version, and invalidated
// when the source texture's modification time changes.
// ---------------------------------------------------------------------------
namespace procedural_cache {

constexpr std::uint32_t kMagic = 0x314D4952U; // "RIM1"

[[nodiscard]] std::int64_t SourceModificationStamp(const fs::path& sourcePath) {
    std::error_code ec{};
    const auto stamp = fs::last_write_time(sourcePath, ec);
    if (ec) {
        return 0;
    }
    return static_cast<std::int64_t>(stamp.time_since_epoch().count());
}

// Stable FNV-1a 64-bit. std::hash is implementation-defined (varies by stdlib/build and
// can collide), which is unsafe for an on-disk cache key; this is deterministic forever.
[[nodiscard]] std::uint64_t StableHash64(const std::string& text) {
    std::uint64_t hash = 0xCBF29CE484222325ULL;
    for (const unsigned char byte : text) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 0x00000100000001B3ULL;
    }
    return hash;
}

[[nodiscard]] fs::path CacheFilePath(const fs::path& cacheDir, const std::string& cacheKey) {
    const std::uint64_t hashed = StableHash64(cacheKey);
    char name[32] = {};
    std::snprintf(name, sizeof(name), "%016llx.rimap", static_cast<unsigned long long>(hashed));
    return cacheDir / name;
}

[[nodiscard]] ri::render::software::RgbaImage Read(const fs::path& cacheFile, const std::int64_t expectedStamp) {
    ri::render::software::RgbaImage image{};
    std::ifstream stream(cacheFile, std::ios::binary);
    if (!stream) {
        return image;
    }
    std::uint32_t magic = 0;
    std::int32_t version = 0;
    std::uint32_t buildId = 0;
    std::int64_t stamp = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    stream.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    stream.read(reinterpret_cast<char*>(&version), sizeof(version));
    stream.read(reinterpret_cast<char*>(&buildId), sizeof(buildId));
    stream.read(reinterpret_cast<char*>(&stamp), sizeof(stamp));
    stream.read(reinterpret_cast<char*>(&width), sizeof(width));
    stream.read(reinterpret_cast<char*>(&height), sizeof(height));
    if (!stream || magic != kMagic || version != ri::render::vulkan::kProceduralMapGeneratorVersion
        || buildId != ri::render::vulkan::ProceduralGeneratorBuildId()
        || stamp != expectedStamp || width <= 0 || height <= 0) {
        return image;
    }
    const std::size_t byteCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U;
    image.rgba.resize(byteCount);
    stream.read(reinterpret_cast<char*>(image.rgba.data()), static_cast<std::streamsize>(byteCount));
    if (!stream) {
        return ri::render::software::RgbaImage{};
    }
    image.width = width;
    image.height = height;
    return image;
}

void Write(const fs::path& cacheFile,
           const std::int64_t sourceStamp,
           const ri::render::software::RgbaImage& image) {
    if (!image.Valid()) {
        return;
    }
    std::error_code ec{};
    fs::create_directories(cacheFile.parent_path(), ec);
    std::ofstream stream(cacheFile, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return;
    }
    const std::int32_t version = ri::render::vulkan::kProceduralMapGeneratorVersion;
    const std::uint32_t buildId = ri::render::vulkan::ProceduralGeneratorBuildId();
    const std::int32_t width = image.width;
    const std::int32_t height = image.height;
    stream.write(reinterpret_cast<const char*>(&kMagic), sizeof(kMagic));
    stream.write(reinterpret_cast<const char*>(&version), sizeof(version));
    stream.write(reinterpret_cast<const char*>(&buildId), sizeof(buildId));
    stream.write(reinterpret_cast<const char*>(&sourceStamp), sizeof(sourceStamp));
    stream.write(reinterpret_cast<const char*>(&width), sizeof(width));
    stream.write(reinterpret_cast<const char*>(&height), sizeof(height));
    stream.write(reinterpret_cast<const char*>(image.rgba.data()),
                 static_cast<std::streamsize>(image.rgba.size()));
}

} // namespace procedural_cache

// Colour maps are gamma-decoded on sample; data maps (normal/ORM/opacity) must be read
// verbatim, otherwise the hardware sRGB curve warps their meaning (e.g. a 128/128/255
// "neutral" normal becomes garbage). See CreateGpuRgba8Image.
inline constexpr VkFormat kColorTextureFormat = VK_FORMAT_R8G8B8A8_SRGB;
inline constexpr VkFormat kDataTextureFormat = VK_FORMAT_R8G8B8A8_UNORM;

[[nodiscard]] ProceduralNormalMapOptions BuildProceduralNormalOptions(const ri::scene::Material& material,
                                                                    const std::int32_t materialStyleFlags) {
    ProceduralNormalMapOptions options{};
    const bool layered = (materialStyleFlags & kNativeMaterialStyleLayered) != 0;
    const bool mixedMedia = (materialStyleFlags & kNativeMaterialStyleMixedMedia) != 0;
    if (layered) {
        options.strength = 1.75f;
        options.heightScale = 1.16f;
    } else if (mixedMedia) {
        options.strength = 1.38f;
        options.heightScale = 1.06f;
    }
    const float roughness = std::clamp(material.roughness, 0.04f, 1.0f);
    options.strength *= std::lerp(1.0f, 0.82f, roughness);
    return options;
}

[[nodiscard]] ProceduralOrmMapOptions BuildProceduralOrmOptions(const ri::scene::Material& material,
                                                                const std::int32_t materialStyleFlags) {
    ProceduralOrmMapOptions options{};
    options.baseRoughness = std::clamp(material.roughness, 0.04f, 1.0f);
    options.baseMetallic = std::clamp(material.metallic, 0.0f, 1.0f);
    const bool layered = (materialStyleFlags & kNativeMaterialStyleLayered) != 0;
    options.aoStrength = layered ? 0.72f : 0.64f;
    options.roughnessDetail = layered ? 0.34f : 0.28f;
    return options;
}

enum class ProceduralMapKind : std::int32_t {
    NormalFromAlbedo = 0,
    OrmFromAlbedo = 1,
    OrmFromNormal = 2,
};

// Matches the push_constant block in ProceduralMaps.comp (tightly packed 4-byte scalars).
struct ProceduralMapPushConstants {
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t kind = 0;
    std::int32_t wrap = 1;
    std::int32_t applyHeightTransform = 0;
    std::int32_t invertHeight = 0;
    float strength = 1.2f;
    float heightBias = 0.0f;
    float heightScale = 1.0f;
    float baseRoughness = 0.85f;
    float baseMetallic = 0.0f;
    float aoStrength = 0.6f;
    float roughnessDetail = 0.25f;
};

// Vulkan compute path for procedural material-map generation. Produces output that
// matches the CPU reference generators in ProceduralMaterialMaps.cpp, then reads the
// result back into an RgbaImage so it flows through the very same on-disk cache. If
// the device/queue cannot support compute (or any setup step fails) it reports
// unavailable and callers transparently fall back to the CPU generators.
struct ProceduralMapGpuGenerator {
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    bool available = false;

    [[nodiscard]] bool isAvailable() const { return available; }

    void initialize(VkPhysicalDevice physDevice, VkDevice dev, VkQueue computeQueue, std::uint32_t queueFamily) {
        physicalDevice = physDevice;
        device = dev;
        queue = computeQueue;

        // Escape hatch for parity testing / driver issues: force the CPU reference path.
        if (EnvironmentPath("RAWIRON_DISABLE_GPU_PROCGEN").has_value()) {
            return;
        }

        // The shared graphics queue family must also advertise compute support.
        std::uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);
        if (queueFamily >= familyCount) {
            return;
        }
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, families.data());
        if ((families[queueFamily].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0U) {
            return;
        }

        const fs::path shaderDir = ResolveVulkanNativeShaderDirectory();
        const fs::path shaderPath = shaderDir / "ProceduralMaps.comp.spv";
        std::error_code ec{};
        if (shaderDir.empty() || !fs::exists(shaderPath, ec)) {
            return;
        }

        try {
            const VkCommandPoolCreateInfo poolInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = queueFamily,
            };
            ExpectVk(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "vkCreateCommandPool(procgen)");

            const std::array<VkDescriptorSetLayoutBinding, 2> bindings{{
                {.binding = 0,
                 .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                 .descriptorCount = 1,
                 .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
                {.binding = 1,
                 .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                 .descriptorCount = 1,
                 .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
            }};
            const VkDescriptorSetLayoutCreateInfo layoutInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = static_cast<std::uint32_t>(bindings.size()),
                .pBindings = bindings.data(),
            };
            ExpectVk(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &setLayout),
                     "vkCreateDescriptorSetLayout(procgen)");

            const VkPushConstantRange pushRange{
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = sizeof(ProceduralMapPushConstants),
            };
            const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 1,
                .pSetLayouts = &setLayout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &pushRange,
            };
            ExpectVk(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout),
                     "vkCreatePipelineLayout(procgen)");

            const VkShaderModule module = CreateShaderModule(device, shaderPath);
            const VkComputePipelineCreateInfo pipelineInfo{
                .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .stage =
                    {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                        .module = module,
                        .pName = "main",
                    },
                .layout = pipelineLayout,
            };
            const VkResult pipelineResult =
                vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
            vkDestroyShaderModule(device, module, nullptr);
            ExpectVk(pipelineResult, "vkCreateComputePipelines(procgen)");

            const VkDescriptorPoolSize poolSize{
                .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 2,
            };
            const VkDescriptorPoolCreateInfo descPoolInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .maxSets = 1,
                .poolSizeCount = 1,
                .pPoolSizes = &poolSize,
            };
            ExpectVk(vkCreateDescriptorPool(device, &descPoolInfo, nullptr, &descriptorPool),
                     "vkCreateDescriptorPool(procgen)");

            const VkDescriptorSetAllocateInfo setAllocInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = descriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &setLayout,
            };
            ExpectVk(vkAllocateDescriptorSets(device, &setAllocInfo, &descriptorSet),
                     "vkAllocateDescriptorSets(procgen)");

            available = true;
        } catch (const std::exception&) {
            destroy();
        }
    }

    void destroy() {
        available = false;
        if (device == VK_NULL_HANDLE) {
            return;
        }
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }
        if (setLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
            setLayout = VK_NULL_HANDLE;
        }
        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            descriptorPool = VK_NULL_HANDLE;
            descriptorSet = VK_NULL_HANDLE;
        }
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }
        device = VK_NULL_HANDLE;
    }

    // Runs the compute generator and reads the result back. Returns an empty image on
    // any failure so the caller falls back to the CPU path.
    [[nodiscard]] ri::render::software::RgbaImage generate(const ProceduralMapKind kind,
                                                           const ri::render::software::RgbaImage& source,
                                                           const ProceduralNormalMapOptions& normalOptions,
                                                           const ProceduralOrmMapOptions& ormOptions) {
        if (!available || !source.Valid()) {
            return {};
        }
        const VkDeviceSize byteSize = static_cast<VkDeviceSize>(source.width) * static_cast<VkDeviceSize>(source.height) * 4U;
        if (byteSize == 0U) {
            return {};
        }

        BufferResource srcBuffer{};
        BufferResource dstBuffer{};
        ri::render::software::RgbaImage result{};
        try {
            srcBuffer = CreateBuffer(physicalDevice, device, byteSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            dstBuffer = CreateBuffer(physicalDevice, device, byteSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            void* mapped = nullptr;
            ExpectVk(vkMapMemory(device, srcBuffer.memory, 0, byteSize, 0, &mapped), "vkMapMemory(procgen-src)");
            std::memcpy(mapped, source.rgba.data(), static_cast<std::size_t>(byteSize));
            vkUnmapMemory(device, srcBuffer.memory);

            const std::array<VkDescriptorBufferInfo, 2> bufferInfos{{
                {.buffer = srcBuffer.buffer, .offset = 0, .range = byteSize},
                {.buffer = dstBuffer.buffer, .offset = 0, .range = byteSize},
            }};
            const std::array<VkWriteDescriptorSet, 2> writes{{
                {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 .dstSet = descriptorSet,
                 .dstBinding = 0,
                 .descriptorCount = 1,
                 .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                 .pBufferInfo = &bufferInfos[0]},
                {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 .dstSet = descriptorSet,
                 .dstBinding = 1,
                 .descriptorCount = 1,
                 .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                 .pBufferInfo = &bufferInfos[1]},
            }};
            vkUpdateDescriptorSets(device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);

            ProceduralMapPushConstants push{};
            push.width = source.width;
            push.height = source.height;
            push.kind = static_cast<std::int32_t>(kind);
            push.wrap = (kind == ProceduralMapKind::NormalFromAlbedo ? normalOptions.wrap : ormOptions.wrap) ? 1 : 0;
            push.strength = normalOptions.strength;
            push.heightBias = normalOptions.heightBias;
            push.heightScale = normalOptions.heightScale;
            push.invertHeight = normalOptions.invertHeight ? 1 : 0;
            push.applyHeightTransform =
                (kind == ProceduralMapKind::NormalFromAlbedo
                 && (normalOptions.invertHeight || normalOptions.heightBias != 0.0f || normalOptions.heightScale != 1.0f))
                    ? 1
                    : 0;
            push.baseRoughness = ormOptions.baseRoughness;
            push.baseMetallic = ormOptions.baseMetallic;
            push.aoStrength = ormOptions.aoStrength;
            push.roughnessDetail = ormOptions.roughnessDetail;

            const std::uint32_t groupsX = (static_cast<std::uint32_t>(source.width) + 7U) / 8U;
            const std::uint32_t groupsY = (static_cast<std::uint32_t>(source.height) + 7U) / 8U;
            SubmitOneTimeCommands(device, commandPool, queue, [&](VkCommandBuffer cmd) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0,
                                        nullptr);
                vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
                vkCmdDispatch(cmd, groupsX, groupsY, 1);
                const VkBufferMemoryBarrier barrier{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer = dstBuffer.buffer,
                    .offset = 0,
                    .size = byteSize,
                };
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                                     nullptr, 1, &barrier, 0, nullptr);
            });

            result.width = source.width;
            result.height = source.height;
            result.rgba.resize(static_cast<std::size_t>(byteSize));
            void* readback = nullptr;
            ExpectVk(vkMapMemory(device, dstBuffer.memory, 0, byteSize, 0, &readback), "vkMapMemory(procgen-dst)");
            std::memcpy(result.rgba.data(), readback, static_cast<std::size_t>(byteSize));
            vkUnmapMemory(device, dstBuffer.memory);
        } catch (const std::exception&) {
            result = {};
        }
        DestroyBuffer(device, srcBuffer);
        DestroyBuffer(device, dstBuffer);
        return result;
    }
};

struct NativeAlbedoTextureCache {
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    ProceduralMapGpuGenerator gpuGenerator{};
    VkDescriptorPool textureDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorPool> textureDescriptorPools{};
    std::uint32_t descriptorSetsPerPool = 0;
    std::uint32_t allocatedDescriptorSetCount = 0;
    VkDescriptorSetLayout textureSetLayout = VK_NULL_HANDLE;
    VkSampler linearSampler = VK_NULL_HANDLE;
    GpuAlbedoImage whiteImage{};
    GpuAlbedoImage blackImage{};
    GpuAlbedoImage flatNormalImage{};
    GpuAlbedoImage ormDefaultImage{};
    VkDescriptorSet whiteDescriptorSet = VK_NULL_HANDLE;
    bool loggedDescriptorPoolFailure = false;
    std::unordered_map<std::string, GpuAlbedoImage> imagesByKey{};
    std::unordered_map<std::string, VkDescriptorSet> descriptorByKey{};
    std::unordered_map<std::string, fs::path> siblingMapByKey{};
    fs::path proceduralCacheDir{};
    VulkanWarmupCache decodedWarmupCache{};

    void destroy() {
        if (device == VK_NULL_HANDLE) {
            return;
        }
        gpuGenerator.destroy();
        descriptorByKey.clear();
        whiteDescriptorSet = VK_NULL_HANDLE;
        for (const VkDescriptorPool pool : textureDescriptorPools) {
            if (pool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, pool, nullptr);
            }
        }
        textureDescriptorPools.clear();
        textureDescriptorPool = VK_NULL_HANDLE;
        descriptorSetsPerPool = 0;
        allocatedDescriptorSetCount = 0;
        for (auto& entry : imagesByKey) {
            DestroyGpuAlbedoImage(device, entry.second);
        }
        imagesByKey.clear();
        DestroyGpuAlbedoImage(device, whiteImage);
        DestroyGpuAlbedoImage(device, blackImage);
        DestroyGpuAlbedoImage(device, flatNormalImage);
        DestroyGpuAlbedoImage(device, ormDefaultImage);
        if (linearSampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, linearSampler, nullptr);
            linearSampler = VK_NULL_HANDLE;
        }
        textureSetLayout = VK_NULL_HANDLE;
        device = VK_NULL_HANDLE;
        physicalDevice = VK_NULL_HANDLE;
        commandPool = VK_NULL_HANDLE;
        graphicsQueue = VK_NULL_HANDLE;
    }

    [[nodiscard]] std::string ResolveMaterialTextureRelPath(const ri::scene::Material& material) const {
        if (!material.baseColorTexture.empty()) {
            return material.baseColorTexture;
        }
        if (!material.baseColorTextureFrames.empty() && !material.baseColorTextureFrames.front().empty()) {
            return material.baseColorTextureFrames.front();
        }
        return {};
    }

    [[nodiscard]] const GpuAlbedoImage& ResolveFallbackWhite() const { return whiteImage; }
    [[nodiscard]] const GpuAlbedoImage& ResolveFallbackBlack() const { return blackImage; }
    [[nodiscard]] const GpuAlbedoImage& ResolveFallbackFlatNormal() const { return flatNormalImage; }
    [[nodiscard]] const GpuAlbedoImage& ResolveFallbackOrm() const { return ormDefaultImage; }

    [[nodiscard]] const GpuAlbedoImage* resolveImageForAbsolutePath(const fs::path& absolutePath,
                                                                    const VkFormat format) {
        if (absolutePath.empty()) {
            return nullptr;
        }
        // Key on the colour-space too: the same file used as both colour and data must not
        // alias to a single (wrongly-decoded) GPU image.
        const std::string key = absolutePath.generic_string() + "|fmt=" + std::to_string(static_cast<int>(format));
        if (const auto it = imagesByKey.find(key); it != imagesByKey.end()) {
            return &it->second;
        }

        std::shared_ptr<const ri::render::software::RgbaImage> rgba = decodedWarmupCache.Load(absolutePath);
        if (!rgba || !rgba->Valid()) {
            static const std::array<const char*, 4> kAlternateExtensions{".png", ".tif", ".tiff", ".jpg"};
            const std::string stem = absolutePath.stem().string();
            const fs::path parent = absolutePath.parent_path();
            for (const char* ext : kAlternateExtensions) {
                const fs::path alternate = parent / (stem + ext);
                if (alternate == absolutePath) {
                    continue;
                }
                std::error_code ec{};
                if (!fs::exists(alternate, ec) || ec) {
                    continue;
                }
                rgba = decodedWarmupCache.Load(alternate);
                if (rgba && rgba->Valid()) {
                    break;
                }
            }
        }
        if (!rgba || !rgba->Valid()) {
            return nullptr;
        }

        GpuAlbedoImage gpuImage = CreateGpuRgba8Image(
            physicalDevice, device, commandPool, graphicsQueue, rgba->width, rgba->height, rgba->rgba.data(), format);
        if (gpuImage.view == VK_NULL_HANDLE) {
            return nullptr;
        }
        const auto [it, _] = imagesByKey.emplace(key, std::move(gpuImage));
        return &it->second;
    }

    [[nodiscard]] const GpuAlbedoImage* uploadGeneratedImage(const std::string& key,
                                                             const ri::render::software::RgbaImage& image,
                                                             const VkFormat format) {
        if (!image.Valid()) {
            return nullptr;
        }
        if (const auto it = imagesByKey.find(key); it != imagesByKey.end()) {
            return &it->second;
        }
        GpuAlbedoImage gpuImage = CreateGpuRgba8Image(
            physicalDevice, device, commandPool, graphicsQueue, image.width, image.height, image.rgba.data(), format);
        if (gpuImage.view == VK_NULL_HANDLE) {
            return nullptr;
        }
        const auto [it, _] = imagesByKey.emplace(key, std::move(gpuImage));
        return &it->second;
    }

    // Runs the GPU compute generator when available and falls back to the CPU reference
    // implementation otherwise. Both paths produce equivalent output that is then cached.
    [[nodiscard]] ri::render::software::RgbaImage generateMap(const ProceduralMapKind kind,
                                                             const ri::render::software::RgbaImage& source,
                                                             const ProceduralNormalMapOptions& normalOptions,
                                                             const ProceduralOrmMapOptions& ormOptions) {
        if (gpuGenerator.isAvailable()) {
            ri::render::software::RgbaImage gpu = gpuGenerator.generate(kind, source, normalOptions, ormOptions);
            if (gpu.Valid()) {
                return gpu;
            }
        }
        switch (kind) {
            case ProceduralMapKind::NormalFromAlbedo:
                return ri::render::vulkan::GenerateNormalMapFromAlbedo(source, normalOptions);
            case ProceduralMapKind::OrmFromAlbedo:
                return ri::render::vulkan::GenerateOrmMapFromAlbedo(source, ormOptions);
            case ProceduralMapKind::OrmFromNormal:
                return ri::render::vulkan::GenerateOrmMapFromNormal(source, ormOptions);
        }
        return {};
    }

    // Derives and caches a normal map from the albedo at the given path. Returns the flat-normal
    // fallback view's owner only via nullptr when generation is unavailable.
    [[nodiscard]] const GpuAlbedoImage* resolveGeneratedNormal(const fs::path& albedoPath,
                                                               const ProceduralNormalMapOptions& normalOptions) {
        if (albedoPath.empty()) {
            return nullptr;
        }
        const std::string key = std::string("__gen_normal__|") + albedoPath.generic_string() + "|s="
                                + std::to_string(normalOptions.strength) + "|hs="
                                + std::to_string(normalOptions.heightScale);
        if (const auto it = imagesByKey.find(key); it != imagesByKey.end()) {
            return &it->second;
        }
        const ri::render::software::RgbaImage normal = generateOrLoadCached(
            albedoPath,
            key,
            [this, normalOptions](const ri::render::software::RgbaImage& albedo) {
                return generateMap(ProceduralMapKind::NormalFromAlbedo, albedo, normalOptions,
                                   ri::render::vulkan::ProceduralOrmMapOptions{});
            });
        return uploadGeneratedImage(key, normal, kDataTextureFormat);
    }

    [[nodiscard]] const GpuAlbedoImage* resolveGeneratedOrm(const fs::path& albedoPath,
                                                            const ProceduralOrmMapOptions& ormOptions) {
        if (albedoPath.empty()) {
            return nullptr;
        }
        const std::string key = std::string("__gen_orm__|") + albedoPath.generic_string() + "|r="
                                + std::to_string(ormOptions.baseRoughness) + "|m="
                                + std::to_string(ormOptions.baseMetallic) + "|ao="
                                + std::to_string(ormOptions.aoStrength) + "|rd="
                                + std::to_string(ormOptions.roughnessDetail);
        if (const auto it = imagesByKey.find(key); it != imagesByKey.end()) {
            return &it->second;
        }
        const ri::render::software::RgbaImage orm = generateOrLoadCached(
            albedoPath,
            key,
            [this, ormOptions](const ri::render::software::RgbaImage& albedo) {
                return generateMap(ProceduralMapKind::OrmFromAlbedo, albedo,
                                   ri::render::vulkan::ProceduralNormalMapOptions{}, ormOptions);
            });
        return uploadGeneratedImage(key, orm, kDataTextureFormat);
    }

    // Higher-quality ORM derived by cross-referencing an authored/discovered normal map
    // (occlusion from curvature, roughness from slope) instead of luminance heuristics.
    [[nodiscard]] const GpuAlbedoImage* resolveGeneratedOrmFromNormal(const fs::path& normalPath,
                                                                      const ProceduralOrmMapOptions& ormOptions) {
        if (normalPath.empty()) {
            return nullptr;
        }
        const std::string key = std::string("__gen_orm_xnormal__|") + normalPath.generic_string() + "|r="
                                + std::to_string(ormOptions.baseRoughness) + "|m="
                                + std::to_string(ormOptions.baseMetallic) + "|ao="
                                + std::to_string(ormOptions.aoStrength) + "|rd="
                                + std::to_string(ormOptions.roughnessDetail);
        if (const auto it = imagesByKey.find(key); it != imagesByKey.end()) {
            return &it->second;
        }
        const ri::render::software::RgbaImage orm = generateOrLoadCached(
            normalPath,
            key,
            [this, ormOptions](const ri::render::software::RgbaImage& normal) {
                return generateMap(ProceduralMapKind::OrmFromNormal, normal,
                                   ri::render::vulkan::ProceduralNormalMapOptions{}, ormOptions);
            });
        return uploadGeneratedImage(key, orm, kDataTextureFormat);
    }

    // Looks for an authored sibling map next to the albedo using the texture-pack naming
    // convention ("<name>.png" -> "<name><suffix>.png", e.g. "_n" / "_s"). Results are
    // memoised because descriptorFor runs per draw. Returns an empty path when none exists.
    [[nodiscard]] fs::path probeSiblingMap(const fs::path& albedoPath, const std::string& suffix) {
        if (albedoPath.empty()) {
            return {};
        }
        const std::string cacheKey = albedoPath.generic_string() + "|" + suffix;
        if (const auto it = siblingMapByKey.find(cacheKey); it != siblingMapByKey.end()) {
            return it->second;
        }
        fs::path result{};
        const std::string stem = albedoPath.stem().string();
        // Do not stack suffixes on maps that are already normal/spec/roughness channels.
        const bool alreadySuffixed = stem.size() >= 2
                                     && (stem.ends_with("_n") || stem.ends_with("_s") || stem.ends_with("_r"));
        if (!alreadySuffixed) {
            static const std::array<const char*, 4> kMapExtensions{".png", ".tif", ".tiff", ".jpg"};
            for (const char* ext : kMapExtensions) {
                const fs::path sibling = albedoPath.parent_path() / (stem + suffix + ext);
                std::error_code ec{};
                if (fs::exists(sibling, ec) && !ec) {
                    result = sibling;
                    break;
                }
            }
            if (result.empty() && suffix == "_n") {
                static const std::array<const char*, 3> kNormalTokens{
                    "_Normal",
                    "_normal",
                    " Normal combined",
                };
                for (const char* token : kNormalTokens) {
                    for (const char* ext : kMapExtensions) {
                        const fs::path sibling = albedoPath.parent_path() / (stem + token + ext);
                        std::error_code ec{};
                        if (fs::exists(sibling, ec) && !ec) {
                            result = sibling;
                            break;
                        }
                    }
                    if (!result.empty()) {
                        break;
                    }
                }
            }
        }
        siblingMapByKey.emplace(cacheKey, result);
        return result;
    }

    // Returns a generated map for the given source, preferring the on-disk cache and
    // falling back to CPU generation (which is then written to the cache).
    template <typename GeneratorFn>
    [[nodiscard]] ri::render::software::RgbaImage generateOrLoadCached(const fs::path& albedoPath,
                                                                       const std::string& cacheKey,
                                                                       GeneratorFn&& generator) {
        const std::int64_t sourceStamp = procedural_cache::SourceModificationStamp(albedoPath);
        fs::path cacheFile{};
        if (!proceduralCacheDir.empty()) {
            cacheFile = procedural_cache::CacheFilePath(proceduralCacheDir, cacheKey);
            ri::render::software::RgbaImage cached = procedural_cache::Read(cacheFile, sourceStamp);
            if (cached.Valid()) {
                return cached;
            }
        }
        const std::shared_ptr<const ri::render::software::RgbaImage> albedo = decodedWarmupCache.Load(albedoPath);
        if (!albedo || !albedo->Valid()) {
            return {};
        }
        ri::render::software::RgbaImage generated = generator(*albedo);
        if (generated.Valid() && !cacheFile.empty()) {
            procedural_cache::Write(cacheFile, sourceStamp, generated);
        }
        return generated;
    }

    [[nodiscard]] VkDescriptorPool createAdditionalDescriptorPool() {
        if (device == VK_NULL_HANDLE || descriptorSetsPerPool == 0) {
            return VK_NULL_HANDLE;
        }
        const VkDescriptorPoolSize poolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = descriptorSetsPerPool * 6U,
        };
        const VkDescriptorPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = descriptorSetsPerPool,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize,
        };
        VkDescriptorPool pool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
            return VK_NULL_HANDLE;
        }
        textureDescriptorPools.push_back(pool);
        textureDescriptorPool = pool;
        ri::core::LogInfo(
            "Vulkan texture descriptors: grew to " + std::to_string(textureDescriptorPools.size())
            + " pools (" + std::to_string(textureDescriptorPools.size() * descriptorSetsPerPool)
            + " material sets capacity).");
        return pool;
    }

    [[nodiscard]] VkDescriptorSet allocateDescriptorSet() {
        VkDescriptorSet set = VK_NULL_HANDLE;
        VkDescriptorSetAllocateInfo allocateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = textureDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &textureSetLayout,
        };
        VkResult result = vkAllocateDescriptorSets(device, &allocateInfo, &set);
        if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
            allocateInfo.descriptorPool = createAdditionalDescriptorPool();
            if (allocateInfo.descriptorPool != VK_NULL_HANDLE) {
                result = vkAllocateDescriptorSets(device, &allocateInfo, &set);
            }
        }
        if (result != VK_SUCCESS) {
            if (!loggedDescriptorPoolFailure) {
                loggedDescriptorPoolFailure = true;
                ri::core::LogInfo(
                    "Vulkan texture descriptor allocation failed after automatic pool growth; "
                    "the affected material uses the white fallback. VkResult="
                    + std::to_string(static_cast<int>(result)) + ".");
            }
            return VK_NULL_HANDLE;
        }
        ++allocatedDescriptorSetCount;
        return set;
    }

    void writeMaterialDescriptorSet(VkDescriptorSet set,
                                    const GpuAlbedoImage& albedo,
                                    const GpuAlbedoImage& normal,
                                    const GpuAlbedoImage& orm,
                                    const GpuAlbedoImage& emissive,
                                    const GpuAlbedoImage& opacity,
                                    const GpuAlbedoImage& detail) const {
        const std::array<VkDescriptorImageInfo, 6> imageInfos = {{
            VkDescriptorImageInfo{.sampler = linearSampler, .imageView = albedo.view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            VkDescriptorImageInfo{.sampler = linearSampler, .imageView = normal.view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            VkDescriptorImageInfo{.sampler = linearSampler, .imageView = orm.view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            VkDescriptorImageInfo{.sampler = linearSampler, .imageView = emissive.view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            VkDescriptorImageInfo{.sampler = linearSampler, .imageView = opacity.view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            VkDescriptorImageInfo{.sampler = linearSampler, .imageView = detail.view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        }};
        std::array<VkWriteDescriptorSet, 6> writes{};
        for (std::uint32_t binding = 0; binding < writes.size(); ++binding) {
            writes[binding] = VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = set,
                .dstBinding = binding,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &imageInfos[binding],
            };
        }
        vkUpdateDescriptorSets(device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    [[nodiscard]] static fs::path resolveAuthoredTexturePath(const fs::path& textureRoot,
                                                             const std::string& authoredPath) {
        if (authoredPath.empty()) {
            return {};
        }
        const fs::path requested = fs::path(authoredPath).lexically_normal();
        if (requested.is_absolute()) {
            return requested;
        }

        // Most material paths are texture-root relative (for example "tile/foo.png").
        // Runtime helpers may instead author workspace-relative paths such as
        // "Assets/Packages/foo.png". Probe both contracts before preserving the legacy
        // candidate for useful missing-file diagnostics.
        const fs::path textureCandidate =
            !textureRoot.empty() ? (textureRoot / requested).lexically_normal() : fs::path{};
        std::error_code ec{};
        if (!textureCandidate.empty() && fs::is_regular_file(textureCandidate, ec) && !ec) {
            return textureCandidate;
        }
        ec.clear();
        if (fs::is_regular_file(requested, ec) && !ec) {
            fs::path absoluteRequested = fs::absolute(requested, ec);
            return !ec ? absoluteRequested.lexically_normal() : requested;
        }

        // Support processes launched outside the workspace by walking up from an absolute
        // texture root. The cap prevents malformed roots from causing unbounded probing.
        ec.clear();
        fs::path ancestor = fs::absolute(textureRoot, ec).lexically_normal();
        if (!ec) {
            for (int depth = 0; depth < 8 && !ancestor.empty(); ++depth) {
                const fs::path candidate = (ancestor / requested).lexically_normal();
                ec.clear();
                if (fs::is_regular_file(candidate, ec) && !ec) {
                    return candidate;
                }
                const fs::path parent = ancestor.parent_path();
                if (parent == ancestor) {
                    break;
                }
                ancestor = parent;
            }
        }
        return textureCandidate;
    }

    [[nodiscard]] VkDescriptorSet descriptorFor(const ri::scene::Scene& scene,
                                               const NativeSceneDraw& draw,
                                               const fs::path& textureRoot) {
        if (whiteDescriptorSet == VK_NULL_HANDLE) {
            return VK_NULL_HANDLE;
        }
        if (!draw.useTexture || draw.materialHandle < 0) {
            return whiteDescriptorSet;
        }
        const ri::scene::Material& material = scene.GetMaterial(draw.materialHandle);
        const std::string albedoRel =
            !draw.resolvedAlbedoRelPath.empty() ? draw.resolvedAlbedoRelPath : ResolveMaterialTextureRelPath(material);
        const auto makeAbsolute = [&textureRoot](const std::string& relPath) -> fs::path {
            return resolveAuthoredTexturePath(textureRoot, relPath);
        };
        const fs::path albedoPath = makeAbsolute(albedoRel);
        const fs::path authoredNormalPath = makeAbsolute(material.normalTexture);
        const fs::path ormPath = makeAbsolute(material.ormTexture);
        const fs::path emissivePath = makeAbsolute(material.emissiveTexture);
        const fs::path opacityPath = makeAbsolute(material.opacityTexture);
        const fs::path detailPath = makeAbsolute(material.detailTexture.empty() ? albedoRel : material.detailTexture);

        const bool metalLookupMaterial = (draw.materialStyleFlags & kNativeMaterialStyleMetalLookup) != 0;
        const bool specGlossWorkflow = material.materialWorkflow == ri::scene::MaterialWorkflow::SpecGloss;

        // Cross-reference authored sibling maps from the texture pack ("<name>_n.png" next
        // to "<name>.png") before considering procedural generation. This lets a material
        // that only names an albedo automatically pick up the pack's real normal map.
        const bool normalEligible = kNativeGenerateMissingMaterialMaps && draw.litShadingModel
                                     && !metalLookupMaterial;
        fs::path normalPath = authoredNormalPath;
        if (normalPath.empty() && normalEligible && !albedoPath.empty()) {
            normalPath = probeSiblingMap(albedoPath, "_n");
        }

        const bool wantsGeneratedNormal = normalEligible && normalPath.empty() && !albedoPath.empty();
        // Generated ORM is MetalRough-packed; SpecGloss materials would misread it as
        // a spec colour, so they keep the safe authored-scalar shader path instead.
        const bool wantsGeneratedOrm = kNativeGenerateMissingMaterialMaps && draw.litShadingModel
                                       && !metalLookupMaterial && !specGlossWorkflow
                                       && material.ormTexture.empty() && !albedoPath.empty();
        // When a normal map is available (authored or auto-discovered) we generate the ORM
        // from it (curvature-based occlusion) for higher quality than luminance heuristics.
        const bool ormFromNormal = wantsGeneratedOrm && !normalPath.empty();
        const ProceduralNormalMapOptions proceduralNormalOptions =
            BuildProceduralNormalOptions(material, draw.materialStyleFlags);
        const ProceduralOrmMapOptions proceduralOrmOptions =
            BuildProceduralOrmOptions(material, draw.materialStyleFlags);

        const std::string key =
            std::string("mat|a=") + albedoPath.generic_string()
            + "|n=" + normalPath.generic_string()
            + "|o=" + ormPath.generic_string()
            + "|e=" + emissivePath.generic_string()
            + "|p=" + opacityPath.generic_string()
            + "|d=" + detailPath.generic_string()
            + "|gn=" + (wantsGeneratedNormal ? std::to_string(proceduralNormalOptions.strength) : "0")
            + "|go=" + (wantsGeneratedOrm
                            ? (std::to_string(proceduralOrmOptions.baseRoughness) + ","
                               + std::to_string(proceduralOrmOptions.baseMetallic) + ","
                               + std::to_string(proceduralOrmOptions.aoStrength))
                            : "0")
            + "|gox=" + (ormFromNormal ? "1" : "0");
        if (const auto it = descriptorByKey.find(key); it != descriptorByKey.end()) {
            return it->second;
        }

        VkDescriptorSet set = allocateDescriptorSet();
        if (set == VK_NULL_HANDLE) {
            // Cache the fallback so an exhausted pool is not retried for this key every frame.
            descriptorByKey.emplace(key, whiteDescriptorSet);
            return whiteDescriptorSet;
        }
        const GpuAlbedoImage* albedoLoaded = resolveImageForAbsolutePath(albedoPath, kColorTextureFormat);
        const GpuAlbedoImage* normalLoaded = resolveImageForAbsolutePath(normalPath, kDataTextureFormat);
        const GpuAlbedoImage* ormLoaded = resolveImageForAbsolutePath(ormPath, kDataTextureFormat);
        const GpuAlbedoImage* emissiveLoaded = resolveImageForAbsolutePath(emissivePath, kColorTextureFormat);
        const GpuAlbedoImage* opacityLoaded = resolveImageForAbsolutePath(opacityPath, kDataTextureFormat);
        const GpuAlbedoImage* detailLoaded = resolveImageForAbsolutePath(detailPath, kColorTextureFormat);
        if (normalLoaded == nullptr && wantsGeneratedNormal) {
            normalLoaded = resolveGeneratedNormal(albedoPath, proceduralNormalOptions);
        }
        if (ormLoaded == nullptr && wantsGeneratedOrm) {
            // Prefer cross-referencing the resolved normal map; fall back to albedo cavity.
            if (!normalPath.empty()) {
                ormLoaded = resolveGeneratedOrmFromNormal(normalPath, proceduralOrmOptions);
            }
            if (ormLoaded == nullptr) {
                ormLoaded = resolveGeneratedOrm(albedoPath, proceduralOrmOptions);
            }
        }
        const GpuAlbedoImage& albedoImage = albedoLoaded != nullptr ? *albedoLoaded : ResolveFallbackWhite();
        const GpuAlbedoImage& normalImage = normalLoaded != nullptr ? *normalLoaded : ResolveFallbackFlatNormal();
        const GpuAlbedoImage& ormImage = ormLoaded != nullptr ? *ormLoaded : ResolveFallbackOrm();
        const GpuAlbedoImage& emissiveImage = emissiveLoaded != nullptr ? *emissiveLoaded : ResolveFallbackBlack();
        const GpuAlbedoImage& opacityImage = opacityLoaded != nullptr ? *opacityLoaded : ResolveFallbackWhite();
        const GpuAlbedoImage& detailImage = detailLoaded != nullptr ? *detailLoaded : ResolveFallbackWhite();
        writeMaterialDescriptorSet(set, albedoImage, normalImage, ormImage, emissiveImage, opacityImage, detailImage);
        descriptorByKey.emplace(key, set);
        return set;
    }

    void warmForScene(const ri::scene::Scene& scene,
                      const std::vector<NativeSceneDraw>& draws,
                      const fs::path& textureRoot,
                      const VulkanWarmupCacheOptions& warmupOptions) {
        std::vector<fs::path> decodePaths{};
        std::unordered_set<std::string> decodeKeys{};
        const auto makeAbsolute = [&textureRoot](const std::string& relPath) -> fs::path {
            return resolveAuthoredTexturePath(textureRoot, relPath);
        };
        const auto addDecodePath = [&decodePaths, &decodeKeys](const fs::path& path) {
            if (path.empty() || path.filename() == "-" || path.filename() == "0"
                || path.filename() == "0.0") {
                return;
            }
            fs::path resolvedPath = path;
            std::error_code existsEc{};
            if (!fs::exists(resolvedPath, existsEc) || existsEc) {
                static const std::array<const char*, 4> kAlternateExtensions{".png", ".tif", ".tiff", ".jpg"};
                const std::string stem = path.stem().string();
                for (const char* extension : kAlternateExtensions) {
                    fs::path alternate = path.parent_path() / (stem + extension);
                    existsEc.clear();
                    if (fs::exists(alternate, existsEc) && !existsEc) {
                        resolvedPath = std::move(alternate);
                        break;
                    }
                }
            }
            const std::string key = resolvedPath.lexically_normal().generic_string();
            if (decodeKeys.emplace(key).second) {
                decodePaths.push_back(std::move(resolvedPath));
            }
        };
        for (const NativeSceneDraw& draw : draws) {
            if (!draw.useTexture || !draw.litShadingModel || draw.materialHandle < 0) {
                continue;
            }
            const ri::scene::Material& material = scene.GetMaterial(draw.materialHandle);
            const std::string albedoRel = !draw.resolvedAlbedoRelPath.empty()
                ? draw.resolvedAlbedoRelPath
                : ResolveMaterialTextureRelPath(material);
            const fs::path albedoPath = makeAbsolute(albedoRel);
            fs::path normalPath = makeAbsolute(material.normalTexture);
            if (normalPath.empty() && !albedoPath.empty()) {
                normalPath = probeSiblingMap(albedoPath, "_n");
            }
            addDecodePath(albedoPath);
            addDecodePath(normalPath);
            addDecodePath(makeAbsolute(material.ormTexture));
            addDecodePath(makeAbsolute(material.emissiveTexture));
            addDecodePath(makeAbsolute(material.opacityTexture));
            addDecodePath(makeAbsolute(material.detailTexture.empty() ? albedoRel : material.detailTexture));
        }

        const VulkanWarmupCacheStats warmupStats = decodedWarmupCache.Preload(decodePaths, warmupOptions);
        if (warmupStats.uniquePaths > 0U) {
            ri::core::LogInfo(
                "Vulkan burst warmup: textures=" + std::to_string(warmupStats.decodedPaths) + "/"
                + std::to_string(warmupStats.uniquePaths) + " workers=" + std::to_string(warmupStats.workerThreads)
                + " retainedMiB=" + std::to_string(warmupStats.retainedBytes / (1024ULL * 1024ULL))
                + " elapsedMs=" + std::to_string(warmupStats.elapsedMilliseconds)
                + (warmupStats.failedPaths > 0U ? " failed=" + std::to_string(warmupStats.failedPaths) : "")
                + (!warmupStats.failedPathSamples.empty()
                       ? " firstMissing=" + warmupStats.failedPathSamples.front().generic_string()
                       : "")
                + (warmupStats.budgetSkippedPaths > 0U
                       ? " budgetSkipped=" + std::to_string(warmupStats.budgetSkippedPaths)
                       : ""));
        }
        for (const NativeSceneDraw& draw : draws) {
            if (draw.useTexture && draw.litShadingModel) {
                (void)descriptorFor(scene, draw, textureRoot);
            }
        }
        decodedWarmupCache.Clear();
    }

    [[nodiscard]] VkDescriptorSet descriptorForAbsolutePath(const fs::path& absolutePath, const bool srgb = true) {
        if (whiteDescriptorSet == VK_NULL_HANDLE) {
            return VK_NULL_HANDLE;
        }
        if (absolutePath.empty()) {
            return whiteDescriptorSet;
        }

        const VkFormat format = srgb ? kColorTextureFormat : kDataTextureFormat;
        const std::string key = std::string("__abs__|") + absolutePath.generic_string()
            + "|fmt=" + std::to_string(static_cast<int>(format));
        if (const auto it = descriptorByKey.find(key); it != descriptorByKey.end()) {
            return it->second;
        }

        const GpuAlbedoImage* loaded = resolveImageForAbsolutePath(absolutePath, format);
        if (loaded == nullptr) {
            descriptorByKey.emplace(key, whiteDescriptorSet);
            return whiteDescriptorSet;
        }
        VkDescriptorSet set = allocateDescriptorSet();
        if (set == VK_NULL_HANDLE) {
            descriptorByKey.emplace(key, whiteDescriptorSet);
            return whiteDescriptorSet;
        }
        writeMaterialDescriptorSet(
            set, *loaded, ResolveFallbackFlatNormal(), ResolveFallbackOrm(), ResolveFallbackBlack(), ResolveFallbackWhite(), ResolveFallbackWhite());
        descriptorByKey.emplace(key, set);
        return set;
    }

    // Native post-processing effects share the six-binding material texture layout so the
    // renderer does not need another descriptor pool/layout family. Unlike
    // descriptorForAbsolutePath(), each binding is intentionally distinct and decoded as
    // linear data: noise, permutation, LUT, mask, and font texels must never receive an sRGB transform.
    // Binding 3 is lens dirt and intentionally uses the color texture format.
    [[nodiscard]] VkDescriptorSet descriptorForNativePostBundle(
        const std::array<fs::path, 6>& absolutePaths) {
        if (whiteDescriptorSet == VK_NULL_HANDLE) {
            return VK_NULL_HANDLE;
        }

        std::string key = "__native_post_bundle__";
        for (const fs::path& path : absolutePaths) {
            key += '|';
            key += path.generic_string();
        }
        if (const auto it = descriptorByKey.find(key); it != descriptorByKey.end()) {
            return it->second;
        }

        std::array<const GpuAlbedoImage*, 6> loaded{};
        for (std::size_t index = 0; index < absolutePaths.size(); ++index) {
            if (!absolutePaths[index].empty()) {
                loaded[index] = resolveImageForAbsolutePath(
                    absolutePaths[index], index == 3 ? kColorTextureFormat : kDataTextureFormat);
            }
        }
        const GpuAlbedoImage& fallback = ResolveFallbackWhite();
        VkDescriptorSet set = allocateDescriptorSet();
        if (set == VK_NULL_HANDLE) {
            descriptorByKey.emplace(key, whiteDescriptorSet);
            return whiteDescriptorSet;
        }
        writeMaterialDescriptorSet(set,
                                   loaded[0] != nullptr ? *loaded[0] : fallback,
                                   loaded[1] != nullptr ? *loaded[1] : fallback,
                                   loaded[2] != nullptr ? *loaded[2] : fallback,
                                   loaded[3] != nullptr ? *loaded[3] : fallback,
                                   loaded[4] != nullptr ? *loaded[4] : fallback,
                                   loaded[5] != nullptr ? *loaded[5] : fallback);
        descriptorByKey.emplace(std::move(key), set);
        return set;
    }

    void initialize(VkPhysicalDevice physDevice,
                    VkDevice dev,
                    VkCommandPool pool,
                    VkQueue queue,
                    std::uint32_t queueFamily,
                    VkDescriptorSetLayout textureLayout,
                    VkDescriptorPool texturePool,
                    std::uint32_t setsPerPool,
                    VkSampler sampler) {
        physicalDevice = physDevice;
        device = dev;
        commandPool = pool;
        graphicsQueue = queue;
        textureSetLayout = textureLayout;
        textureDescriptorPool = texturePool;
        textureDescriptorPools.push_back(texturePool);
        descriptorSetsPerPool = setsPerPool;
        linearSampler = sampler;

        if (kNativeGenerateMissingMaterialMaps) {
            gpuGenerator.initialize(physDevice, dev, queue, queueFamily);
            ri::core::LogInfo(gpuGenerator.isAvailable()
                                  ? "Procedural material maps: GPU compute generator active (CPU fallback ready)."
                                  : "Procedural material maps: using CPU generator (GPU compute unavailable).");
        }

        if (kNativeGenerateMissingMaterialMaps) {
            std::error_code cacheEc{};
            fs::path candidate = fs::current_path(cacheEc) / "Saved" / "Cache" / "ProceduralMaps";
            if (!cacheEc) {
                fs::create_directories(candidate, cacheEc);
                if (!cacheEc) {
                    proceduralCacheDir = std::move(candidate);
                }
            }
        }

        constexpr std::uint8_t whitePixel[4] = {255, 255, 255, 255};
        constexpr std::uint8_t blackPixel[4] = {0, 0, 0, 255};
        constexpr std::uint8_t flatNormalPixel[4] = {128, 128, 255, 255};
        constexpr std::uint8_t ormDefaultPixel[4] = {255, 255, 0, 255};
        whiteImage = CreateGpuRgba8Image(
            physicalDevice, device, commandPool, graphicsQueue, 1, 1, whitePixel, kColorTextureFormat);
        blackImage = CreateGpuRgba8Image(
            physicalDevice, device, commandPool, graphicsQueue, 1, 1, blackPixel, kColorTextureFormat);
        // Data fallbacks MUST be UNORM so the "neutral" normal/ORM are read verbatim.
        flatNormalImage = CreateGpuRgba8Image(
            physicalDevice, device, commandPool, graphicsQueue, 1, 1, flatNormalPixel, kDataTextureFormat);
        ormDefaultImage = CreateGpuRgba8Image(
            physicalDevice, device, commandPool, graphicsQueue, 1, 1, ormDefaultPixel, kDataTextureFormat);
        if (whiteImage.view == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to create fallback white albedo texture.");
        }
        if (blackImage.view == VK_NULL_HANDLE || flatNormalImage.view == VK_NULL_HANDLE || ormDefaultImage.view == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to create fallback material support textures.");
        }

        whiteDescriptorSet = allocateDescriptorSet();
        ExpectVk(whiteDescriptorSet != VK_NULL_HANDLE ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED,
                 "vkAllocateDescriptorSets(white-material)");
        writeMaterialDescriptorSet(
            whiteDescriptorSet, whiteImage, flatNormalImage, ormDefaultImage, blackImage, whiteImage, whiteImage);
    }
};

void RecordHybridCompositeInCommandBuffer(VkCommandBuffer commandBuffer,
                                          VkRenderPass compositeRenderPass,
                                          VkFramebuffer compositeFramebuffer,
                                          VkExtent2D extent,
                                          VkPipeline compositePipeline,
                                          VkPipelineLayout compositePipelineLayout,
                                          VkDescriptorSet cameraDescriptorSet,
                                          VkDescriptorSet hdrTextureDescriptorSet,
                                          VkDescriptorSet layerTextureDescriptorSet,
                                          VkDescriptorSet smaaAreaTextureDescriptorSet,
                                          VkDescriptorSet smaaSearchTextureDescriptorSet,
                                          VkDescriptorSet lutTextureDescriptorSet,
                                          VkDescriptorSet barbatosLutTextureDescriptorSet,
                                          VkDescriptorSet nativePostTextureDescriptorSet,
                                          VkImage swapchainImage,
                                          VkImage fakeMotionBlurHistoryImage,
                                          float fakeMotionBlurRecall);

void RecordHybridScreenSpaceBundle(VkCommandBuffer commandBuffer,
                                   VkRenderPass bundleRenderPass,
                                   VkFramebuffer bundleFramebuffer,
                                   VkExtent2D extent,
                                   VkPipeline bundlePipeline,
                                   VkPipelineLayout bundlePipelineLayout,
                                   VkDescriptorSet cameraDescriptorSet,
                                   VkDescriptorSet bundleTextureDescriptorSet);

void RecordSceneCommandBuffer(VkCommandBuffer commandBuffer,
                              VkRenderPass shadowRenderPass,
                              VkFramebuffer shadowFramebuffer,
                              VkExtent2D shadowExtent,
                              VkPipeline shadowPipeline,
                              VkPipelineLayout shadowPipelineLayout,
                              VkFramebuffer framebuffer,
                              VkRenderPass renderPass,
                              VkExtent2D extent,
                              VkPipeline skyPipeline,
                              VkPipelineLayout skyPipelineLayout,
                              VkDescriptorSet skyDescriptorSet,
                              VkDescriptorSet skyTextureDescriptorSet,
                              const CachedGpuMesh& skyMesh,
                              VkPipeline scenePipeline,
                              VkPipeline scenePipelineTransparent,
                              VkPipeline scenePipelineAdditive,
                              VkPipelineLayout pipelineLayout,
                              VkDescriptorSet cameraDescriptorSet,
                              VkDescriptorSet shadowDescriptorSet,
                              NativeAlbedoTextureCache& textureCache,
                              const ri::scene::Scene& scene,
                              const std::unordered_map<int, CachedGpuMesh>& meshCache,
                              const NativeScenePreviewData& sceneData,
                              VkFramebuffer hybridCompositeFramebuffer,
                              VkRenderPass hybridCompositeRenderPass,
                              VkPipeline hybridCompositePipeline,
                              VkPipelineLayout hybridCompositePipelineLayout,
                              VkDescriptorSet hybridCompositeHdrDescriptorSet,
                              VkDescriptorSet hybridCompositeLayerDescriptorSet,
                              VkDescriptorSet hybridCompositeSmaaAreaDescriptorSet,
                              VkDescriptorSet hybridCompositeSmaaSearchDescriptorSet,
                              VkDescriptorSet hybridCompositeLutDescriptorSet,
                              VkDescriptorSet hybridCompositeBarbatosLutDescriptorSet,
                              VkDescriptorSet hybridCompositeNativePostDescriptorSet,
                              VkFramebuffer hybridBundleFramebuffer,
                              VkRenderPass hybridBundleRenderPass,
                              VkPipeline hybridBundlePipeline,
                              VkPipelineLayout hybridBundlePipelineLayout,
                              VkDescriptorSet hybridBundleDescriptorSet,
                              VkImage swapchainImage,
                              VkImage fakeMotionBlurHistoryImage,
                              float fakeMotionBlurRecall) {
    const VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    ExpectVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer");

    const std::array<VkClearValue, 4> clearValues = {{
        VkClearValue{.color = {
            sceneData.clearColor[0],
            sceneData.clearColor[1],
            sceneData.clearColor[2],
            sceneData.clearColor[3],
        }},
        VkClearValue{.color = {{0.5f, 0.5f, 1.0f, 1.0f}}},
        VkClearValue{.color = {{0.0f, 1.0f, 0.0f, 0.0f}}},
        VkClearValue{.depthStencil = {1.0f, 0}},
    }};
    const VkRenderPassBeginInfo renderPassBeginInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = {0, 0},
            .extent = extent,
        },
        .clearValueCount = static_cast<std::uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data(),
    };
    const VkViewport viewport{
        .x = 0.0f,
        .y = static_cast<float>(extent.height),
        .width = static_cast<float>(extent.width),
        .height = -static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    const VkRect2D scissor{
        .offset = {0, 0},
        .extent = extent,
    };

    static thread_local std::vector<std::size_t> sortedDrawIndices;
    sortedDrawIndices.resize(sceneData.draws.size());
    std::iota(sortedDrawIndices.begin(), sortedDrawIndices.end(), 0U);
    std::stable_sort(sortedDrawIndices.begin(),
                     sortedDrawIndices.end(),
                     [&sceneData](const std::size_t leftIndex, const std::size_t rightIndex) {
                         const NativeSceneDraw& left = sceneData.draws[leftIndex];
                         const NativeSceneDraw& right = sceneData.draws[rightIndex];
                         if (left.transparent && right.transparent && left.sortDepthSq != right.sortDepthSq) {
                             return left.sortDepthSq > right.sortDepthSq;
                         }
                         if (left.transparent != right.transparent) {
                             return left.transparent < right.transparent;
                         }
                         if (left.additiveBlend != right.additiveBlend) {
                             return left.additiveBlend < right.additiveBlend;
                         }
                         if (left.meshHandle != right.meshHandle) {
                             return left.meshHandle < right.meshHandle;
                         }
                         if (left.materialHandle != right.materialHandle) {
                             return left.materialHandle < right.materialHandle;
                         }
                         return left.resolvedAlbedoRelPath < right.resolvedAlbedoRelPath;
                     });

    if (shadowRenderPass != VK_NULL_HANDLE && shadowFramebuffer != VK_NULL_HANDLE
        && shadowPipeline != VK_NULL_HANDLE && shadowPipelineLayout != VK_NULL_HANDLE) {
        const VkClearValue shadowClearValue{
            .depthStencil = {1.0f, 0},
        };
        const VkRenderPassBeginInfo shadowBeginInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = shadowRenderPass,
            .framebuffer = shadowFramebuffer,
            .renderArea = {
                .offset = {0, 0},
                .extent = shadowExtent,
            },
            .clearValueCount = 1,
            .pClearValues = &shadowClearValue,
        };
        const VkViewport shadowViewport{
            .x = 0.0f,
            .y = static_cast<float>(shadowExtent.height),
            .width = static_cast<float>(shadowExtent.width),
            .height = -static_cast<float>(shadowExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        const VkRect2D shadowScissor{
            .offset = {0, 0},
            .extent = shadowExtent,
        };
        vkCmdBeginRenderPass(commandBuffer, &shadowBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);
        vkCmdSetViewport(commandBuffer, 0, 1, &shadowViewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &shadowScissor);
        vkCmdSetDepthBias(commandBuffer, 0.12f, 0.0f, 0.45f);
        vkCmdBindDescriptorSets(commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                shadowPipelineLayout,
                                0,
                                1,
                                &cameraDescriptorSet,
                                0,
                                nullptr);
        for (const std::size_t drawIndex : sortedDrawIndices) {
            const NativeSceneDraw& draw = sceneData.draws[drawIndex];
            if (draw.transparent || draw.additiveBlend) {
                continue;
            }
            const auto meshIt = meshCache.find(draw.meshHandle);
            if (meshIt == meshCache.end()) {
                continue;
            }
            const CachedGpuMesh& mesh = meshIt->second;
            const VkDeviceSize vertexOffset = 0;
            const std::uint32_t firstIndex = std::min(draw.firstIndex, mesh.indexCount);
            const std::uint32_t availableIndexCount = mesh.indexCount - firstIndex;
            const std::uint32_t indexCount =
                draw.indexCount == 0 ? availableIndexCount : std::min(draw.indexCount, availableIndexCount);
            if (indexCount == 0) {
                continue;
            }
            NativeDrawPushConstants pushConstants{};
            std::copy(draw.model.begin(), draw.model.end(), std::begin(pushConstants.model));
            std::copy(draw.color.begin(), draw.color.end(), std::begin(pushConstants.color));
            pushConstants.tiling[0] = draw.textureTiling[0];
            pushConstants.tiling[1] = draw.textureTiling[1];
            pushConstants.useTexture = draw.useTexture ? 1 : 0;
            pushConstants.litShadingModel = (draw.litShadingModel ? 1 : 0) | (draw.alphaCutout ? 2 : 0);
            if (draw.doubleSided) {
                pushConstants.litShadingModel |= 8;
            }
            if (draw.alphaCutout && draw.doubleSided) {
                pushConstants.litShadingModel |= 16;
            }
            pushConstants.litShadingModel |= draw.materialStyleFlags;
            pushConstants.alphaCutoff = draw.alphaCutoff;
            if (draw.alphaCutout && draw.useTexture) {
                VkDescriptorSet textureSet = textureCache.descriptorFor(scene, draw, sceneData.textureRoot);
                if (textureSet == VK_NULL_HANDLE) {
                    textureSet = textureCache.whiteDescriptorSet;
                }
                vkCmdBindDescriptorSets(commandBuffer,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        shadowPipelineLayout,
                                        1,
                                        1,
                                        &textureSet,
                                        0,
                                        nullptr);
            }
            vkCmdPushConstants(commandBuffer,
                               shadowPipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(NativeDrawPushConstants),
                               &pushConstants);
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mesh.vertexBuffer.buffer, &vertexOffset);
            vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, indexCount, std::max(draw.instanceCount, 1U), firstIndex, 0, 0);
        }
        vkCmdEndRenderPass(commandBuffer);
    }

    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    if (skyPipeline != VK_NULL_HANDLE && skyPipelineLayout != VK_NULL_HANDLE && skyDescriptorSet != VK_NULL_HANDLE
        && skyTextureDescriptorSet != VK_NULL_HANDLE && skyMesh.indexCount > 0U) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        const std::array<VkDescriptorSet, 2> skySets = {skyDescriptorSet, skyTextureDescriptorSet};
        vkCmdBindDescriptorSets(commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                skyPipelineLayout,
                                0,
                                static_cast<std::uint32_t>(skySets.size()),
                                skySets.data(),
                                0,
                                nullptr);
        const VkDeviceSize skyVertexOffset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &skyMesh.vertexBuffer.buffer, &skyVertexOffset);
        vkCmdBindIndexBuffer(commandBuffer, skyMesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, skyMesh.indexCount, 1, 0, 0, 0);
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipeline);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout,
                            0,
                            1,
                            &cameraDescriptorSet,
                            0,
                            nullptr);
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout,
                            2,
                            1,
                            &shadowDescriptorSet,
                            0,
                            nullptr);
    enum class ScenePipelineMode {
        Opaque,
        Transparent,
        Additive,
    };
    ScenePipelineMode scenePipelineMode = ScenePipelineMode::Opaque;
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipeline);
    VkDescriptorSet lastBoundTextureSet = VK_NULL_HANDLE;
    for (const std::size_t drawIndex : sortedDrawIndices) {
        const NativeSceneDraw& draw = sceneData.draws[drawIndex];
        const auto meshIt = meshCache.find(draw.meshHandle);
        if (meshIt == meshCache.end()) {
            continue;
        }
        const CachedGpuMesh& mesh = meshIt->second;
        const VkDeviceSize vertexOffset = 0;
        const std::uint32_t firstIndex = std::min(draw.firstIndex, mesh.indexCount);
        const std::uint32_t availableIndexCount = mesh.indexCount - firstIndex;
        const std::uint32_t indexCount = draw.indexCount == 0 ? availableIndexCount : std::min(draw.indexCount, availableIndexCount);
        if (indexCount == 0) {
            continue;
        }

        const ScenePipelineMode neededPipelineMode = draw.additiveBlend
            ? ScenePipelineMode::Additive
            : (draw.transparent ? ScenePipelineMode::Transparent : ScenePipelineMode::Opaque);
        if (neededPipelineMode != scenePipelineMode) {
            VkPipeline boundPipeline = scenePipeline;
            if (neededPipelineMode == ScenePipelineMode::Transparent && scenePipelineTransparent != VK_NULL_HANDLE) {
                boundPipeline = scenePipelineTransparent;
            } else if (neededPipelineMode == ScenePipelineMode::Additive && scenePipelineAdditive != VK_NULL_HANDLE) {
                boundPipeline = scenePipelineAdditive;
            }
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, boundPipeline);
            scenePipelineMode = neededPipelineMode;
        }

        VkDescriptorSet textureSet = textureCache.descriptorFor(scene, draw, sceneData.textureRoot);
        if (textureSet == VK_NULL_HANDLE) {
            textureSet = textureCache.whiteDescriptorSet;
        }
        if (textureSet != lastBoundTextureSet) {
            vkCmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout,
                                    1,
                                    1,
                                    &textureSet,
                                    0,
                                    nullptr);
            lastBoundTextureSet = textureSet;
        }

        NativeDrawPushConstants pushConstants{};
        std::copy(draw.model.begin(), draw.model.end(), std::begin(pushConstants.model));
        std::copy(draw.color.begin(), draw.color.end(), std::begin(pushConstants.color));
        pushConstants.tiling[0] = draw.textureTiling[0];
        pushConstants.tiling[1] = draw.textureTiling[1];
        pushConstants.useTexture = draw.useTexture ? 1 : 0;
        pushConstants.nativeWaterUvMotion = draw.nativeWaterUvMotion ? 1 : 0;
        pushConstants.nativeWaterTime = sceneData.sceneAnimationTimeSeconds;
        pushConstants.litShadingModel = (draw.litShadingModel ? 1 : 0) | (draw.alphaCutout ? 2 : 0);
        if (draw.additiveBlend) {
            pushConstants.litShadingModel |= 4;
        }
        if (draw.doubleSided) {
            pushConstants.litShadingModel |= 8;
        }
        if (draw.alphaCutout && draw.doubleSided) {
            pushConstants.litShadingModel |= 16;
        }
        pushConstants.litShadingModel |= draw.materialStyleFlags;
        pushConstants.metallic = draw.metallic;
        pushConstants.roughness = draw.roughness;
        pushConstants.emissiveColor[0] = draw.emissiveColor[0];
        pushConstants.emissiveColor[1] = draw.emissiveColor[1];
        pushConstants.emissiveColor[2] = draw.emissiveColor[2];
        pushConstants.qualityTier = static_cast<float>(sceneData.renderQualityTier);
        pushConstants.alphaCutoff = draw.alphaCutoff;
        vkCmdPushConstants(commandBuffer,
                           pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(NativeDrawPushConstants),
                           &pushConstants);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mesh.vertexBuffer.buffer, &vertexOffset);
        vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, indexCount, std::max(draw.instanceCount, 1U), firstIndex, 0, 0);
    }
    vkCmdEndRenderPass(commandBuffer);
    RecordHybridScreenSpaceBundle(commandBuffer,
                                  hybridBundleRenderPass,
                                  hybridBundleFramebuffer,
                                  extent,
                                  hybridBundlePipeline,
                                  hybridBundlePipelineLayout,
                                  cameraDescriptorSet,
                                  hybridBundleDescriptorSet);
    RecordHybridCompositeInCommandBuffer(commandBuffer,
                                         hybridCompositeRenderPass,
                                         hybridCompositeFramebuffer,
                                         extent,
                                         hybridCompositePipeline,
                                         hybridCompositePipelineLayout,
                                         cameraDescriptorSet,
                                         hybridCompositeHdrDescriptorSet,
                                         hybridCompositeLayerDescriptorSet,
                                         hybridCompositeSmaaAreaDescriptorSet,
                                         hybridCompositeSmaaSearchDescriptorSet,
                                         hybridCompositeLutDescriptorSet,
                                         hybridCompositeBarbatosLutDescriptorSet,
                                         hybridCompositeNativePostDescriptorSet,
                                         swapchainImage,
                                         fakeMotionBlurHistoryImage,
                                         fakeMotionBlurRecall);
    ExpectVk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
}

void RecordHybridCompositeInCommandBuffer(VkCommandBuffer commandBuffer,
                                          VkRenderPass compositeRenderPass,
                                          VkFramebuffer compositeFramebuffer,
                                          VkExtent2D extent,
                                          VkPipeline compositePipeline,
                                          VkPipelineLayout compositePipelineLayout,
                                          VkDescriptorSet cameraDescriptorSet,
                                          VkDescriptorSet hdrTextureDescriptorSet,
                                          VkDescriptorSet layerTextureDescriptorSet,
                                          VkDescriptorSet smaaAreaTextureDescriptorSet,
                                          VkDescriptorSet smaaSearchTextureDescriptorSet,
                                          VkDescriptorSet lutTextureDescriptorSet,
                                          VkDescriptorSet barbatosLutTextureDescriptorSet,
                                          VkDescriptorSet nativePostTextureDescriptorSet,
                                          VkImage swapchainImage,
                                          VkImage fakeMotionBlurHistoryImage,
                                          float fakeMotionBlurRecall) {
    if (compositeFramebuffer == VK_NULL_HANDLE || compositeRenderPass == VK_NULL_HANDLE
        || compositePipeline == VK_NULL_HANDLE || compositePipelineLayout == VK_NULL_HANDLE
        || hdrTextureDescriptorSet == VK_NULL_HANDLE || layerTextureDescriptorSet == VK_NULL_HANDLE
        || smaaAreaTextureDescriptorSet == VK_NULL_HANDLE || smaaSearchTextureDescriptorSet == VK_NULL_HANDLE
        || lutTextureDescriptorSet == VK_NULL_HANDLE || barbatosLutTextureDescriptorSet == VK_NULL_HANDLE
        || nativePostTextureDescriptorSet == VK_NULL_HANDLE) {
        return;
    }
    const VkClearValue compositeClear{};
    const VkRenderPassBeginInfo compositeBegin{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = compositeRenderPass,
        .framebuffer = compositeFramebuffer,
        .renderArea = {
            .offset = {0, 0},
            .extent = extent,
        },
        .clearValueCount = 1,
        .pClearValues = &compositeClear,
    };
    const VkViewport compositeViewport{
        .x = 0.0f,
        .y = static_cast<float>(extent.height),
        .width = static_cast<float>(extent.width),
        .height = -static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    const VkRect2D compositeScissor{
        .offset = {0, 0},
        .extent = extent,
    };
    vkCmdBeginRenderPass(commandBuffer, &compositeBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline);
    vkCmdSetViewport(commandBuffer, 0, 1, &compositeViewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &compositeScissor);
    const std::array<VkDescriptorSet, 8> compositeSets = {
        cameraDescriptorSet,
        hdrTextureDescriptorSet,
        layerTextureDescriptorSet,
        smaaAreaTextureDescriptorSet,
        smaaSearchTextureDescriptorSet,
        lutTextureDescriptorSet,
        barbatosLutTextureDescriptorSet,
        nativePostTextureDescriptorSet};
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            compositePipelineLayout,
                            0,
                            static_cast<std::uint32_t>(compositeSets.size()),
                            compositeSets.data(),
                            0,
                            nullptr);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);

    if (fakeMotionBlurRecall > 1e-6f && swapchainImage != VK_NULL_HANDLE && fakeMotionBlurHistoryImage != VK_NULL_HANDLE) {
        RecordFakeMotionBlurHistoryCopy(commandBuffer, swapchainImage, fakeMotionBlurHistoryImage, extent);
    }
}

void RecordHybridScreenSpaceBundle(VkCommandBuffer commandBuffer,
                                   VkRenderPass bundleRenderPass,
                                   VkFramebuffer bundleFramebuffer,
                                   VkExtent2D extent,
                                   VkPipeline bundlePipeline,
                                   VkPipelineLayout bundlePipelineLayout,
                                   VkDescriptorSet cameraDescriptorSet,
                                   VkDescriptorSet bundleTextureDescriptorSet) {
    if (bundleFramebuffer == VK_NULL_HANDLE || bundleRenderPass == VK_NULL_HANDLE || bundlePipeline == VK_NULL_HANDLE
        || bundlePipelineLayout == VK_NULL_HANDLE || bundleTextureDescriptorSet == VK_NULL_HANDLE) {
        return;
    }
    const VkMemoryBarrier forwardToDeferredBundleBarrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    };
    vkCmdPipelineBarrier(commandBuffer,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           0,
                           1,
                           &forwardToDeferredBundleBarrier,
                           0,
                           nullptr,
                           0,
                           nullptr);
    const VkClearValue bundleClear{
        .color = {0.0f, 0.0f, 0.0f, 0.0f},
    };
    const VkRenderPassBeginInfo bundleBegin{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = bundleRenderPass,
        .framebuffer = bundleFramebuffer,
        .renderArea = {
            .offset = {0, 0},
            .extent = extent,
        },
        .clearValueCount = 1,
        .pClearValues = &bundleClear,
    };
    const VkViewport bundleViewport{
        .x = 0.0f,
        .y = static_cast<float>(extent.height),
        .width = static_cast<float>(extent.width),
        .height = -static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    const VkRect2D bundleScissor{
        .offset = {0, 0},
        .extent = extent,
    };
    vkCmdBeginRenderPass(commandBuffer, &bundleBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bundlePipeline);
    vkCmdSetViewport(commandBuffer, 0, 1, &bundleViewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &bundleScissor);
    const std::array<VkDescriptorSet, 2> bundleSets = {cameraDescriptorSet, bundleTextureDescriptorSet};
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            bundlePipelineLayout,
                            0,
                            static_cast<std::uint32_t>(bundleSets.size()),
                            bundleSets.data(),
                            0,
                            nullptr);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
}

} // namespace

bool RunVulkanNativeSceneLoop(const int width,
                              const int height,
                              const VulkanNativeSceneFrameCallback& buildFrame,
                              const VulkanPreviewWindowOptions& options,
                              std::string* error) {
#if RAWIRON_VULKAN_NATIVE_PREVIEW_ENABLED
    try {
        if (!buildFrame) {
            throw std::runtime_error("RunVulkanNativeSceneLoop requires a frame callback.");
        }

        WindowState windowState{};
        windowState.messageUserData = options.messageUserData;
        windowState.onWin32Message = options.onWin32Message;

        const HWND existingClientHwnd = static_cast<HWND>(options.clientHwnd);
        const bool usingExistingClient = existingClientHwnd != nullptr;
        const HWND parentHwnd = static_cast<HWND>(options.parentHwnd);
        const bool embedded = parentHwnd != nullptr || usingExistingClient;
        std::unique_ptr<ScopedWindowClass> ownedWindowClass{};
        const HINSTANCE surfaceInstance = GetModuleHandleW(nullptr);
        HWND hwnd = existingClientHwnd;
        if (!usingExistingClient) {
            ownedWindowClass = std::make_unique<ScopedWindowClass>(&NativePreviewWindowProc);
            const DWORD visibilityStyle = options.showWindow ? WS_VISIBLE : 0U;
            const DWORD windowStyle = embedded
                ? (WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | visibilityStyle)
                : (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | visibilityStyle);
            RECT rect{0, 0, std::max(width, 1), std::max(height, 1)};
            if (!embedded) {
                AdjustWindowRect(&rect, windowStyle, FALSE);
            }
            hwnd = CreateWindowExW(
                embedded ? WS_EX_NOPARENTNOTIFY : 0,
                ownedWindowClass->className,
                Widen(options.windowTitle).c_str(),
                windowStyle,
                embedded ? 0 : CW_USEDEFAULT,
                embedded ? 0 : CW_USEDEFAULT,
                rect.right - rect.left,
                rect.bottom - rect.top,
                parentHwnd,
                nullptr,
                ownedWindowClass->instance,
                &windowState);
            if (hwnd == nullptr) {
                throw std::runtime_error("CreateWindowExW failed for Vulkan native scene preview window.");
            }
        }
        windowState.hwnd = hwnd;
        if (options.outClientHwnd != nullptr) {
            *static_cast<HWND*>(options.outClientHwnd) = hwnd;
        }
        if (!usingExistingClient && options.onClientHwndCreated) {
            options.onClientHwndCreated(hwnd);
        }

        const std::array<const char*, 2> extensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        };
        // ReShade and other layers often assume Vulkan 1.1+ instance API; 1.0-only instances can break
        // vkCreateDevice after layer chaining (seen as VK_ERROR_FEATURE_NOT_PRESENT / -8).
        const VkApplicationInfo applicationInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "RawIron Vulkan Native Preview",
            .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
            .pEngineName = "RawIron",
            .engineVersion = VK_MAKE_VERSION(0, 1, 0),
            .apiVersion = VK_API_VERSION_1_1,
        };
        const VkInstanceCreateInfo instanceInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &applicationInfo,
            .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
        };

        VkInstance instance = VK_NULL_HANDLE;
        ExpectVk(vkCreateInstance(&instanceInfo, nullptr, &instance), "vkCreateInstance");

        const VkWin32SurfaceCreateInfoKHR surfaceInfo{
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .hinstance = surfaceInstance,
            .hwnd = hwnd,
        };
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        ExpectVk(vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface), "vkCreateWin32SurfaceKHR");

        const DeviceSelection selection = PickDevice(instance, surface);

        const float queuePriority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        std::vector<std::uint32_t> families = {selection.graphicsQueueFamily, selection.presentQueueFamily};
        std::sort(families.begin(), families.end());
        families.erase(std::unique(families.begin(), families.end()), families.end());
        for (const std::uint32_t family : families) {
            queueInfos.push_back(VkDeviceQueueCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = family,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority,
            });
        }

        const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        // Match VulkanPreviewPresenter: omit pEnabledFeatures (implicit default features only). We still clamp
        // maxAnisotropy from device limits for sampler creation; avoid requesting features here so layers /
        // odd drivers cannot reject vkCreateDevice with VK_ERROR_FEATURE_NOT_PRESENT.
        const VkDeviceCreateInfo deviceInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = static_cast<std::uint32_t>(queueInfos.size()),
            .pQueueCreateInfos = queueInfos.data(),
            .enabledExtensionCount = 1,
            .ppEnabledExtensionNames = deviceExtensions,
        };

        VkDevice device = VK_NULL_HANDLE;
        ExpectVk(vkCreateDevice(selection.physicalDevice, &deviceInfo, nullptr, &device), "vkCreateDevice");

        VkPhysicalDeviceProperties physicalDeviceProperties{};
        vkGetPhysicalDeviceProperties(selection.physicalDevice, &physicalDeviceProperties);
        ri::core::LogInfo(std::string("Vulkan device: ") + physicalDeviceProperties.deviceName);
        const float maxSamplerAnisotropy =
            std::min(16.0f, std::max(1.0f, physicalDeviceProperties.limits.maxSamplerAnisotropy));

        VulkanPipelineCacheIdentity pipelineCacheIdentity{
            .vendorId = physicalDeviceProperties.vendorID,
            .deviceId = physicalDeviceProperties.deviceID,
            .driverVersion = physicalDeviceProperties.driverVersion,
        };
        std::copy_n(physicalDeviceProperties.pipelineCacheUUID,
                    pipelineCacheIdentity.uuid.size(),
                    pipelineCacheIdentity.uuid.begin());
        const fs::path pipelineWarmupCachePath = options.pipelineWarmupCachePath.empty()
            ? DefaultVulkanPipelineWarmupCachePath()
            : options.pipelineWarmupCachePath;
        std::vector<std::uint8_t> pipelineWarmupBlob = options.enablePersistentPipelineWarmupCache
            ? LoadVulkanPipelineWarmupBlob(pipelineWarmupCachePath, pipelineCacheIdentity)
            : std::vector<std::uint8_t>{};
        VkPipelineCacheCreateInfo pipelineCacheInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
            .initialDataSize = pipelineWarmupBlob.size(),
            .pInitialData = pipelineWarmupBlob.empty() ? nullptr : pipelineWarmupBlob.data(),
        };
        VkPipelineCache pipelineCache = VK_NULL_HANDLE;
        VkResult pipelineCacheResult = vkCreatePipelineCache(device, &pipelineCacheInfo, nullptr, &pipelineCache);
        if (pipelineCacheResult != VK_SUCCESS && !pipelineWarmupBlob.empty()) {
            ri::core::LogInfo("Vulkan pipeline warmup cache was rejected by the driver; rebuilding it.");
            pipelineWarmupBlob.clear();
            pipelineCacheInfo.initialDataSize = 0U;
            pipelineCacheInfo.pInitialData = nullptr;
            pipelineCacheResult = vkCreatePipelineCache(device, &pipelineCacheInfo, nullptr, &pipelineCache);
        }
        ExpectVk(pipelineCacheResult, "vkCreatePipelineCache");
        if (options.enablePersistentPipelineWarmupCache) {
            ri::core::LogInfo(
                std::string("Vulkan pipeline warmup cache: ")
                + (pipelineWarmupBlob.empty() ? "cold" : "loaded " + std::to_string(pipelineWarmupBlob.size()) + " bytes")
                + " path=" + pipelineWarmupCachePath.generic_string());
        }

        VkQueue graphicsQueue = VK_NULL_HANDLE;
        VkQueue presentQueue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, selection.graphicsQueueFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, selection.presentQueueFamily, 0, &presentQueue);

        VkSurfaceCapabilitiesKHR capabilities{};
        std::uint32_t formatCount = 0;
        std::vector<VkSurfaceFormatKHR> formats{};
        std::uint32_t presentModeCount = 0;
        std::vector<VkPresentModeKHR> presentModes{};
        const int surfaceQueryAttempts = usingExistingClient ? 20 : 1;
        bool surfaceQueryOk = false;
        for (int attempt = 0; attempt < surfaceQueryAttempts; ++attempt) {
            const VkResult capabilitiesResult =
                vkGetPhysicalDeviceSurfaceCapabilitiesKHR(selection.physicalDevice, surface, &capabilities);
            if (capabilitiesResult == VK_ERROR_SURFACE_LOST_KHR || capabilitiesResult == VK_ERROR_OUT_OF_DATE_KHR) {
                Sleep(25);
                continue;
            }
            ExpectVk(capabilitiesResult, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

            const VkResult formatCountResult =
                vkGetPhysicalDeviceSurfaceFormatsKHR(selection.physicalDevice, surface, &formatCount, nullptr);
            if (formatCountResult == VK_ERROR_SURFACE_LOST_KHR || formatCountResult == VK_ERROR_OUT_OF_DATE_KHR) {
                Sleep(25);
                continue;
            }
            ExpectVk(formatCountResult, "vkGetPhysicalDeviceSurfaceFormatsKHR(count)");
            formats.assign(formatCount, {});
            if (formatCount > 0U) {
                const VkResult formatListResult = vkGetPhysicalDeviceSurfaceFormatsKHR(
                    selection.physicalDevice, surface, &formatCount, formats.data());
                if (formatListResult == VK_ERROR_SURFACE_LOST_KHR || formatListResult == VK_ERROR_OUT_OF_DATE_KHR) {
                    Sleep(25);
                    continue;
                }
                ExpectVk(formatListResult, "vkGetPhysicalDeviceSurfaceFormatsKHR(list)");
            }

            const VkResult presentModeCountResult =
                vkGetPhysicalDeviceSurfacePresentModesKHR(selection.physicalDevice, surface, &presentModeCount, nullptr);
            if (presentModeCountResult == VK_ERROR_SURFACE_LOST_KHR || presentModeCountResult == VK_ERROR_OUT_OF_DATE_KHR) {
                Sleep(25);
                continue;
            }
            ExpectVk(presentModeCountResult, "vkGetPhysicalDeviceSurfacePresentModesKHR(count)");
            presentModes.assign(presentModeCount, {});
            if (presentModeCount > 0U) {
                const VkResult presentModeListResult = vkGetPhysicalDeviceSurfacePresentModesKHR(
                    selection.physicalDevice, surface, &presentModeCount, presentModes.data());
                if (presentModeListResult == VK_ERROR_SURFACE_LOST_KHR || presentModeListResult == VK_ERROR_OUT_OF_DATE_KHR) {
                    Sleep(25);
                    continue;
                }
                ExpectVk(presentModeListResult, "vkGetPhysicalDeviceSurfacePresentModesKHR(list)");
            }
            surfaceQueryOk = true;
            break;
        }
        if (!surfaceQueryOk) {
            throw std::runtime_error("Vulkan surface did not stabilize for swapchain creation.");
        }
        if (formats.empty()) {
            throw std::runtime_error("No Vulkan surface formats were available.");
        }
        const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(formats);
        const VkPresentModeKHR presentMode = ChoosePresentMode(presentModes, options.presentModePreference);
        ri::core::LogInfo(std::string("Vulkan present mode: ") + PresentModeName(presentMode));

        VkExtent2D extent = capabilities.currentExtent;
        if (extent.width == std::numeric_limits<std::uint32_t>::max()) {
            extent.width = static_cast<std::uint32_t>(std::max(width, 1));
            extent.height = static_cast<std::uint32_t>(std::max(height, 1));
        }

        std::uint32_t imageCount = capabilities.minImageCount + 1U;
        if (capabilities.maxImageCount > 0U) {
            imageCount = std::min(imageCount, capabilities.maxImageCount);
        }

        if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0U) {
            throw std::runtime_error("Swapchain images do not support color attachments on this device.");
        }

        VkSwapchainCreateInfoKHR swapchainInfo{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface,
            .minImageCount = imageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                | (((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0U)
                       ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                       : 0U),
            .imageSharingMode = selection.graphicsQueueFamily == selection.presentQueueFamily
                ? VK_SHARING_MODE_EXCLUSIVE
                : VK_SHARING_MODE_CONCURRENT,
            .queueFamilyIndexCount = selection.graphicsQueueFamily == selection.presentQueueFamily ? 0U : 2U,
            .pQueueFamilyIndices = selection.graphicsQueueFamily == selection.presentQueueFamily ? nullptr : families.data(),
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = presentMode,
            .clipped = VK_TRUE,
        };

        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        ExpectVk(vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain), "vkCreateSwapchainKHR");

        std::uint32_t swapchainImageCount = 0;
        ExpectVk(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr), "vkGetSwapchainImagesKHR(count)");
        std::vector<VkImage> swapchainImages(swapchainImageCount);
        ExpectVk(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data()), "vkGetSwapchainImagesKHR(list)");

        std::vector<VkImageView> swapchainImageViews;
        swapchainImageViews.reserve(swapchainImages.size());
        for (VkImage image : swapchainImages) {
            const VkImageViewCreateInfo imageViewInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = surfaceFormat.format,
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            VkImageView view = VK_NULL_HANDLE;
            ExpectVk(vkCreateImageView(device, &imageViewInfo, nullptr, &view), "vkCreateImageView(color)");
            swapchainImageViews.push_back(view);
        }

        const VkFormat depthFormat = FindDepthFormat(selection.physicalDevice);
        const VkFormat gbufferFormat = FindHdrSceneColorFormat(selection.physicalDevice);
        const ImageResource depthImage =
            CreateDepthImage(selection.physicalDevice,
                             device,
                             depthFormat,
                             extent.width,
                             extent.height,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        ImageResource gbufferNormalRoughnessImage =
            CreateHdrSceneColorImage(selection.physicalDevice, device, gbufferFormat, extent.width, extent.height);
        ImageResource gbufferMaterialImage =
            CreateHdrSceneColorImage(selection.physicalDevice, device, gbufferFormat, extent.width, extent.height);
        const VkFormat shadowDepthFormat = FindShadowDepthFormat(selection.physicalDevice);
        const std::uint32_t kShadowMapResolution =
            NativeShadowMapResolutionForTier(options.initialRenderQualityTier);
        ImageResource shadowDepthImage = CreateDepthImage(
            selection.physicalDevice,
            device,
            shadowDepthFormat,
            kShadowMapResolution,
            kShadowMapResolution,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        const VkAttachmentDescription colorAttachment{
            .format = surfaceFormat.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        };
        const VkAttachmentDescription depthAttachment{
            .format = depthFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        const VkAttachmentDescription gbufferAttachment{
            .format = gbufferFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        const std::array<VkAttachmentReference, 3> colorReferences = {{
            VkAttachmentReference{
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            VkAttachmentReference{
                .attachment = 1,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            VkAttachmentReference{
                .attachment = 2,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
        }};
        const VkAttachmentReference depthReference{
            .attachment = 3,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        const VkSubpassDescription subpass{
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = static_cast<std::uint32_t>(colorReferences.size()),
            .pColorAttachments = colorReferences.data(),
            .pDepthStencilAttachment = &depthReference,
        };
        const VkSubpassDependency dependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        };
        const std::array<VkAttachmentDescription, 4> attachments = {
            colorAttachment,
            gbufferAttachment,
            gbufferAttachment,
            depthAttachment,
        };
        const VkRenderPassCreateInfo renderPassInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = static_cast<std::uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency,
        };
        VkRenderPass renderPass = VK_NULL_HANDLE;
        ExpectVk(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass), "vkCreateRenderPass");

        const VkAttachmentDescription shadowDepthAttachment{
            .format = shadowDepthFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        };
        const VkAttachmentReference shadowDepthReference{
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        const VkSubpassDescription shadowSubpass{
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .pDepthStencilAttachment = &shadowDepthReference,
        };
        const std::array<VkSubpassDependency, 2> shadowDependencies = {{
            VkSubpassDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            },
            VkSubpassDependency{
                .srcSubpass = 0,
                .dstSubpass = VK_SUBPASS_EXTERNAL,
                .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            },
        }};
        const VkRenderPassCreateInfo shadowRenderPassInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &shadowDepthAttachment,
            .subpassCount = 1,
            .pSubpasses = &shadowSubpass,
            .dependencyCount = static_cast<std::uint32_t>(shadowDependencies.size()),
            .pDependencies = shadowDependencies.data(),
        };
        VkRenderPass shadowRenderPass = VK_NULL_HANDLE;
        ExpectVk(vkCreateRenderPass(device, &shadowRenderPassInfo, nullptr, &shadowRenderPass),
                 "vkCreateRenderPass(shadow)");
        const VkFramebufferCreateInfo shadowFramebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = shadowRenderPass,
            .attachmentCount = 1,
            .pAttachments = &shadowDepthImage.view,
            .width = kShadowMapResolution,
            .height = kShadowMapResolution,
            .layers = 1,
        };
        VkFramebuffer shadowFramebuffer = VK_NULL_HANDLE;
        ExpectVk(vkCreateFramebuffer(device, &shadowFramebufferInfo, nullptr, &shadowFramebuffer),
                 "vkCreateFramebuffer(shadow)");

        std::vector<VkFramebuffer> framebuffers;
        framebuffers.reserve(swapchainImageViews.size());
        for (VkImageView imageView : swapchainImageViews) {
            const std::array<VkImageView, 4> attachmentsForFramebuffer = {
                imageView,
                gbufferNormalRoughnessImage.view,
                gbufferMaterialImage.view,
                depthImage.view,
            };
            const VkFramebufferCreateInfo framebufferInfo{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = renderPass,
                .attachmentCount = static_cast<std::uint32_t>(attachmentsForFramebuffer.size()),
                .pAttachments = attachmentsForFramebuffer.data(),
                .width = extent.width,
                .height = extent.height,
                .layers = 1,
            };
            VkFramebuffer framebuffer = VK_NULL_HANDLE;
            ExpectVk(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer), "vkCreateFramebuffer");
            framebuffers.push_back(framebuffer);
        }

        const VkDescriptorSetLayoutBinding cameraBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        };
        const VkDescriptorSetLayoutCreateInfo cameraSetLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &cameraBinding,
        };
        VkDescriptorSetLayout cameraSetLayout = VK_NULL_HANDLE;
        ExpectVk(vkCreateDescriptorSetLayout(device, &cameraSetLayoutInfo, nullptr, &cameraSetLayout),
                 "vkCreateDescriptorSetLayout(camera)");

        std::array<VkDescriptorSetLayoutBinding, 6> textureBindings{};
        for (std::uint32_t index = 0; index < textureBindings.size(); ++index) {
            textureBindings[index] = VkDescriptorSetLayoutBinding{
                .binding = index,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            };
        }
        const VkDescriptorSetLayoutCreateInfo textureSetLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = static_cast<std::uint32_t>(textureBindings.size()),
            .pBindings = textureBindings.data(),
        };
        VkDescriptorSetLayout textureSetLayout = VK_NULL_HANDLE;
        ExpectVk(vkCreateDescriptorSetLayout(device, &textureSetLayoutInfo, nullptr, &textureSetLayout),
                 "vkCreateDescriptorSetLayout(texture)");

        const VkDescriptorSetLayoutBinding shadowBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        };
        const VkDescriptorSetLayoutCreateInfo shadowSetLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &shadowBinding,
        };
        VkDescriptorSetLayout shadowSetLayout = VK_NULL_HANDLE;
        ExpectVk(vkCreateDescriptorSetLayout(device, &shadowSetLayoutInfo, nullptr, &shadowSetLayout),
                 "vkCreateDescriptorSetLayout(shadow)");

        const VkDescriptorSetLayoutBinding skyUniformBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        };
        const VkDescriptorSetLayoutCreateInfo skyCameraSetLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &skyUniformBinding,
        };
        VkDescriptorSetLayout skyCameraSetLayout = VK_NULL_HANDLE;
        ExpectVk(vkCreateDescriptorSetLayout(device, &skyCameraSetLayoutInfo, nullptr, &skyCameraSetLayout),
                 "vkCreateDescriptorSetLayout(sky-camera)");

        const bool enableHybridHdr = options.enableHybridHdrPresentation;
        VkFormat hdrSceneFormat = VK_FORMAT_UNDEFINED;
        ImageResource hdrSceneColorImage{};
        VkRenderPass hdrSceneRenderPass = VK_NULL_HANDLE;
        VkFramebuffer hdrSceneFramebuffer = VK_NULL_HANDLE;
        VkRenderPass compositeRenderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> compositeFramebuffers;
        VkDescriptorSetLayout compositeHdrTextureSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout compositePipelineLayout = VK_NULL_HANDLE;
        VkShaderModule compositeVertShader = VK_NULL_HANDLE;
        VkShaderModule compositeFragShader = VK_NULL_HANDLE;
        VkPipeline pipelineHdrScene = VK_NULL_HANDLE;
        VkPipeline pipelineHdrSceneTransparent = VK_NULL_HANDLE;
        VkPipeline pipelineHdrSceneAdditive = VK_NULL_HANDLE;
        VkPipeline skyPipelineHdr = VK_NULL_HANDLE;
        VkPipeline compositePipeline = VK_NULL_HANDLE;
        VkDescriptorPool compositeDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet compositeHdrDescriptorSet = VK_NULL_HANDLE;
        ImageResource fakeMotionBlurHistory{};
        VkSampler hybridDepthSamplerNearest = VK_NULL_HANDLE;
        ImageResource hybridBundleHdrImage{};
        VkRenderPass hybridBundleRenderPass = VK_NULL_HANDLE;
        VkFramebuffer hybridBundleFramebuffer = VK_NULL_HANDLE;
        VkDescriptorSetLayout hybridBundleTexturesSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout hybridBundlePipelineLayout = VK_NULL_HANDLE;
        VkShaderModule hybridBundleVertShader = VK_NULL_HANDLE;
        VkShaderModule hybridBundleFragShader = VK_NULL_HANDLE;
        VkPipeline hybridBundlePipeline = VK_NULL_HANDLE;
        VkDescriptorPool hybridBundleDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet hybridBundleDescriptorSet = VK_NULL_HANDLE;

        if (enableHybridHdr) {
            hdrSceneFormat = FindHdrSceneColorFormat(selection.physicalDevice);
            hdrSceneColorImage =
                CreateHdrSceneColorImage(selection.physicalDevice, device, hdrSceneFormat, extent.width, extent.height);

            const VkAttachmentDescription hdrSceneColorAttachmentDesc{
                .format = hdrSceneFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            const VkAttachmentDescription hdrSceneDepthAttachmentDesc{
                .format = depthFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            };
            const std::array<VkAttachmentDescription, 4> hdrSceneAttachmentDescs = {
                hdrSceneColorAttachmentDesc,
                gbufferAttachment,
                gbufferAttachment,
                hdrSceneDepthAttachmentDesc,
            };
            const VkSubpassDependency hdrSceneToSampleDependency{
                .srcSubpass = 0,
                .dstSubpass = VK_SUBPASS_EXTERNAL,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            };
            const VkSubpassDependency hdrSceneDepthToSampleDependency{
                .srcSubpass = 0,
                .dstSubpass = VK_SUBPASS_EXTERNAL,
                .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            };
            const std::array<VkSubpassDependency, 3> hdrSceneDependencies = {
                dependency,
                hdrSceneToSampleDependency,
                hdrSceneDepthToSampleDependency,
            };
            const VkRenderPassCreateInfo hdrSceneRenderPassInfo{
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .attachmentCount = static_cast<std::uint32_t>(hdrSceneAttachmentDescs.size()),
                .pAttachments = hdrSceneAttachmentDescs.data(),
                .subpassCount = 1,
                .pSubpasses = &subpass,
                .dependencyCount = static_cast<std::uint32_t>(hdrSceneDependencies.size()),
                .pDependencies = hdrSceneDependencies.data(),
            };
            ExpectVk(vkCreateRenderPass(device, &hdrSceneRenderPassInfo, nullptr, &hdrSceneRenderPass),
                     "vkCreateRenderPass(hdr-scene)");

            const std::array<VkImageView, 4> hdrSceneFbAttachments = {
                hdrSceneColorImage.view,
                gbufferNormalRoughnessImage.view,
                gbufferMaterialImage.view,
                depthImage.view,
            };
            const VkFramebufferCreateInfo hdrSceneFramebufferInfo{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = hdrSceneRenderPass,
                .attachmentCount = static_cast<std::uint32_t>(hdrSceneFbAttachments.size()),
                .pAttachments = hdrSceneFbAttachments.data(),
                .width = extent.width,
                .height = extent.height,
                .layers = 1,
            };
            ExpectVk(vkCreateFramebuffer(device, &hdrSceneFramebufferInfo, nullptr, &hdrSceneFramebuffer),
                     "vkCreateFramebuffer(hdr-scene)");

            const VkAttachmentDescription compositeSwapAttachmentDesc{
                .format = surfaceFormat.format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            };
            const VkAttachmentReference compositeSwapColorReference{
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };
            const VkSubpassDescription compositeSubpassDesc{
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = 1,
                .pColorAttachments = &compositeSwapColorReference,
            };
            const VkSubpassDependency compositePassDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            };
            const VkRenderPassCreateInfo compositeRenderPassInfo{
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments = &compositeSwapAttachmentDesc,
                .subpassCount = 1,
                .pSubpasses = &compositeSubpassDesc,
                .dependencyCount = 1,
                .pDependencies = &compositePassDependency,
            };
            ExpectVk(vkCreateRenderPass(device, &compositeRenderPassInfo, nullptr, &compositeRenderPass),
                     "vkCreateRenderPass(composite)");
            compositeFramebuffers.reserve(swapchainImageViews.size());
            for (VkImageView swapColorView : swapchainImageViews) {
                const VkFramebufferCreateInfo compositeFbInfo{
                    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                    .renderPass = compositeRenderPass,
                    .attachmentCount = 1,
                    .pAttachments = &swapColorView,
                    .width = extent.width,
                    .height = extent.height,
                    .layers = 1,
                };
                VkFramebuffer compositeFb = VK_NULL_HANDLE;
                ExpectVk(vkCreateFramebuffer(device, &compositeFbInfo, nullptr, &compositeFb), "vkCreateFramebuffer(composite)");
                compositeFramebuffers.push_back(compositeFb);
            }

            const std::array<VkDescriptorSetLayoutBinding, 3> compositeHdrSamplerBindings = {{
                VkDescriptorSetLayoutBinding{
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                VkDescriptorSetLayoutBinding{
                    .binding = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                VkDescriptorSetLayoutBinding{
                    .binding = 2,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
            }};
            const VkDescriptorSetLayoutCreateInfo compositeHdrSamplerLayoutInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = static_cast<std::uint32_t>(compositeHdrSamplerBindings.size()),
                .pBindings = compositeHdrSamplerBindings.data(),
            };
            ExpectVk(vkCreateDescriptorSetLayout(device, &compositeHdrSamplerLayoutInfo, nullptr, &compositeHdrTextureSetLayout),
                     "vkCreateDescriptorSetLayout(composite-hdr)");

            const std::array<VkDescriptorSetLayout, 8> compositePipelineSetLayouts = {
                cameraSetLayout,
                compositeHdrTextureSetLayout,
                textureSetLayout,
                textureSetLayout,
                textureSetLayout,
                textureSetLayout,
                textureSetLayout,
                textureSetLayout};
            const VkPipelineLayoutCreateInfo compositePipelineLayoutInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = static_cast<std::uint32_t>(compositePipelineSetLayouts.size()),
                .pSetLayouts = compositePipelineSetLayouts.data(),
                .pushConstantRangeCount = 0,
                .pPushConstantRanges = nullptr,
            };
            ExpectVk(vkCreatePipelineLayout(device, &compositePipelineLayoutInfo, nullptr, &compositePipelineLayout),
                     "vkCreatePipelineLayout(composite)");

            hybridBundleHdrImage =
                CreateHdrSceneColorImage(selection.physicalDevice, device, hdrSceneFormat, extent.width, extent.height);
            fakeMotionBlurHistory = CreateRgba8HistoryImage(
                selection.physicalDevice, device, surfaceFormat.format, extent.width, extent.height);

            const VkAttachmentDescription hybridBundleColorAttachmentDesc{
                .format = hdrSceneFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            const VkAttachmentReference hybridBundleColorReference{
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };
            const VkSubpassDescription hybridBundleSubpassDesc{
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = 1,
                .pColorAttachments = &hybridBundleColorReference,
            };
            const VkSubpassDependency hybridBundlePassDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            };
            const VkRenderPassCreateInfo hybridBundleRenderPassInfo{
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments = &hybridBundleColorAttachmentDesc,
                .subpassCount = 1,
                .pSubpasses = &hybridBundleSubpassDesc,
                .dependencyCount = 1,
                .pDependencies = &hybridBundlePassDependency,
            };
            ExpectVk(vkCreateRenderPass(device, &hybridBundleRenderPassInfo, nullptr, &hybridBundleRenderPass),
                     "vkCreateRenderPass(hybrid-bundle)");

            const std::array<VkImageView, 1> hybridBundleAttachmentViews = {hybridBundleHdrImage.view};
            const VkFramebufferCreateInfo hybridBundleFramebufferInfo{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = hybridBundleRenderPass,
                .attachmentCount = static_cast<std::uint32_t>(hybridBundleAttachmentViews.size()),
                .pAttachments = hybridBundleAttachmentViews.data(),
                .width = extent.width,
                .height = extent.height,
                .layers = 1,
            };
            ExpectVk(vkCreateFramebuffer(device, &hybridBundleFramebufferInfo, nullptr, &hybridBundleFramebuffer),
                     "vkCreateFramebuffer(hybrid-bundle)");

            const std::array<VkDescriptorSetLayoutBinding, 4> hybridBundleTextureBindings = {{
                VkDescriptorSetLayoutBinding{
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                VkDescriptorSetLayoutBinding{
                    .binding = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                VkDescriptorSetLayoutBinding{
                    .binding = 2,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                VkDescriptorSetLayoutBinding{
                    .binding = 3,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
            }};
            const VkDescriptorSetLayoutCreateInfo hybridBundleTexturesLayoutInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = static_cast<std::uint32_t>(hybridBundleTextureBindings.size()),
                .pBindings = hybridBundleTextureBindings.data(),
            };
            ExpectVk(vkCreateDescriptorSetLayout(device, &hybridBundleTexturesLayoutInfo, nullptr, &hybridBundleTexturesSetLayout),
                     "vkCreateDescriptorSetLayout(hybrid-bundle-textures)");

            const std::array<VkDescriptorSetLayout, 2> hybridBundlePipelineSetLayouts = {cameraSetLayout,
                                                                                       hybridBundleTexturesSetLayout};
            const VkPipelineLayoutCreateInfo hybridBundlePipelineLayoutInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = static_cast<std::uint32_t>(hybridBundlePipelineSetLayouts.size()),
                .pSetLayouts = hybridBundlePipelineSetLayouts.data(),
            };
            ExpectVk(vkCreatePipelineLayout(device, &hybridBundlePipelineLayoutInfo, nullptr, &hybridBundlePipelineLayout),
                     "vkCreatePipelineLayout(hybrid-bundle)");
        }

        const std::array<VkDescriptorSetLayout, 3> pipelineSetLayouts = {cameraSetLayout, textureSetLayout, shadowSetLayout};

        const VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(NativeDrawPushConstants),
        };
        const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = static_cast<std::uint32_t>(pipelineSetLayouts.size()),
            .pSetLayouts = pipelineSetLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange,
        };
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        ExpectVk(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout");

        const std::array<VkDescriptorSetLayout, 2> skyPipelineSetLayouts = {skyCameraSetLayout, textureSetLayout};
        const VkPipelineLayoutCreateInfo skyPipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = static_cast<std::uint32_t>(skyPipelineSetLayouts.size()),
            .pSetLayouts = skyPipelineSetLayouts.data(),
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
        };
        VkPipelineLayout skyPipelineLayout = VK_NULL_HANDLE;
        ExpectVk(vkCreatePipelineLayout(device, &skyPipelineLayoutInfo, nullptr, &skyPipelineLayout),
                 "vkCreatePipelineLayout(sky)");

        const std::array<VkDescriptorSetLayout, 2> shadowPipelineSetLayouts = {cameraSetLayout, textureSetLayout};
        const VkPushConstantRange shadowPushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(NativeDrawPushConstants),
        };
        const VkPipelineLayoutCreateInfo shadowPipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = static_cast<std::uint32_t>(shadowPipelineSetLayouts.size()),
            .pSetLayouts = shadowPipelineSetLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &shadowPushConstantRange,
        };
        VkPipelineLayout shadowPipelineLayout = VK_NULL_HANDLE;
        ExpectVk(vkCreatePipelineLayout(device, &shadowPipelineLayoutInfo, nullptr, &shadowPipelineLayout),
                 "vkCreatePipelineLayout(shadow)");

        const fs::path shaderDir = ResolveVulkanNativeShaderDirectory();
        const VkShaderModule vertShader = CreateShaderModule(device, shaderDir / "NativeScenePreview.vert.spv");
        const VkShaderModule fragShader = CreateShaderModule(device, shaderDir / "NativeScenePreview.frag.spv");
        const VkShaderModule skyVertShader = CreateShaderModule(device, shaderDir / "NativeSkybox.vert.spv");
        const VkShaderModule skyFragShader = CreateShaderModule(device, shaderDir / "NativeSkybox.frag.spv");
        const VkShaderModule shadowVertShader = CreateShaderModule(device, shaderDir / "NativeShadowDepth.vert.spv");
        const VkShaderModule shadowFragShader = CreateShaderModule(device, shaderDir / "NativeShadowDepth.frag.spv");
        if (enableHybridHdr) {
            compositeVertShader = CreateShaderModule(device, shaderDir / "NativeHybridComposite.vert.spv");
            compositeFragShader = CreateShaderModule(
                device,
                shaderDir
                    / (options.enableExtendedPostProcessShader
                           ? "NativeComposite.frag.spv"
                           : "NativeHybridComposite.frag.spv"));
            hybridBundleVertShader = CreateShaderModule(device, shaderDir / "NativeHybridScreenSpace.vert.spv");
            hybridBundleFragShader = CreateShaderModule(device, shaderDir / "NativeHybridScreenSpace.frag.spv");
        }

        const VkPipelineShaderStageCreateInfo vertStage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShader,
            .pName = "main",
        };
        const VkPipelineShaderStageCreateInfo fragStage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShader,
            .pName = "main",
        };
        const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {vertStage, fragStage};

        const VkVertexInputBindingDescription vertexBinding{
            .binding = 0,
            .stride = static_cast<std::uint32_t>(sizeof(NativeSceneVertex)),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
        const std::array<VkVertexInputAttributeDescription, 3> vertexAttributes = {{
            VkVertexInputAttributeDescription{
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = static_cast<std::uint32_t>(offsetof(NativeSceneVertex, position)),
            },
            VkVertexInputAttributeDescription{
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = static_cast<std::uint32_t>(offsetof(NativeSceneVertex, normal)),
            },
            VkVertexInputAttributeDescription{
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = static_cast<std::uint32_t>(offsetof(NativeSceneVertex, uv)),
            },
        }};
        const VkPipelineVertexInputStateCreateInfo vertexInputInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &vertexBinding,
            .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(vertexAttributes.size()),
            .pVertexAttributeDescriptions = vertexAttributes.data(),
        };
        const VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        };
        const VkPipelineViewportStateCreateInfo viewportStateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };
        const VkPipelineRasterizationStateCreateInfo rasterizationInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.0f,
        };
        const VkPipelineMultisampleStateCreateInfo multisampleInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };
        const VkPipelineDepthStencilStateCreateInfo depthStencilInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        };
        const VkPipelineColorBlendAttachmentState blendAttachmentOpaque{
            .blendEnable = VK_FALSE,
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
        const VkPipelineColorBlendAttachmentState blendAttachmentTransparent{
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
        const VkPipelineColorBlendAttachmentState gbufferBlendAttachment{
            .blendEnable = VK_FALSE,
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
        const std::array<VkPipelineColorBlendAttachmentState, 3> sceneBlendAttachmentsOpaque = {
            blendAttachmentOpaque,
            gbufferBlendAttachment,
            gbufferBlendAttachment,
        };
        const std::array<VkPipelineColorBlendAttachmentState, 3> sceneBlendAttachmentsTransparent = {
            blendAttachmentTransparent,
            gbufferBlendAttachment,
            gbufferBlendAttachment,
        };
        const VkPipelineColorBlendStateCreateInfo colorBlendInfoOpaque{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = static_cast<std::uint32_t>(sceneBlendAttachmentsOpaque.size()),
            .pAttachments = sceneBlendAttachmentsOpaque.data(),
        };
        const VkPipelineColorBlendStateCreateInfo colorBlendInfoTransparent{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = static_cast<std::uint32_t>(sceneBlendAttachmentsTransparent.size()),
            .pAttachments = sceneBlendAttachmentsTransparent.data(),
        };
        const std::array<VkDynamicState, 2> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const VkPipelineDynamicStateCreateInfo dynamicStateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data(),
        };
        const VkGraphicsPipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = static_cast<std::uint32_t>(shaderStages.size()),
            .pStages = shaderStages.data(),
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssemblyInfo,
            .pViewportState = &viewportStateInfo,
            .pRasterizationState = &rasterizationInfo,
            .pMultisampleState = &multisampleInfo,
            .pDepthStencilState = &depthStencilInfo,
            .pColorBlendState = &colorBlendInfoOpaque,
            .pDynamicState = &dynamicStateInfo,
            .layout = pipelineLayout,
            .renderPass = renderPass,
            .subpass = 0,
        };
        VkPipeline pipeline = VK_NULL_HANDLE;
        ExpectVk(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipeline),
                 "vkCreateGraphicsPipelines");

        VkGraphicsPipelineCreateInfo pipelineTransparentInfo = pipelineInfo;
        pipelineTransparentInfo.pColorBlendState = &colorBlendInfoTransparent;
        VkPipelineDepthStencilStateCreateInfo depthStencilTransparentInfo = depthStencilInfo;
        depthStencilTransparentInfo.depthWriteEnable = VK_FALSE;
        pipelineTransparentInfo.pDepthStencilState = &depthStencilTransparentInfo;
        VkPipeline pipelineTransparent = VK_NULL_HANDLE;
        ExpectVk(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineTransparentInfo, nullptr, &pipelineTransparent),
                 "vkCreateGraphicsPipelines(scene-transparent)");

        VkPipelineDepthStencilStateCreateInfo depthStencilAdditiveInfo = depthStencilInfo;
        depthStencilAdditiveInfo.depthWriteEnable = VK_FALSE;
        const VkPipelineColorBlendAttachmentState blendAttachmentAdditive{
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
        const VkPipelineColorBlendAttachmentState gbufferNoWriteAttachment{
            .blendEnable = VK_FALSE,
            .colorWriteMask = 0,
        };
        const std::array<VkPipelineColorBlendAttachmentState, 3> sceneAdditiveBlendAttachments = {
            blendAttachmentAdditive,
            gbufferNoWriteAttachment,
            gbufferNoWriteAttachment,
        };
        const VkPipelineColorBlendStateCreateInfo colorBlendAdditiveInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = static_cast<std::uint32_t>(sceneAdditiveBlendAttachments.size()),
            .pAttachments = sceneAdditiveBlendAttachments.data(),
        };
        VkGraphicsPipelineCreateInfo pipelineAdditiveInfo = pipelineInfo;
        pipelineAdditiveInfo.pDepthStencilState = &depthStencilAdditiveInfo;
        pipelineAdditiveInfo.pColorBlendState = &colorBlendAdditiveInfo;
        VkPipeline pipelineAdditive = VK_NULL_HANDLE;
        ExpectVk(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineAdditiveInfo, nullptr, &pipelineAdditive),
                 "vkCreateGraphicsPipelines(scene-additive)");

        const VkPipelineShaderStageCreateInfo skyVertStage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = skyVertShader,
            .pName = "main",
        };
        const VkPipelineShaderStageCreateInfo skyFragStage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = skyFragShader,
            .pName = "main",
        };
        const std::array<VkPipelineShaderStageCreateInfo, 2> skyShaderStages = {skyVertStage, skyFragStage};
        const VkPipelineRasterizationStateCreateInfo skyRasterizationInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_FRONT_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.0f,
        };
        const VkPipelineDepthStencilStateCreateInfo skyDepthStencilInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_FALSE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        };
        const VkPipelineColorBlendAttachmentState skyBlendAttachment{
            .blendEnable = VK_FALSE,
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
        const std::array<VkPipelineColorBlendAttachmentState, 3> skyBlendAttachments = {
            skyBlendAttachment,
            gbufferNoWriteAttachment,
            gbufferNoWriteAttachment,
        };
        const VkPipelineColorBlendStateCreateInfo skyColorBlendInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = static_cast<std::uint32_t>(skyBlendAttachments.size()),
            .pAttachments = skyBlendAttachments.data(),
        };
        const VkGraphicsPipelineCreateInfo skyPipelineInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = static_cast<std::uint32_t>(skyShaderStages.size()),
            .pStages = skyShaderStages.data(),
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssemblyInfo,
            .pViewportState = &viewportStateInfo,
            .pRasterizationState = &skyRasterizationInfo,
            .pMultisampleState = &multisampleInfo,
            .pDepthStencilState = &skyDepthStencilInfo,
            .pColorBlendState = &skyColorBlendInfo,
            .pDynamicState = &dynamicStateInfo,
            .layout = skyPipelineLayout,
            .renderPass = renderPass,
            .subpass = 0,
        };
        VkPipeline skyPipeline = VK_NULL_HANDLE;
        ExpectVk(vkCreateGraphicsPipelines(device, pipelineCache, 1, &skyPipelineInfo, nullptr, &skyPipeline),
                 "vkCreateGraphicsPipelines(sky)");

        if (enableHybridHdr) {
            VkGraphicsPipelineCreateInfo hdrPipelineInfo = pipelineInfo;
            hdrPipelineInfo.renderPass = hdrSceneRenderPass;
            ExpectVk(vkCreateGraphicsPipelines(device, pipelineCache, 1, &hdrPipelineInfo, nullptr, &pipelineHdrScene),
                     "vkCreateGraphicsPipelines(hdr-scene)");

            VkGraphicsPipelineCreateInfo hdrTransparentInfo = pipelineTransparentInfo;
            hdrTransparentInfo.renderPass = hdrSceneRenderPass;
            ExpectVk(vkCreateGraphicsPipelines(device, pipelineCache, 1, &hdrTransparentInfo, nullptr, &pipelineHdrSceneTransparent),
                     "vkCreateGraphicsPipelines(hdr-scene-transparent)");

            VkGraphicsPipelineCreateInfo hdrAdditiveInfo = pipelineAdditiveInfo;
            hdrAdditiveInfo.renderPass = hdrSceneRenderPass;
            ExpectVk(vkCreateGraphicsPipelines(device, pipelineCache, 1, &hdrAdditiveInfo, nullptr, &pipelineHdrSceneAdditive),
                     "vkCreateGraphicsPipelines(hdr-scene-additive)");

            VkGraphicsPipelineCreateInfo skyHdrPipelineInfo = skyPipelineInfo;
            skyHdrPipelineInfo.renderPass = hdrSceneRenderPass;
            ExpectVk(vkCreateGraphicsPipelines(device, pipelineCache, 1, &skyHdrPipelineInfo, nullptr, &skyPipelineHdr),
                     "vkCreateGraphicsPipelines(sky-hdr)");

            const VkPipelineShaderStageCreateInfo compositeVertStage{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = compositeVertShader,
                .pName = "main",
            };
            const VkPipelineShaderStageCreateInfo compositeFragStage{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = compositeFragShader,
                .pName = "main",
            };
            const std::array<VkPipelineShaderStageCreateInfo, 2> compositeStages = {compositeVertStage, compositeFragStage};
            const VkPipelineVertexInputStateCreateInfo compositeVertexInputInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 0,
                .pVertexBindingDescriptions = nullptr,
                .vertexAttributeDescriptionCount = 0,
                .pVertexAttributeDescriptions = nullptr,
            };
            const VkPipelineRasterizationStateCreateInfo compositeRasterInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .lineWidth = 1.0f,
            };
            const VkPipelineDepthStencilStateCreateInfo compositeDepthStencilInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                .depthTestEnable = VK_FALSE,
                .depthWriteEnable = VK_FALSE,
            };
            const VkPipelineColorBlendAttachmentState compositeBlendAttachment{
                .blendEnable = VK_FALSE,
                .colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            };
            const VkPipelineColorBlendStateCreateInfo compositeColorBlendInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments = &compositeBlendAttachment,
            };
            const VkGraphicsPipelineCreateInfo compositePipelineInfo{
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .stageCount = static_cast<std::uint32_t>(compositeStages.size()),
                .pStages = compositeStages.data(),
                .pVertexInputState = &compositeVertexInputInfo,
                .pInputAssemblyState = &inputAssemblyInfo,
                .pViewportState = &viewportStateInfo,
                .pRasterizationState = &compositeRasterInfo,
                .pMultisampleState = &multisampleInfo,
                .pDepthStencilState = &compositeDepthStencilInfo,
                .pColorBlendState = &compositeColorBlendInfo,
                .pDynamicState = &dynamicStateInfo,
                .layout = compositePipelineLayout,
                .renderPass = compositeRenderPass,
                .subpass = 0,
            };
            ExpectVk(vkCreateGraphicsPipelines(device, pipelineCache, 1, &compositePipelineInfo, nullptr, &compositePipeline),
                     "vkCreateGraphicsPipelines(hybrid-composite)");

            const VkPipelineShaderStageCreateInfo hybridBundleVertStage{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = hybridBundleVertShader,
                .pName = "main",
            };
            const VkPipelineShaderStageCreateInfo hybridBundleFragStage{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = hybridBundleFragShader,
                .pName = "main",
            };
            const std::array<VkPipelineShaderStageCreateInfo, 2> hybridBundleStages = {hybridBundleVertStage,
                                                                                       hybridBundleFragStage};
            VkGraphicsPipelineCreateInfo hybridBundlePipelineInfo = compositePipelineInfo;
            hybridBundlePipelineInfo.stageCount = static_cast<std::uint32_t>(hybridBundleStages.size());
            hybridBundlePipelineInfo.pStages = hybridBundleStages.data();
            hybridBundlePipelineInfo.layout = hybridBundlePipelineLayout;
            hybridBundlePipelineInfo.renderPass = hybridBundleRenderPass;
            ExpectVk(vkCreateGraphicsPipelines(device, pipelineCache, 1, &hybridBundlePipelineInfo, nullptr, &hybridBundlePipeline),
                     "vkCreateGraphicsPipelines(hybrid-bundle)");
        }

        const VkPipelineShaderStageCreateInfo shadowVertStage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = shadowVertShader,
            .pName = "main",
        };
        const VkPipelineShaderStageCreateInfo shadowFragStage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = shadowFragShader,
            .pName = "main",
        };
        const std::array<VkPipelineShaderStageCreateInfo, 2> shadowShaderStages = {shadowVertStage, shadowFragStage};
        const VkPipelineRasterizationStateCreateInfo shadowRasterizationInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            // Keep the depth-only shadow pass tolerant of winding mistakes and mirrored content.
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_TRUE,
            .lineWidth = 1.0f,
        };
        const VkPipelineColorBlendStateCreateInfo shadowColorBlendInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 0,
            .pAttachments = nullptr,
        };
        const VkPipelineDepthStencilStateCreateInfo shadowDepthStencilInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        };
        const std::array<VkDynamicState, 3> shadowDynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_DEPTH_BIAS,
        };
        const VkPipelineDynamicStateCreateInfo shadowDynamicStateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = static_cast<std::uint32_t>(shadowDynamicStates.size()),
            .pDynamicStates = shadowDynamicStates.data(),
        };
        const VkGraphicsPipelineCreateInfo shadowPipelineInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = static_cast<std::uint32_t>(shadowShaderStages.size()),
            .pStages = shadowShaderStages.data(),
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssemblyInfo,
            .pViewportState = &viewportStateInfo,
            .pRasterizationState = &shadowRasterizationInfo,
            .pMultisampleState = &multisampleInfo,
            .pDepthStencilState = &shadowDepthStencilInfo,
            .pColorBlendState = &shadowColorBlendInfo,
            .pDynamicState = &shadowDynamicStateInfo,
            .layout = shadowPipelineLayout,
            .renderPass = shadowRenderPass,
            .subpass = 0,
        };
        VkPipeline shadowPipeline = VK_NULL_HANDLE;
        ExpectVk(vkCreateGraphicsPipelines(device, pipelineCache, 1, &shadowPipelineInfo, nullptr, &shadowPipeline),
                 "vkCreateGraphicsPipelines(shadow)");

        BufferResource uniformBuffer{};
        VkDeviceSize uniformBufferCapacity = 0;
        void* mappedUniformMemory = nullptr;
        constexpr VkDeviceSize kSceneCameraUniformBytes = sizeof(CameraUniformStd140);
        EnsureMappedBufferCapacity(selection.physicalDevice,
                                   device,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   kSceneCameraUniformBytes,
                                   uniformBuffer,
                                   uniformBufferCapacity,
                                   mappedUniformMemory,
                                   "vkMapMemory(uniform)");

        std::unordered_map<int, CachedGpuMesh> meshCache{};
        const void* cachedSceneIdentity = nullptr;

        const VkDescriptorPoolSize cameraPoolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
        };
        const VkDescriptorPoolCreateInfo cameraPoolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &cameraPoolSize,
        };
        VkDescriptorPool cameraDescriptorPool = VK_NULL_HANDLE;
        ExpectVk(vkCreateDescriptorPool(device, &cameraPoolInfo, nullptr, &cameraDescriptorPool), "vkCreateDescriptorPool(camera)");

        const VkDescriptorSetAllocateInfo cameraSetAllocateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = cameraDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &cameraSetLayout,
        };
        VkDescriptorSet cameraDescriptorSet = VK_NULL_HANDLE;
        ExpectVk(vkAllocateDescriptorSets(device, &cameraSetAllocateInfo, &cameraDescriptorSet), "vkAllocateDescriptorSets(camera)");

        const VkDescriptorBufferInfo descriptorBufferInfo{
            .buffer = uniformBuffer.buffer,
            .offset = 0,
            .range = kSceneCameraUniformBytes,
        };
        const VkWriteDescriptorSet writeCamera{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = cameraDescriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &descriptorBufferInfo,
        };
        vkUpdateDescriptorSets(device, 1, &writeCamera, 0, nullptr);

        BufferResource skyUniformBuffer{};
        VkDeviceSize skyUniformBufferCapacity = 0;
        void* mappedSkyUniformMemory = nullptr;
        EnsureMappedBufferCapacity(selection.physicalDevice,
                                   device,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   sizeof(SkyUniformStd140),
                                   skyUniformBuffer,
                                   skyUniformBufferCapacity,
                                   mappedSkyUniformMemory,
                                   "vkMapMemory(sky-uniform)");

        const VkDescriptorPoolSize skyCameraPoolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
        };
        const VkDescriptorPoolCreateInfo skyCameraPoolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &skyCameraPoolSize,
        };
        VkDescriptorPool skyDescriptorPool = VK_NULL_HANDLE;
        ExpectVk(vkCreateDescriptorPool(device, &skyCameraPoolInfo, nullptr, &skyDescriptorPool),
                 "vkCreateDescriptorPool(sky-camera)");

        const VkDescriptorSetAllocateInfo skySetAllocateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = skyDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &skyCameraSetLayout,
        };
        VkDescriptorSet skyDescriptorSet = VK_NULL_HANDLE;
        ExpectVk(vkAllocateDescriptorSets(device, &skySetAllocateInfo, &skyDescriptorSet), "vkAllocateDescriptorSets(sky-camera)");

        const VkDescriptorBufferInfo skyDescriptorBufferInfo{
            .buffer = skyUniformBuffer.buffer,
            .offset = 0,
            .range = sizeof(SkyUniformStd140),
        };
        const VkWriteDescriptorSet writeSkyCamera{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = skyDescriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &skyDescriptorBufferInfo,
        };
        vkUpdateDescriptorSets(device, 1, &writeSkyCamera, 0, nullptr);

        const VkCommandPoolCreateInfo commandPoolInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = selection.graphicsQueueFamily,
        };
        VkCommandPool commandPool = VK_NULL_HANDLE;
        ExpectVk(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool), "vkCreateCommandPool");

        const VkSamplerCreateInfo samplerInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .anisotropyEnable = maxSamplerAnisotropy > 1.0f ? VK_TRUE : VK_FALSE,
            .maxAnisotropy = maxSamplerAnisotropy,
            .maxLod = VK_LOD_CLAMP_NONE,
        };
        VkSampler linearSampler = VK_NULL_HANDLE;
        ExpectVk(vkCreateSampler(device, &samplerInfo, nullptr, &linearSampler), "vkCreateSampler(albedo)");

        if (enableHybridHdr) {
            VkSamplerCreateInfo depthNearestSamplerInfo = samplerInfo;
            depthNearestSamplerInfo.magFilter = VK_FILTER_NEAREST;
            depthNearestSamplerInfo.minFilter = VK_FILTER_NEAREST;
            depthNearestSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            ExpectVk(vkCreateSampler(device, &depthNearestSamplerInfo, nullptr, &hybridDepthSamplerNearest),
                     "vkCreateSampler(hybrid-depth-nearest)");

            SubmitOneTimeCommands(device, commandPool, graphicsQueue, [&](VkCommandBuffer uploadCommandBuffer) {
                TransitionImageLayout(uploadCommandBuffer,
                                      fakeMotionBlurHistory.image,
                                      VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      0,
                                      VK_ACCESS_TRANSFER_WRITE_BIT,
                                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT);
                const VkClearColorValue clearBlack{{0.0f, 0.0f, 0.0f, 0.0f}};
                const VkImageSubresourceRange clearRange{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                };
                vkCmdClearColorImage(uploadCommandBuffer,
                                     fakeMotionBlurHistory.image,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     &clearBlack,
                                     1,
                                     &clearRange);
                TransitionImageLayout(uploadCommandBuffer,
                                      fakeMotionBlurHistory.image,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                      VK_ACCESS_TRANSFER_WRITE_BIT,
                                      VK_ACCESS_SHADER_READ_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            });

            const std::array<VkDescriptorPoolSize, 1> hybridBundlePoolSizes{VkDescriptorPoolSize{
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 4,
            }};
            const VkDescriptorPoolCreateInfo hybridBundlePoolInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .maxSets = 1,
                .poolSizeCount = static_cast<std::uint32_t>(hybridBundlePoolSizes.size()),
                .pPoolSizes = hybridBundlePoolSizes.data(),
            };
            ExpectVk(vkCreateDescriptorPool(device, &hybridBundlePoolInfo, nullptr, &hybridBundleDescriptorPool),
                     "vkCreateDescriptorPool(hybrid-bundle)");

            const VkDescriptorSetAllocateInfo hybridBundleAllocateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = hybridBundleDescriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &hybridBundleTexturesSetLayout,
            };
            ExpectVk(vkAllocateDescriptorSets(device, &hybridBundleAllocateInfo, &hybridBundleDescriptorSet),
                     "vkAllocateDescriptorSets(hybrid-bundle)");

            const VkDescriptorImageInfo hdrForwardPassImageInfo{
                .sampler = linearSampler,
                .imageView = hdrSceneColorImage.view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            const VkDescriptorImageInfo sceneDepthImageInfo{
                .sampler = hybridDepthSamplerNearest,
                .imageView = depthImage.view,
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            };
            const VkDescriptorImageInfo sceneNormalRoughnessImageInfo{
                .sampler = linearSampler,
                .imageView = gbufferNormalRoughnessImage.view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            const VkDescriptorImageInfo sceneMaterialImageInfo{
                .sampler = linearSampler,
                .imageView = gbufferMaterialImage.view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            const std::array<VkWriteDescriptorSet, 4> writeHybridBundle = {{
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = hybridBundleDescriptorSet,
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &hdrForwardPassImageInfo,
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = hybridBundleDescriptorSet,
                    .dstBinding = 1,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &sceneDepthImageInfo,
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = hybridBundleDescriptorSet,
                    .dstBinding = 2,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &sceneNormalRoughnessImageInfo,
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = hybridBundleDescriptorSet,
                    .dstBinding = 3,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &sceneMaterialImageInfo,
                },
            }};
            vkUpdateDescriptorSets(device,
                                   static_cast<std::uint32_t>(writeHybridBundle.size()),
                                   writeHybridBundle.data(),
                                   0,
                                   nullptr);

            const VkDescriptorPoolSize compositePoolSize{
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 3,
            };
            const VkDescriptorPoolCreateInfo compositePoolInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .maxSets = 1,
                .poolSizeCount = 1,
                .pPoolSizes = &compositePoolSize,
            };
            ExpectVk(vkCreateDescriptorPool(device, &compositePoolInfo, nullptr, &compositeDescriptorPool),
                     "vkCreateDescriptorPool(composite-hdr)");
            const VkDescriptorSetAllocateInfo compositeAllocateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = compositeDescriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &compositeHdrTextureSetLayout,
            };
            ExpectVk(vkAllocateDescriptorSets(device, &compositeAllocateInfo, &compositeHdrDescriptorSet),
                     "vkAllocateDescriptorSets(composite-hdr)");
            const VkDescriptorImageInfo hybridBundleOutImageInfo{
                .sampler = linearSampler,
                .imageView = hybridBundleHdrImage.view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            const VkDescriptorImageInfo compositeDepthImageInfo{
                .sampler = hybridDepthSamplerNearest,
                .imageView = depthImage.view,
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            };
            const VkDescriptorImageInfo fakeMotionBlurHistoryImageInfo{
                .sampler = linearSampler,
                .imageView = fakeMotionBlurHistory.view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            const std::array<VkWriteDescriptorSet, 3> writeCompositeHdr = {{
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = compositeHdrDescriptorSet,
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &hybridBundleOutImageInfo,
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = compositeHdrDescriptorSet,
                    .dstBinding = 1,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &compositeDepthImageInfo,
                },
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = compositeHdrDescriptorSet,
                    .dstBinding = 2,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &fakeMotionBlurHistoryImageInfo,
                },
            }};
            vkUpdateDescriptorSets(device,
                                   static_cast<std::uint32_t>(writeCompositeHdr.size()),
                                   writeCompositeHdr.data(),
                                   0,
                                   nullptr);
        }

        VkSamplerCreateInfo shadowSamplerInfo{};
        shadowSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        shadowSamplerInfo.magFilter = VK_FILTER_NEAREST;
        shadowSamplerInfo.minFilter = VK_FILTER_NEAREST;
        shadowSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        shadowSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        shadowSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        shadowSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        shadowSamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        shadowSamplerInfo.compareEnable = VK_FALSE;
        shadowSamplerInfo.maxLod = 0.0f;
        VkSampler shadowSampler = VK_NULL_HANDLE;
        ExpectVk(vkCreateSampler(device, &shadowSamplerInfo, nullptr, &shadowSampler), "vkCreateSampler(shadow)");

        const VkDescriptorPoolSize shadowPoolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
        };
        const VkDescriptorPoolCreateInfo shadowPoolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &shadowPoolSize,
        };
        VkDescriptorPool shadowDescriptorPool = VK_NULL_HANDLE;
        ExpectVk(vkCreateDescriptorPool(device, &shadowPoolInfo, nullptr, &shadowDescriptorPool),
                 "vkCreateDescriptorPool(shadow)");
        const VkDescriptorSetAllocateInfo shadowSetAllocateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = shadowDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &shadowSetLayout,
        };
        VkDescriptorSet shadowDescriptorSet = VK_NULL_HANDLE;
        ExpectVk(vkAllocateDescriptorSets(device, &shadowSetAllocateInfo, &shadowDescriptorSet),
                 "vkAllocateDescriptorSets(shadow)");
        const VkDescriptorImageInfo shadowDescriptorImageInfo{
            .sampler = shadowSampler,
            .imageView = shadowDepthImage.view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        };
        const VkWriteDescriptorSet writeShadow{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = shadowDescriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &shadowDescriptorImageInfo,
        };
        vkUpdateDescriptorSets(device, 1, &writeShadow, 0, nullptr);

        // Outdoor / scatter scenes (Wilderness Ruins) easily exceed 512 unique material
        // texture combinations once bushes, rocks, and conifer billboards are instanced.
        constexpr std::uint32_t kMaxTextureSets = 2048U;
        const VkDescriptorPoolSize texturePoolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = kMaxTextureSets * 6U,
        };
        const VkDescriptorPoolCreateInfo texturePoolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = kMaxTextureSets,
            .poolSizeCount = 1,
            .pPoolSizes = &texturePoolSize,
        };
        VkDescriptorPool textureDescriptorPool = VK_NULL_HANDLE;
        ExpectVk(vkCreateDescriptorPool(device, &texturePoolInfo, nullptr, &textureDescriptorPool),
                 "vkCreateDescriptorPool(texture)");

        NativeAlbedoTextureCache textureCache{};
        textureCache.initialize(selection.physicalDevice,
                                device,
                                commandPool,
                                graphicsQueue,
                                selection.graphicsQueueFamily,
                                textureSetLayout,
                                textureDescriptorPool,
                                kMaxTextureSets,
                                linearSampler);
        const fs::path nativePostTextureRoot = ResolveVulkanNativePostTextureDirectory();
        const fs::path sweetFxLayerTexturePath = nativePostTextureRoot / "Layer.png";
        const fs::path sweetFxSmaaAreaTexturePath = nativePostTextureRoot / "AreaTex.png";
        const fs::path sweetFxSmaaSearchTexturePath = nativePostTextureRoot / "SearchTex.png";
        const fs::path reshadeLutTexturePath = nativePostTextureRoot / "lut.png";
        const fs::path barbatosLutTexturePath = nativePostTextureRoot / "Barbatos" / "Barbatos_LUT_Atlas.png";
        const std::array<fs::path, 6> nativePostTexturePaths = {
            nativePostTextureRoot / "PD80" / "pd80_bluenoise_rgba.png",
            nativePostTextureRoot / "PD80" / "pd80_permtexture.png",
            nativePostTextureRoot / "PD80" / "pd80_cinelut.png",
            nativePostTextureRoot / "MagicBloom_Dirt.png",
            nativePostTextureRoot / "brussell" / "UIDetectMaskRGB.png",
            nativePostTextureRoot / "FontAtlas.png",
        };
        if (nativePostTextureRoot.empty()) {
            ri::core::LogInfo("Vulkan native post texture bundle unavailable; texture-backed post effects use safe fallbacks.");
        } else {
            ri::core::LogInfo("Vulkan native post textures: " + nativePostTextureRoot.generic_string());
        }

        CachedGpuMesh skyMesh = CreateStaticUnitCubeGpuMesh(selection.physicalDevice, device);

        std::vector<VkCommandBuffer> commandBuffers(framebuffers.size(), VK_NULL_HANDLE);
        const VkCommandBufferAllocateInfo commandBufferInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = static_cast<std::uint32_t>(commandBuffers.size()),
        };
        ExpectVk(vkAllocateCommandBuffers(device, &commandBufferInfo, commandBuffers.data()), "vkAllocateCommandBuffers");

        constexpr std::uint32_t kFramesInFlight = 2U;
        const VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        const VkFenceCreateInfo fenceInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        std::array<VkSemaphore, kFramesInFlight> imageAvailable{};
        std::array<VkFence, kFramesInFlight> inFlightFences{};
        for (std::uint32_t frameIndex = 0; frameIndex < kFramesInFlight; ++frameIndex) {
            ExpectVk(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailable[frameIndex]),
                     "vkCreateSemaphore(imageAvailable)");
            ExpectVk(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[frameIndex]), "vkCreateFence");
        }
        // One renderFinished semaphore per swapchain image (not per frame in flight): the
        // presentation engine may still wait on the semaphore after our frame fence signals,
        // so reusing a per-frame semaphore two frames later races with the outstanding present.
        std::vector<VkSemaphore> renderFinished(commandBuffers.size(), VK_NULL_HANDLE);
        for (VkSemaphore& semaphore : renderFinished) {
            ExpectVk(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore),
                     "vkCreateSemaphore(renderFinished)");
        }
        std::vector<VkFence> imageInFlight(commandBuffers.size(), VK_NULL_HANDLE);
        std::uint32_t currentFrame = 0U;
        std::uint64_t lastFrameSequence = 0;
        bool hasPresentedFrame = false;
        // Roughly ten seconds at the 16 ms retry cadence before declaring the swapchain dead.
        constexpr std::uint32_t kMaxConsecutiveOutOfDateFrames = 600U;
        std::uint32_t consecutiveOutOfDateFrames = 0U;

        if (!usingExistingClient && options.showWindow) {
            ShowWindow(hwnd, SW_SHOW);
            UpdateWindow(hwnd);
        }

        while (windowState.running) {
            if (!usingExistingClient) {
                MSG message{};
                while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
                if (!windowState.running) {
                    break;
                }
            } else if (!IsWindow(hwnd)) {
                break;
            }

            VulkanNativeSceneFrame frame{};
            std::string frameError;
            if (!buildFrame(frame, &frameError)) {
                throw std::runtime_error(frameError.empty() ? "Native Vulkan frame callback failed." : frameError);
            }
            if (hasPresentedFrame && frame.frameSequence == lastFrameSequence) {
                Sleep(33);
                continue;
            }
            if (options.shaderPresentation.loaded) {
                ri::render::ApplyShaderConfig(frame.postProcess, options.shaderPresentation);
            }
            if (frame.textureRoot.empty() && !options.textureRoot.empty()) {
                frame.textureRoot = options.textureRoot;
            }

            NativeScenePreviewData sceneData{};
            if (!BuildNativeScenePreviewData(frame, width, height, &sceneData, &frameError, kShadowMapResolution)) {
                throw std::runtime_error(frameError.empty() ? "Failed to build native Vulkan scene data." : frameError);
            }
            if (options.enableHybridHdrPresentation) {
                sceneData.postProcessSecondary[3] = 1.0f;
                ri::render::vulkan::ApplyHybridHdrPresentationSafety(sceneData.presentationTuning,
                                                                     sceneData.renderQualityTier);
            }
            if (sceneData.scene == nullptr) {
                throw std::runtime_error("Native Vulkan scene data did not include a scene pointer.");
            }
            const void* sceneIdentity =
                frame.sceneCacheIdentity != nullptr ? frame.sceneCacheIdentity : sceneData.scene;
            if (cachedSceneIdentity != sceneIdentity) {
                ClearGpuMeshCache(device, meshCache);
                cachedSceneIdentity = sceneIdentity;
                textureCache.warmForScene(
                    *sceneData.scene, sceneData.draws, sceneData.textureRoot, options.warmupCache);
            }
            for (const NativeSceneDraw& draw : sceneData.draws) {
                if (draw.meshHandle >= 0) {
                    EnsureGpuMeshCached(selection.physicalDevice, device, *sceneData.scene, draw.meshHandle, meshCache);
                }
            }

            CameraUniformStd140 cameraUniform{};
            std::memcpy(cameraUniform.viewProjection, sceneData.viewProjection.data(), sizeof(cameraUniform.viewProjection));
            std::memcpy(cameraUniform.cameraWorldPosition,
                        sceneData.cameraWorldPosition.data(),
                        sizeof(cameraUniform.cameraWorldPosition));
            std::memcpy(cameraUniform.renderTuning, sceneData.renderTuning.data(), sizeof(cameraUniform.renderTuning));
            std::memcpy(cameraUniform.postProcessPrimary,
                        sceneData.postProcessPrimary.data(),
                        sizeof(cameraUniform.postProcessPrimary));
            std::memcpy(cameraUniform.postProcessTint, sceneData.postProcessTint.data(), sizeof(cameraUniform.postProcessTint));
            std::memcpy(cameraUniform.postProcessSecondary,
                        sceneData.postProcessSecondary.data(),
                        sizeof(cameraUniform.postProcessSecondary));
            std::memcpy(cameraUniform.lightViewProjection,
                        sceneData.lightViewProjection.data(),
                        sizeof(cameraUniform.lightViewProjection));
            std::memcpy(cameraUniform.lightDirectionIntensity,
                        sceneData.lightDirectionIntensity.data(),
                        sizeof(cameraUniform.lightDirectionIntensity));
            std::memcpy(cameraUniform.localLightPositionRange,
                        sceneData.localLightPositionRange.data(),
                        sizeof(cameraUniform.localLightPositionRange));
            std::memcpy(cameraUniform.localLightColorIntensity,
                        sceneData.localLightColorIntensity.data(),
                        sizeof(cameraUniform.localLightColorIntensity));
            std::memcpy(cameraUniform.directionalLightColorIntensity,
                        sceneData.directionalLightColorIntensity.data(),
                        sizeof(cameraUniform.directionalLightColorIntensity));
            std::memcpy(cameraUniform.viewportMetrics, sceneData.viewportMetrics.data(), sizeof(cameraUniform.viewportMetrics));
            std::memcpy(cameraUniform.presentationTuning,
                        sceneData.presentationTuning.data(),
                        sizeof(cameraUniform.presentationTuning));
            std::memcpy(cameraUniform.presentationColorGrading,
                        sceneData.presentationColorGrading.data(),
                        sizeof(cameraUniform.presentationColorGrading));
            std::memcpy(cameraUniform.presentationExtra,
                        sceneData.presentationExtra.data(),
                        sizeof(cameraUniform.presentationExtra));
            std::memcpy(cameraUniform.lggLiftMix, sceneData.lggLiftMix.data(), sizeof(cameraUniform.lggLiftMix));
            std::memcpy(cameraUniform.lggGammaRgb, sceneData.lggGammaRgb.data(), sizeof(cameraUniform.lggGammaRgb));
            std::memcpy(cameraUniform.lggGainRgb, sceneData.lggGainRgb.data(), sizeof(cameraUniform.lggGainRgb));
            std::memcpy(cameraUniform.vibranceBalanceAmount,
                        sceneData.vibranceBalanceAmount.data(),
                        sizeof(cameraUniform.vibranceBalanceAmount));
            std::memcpy(cameraUniform.technicolor1PowStrNegRg,
                        sceneData.technicolor1PowStrNegRg.data(),
                        sizeof(cameraUniform.technicolor1PowStrNegRg));
            std::memcpy(cameraUniform.technicolor1NegBPad,
                        sceneData.technicolor1NegBPad.data(),
                        sizeof(cameraUniform.technicolor1NegBPad));
            std::memcpy(cameraUniform.technicolor2ColBright,
                        sceneData.technicolor2ColBright.data(),
                        sizeof(cameraUniform.technicolor2ColBright));
            std::memcpy(cameraUniform.technicolor2SatStrPad,
                        sceneData.technicolor2SatStrPad.data(),
                        sizeof(cameraUniform.technicolor2SatStrPad));
            std::memcpy(cameraUniform.sepiaTintXyzStrength,
                        sceneData.sepiaTintXyzStrength.data(),
                        sizeof(cameraUniform.sepiaTintXyzStrength));
            std::memcpy(cameraUniform.monochromePresetSat,
                        sceneData.monochromePresetSat.data(),
                        sizeof(cameraUniform.monochromePresetSat));
            std::memcpy(cameraUniform.monochromeCustomCoeff,
                        sceneData.monochromeCustomCoeff.data(),
                        sizeof(cameraUniform.monochromeCustomCoeff));
            std::memcpy(cameraUniform.dpxRgbCurvePad,
                        sceneData.dpxRgbCurvePad.data(),
                        sizeof(cameraUniform.dpxRgbCurvePad));
            std::memcpy(cameraUniform.dpxRgbCPad,
                        sceneData.dpxRgbCPad.data(),
                        sizeof(cameraUniform.dpxRgbCPad));
            std::memcpy(cameraUniform.dpxContrastSatColorStr,
                        sceneData.dpxContrastSatColorStr.data(),
                        sizeof(cameraUniform.dpxContrastSatColorStr));
            std::memcpy(cameraUniform.colorMatrixRowR,
                        sceneData.colorMatrixRowR.data(),
                        sizeof(cameraUniform.colorMatrixRowR));
            std::memcpy(cameraUniform.colorMatrixRowG,
                        sceneData.colorMatrixRowG.data(),
                        sizeof(cameraUniform.colorMatrixRowG));
            std::memcpy(cameraUniform.colorMatrixRowBStr,
                        sceneData.colorMatrixRowBStr.data(),
                        sizeof(cameraUniform.colorMatrixRowBStr));
            std::memcpy(cameraUniform.fakeHdrPowerR1R2Str,
                        sceneData.fakeHdrPowerR1R2Str.data(),
                        sizeof(cameraUniform.fakeHdrPowerR1R2Str));
            std::memcpy(cameraUniform.levelsBlackWhiteStrClip,
                        sceneData.levelsBlackWhiteStrClip.data(),
                        sizeof(cameraUniform.levelsBlackWhiteStrClip));
            std::memcpy(cameraUniform.lumaSharpenPack,
                        sceneData.lumaSharpenPack.data(),
                        sizeof(cameraUniform.lumaSharpenPack));
            std::memcpy(cameraUniform.sweetFxCurvesPack,
                        sceneData.sweetFxCurvesPack.data(),
                        sizeof(cameraUniform.sweetFxCurvesPack));
            std::memcpy(cameraUniform.sweetFxChromaticAberrationPack,
                        sceneData.sweetFxChromaticAberrationPack.data(),
                        sizeof(cameraUniform.sweetFxChromaticAberrationPack));
            std::memcpy(cameraUniform.sweetFxBorderPack,
                        sceneData.sweetFxBorderPack.data(),
                        sizeof(cameraUniform.sweetFxBorderPack));
            std::memcpy(cameraUniform.sweetFxBorderColorPad,
                        sceneData.sweetFxBorderColorPad.data(),
                        sizeof(cameraUniform.sweetFxBorderColorPad));
            std::memcpy(cameraUniform.sweetFxCartoonPack,
                        sceneData.sweetFxCartoonPack.data(),
                        sizeof(cameraUniform.sweetFxCartoonPack));
            std::memcpy(cameraUniform.sweetFxTonemapGammaExpSatBleach,
                        sceneData.sweetFxTonemapGammaExpSatBleach.data(),
                        sizeof(cameraUniform.sweetFxTonemapGammaExpSatBleach));
            std::memcpy(cameraUniform.sweetFxTonemapFogColorDefog,
                        sceneData.sweetFxTonemapFogColorDefog.data(),
                        sizeof(cameraUniform.sweetFxTonemapFogColorDefog));
            std::memcpy(cameraUniform.sweetFxTonemapStrengthPad,
                        sceneData.sweetFxTonemapStrengthPad.data(),
                        sizeof(cameraUniform.sweetFxTonemapStrengthPad));
            std::memcpy(cameraUniform.sweetFxSplitscreenModeStrength,
                        sceneData.sweetFxSplitscreenModeStrength.data(),
                        sizeof(cameraUniform.sweetFxSplitscreenModeStrength));
            std::memcpy(cameraUniform.sweetFxNostalgiaPack,
                        sceneData.sweetFxNostalgiaPack.data(),
                        sizeof(cameraUniform.sweetFxNostalgiaPack));
            std::memcpy(cameraUniform.sweetFxComparePack,
                        sceneData.sweetFxComparePack.data(),
                        sizeof(cameraUniform.sweetFxComparePack));
            std::memcpy(cameraUniform.sweetFxLayerPosScaleBlend,
                        sceneData.sweetFxLayerPosScaleBlend.data(),
                        sizeof(cameraUniform.sweetFxLayerPosScaleBlend));
            std::memcpy(cameraUniform.sweetFxLayerTexSizePad,
                        sceneData.sweetFxLayerTexSizePad.data(),
                        sizeof(cameraUniform.sweetFxLayerTexSizePad));
            std::memcpy(cameraUniform.sweetFxFxaaPack,
                        sceneData.sweetFxFxaaPack.data(),
                        sizeof(cameraUniform.sweetFxFxaaPack));
            std::memcpy(cameraUniform.sweetFxCrtPack0,
                        sceneData.sweetFxCrtPack0.data(),
                        sizeof(cameraUniform.sweetFxCrtPack0));
            std::memcpy(cameraUniform.sweetFxCrtPack1,
                        sceneData.sweetFxCrtPack1.data(),
                        sizeof(cameraUniform.sweetFxCrtPack1));
            std::memcpy(cameraUniform.sweetFxCrtPack2,
                        sceneData.sweetFxCrtPack2.data(),
                        sizeof(cameraUniform.sweetFxCrtPack2));
            std::memcpy(cameraUniform.sweetFxCrtPack3,
                        sceneData.sweetFxCrtPack3.data(),
                        sizeof(cameraUniform.sweetFxCrtPack3));
            std::memcpy(cameraUniform.sweetFxAsciiPack0,
                        sceneData.sweetFxAsciiPack0.data(),
                        sizeof(cameraUniform.sweetFxAsciiPack0));
            std::memcpy(cameraUniform.sweetFxAsciiPack1,
                        sceneData.sweetFxAsciiPack1.data(),
                        sizeof(cameraUniform.sweetFxAsciiPack1));
            std::memcpy(cameraUniform.sweetFxAsciiPack2,
                        sceneData.sweetFxAsciiPack2.data(),
                        sizeof(cameraUniform.sweetFxAsciiPack2));
            std::memcpy(cameraUniform.sweetFxAsciiFontColorPad,
                        sceneData.sweetFxAsciiFontColorPad.data(),
                        sizeof(cameraUniform.sweetFxAsciiFontColorPad));
            std::memcpy(cameraUniform.sweetFxAsciiBackgroundColorPad,
                        sceneData.sweetFxAsciiBackgroundColorPad.data(),
                        sizeof(cameraUniform.sweetFxAsciiBackgroundColorPad));
            std::memcpy(cameraUniform.sweetFxSmaaPack0,
                        sceneData.sweetFxSmaaPack0.data(),
                        sizeof(cameraUniform.sweetFxSmaaPack0));
            std::memcpy(cameraUniform.sweetFxSmaaPack1,
                        sceneData.sweetFxSmaaPack1.data(),
                        sizeof(cameraUniform.sweetFxSmaaPack1));
            std::memcpy(cameraUniform.reshadeDaltonizePack,
                        sceneData.reshadeDaltonizePack.data(),
                        sizeof(cameraUniform.reshadeDaltonizePack));
            std::memcpy(cameraUniform.reshadeDisplayDepthPack,
                        sceneData.reshadeDisplayDepthPack.data(),
                        sizeof(cameraUniform.reshadeDisplayDepthPack));
            std::memcpy(cameraUniform.reshadeLutPack,
                        sceneData.reshadeLutPack.data(),
                        sizeof(cameraUniform.reshadeLutPack));
            std::memcpy(cameraUniform.pd80TcRedStrPad,
                        sceneData.pd80TcRedStrPad.data(),
                        sizeof(cameraUniform.pd80TcRedStrPad));
            std::memcpy(cameraUniform.pd80TcCyanPad,
                        sceneData.pd80TcCyanPad.data(),
                        sizeof(cameraUniform.pd80TcCyanPad));
            std::memcpy(cameraUniform.pd80TcKeySat2Pad,
                        sceneData.pd80TcKeySat2Pad.data(),
                        sizeof(cameraUniform.pd80TcKeySat2Pad));
            std::memcpy(cameraUniform.pd80Tc3ColBrightPad,
                        sceneData.pd80Tc3ColBrightPad.data(),
                        sizeof(cameraUniform.pd80Tc3ColBrightPad));
            std::memcpy(cameraUniform.pd80Tc3SatStrEnPad,
                        sceneData.pd80Tc3SatStrEnPad.data(),
                        sizeof(cameraUniform.pd80Tc3SatStrEnPad));
            std::memcpy(cameraUniform.pd80ColorTempKelvinLumMixStr,
                        sceneData.pd80ColorTempKelvinLumMixStr.data(),
                        sizeof(cameraUniform.pd80ColorTempKelvinLumMixStr));
            std::memcpy(cameraUniform.pd80SatLimitCapStr,
                        sceneData.pd80SatLimitCapStr.data(),
                        sizeof(cameraUniform.pd80SatLimitCapStr));
            std::memcpy(cameraUniform.pd80ColorBalanceShadowPad,
                        sceneData.pd80ColorBalanceShadowPad.data(),
                        sizeof(cameraUniform.pd80ColorBalanceShadowPad));
            std::memcpy(cameraUniform.pd80ColorBalanceMidPad,
                        sceneData.pd80ColorBalanceMidPad.data(),
                        sizeof(cameraUniform.pd80ColorBalanceMidPad));
            std::memcpy(cameraUniform.pd80ColorBalanceHighPad,
                        sceneData.pd80ColorBalanceHighPad.data(),
                        sizeof(cameraUniform.pd80ColorBalanceHighPad));
            std::memcpy(cameraUniform.pd80ColorBalanceOptStr,
                        sceneData.pd80ColorBalanceOptStr.data(),
                        sizeof(cameraUniform.pd80ColorBalanceOptStr));
            std::memcpy(cameraUniform.pd80ColorIsolationHueRangeSatMix,
                        sceneData.pd80ColorIsolationHueRangeSatMix.data(),
                        sizeof(cameraUniform.pd80ColorIsolationHueRangeSatMix));
            std::memcpy(cameraUniform.pd80ColorIsolationStrPad,
                        sceneData.pd80ColorIsolationStrPad.data(),
                        sizeof(cameraUniform.pd80ColorIsolationStrPad));
            std::memcpy(cameraUniform.pd80LevelsIbPad,
                        sceneData.pd80LevelsIbPad.data(),
                        sizeof(cameraUniform.pd80LevelsIbPad));
            std::memcpy(cameraUniform.pd80LevelsIwPad,
                        sceneData.pd80LevelsIwPad.data(),
                        sizeof(cameraUniform.pd80LevelsIwPad));
            std::memcpy(cameraUniform.pd80LevelsObPad,
                        sceneData.pd80LevelsObPad.data(),
                        sizeof(cameraUniform.pd80LevelsObPad));
            std::memcpy(cameraUniform.pd80LevelsOwPad,
                        sceneData.pd80LevelsOwPad.data(),
                        sizeof(cameraUniform.pd80LevelsOwPad));
            std::memcpy(cameraUniform.pd80LevelsGammaDitherStr,
                        sceneData.pd80LevelsGammaDitherStr.data(),
                        sizeof(cameraUniform.pd80LevelsGammaDitherStr));
            std::memcpy(cameraUniform.pd80BwPack0, sceneData.pd80BwPack0.data(), sizeof(cameraUniform.pd80BwPack0));
            std::memcpy(cameraUniform.pd80BwPack1, sceneData.pd80BwPack1.data(), sizeof(cameraUniform.pd80BwPack1));
            std::memcpy(cameraUniform.pd80BwPack2, sceneData.pd80BwPack2.data(), sizeof(cameraUniform.pd80BwPack2));
            std::memcpy(cameraUniform.pd80BwPack3, sceneData.pd80BwPack3.data(), sizeof(cameraUniform.pd80BwPack3));
            std::memcpy(cameraUniform.pd80CbsPack0, sceneData.pd80CbsPack0.data(), sizeof(cameraUniform.pd80CbsPack0));
            std::memcpy(cameraUniform.pd80CbsPack1, sceneData.pd80CbsPack1.data(), sizeof(cameraUniform.pd80CbsPack1));
            std::memcpy(cameraUniform.pd80CbsPack2, sceneData.pd80CbsPack2.data(), sizeof(cameraUniform.pd80CbsPack2));
            std::memcpy(cameraUniform.pd80CbsPack3, sceneData.pd80CbsPack3.data(), sizeof(cameraUniform.pd80CbsPack3));
            std::memcpy(cameraUniform.pd80CbsPack4, sceneData.pd80CbsPack4.data(), sizeof(cameraUniform.pd80CbsPack4));
            std::memcpy(cameraUniform.pd80CbsPack5, sceneData.pd80CbsPack5.data(), sizeof(cameraUniform.pd80CbsPack5));
            std::memcpy(cameraUniform.pd80CbsPack6, sceneData.pd80CbsPack6.data(), sizeof(cameraUniform.pd80CbsPack6));
            std::memcpy(cameraUniform.pd80CbsPack7, sceneData.pd80CbsPack7.data(), sizeof(cameraUniform.pd80CbsPack7));
            std::memcpy(cameraUniform.pd80CaPack0, sceneData.pd80CaPack0.data(), sizeof(cameraUniform.pd80CaPack0));
            std::memcpy(cameraUniform.pd80CaPack1, sceneData.pd80CaPack1.data(), sizeof(cameraUniform.pd80CaPack1));
            std::memcpy(cameraUniform.pd80CaPack2, sceneData.pd80CaPack2.data(), sizeof(cameraUniform.pd80CaPack2));
            std::memcpy(cameraUniform.pd80CaPack3, sceneData.pd80CaPack3.data(), sizeof(cameraUniform.pd80CaPack3));
            std::memcpy(cameraUniform.pd80CaPack4, sceneData.pd80CaPack4.data(), sizeof(cameraUniform.pd80CaPack4));
            std::memcpy(cameraUniform.pd80CaPack5, sceneData.pd80CaPack5.data(), sizeof(cameraUniform.pd80CaPack5));
            std::memcpy(cameraUniform.pd80LsPack0, sceneData.pd80LsPack0.data(), sizeof(cameraUniform.pd80LsPack0));
            std::memcpy(cameraUniform.pd80LsPack1, sceneData.pd80LsPack1.data(), sizeof(cameraUniform.pd80LsPack1));
            std::memcpy(cameraUniform.pd80LsPack2, sceneData.pd80LsPack2.data(), sizeof(cameraUniform.pd80LsPack2));
            std::memcpy(cameraUniform.pd80FgPack0, sceneData.pd80FgPack0.data(), sizeof(cameraUniform.pd80FgPack0));
            std::memcpy(cameraUniform.pd80FgPack1, sceneData.pd80FgPack1.data(), sizeof(cameraUniform.pd80FgPack1));
            std::memcpy(cameraUniform.pd80FgPack2, sceneData.pd80FgPack2.data(), sizeof(cameraUniform.pd80FgPack2));
            std::memcpy(cameraUniform.pd80FgPack3, sceneData.pd80FgPack3.data(), sizeof(cameraUniform.pd80FgPack3));
            std::memcpy(cameraUniform.pd80FgPack4, sceneData.pd80FgPack4.data(), sizeof(cameraUniform.pd80FgPack4));
            std::memcpy(cameraUniform.pd80DsPack0, sceneData.pd80DsPack0.data(), sizeof(cameraUniform.pd80DsPack0));
            std::memcpy(cameraUniform.pd80DsPack1, sceneData.pd80DsPack1.data(), sizeof(cameraUniform.pd80DsPack1));
            std::memcpy(cameraUniform.pd80DsPack2, sceneData.pd80DsPack2.data(), sizeof(cameraUniform.pd80DsPack2));
            std::memcpy(cameraUniform.pd80CgPack0, sceneData.pd80CgPack0.data(), sizeof(cameraUniform.pd80CgPack0));
            std::memcpy(cameraUniform.pd80CscPack0, sceneData.pd80CscPack0.data(), sizeof(cameraUniform.pd80CscPack0));
            std::memcpy(cameraUniform.pd80CscPack1, sceneData.pd80CscPack1.data(), sizeof(cameraUniform.pd80CscPack1));
            std::memcpy(cameraUniform.pd80CscPack2, sceneData.pd80CscPack2.data(), sizeof(cameraUniform.pd80CscPack2));
            std::memcpy(cameraUniform.pd80SmhPack0, sceneData.pd80SmhPack0.data(), sizeof(cameraUniform.pd80SmhPack0));
            std::memcpy(cameraUniform.pd80SmhPack1, sceneData.pd80SmhPack1.data(), sizeof(cameraUniform.pd80SmhPack1));
            std::memcpy(cameraUniform.pd80SmhPack2, sceneData.pd80SmhPack2.data(), sizeof(cameraUniform.pd80SmhPack2));
            std::memcpy(cameraUniform.pd80SmhPack3, sceneData.pd80SmhPack3.data(), sizeof(cameraUniform.pd80SmhPack3));
            std::memcpy(cameraUniform.pd80SmhPack4, sceneData.pd80SmhPack4.data(), sizeof(cameraUniform.pd80SmhPack4));
            std::memcpy(cameraUniform.pd80SmhPack5, sceneData.pd80SmhPack5.data(), sizeof(cameraUniform.pd80SmhPack5));
            std::memcpy(cameraUniform.pd80SmhPack6, sceneData.pd80SmhPack6.data(), sizeof(cameraUniform.pd80SmhPack6));
            std::memcpy(cameraUniform.pd80SmhPack7, sceneData.pd80SmhPack7.data(), sizeof(cameraUniform.pd80SmhPack7));
            std::memcpy(cameraUniform.pd80SmhPack8, sceneData.pd80SmhPack8.data(), sizeof(cameraUniform.pd80SmhPack8));
            std::memcpy(cameraUniform.pd80SmhPack9, sceneData.pd80SmhPack9.data(), sizeof(cameraUniform.pd80SmhPack9));
            std::memcpy(cameraUniform.pd80SmhPack10, sceneData.pd80SmhPack10.data(), sizeof(cameraUniform.pd80SmhPack10));
            std::memcpy(cameraUniform.pd80ClPack0, sceneData.pd80ClPack0.data(), sizeof(cameraUniform.pd80ClPack0));
            std::memcpy(cameraUniform.pd80ClPack1, sceneData.pd80ClPack1.data(), sizeof(cameraUniform.pd80ClPack1));
            std::memcpy(cameraUniform.pd80ClPack2, sceneData.pd80ClPack2.data(), sizeof(cameraUniform.pd80ClPack2));
            std::memcpy(cameraUniform.pd80ClPack3, sceneData.pd80ClPack3.data(), sizeof(cameraUniform.pd80ClPack3));
            std::memcpy(cameraUniform.pd80ClPack4, sceneData.pd80ClPack4.data(), sizeof(cameraUniform.pd80ClPack4));
            std::memcpy(cameraUniform.pd80ClPack5, sceneData.pd80ClPack5.data(), sizeof(cameraUniform.pd80ClPack5));
            std::memcpy(cameraUniform.pd80ClPack6, sceneData.pd80ClPack6.data(), sizeof(cameraUniform.pd80ClPack6));
            std::memcpy(cameraUniform.pd80ClPack7, sceneData.pd80ClPack7.data(), sizeof(cameraUniform.pd80ClPack7));
            std::memcpy(cameraUniform.pd80ClPack8, sceneData.pd80ClPack8.data(), sizeof(cameraUniform.pd80ClPack8));
            std::memcpy(cameraUniform.pd80ScPack0, sceneData.pd80ScPack0.data(), sizeof(cameraUniform.pd80ScPack0));
            std::memcpy(cameraUniform.pd80ScPack1, sceneData.pd80ScPack1.data(), sizeof(cameraUniform.pd80ScPack1));
            std::memcpy(cameraUniform.pd80ScPack2, sceneData.pd80ScPack2.data(), sizeof(cameraUniform.pd80ScPack2));
            std::memcpy(cameraUniform.pd80ScPack3, sceneData.pd80ScPack3.data(), sizeof(cameraUniform.pd80ScPack3));
            std::memcpy(cameraUniform.pd80ScPack4, sceneData.pd80ScPack4.data(), sizeof(cameraUniform.pd80ScPack4));
            std::memcpy(cameraUniform.pd80ScPack5, sceneData.pd80ScPack5.data(), sizeof(cameraUniform.pd80ScPack5));
            std::memcpy(cameraUniform.pd80ScPack6, sceneData.pd80ScPack6.data(), sizeof(cameraUniform.pd80ScPack6));
            std::memcpy(cameraUniform.pd80ScPack7, sceneData.pd80ScPack7.data(), sizeof(cameraUniform.pd80ScPack7));
            std::memcpy(cameraUniform.pd80ScPack8, sceneData.pd80ScPack8.data(), sizeof(cameraUniform.pd80ScPack8));
            std::memcpy(cameraUniform.pd80ScPack9, sceneData.pd80ScPack9.data(), sizeof(cameraUniform.pd80ScPack9));
            std::memcpy(cameraUniform.pd80ScPack10, sceneData.pd80ScPack10.data(), sizeof(cameraUniform.pd80ScPack10));
            std::memcpy(cameraUniform.pd80ScPack11, sceneData.pd80ScPack11.data(), sizeof(cameraUniform.pd80ScPack11));
            std::memcpy(cameraUniform.pd80ScPack12, sceneData.pd80ScPack12.data(), sizeof(cameraUniform.pd80ScPack12));
            std::memcpy(cameraUniform.pd80ScPack13, sceneData.pd80ScPack13.data(), sizeof(cameraUniform.pd80ScPack13));
            std::memcpy(cameraUniform.pd80ScPack14, sceneData.pd80ScPack14.data(), sizeof(cameraUniform.pd80ScPack14));
            std::memcpy(cameraUniform.pd80ScPack15, sceneData.pd80ScPack15.data(), sizeof(cameraUniform.pd80ScPack15));
            std::memcpy(cameraUniform.pd80ScPack16, sceneData.pd80ScPack16.data(), sizeof(cameraUniform.pd80ScPack16));
            std::memcpy(cameraUniform.pd80ScPack17, sceneData.pd80ScPack17.data(), sizeof(cameraUniform.pd80ScPack17));
            std::memcpy(cameraUniform.pd80ScPack18, sceneData.pd80ScPack18.data(), sizeof(cameraUniform.pd80ScPack18));
            std::memcpy(cameraUniform.pd80PpPack0, sceneData.pd80PpPack0.data(), sizeof(cameraUniform.pd80PpPack0));
            std::memcpy(cameraUniform.pd80PpPack1, sceneData.pd80PpPack1.data(), sizeof(cameraUniform.pd80PpPack1));
            std::memcpy(cameraUniform.pd80MrPack0, sceneData.pd80MrPack0.data(), sizeof(cameraUniform.pd80MrPack0));
            std::memcpy(cameraUniform.pd80MrPack1, sceneData.pd80MrPack1.data(), sizeof(cameraUniform.pd80MrPack1));
            std::memcpy(cameraUniform.pd80MrPack2, sceneData.pd80MrPack2.data(), sizeof(cameraUniform.pd80MrPack2));
            std::memcpy(cameraUniform.pd80MrPack3, sceneData.pd80MrPack3.data(), sizeof(cameraUniform.pd80MrPack3));
            std::memcpy(cameraUniform.pd80MrPack4, sceneData.pd80MrPack4.data(), sizeof(cameraUniform.pd80MrPack4));
            std::memcpy(cameraUniform.pd80MrPack5, sceneData.pd80MrPack5.data(), sizeof(cameraUniform.pd80MrPack5));
            std::memcpy(cameraUniform.pd80MrPack6, sceneData.pd80MrPack6.data(), sizeof(cameraUniform.pd80MrPack6));
            std::memcpy(cameraUniform.pd80MrPack7, sceneData.pd80MrPack7.data(), sizeof(cameraUniform.pd80MrPack7));
            std::memcpy(cameraUniform.pd80BlpPack0, sceneData.pd80BlpPack0.data(), sizeof(cameraUniform.pd80BlpPack0));
            std::memcpy(cameraUniform.pd80BlpPack1, sceneData.pd80BlpPack1.data(), sizeof(cameraUniform.pd80BlpPack1));
            std::memcpy(cameraUniform.pd80BlpPack2, sceneData.pd80BlpPack2.data(), sizeof(cameraUniform.pd80BlpPack2));
            std::memcpy(cameraUniform.pd80BlpPack3, sceneData.pd80BlpPack3.data(), sizeof(cameraUniform.pd80BlpPack3));
            std::memcpy(cameraUniform.pd80BlpPack4, sceneData.pd80BlpPack4.data(), sizeof(cameraUniform.pd80BlpPack4));
            std::memcpy(cameraUniform.pd80BlpPack5, sceneData.pd80BlpPack5.data(), sizeof(cameraUniform.pd80BlpPack5));
            std::memcpy(cameraUniform.pd80CltPack0, sceneData.pd80CltPack0.data(), sizeof(cameraUniform.pd80CltPack0));
            std::memcpy(cameraUniform.pd80CltPack1, sceneData.pd80CltPack1.data(), sizeof(cameraUniform.pd80CltPack1));
            std::memcpy(cameraUniform.pd80CltPack2, sceneData.pd80CltPack2.data(), sizeof(cameraUniform.pd80CltPack2));
            std::memcpy(cameraUniform.pd80CltPack3, sceneData.pd80CltPack3.data(), sizeof(cameraUniform.pd80CltPack3));
            std::memcpy(cameraUniform.pd80CltPack4, sceneData.pd80CltPack4.data(), sizeof(cameraUniform.pd80CltPack4));
            std::memcpy(cameraUniform.pd80CltPack5, sceneData.pd80CltPack5.data(), sizeof(cameraUniform.pd80CltPack5));
            std::memcpy(cameraUniform.pd80LcPack0, sceneData.pd80LcPack0.data(), sizeof(cameraUniform.pd80LcPack0));
            std::memcpy(cameraUniform.pd80LfPack0, sceneData.pd80LfPack0.data(), sizeof(cameraUniform.pd80LfPack0));
            std::memcpy(cameraUniform.pd80Cg4Pack0, sceneData.pd80Cg4Pack0.data(), sizeof(cameraUniform.pd80Cg4Pack0));
            std::memcpy(cameraUniform.pd80Cg4Pack1, sceneData.pd80Cg4Pack1.data(), sizeof(cameraUniform.pd80Cg4Pack1));
            std::memcpy(cameraUniform.pd80Cg4Pack2, sceneData.pd80Cg4Pack2.data(), sizeof(cameraUniform.pd80Cg4Pack2));
            std::memcpy(cameraUniform.pd80Cg4Pack3, sceneData.pd80Cg4Pack3.data(), sizeof(cameraUniform.pd80Cg4Pack3));
            std::memcpy(cameraUniform.pd80Cg4Pack4, sceneData.pd80Cg4Pack4.data(), sizeof(cameraUniform.pd80Cg4Pack4));
            std::memcpy(cameraUniform.pd80Cg4Pack5, sceneData.pd80Cg4Pack5.data(), sizeof(cameraUniform.pd80Cg4Pack5));
            std::memcpy(cameraUniform.pd80Cg4Pack6, sceneData.pd80Cg4Pack6.data(), sizeof(cameraUniform.pd80Cg4Pack6));
            std::memcpy(cameraUniform.pd80Cg4Pack7, sceneData.pd80Cg4Pack7.data(), sizeof(cameraUniform.pd80Cg4Pack7));
            std::memcpy(cameraUniform.pd80Cg4Pack8, sceneData.pd80Cg4Pack8.data(), sizeof(cameraUniform.pd80Cg4Pack8));
            std::memcpy(cameraUniform.pd80CcPack0, sceneData.pd80CcPack0.data(), sizeof(cameraUniform.pd80CcPack0));
            std::memcpy(cameraUniform.pd80RccPack0, sceneData.pd80RccPack0.data(), sizeof(cameraUniform.pd80RccPack0));
            std::memcpy(cameraUniform.pd80RccPack1, sceneData.pd80RccPack1.data(), sizeof(cameraUniform.pd80RccPack1));
            std::memcpy(cameraUniform.pd80RccPack2, sceneData.pd80RccPack2.data(), sizeof(cameraUniform.pd80RccPack2));
            std::memcpy(cameraUniform.pd80RccPack3, sceneData.pd80RccPack3.data(), sizeof(cameraUniform.pd80RccPack3));
            std::memcpy(cameraUniform.pd80RccPack4, sceneData.pd80RccPack4.data(), sizeof(cameraUniform.pd80RccPack4));
            std::memcpy(cameraUniform.pd80FaPack0, sceneData.pd80FaPack0.data(), sizeof(cameraUniform.pd80FaPack0));
            std::memcpy(cameraUniform.pd80HbPack0, sceneData.pd80HbPack0.data(), sizeof(cameraUniform.pd80HbPack0));
            std::memcpy(cameraUniform.pd80HbPack1, sceneData.pd80HbPack1.data(), sizeof(cameraUniform.pd80HbPack1));
            std::memcpy(cameraUniform.pd80HbPack2, sceneData.pd80HbPack2.data(), sizeof(cameraUniform.pd80HbPack2));
            std::memcpy(cameraUniform.pd80Sc2Pack0, sceneData.pd80Sc2Pack0.data(), sizeof(cameraUniform.pd80Sc2Pack0));
            std::memcpy(cameraUniform.creatorColourfulnessPack,
                        sceneData.creatorColourfulnessPack.data(),
                        sizeof(cameraUniform.creatorColourfulnessPack));
            std::memcpy(cameraUniform.creatorFilmicPassPack,
                        sceneData.creatorFilmicPassPack.data(),
                        sizeof(cameraUniform.creatorFilmicPassPack));
            std::memcpy(cameraUniform.creatorFilmGrain2Pack,
                        sceneData.creatorFilmGrain2Pack.data(),
                        sizeof(cameraUniform.creatorFilmGrain2Pack));
            std::memcpy(cameraUniform.creatorDenoisePack,
                        sceneData.creatorDenoisePack.data(),
                        sizeof(cameraUniform.creatorDenoisePack));
            std::memcpy(cameraUniform.creatorDenoisePack2,
                        sceneData.creatorDenoisePack2.data(),
                        sizeof(cameraUniform.creatorDenoisePack2));
            std::memcpy(cameraUniform.creatorAdaptiveSharpenPack0,
                        sceneData.creatorAdaptiveSharpenPack0.data(),
                        sizeof(cameraUniform.creatorAdaptiveSharpenPack0));
            std::memcpy(cameraUniform.creatorAdaptiveSharpenPack1,
                        sceneData.creatorAdaptiveSharpenPack1.data(),
                        sizeof(cameraUniform.creatorAdaptiveSharpenPack1));
            std::memcpy(cameraUniform.creatorAdaptiveSharpenPack2,
                        sceneData.creatorAdaptiveSharpenPack2.data(),
                        sizeof(cameraUniform.creatorAdaptiveSharpenPack2));
            std::memcpy(cameraUniform.creatorGaussianBlurPack,
                        sceneData.creatorGaussianBlurPack.data(),
                        sizeof(cameraUniform.creatorGaussianBlurPack));
            std::memcpy(cameraUniform.creatorFineSharpPack0,
                        sceneData.creatorFineSharpPack0.data(),
                        sizeof(cameraUniform.creatorFineSharpPack0));
            std::memcpy(cameraUniform.creatorFineSharpPack1,
                        sceneData.creatorFineSharpPack1.data(),
                        sizeof(cameraUniform.creatorFineSharpPack1));
            std::memcpy(cameraUniform.creatorMartyBloomPack0,
                        sceneData.creatorMartyBloomPack0.data(),
                        sizeof(cameraUniform.creatorMartyBloomPack0));
            std::memcpy(cameraUniform.creatorMartyBloomPack1,
                        sceneData.creatorMartyBloomPack1.data(),
                        sizeof(cameraUniform.creatorMartyBloomPack1));
            std::memcpy(cameraUniform.creatorDofPack0,
                        sceneData.creatorDofPack0.data(),
                        sizeof(cameraUniform.creatorDofPack0));
            std::memcpy(cameraUniform.creatorDofPack1,
                        sceneData.creatorDofPack1.data(),
                        sizeof(cameraUniform.creatorDofPack1));
            std::memcpy(cameraUniform.creatorDofPack2,
                        sceneData.creatorDofPack2.data(),
                        sizeof(cameraUniform.creatorDofPack2));
            std::memcpy(cameraUniform.creatorDofPack3,
                        sceneData.creatorDofPack3.data(),
                        sizeof(cameraUniform.creatorDofPack3));
            std::memcpy(cameraUniform.creatorDofPack4,
                        sceneData.creatorDofPack4.data(),
                        sizeof(cameraUniform.creatorDofPack4));
            std::memcpy(cameraUniform.creatorAmbientLightPack0,
                        sceneData.creatorAmbientLightPack0.data(),
                        sizeof(cameraUniform.creatorAmbientLightPack0));
            std::memcpy(cameraUniform.creatorAmbientLightPack1,
                        sceneData.creatorAmbientLightPack1.data(),
                        sizeof(cameraUniform.creatorAmbientLightPack1));
            std::memcpy(cameraUniform.creatorAmbientLightPack2,
                        sceneData.creatorAmbientLightPack2.data(),
                        sizeof(cameraUniform.creatorAmbientLightPack2));
            std::memcpy(cameraUniform.creatorFakeMotionBlurPack0,
                        sceneData.creatorFakeMotionBlurPack0.data(),
                        sizeof(cameraUniform.creatorFakeMotionBlurPack0));
            std::memcpy(cameraUniform.creatorReflectiveBumpMappingPack0,
                        sceneData.creatorReflectiveBumpMappingPack0.data(),
                        sizeof(cameraUniform.creatorReflectiveBumpMappingPack0));
            std::memcpy(cameraUniform.creatorReflectiveBumpMappingPack1,
                        sceneData.creatorReflectiveBumpMappingPack1.data(),
                        sizeof(cameraUniform.creatorReflectiveBumpMappingPack1));
            std::memcpy(cameraUniform.creatorReflectiveBumpMappingPack2,
                        sceneData.creatorReflectiveBumpMappingPack2.data(),
                        sizeof(cameraUniform.creatorReflectiveBumpMappingPack2));
            std::memcpy(cameraUniform.creatorReflectiveBumpMappingPack3,
                        sceneData.creatorReflectiveBumpMappingPack3.data(),
                        sizeof(cameraUniform.creatorReflectiveBumpMappingPack3));
            std::memcpy(cameraUniform.cropScaleContentIntermediate,
                        sceneData.cropScaleContentIntermediate.data(),
                        sizeof(cameraUniform.cropScaleContentIntermediate));
            std::memcpy(cameraUniform.cropScaleFinalFilterStrength,
                        sceneData.cropScaleFinalFilterStrength.data(),
                        sizeof(cameraUniform.cropScaleFinalFilterStrength));
            std::memcpy(cameraUniform.barbatosFakeHdrPack,
                        sceneData.barbatosFakeHdrPack.data(),
                        sizeof(cameraUniform.barbatosFakeHdrPack));
            std::memcpy(cameraUniform.riAdaptiveDebandPack,
                        sceneData.riAdaptiveDebandPack.data(),
                        sizeof(cameraUniform.riAdaptiveDebandPack));
            std::memcpy(cameraUniform.riLocalSharpenPack,
                        sceneData.riLocalSharpenPack.data(),
                        sizeof(cameraUniform.riLocalSharpenPack));
            std::memcpy(cameraUniform.riOutlinePack0,
                        sceneData.riOutlinePack0.data(),
                        sizeof(cameraUniform.riOutlinePack0));
            std::memcpy(cameraUniform.riOutlineColorMethod,
                        sceneData.riOutlineColorMethod.data(),
                        sizeof(cameraUniform.riOutlineColorMethod));
            std::memcpy(cameraUniform.riOutlineWobbleDebug,
                        sceneData.riOutlineWobbleDebug.data(),
                        sizeof(cameraUniform.riOutlineWobbleDebug));
            std::memcpy(cameraUniform.riSignalGlitchPack,
                        sceneData.riSignalGlitchPack.data(),
                        sizeof(cameraUniform.riSignalGlitchPack));
            std::memcpy(cameraUniform.riNightVisionPack,
                        sceneData.riNightVisionPack.data(),
                        sizeof(cameraUniform.riNightVisionPack));
            std::memcpy(cameraUniform.riHq4xPack0, sceneData.riHq4xPack0.data(), sizeof(cameraUniform.riHq4xPack0));
            std::memcpy(cameraUniform.riHq4xPack1, sceneData.riHq4xPack1.data(), sizeof(cameraUniform.riHq4xPack1));
            std::memcpy(cameraUniform.riHslAnchor0, sceneData.riHslAnchor0.data(), sizeof(cameraUniform.riHslAnchor0));
            std::memcpy(cameraUniform.riHslAnchor1, sceneData.riHslAnchor1.data(), sizeof(cameraUniform.riHslAnchor1));
            std::memcpy(cameraUniform.riHslAnchor2, sceneData.riHslAnchor2.data(), sizeof(cameraUniform.riHslAnchor2));
            std::memcpy(cameraUniform.riHslAnchor3, sceneData.riHslAnchor3.data(), sizeof(cameraUniform.riHslAnchor3));
            std::memcpy(cameraUniform.riHslAnchor4, sceneData.riHslAnchor4.data(), sizeof(cameraUniform.riHslAnchor4));
            std::memcpy(cameraUniform.riHslAnchor5, sceneData.riHslAnchor5.data(), sizeof(cameraUniform.riHslAnchor5));
            std::memcpy(cameraUniform.riHslAnchor6, sceneData.riHslAnchor6.data(), sizeof(cameraUniform.riHslAnchor6));
            std::memcpy(cameraUniform.riHslAnchor7, sceneData.riHslAnchor7.data(), sizeof(cameraUniform.riHslAnchor7));
            std::memcpy(cameraUniform.riLevelsPlusPack0, sceneData.riLevelsPlusPack0.data(), sizeof(cameraUniform.riLevelsPlusPack0));
            std::memcpy(cameraUniform.riLevelsPlusPack1, sceneData.riLevelsPlusPack1.data(), sizeof(cameraUniform.riLevelsPlusPack1));
            std::memcpy(cameraUniform.riLevelsPlusPack2, sceneData.riLevelsPlusPack2.data(), sizeof(cameraUniform.riLevelsPlusPack2));
            std::memcpy(cameraUniform.riLevelsPlusPack3, sceneData.riLevelsPlusPack3.data(), sizeof(cameraUniform.riLevelsPlusPack3));
            std::memcpy(cameraUniform.riLevelsPlusPack4, sceneData.riLevelsPlusPack4.data(), sizeof(cameraUniform.riLevelsPlusPack4));
            std::memcpy(cameraUniform.riLevelsPlusPack5, sceneData.riLevelsPlusPack5.data(), sizeof(cameraUniform.riLevelsPlusPack5));
            std::memcpy(cameraUniform.riLevelsPlusPack6, sceneData.riLevelsPlusPack6.data(), sizeof(cameraUniform.riLevelsPlusPack6));
            std::memcpy(cameraUniform.riLightDofPack0, sceneData.riLightDofPack0.data(), sizeof(cameraUniform.riLightDofPack0));
            std::memcpy(cameraUniform.riLightDofPack1, sceneData.riLightDofPack1.data(), sizeof(cameraUniform.riLightDofPack1));
            std::memcpy(cameraUniform.riLightDofPack2, sceneData.riLightDofPack2.data(), sizeof(cameraUniform.riLightDofPack2));
            std::memcpy(cameraUniform.riMagicBloomPack0, sceneData.riMagicBloomPack0.data(), sizeof(cameraUniform.riMagicBloomPack0));
            std::memcpy(cameraUniform.riMagicBloomPack1, sceneData.riMagicBloomPack1.data(), sizeof(cameraUniform.riMagicBloomPack1));
            std::memcpy(cameraUniform.riUiMaskPack0, sceneData.riUiMaskPack0.data(), sizeof(cameraUniform.riUiMaskPack0));
            std::memcpy(cameraUniform.riUiMaskPack1, sceneData.riUiMaskPack1.data(), sizeof(cameraUniform.riUiMaskPack1));
            std::memcpy(cameraUniform.riLuminanceThresholdPack,
                        sceneData.riLuminanceThresholdPack.data(),
                        sizeof(cameraUniform.riLuminanceThresholdPack));
            std::memcpy(cameraUniform.riColorQuantizePack0,
                        sceneData.riColorQuantizePack0.data(),
                        sizeof(cameraUniform.riColorQuantizePack0));
            std::memcpy(cameraUniform.riColorQuantizePack1,
                        sceneData.riColorQuantizePack1.data(),
                        sizeof(cameraUniform.riColorQuantizePack1));
            std::memcpy(cameraUniform.riColorQuantizePack2,
                        sceneData.riColorQuantizePack2.data(),
                        sizeof(cameraUniform.riColorQuantizePack2));
            std::memcpy(cameraUniform.riKaleidoscopePack0,
                        sceneData.riKaleidoscopePack0.data(),
                        sizeof(cameraUniform.riKaleidoscopePack0));
            std::memcpy(cameraUniform.riKaleidoscopePack1,
                        sceneData.riKaleidoscopePack1.data(),
                        sizeof(cameraUniform.riKaleidoscopePack1));
            std::memcpy(mappedUniformMemory, &cameraUniform, sizeof(CameraUniformStd140));
            SkyUniformStd140 skyUniform{};
            skyUniform.hasSkyTexture = sceneData.skyUseTextureFile;
            skyUniform.useAuthoredGradient = sceneData.skyUseAuthoredGradient;
            std::memcpy(skyUniform.clipFromLocal, sceneData.skyClipFromLocal.data(), sizeof(skyUniform.clipFromLocal));
            std::memcpy(skyUniform.eyeToWorldRotation, sceneData.skyEyeToWorld.data(), sizeof(skyUniform.eyeToWorldRotation));
            std::memcpy(skyUniform.horizonColor, sceneData.skyHorizonColor.data(), sizeof(skyUniform.horizonColor));
            std::memcpy(skyUniform.zenithColor, sceneData.skyZenithColor.data(), sizeof(skyUniform.zenithColor));
            skyUniform.sunDirection[0] = sceneData.lightDirectionIntensity[0];
            skyUniform.sunDirection[1] = sceneData.lightDirectionIntensity[1];
            skyUniform.sunDirection[2] = sceneData.lightDirectionIntensity[2];
            skyUniform.sunDirection[3] = std::max(sceneData.lightDirectionIntensity[3], 0.0f);
            {
                const float sunTintMax = std::max({
                    sceneData.directionalLightColorIntensity[0],
                    sceneData.directionalLightColorIntensity[1],
                    sceneData.directionalLightColorIntensity[2],
                    1.0e-4f,
                });
                skyUniform.sunColor[0] = sceneData.directionalLightColorIntensity[0] / sunTintMax;
                skyUniform.sunColor[1] = sceneData.directionalLightColorIntensity[1] / sunTintMax;
                skyUniform.sunColor[2] = sceneData.directionalLightColorIntensity[2] / sunTintMax;
                skyUniform.sunColor[3] = 1.0f;
            }
            std::memcpy(mappedSkyUniformMemory, &skyUniform, sizeof(SkyUniformStd140));

            VkDescriptorSet skyTextureSet = textureCache.whiteDescriptorSet;
            if (sceneData.skyUseTextureFile != 0) {
                const VkDescriptorSet loaded = textureCache.descriptorForAbsolutePath(sceneData.skyEquirectAbsolute);
                if (loaded != VK_NULL_HANDLE) {
                    skyTextureSet = loaded;
                }
            }
            VkDescriptorSet sweetFxLayerTextureSet = textureCache.whiteDescriptorSet;
            VkDescriptorSet sweetFxSmaaAreaTextureSet = textureCache.whiteDescriptorSet;
            VkDescriptorSet sweetFxSmaaSearchTextureSet = textureCache.whiteDescriptorSet;
            VkDescriptorSet reshadeLutTextureSet = textureCache.whiteDescriptorSet;
            VkDescriptorSet barbatosLutTextureSet = textureCache.whiteDescriptorSet;
            VkDescriptorSet nativePostTextureSet = textureCache.whiteDescriptorSet;
            if (enableHybridHdr) {
                if (const VkDescriptorSet loaded = textureCache.descriptorForAbsolutePath(sweetFxLayerTexturePath);
                    loaded != VK_NULL_HANDLE) {
                    sweetFxLayerTextureSet = loaded;
                }
                if (const VkDescriptorSet loaded = textureCache.descriptorForAbsolutePath(sweetFxSmaaAreaTexturePath, false);
                    loaded != VK_NULL_HANDLE) {
                    sweetFxSmaaAreaTextureSet = loaded;
                }
                if (const VkDescriptorSet loaded = textureCache.descriptorForAbsolutePath(sweetFxSmaaSearchTexturePath, false);
                    loaded != VK_NULL_HANDLE) {
                    sweetFxSmaaSearchTextureSet = loaded;
                }
                if (const VkDescriptorSet loaded = textureCache.descriptorForAbsolutePath(reshadeLutTexturePath, false);
                    loaded != VK_NULL_HANDLE) {
                    reshadeLutTextureSet = loaded;
                }
                if (const VkDescriptorSet loaded = textureCache.descriptorForAbsolutePath(barbatosLutTexturePath, false);
                    loaded != VK_NULL_HANDLE) {
                    barbatosLutTextureSet = loaded;
                }
                if (const VkDescriptorSet loaded = textureCache.descriptorForNativePostBundle(nativePostTexturePaths);
                    loaded != VK_NULL_HANDLE) {
                    nativePostTextureSet = loaded;
                }
            }

            VkFence& frameFence = inFlightFences[currentFrame];
            ExpectVk(vkWaitForFences(device, 1, &frameFence, VK_TRUE, UINT64_MAX), "vkWaitForFences");

            std::uint32_t imageIndex = 0;
            VkResult acquireResult = vkAcquireNextImageKHR(
                device,
                swapchain,
                UINT64_MAX,
                imageAvailable[currentFrame],
                VK_NULL_HANDLE,
                &imageIndex);
            if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
                // The fixed-extent swapchain cannot recover in-loop; hosts (e.g. the editor
                // viewport) restart the loop on resize. Keep waiting while the window is
                // minimized, but fail with a clear error instead of spinning forever if the
                // swapchain stays out of date while the window is visible.
                if (IsIconic(hwnd) == 0) {
                    ++consecutiveOutOfDateFrames;
                    if (consecutiveOutOfDateFrames >= kMaxConsecutiveOutOfDateFrames) {
                        throw std::runtime_error(
                            "Vulkan swapchain stayed VK_ERROR_OUT_OF_DATE_KHR; the surface size "
                            "changed and the native preview loop must be restarted.");
                    }
                }
                Sleep(16);
                continue;
            }
            // VK_SUBOPTIMAL_KHR still delivers a usable image; treat it as success.
            if (acquireResult != VK_SUBOPTIMAL_KHR) {
                ExpectVk(acquireResult, "vkAcquireNextImageKHR");
            }
            consecutiveOutOfDateFrames = 0U;
            if (imageInFlight[imageIndex] != VK_NULL_HANDLE) {
                ExpectVk(vkWaitForFences(device, 1, &imageInFlight[imageIndex], VK_TRUE, UINT64_MAX),
                         "vkWaitForFences(image)");
            }
            imageInFlight[imageIndex] = frameFence;
            ExpectVk(vkResetFences(device, 1, &frameFence), "vkResetFences");
            ExpectVk(vkResetCommandBuffer(commandBuffers[imageIndex], 0), "vkResetCommandBuffer");
            RecordSceneCommandBuffer(commandBuffers[imageIndex],
                                     shadowRenderPass,
                                     shadowFramebuffer,
                                     VkExtent2D{kShadowMapResolution, kShadowMapResolution},
                                     shadowPipeline,
                                     shadowPipelineLayout,
                                     enableHybridHdr ? hdrSceneFramebuffer : framebuffers[imageIndex],
                                     enableHybridHdr ? hdrSceneRenderPass : renderPass,
                                     extent,
                                     enableHybridHdr ? skyPipelineHdr : skyPipeline,
                                     skyPipelineLayout,
                                     skyDescriptorSet,
                                     skyTextureSet,
                                     skyMesh,
                                     enableHybridHdr ? pipelineHdrScene : pipeline,
                                     enableHybridHdr ? pipelineHdrSceneTransparent : pipelineTransparent,
                                     enableHybridHdr ? pipelineHdrSceneAdditive : pipelineAdditive,
                                     pipelineLayout,
                                     cameraDescriptorSet,
                                     shadowDescriptorSet,
                                     textureCache,
                                     *sceneData.scene,
                                     meshCache,
                                     sceneData,
                                     enableHybridHdr ? compositeFramebuffers[imageIndex] : VK_NULL_HANDLE,
                                     enableHybridHdr ? compositeRenderPass : VK_NULL_HANDLE,
                                     enableHybridHdr ? compositePipeline : VK_NULL_HANDLE,
                                     enableHybridHdr ? compositePipelineLayout : VK_NULL_HANDLE,
                                     enableHybridHdr ? compositeHdrDescriptorSet : VK_NULL_HANDLE,
                                     enableHybridHdr ? sweetFxLayerTextureSet : VK_NULL_HANDLE,
                                     enableHybridHdr ? sweetFxSmaaAreaTextureSet : VK_NULL_HANDLE,
                                     enableHybridHdr ? sweetFxSmaaSearchTextureSet : VK_NULL_HANDLE,
                                     enableHybridHdr ? reshadeLutTextureSet : VK_NULL_HANDLE,
                                     enableHybridHdr ? barbatosLutTextureSet : VK_NULL_HANDLE,
                                     enableHybridHdr ? nativePostTextureSet : VK_NULL_HANDLE,
                                     enableHybridHdr ? hybridBundleFramebuffer : VK_NULL_HANDLE,
                                     enableHybridHdr ? hybridBundleRenderPass : VK_NULL_HANDLE,
                                     enableHybridHdr ? hybridBundlePipeline : VK_NULL_HANDLE,
                                     enableHybridHdr ? hybridBundlePipelineLayout : VK_NULL_HANDLE,
                                     enableHybridHdr ? hybridBundleDescriptorSet : VK_NULL_HANDLE,
                                     enableHybridHdr ? swapchainImages[imageIndex] : VK_NULL_HANDLE,
                                     enableHybridHdr ? fakeMotionBlurHistory.image : VK_NULL_HANDLE,
                                     sceneData.creatorFakeMotionBlurPack0[0]);

            const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            const VkSubmitInfo submitInfo{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &imageAvailable[currentFrame],
                .pWaitDstStageMask = &waitStage,
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffers[imageIndex],
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &renderFinished[imageIndex],
            };
            ExpectVk(vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameFence), "vkQueueSubmit");

            const VkPresentInfoKHR presentInfo{
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &renderFinished[imageIndex],
                .swapchainCount = 1,
                .pSwapchains = &swapchain,
                .pImageIndices = &imageIndex,
            };
            const VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
            if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
                Sleep(16);
                currentFrame = (currentFrame + 1U) % kFramesInFlight;
                continue;
            }
            // VK_SUBOPTIMAL_KHR means the frame was still presented; only other non-success
            // results are fatal.
            if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR) {
                ExpectVk(presentResult, "vkQueuePresentKHR");
            }
            lastFrameSequence = frame.frameSequence;
            hasPresentedFrame = true;
            currentFrame = (currentFrame + 1U) % kFramesInFlight;
        }

        vkDeviceWaitIdle(device);
        textureCache.destroy();
        for (std::uint32_t frameIndex = 0; frameIndex < kFramesInFlight; ++frameIndex) {
            vkDestroyFence(device, inFlightFences[frameIndex], nullptr);
            vkDestroySemaphore(device, imageAvailable[frameIndex], nullptr);
        }
        for (VkSemaphore semaphore : renderFinished) {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyDescriptorPool(device, cameraDescriptorPool, nullptr);
        vkDestroyDescriptorPool(device, shadowDescriptorPool, nullptr);
        vkDestroyDescriptorPool(device, skyDescriptorPool, nullptr);
        if (mappedUniformMemory != nullptr) {
            vkUnmapMemory(device, uniformBuffer.memory);
        }
        if (mappedSkyUniformMemory != nullptr) {
            vkUnmapMemory(device, skyUniformBuffer.memory);
        }
        vkDestroySampler(device, shadowSampler, nullptr);
        ClearGpuMeshCache(device, meshCache);
        DestroyBuffer(device, skyMesh.vertexBuffer);
        DestroyBuffer(device, skyMesh.indexBuffer);
        DestroyBuffer(device, uniformBuffer);
        DestroyBuffer(device, skyUniformBuffer);
        vkDestroyPipeline(device, shadowPipeline, nullptr);
        vkDestroyShaderModule(device, shadowFragShader, nullptr);
        vkDestroyShaderModule(device, shadowVertShader, nullptr);
        vkDestroyPipelineLayout(device, shadowPipelineLayout, nullptr);
        vkDestroyFramebuffer(device, shadowFramebuffer, nullptr);
        vkDestroyRenderPass(device, shadowRenderPass, nullptr);
        if (enableHybridHdr) {
            vkDestroySampler(device, hybridDepthSamplerNearest, nullptr);
            vkDestroyPipeline(device, hybridBundlePipeline, nullptr);
            vkDestroyShaderModule(device, hybridBundleFragShader, nullptr);
            vkDestroyShaderModule(device, hybridBundleVertShader, nullptr);
            vkDestroyDescriptorPool(device, hybridBundleDescriptorPool, nullptr);
            vkDestroyFramebuffer(device, hybridBundleFramebuffer, nullptr);
            vkDestroyRenderPass(device, hybridBundleRenderPass, nullptr);
            vkDestroyImageView(device, hybridBundleHdrImage.view, nullptr);
            vkDestroyImage(device, hybridBundleHdrImage.image, nullptr);
            vkFreeMemory(device, hybridBundleHdrImage.memory, nullptr);
            vkDestroyImageView(device, fakeMotionBlurHistory.view, nullptr);
            vkDestroyImage(device, fakeMotionBlurHistory.image, nullptr);
            vkFreeMemory(device, fakeMotionBlurHistory.memory, nullptr);
            vkDestroyPipelineLayout(device, hybridBundlePipelineLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, hybridBundleTexturesSetLayout, nullptr);
            vkDestroyPipeline(device, compositePipeline, nullptr);
            vkDestroyPipeline(device, skyPipelineHdr, nullptr);
            vkDestroyPipeline(device, pipelineHdrSceneAdditive, nullptr);
            vkDestroyPipeline(device, pipelineHdrSceneTransparent, nullptr);
            vkDestroyPipeline(device, pipelineHdrScene, nullptr);
            vkDestroyShaderModule(device, compositeFragShader, nullptr);
            vkDestroyShaderModule(device, compositeVertShader, nullptr);
            vkDestroyDescriptorPool(device, compositeDescriptorPool, nullptr);
            for (VkFramebuffer compositeFb : compositeFramebuffers) {
                if (compositeFb != VK_NULL_HANDLE) {
                    vkDestroyFramebuffer(device, compositeFb, nullptr);
                }
            }
            vkDestroyRenderPass(device, compositeRenderPass, nullptr);
            vkDestroyFramebuffer(device, hdrSceneFramebuffer, nullptr);
            vkDestroyRenderPass(device, hdrSceneRenderPass, nullptr);
            vkDestroyImageView(device, hdrSceneColorImage.view, nullptr);
            vkDestroyImage(device, hdrSceneColorImage.image, nullptr);
            vkFreeMemory(device, hdrSceneColorImage.memory, nullptr);
            vkDestroyPipelineLayout(device, compositePipelineLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, compositeHdrTextureSetLayout, nullptr);
        }
        vkDestroyPipeline(device, skyPipeline, nullptr);
        vkDestroyShaderModule(device, skyFragShader, nullptr);
        vkDestroyShaderModule(device, skyVertShader, nullptr);
        vkDestroyPipelineLayout(device, skyPipelineLayout, nullptr);
        vkDestroyPipeline(device, pipelineAdditive, nullptr);
        vkDestroyPipeline(device, pipelineTransparent, nullptr);
        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyShaderModule(device, fragShader, nullptr);
        vkDestroyShaderModule(device, vertShader, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, shadowSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, textureSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, skyCameraSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, cameraSetLayout, nullptr);
        for (VkFramebuffer framebuffer : framebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        vkDestroyRenderPass(device, renderPass, nullptr);
        vkDestroyImageView(device, gbufferMaterialImage.view, nullptr);
        vkDestroyImage(device, gbufferMaterialImage.image, nullptr);
        vkFreeMemory(device, gbufferMaterialImage.memory, nullptr);
        vkDestroyImageView(device, gbufferNormalRoughnessImage.view, nullptr);
        vkDestroyImage(device, gbufferNormalRoughnessImage.image, nullptr);
        vkFreeMemory(device, gbufferNormalRoughnessImage.memory, nullptr);
        vkDestroyImageView(device, depthImage.view, nullptr);
        vkDestroyImage(device, depthImage.image, nullptr);
        vkFreeMemory(device, depthImage.memory, nullptr);
        vkDestroyImageView(device, shadowDepthImage.view, nullptr);
        vkDestroyImage(device, shadowDepthImage.image, nullptr);
        vkFreeMemory(device, shadowDepthImage.memory, nullptr);
        for (VkImageView imageView : swapchainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        if (options.enablePersistentPipelineWarmupCache && pipelineCache != VK_NULL_HANDLE) {
            std::size_t pipelineCacheBytes = 0U;
            if (vkGetPipelineCacheData(device, pipelineCache, &pipelineCacheBytes, nullptr) == VK_SUCCESS
                && pipelineCacheBytes > 0U && pipelineCacheBytes <= kVulkanPipelineWarmupMaxBytes) {
                std::vector<std::uint8_t> pipelineCacheData(pipelineCacheBytes);
                VkResult cacheDataResult =
                    vkGetPipelineCacheData(device, pipelineCache, &pipelineCacheBytes, pipelineCacheData.data());
                if (cacheDataResult == VK_SUCCESS && pipelineCacheBytes > 0U) {
                    pipelineCacheData.resize(pipelineCacheBytes);
                    if (SaveVulkanPipelineWarmupBlob(
                            pipelineWarmupCachePath, pipelineCacheIdentity, pipelineCacheData)) {
                        ri::core::LogInfo(
                            "Vulkan pipeline warmup cache saved: " + std::to_string(pipelineCacheData.size()) + " bytes");
                    }
                }
            }
        }
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        return true;
    } catch (const std::exception& exception) {
        if (error != nullptr) {
            *error = exception.what();
        }
        return false;
    }
#else
    if (error != nullptr) {
        *error = "Native Vulkan scene preview is disabled because glslangValidator was not available at build time.";
    }
    (void)width;
    (void)height;
    (void)buildFrame;
    (void)options;
    return false;
#endif
}

bool PresentSceneKitPreviewWindowNative(const ri::scene::SceneKitPreview& preview,
                                        const int width,
                                        const int height,
                                        const VulkanPreviewWindowOptions& options,
                                        std::string* error) {
    const VulkanNativeSceneFrameCallback buildFrame =
        [&preview, &options](VulkanNativeSceneFrame& frame, std::string*) {
            frame.scene = &preview.scene;
            frame.cameraNode = preview.orbitCamera.cameraNode;
            frame.photoMode = options.scenePhotoMode;
            frame.photoModeEnabled = ri::scene::PhotoModeFieldOfViewActive(options.scenePhotoMode);
            frame.textureRoot = options.textureRoot;
            frame.animationTimeSeconds = static_cast<double>(GetTickCount64()) * 0.001;
            frame.postProcess.timeSeconds = static_cast<float>(frame.animationTimeSeconds);
            return true;
        };
    return RunVulkanNativeSceneLoop(width, height, buildFrame, options, error);
}

} // namespace ri::render::vulkan

#else

namespace ri::render::vulkan {

bool RunVulkanNativeSceneLoop(int,
                              int,
                              const VulkanNativeSceneFrameCallback&,
                              const VulkanPreviewWindowOptions&,
                              std::string* error) {
    if (error != nullptr) {
        *error = "Native Vulkan scene preview is currently only implemented on Windows.";
    }
    return false;
}

bool PresentSceneKitPreviewWindowNative(const ri::scene::SceneKitPreview&,
                                        int,
                                        int,
                                        const VulkanPreviewWindowOptions&,
                                        std::string* error) {
    if (error != nullptr) {
        *error = "Native Vulkan scene preview is currently only implemented on Windows.";
    }
    return false;
}

} // namespace ri::render::vulkan

#endif
