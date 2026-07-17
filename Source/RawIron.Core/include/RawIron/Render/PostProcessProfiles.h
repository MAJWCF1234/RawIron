#pragma once

#include "RawIron/Math/Vec2.h"
#include "RawIron/Math/Vec3.h"

#include <algorithm>
#include <utility>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace ri::render {

enum class PostProcessPreset {
    Neutral,
    CrispGameplay,
    SoftVhs,
    Vhs,
    ColdFacility,
    IndustrialHaze,
    DreamPulse,
    CombatFocus,
    AnalogHorror,
    StaticTransition,
};

struct PostProcessParameters {
    float timeSeconds = 0.0f;
    float noiseAmount = 0.0f;
    float scanlineAmount = 0.0f;
    float barrelDistortion = 0.0f;
    float chromaticAberration = 0.0f;
    ri::math::Vec3 tintColor{1.0f, 1.0f, 1.0f};
    float tintStrength = 0.0f;
    float blurAmount = 0.0f;
    float staticFadeAmount = 0.0f;
    /// Native Vulkan composite: FidelityFX-style CAS mix (0 = off, 1 = full sharpened output).
    float casSharpenAmount = 0.0f;
    /// Maps to SweetFX `Contrast` in CAS (0–1); higher = more contrast-adaptive sharpening.
    float casContrastAdaptation = 0.0f;
    /// HDR bloom add intensity (after CAS, before tonemap; scene-linear).
    float bloomIntensity = 0.0f;
    /// Linear HDR luminance threshold for bloom (scene-linear, after exposure in composite).
    float bloomThreshold = 0.65f;
    /// LDR deband after the stylized post chain; read from `presentationColorGrading.z` in composite.
    float debandStrength = 0.0f;
    /// SweetFX Curves–style luma S-curve (simplified Catmull–Rom, formula 4) mix amount 0–1.
    float toneCurveStrength = 0.0f;
    /// Triangular dither to 8-bit (TriDither.fxh-style), scaled 0–1; applied last in LDR.
    float outputDitherStrength = 0.0f;
    /// SweetFX Vignette type 0 style lens falloff (LDR, independent of barrel/vignette from `barrelDistortion`).
    float vignetteStrength = 0.0f;
    /// SweetFX FilmGrain-style multiplicative grain (LDR, after stylized post; 0 = off).
    float filmGrainIntensity = 0.0f;
    /// SweetFX LiftGammaGain — per-channel controls (UI range 0–2 in reference); identity is (1,1,1).
    ri::math::Vec3 liftRgb{1.0f, 1.0f, 1.0f};
    ri::math::Vec3 gammaRgb{1.0f, 1.0f, 1.0f};
    ri::math::Vec3 gainRgb{1.0f, 1.0f, 1.0f};
    /// Blends identity toward full LGG pass (0 = skip; creators enable per title/scene via cfg).
    float liftGammaGainMix = 0.0f;
    /// SweetFX Vibrance strength (-1 desaturate … +1 saturate); 0 skips.
    float vibrance = 0.0f;
    /// Per-channel multiplier on vibrance strength (reference default 1,1,1; UI up to 10).
    ri::math::Vec3 vibranceRgbBalance{1.0f, 1.0f, 1.0f};
    /// SweetFX Technicolor v1.1 (DKT70 / CeeJay) — distinct three-strip emulation (not Technicolor2).
    float technicolorPower = 4.0f;
    ri::math::Vec3 technicolorRgbNegative{0.88f, 0.88f, 0.88f};
    float technicolorStrength = 0.0f;
    /// SweetFX Technicolor2 v1.0 (Prod80 / CeeJay) — separate mathematics from Technicolor v1.
    ri::math::Vec3 technicolor2ColorStrength{0.2f, 0.2f, 0.2f};
    float technicolor2Brightness = 1.0f;
    float technicolor2Saturation = 1.0f;
    float technicolor2Strength = 0.0f;
    /// SweetFX Sepia.fx (technique `Tint`): `lerp(col, col * Tint * 2.55, Strength)` — separate from scene `tintColor`/`tintStrength`.
    ri::math::Vec3 sepiaTint{0.55f, 0.43f, 0.42f};
    float sepiaStrength = 0.0f;
    /// SweetFX Monochrome.fx v1.1 — preset 0 = custom coefficients; 1–17 fixed film weights; `mix(grey,color,sat)`: sat 1 = identity.
    int monochromePreset = 0;
    ri::math::Vec3 monochromeCustomCoeff{0.21f, 0.72f, 0.07f};
    float monochromeColorSaturation = 1.0f;
    /// SweetFX DPX.fx (Loadus) — sigmoid curve + XYZ/RGB film matrices; separate from scene contrast/tone curve.
    ri::math::Vec3 dpxRgbCurve{8.0f, 8.0f, 8.0f};
    ri::math::Vec3 dpxRgbC{0.36f, 0.36f, 0.34f};
    float dpxContrast = 0.1f;
    float dpxSaturation = 3.0f;
    float dpxColorfulness = 2.5f;
    float dpxStrength = 0.0f;
    /// SweetFX ColorMatrix.fx v1.0 — `lerp(c, M*c, Strength)`; rows = new R/G/B as mix of old RGB (not DPX/grade).
    ri::math::Vec3 colorMatrixRed{0.817f, 0.183f, 0.0f};
    ri::math::Vec3 colorMatrixGreen{0.333f, 0.667f, 0.0f};
    ri::math::Vec3 colorMatrixBlue{0.0f, 0.125f, 0.875f};
    float colorMatrixStrength = 0.0f;
    /// SweetFX FakeHDR.fx — `pow(|blend|,|HDRPower|)+HDR` with two neighbor rings (not engine HDR bloom).
    float fakeHdrPower = 1.30f;
    float fakeHdrRadius1 = 0.793f;
    float fakeHdrRadius2 = 0.87f;
    float fakeHdrStrength = 0.0f;
    /// SweetFX Levels.fx v1.2 — linear stretch black/white (0–255 UI); optional clipping debug overlay.
    float levelsBlackPoint = 16.0f;
    float levelsWhitePoint = 235.0f;
    float levelsStrength = 0.0f;
    float levelsClipHighlight = 0.0f;
    /// SweetFX LumaSharpen.fx v1.5 — luma unsharp mask (distinct from CAS); 0 strength skips.
    float lumaSharpenStrength = 0.0f;
    float lumaSharpenClamp = 0.035f;
    int lumaSharpenPattern = 1;
    float lumaSharpenOffsetBias = 1.0f;
    float lumaSharpenShowPattern = 0.0f;
    /// SweetFX Curves.fx — full formula set (not `toneCurveStrength` / simplified Catmull in CAS chain).
    int sweetFxCurvesMode = 0;
    int sweetFxCurvesFormula = 4;
    float sweetFxCurvesContrast = 0.65f;
    float sweetFxCurvesStrength = 0.0f;
    /// SweetFX ChromaticAberration.fx — `Shift` in pixels (reference ±10); `Strength` 0 = skip (distinct from radial `chromaticAberration`).
    float sweetFxChromaticAberrationShiftX = 2.5f;
    float sweetFxChromaticAberrationShiftY = -0.5f;
    float sweetFxChromaticAberrationStrength = 0.0f;
    /// SweetFX Border.fx v1.4.1 — pixel `border_width` or aspect `border_ratio`; `sweetFxBorderStrength` 0 = skip.
    float sweetFxBorderWidthX = 0.0f;
    float sweetFxBorderWidthY = 0.0f;
    float sweetFxBorderRatio = 2.35f;
    ri::math::Vec3 sweetFxBorderColor{0.0f, 0.0f, 0.0f};
    float sweetFxBorderStrength = 0.0f;
    /// SweetFX Cartoon.fx — edge darkening; `sweetFxCartoonStrength` 0 = skip.
    float sweetFxCartoonPower = 1.5f;
    float sweetFxCartoonEdgeSlope = 1.5f;
    float sweetFxCartoonStrength = 0.0f;
    /// SweetFX Tonemap.fx v1.1 — exposure/gamma/bleach/defog/saturation; `sweetFxTonemapStrength` 0 = skip.
    float sweetFxTonemapGamma = 1.0f;
    float sweetFxTonemapExposure = 0.0f;
    float sweetFxTonemapSaturation = 0.0f;
    float sweetFxTonemapBleach = 0.0f;
    float sweetFxTonemapDefog = 0.0f;
    ri::math::Vec3 sweetFxTonemapFogColor{0.0f, 0.0f, 1.0f};
    float sweetFxTonemapStrength = 0.0f;
    /// SweetFX Splitscreen.fx v2.0 — mode 0..6 compares pre/post stack regions; `sweetFxSplitscreenStrength` 0 = skip.
    int sweetFxSplitscreenMode = 0;
    float sweetFxSplitscreenStrength = 0.0f;
    /// SweetFX Nostalgia.fx v1.3 — palette quantization + optional dither + scanlines.
    int sweetFxNostalgiaPalette = 1;
    int sweetFxNostalgiaScanlines = 1;
    float sweetFxNostalgiaDither = 0.0f;
    float sweetFxNostalgiaStrength = 0.0f;
    /// SweetFX Compare.fx v1.0 — effect A/B compare modes plus signed/absolute difference.
    int sweetFxCompareMode = 7;
    float sweetFxCompareDifferenceScale = 5.0f;
    float sweetFxCompareStrength = 0.0f;
    /// SweetFX Layer.fx v0.2 — screen-space layer UV (`Layer_Pos`, `Layer_Scale`, `Layer_Blend`); texture size defaults match LAYER_SIZE.
    ri::math::Vec2 sweetFxLayerPosition{0.5f, 0.5f};
    float sweetFxLayerScale = 1.0f;
    float sweetFxLayerBlend = 0.0f;
    float sweetFxLayerTexWidth = 1280.0f;
    float sweetFxLayerTexHeight = 720.0f;
    /// SweetFX FXAA 3.11 — subpixel AA plus contrast/dark thresholds; distinct from engine sharpening.
    float sweetFxFxaaSubpix = 0.25f;
    float sweetFxFxaaEdgeThreshold = 0.125f;
    float sweetFxFxaaEdgeThresholdMin = 0.0f;
    float sweetFxFxaaStrength = 0.0f;
    /// SweetFX CRT.fx — cgwg/Themaister beam+curvature path; kept distinct from scanlines/noise.
    float sweetFxCrtAmount = 0.0f;
    float sweetFxCrtResolution = 1.15f;
    float sweetFxCrtGamma = 2.4f;
    float sweetFxCrtMonitorGamma = 2.2f;
    float sweetFxCrtBrightness = 0.9f;
    int sweetFxCrtScanlineIntensity = 2;
    float sweetFxCrtScanlineGaussian = 1.0f;
    float sweetFxCrtCurvature = 0.0f;
    float sweetFxCrtCurvatureRadius = 1.5f;
    float sweetFxCrtCornerSize = 0.01f;
    float sweetFxCrtViewerDistance = 2.0f;
    ri::math::Vec2 sweetFxCrtAngle{0.0f, 0.0f};
    float sweetFxCrtOverscan = 1.01f;
    float sweetFxCrtOversample = 1.0f;
    /// SweetFX Ascii.fx — bitfield glyph decode + optional noise dither.
    int sweetFxAsciiSpacing = 1;
    int sweetFxAsciiFont = 1;
    int sweetFxAsciiFontColorMode = 1;
    ri::math::Vec3 sweetFxAsciiFontColor{1.0f, 1.0f, 1.0f};
    ri::math::Vec3 sweetFxAsciiBackgroundColor{0.0f, 0.0f, 0.0f};
    float sweetFxAsciiSwapColors = 0.0f;
    float sweetFxAsciiInvertBrightness = 0.0f;
    float sweetFxAsciiDithering = 1.0f;
    float sweetFxAsciiDitheringIntensity = 2.0f;
    float sweetFxAsciiDitheringDebugGradient = 0.0f;
    float sweetFxAsciiStrength = 0.0f;
    /// SweetFX SMAA.fx — morphological edge detect + directional neighborhood blend.
    int sweetFxSmaaEdgeDetectionType = 1;
    float sweetFxSmaaEdgeThreshold = 0.10f;
    float sweetFxSmaaDepthThreshold = 0.01f;
    int sweetFxSmaaMaxSearchSteps = 32;
    int sweetFxSmaaMaxSearchStepsDiagonal = 16;
    int sweetFxSmaaCornerRounding = 25;
    float sweetFxSmaaDebugOutput = 0.0f;
    float sweetFxSmaaStrength = 0.0f;
    /// ReShade Daltonize.fx — LMS simulation/correction for color vision deficiency modes.
    int reshadeDaltonizeType = 0;
    float reshadeDaltonizeStrength = 0.0f;
    /// ReShade DisplayDepth.fx — depth/normal debug visualization modes.
    int reshadeDisplayDepthPresentType = 2;
    float reshadeDisplayDepthStrength = 0.0f;
    /// ReShade LUT.fx — 3D LUT strip remap with separate chroma/luma control.
    float reshadeLutAmountChroma = 1.0f;
    float reshadeLutAmountLuma = 1.0f;
    float reshadeLutStrength = 0.0f;
    /// PD80_04_Technicolor.fx — 2-strip dye + quaternion hue + optional 3-strip overlay (not `technicolor` / `technicolor2`).
    float pd80TechnicolorStrength = 0.0f;
    ri::math::Vec3 pd80TechnicolorRed2strip{1.0f, 0.098f, 0.0f};
    ri::math::Vec3 pd80TechnicolorCyan2strip{0.0f, 0.988f, 1.0f};
    ri::math::Vec3 pd80TechnicolorColorKey{1.0f, 1.0f, 1.0f};
    float pd80TechnicolorSaturation2 = 1.5f;
    float pd80TechnicolorEnable3strip = 0.0f;
    ri::math::Vec3 pd80Technicolor3ColorStrength{0.2f, 0.2f, 0.2f};
    float pd80Technicolor3Brightness = 1.0f;
    float pd80Technicolor3Saturation = 1.0f;
    float pd80Technicolor3Strength = 1.0f;
    /// PD80_04_Color_Temperature.fx — Kelvin tint × `kMix`, optional luminance preservation (Tanner Helland RGB).
    float pd80ColorTemperatureKelvin = 6500.0f;
    float pd80ColorTemperatureLuminancePreservation = 1.0f;
    float pd80ColorTemperatureMix = 1.0f;
    float pd80ColorTemperatureStrength = 0.0f;
    /// PD80_04_Saturation_Limit.fx — `min(HSL_S, saturation_limit)` (prod80 RGB↔HSL).
    float pd80SaturationLimit = 1.0f;
    float pd80SaturationLimitStrength = 0.0f;
    /// PD80_04_Color_Balance.fx — per-channel shadow/mid/highlight RGB shifts + separation curves + optional luma preservation.
    ri::math::Vec3 pd80ColorBalanceShadow{};
    ri::math::Vec3 pd80ColorBalanceMid{};
    ri::math::Vec3 pd80ColorBalanceHigh{};
    float pd80ColorBalancePreserveLuma = 1.0f;
    float pd80ColorBalanceSeparationMode = 0.0f;
    float pd80ColorBalanceStrength = 0.0f;
    /// PD80_04_Color_Isolation.fx — hue band + smootherstep weight + Rec.709 luma isolate.
    float pd80ColorIsolationHueMid = 0.0f;
    float pd80ColorIsolationHueRange = 0.167f;
    float pd80ColorIsolationSatLimit = 1.0f;
    float pd80ColorIsolationFxMix = 1.0f;
    float pd80ColorIsolationStrength = 0.0f;
    /// PD80_03_Levels.fx — `levels()` in/out/gamma (optional procedural dither stand-in for RGB noise).
    ri::math::Vec3 pd80LevelsBlackIn{};
    ri::math::Vec3 pd80LevelsWhiteIn{1.0f, 1.0f, 1.0f};
    ri::math::Vec3 pd80LevelsBlackOut{};
    ri::math::Vec3 pd80LevelsWhiteOut{1.0f, 1.0f, 1.0f};
    float pd80LevelsGamma = 1.0f;
    float pd80LevelsEnableDither = 1.0f;
    float pd80LevelsDitherStrength = 1.0f;
    float pd80LevelsStrength = 0.0f;
    /// PD80_04_BlacknWhite.fx — `ProcessBW` + iq `curve` + optional tint/clipping (noise via Hash21).
    float pd80BlackWhiteMode = 13.0f;
    float pd80BlackWhiteCurveStr = 1.5f;
    float pd80BlackWhiteEnableDither = 1.0f;
    float pd80BlackWhiteDitherStrength = 1.0f;
    float pd80BlackWhiteRedChannel = 0.2f;
    float pd80BlackWhiteYellowChannel = 0.4f;
    float pd80BlackWhiteGreenChannel = 0.6f;
    float pd80BlackWhiteCyanChannel = 0.0f;
    float pd80BlackWhiteBlueChannel = -0.6f;
    float pd80BlackWhiteMagentaChannel = -0.2f;
    float pd80BlackWhiteUseTint = 0.0f;
    float pd80BlackWhiteTintHue = 0.083f;
    float pd80BlackWhiteTintSat = 0.12f;
    float pd80BlackWhiteShowClip = 0.0f;
    float pd80BlackWhiteStrength = 0.0f;
    /// PD80_04_Contrast_Brightness_Saturation.fx — prod80 `exposure`/`con`/`bri`/`sat`/`vib`, channel/custom sat, depth blend.
    float pd80CbsEnableDither = 1.0f;
    float pd80CbsDitherStrength = 1.0f;
    float pd80CbsTint = 0.0f;
    float pd80CbsExposure = 0.0f;
    float pd80CbsContrast = 0.0f;
    float pd80CbsBrightness = 0.0f;
    float pd80CbsSaturation = 0.0f;
    float pd80CbsVibrance = 0.0f;
    float pd80CbsHueMid = 0.0f;
    float pd80CbsHueRange = 0.167f;
    float pd80CbsSatCustom = 0.0f;
    float pd80CbsSatR = 0.0f;
    float pd80CbsSatY = 0.0f;
    float pd80CbsSatG = 0.0f;
    float pd80CbsSatA = 0.0f;
    float pd80CbsSatB = 0.0f;
    float pd80CbsSatP = 0.0f;
    float pd80CbsSatM = 0.0f;
    float pd80CbsEnableDepth = 0.0f;
    float pd80CbsDisplayDepth = 0.0f;
    float pd80CbsDepthStart = 0.0f;
    float pd80CbsDepthEnd = 0.1f;
    float pd80CbsDepthCurve = 1.0f;
    float pd80CbsExposureFar = 0.0f;
    float pd80CbsContrastFar = 0.0f;
    float pd80CbsBrightnessFar = 0.0f;
    float pd80CbsSaturationFar = 0.0f;
    float pd80CbsVibranceFar = 0.0f;
    float pd80CbsStrength = 0.0f;
    /// PD80_06_Chromatic_Aberration.fx — multi-tap hue ring; offset taps use the pre-CA grade chain only.
    float pd80CaMasterStrength = 0.0f;
    float pd80CaEffectStrength = 1.0f;
    float pd80CaGlobalWidth = -12.0f;
    float pd80CaSampleSteps = 24.0f;
    float pd80CaType = 0.0f;
    float pd80CaDegrees = 135.0f;
    float pd80CaWidth = 1.0f;
    float pd80CaCurve = 1.0f;
    float pd80CaOX = 0.0f;
    float pd80CaOY = 0.0f;
    float pd80CaShapeX = 1.0f;
    float pd80CaShapeY = 1.0f;
    ri::math::Vec3 pd80CaVignetteColor{};
    float pd80CaShowCa = 0.0f;
    float pd80CaEnableDepthInt = 0.0f;
    float pd80CaEnableDepthWidth = 0.0f;
    float pd80CaDisplayDepth = 0.0f;
    float pd80CaDepthStart = 0.0f;
    float pd80CaDepthEnd = 0.1f;
    float pd80CaDepthCurve = 1.0f;
    /// PD80_05_Sharpening.fx — separable Gaussian blur + luma screen sharpen (`EvaluatePrePostProcessChainBase` taps).
    float pd80LsMasterStrength = 0.0f;
    float pd80LsBlurSigma = 0.45f;
    float pd80LsSharpening = 1.7f;
    float pd80LsThreshold = 0.0f;
    float pd80LsLimiter = 0.03f;
    float pd80LsShowEdges = 0.0f;
    float pd80LsEnableDepth = 0.0f;
    float pd80LsEnableReverse = 0.0f;
    float pd80LsDisplayDepth = 0.0f;
    float pd80LsDepthStart = 0.0f;
    float pd80LsDepthEnd = 0.1f;
    float pd80LsDepthCurve = 1.0f;
    /// PD80_06_Film_Grain.fx — simplex grain + merge (`pd80_permtexture` procedural in shader).
    float pd80FgMasterStrength = 0.0f;
    float pd80FgGrainAdjust = 1.0f;
    float pd80FgGrainSize = 1.0f;
    float pd80FgGrainMotion = 1.0f;
    float pd80FgGrainOrigColor = 1.0f;
    float pd80FgUseNegnoise = 0.0f;
    float pd80FgGrainColor = 1.0f;
    float pd80FgGrainAmount = 0.333f;
    float pd80FgGrainIntensity = 0.65f;
    float pd80FgGrainDensity = 10.0f;
    float pd80FgGrainIntHigh = 1.0f;
    float pd80FgGrainIntLow = 1.0f;
    float pd80FgEnableTest = 0.0f;
    float pd80FgEnableDepth = 0.0f;
    float pd80FgDisplayDepth = 0.0f;
    float pd80FgDepthStart = 0.0f;
    float pd80FgDepthEnd = 0.1f;
    float pd80FgDepthCurve = 1.0f;
    /// PD80_06_Depth_Slicer.fx — linear depth band + `HSVToRGB` tint + `blendmode()` (`PD80_00_Blend_Modes.fxh`).
    float pd80DsMasterStrength = 0.0f;
    float pd80DsDepthNear = 0.0f;
    float pd80DsDepthPos = 0.015f;
    float pd80DsDepthFar = 0.0f;
    float pd80DsDepthSmoothing = 0.005f;
    float pd80DsIntensity = 0.0f;
    float pd80DsHue = 0.083f;
    float pd80DsSaturation = 0.0f;
    float pd80DsBlendMode = 0.0f;
    float pd80DsOpacity = 1.0f;
    /// PD80_01_Color_Gamut.fx — sRGB linearize → XYZ → target RGB primaries → encode sRGB (combo index 0–15).
    float pd80CgMasterStrength = 0.0f;
    float pd80ColorGamut = 0.0f;
    /// PD80_03_Color_Space_Curves.fx — hyperbolic tonemap on luma/L*/L/V + chroma scale (dither: procedural vs RGB noise tex).
    float pd80CscMasterStrength = 0.0f;
    float pd80CscEnableDither = 1.0f;
    float pd80CscDitherStrength = 1.0f;
    float pd80CscColorSpace = 1.0f;
    float pd80CscPos0ToeGrey = 0.2f;
    float pd80CscPos1ToeGrey = 0.2f;
    float pd80CscPos0ShoulderGrey = 0.8f;
    float pd80CscPos1ShoulderGrey = 0.8f;
    float pd80CscColorSat = 0.0f;
    /// PD80_03_Shadows_Midtones_Highlights.fx — luma-split weights + per-band prod80 `exposure`/`con`/`bri`/`blendmode`/tint/sat/vib.
    float pd80SmhMasterStrength = 0.0f;
    float pd80SmhLumaMode = 2.0f;
    float pd80SmhSeparationMode = 0.0f;
    float pd80SmhEnableDither = 1.0f;
    float pd80SmhDitherStrength = 2.0f;
    ri::math::Vec3 pd80SmhBlendColorShadow{0.0f, 0.365f, 1.0f};
    float pd80SmhShadowExposure = 0.0f;
    float pd80SmhShadowContrast = 0.0f;
    float pd80SmhShadowBrightness = 0.0f;
    float pd80SmhShadowBlendMode = 0.0f;
    float pd80SmhShadowOpacity = 0.0f;
    float pd80SmhShadowTint = 0.0f;
    float pd80SmhShadowSaturation = 0.0f;
    float pd80SmhShadowVibrance = 0.0f;
    ri::math::Vec3 pd80SmhBlendColorMid{0.98f, 0.588f, 0.0f};
    float pd80SmhMidExposure = 0.0f;
    float pd80SmhMidContrast = 0.0f;
    float pd80SmhMidBrightness = 0.0f;
    float pd80SmhMidBlendMode = 0.0f;
    float pd80SmhMidOpacity = 0.0f;
    float pd80SmhMidTint = 0.0f;
    float pd80SmhMidSaturation = 0.0f;
    float pd80SmhMidVibrance = 0.0f;
    ri::math::Vec3 pd80SmhBlendColorHighlight{1.0f, 1.0f, 1.0f};
    float pd80SmhHighlightExposure = 0.0f;
    float pd80SmhHighlightContrast = 0.0f;
    float pd80SmhHighlightBrightness = 0.0f;
    float pd80SmhHighlightBlendMode = 0.0f;
    float pd80SmhHighlightOpacity = 0.0f;
    float pd80SmhHighlightTint = 0.0f;
    float pd80SmhHighlightSaturation = 0.0f;
    float pd80SmhHighlightVibrance = 0.0f;
    /// PD80_03_Curved_Levels.fx — ishiyama hyperbolic tone curve + in/out points; optional per-channel RGB curve branch.
    float pd80ClMasterStrength = 0.0f;
    float pd80ClEnableDither = 1.0f;
    float pd80ClDitherStrength = 1.0f;
    float pd80ClEnableRgb = 0.0f;
    float pd80ClGreyBlackIn = 0.0f;
    float pd80ClGreyWhiteIn = 255.0f;
    float pd80ClGreyBlackOut = 0.0f;
    float pd80ClGreyWhiteOut = 255.0f;
    float pd80ClGreyPos0Shoulder = 0.75f;
    float pd80ClGreyPos1Shoulder = 0.75f;
    float pd80ClGreyPos0Toe = 0.25f;
    float pd80ClGreyPos1Toe = 0.25f;
    float pd80ClRedBlackIn = 0.0f;
    float pd80ClRedWhiteIn = 255.0f;
    float pd80ClRedBlackOut = 0.0f;
    float pd80ClRedWhiteOut = 255.0f;
    float pd80ClRedPos0Shoulder = 0.75f;
    float pd80ClRedPos1Shoulder = 0.75f;
    float pd80ClRedPos0Toe = 0.25f;
    float pd80ClRedPos1Toe = 0.25f;
    float pd80ClGreenBlackIn = 0.0f;
    float pd80ClGreenWhiteIn = 255.0f;
    float pd80ClGreenBlackOut = 0.0f;
    float pd80ClGreenWhiteOut = 255.0f;
    float pd80ClGreenPos0Shoulder = 0.75f;
    float pd80ClGreenPos1Shoulder = 0.75f;
    float pd80ClGreenPos0Toe = 0.25f;
    float pd80ClGreenPos1Toe = 0.25f;
    float pd80ClBlueBlackIn = 0.0f;
    float pd80ClBlueWhiteIn = 255.0f;
    float pd80ClBlueBlackOut = 0.0f;
    float pd80ClBlueWhiteOut = 255.0f;
    float pd80ClBluePos0Shoulder = 0.75f;
    float pd80ClBluePos1Shoulder = 0.75f;
    float pd80ClBluePos0Toe = 0.25f;
    float pd80ClBluePos1Toe = 0.25f;
    /// PD80_04_Selective_Color.fx — Photoshop-style selective CMYK adjustments by color range (absolute/relative).
    float pd80ScMasterStrength = 0.0f;
    float pd80ScCorrectionMethod = 1.0f;
    float pd80ScCorrectionMethodSaturation = 1.0f;
    /// Reds: cyan, magenta, yellow, black.
    float pd80ScRedsCyan = 0.0f;
    float pd80ScRedsMagenta = 0.0f;
    float pd80ScRedsYellow = 0.0f;
    float pd80ScRedsBlack = 0.0f;
    float pd80ScRedsSaturation = 0.0f;
    float pd80ScRedsVibrance = 0.0f;
    /// Yellows.
    float pd80ScYellowsCyan = 0.0f;
    float pd80ScYellowsMagenta = 0.0f;
    float pd80ScYellowsYellow = 0.0f;
    float pd80ScYellowsBlack = 0.0f;
    float pd80ScYellowsSaturation = 0.0f;
    float pd80ScYellowsVibrance = 0.0f;
    /// Greens.
    float pd80ScGreensCyan = 0.0f;
    float pd80ScGreensMagenta = 0.0f;
    float pd80ScGreensYellow = 0.0f;
    float pd80ScGreensBlack = 0.0f;
    float pd80ScGreensSaturation = 0.0f;
    float pd80ScGreensVibrance = 0.0f;
    /// Cyans.
    float pd80ScCyansCyan = 0.0f;
    float pd80ScCyansMagenta = 0.0f;
    float pd80ScCyansYellow = 0.0f;
    float pd80ScCyansBlack = 0.0f;
    float pd80ScCyansSaturation = 0.0f;
    float pd80ScCyansVibrance = 0.0f;
    /// Blues.
    float pd80ScBluesCyan = 0.0f;
    float pd80ScBluesMagenta = 0.0f;
    float pd80ScBluesYellow = 0.0f;
    float pd80ScBluesBlack = 0.0f;
    float pd80ScBluesSaturation = 0.0f;
    float pd80ScBluesVibrance = 0.0f;
    /// Magentas.
    float pd80ScMagentasCyan = 0.0f;
    float pd80ScMagentasMagenta = 0.0f;
    float pd80ScMagentasYellow = 0.0f;
    float pd80ScMagentasBlack = 0.0f;
    float pd80ScMagentasSaturation = 0.0f;
    float pd80ScMagentasVibrance = 0.0f;
    /// Whites.
    float pd80ScWhitesCyan = 0.0f;
    float pd80ScWhitesMagenta = 0.0f;
    float pd80ScWhitesYellow = 0.0f;
    float pd80ScWhitesBlack = 0.0f;
    float pd80ScWhitesSaturation = 0.0f;
    float pd80ScWhitesVibrance = 0.0f;
    /// Neutrals.
    float pd80ScNeutralsCyan = 0.0f;
    float pd80ScNeutralsMagenta = 0.0f;
    float pd80ScNeutralsYellow = 0.0f;
    float pd80ScNeutralsBlack = 0.0f;
    float pd80ScNeutralsSaturation = 0.0f;
    float pd80ScNeutralsVibrance = 0.0f;
    /// Blacks.
    float pd80ScBlacksCyan = 0.0f;
    float pd80ScBlacksMagenta = 0.0f;
    float pd80ScBlacksYellow = 0.0f;
    float pd80ScBlacksBlack = 0.0f;
    float pd80ScBlacksSaturation = 0.0f;
    float pd80ScBlacksVibrance = 0.0f;
    /// PD80_06_Posterize_Pixelate.fx — level quantization + pixel cell borders + optional dither.
    float pd80PpMasterStrength = 0.0f;
    float pd80PpNumberOfLevels = 255.0f;
    float pd80PpPixelSize = 1.0f;
    float pd80PpBorderStrength = 0.0f;
    float pd80PpEnableDither = 0.0f;
    float pd80PpDitherMotion = 1.0f;
    float pd80PpDitherStrength = 1.0f;
    /// PD80_04_Magical_Rectangle.fx — depth-aware transformed shape with blend-moded coloration.
    float pd80MrShape = 0.0f;
    float pd80MrInvertShape = 0.0f;
    float pd80MrRotation = 45.0f;
    ri::math::Vec2 pd80MrCenter = {0.5f, 0.5f};
    float pd80MrSizeX = 0.125f;
    float pd80MrSizeY = 0.125f;
    float pd80MrDepthPosition = 0.0f;
    float pd80MrSmoothing = 0.01f;
    float pd80MrDepthSmoothing = 0.002f;
    float pd80MrDitherStrength = 0.0f;
    ri::math::Vec3 pd80MrColor = {0.5f, 0.5f, 0.5f};
    float pd80MrExposure = 0.0f;
    float pd80MrContrast = 0.0f;
    float pd80MrBrightness = 0.0f;
    float pd80MrHue = 0.0f;
    float pd80MrSaturation = 0.0f;
    float pd80MrVibrance = 0.0f;
    float pd80MrEnableGradient = 0.0f;
    float pd80MrGradientType = 0.0f;
    float pd80MrGradientCurve = 0.25f;
    float pd80MrIntensityBoost = 1.0f;
    float pd80MrBlendMode = 0.0f;
    float pd80MrOpacity = 1.0f;
    /// PD80_02_Bonus_LUT_pack.fx (PD80_LUT_v2.fxh): multi-LUT atlas selection + LAB luma/chroma mixing.
    float pd80BlpMasterStrength = 0.0f;
    float pd80BlpEnableDither = 1.0f;
    float pd80BlpDitherStrength = 1.0f;
    float pd80BlpLutSelector = 0.0f;
    float pd80BlpMixChroma = 1.0f;
    float pd80BlpMixLuma = 1.0f;
    ri::math::Vec3 pd80BlpBlackIn = {0.0f, 0.0f, 0.0f};
    ri::math::Vec3 pd80BlpWhiteIn = {1.0f, 1.0f, 1.0f};
    ri::math::Vec3 pd80BlpBlackOut = {0.0f, 0.0f, 0.0f};
    ri::math::Vec3 pd80BlpWhiteOut = {1.0f, 1.0f, 1.0f};
    float pd80BlpGamma = 1.0f;
    /// PD80_02_Cinetools_LUT.fx (PD80_LUT_v2.fxh): independent LUT-v2 settings for cinelut atlas.
    float pd80CltMasterStrength = 0.0f;
    float pd80CltEnableDither = 1.0f;
    float pd80CltDitherStrength = 1.0f;
    float pd80CltLutSelector = 0.0f;
    float pd80CltMixChroma = 1.0f;
    float pd80CltMixLuma = 1.0f;
    ri::math::Vec3 pd80CltBlackIn = {0.0f, 0.0f, 0.0f};
    ri::math::Vec3 pd80CltWhiteIn = {1.0f, 1.0f, 1.0f};
    ri::math::Vec3 pd80CltBlackOut = {0.0f, 0.0f, 0.0f};
    ri::math::Vec3 pd80CltWhiteOut = {1.0f, 1.0f, 1.0f};
    float pd80CltGamma = 1.0f;
    /// PD80_02_LUT_Creator.fx — top-left neutral LUT overlay for LUT baking workflows.
    float pd80LcMasterStrength = 0.0f;
    float pd80LcTextureWidth = 512.0f;
    float pd80LcTextureHeight = 512.0f;
    /// PD80_06_Luma_Fade.fx — luma-gated interpolation between pre-effect and post-effect color.
    float pd80LfMasterStrength = 0.0f;
    float pd80LfTransitionSpeed = 0.5f;
    float pd80LfMinLevel = 0.125f;
    float pd80LfMaxLevel = 0.3f;
    /// PD80_04_Color_Gradients.fx — luma-separated shadow/mid/high color blending with optional day-night transition.
    float pd80Cg4MasterStrength = 0.0f;
    float pd80Cg4LumaMode = 0.0f;
    float pd80Cg4SeparationMode = 0.0f;
    float pd80Cg4EnableDither = 1.0f;
    float pd80Cg4DitherStrength = 1.0f;
    float pd80Cg4DesaturateBase = 0.0f;
    float pd80Cg4FinalMix = 0.333f;
    ri::math::Vec3 pd80Cg4LightSceneMidColor = {0.98f, 0.588f, 0.0f};
    float pd80Cg4LightSceneMidBlendMode = 10.0f;
    float pd80Cg4LightSceneMidOpacity = 1.0f;
    ri::math::Vec3 pd80Cg4LightSceneShadowColor = {0.0f, 0.365f, 1.0f};
    float pd80Cg4LightSceneShadowBlendMode = 5.0f;
    float pd80Cg4LightSceneShadowOpacity = 0.3f;
    float pd80Cg4EnableDarkScene = 1.0f;
    ri::math::Vec3 pd80Cg4DarkSceneMidColor = {0.0f, 0.365f, 1.0f};
    float pd80Cg4DarkSceneMidBlendMode = 10.0f;
    float pd80Cg4DarkSceneMidOpacity = 1.0f;
    ri::math::Vec3 pd80Cg4DarkSceneShadowColor = {0.0f, 0.039f, 0.588f};
    float pd80Cg4DarkSceneShadowBlendMode = 10.0f;
    float pd80Cg4DarkSceneShadowOpacity = 1.0f;
    float pd80Cg4MinLevel = 0.125f;
    float pd80Cg4MaxLevel = 0.3f;
    /// PD80_01A_RT_Correct_Contrast.fx — adaptive black/white point normalization.
    float pd80CcMasterStrength = 0.0f;
    float pd80CcEnableWhitepoint = 0.0f;
    float pd80CcWhitepointStrength = 1.0f;
    float pd80CcEnableBlackpoint = 1.0f;
    float pd80CcBlackpointStrength = 1.0f;
    /// PD80_01B_RT_Correct_Color.fx — tint removal across black/mid/white reference colors.
    float pd80RccMasterStrength = 0.0f;
    float pd80RccEnableDither = 1.0f;
    float pd80RccDitherStrength = 1.0f;
    float pd80RccEnableWhitepoint = 1.0f;
    float pd80RccWhitepointRespectLuma = 1.0f;
    float pd80RccWhitepointMethod = 0.0f;
    float pd80RccWhitepointStrength = 1.0f;
    float pd80RccWhitepointLumaStrength = 1.0f;
    float pd80RccEnableBlackpoint = 1.0f;
    float pd80RccBlackpointRespectLuma = 0.0f;
    float pd80RccBlackpointMethod = 1.0f;
    float pd80RccBlackpointStrength = 1.0f;
    float pd80RccBlackpointLumaStrength = 1.0f;
    float pd80RccEnableMidpoint = 1.0f;
    float pd80RccMidpointRespectLuma = 1.0f;
    float pd80RccMidUseAltMethod = 1.0f;
    float pd80RccMidScale = 0.5f;
    /// PD80_03_Filmic_Adaptation.fx — uncharted-style filmic curve with scene-luma toe adaptation.
    float pd80FaMasterStrength = 0.0f;
    float pd80FaAdjustShoulder = 1.0f;
    float pd80FaAdjustLinear = 1.0f;
    float pd80FaAdjustToe = 1.0f;
    /// PD80_02_Bloom.fx — thresholded HQ bloom with screen blend and optional bloom-only debug.
    float pd80HbMasterStrength = 0.0f;
    float pd80HbDebugBloom = 0.0f;
    float pd80HbDitherStrength = 2.0f;
    float pd80HbMix = 0.5f;
    float pd80HbThreshold = 0.333f;
    float pd80HbGreyValue = 0.333f;
    float pd80HbExposure = 0.0f;
    float pd80HbBlurSigma = 30.0f;
    float pd80HbSaturation = 0.0f;
    /// PD80_04_Selective_Color_v2.fx — alternate selective-color weighting with tonal shaping.
    float pd80Sc2MasterStrength = 0.0f;
    float pd80Sc2CorrectionMethod = 1.0f;
    float pd80Sc2SaturationScale = 1.0f;
    float pd80Sc2LightnessScale = 1.0f;
    /// Colourfulness.fx (bacondither) — perceptual saturation that protects near-clipping detail.
    /// `colourfulness` 0 = neutral/skip; negative desaturates, positive (with soft limit) enriches.
    float colourfulness = 0.0f;
    float colourfulnessLimitLuma = 0.7f;
    /// FilmicPass.fx — cinematic tone/colour curve. `filmicPassStrength` 0 = skip (opt-in).
    float filmicPassStrength = 0.0f;
    float filmicPassFade = 0.4f;
    float filmicPassBleach = 0.0f;
    float filmicPassSaturation = -0.15f;
    /// FilmGrain2.fx (martinsh) — animated 3D-Perlin grain (distinct from the simpler SweetFX grain).
    /// `filmGrain2Amount` 0 = skip.
    float filmGrain2Amount = 0.0f;
    float filmGrain2ColorAmount = 0.6f;
    float filmGrain2LuminanceAmount = 1.0f;
    float filmGrain2Size = 1.6f;
    /// Denoise.fx (NVIDIA KNN path) — spatial noise reduction on the graded LDR chain.
    /// `denoiseStrength` 0 = skip.
    float denoiseStrength = 0.0f;
    float denoiseNoiseLevel = 0.15f;
    float denoiseLerpCoefficient = 0.8f;
    float denoiseWeightThreshold = 0.03f;
    float denoiseCounterThreshold = 0.05f;
    float denoiseGaussianSigma = 50.0f;
    /// AdaptiveSharpen.fx (bacondither) — edge-aware adaptive sharpening on the graded LDR chain.
    /// `adaptiveSharpenStrength` maps to reference `curve_height`; 0 = skip.
    float adaptiveSharpenStrength = 0.0f;
    float adaptiveSharpenCurveSlope = 0.5f;
    float adaptiveSharpenLightOvershoot = 0.003f;
    float adaptiveSharpenDarkOvershoot = 0.009f;
    float adaptiveSharpenLightComprLow = 0.167f;
    float adaptiveSharpenLightComprHigh = 0.334f;
    float adaptiveSharpenDarkComprLow = 0.250f;
    float adaptiveSharpenDarkComprHigh = 0.500f;
    float adaptiveSharpenScaleLim = 0.1f;
    float adaptiveSharpenScaleCs = 0.056f;
    float adaptiveSharpenPmP = 0.7f;
    /// GaussianBlur.fx (Ioxa) — separable gaussian on graded LDR; `gaussianBlurStrength` 0 = skip.
    float gaussianBlurStrength = 0.0f;
    float gaussianBlurOffset = 1.0f;
    int gaussianBlurRadius = 1;
    /// FineSharp.fx (Didée / JPulowski) — YUV multi-pass sharpening; `fineSharpStrength` (`sstr`) 0 = skip.
    float fineSharpStrength = 0.0f;
    float fineSharpEqualization = 0.9f;
    float fineSharpXStrength = 0.19f;
    float fineSharpXRepair = 0.25f;
    float fineSharpLStrength = 1.49f;
    float fineSharpPStrength = 1.272f;
    int fineSharpMode = 0;
    /// Bloom.fx Marty McFly — pyramid bloom + screen/lighten combine; `martyBloomAmount` 0 = skip.
    float martyBloomThreshold = 0.8f;
    float martyBloomAmount = 0.0f;
    float martyBloomSaturation = 0.8f;
    int martyBloomMixMode = 2;
    ri::math::Vec3 martyBloomTint{0.7f, 0.8f, 1.0f};
    /// DOF.fx Marty McFly ring DOF — `creatorDofStrength` 0 = skip.
    float creatorDofStrength = 0.0f;
    bool creatorDofAutoFocus = true;
    float creatorDofManualFocusDepth = 0.02f;
    float creatorDofInfiniteFocus = 1.0f;
    ri::math::Vec2 creatorDofFocusPoint{0.5f, 0.5f};
    float creatorDofFocusRadius = 0.05f;
    int creatorDofFocusSamples = 6;
    float creatorDofNearBlurCurve = 1.6f;
    float creatorDofFarBlurCurve = 2.0f;
    float creatorDofBlurRadius = 15.0f;
    int creatorDofRingSamples = 6;
    int creatorDofRingRings = 4;
    float creatorDofRingThreshold = 0.7f;
    float creatorDofRingGain = 27.0f;
    float creatorDofRingBias = 0.0f;
    float creatorDofRingFringe = 0.5f;
    /// AmbientLight.fx Ganossa — `ambientLightIntensity` 0 = skip.
    float ambientLightIntensity = 0.0f;
    float ambientLightThreshold = 15.0f;
    bool ambientLightAdaptation = true;
    float ambientLightAdapt = 0.7f;
    float ambientLightAdaptBaseMult = 1.0f;
    int ambientLightAdaptBlackLevel = 2;
    bool ambientLightDither = true;
    bool ambientLightDirt = true;
    int ambientLightAdaptiveMode = 0;
    float ambientLightDirtInt = 1.0f;
    float ambientLightDirtOvrInt = 1.0f;
    /// FakeMotionBlur.fx Ganossa — `fakeMotionBlurRecall` 0 = skip.
    float fakeMotionBlurRecall = 0.0f;
    float fakeMotionBlurSoftness = 1.0f;
    /// ReflectiveBumpMapping.fx Marty McFly — `reflectiveBumpMappingStrength` 0 = skip.
    float reflectiveBumpMappingStrength = 0.0f;
    float reflectiveBumpMappingBlurWidthPixels = 100.0f;
    int reflectiveBumpMappingSampleCount = 32;
    float reflectiveBumpMappingReliefHeight = 0.3f;
    float reflectiveBumpMappingFresnelReflectance = 0.3f;
    float reflectiveBumpMappingFresnelMult = 0.5f;
    float reflectiveBumpMappingLowerThreshold = 0.1f;
    float reflectiveBumpMappingUpperThreshold = 0.2f;
    float reflectiveBumpMappingColorMaskRed = 1.0f;
    float reflectiveBumpMappingColorMaskOrange = 1.0f;
    float reflectiveBumpMappingColorMaskYellow = 1.0f;
    float reflectiveBumpMappingColorMaskGreen = 1.0f;
    float reflectiveBumpMappingColorMaskCyan = 1.0f;
    float reflectiveBumpMappingColorMaskBlue = 1.0f;
    float reflectiveBumpMappingColorMaskMagenta = 1.0f;
    float reflectiveBumpMappingDepthFarPlane = 1000.0f;
    /// Native CropResize/Resizer port. Pixel dimensions <= 0 inherit the current viewport.
    float cropScaleContentWidth = 0.0f;
    float cropScaleContentHeight = 0.0f;
    float cropScaleIntermediateWidth = 0.0f;
    float cropScaleIntermediateHeight = 0.0f;
    float cropScaleFinalWidth = 0.0f;
    float cropScaleFinalHeight = 0.0f;
    /// 0 = point/virtual-pixel sampling, 1 = linear sampling.
    int cropScaleFilter = 0;
    float cropScaleStrength = 0.0f;
    /// Barbatos uFakeHDR v3.2: atlas row 0 Natural, 1 Vivid, 2 FakeHDR.
    int barbatosFakeHdrPreset = 2;
    /// Reference range is 0..2; zero skips both LUT sampling and its triangular dither.
    float barbatosFakeHdrStrength = 0.0f;
    /// Raw Iron adaptive gradient debander. Operates in scene-linear space before tonemapping.
    float riAdaptiveDebandStrength = 0.0f;
    float riAdaptiveDebandRadius = 24.0f;
    float riAdaptiveDebandThreshold = 0.012f;
    int riAdaptiveDebandIterations = 1;
    /// Raw Iron edge-limited local-contrast sharpening in scene-linear space.
    float riLocalSharpenStrength = 0.0f;
    float riLocalSharpenRadius = 1.0f;
    float riLocalSharpenClamp = 0.08f;
    float riLocalSharpenEdgeLimit = 0.65f;
    /// Raw Iron ink outline: method 0 depth, 1 color, 2 both, 3 either.
    float riOutlineStrength = 0.0f;
    float riOutlineThickness = 1.5f;
    float riOutlineDepthSensitivity = 0.01f;
    float riOutlineColorSensitivity = 0.30f;
    int riOutlineMethod = 3;
    ri::math::Vec3 riOutlineColor{0.0f, 0.0f, 0.0f};
    float riOutlineWobbleAmount = 0.0f;
    float riOutlineWobbleSpeed = 1.0f;
    float riOutlineWobbleFrequency = 10.0f;
    float riOutlineDebug = 0.0f;
};

struct PostProcessPresetDefinition {
    PostProcessPreset preset = PostProcessPreset::Neutral;
    std::string_view slug{};
    std::string_view label{};
    std::string_view summary{};
};

struct PostProcessPresetLayer {
    PostProcessPreset preset = PostProcessPreset::Neutral;
    float blend = 1.0f;
};

inline constexpr std::array<PostProcessPresetDefinition, 10> kPostProcessPresetDefinitions{{
    {PostProcessPreset::Neutral, "neutral", "Neutral", "No post-process shaping; use the scene as-authored."},
    {PostProcessPreset::CrispGameplay, "crisp_gameplay", "Crisp Gameplay", "Clear low-noise presentation tuned for readable play spaces."},
    {PostProcessPreset::SoftVhs, "soft_vhs", "Soft VHS", "Gentle tape-era breakup without heavy blur or distortion."},
    {PostProcessPreset::Vhs, "vhs", "VHS", "Classic low-intensity scanline and chroma split."},
    {PostProcessPreset::ColdFacility, "cold_facility", "Cold Facility", "Sterile cyan-tinted surveillance look for industrial spaces."},
    {PostProcessPreset::IndustrialHaze, "industrial_haze", "Industrial Haze", "Warm haze and soft blur for dense machinery rooms."},
    {PostProcessPreset::DreamPulse, "dream_pulse", "Dream Pulse", "Blurred tinted pulse suited for flashbacks and unstable states."},
    {PostProcessPreset::CombatFocus, "combat_focus", "Combat Focus", "Subtle urgency shaping with restrained aberration and low clutter."},
    {PostProcessPreset::AnalogHorror, "analog_horror", "Analog Horror", "Heavy distortion and scanline breakup for hostile transitions."},
    {PostProcessPreset::StaticTransition, "static_transition", "Static Transition", "Full static fade layer for room cuts and terminal jumps."},
}};

inline std::span<const PostProcessPresetDefinition> GetPostProcessPresetDefinitions() {
    return kPostProcessPresetDefinitions;
}

inline std::string_view ToString(PostProcessPreset preset) {
    for (const PostProcessPresetDefinition& definition : kPostProcessPresetDefinitions) {
        if (definition.preset == preset) {
            return definition.slug;
        }
    }
    return "neutral";
}

inline std::optional<PostProcessPreset> TryParsePostProcessPreset(std::string_view slug) {
    const auto normalize = [](std::string_view value) {
        std::string normalized;
        normalized.reserve(value.size());
        bool wroteSeparator = false;
        for (char ch : value) {
            const unsigned char code = static_cast<unsigned char>(ch);
            if (std::isalnum(code)) {
                normalized.push_back(static_cast<char>(std::tolower(code)));
                wroteSeparator = false;
                continue;
            }
            if (ch == '_' || ch == '-' || std::isspace(code)) {
                if (!normalized.empty() && !wroteSeparator) {
                    normalized.push_back('_');
                    wroteSeparator = true;
                }
            }
        }
        while (!normalized.empty() && normalized.back() == '_') {
            normalized.pop_back();
        }
        return normalized;
    };

    const std::string normalizedSlug = normalize(slug);
    if (normalizedSlug.empty()) {
        return std::nullopt;
    }
    for (const PostProcessPresetDefinition& definition : kPostProcessPresetDefinitions) {
        if (definition.slug == normalizedSlug) {
            return definition.preset;
        }
    }
    return std::nullopt;
}

inline float ClampUnit(float value) {
    if (!std::isfinite(value)) {
        return 0.0f;
    }
    return std::clamp(value, 0.0f, 1.0f);
}

inline float ClampFinite(float value, float minValue, float maxValue, float fallback) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, minValue, maxValue);
}

inline PostProcessParameters SanitizePostProcessParameters(const PostProcessParameters& input) {
    PostProcessParameters out{};
    const float finiteTime = std::isfinite(input.timeSeconds) ? input.timeSeconds : 0.0f;
    out.timeSeconds = std::fmod(finiteTime, 4096.0f);
    out.noiseAmount = ClampFinite(input.noiseAmount, 0.0f, 0.30f, 0.0f);
    out.scanlineAmount = ClampFinite(input.scanlineAmount, 0.0f, 0.20f, 0.0f);
    out.barrelDistortion = ClampFinite(input.barrelDistortion, 0.0f, 0.20f, 0.0f);
    out.chromaticAberration = ClampFinite(input.chromaticAberration, 0.0f, 0.05f, 0.0f);
    out.tintColor = {
        ClampUnit(input.tintColor.x),
        ClampUnit(input.tintColor.y),
        ClampUnit(input.tintColor.z),
    };
    out.tintStrength = ClampFinite(input.tintStrength, 0.0f, 1.0f, 0.0f);
    out.blurAmount = ClampFinite(input.blurAmount, 0.0f, 0.05f, 0.0f);
    out.staticFadeAmount = ClampFinite(input.staticFadeAmount, 0.0f, 1.0f, 0.0f);
    out.casSharpenAmount = ClampFinite(input.casSharpenAmount, 0.0f, 1.0f, 0.0f);
    out.casContrastAdaptation = ClampFinite(input.casContrastAdaptation, 0.0f, 1.0f, 0.0f);
    out.bloomIntensity = ClampFinite(input.bloomIntensity, 0.0f, 0.5f, 0.0f);
    out.bloomThreshold = ClampFinite(input.bloomThreshold, 0.0f, 4.0f, 0.65f);
    out.debandStrength = ClampFinite(input.debandStrength, 0.0f, 0.12f, 0.0f);
    out.toneCurveStrength = ClampFinite(input.toneCurveStrength, 0.0f, 1.0f, 0.0f);
    out.outputDitherStrength = ClampFinite(input.outputDitherStrength, 0.0f, 1.0f, 0.0f);
    out.vignetteStrength = ClampFinite(input.vignetteStrength, 0.0f, 1.0f, 0.0f);
    out.filmGrainIntensity = ClampFinite(input.filmGrainIntensity, 0.0f, 0.5f, 0.0f);
    out.liftRgb = {
        ClampFinite(input.liftRgb.x, 0.02f, 2.0f, 1.0f),
        ClampFinite(input.liftRgb.y, 0.02f, 2.0f, 1.0f),
        ClampFinite(input.liftRgb.z, 0.02f, 2.0f, 1.0f),
    };
    out.gammaRgb = {
        ClampFinite(input.gammaRgb.x, 0.02f, 2.0f, 1.0f),
        ClampFinite(input.gammaRgb.y, 0.02f, 2.0f, 1.0f),
        ClampFinite(input.gammaRgb.z, 0.02f, 2.0f, 1.0f),
    };
    out.gainRgb = {
        ClampFinite(input.gainRgb.x, 0.02f, 2.0f, 1.0f),
        ClampFinite(input.gainRgb.y, 0.02f, 2.0f, 1.0f),
        ClampFinite(input.gainRgb.z, 0.02f, 2.0f, 1.0f),
    };
    out.liftGammaGainMix = ClampFinite(input.liftGammaGainMix, 0.0f, 1.0f, 0.0f);
    out.vibrance = ClampFinite(input.vibrance, -1.0f, 1.0f, 0.0f);
    out.vibranceRgbBalance = {
        ClampFinite(input.vibranceRgbBalance.x, 0.0f, 10.0f, 1.0f),
        ClampFinite(input.vibranceRgbBalance.y, 0.0f, 10.0f, 1.0f),
        ClampFinite(input.vibranceRgbBalance.z, 0.0f, 10.0f, 1.0f),
    };
    out.technicolorPower = ClampFinite(input.technicolorPower, 0.0f, 8.0f, 4.0f);
    out.technicolorRgbNegative = {
        ClampFinite(input.technicolorRgbNegative.x, 0.05f, 1.0f, 0.88f),
        ClampFinite(input.technicolorRgbNegative.y, 0.05f, 1.0f, 0.88f),
        ClampFinite(input.technicolorRgbNegative.z, 0.05f, 1.0f, 0.88f),
    };
    out.technicolorStrength = ClampFinite(input.technicolorStrength, 0.0f, 1.0f, 0.0f);
    out.technicolor2ColorStrength = {
        ClampFinite(input.technicolor2ColorStrength.x, 0.0f, 2.0f, 0.2f),
        ClampFinite(input.technicolor2ColorStrength.y, 0.0f, 2.0f, 0.2f),
        ClampFinite(input.technicolor2ColorStrength.z, 0.0f, 2.0f, 0.2f),
    };
    out.technicolor2Brightness = ClampFinite(input.technicolor2Brightness, 0.5f, 1.5f, 1.0f);
    out.technicolor2Saturation = ClampFinite(input.technicolor2Saturation, 0.0f, 1.5f, 1.0f);
    out.technicolor2Strength = ClampFinite(input.technicolor2Strength, 0.0f, 1.0f, 0.0f);
    out.sepiaTint = {
        ClampUnit(input.sepiaTint.x),
        ClampUnit(input.sepiaTint.y),
        ClampUnit(input.sepiaTint.z),
    };
    out.sepiaStrength = ClampFinite(input.sepiaStrength, 0.0f, 1.0f, 0.0f);
    out.monochromePreset = std::clamp(input.monochromePreset, 0, 17);
    out.monochromeCustomCoeff = {
        ClampFinite(input.monochromeCustomCoeff.x, 0.0f, 1.0f, 0.21f),
        ClampFinite(input.monochromeCustomCoeff.y, 0.0f, 1.0f, 0.72f),
        ClampFinite(input.monochromeCustomCoeff.z, 0.0f, 1.0f, 0.07f),
    };
    out.monochromeColorSaturation = ClampFinite(input.monochromeColorSaturation, 0.0f, 1.0f, 1.0f);
    out.dpxRgbCurve = {
        ClampFinite(input.dpxRgbCurve.x, 1.0f, 15.0f, 8.0f),
        ClampFinite(input.dpxRgbCurve.y, 1.0f, 15.0f, 8.0f),
        ClampFinite(input.dpxRgbCurve.z, 1.0f, 15.0f, 8.0f),
    };
    out.dpxRgbC = {
        ClampFinite(input.dpxRgbC.x, 0.2f, 0.5f, 0.36f),
        ClampFinite(input.dpxRgbC.y, 0.2f, 0.5f, 0.36f),
        ClampFinite(input.dpxRgbC.z, 0.2f, 0.5f, 0.34f),
    };
    out.dpxContrast = ClampFinite(input.dpxContrast, 0.0f, 1.0f, 0.1f);
    out.dpxSaturation = ClampFinite(input.dpxSaturation, 0.0f, 8.0f, 3.0f);
    out.dpxColorfulness = ClampFinite(input.dpxColorfulness, 0.1f, 2.5f, 2.5f);
    out.dpxStrength = ClampFinite(input.dpxStrength, 0.0f, 1.0f, 0.0f);
    out.colorMatrixRed = {
        ClampFinite(input.colorMatrixRed.x, 0.0f, 1.0f, 0.817f),
        ClampFinite(input.colorMatrixRed.y, 0.0f, 1.0f, 0.183f),
        ClampFinite(input.colorMatrixRed.z, 0.0f, 1.0f, 0.0f),
    };
    out.colorMatrixGreen = {
        ClampFinite(input.colorMatrixGreen.x, 0.0f, 1.0f, 0.333f),
        ClampFinite(input.colorMatrixGreen.y, 0.0f, 1.0f, 0.667f),
        ClampFinite(input.colorMatrixGreen.z, 0.0f, 1.0f, 0.0f),
    };
    out.colorMatrixBlue = {
        ClampFinite(input.colorMatrixBlue.x, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.colorMatrixBlue.y, 0.0f, 1.0f, 0.125f),
        ClampFinite(input.colorMatrixBlue.z, 0.0f, 1.0f, 0.875f),
    };
    out.colorMatrixStrength = ClampFinite(input.colorMatrixStrength, 0.0f, 1.0f, 0.0f);
    out.fakeHdrPower = ClampFinite(input.fakeHdrPower, 0.0f, 8.0f, 1.30f);
    out.fakeHdrRadius1 = ClampFinite(input.fakeHdrRadius1, 0.0f, 8.0f, 0.793f);
    out.fakeHdrRadius2 = ClampFinite(input.fakeHdrRadius2, 0.0f, 8.0f, 0.87f);
    out.fakeHdrStrength = ClampFinite(input.fakeHdrStrength, 0.0f, 1.0f, 0.0f);
    out.levelsBlackPoint = ClampFinite(input.levelsBlackPoint, 0.0f, 255.0f, 16.0f);
    out.levelsWhitePoint = ClampFinite(input.levelsWhitePoint, 0.0f, 255.0f, 235.0f);
    if (out.levelsWhitePoint < out.levelsBlackPoint) {
        std::swap(out.levelsBlackPoint, out.levelsWhitePoint);
    }
    out.levelsStrength = ClampFinite(input.levelsStrength, 0.0f, 1.0f, 0.0f);
    out.levelsClipHighlight =
        (input.levelsClipHighlight >= 0.5f && std::isfinite(input.levelsClipHighlight)) ? 1.0f : 0.0f;
    out.lumaSharpenStrength = ClampFinite(input.lumaSharpenStrength, 0.0f, 3.0f, 0.0f);
    out.lumaSharpenClamp = std::max(ClampFinite(input.lumaSharpenClamp, 0.0f, 1.0f, 0.035f), 1e-5f);
    out.lumaSharpenPattern = std::clamp(input.lumaSharpenPattern, 0, 3);
    out.lumaSharpenOffsetBias = ClampFinite(input.lumaSharpenOffsetBias, 0.0f, 6.0f, 1.0f);
    out.lumaSharpenShowPattern =
        (input.lumaSharpenShowPattern >= 0.5f && std::isfinite(input.lumaSharpenShowPattern)) ? 1.0f : 0.0f;
    out.sweetFxCurvesMode = std::clamp(input.sweetFxCurvesMode, 0, 2);
    out.sweetFxCurvesFormula = std::clamp(input.sweetFxCurvesFormula, 0, 10);
    out.sweetFxCurvesContrast = ClampFinite(input.sweetFxCurvesContrast, -1.0f, 1.0f, 0.65f);
    out.sweetFxCurvesStrength = ClampFinite(input.sweetFxCurvesStrength, 0.0f, 1.0f, 0.0f);
    out.sweetFxChromaticAberrationShiftX =
        ClampFinite(input.sweetFxChromaticAberrationShiftX, -10.0f, 10.0f, 2.5f);
    out.sweetFxChromaticAberrationShiftY =
        ClampFinite(input.sweetFxChromaticAberrationShiftY, -10.0f, 10.0f, -0.5f);
    out.sweetFxChromaticAberrationStrength =
        ClampFinite(input.sweetFxChromaticAberrationStrength, 0.0f, 1.0f, 0.0f);
    out.sweetFxBorderWidthX = ClampFinite(input.sweetFxBorderWidthX, 0.0f, 65504.0f, 0.0f);
    out.sweetFxBorderWidthY = ClampFinite(input.sweetFxBorderWidthY, 0.0f, 65504.0f, 0.0f);
    out.sweetFxBorderRatio = ClampFinite(input.sweetFxBorderRatio, 0.25f, 8.0f, 2.35f);
    out.sweetFxBorderColor = {
        ClampFinite(input.sweetFxBorderColor.x, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.sweetFxBorderColor.y, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.sweetFxBorderColor.z, 0.0f, 1.0f, 0.0f),
    };
    out.sweetFxBorderStrength = ClampFinite(input.sweetFxBorderStrength, 0.0f, 1.0f, 0.0f);
    out.sweetFxCartoonPower = ClampFinite(input.sweetFxCartoonPower, 0.1f, 10.0f, 1.5f);
    out.sweetFxCartoonEdgeSlope = ClampFinite(input.sweetFxCartoonEdgeSlope, 0.1f, 6.0f, 1.5f);
    out.sweetFxCartoonStrength = ClampFinite(input.sweetFxCartoonStrength, 0.0f, 1.0f, 0.0f);
    out.sweetFxTonemapGamma = ClampFinite(input.sweetFxTonemapGamma, 0.0f, 2.0f, 1.0f);
    out.sweetFxTonemapExposure = ClampFinite(input.sweetFxTonemapExposure, -1.0f, 1.0f, 0.0f);
    out.sweetFxTonemapSaturation = ClampFinite(input.sweetFxTonemapSaturation, -1.0f, 1.0f, 0.0f);
    out.sweetFxTonemapBleach = ClampFinite(input.sweetFxTonemapBleach, 0.0f, 1.0f, 0.0f);
    out.sweetFxTonemapDefog = ClampFinite(input.sweetFxTonemapDefog, 0.0f, 1.0f, 0.0f);
    out.sweetFxTonemapFogColor = {
        ClampFinite(input.sweetFxTonemapFogColor.x, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.sweetFxTonemapFogColor.y, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.sweetFxTonemapFogColor.z, 0.0f, 1.0f, 1.0f),
    };
    out.sweetFxTonemapStrength = ClampFinite(input.sweetFxTonemapStrength, 0.0f, 1.0f, 0.0f);
    out.sweetFxSplitscreenMode = std::clamp(input.sweetFxSplitscreenMode, 0, 6);
    out.sweetFxSplitscreenStrength = ClampFinite(input.sweetFxSplitscreenStrength, 0.0f, 1.0f, 0.0f);
    out.sweetFxNostalgiaPalette = std::clamp(input.sweetFxNostalgiaPalette, 0, 14);
    out.sweetFxNostalgiaScanlines = std::clamp(input.sweetFxNostalgiaScanlines, 0, 2);
    out.sweetFxNostalgiaDither = ClampFinite(input.sweetFxNostalgiaDither, 0.0f, 1.0f, 0.0f);
    out.sweetFxNostalgiaStrength = ClampFinite(input.sweetFxNostalgiaStrength, 0.0f, 1.0f, 0.0f);
    out.sweetFxCompareMode = std::clamp(input.sweetFxCompareMode, 0, 8);
    out.sweetFxCompareDifferenceScale = ClampFinite(input.sweetFxCompareDifferenceScale, 1.0f, 20.0f, 5.0f);
    out.sweetFxCompareStrength = ClampFinite(input.sweetFxCompareStrength, 0.0f, 1.0f, 0.0f);
    out.sweetFxLayerPosition = {
        ClampFinite(input.sweetFxLayerPosition.x, 0.0f, 1.0f, 0.5f),
        ClampFinite(input.sweetFxLayerPosition.y, 0.0f, 1.0f, 0.5f),
    };
    out.sweetFxLayerScale = ClampFinite(input.sweetFxLayerScale, 0.01f, 4.0f, 1.0f);
    out.sweetFxLayerBlend = ClampFinite(input.sweetFxLayerBlend, 0.0f, 1.0f, 0.0f);
    out.sweetFxLayerTexWidth = ClampFinite(input.sweetFxLayerTexWidth, 1.0f, 8192.0f, 1280.0f);
    out.sweetFxLayerTexHeight = ClampFinite(input.sweetFxLayerTexHeight, 1.0f, 8192.0f, 720.0f);
    out.sweetFxFxaaSubpix = ClampFinite(input.sweetFxFxaaSubpix, 0.0f, 1.0f, 0.25f);
    out.sweetFxFxaaEdgeThreshold = ClampFinite(input.sweetFxFxaaEdgeThreshold, 0.0f, 1.0f, 0.125f);
    out.sweetFxFxaaEdgeThresholdMin = ClampFinite(input.sweetFxFxaaEdgeThresholdMin, 0.0f, 1.0f, 0.0f);
    out.sweetFxFxaaStrength = ClampFinite(input.sweetFxFxaaStrength, 0.0f, 1.0f, 0.0f);
    out.sweetFxCrtAmount = ClampFinite(input.sweetFxCrtAmount, 0.0f, 1.0f, 0.0f);
    out.sweetFxCrtResolution = ClampFinite(input.sweetFxCrtResolution, 1.0f, 8.0f, 1.15f);
    out.sweetFxCrtGamma = ClampFinite(input.sweetFxCrtGamma, 0.0f, 4.0f, 2.4f);
    out.sweetFxCrtMonitorGamma = ClampFinite(input.sweetFxCrtMonitorGamma, 0.0f, 4.0f, 2.2f);
    out.sweetFxCrtBrightness = ClampFinite(input.sweetFxCrtBrightness, 0.0f, 3.0f, 0.9f);
    out.sweetFxCrtScanlineIntensity = std::clamp(input.sweetFxCrtScanlineIntensity, 2, 4);
    out.sweetFxCrtScanlineGaussian = ClampFinite(input.sweetFxCrtScanlineGaussian, 0.0f, 1.0f, 1.0f);
    out.sweetFxCrtCurvature = ClampFinite(input.sweetFxCrtCurvature, 0.0f, 1.0f, 0.0f);
    out.sweetFxCrtCurvatureRadius = ClampFinite(input.sweetFxCrtCurvatureRadius, 0.0f, 2.0f, 1.5f);
    out.sweetFxCrtCornerSize = ClampFinite(input.sweetFxCrtCornerSize, 0.0f, 0.02f, 0.01f);
    out.sweetFxCrtViewerDistance = ClampFinite(input.sweetFxCrtViewerDistance, 0.0f, 4.0f, 2.0f);
    out.sweetFxCrtAngle = {
        ClampFinite(input.sweetFxCrtAngle.x, -0.2f, 0.2f, 0.0f),
        ClampFinite(input.sweetFxCrtAngle.y, -0.2f, 0.2f, 0.0f),
    };
    out.sweetFxCrtOverscan = ClampFinite(input.sweetFxCrtOverscan, 1.0f, 1.10f, 1.01f);
    out.sweetFxCrtOversample = ClampFinite(input.sweetFxCrtOversample, 0.0f, 1.0f, 1.0f);
    out.sweetFxAsciiSpacing = std::clamp(input.sweetFxAsciiSpacing, 0, 5);
    out.sweetFxAsciiFont = std::clamp(input.sweetFxAsciiFont, 0, 1);
    out.sweetFxAsciiFontColorMode = std::clamp(input.sweetFxAsciiFontColorMode, 0, 2);
    out.sweetFxAsciiFontColor = {
        ClampFinite(input.sweetFxAsciiFontColor.x, 0.0f, 1.0f, 1.0f),
        ClampFinite(input.sweetFxAsciiFontColor.y, 0.0f, 1.0f, 1.0f),
        ClampFinite(input.sweetFxAsciiFontColor.z, 0.0f, 1.0f, 1.0f),
    };
    out.sweetFxAsciiBackgroundColor = {
        ClampFinite(input.sweetFxAsciiBackgroundColor.x, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.sweetFxAsciiBackgroundColor.y, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.sweetFxAsciiBackgroundColor.z, 0.0f, 1.0f, 0.0f),
    };
    out.sweetFxAsciiSwapColors = ClampFinite(input.sweetFxAsciiSwapColors, 0.0f, 1.0f, 0.0f);
    out.sweetFxAsciiInvertBrightness = ClampFinite(input.sweetFxAsciiInvertBrightness, 0.0f, 1.0f, 0.0f);
    out.sweetFxAsciiDithering = ClampFinite(input.sweetFxAsciiDithering, 0.0f, 1.0f, 1.0f);
    out.sweetFxAsciiDitheringIntensity = ClampFinite(input.sweetFxAsciiDitheringIntensity, 0.0f, 4.0f, 2.0f);
    out.sweetFxAsciiDitheringDebugGradient = ClampFinite(input.sweetFxAsciiDitheringDebugGradient, 0.0f, 1.0f, 0.0f);
    out.sweetFxAsciiStrength = ClampFinite(input.sweetFxAsciiStrength, 0.0f, 1.0f, 0.0f);
    out.sweetFxSmaaEdgeDetectionType = std::clamp(input.sweetFxSmaaEdgeDetectionType, 0, 2);
    out.sweetFxSmaaEdgeThreshold = ClampFinite(input.sweetFxSmaaEdgeThreshold, 0.01f, 0.50f, 0.10f);
    out.sweetFxSmaaDepthThreshold = ClampFinite(input.sweetFxSmaaDepthThreshold, 0.001f, 0.50f, 0.01f);
    out.sweetFxSmaaMaxSearchSteps = std::clamp(input.sweetFxSmaaMaxSearchSteps, 0, 112);
    out.sweetFxSmaaMaxSearchStepsDiagonal = std::clamp(input.sweetFxSmaaMaxSearchStepsDiagonal, 0, 20);
    out.sweetFxSmaaCornerRounding = std::clamp(input.sweetFxSmaaCornerRounding, 0, 100);
    out.sweetFxSmaaDebugOutput = ClampFinite(input.sweetFxSmaaDebugOutput, 0.0f, 2.0f, 0.0f);
    out.sweetFxSmaaStrength = ClampFinite(input.sweetFxSmaaStrength, 0.0f, 1.0f, 0.0f);
    out.reshadeDaltonizeType = std::clamp(input.reshadeDaltonizeType, 0, 2);
    out.reshadeDaltonizeStrength = ClampFinite(input.reshadeDaltonizeStrength, 0.0f, 1.0f, 0.0f);
    out.reshadeDisplayDepthPresentType = std::clamp(input.reshadeDisplayDepthPresentType, 0, 2);
    out.reshadeDisplayDepthStrength = ClampFinite(input.reshadeDisplayDepthStrength, 0.0f, 2.0f, 0.0f);
    out.reshadeLutAmountChroma = ClampFinite(input.reshadeLutAmountChroma, 0.0f, 1.0f, 1.0f);
    out.reshadeLutAmountLuma = ClampFinite(input.reshadeLutAmountLuma, 0.0f, 1.0f, 1.0f);
    out.reshadeLutStrength = ClampFinite(input.reshadeLutStrength, 0.0f, 1.0f, 0.0f);
    out.pd80TechnicolorStrength = ClampFinite(input.pd80TechnicolorStrength, 0.0f, 1.0f, 0.0f);
    out.pd80TechnicolorRed2strip = {
        ClampUnit(input.pd80TechnicolorRed2strip.x),
        ClampUnit(input.pd80TechnicolorRed2strip.y),
        ClampUnit(input.pd80TechnicolorRed2strip.z),
    };
    out.pd80TechnicolorCyan2strip = {
        ClampUnit(input.pd80TechnicolorCyan2strip.x),
        ClampUnit(input.pd80TechnicolorCyan2strip.y),
        ClampUnit(input.pd80TechnicolorCyan2strip.z),
    };
    out.pd80TechnicolorColorKey = {
        ClampFinite(input.pd80TechnicolorColorKey.x, 0.0f, 2.0f, 1.0f),
        ClampFinite(input.pd80TechnicolorColorKey.y, 0.0f, 2.0f, 1.0f),
        ClampFinite(input.pd80TechnicolorColorKey.z, 0.0f, 2.0f, 1.0f),
    };
    out.pd80TechnicolorSaturation2 = ClampFinite(input.pd80TechnicolorSaturation2, 1.0f, 2.0f, 1.5f);
    out.pd80TechnicolorEnable3strip = input.pd80TechnicolorEnable3strip >= 0.5f ? 1.0f : 0.0f;
    out.pd80Technicolor3ColorStrength = {
        ClampFinite(input.pd80Technicolor3ColorStrength.x, 0.0f, 2.0f, 0.2f),
        ClampFinite(input.pd80Technicolor3ColorStrength.y, 0.0f, 2.0f, 0.2f),
        ClampFinite(input.pd80Technicolor3ColorStrength.z, 0.0f, 2.0f, 0.2f),
    };
    out.pd80Technicolor3Brightness = ClampFinite(input.pd80Technicolor3Brightness, 0.5f, 1.5f, 1.0f);
    out.pd80Technicolor3Saturation = ClampFinite(input.pd80Technicolor3Saturation, 0.0f, 1.5f, 1.0f);
    out.pd80Technicolor3Strength = ClampFinite(input.pd80Technicolor3Strength, 0.0f, 1.0f, 1.0f);
    out.pd80ColorTemperatureKelvin = ClampFinite(input.pd80ColorTemperatureKelvin, 1000.0f, 40000.0f, 6500.0f);
    out.pd80ColorTemperatureLuminancePreservation =
        ClampFinite(input.pd80ColorTemperatureLuminancePreservation, 0.0f, 1.0f, 1.0f);
    out.pd80ColorTemperatureMix = ClampFinite(input.pd80ColorTemperatureMix, 0.0f, 1.0f, 1.0f);
    out.pd80ColorTemperatureStrength = ClampFinite(input.pd80ColorTemperatureStrength, 0.0f, 1.0f, 0.0f);
    out.pd80SaturationLimit = ClampFinite(input.pd80SaturationLimit, 0.0f, 1.0f, 1.0f);
    out.pd80SaturationLimitStrength = ClampFinite(input.pd80SaturationLimitStrength, 0.0f, 1.0f, 0.0f);
    out.pd80ColorBalanceShadow = {
        ClampFinite(input.pd80ColorBalanceShadow.x, -1.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80ColorBalanceShadow.y, -1.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80ColorBalanceShadow.z, -1.0f, 1.0f, 0.0f),
    };
    out.pd80ColorBalanceMid = {
        ClampFinite(input.pd80ColorBalanceMid.x, -1.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80ColorBalanceMid.y, -1.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80ColorBalanceMid.z, -1.0f, 1.0f, 0.0f),
    };
    out.pd80ColorBalanceHigh = {
        ClampFinite(input.pd80ColorBalanceHigh.x, -1.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80ColorBalanceHigh.y, -1.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80ColorBalanceHigh.z, -1.0f, 1.0f, 0.0f),
    };
    out.pd80ColorBalancePreserveLuma = input.pd80ColorBalancePreserveLuma >= 0.5f ? 1.0f : 0.0f;
    out.pd80ColorBalanceSeparationMode = input.pd80ColorBalanceSeparationMode >= 0.5f ? 1.0f : 0.0f;
    out.pd80ColorBalanceStrength = ClampFinite(input.pd80ColorBalanceStrength, 0.0f, 1.0f, 0.0f);
    out.pd80ColorIsolationHueMid = ClampFinite(input.pd80ColorIsolationHueMid, 0.0f, 1.0f, 0.0f);
    out.pd80ColorIsolationHueRange =
        ClampFinite(input.pd80ColorIsolationHueRange, 1.0e-5f, 1.0f, 0.167f);
    out.pd80ColorIsolationSatLimit = ClampFinite(input.pd80ColorIsolationSatLimit, 0.0f, 1.0f, 1.0f);
    out.pd80ColorIsolationFxMix = ClampFinite(input.pd80ColorIsolationFxMix, 0.0f, 1.0f, 1.0f);
    out.pd80ColorIsolationStrength = ClampFinite(input.pd80ColorIsolationStrength, 0.0f, 1.0f, 0.0f);
    out.pd80LevelsBlackIn = {
        ClampFinite(input.pd80LevelsBlackIn.x, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80LevelsBlackIn.y, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80LevelsBlackIn.z, 0.0f, 1.0f, 0.0f),
    };
    out.pd80LevelsWhiteIn = {
        ClampFinite(input.pd80LevelsWhiteIn.x, 0.0f, 1.0f, 1.0f),
        ClampFinite(input.pd80LevelsWhiteIn.y, 0.0f, 1.0f, 1.0f),
        ClampFinite(input.pd80LevelsWhiteIn.z, 0.0f, 1.0f, 1.0f),
    };
    out.pd80LevelsBlackOut = {
        ClampFinite(input.pd80LevelsBlackOut.x, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80LevelsBlackOut.y, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80LevelsBlackOut.z, 0.0f, 1.0f, 0.0f),
    };
    out.pd80LevelsWhiteOut = {
        ClampFinite(input.pd80LevelsWhiteOut.x, 0.0f, 1.0f, 1.0f),
        ClampFinite(input.pd80LevelsWhiteOut.y, 0.0f, 1.0f, 1.0f),
        ClampFinite(input.pd80LevelsWhiteOut.z, 0.0f, 1.0f, 1.0f),
    };
    out.pd80LevelsGamma = ClampFinite(input.pd80LevelsGamma, 0.05f, 10.0f, 1.0f);
    out.pd80LevelsEnableDither = input.pd80LevelsEnableDither >= 0.5f ? 1.0f : 0.0f;
    out.pd80LevelsDitherStrength = ClampFinite(input.pd80LevelsDitherStrength, 0.0f, 10.0f, 1.0f);
    out.pd80LevelsStrength = ClampFinite(input.pd80LevelsStrength, 0.0f, 1.0f, 0.0f);
    out.pd80BlackWhiteMode =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80BlackWhiteMode)), 0, 13));
    out.pd80BlackWhiteCurveStr = ClampFinite(input.pd80BlackWhiteCurveStr, 1.0f, 4.0f, 1.5f);
    out.pd80BlackWhiteEnableDither = input.pd80BlackWhiteEnableDither >= 0.5f ? 1.0f : 0.0f;
    out.pd80BlackWhiteDitherStrength =
        ClampFinite(input.pd80BlackWhiteDitherStrength, 0.0f, 10.0f, 1.0f);
    out.pd80BlackWhiteRedChannel = ClampFinite(input.pd80BlackWhiteRedChannel, -2.0f, 3.0f, 0.2f);
    out.pd80BlackWhiteYellowChannel = ClampFinite(input.pd80BlackWhiteYellowChannel, -2.0f, 3.0f, 0.4f);
    out.pd80BlackWhiteGreenChannel = ClampFinite(input.pd80BlackWhiteGreenChannel, -2.0f, 3.0f, 0.6f);
    out.pd80BlackWhiteCyanChannel = ClampFinite(input.pd80BlackWhiteCyanChannel, -2.0f, 3.0f, 0.0f);
    out.pd80BlackWhiteBlueChannel = ClampFinite(input.pd80BlackWhiteBlueChannel, -2.0f, 3.0f, -0.6f);
    out.pd80BlackWhiteMagentaChannel = ClampFinite(input.pd80BlackWhiteMagentaChannel, -2.0f, 3.0f, -0.2f);
    out.pd80BlackWhiteUseTint = input.pd80BlackWhiteUseTint >= 0.5f ? 1.0f : 0.0f;
    out.pd80BlackWhiteTintHue = ClampFinite(input.pd80BlackWhiteTintHue, 0.0f, 1.0f, 0.083f);
    out.pd80BlackWhiteTintSat = ClampFinite(input.pd80BlackWhiteTintSat, 0.0f, 1.0f, 0.12f);
    out.pd80BlackWhiteShowClip = input.pd80BlackWhiteShowClip >= 0.5f ? 1.0f : 0.0f;
    out.pd80BlackWhiteStrength = ClampFinite(input.pd80BlackWhiteStrength, 0.0f, 1.0f, 0.0f);
    out.pd80CbsEnableDither = input.pd80CbsEnableDither >= 0.5f ? 1.0f : 0.0f;
    out.pd80CbsDitherStrength = ClampFinite(input.pd80CbsDitherStrength, 0.0f, 10.0f, 1.0f);
    out.pd80CbsTint = ClampFinite(input.pd80CbsTint, -1.0f, 1.0f, 0.0f);
    out.pd80CbsExposure = ClampFinite(input.pd80CbsExposure, -4.0f, 4.0f, 0.0f);
    out.pd80CbsContrast = ClampFinite(input.pd80CbsContrast, -1.0f, 1.5f, 0.0f);
    out.pd80CbsBrightness = ClampFinite(input.pd80CbsBrightness, -1.0f, 1.5f, 0.0f);
    out.pd80CbsSaturation = ClampFinite(input.pd80CbsSaturation, -1.0f, 1.0f, 0.0f);
    out.pd80CbsVibrance = ClampFinite(input.pd80CbsVibrance, -1.0f, 1.0f, 0.0f);
    out.pd80CbsHueMid = ClampFinite(input.pd80CbsHueMid, 0.0f, 1.0f, 0.0f);
    out.pd80CbsHueRange = std::max(ClampFinite(input.pd80CbsHueRange, 0.0f, 1.0f, 0.167f), 1e-4f);
    out.pd80CbsSatCustom = ClampFinite(input.pd80CbsSatCustom, -2.0f, 2.0f, 0.0f);
    out.pd80CbsSatR = ClampFinite(input.pd80CbsSatR, -2.0f, 2.0f, 0.0f);
    out.pd80CbsSatY = ClampFinite(input.pd80CbsSatY, -2.0f, 2.0f, 0.0f);
    out.pd80CbsSatG = ClampFinite(input.pd80CbsSatG, -2.0f, 2.0f, 0.0f);
    out.pd80CbsSatA = ClampFinite(input.pd80CbsSatA, -2.0f, 2.0f, 0.0f);
    out.pd80CbsSatB = ClampFinite(input.pd80CbsSatB, -2.0f, 2.0f, 0.0f);
    out.pd80CbsSatP = ClampFinite(input.pd80CbsSatP, -2.0f, 2.0f, 0.0f);
    out.pd80CbsSatM = ClampFinite(input.pd80CbsSatM, -2.0f, 2.0f, 0.0f);
    out.pd80CbsEnableDepth = input.pd80CbsEnableDepth >= 0.5f ? 1.0f : 0.0f;
    out.pd80CbsDisplayDepth = input.pd80CbsDisplayDepth >= 0.5f ? 1.0f : 0.0f;
    out.pd80CbsDepthStart = ClampFinite(input.pd80CbsDepthStart, 0.0f, 1.0f, 0.0f);
    out.pd80CbsDepthEnd = ClampFinite(input.pd80CbsDepthEnd, 0.0f, 1.0f, 0.1f);
    out.pd80CbsDepthCurve = ClampFinite(input.pd80CbsDepthCurve, 0.05f, 8.0f, 1.0f);
    out.pd80CbsExposureFar = ClampFinite(input.pd80CbsExposureFar, -4.0f, 4.0f, 0.0f);
    out.pd80CbsContrastFar = ClampFinite(input.pd80CbsContrastFar, -1.0f, 1.5f, 0.0f);
    out.pd80CbsBrightnessFar = ClampFinite(input.pd80CbsBrightnessFar, -1.0f, 1.5f, 0.0f);
    out.pd80CbsSaturationFar = ClampFinite(input.pd80CbsSaturationFar, -1.0f, 1.0f, 0.0f);
    out.pd80CbsVibranceFar = ClampFinite(input.pd80CbsVibranceFar, -1.0f, 1.0f, 0.0f);
    out.pd80CbsStrength = ClampFinite(input.pd80CbsStrength, 0.0f, 1.0f, 0.0f);
    out.pd80CaMasterStrength = ClampFinite(input.pd80CaMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80CaEffectStrength = ClampFinite(input.pd80CaEffectStrength, 0.0f, 1.0f, 1.0f);
    out.pd80CaGlobalWidth = ClampFinite(input.pd80CaGlobalWidth, -150.0f, 150.0f, -12.0f);
    out.pd80CaSampleSteps =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80CaSampleSteps)), 8, 96));
    out.pd80CaType = static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80CaType)), 0, 3));
    out.pd80CaDegrees = ClampFinite(input.pd80CaDegrees, 0.0f, 360.0f, 135.0f);
    out.pd80CaWidth = ClampFinite(input.pd80CaWidth, 0.0f, 5.0f, 1.0f);
    out.pd80CaCurve = ClampFinite(input.pd80CaCurve, 0.1f, 12.0f, 1.0f);
    out.pd80CaOX = ClampFinite(input.pd80CaOX, -1.0f, 1.0f, 0.0f);
    out.pd80CaOY = ClampFinite(input.pd80CaOY, -1.0f, 1.0f, 0.0f);
    out.pd80CaShapeX = ClampFinite(input.pd80CaShapeX, 0.2f, 6.0f, 1.0f);
    out.pd80CaShapeY = ClampFinite(input.pd80CaShapeY, 0.2f, 6.0f, 1.0f);
    out.pd80CaVignetteColor = {
        ClampUnit(input.pd80CaVignetteColor.x),
        ClampUnit(input.pd80CaVignetteColor.y),
        ClampUnit(input.pd80CaVignetteColor.z),
    };
    out.pd80CaShowCa = input.pd80CaShowCa >= 0.5f ? 1.0f : 0.0f;
    out.pd80CaEnableDepthInt = input.pd80CaEnableDepthInt >= 0.5f ? 1.0f : 0.0f;
    out.pd80CaEnableDepthWidth = input.pd80CaEnableDepthWidth >= 0.5f ? 1.0f : 0.0f;
    out.pd80CaDisplayDepth = input.pd80CaDisplayDepth >= 0.5f ? 1.0f : 0.0f;
    out.pd80CaDepthStart = ClampFinite(input.pd80CaDepthStart, 0.0f, 1.0f, 0.0f);
    out.pd80CaDepthEnd = ClampFinite(input.pd80CaDepthEnd, 0.0f, 1.0f, 0.1f);
    out.pd80CaDepthCurve = ClampFinite(input.pd80CaDepthCurve, 0.05f, 8.0f, 1.0f);
    out.pd80LsMasterStrength = ClampFinite(input.pd80LsMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80LsBlurSigma = ClampFinite(input.pd80LsBlurSigma, 0.05f, 2.0f, 0.45f);
    out.pd80LsSharpening = ClampFinite(input.pd80LsSharpening, 0.0f, 5.0f, 1.7f);
    out.pd80LsThreshold = ClampFinite(input.pd80LsThreshold, 0.0f, 1.0f, 0.0f);
    out.pd80LsLimiter = ClampFinite(input.pd80LsLimiter, 0.0f, 1.0f, 0.03f);
    out.pd80LsShowEdges = input.pd80LsShowEdges >= 0.5f ? 1.0f : 0.0f;
    out.pd80LsEnableDepth = input.pd80LsEnableDepth >= 0.5f ? 1.0f : 0.0f;
    out.pd80LsEnableReverse = input.pd80LsEnableReverse >= 0.5f ? 1.0f : 0.0f;
    out.pd80LsDisplayDepth = input.pd80LsDisplayDepth >= 0.5f ? 1.0f : 0.0f;
    out.pd80LsDepthStart = ClampFinite(input.pd80LsDepthStart, 0.0f, 1.0f, 0.0f);
    out.pd80LsDepthEnd = ClampFinite(input.pd80LsDepthEnd, 0.0f, 1.0f, 0.1f);
    out.pd80LsDepthCurve = ClampFinite(input.pd80LsDepthCurve, 0.05f, 8.0f, 1.0f);
    out.pd80FgMasterStrength = ClampFinite(input.pd80FgMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80FgGrainAdjust = ClampFinite(input.pd80FgGrainAdjust, 1.0f, 2.0f, 1.0f);
    out.pd80FgGrainSize =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80FgGrainSize)), 1, 4));
    out.pd80FgGrainMotion = input.pd80FgGrainMotion >= 0.5f ? 1.0f : 0.0f;
    out.pd80FgGrainOrigColor = input.pd80FgGrainOrigColor >= 0.5f ? 1.0f : 0.0f;
    out.pd80FgUseNegnoise = input.pd80FgUseNegnoise >= 0.5f ? 1.0f : 0.0f;
    out.pd80FgGrainColor = ClampFinite(input.pd80FgGrainColor, 0.0f, 1.0f, 1.0f);
    out.pd80FgGrainAmount = ClampFinite(input.pd80FgGrainAmount, 0.0f, 1.0f, 0.333f);
    out.pd80FgGrainIntensity = ClampFinite(input.pd80FgGrainIntensity, 0.0f, 1.0f, 0.65f);
    out.pd80FgGrainDensity = ClampFinite(input.pd80FgGrainDensity, 0.0f, 10.0f, 10.0f);
    out.pd80FgGrainIntHigh = ClampFinite(input.pd80FgGrainIntHigh, 0.0f, 1.0f, 1.0f);
    out.pd80FgGrainIntLow = ClampFinite(input.pd80FgGrainIntLow, 0.0f, 1.0f, 1.0f);
    out.pd80FgEnableTest = input.pd80FgEnableTest >= 0.5f ? 1.0f : 0.0f;
    out.pd80FgEnableDepth = input.pd80FgEnableDepth >= 0.5f ? 1.0f : 0.0f;
    out.pd80FgDisplayDepth = input.pd80FgDisplayDepth >= 0.5f ? 1.0f : 0.0f;
    out.pd80FgDepthStart = ClampFinite(input.pd80FgDepthStart, 0.0f, 1.0f, 0.0f);
    out.pd80FgDepthEnd = ClampFinite(input.pd80FgDepthEnd, 0.0f, 1.0f, 0.1f);
    out.pd80FgDepthCurve = ClampFinite(input.pd80FgDepthCurve, 0.05f, 8.0f, 1.0f);
    out.pd80DsMasterStrength = ClampFinite(input.pd80DsMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80DsDepthNear = ClampFinite(input.pd80DsDepthNear, 0.0f, 1.0f, 0.0f);
    out.pd80DsDepthPos = ClampFinite(input.pd80DsDepthPos, 0.0f, 1.0f, 0.015f);
    out.pd80DsDepthFar = ClampFinite(input.pd80DsDepthFar, 0.0f, 1.0f, 0.0f);
    out.pd80DsDepthSmoothing = ClampFinite(input.pd80DsDepthSmoothing, 0.0f, 1.0f, 0.005f);
    out.pd80DsIntensity = ClampFinite(input.pd80DsIntensity, 0.0f, 1.0f, 0.0f);
    out.pd80DsHue = ClampFinite(input.pd80DsHue, 0.0f, 1.0f, 0.083f);
    out.pd80DsSaturation = ClampFinite(input.pd80DsSaturation, 0.0f, 1.0f, 0.0f);
    out.pd80DsBlendMode =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80DsBlendMode)), 0, 20));
    out.pd80DsOpacity = ClampFinite(input.pd80DsOpacity, 0.0f, 1.0f, 1.0f);
    out.pd80CgMasterStrength = ClampFinite(input.pd80CgMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80ColorGamut =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80ColorGamut)), 0, 15));
    out.pd80CscMasterStrength = ClampFinite(input.pd80CscMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80CscEnableDither = input.pd80CscEnableDither >= 0.5f ? 1.0f : 0.0f;
    out.pd80CscDitherStrength = ClampFinite(input.pd80CscDitherStrength, 0.0f, 10.0f, 1.0f);
    out.pd80CscColorSpace =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80CscColorSpace)), 0, 3));
    out.pd80CscPos0ToeGrey = ClampFinite(input.pd80CscPos0ToeGrey, 0.0f, 1.0f, 0.2f);
    out.pd80CscPos1ToeGrey = ClampFinite(input.pd80CscPos1ToeGrey, 0.0f, 1.0f, 0.2f);
    out.pd80CscPos0ShoulderGrey = ClampFinite(input.pd80CscPos0ShoulderGrey, 0.0f, 1.0f, 0.8f);
    out.pd80CscPos1ShoulderGrey = ClampFinite(input.pd80CscPos1ShoulderGrey, 0.0f, 1.0f, 0.8f);
    out.pd80CscColorSat = ClampFinite(input.pd80CscColorSat, -1.0f, 1.0f, 0.0f);
    out.pd80SmhMasterStrength = ClampFinite(input.pd80SmhMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80SmhLumaMode =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80SmhLumaMode)), 0, 2));
    out.pd80SmhSeparationMode =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80SmhSeparationMode)), 0, 1));
    out.pd80SmhEnableDither = input.pd80SmhEnableDither >= 0.5f ? 1.0f : 0.0f;
    out.pd80SmhDitherStrength = ClampFinite(input.pd80SmhDitherStrength, 0.0f, 10.0f, 2.0f);
    out.pd80SmhBlendColorShadow = {
        ClampUnit(input.pd80SmhBlendColorShadow.x),
        ClampUnit(input.pd80SmhBlendColorShadow.y),
        ClampUnit(input.pd80SmhBlendColorShadow.z),
    };
    out.pd80SmhShadowExposure = ClampFinite(input.pd80SmhShadowExposure, -4.0f, 4.0f, 0.0f);
    out.pd80SmhShadowContrast = ClampFinite(input.pd80SmhShadowContrast, -1.0f, 1.5f, 0.0f);
    out.pd80SmhShadowBrightness = ClampFinite(input.pd80SmhShadowBrightness, -1.0f, 1.5f, 0.0f);
    out.pd80SmhShadowBlendMode =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80SmhShadowBlendMode)), 0, 20));
    out.pd80SmhShadowOpacity = ClampFinite(input.pd80SmhShadowOpacity, 0.0f, 1.0f, 0.0f);
    out.pd80SmhShadowTint = ClampFinite(input.pd80SmhShadowTint, -1.0f, 1.0f, 0.0f);
    out.pd80SmhShadowSaturation = ClampFinite(input.pd80SmhShadowSaturation, -1.0f, 1.0f, 0.0f);
    out.pd80SmhShadowVibrance = ClampFinite(input.pd80SmhShadowVibrance, -1.0f, 1.0f, 0.0f);
    out.pd80SmhBlendColorMid = {
        ClampUnit(input.pd80SmhBlendColorMid.x),
        ClampUnit(input.pd80SmhBlendColorMid.y),
        ClampUnit(input.pd80SmhBlendColorMid.z),
    };
    out.pd80SmhMidExposure = ClampFinite(input.pd80SmhMidExposure, -4.0f, 4.0f, 0.0f);
    out.pd80SmhMidContrast = ClampFinite(input.pd80SmhMidContrast, -1.0f, 1.5f, 0.0f);
    out.pd80SmhMidBrightness = ClampFinite(input.pd80SmhMidBrightness, -1.0f, 1.5f, 0.0f);
    out.pd80SmhMidBlendMode =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80SmhMidBlendMode)), 0, 20));
    out.pd80SmhMidOpacity = ClampFinite(input.pd80SmhMidOpacity, 0.0f, 1.0f, 0.0f);
    out.pd80SmhMidTint = ClampFinite(input.pd80SmhMidTint, -1.0f, 1.0f, 0.0f);
    out.pd80SmhMidSaturation = ClampFinite(input.pd80SmhMidSaturation, -1.0f, 1.0f, 0.0f);
    out.pd80SmhMidVibrance = ClampFinite(input.pd80SmhMidVibrance, -1.0f, 1.0f, 0.0f);
    out.pd80SmhBlendColorHighlight = {
        ClampUnit(input.pd80SmhBlendColorHighlight.x),
        ClampUnit(input.pd80SmhBlendColorHighlight.y),
        ClampUnit(input.pd80SmhBlendColorHighlight.z),
    };
    out.pd80SmhHighlightExposure = ClampFinite(input.pd80SmhHighlightExposure, -4.0f, 4.0f, 0.0f);
    out.pd80SmhHighlightContrast = ClampFinite(input.pd80SmhHighlightContrast, -1.0f, 1.5f, 0.0f);
    out.pd80SmhHighlightBrightness = ClampFinite(input.pd80SmhHighlightBrightness, -1.0f, 1.5f, 0.0f);
    out.pd80SmhHighlightBlendMode =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80SmhHighlightBlendMode)), 0, 20));
    out.pd80SmhHighlightOpacity = ClampFinite(input.pd80SmhHighlightOpacity, 0.0f, 1.0f, 0.0f);
    out.pd80SmhHighlightTint = ClampFinite(input.pd80SmhHighlightTint, -1.0f, 1.0f, 0.0f);
    out.pd80SmhHighlightSaturation = ClampFinite(input.pd80SmhHighlightSaturation, -1.0f, 1.0f, 0.0f);
    out.pd80SmhHighlightVibrance = ClampFinite(input.pd80SmhHighlightVibrance, -1.0f, 1.0f, 0.0f);
    out.pd80ClMasterStrength = ClampFinite(input.pd80ClMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80ClEnableDither = input.pd80ClEnableDither >= 0.5f ? 1.0f : 0.0f;
    out.pd80ClDitherStrength = ClampFinite(input.pd80ClDitherStrength, 0.0f, 10.0f, 1.0f);
    out.pd80ClEnableRgb = input.pd80ClEnableRgb >= 0.5f ? 1.0f : 0.0f;
    out.pd80ClGreyBlackIn = ClampFinite(input.pd80ClGreyBlackIn, 0.0f, 255.0f, 0.0f);
    out.pd80ClGreyWhiteIn = ClampFinite(input.pd80ClGreyWhiteIn, 0.0f, 255.0f, 255.0f);
    out.pd80ClGreyBlackOut = ClampFinite(input.pd80ClGreyBlackOut, 0.0f, 255.0f, 0.0f);
    out.pd80ClGreyWhiteOut = ClampFinite(input.pd80ClGreyWhiteOut, 0.0f, 255.0f, 255.0f);
    out.pd80ClGreyPos0Shoulder = ClampFinite(input.pd80ClGreyPos0Shoulder, 0.0f, 1.0f, 0.75f);
    out.pd80ClGreyPos1Shoulder = ClampFinite(input.pd80ClGreyPos1Shoulder, 0.0f, 1.0f, 0.75f);
    out.pd80ClGreyPos0Toe = ClampFinite(input.pd80ClGreyPos0Toe, 0.0f, 1.0f, 0.25f);
    out.pd80ClGreyPos1Toe = ClampFinite(input.pd80ClGreyPos1Toe, 0.0f, 1.0f, 0.25f);
    out.pd80ClRedBlackIn = ClampFinite(input.pd80ClRedBlackIn, 0.0f, 255.0f, 0.0f);
    out.pd80ClRedWhiteIn = ClampFinite(input.pd80ClRedWhiteIn, 0.0f, 255.0f, 255.0f);
    out.pd80ClRedBlackOut = ClampFinite(input.pd80ClRedBlackOut, 0.0f, 255.0f, 0.0f);
    out.pd80ClRedWhiteOut = ClampFinite(input.pd80ClRedWhiteOut, 0.0f, 255.0f, 255.0f);
    out.pd80ClRedPos0Shoulder = ClampFinite(input.pd80ClRedPos0Shoulder, 0.0f, 1.0f, 0.75f);
    out.pd80ClRedPos1Shoulder = ClampFinite(input.pd80ClRedPos1Shoulder, 0.0f, 1.0f, 0.75f);
    out.pd80ClRedPos0Toe = ClampFinite(input.pd80ClRedPos0Toe, 0.0f, 1.0f, 0.25f);
    out.pd80ClRedPos1Toe = ClampFinite(input.pd80ClRedPos1Toe, 0.0f, 1.0f, 0.25f);
    out.pd80ClGreenBlackIn = ClampFinite(input.pd80ClGreenBlackIn, 0.0f, 255.0f, 0.0f);
    out.pd80ClGreenWhiteIn = ClampFinite(input.pd80ClGreenWhiteIn, 0.0f, 255.0f, 255.0f);
    out.pd80ClGreenBlackOut = ClampFinite(input.pd80ClGreenBlackOut, 0.0f, 255.0f, 0.0f);
    out.pd80ClGreenWhiteOut = ClampFinite(input.pd80ClGreenWhiteOut, 0.0f, 255.0f, 255.0f);
    out.pd80ClGreenPos0Shoulder = ClampFinite(input.pd80ClGreenPos0Shoulder, 0.0f, 1.0f, 0.75f);
    out.pd80ClGreenPos1Shoulder = ClampFinite(input.pd80ClGreenPos1Shoulder, 0.0f, 1.0f, 0.75f);
    out.pd80ClGreenPos0Toe = ClampFinite(input.pd80ClGreenPos0Toe, 0.0f, 1.0f, 0.25f);
    out.pd80ClGreenPos1Toe = ClampFinite(input.pd80ClGreenPos1Toe, 0.0f, 1.0f, 0.25f);
    out.pd80ClBlueBlackIn = ClampFinite(input.pd80ClBlueBlackIn, 0.0f, 255.0f, 0.0f);
    out.pd80ClBlueWhiteIn = ClampFinite(input.pd80ClBlueWhiteIn, 0.0f, 255.0f, 255.0f);
    out.pd80ClBlueBlackOut = ClampFinite(input.pd80ClBlueBlackOut, 0.0f, 255.0f, 0.0f);
    out.pd80ClBlueWhiteOut = ClampFinite(input.pd80ClBlueWhiteOut, 0.0f, 255.0f, 255.0f);
    out.pd80ClBluePos0Shoulder = ClampFinite(input.pd80ClBluePos0Shoulder, 0.0f, 1.0f, 0.75f);
    out.pd80ClBluePos1Shoulder = ClampFinite(input.pd80ClBluePos1Shoulder, 0.0f, 1.0f, 0.75f);
    out.pd80ClBluePos0Toe = ClampFinite(input.pd80ClBluePos0Toe, 0.0f, 1.0f, 0.25f);
    out.pd80ClBluePos1Toe = ClampFinite(input.pd80ClBluePos1Toe, 0.0f, 1.0f, 0.25f);
    out.pd80ScMasterStrength = ClampFinite(input.pd80ScMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80ScCorrectionMethod =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80ScCorrectionMethod)), 0, 1));
    out.pd80ScCorrectionMethodSaturation =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80ScCorrectionMethodSaturation)), 0, 1));
    auto clampAdj = [](float v) { return ClampFinite(v, -1.0f, 1.0f, 0.0f); };
    out.pd80ScRedsCyan = clampAdj(input.pd80ScRedsCyan);
    out.pd80ScRedsMagenta = clampAdj(input.pd80ScRedsMagenta);
    out.pd80ScRedsYellow = clampAdj(input.pd80ScRedsYellow);
    out.pd80ScRedsBlack = clampAdj(input.pd80ScRedsBlack);
    out.pd80ScRedsSaturation = clampAdj(input.pd80ScRedsSaturation);
    out.pd80ScRedsVibrance = clampAdj(input.pd80ScRedsVibrance);
    out.pd80ScYellowsCyan = clampAdj(input.pd80ScYellowsCyan);
    out.pd80ScYellowsMagenta = clampAdj(input.pd80ScYellowsMagenta);
    out.pd80ScYellowsYellow = clampAdj(input.pd80ScYellowsYellow);
    out.pd80ScYellowsBlack = clampAdj(input.pd80ScYellowsBlack);
    out.pd80ScYellowsSaturation = clampAdj(input.pd80ScYellowsSaturation);
    out.pd80ScYellowsVibrance = clampAdj(input.pd80ScYellowsVibrance);
    out.pd80ScGreensCyan = clampAdj(input.pd80ScGreensCyan);
    out.pd80ScGreensMagenta = clampAdj(input.pd80ScGreensMagenta);
    out.pd80ScGreensYellow = clampAdj(input.pd80ScGreensYellow);
    out.pd80ScGreensBlack = clampAdj(input.pd80ScGreensBlack);
    out.pd80ScGreensSaturation = clampAdj(input.pd80ScGreensSaturation);
    out.pd80ScGreensVibrance = clampAdj(input.pd80ScGreensVibrance);
    out.pd80ScCyansCyan = clampAdj(input.pd80ScCyansCyan);
    out.pd80ScCyansMagenta = clampAdj(input.pd80ScCyansMagenta);
    out.pd80ScCyansYellow = clampAdj(input.pd80ScCyansYellow);
    out.pd80ScCyansBlack = clampAdj(input.pd80ScCyansBlack);
    out.pd80ScCyansSaturation = clampAdj(input.pd80ScCyansSaturation);
    out.pd80ScCyansVibrance = clampAdj(input.pd80ScCyansVibrance);
    out.pd80ScBluesCyan = clampAdj(input.pd80ScBluesCyan);
    out.pd80ScBluesMagenta = clampAdj(input.pd80ScBluesMagenta);
    out.pd80ScBluesYellow = clampAdj(input.pd80ScBluesYellow);
    out.pd80ScBluesBlack = clampAdj(input.pd80ScBluesBlack);
    out.pd80ScBluesSaturation = clampAdj(input.pd80ScBluesSaturation);
    out.pd80ScBluesVibrance = clampAdj(input.pd80ScBluesVibrance);
    out.pd80ScMagentasCyan = clampAdj(input.pd80ScMagentasCyan);
    out.pd80ScMagentasMagenta = clampAdj(input.pd80ScMagentasMagenta);
    out.pd80ScMagentasYellow = clampAdj(input.pd80ScMagentasYellow);
    out.pd80ScMagentasBlack = clampAdj(input.pd80ScMagentasBlack);
    out.pd80ScMagentasSaturation = clampAdj(input.pd80ScMagentasSaturation);
    out.pd80ScMagentasVibrance = clampAdj(input.pd80ScMagentasVibrance);
    out.pd80ScWhitesCyan = clampAdj(input.pd80ScWhitesCyan);
    out.pd80ScWhitesMagenta = clampAdj(input.pd80ScWhitesMagenta);
    out.pd80ScWhitesYellow = clampAdj(input.pd80ScWhitesYellow);
    out.pd80ScWhitesBlack = clampAdj(input.pd80ScWhitesBlack);
    out.pd80ScWhitesSaturation = clampAdj(input.pd80ScWhitesSaturation);
    out.pd80ScWhitesVibrance = clampAdj(input.pd80ScWhitesVibrance);
    out.pd80ScNeutralsCyan = clampAdj(input.pd80ScNeutralsCyan);
    out.pd80ScNeutralsMagenta = clampAdj(input.pd80ScNeutralsMagenta);
    out.pd80ScNeutralsYellow = clampAdj(input.pd80ScNeutralsYellow);
    out.pd80ScNeutralsBlack = clampAdj(input.pd80ScNeutralsBlack);
    out.pd80ScNeutralsSaturation = clampAdj(input.pd80ScNeutralsSaturation);
    out.pd80ScNeutralsVibrance = clampAdj(input.pd80ScNeutralsVibrance);
    out.pd80ScBlacksCyan = clampAdj(input.pd80ScBlacksCyan);
    out.pd80ScBlacksMagenta = clampAdj(input.pd80ScBlacksMagenta);
    out.pd80ScBlacksYellow = clampAdj(input.pd80ScBlacksYellow);
    out.pd80ScBlacksBlack = clampAdj(input.pd80ScBlacksBlack);
    out.pd80ScBlacksSaturation = clampAdj(input.pd80ScBlacksSaturation);
    out.pd80ScBlacksVibrance = clampAdj(input.pd80ScBlacksVibrance);
    out.pd80PpMasterStrength = ClampFinite(input.pd80PpMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80PpNumberOfLevels =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80PpNumberOfLevels)), 2, 255));
    out.pd80PpPixelSize =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80PpPixelSize)), 1, 9));
    out.pd80PpBorderStrength = ClampFinite(input.pd80PpBorderStrength, 0.0f, 1.0f, 0.0f);
    out.pd80PpEnableDither = input.pd80PpEnableDither >= 0.5f ? 1.0f : 0.0f;
    out.pd80PpDitherMotion = input.pd80PpDitherMotion >= 0.5f ? 1.0f : 0.0f;
    out.pd80PpDitherStrength = ClampFinite(input.pd80PpDitherStrength, 0.0f, 10.0f, 1.0f);
    out.pd80MrShape = ClampFinite(input.pd80MrShape, 0.0f, 1.0f, 0.0f);
    out.pd80MrInvertShape = input.pd80MrInvertShape >= 0.5f ? 1.0f : 0.0f;
    out.pd80MrRotation =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80MrRotation)), 0, 360));
    out.pd80MrCenter = {
        ClampFinite(input.pd80MrCenter.x, 0.0f, 1.0f, 0.5f),
        ClampFinite(input.pd80MrCenter.y, 0.0f, 1.0f, 0.5f),
    };
    out.pd80MrSizeX = ClampFinite(input.pd80MrSizeX, 0.0f, 0.5f, 0.125f);
    out.pd80MrSizeY = ClampFinite(input.pd80MrSizeY, 0.0f, 0.5f, 0.125f);
    out.pd80MrDepthPosition = ClampFinite(input.pd80MrDepthPosition, 0.0f, 1.0f, 0.0f);
    out.pd80MrSmoothing = ClampFinite(input.pd80MrSmoothing, 0.0f, 1.0f, 0.01f);
    out.pd80MrDepthSmoothing = ClampFinite(input.pd80MrDepthSmoothing, 0.0f, 1.0f, 0.002f);
    out.pd80MrDitherStrength = ClampFinite(input.pd80MrDitherStrength, 0.0f, 10.0f, 0.0f);
    out.pd80MrColor = {
        ClampFinite(input.pd80MrColor.x, 0.0f, 1.0f, 0.5f),
        ClampFinite(input.pd80MrColor.y, 0.0f, 1.0f, 0.5f),
        ClampFinite(input.pd80MrColor.z, 0.0f, 1.0f, 0.5f),
    };
    out.pd80MrExposure = ClampFinite(input.pd80MrExposure, -4.0f, 4.0f, 0.0f);
    out.pd80MrContrast = ClampFinite(input.pd80MrContrast, -1.0f, 1.0f, 0.0f);
    out.pd80MrBrightness = ClampFinite(input.pd80MrBrightness, -1.0f, 1.0f, 0.0f);
    out.pd80MrHue = ClampFinite(input.pd80MrHue, -1.0f, 1.0f, 0.0f);
    out.pd80MrSaturation = ClampFinite(input.pd80MrSaturation, -1.0f, 1.0f, 0.0f);
    out.pd80MrVibrance = ClampFinite(input.pd80MrVibrance, -1.0f, 1.0f, 0.0f);
    out.pd80MrEnableGradient = input.pd80MrEnableGradient >= 0.5f ? 1.0f : 0.0f;
    out.pd80MrGradientType = input.pd80MrGradientType >= 0.5f ? 1.0f : 0.0f;
    out.pd80MrGradientCurve = ClampFinite(input.pd80MrGradientCurve, 0.001f, 2.0f, 0.25f);
    out.pd80MrIntensityBoost = ClampFinite(input.pd80MrIntensityBoost, 1.0f, 4.0f, 1.0f);
    out.pd80MrBlendMode = ClampFinite(input.pd80MrBlendMode, 0.0f, 20.0f, 0.0f);
    out.pd80MrOpacity = ClampFinite(input.pd80MrOpacity, 0.0f, 1.0f, 1.0f);
    out.pd80BlpMasterStrength = ClampFinite(input.pd80BlpMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80BlpEnableDither = input.pd80BlpEnableDither >= 0.5f ? 1.0f : 0.0f;
    out.pd80BlpDitherStrength = ClampFinite(input.pd80BlpDitherStrength, 0.0f, 10.0f, 1.0f);
    out.pd80BlpLutSelector =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80BlpLutSelector)), 0, 49));
    out.pd80BlpMixChroma = ClampFinite(input.pd80BlpMixChroma, 0.0f, 1.0f, 1.0f);
    out.pd80BlpMixLuma = ClampFinite(input.pd80BlpMixLuma, 0.0f, 1.0f, 1.0f);
    out.pd80BlpBlackIn = {
        ClampFinite(input.pd80BlpBlackIn.x, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80BlpBlackIn.y, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80BlpBlackIn.z, 0.0f, 1.0f, 0.0f),
    };
    out.pd80BlpWhiteIn = {
        ClampFinite(input.pd80BlpWhiteIn.x, 0.0f, 1.0f, 1.0f),
        ClampFinite(input.pd80BlpWhiteIn.y, 0.0f, 1.0f, 1.0f),
        ClampFinite(input.pd80BlpWhiteIn.z, 0.0f, 1.0f, 1.0f),
    };
    out.pd80BlpBlackOut = {
        ClampFinite(input.pd80BlpBlackOut.x, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80BlpBlackOut.y, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80BlpBlackOut.z, 0.0f, 1.0f, 0.0f),
    };
    out.pd80BlpWhiteOut = {
        ClampFinite(input.pd80BlpWhiteOut.x, 0.0f, 1.0f, 1.0f),
        ClampFinite(input.pd80BlpWhiteOut.y, 0.0f, 1.0f, 1.0f),
        ClampFinite(input.pd80BlpWhiteOut.z, 0.0f, 1.0f, 1.0f),
    };
    out.pd80BlpGamma = ClampFinite(input.pd80BlpGamma, 0.05f, 10.0f, 1.0f);
    out.pd80CltMasterStrength = ClampFinite(input.pd80CltMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80CltEnableDither = input.pd80CltEnableDither >= 0.5f ? 1.0f : 0.0f;
    out.pd80CltDitherStrength = ClampFinite(input.pd80CltDitherStrength, 0.0f, 10.0f, 1.0f);
    out.pd80CltLutSelector =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80CltLutSelector)), 0, 30));
    out.pd80CltMixChroma = ClampFinite(input.pd80CltMixChroma, 0.0f, 1.0f, 1.0f);
    out.pd80CltMixLuma = ClampFinite(input.pd80CltMixLuma, 0.0f, 1.0f, 1.0f);
    out.pd80CltBlackIn = {
        ClampFinite(input.pd80CltBlackIn.x, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80CltBlackIn.y, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80CltBlackIn.z, 0.0f, 1.0f, 0.0f),
    };
    out.pd80CltWhiteIn = {
        ClampFinite(input.pd80CltWhiteIn.x, 0.0f, 1.0f, 1.0f),
        ClampFinite(input.pd80CltWhiteIn.y, 0.0f, 1.0f, 1.0f),
        ClampFinite(input.pd80CltWhiteIn.z, 0.0f, 1.0f, 1.0f),
    };
    out.pd80CltBlackOut = {
        ClampFinite(input.pd80CltBlackOut.x, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80CltBlackOut.y, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80CltBlackOut.z, 0.0f, 1.0f, 0.0f),
    };
    out.pd80CltWhiteOut = {
        ClampFinite(input.pd80CltWhiteOut.x, 0.0f, 1.0f, 1.0f),
        ClampFinite(input.pd80CltWhiteOut.y, 0.0f, 1.0f, 1.0f),
        ClampFinite(input.pd80CltWhiteOut.z, 0.0f, 1.0f, 1.0f),
    };
    out.pd80CltGamma = ClampFinite(input.pd80CltGamma, 0.05f, 10.0f, 1.0f);
    out.pd80LcMasterStrength = ClampFinite(input.pd80LcMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80LcTextureWidth = ClampFinite(input.pd80LcTextureWidth, 1.0f, 4096.0f, 512.0f);
    out.pd80LcTextureHeight = ClampFinite(input.pd80LcTextureHeight, 1.0f, 4096.0f, 512.0f);
    out.pd80LfMasterStrength = ClampFinite(input.pd80LfMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80LfTransitionSpeed = ClampFinite(input.pd80LfTransitionSpeed, 0.0f, 1.0f, 0.5f);
    out.pd80LfMinLevel = ClampFinite(input.pd80LfMinLevel, 0.0f, 1.0f, 0.125f);
    out.pd80LfMaxLevel = ClampFinite(input.pd80LfMaxLevel, 0.0f, 1.0f, 0.3f);
    out.pd80Cg4MasterStrength = ClampFinite(input.pd80Cg4MasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80Cg4LumaMode = ClampFinite(input.pd80Cg4LumaMode, 0.0f, 2.0f, 0.0f);
    out.pd80Cg4SeparationMode = ClampFinite(input.pd80Cg4SeparationMode, 0.0f, 1.0f, 0.0f);
    out.pd80Cg4EnableDither = input.pd80Cg4EnableDither >= 0.5f ? 1.0f : 0.0f;
    out.pd80Cg4DitherStrength = ClampFinite(input.pd80Cg4DitherStrength, 0.0f, 10.0f, 1.0f);
    out.pd80Cg4DesaturateBase = ClampFinite(input.pd80Cg4DesaturateBase, 0.0f, 1.0f, 0.0f);
    out.pd80Cg4FinalMix = ClampFinite(input.pd80Cg4FinalMix, 0.0f, 1.0f, 0.333f);
    out.pd80Cg4LightSceneMidColor = {
        ClampFinite(input.pd80Cg4LightSceneMidColor.x, 0.0f, 1.0f, 0.98f),
        ClampFinite(input.pd80Cg4LightSceneMidColor.y, 0.0f, 1.0f, 0.588f),
        ClampFinite(input.pd80Cg4LightSceneMidColor.z, 0.0f, 1.0f, 0.0f),
    };
    out.pd80Cg4LightSceneMidBlendMode = ClampFinite(input.pd80Cg4LightSceneMidBlendMode, 0.0f, 20.0f, 10.0f);
    out.pd80Cg4LightSceneMidOpacity = ClampFinite(input.pd80Cg4LightSceneMidOpacity, 0.0f, 1.0f, 1.0f);
    out.pd80Cg4LightSceneShadowColor = {
        ClampFinite(input.pd80Cg4LightSceneShadowColor.x, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80Cg4LightSceneShadowColor.y, 0.0f, 1.0f, 0.365f),
        ClampFinite(input.pd80Cg4LightSceneShadowColor.z, 0.0f, 1.0f, 1.0f),
    };
    out.pd80Cg4LightSceneShadowBlendMode = ClampFinite(input.pd80Cg4LightSceneShadowBlendMode, 0.0f, 20.0f, 5.0f);
    out.pd80Cg4LightSceneShadowOpacity = ClampFinite(input.pd80Cg4LightSceneShadowOpacity, 0.0f, 1.0f, 0.3f);
    out.pd80Cg4EnableDarkScene = input.pd80Cg4EnableDarkScene >= 0.5f ? 1.0f : 0.0f;
    out.pd80Cg4DarkSceneMidColor = {
        ClampFinite(input.pd80Cg4DarkSceneMidColor.x, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80Cg4DarkSceneMidColor.y, 0.0f, 1.0f, 0.365f),
        ClampFinite(input.pd80Cg4DarkSceneMidColor.z, 0.0f, 1.0f, 1.0f),
    };
    out.pd80Cg4DarkSceneMidBlendMode = ClampFinite(input.pd80Cg4DarkSceneMidBlendMode, 0.0f, 20.0f, 10.0f);
    out.pd80Cg4DarkSceneMidOpacity = ClampFinite(input.pd80Cg4DarkSceneMidOpacity, 0.0f, 1.0f, 1.0f);
    out.pd80Cg4DarkSceneShadowColor = {
        ClampFinite(input.pd80Cg4DarkSceneShadowColor.x, 0.0f, 1.0f, 0.0f),
        ClampFinite(input.pd80Cg4DarkSceneShadowColor.y, 0.0f, 1.0f, 0.039f),
        ClampFinite(input.pd80Cg4DarkSceneShadowColor.z, 0.0f, 1.0f, 0.588f),
    };
    out.pd80Cg4DarkSceneShadowBlendMode = ClampFinite(input.pd80Cg4DarkSceneShadowBlendMode, 0.0f, 20.0f, 10.0f);
    out.pd80Cg4DarkSceneShadowOpacity = ClampFinite(input.pd80Cg4DarkSceneShadowOpacity, 0.0f, 1.0f, 1.0f);
    out.pd80Cg4MinLevel = ClampFinite(input.pd80Cg4MinLevel, 0.0f, 1.0f, 0.125f);
    out.pd80Cg4MaxLevel = ClampFinite(input.pd80Cg4MaxLevel, 0.0f, 1.0f, 0.3f);
    out.pd80CcMasterStrength = ClampFinite(input.pd80CcMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80CcEnableWhitepoint = input.pd80CcEnableWhitepoint >= 0.5f ? 1.0f : 0.0f;
    out.pd80CcWhitepointStrength = ClampFinite(input.pd80CcWhitepointStrength, 0.0f, 1.0f, 1.0f);
    out.pd80CcEnableBlackpoint = input.pd80CcEnableBlackpoint >= 0.5f ? 1.0f : 0.0f;
    out.pd80CcBlackpointStrength = ClampFinite(input.pd80CcBlackpointStrength, 0.0f, 1.0f, 1.0f);
    out.pd80RccMasterStrength = ClampFinite(input.pd80RccMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80RccEnableDither = input.pd80RccEnableDither >= 0.5f ? 1.0f : 0.0f;
    out.pd80RccDitherStrength = ClampFinite(input.pd80RccDitherStrength, 0.0f, 10.0f, 1.0f);
    out.pd80RccEnableWhitepoint = input.pd80RccEnableWhitepoint >= 0.5f ? 1.0f : 0.0f;
    out.pd80RccWhitepointRespectLuma = input.pd80RccWhitepointRespectLuma >= 0.5f ? 1.0f : 0.0f;
    out.pd80RccWhitepointMethod =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80RccWhitepointMethod)), 0, 1));
    out.pd80RccWhitepointStrength = ClampFinite(input.pd80RccWhitepointStrength, 0.0f, 1.0f, 1.0f);
    out.pd80RccWhitepointLumaStrength = ClampFinite(input.pd80RccWhitepointLumaStrength, 0.0f, 1.0f, 1.0f);
    out.pd80RccEnableBlackpoint = input.pd80RccEnableBlackpoint >= 0.5f ? 1.0f : 0.0f;
    out.pd80RccBlackpointRespectLuma = input.pd80RccBlackpointRespectLuma >= 0.5f ? 1.0f : 0.0f;
    out.pd80RccBlackpointMethod =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80RccBlackpointMethod)), 0, 1));
    out.pd80RccBlackpointStrength = ClampFinite(input.pd80RccBlackpointStrength, 0.0f, 1.0f, 1.0f);
    out.pd80RccBlackpointLumaStrength = ClampFinite(input.pd80RccBlackpointLumaStrength, 0.0f, 1.0f, 1.0f);
    out.pd80RccEnableMidpoint = input.pd80RccEnableMidpoint >= 0.5f ? 1.0f : 0.0f;
    out.pd80RccMidpointRespectLuma = input.pd80RccMidpointRespectLuma >= 0.5f ? 1.0f : 0.0f;
    out.pd80RccMidUseAltMethod = input.pd80RccMidUseAltMethod >= 0.5f ? 1.0f : 0.0f;
    out.pd80RccMidScale = ClampFinite(input.pd80RccMidScale, 0.0f, 5.0f, 0.5f);
    out.pd80FaMasterStrength = ClampFinite(input.pd80FaMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80FaAdjustShoulder = ClampFinite(input.pd80FaAdjustShoulder, 1.0f, 5.0f, 1.0f);
    out.pd80FaAdjustLinear = ClampFinite(input.pd80FaAdjustLinear, 1.0f, 10.0f, 1.0f);
    out.pd80FaAdjustToe = ClampFinite(input.pd80FaAdjustToe, 1.0f, 5.0f, 1.0f);
    out.pd80HbMasterStrength = ClampFinite(input.pd80HbMasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80HbDebugBloom = input.pd80HbDebugBloom >= 0.5f ? 1.0f : 0.0f;
    out.pd80HbDitherStrength = ClampFinite(input.pd80HbDitherStrength, 0.0f, 10.0f, 2.0f);
    out.pd80HbMix = ClampFinite(input.pd80HbMix, 0.0f, 1.0f, 0.5f);
    out.pd80HbThreshold = ClampFinite(input.pd80HbThreshold, 0.0f, 1.0f, 0.333f);
    out.pd80HbGreyValue = ClampFinite(input.pd80HbGreyValue, 0.0f, 1.0f, 0.333f);
    out.pd80HbExposure = ClampFinite(input.pd80HbExposure, -1.0f, 5.0f, 0.0f);
    out.pd80HbBlurSigma = ClampFinite(input.pd80HbBlurSigma, 10.0f, 300.0f, 30.0f);
    out.pd80HbSaturation = ClampFinite(input.pd80HbSaturation, 0.0f, 2.0f, 0.0f);
    out.pd80Sc2MasterStrength = ClampFinite(input.pd80Sc2MasterStrength, 0.0f, 1.0f, 0.0f);
    out.pd80Sc2CorrectionMethod =
        static_cast<float>(std::clamp(static_cast<int>(std::lround(input.pd80Sc2CorrectionMethod)), 0, 1));
    out.pd80Sc2SaturationScale = ClampFinite(input.pd80Sc2SaturationScale, 0.0f, 2.0f, 1.0f);
    out.pd80Sc2LightnessScale = ClampFinite(input.pd80Sc2LightnessScale, 0.0f, 2.0f, 1.0f);
    out.colourfulness = ClampFinite(input.colourfulness, -1.0f, 2.0f, 0.0f);
    out.colourfulnessLimitLuma = ClampFinite(input.colourfulnessLimitLuma, 0.1f, 1.0f, 0.7f);
    out.filmicPassStrength = ClampFinite(input.filmicPassStrength, 0.0f, 1.5f, 0.0f);
    out.filmicPassFade = ClampFinite(input.filmicPassFade, 0.0f, 0.6f, 0.4f);
    out.filmicPassBleach = ClampFinite(input.filmicPassBleach, -0.5f, 1.0f, 0.0f);
    out.filmicPassSaturation = ClampFinite(input.filmicPassSaturation, -1.0f, 1.0f, -0.15f);
    out.filmGrain2Amount = ClampFinite(input.filmGrain2Amount, 0.0f, 0.2f, 0.0f);
    out.filmGrain2ColorAmount = ClampFinite(input.filmGrain2ColorAmount, 0.0f, 1.0f, 0.6f);
    out.filmGrain2LuminanceAmount = ClampFinite(input.filmGrain2LuminanceAmount, 0.0f, 1.0f, 1.0f);
    out.filmGrain2Size = ClampFinite(input.filmGrain2Size, 1.5f, 2.5f, 1.6f);
    out.denoiseStrength = ClampFinite(input.denoiseStrength, 0.0f, 1.0f, 0.0f);
    out.denoiseNoiseLevel = ClampFinite(input.denoiseNoiseLevel, 0.01f, 1.0f, 0.15f);
    out.denoiseLerpCoefficient = ClampFinite(input.denoiseLerpCoefficient, 0.0f, 1.0f, 0.8f);
    out.denoiseWeightThreshold = ClampFinite(input.denoiseWeightThreshold, 0.0f, 1.0f, 0.03f);
    out.denoiseCounterThreshold = ClampFinite(input.denoiseCounterThreshold, 0.0f, 1.0f, 0.05f);
    out.denoiseGaussianSigma = ClampFinite(input.denoiseGaussianSigma, 1.0f, 100.0f, 50.0f);
    out.adaptiveSharpenStrength = ClampFinite(input.adaptiveSharpenStrength, 0.0f, 2.0f, 0.0f);
    out.adaptiveSharpenCurveSlope = ClampFinite(input.adaptiveSharpenCurveSlope, 0.01f, 2.0f, 0.5f);
    out.adaptiveSharpenLightOvershoot = ClampFinite(input.adaptiveSharpenLightOvershoot, 0.001f, 0.1f, 0.003f);
    out.adaptiveSharpenDarkOvershoot = ClampFinite(input.adaptiveSharpenDarkOvershoot, 0.001f, 0.1f, 0.009f);
    out.adaptiveSharpenLightComprLow = ClampFinite(input.adaptiveSharpenLightComprLow, 0.0f, 1.0f, 0.167f);
    out.adaptiveSharpenLightComprHigh = ClampFinite(input.adaptiveSharpenLightComprHigh, 0.0f, 1.0f, 0.334f);
    out.adaptiveSharpenDarkComprLow = ClampFinite(input.adaptiveSharpenDarkComprLow, 0.0f, 1.0f, 0.250f);
    out.adaptiveSharpenDarkComprHigh = ClampFinite(input.adaptiveSharpenDarkComprHigh, 0.0f, 1.0f, 0.500f);
    out.adaptiveSharpenScaleLim = ClampFinite(input.adaptiveSharpenScaleLim, 0.01f, 1.0f, 0.1f);
    out.adaptiveSharpenScaleCs = ClampFinite(input.adaptiveSharpenScaleCs, 0.0f, 1.0f, 0.056f);
    out.adaptiveSharpenPmP = ClampFinite(input.adaptiveSharpenPmP, 0.01f, 1.0f, 0.7f);
    out.gaussianBlurStrength = ClampFinite(input.gaussianBlurStrength, 0.0f, 1.0f, 0.0f);
    out.gaussianBlurOffset = ClampFinite(input.gaussianBlurOffset, 0.0f, 1.0f, 1.0f);
    out.gaussianBlurRadius = std::clamp(input.gaussianBlurRadius, 0, 4);
    out.fineSharpStrength = ClampFinite(input.fineSharpStrength, 0.0f, 8.0f, 0.0f);
    out.fineSharpEqualization = ClampFinite(input.fineSharpEqualization, 0.0f, 1.249f, 0.9f);
    out.fineSharpXStrength = ClampFinite(input.fineSharpXStrength, 0.0f, 1.0f, 0.19f);
    out.fineSharpXRepair = ClampFinite(input.fineSharpXRepair, 0.0f, 1.0f, 0.25f);
    out.fineSharpLStrength = ClampFinite(input.fineSharpLStrength, 0.01f, 8.0f, 1.49f);
    out.fineSharpPStrength = ClampFinite(input.fineSharpPStrength, 0.01f, 8.0f, 1.272f);
    out.fineSharpMode = std::clamp(input.fineSharpMode, 0, 2);
    out.martyBloomThreshold = ClampFinite(input.martyBloomThreshold, 0.1f, 1.0f, 0.8f);
    out.martyBloomAmount = ClampFinite(input.martyBloomAmount, 0.0f, 20.0f, 0.0f);
    out.martyBloomSaturation = ClampFinite(input.martyBloomSaturation, 0.0f, 2.0f, 0.8f);
    out.martyBloomMixMode = std::clamp(input.martyBloomMixMode, 0, 3);
    out.martyBloomTint = {
        ClampFinite(input.martyBloomTint.x, 0.0f, 4.0f, 0.7f),
        ClampFinite(input.martyBloomTint.y, 0.0f, 4.0f, 0.8f),
        ClampFinite(input.martyBloomTint.z, 0.0f, 4.0f, 1.0f),
    };
    out.creatorDofStrength = ClampFinite(input.creatorDofStrength, 0.0f, 1.0f, 0.0f);
    out.creatorDofAutoFocus = input.creatorDofAutoFocus;
    out.creatorDofManualFocusDepth = ClampFinite(input.creatorDofManualFocusDepth, 0.0f, 1.0f, 0.02f);
    out.creatorDofInfiniteFocus = ClampFinite(input.creatorDofInfiniteFocus, 0.01f, 1.0f, 1.0f);
    out.creatorDofFocusPoint = {
        ClampFinite(input.creatorDofFocusPoint.x, 0.0f, 1.0f, 0.5f),
        ClampFinite(input.creatorDofFocusPoint.y, 0.0f, 1.0f, 0.5f),
    };
    out.creatorDofFocusRadius = ClampFinite(input.creatorDofFocusRadius, 0.02f, 0.2f, 0.05f);
    out.creatorDofFocusSamples = std::clamp(input.creatorDofFocusSamples, 3, 10);
    out.creatorDofNearBlurCurve = ClampFinite(input.creatorDofNearBlurCurve, 0.5f, 1000.0f, 1.6f);
    out.creatorDofFarBlurCurve = ClampFinite(input.creatorDofFarBlurCurve, 0.05f, 5.0f, 2.0f);
    out.creatorDofBlurRadius = ClampFinite(input.creatorDofBlurRadius, 2.0f, 100.0f, 15.0f);
    out.creatorDofRingSamples = std::clamp(input.creatorDofRingSamples, 5, 30);
    out.creatorDofRingRings = std::clamp(input.creatorDofRingRings, 1, 8);
    out.creatorDofRingThreshold = ClampFinite(input.creatorDofRingThreshold, 0.5f, 3.0f, 0.7f);
    out.creatorDofRingGain = ClampFinite(input.creatorDofRingGain, 0.1f, 30.0f, 27.0f);
    out.creatorDofRingBias = ClampFinite(input.creatorDofRingBias, 0.0f, 2.0f, 0.0f);
    out.creatorDofRingFringe = ClampFinite(input.creatorDofRingFringe, 0.0f, 1.0f, 0.5f);
    out.ambientLightIntensity = ClampFinite(input.ambientLightIntensity, 0.0f, 20.0f, 0.0f);
    out.ambientLightThreshold = ClampFinite(input.ambientLightThreshold, 0.0f, 100.0f, 15.0f);
    out.ambientLightAdaptation = input.ambientLightAdaptation;
    out.ambientLightAdapt = ClampFinite(input.ambientLightAdapt, 0.0f, 4.0f, 0.7f);
    out.ambientLightAdaptBaseMult = ClampFinite(input.ambientLightAdaptBaseMult, 0.0f, 4.0f, 1.0f);
    out.ambientLightAdaptBlackLevel = std::clamp(input.ambientLightAdaptBlackLevel, 0, 4);
    out.ambientLightDither = input.ambientLightDither;
    out.ambientLightDirt = input.ambientLightDirt;
    out.ambientLightAdaptiveMode = std::clamp(input.ambientLightAdaptiveMode, 0, 2);
    out.ambientLightDirtInt = ClampFinite(input.ambientLightDirtInt, 0.0f, 2.0f, 1.0f);
    out.ambientLightDirtOvrInt = ClampFinite(input.ambientLightDirtOvrInt, 0.0f, 2.0f, 1.0f);
    out.fakeMotionBlurRecall = ClampFinite(input.fakeMotionBlurRecall, 0.0f, 1.0f, 0.0f);
    out.fakeMotionBlurSoftness = ClampFinite(input.fakeMotionBlurSoftness, 0.0f, 2.0f, 1.0f);
    out.reflectiveBumpMappingStrength = ClampFinite(input.reflectiveBumpMappingStrength, 0.0f, 1.0f, 0.0f);
    out.reflectiveBumpMappingBlurWidthPixels =
        ClampFinite(input.reflectiveBumpMappingBlurWidthPixels, 0.0f, 400.0f, 100.0f);
    out.reflectiveBumpMappingSampleCount = std::clamp(input.reflectiveBumpMappingSampleCount, 16, 128);
    out.reflectiveBumpMappingReliefHeight =
        ClampFinite(input.reflectiveBumpMappingReliefHeight, 0.0f, 2.0f, 0.3f);
    out.reflectiveBumpMappingFresnelReflectance =
        ClampFinite(input.reflectiveBumpMappingFresnelReflectance, 0.0f, 1.0f, 0.3f);
    out.reflectiveBumpMappingFresnelMult =
        ClampFinite(input.reflectiveBumpMappingFresnelMult, 0.0f, 1.0f, 0.5f);
    out.reflectiveBumpMappingLowerThreshold =
        ClampFinite(input.reflectiveBumpMappingLowerThreshold, 0.0f, 1.0f, 0.1f);
    out.reflectiveBumpMappingUpperThreshold =
        ClampFinite(input.reflectiveBumpMappingUpperThreshold, 0.0f, 1.0f, 0.2f);
    out.reflectiveBumpMappingColorMaskRed =
        ClampFinite(input.reflectiveBumpMappingColorMaskRed, 0.0f, 1.0f, 1.0f);
    out.reflectiveBumpMappingColorMaskOrange =
        ClampFinite(input.reflectiveBumpMappingColorMaskOrange, 0.0f, 1.0f, 1.0f);
    out.reflectiveBumpMappingColorMaskYellow =
        ClampFinite(input.reflectiveBumpMappingColorMaskYellow, 0.0f, 1.0f, 1.0f);
    out.reflectiveBumpMappingColorMaskGreen =
        ClampFinite(input.reflectiveBumpMappingColorMaskGreen, 0.0f, 1.0f, 1.0f);
    out.reflectiveBumpMappingColorMaskCyan =
        ClampFinite(input.reflectiveBumpMappingColorMaskCyan, 0.0f, 1.0f, 1.0f);
    out.reflectiveBumpMappingColorMaskBlue =
        ClampFinite(input.reflectiveBumpMappingColorMaskBlue, 0.0f, 1.0f, 1.0f);
    out.reflectiveBumpMappingColorMaskMagenta =
        ClampFinite(input.reflectiveBumpMappingColorMaskMagenta, 0.0f, 1.0f, 1.0f);
    out.reflectiveBumpMappingDepthFarPlane =
        ClampFinite(input.reflectiveBumpMappingDepthFarPlane, 1.0f, 10000.0f, 1000.0f);
    out.cropScaleContentWidth = ClampFinite(input.cropScaleContentWidth, 0.0f, 65536.0f, 0.0f);
    out.cropScaleContentHeight = ClampFinite(input.cropScaleContentHeight, 0.0f, 65536.0f, 0.0f);
    out.cropScaleIntermediateWidth = ClampFinite(input.cropScaleIntermediateWidth, 0.0f, 65536.0f, 0.0f);
    out.cropScaleIntermediateHeight = ClampFinite(input.cropScaleIntermediateHeight, 0.0f, 65536.0f, 0.0f);
    out.cropScaleFinalWidth = ClampFinite(input.cropScaleFinalWidth, 0.0f, 65536.0f, 0.0f);
    out.cropScaleFinalHeight = ClampFinite(input.cropScaleFinalHeight, 0.0f, 65536.0f, 0.0f);
    out.cropScaleFilter = std::clamp(input.cropScaleFilter, 0, 1);
    out.cropScaleStrength = ClampFinite(input.cropScaleStrength, 0.0f, 1.0f, 0.0f);
    out.barbatosFakeHdrPreset = std::clamp(input.barbatosFakeHdrPreset, 0, 2);
    out.barbatosFakeHdrStrength = ClampFinite(input.barbatosFakeHdrStrength, 0.0f, 2.0f, 0.0f);
    out.riAdaptiveDebandStrength = ClampFinite(input.riAdaptiveDebandStrength, 0.0f, 1.0f, 0.0f);
    out.riAdaptiveDebandRadius = ClampFinite(input.riAdaptiveDebandRadius, 0.0f, 128.0f, 24.0f);
    out.riAdaptiveDebandThreshold = ClampFinite(input.riAdaptiveDebandThreshold, 0.0001f, 0.1f, 0.012f);
    out.riAdaptiveDebandIterations = std::clamp(input.riAdaptiveDebandIterations, 1, 3);
    out.riLocalSharpenStrength = ClampFinite(input.riLocalSharpenStrength, 0.0f, 2.0f, 0.0f);
    out.riLocalSharpenRadius = ClampFinite(input.riLocalSharpenRadius, 0.5f, 4.0f, 1.0f);
    out.riLocalSharpenClamp = ClampFinite(input.riLocalSharpenClamp, 0.0f, 0.25f, 0.08f);
    out.riLocalSharpenEdgeLimit = ClampFinite(input.riLocalSharpenEdgeLimit, 0.0f, 1.0f, 0.65f);
    out.riOutlineStrength = ClampFinite(input.riOutlineStrength, 0.0f, 1.0f, 0.0f);
    out.riOutlineThickness = ClampFinite(input.riOutlineThickness, 0.0f, 10.0f, 1.5f);
    out.riOutlineDepthSensitivity = ClampFinite(input.riOutlineDepthSensitivity, 0.0001f, 0.1f, 0.01f);
    out.riOutlineColorSensitivity = ClampFinite(input.riOutlineColorSensitivity, 0.001f, 2.0f, 0.30f);
    out.riOutlineMethod = std::clamp(input.riOutlineMethod, 0, 3);
    out.riOutlineColor = {
        ClampUnit(input.riOutlineColor.x),
        ClampUnit(input.riOutlineColor.y),
        ClampUnit(input.riOutlineColor.z),
    };
    out.riOutlineWobbleAmount = ClampFinite(input.riOutlineWobbleAmount, 0.0f, 10.0f, 0.0f);
    out.riOutlineWobbleSpeed = ClampFinite(input.riOutlineWobbleSpeed, 0.0f, 5.0f, 1.0f);
    out.riOutlineWobbleFrequency = ClampFinite(input.riOutlineWobbleFrequency, 1.0f, 50.0f, 10.0f);
    out.riOutlineDebug = ClampFinite(input.riOutlineDebug, 0.0f, 1.0f, 0.0f);
    return out;
}

inline PostProcessParameters MakePostProcessPreset(PostProcessPreset preset) {
    switch (preset) {
        case PostProcessPreset::CrispGameplay:
            return PostProcessParameters{
                .noiseAmount = 0.0015f,
                .scanlineAmount = 0.002f,
                .barrelDistortion = 0.002f,
                .chromaticAberration = 0.00015f,
                .tintColor = {1.0f, 1.0f, 1.0f},
                .tintStrength = 0.0f,
                .blurAmount = 0.0f,
                .staticFadeAmount = 0.0f,
                .casSharpenAmount = 0.62f,
                .casContrastAdaptation = 0.38f,
                .bloomIntensity = 0.045f,
                .bloomThreshold = 1.35f,
                .debandStrength = 0.032f,
                .toneCurveStrength = 0.12f,
                .outputDitherStrength = 0.42f,
            };
        case PostProcessPreset::SoftVhs:
            return PostProcessParameters{
                .noiseAmount = 0.0025f,
                .scanlineAmount = 0.004f,
                .barrelDistortion = 0.001f,
                .chromaticAberration = 0.00035f,
                .tintColor = {1.0f, 1.0f, 1.0f},
                .tintStrength = 0.0f,
                .blurAmount = 0.0f,
                .staticFadeAmount = 0.0f,
                .casSharpenAmount = 0.22f,
                .casContrastAdaptation = 0.28f,
                .bloomIntensity = 0.028f,
                .bloomThreshold = 1.1f,
                .debandStrength = 0.048f,
                .toneCurveStrength = 0.08f,
                .outputDitherStrength = 0.55f,
            };
        case PostProcessPreset::Vhs:
            return PostProcessParameters{
                .noiseAmount = 0.004f,
                .scanlineAmount = 0.008f,
                .barrelDistortion = 0.0f,
                .chromaticAberration = 0.0007f,
                .tintColor = {1.0f, 1.0f, 1.0f},
                .tintStrength = 0.0f,
                .blurAmount = 0.0f,
                .staticFadeAmount = 0.0f,
                .casSharpenAmount = 0.18f,
                .casContrastAdaptation = 0.25f,
                .bloomIntensity = 0.055f,
                .bloomThreshold = 0.95f,
                .debandStrength = 0.042f,
                .toneCurveStrength = 0.18f,
                .outputDitherStrength = 0.62f,
            };
        case PostProcessPreset::AnalogHorror:
            return PostProcessParameters{
                .noiseAmount = 0.03f,
                .scanlineAmount = 0.045f,
                .barrelDistortion = 0.03f,
                .chromaticAberration = 0.0012f,
                .tintColor = {1.0f, 1.0f, 1.0f},
                .tintStrength = 0.0f,
                .blurAmount = 0.0f,
                .staticFadeAmount = 0.0f,
                .casSharpenAmount = 0.35f,
                .casContrastAdaptation = 0.32f,
                .bloomIntensity = 0.08f,
                .bloomThreshold = 0.75f,
                .debandStrength = 0.055f,
                .toneCurveStrength = 0.22f,
                .outputDitherStrength = 0.5f,
                .vignetteStrength = 0.28f,
                .filmGrainIntensity = 0.16f,
            };
        case PostProcessPreset::ColdFacility:
            return PostProcessParameters{
                .noiseAmount = 0.0035f,
                .scanlineAmount = 0.006f,
                .barrelDistortion = 0.003f,
                .chromaticAberration = 0.00045f,
                .tintColor = {0.72f, 0.88f, 1.0f},
                .tintStrength = 0.16f,
                .blurAmount = 0.0015f,
                .staticFadeAmount = 0.0f,
                .casSharpenAmount = 0.45f,
                .casContrastAdaptation = 0.42f,
                .bloomIntensity = 0.035f,
                .bloomThreshold = 1.45f,
                .debandStrength = 0.038f,
                .toneCurveStrength = 0.1f,
                .outputDitherStrength = 0.38f,
            };
        case PostProcessPreset::IndustrialHaze:
            return PostProcessParameters{
                .noiseAmount = 0.006f,
                .scanlineAmount = 0.004f,
                .barrelDistortion = 0.005f,
                .chromaticAberration = 0.00035f,
                .tintColor = {1.0f, 0.82f, 0.68f},
                .tintStrength = 0.22f,
                .blurAmount = 0.004f,
                .staticFadeAmount = 0.0f,
                .casSharpenAmount = 0.2f,
                .casContrastAdaptation = 0.22f,
                .bloomIntensity = 0.09f,
                .bloomThreshold = 1.05f,
                .debandStrength = 0.05f,
                .toneCurveStrength = 0.06f,
                .outputDitherStrength = 0.45f,
            };
        case PostProcessPreset::DreamPulse:
            return PostProcessParameters{
                .noiseAmount = 0.009f,
                .scanlineAmount = 0.0025f,
                .barrelDistortion = 0.008f,
                .chromaticAberration = 0.0016f,
                .tintColor = {0.86f, 0.74f, 1.0f},
                .tintStrength = 0.28f,
                .blurAmount = 0.008f,
                .staticFadeAmount = 0.0f,
                .casSharpenAmount = 0.12f,
                .casContrastAdaptation = 0.2f,
                .bloomIntensity = 0.12f,
                .bloomThreshold = 0.85f,
                .debandStrength = 0.06f,
                .toneCurveStrength = 0.14f,
                .outputDitherStrength = 0.48f,
            };
        case PostProcessPreset::CombatFocus:
            return PostProcessParameters{
                .noiseAmount = 0.0045f,
                .scanlineAmount = 0.0015f,
                .barrelDistortion = 0.0f,
                .chromaticAberration = 0.00055f,
                .tintColor = {1.0f, 0.86f, 0.82f},
                .tintStrength = 0.12f,
                .blurAmount = 0.0f,
                .staticFadeAmount = 0.0f,
                .casSharpenAmount = 0.55f,
                .casContrastAdaptation = 0.45f,
                .bloomIntensity = 0.04f,
                .bloomThreshold = 1.4f,
                .debandStrength = 0.028f,
                .toneCurveStrength = 0.06f,
                .outputDitherStrength = 0.35f,
            };
        case PostProcessPreset::StaticTransition:
            return PostProcessParameters{
                .noiseAmount = 0.04f,
                .scanlineAmount = 0.02f,
                .barrelDistortion = 0.0f,
                .chromaticAberration = 0.0006f,
                .tintColor = {1.0f, 1.0f, 1.0f},
                .tintStrength = 0.0f,
                .blurAmount = 0.0f,
                .staticFadeAmount = 1.0f,
                .casSharpenAmount = 0.1f,
                .casContrastAdaptation = 0.15f,
                .bloomIntensity = 0.02f,
                .bloomThreshold = 1.6f,
                .debandStrength = 0.04f,
                .toneCurveStrength = 0.02f,
                .outputDitherStrength = 0.85f,
            };
        case PostProcessPreset::Neutral:
        default:
            return PostProcessParameters{
                .casSharpenAmount = 0.0f,
                .casContrastAdaptation = 0.0f,
                .bloomIntensity = 0.0f,
                .bloomThreshold = 0.0f,
                .debandStrength = 0.0f,
                .toneCurveStrength = 0.0f,
                .outputDitherStrength = 0.0f,
                .vignetteStrength = 0.0f,
                .filmGrainIntensity = 0.0f,
                .liftRgb = {1.0f, 1.0f, 1.0f},
                .gammaRgb = {1.0f, 1.0f, 1.0f},
                .gainRgb = {1.0f, 1.0f, 1.0f},
                .liftGammaGainMix = 0.0f,
                .vibrance = 0.0f,
                .vibranceRgbBalance = {1.0f, 1.0f, 1.0f},
                .technicolorPower = 4.0f,
                .technicolorRgbNegative = {0.88f, 0.88f, 0.88f},
                .technicolorStrength = 0.0f,
                .technicolor2ColorStrength = {0.2f, 0.2f, 0.2f},
                .technicolor2Brightness = 1.0f,
                .technicolor2Saturation = 1.0f,
                .technicolor2Strength = 0.0f,
                .sepiaTint = {0.55f, 0.43f, 0.42f},
                .sepiaStrength = 0.0f,
                .monochromePreset = 0,
                .monochromeCustomCoeff = {0.21f, 0.72f, 0.07f},
                .monochromeColorSaturation = 1.0f,
                .dpxRgbCurve = {8.0f, 8.0f, 8.0f},
                .dpxRgbC = {0.36f, 0.36f, 0.34f},
                .dpxContrast = 0.1f,
                .dpxSaturation = 3.0f,
                .dpxColorfulness = 2.5f,
                .dpxStrength = 0.0f,
                .colorMatrixRed = {0.817f, 0.183f, 0.0f},
                .colorMatrixGreen = {0.333f, 0.667f, 0.0f},
                .colorMatrixBlue = {0.0f, 0.125f, 0.875f},
                .colorMatrixStrength = 0.0f,
                .fakeHdrPower = 1.30f,
                .fakeHdrRadius1 = 0.793f,
                .fakeHdrRadius2 = 0.87f,
                .fakeHdrStrength = 0.0f,
                .levelsBlackPoint = 16.0f,
                .levelsWhitePoint = 235.0f,
                .levelsStrength = 0.0f,
                .levelsClipHighlight = 0.0f,
                .lumaSharpenStrength = 0.0f,
                .lumaSharpenClamp = 0.035f,
                .lumaSharpenPattern = 1,
                .lumaSharpenOffsetBias = 1.0f,
                .lumaSharpenShowPattern = 0.0f,
                .sweetFxCurvesMode = 0,
                .sweetFxCurvesFormula = 4,
                .sweetFxCurvesContrast = 0.65f,
                .sweetFxCurvesStrength = 0.0f,
                .sweetFxChromaticAberrationShiftX = 2.5f,
                .sweetFxChromaticAberrationShiftY = -0.5f,
                .sweetFxChromaticAberrationStrength = 0.0f,
                .sweetFxBorderWidthX = 0.0f,
                .sweetFxBorderWidthY = 0.0f,
                .sweetFxBorderRatio = 2.35f,
                .sweetFxBorderColor = {0.0f, 0.0f, 0.0f},
                .sweetFxBorderStrength = 0.0f,
                .sweetFxCartoonPower = 1.5f,
                .sweetFxCartoonEdgeSlope = 1.5f,
                .sweetFxCartoonStrength = 0.0f,
                .sweetFxTonemapGamma = 1.0f,
                .sweetFxTonemapExposure = 0.0f,
                .sweetFxTonemapSaturation = 0.0f,
                .sweetFxTonemapBleach = 0.0f,
                .sweetFxTonemapDefog = 0.0f,
                .sweetFxTonemapFogColor = {0.0f, 0.0f, 1.0f},
                .sweetFxTonemapStrength = 0.0f,
                .sweetFxSplitscreenMode = 0,
                .sweetFxSplitscreenStrength = 0.0f,
                .sweetFxNostalgiaPalette = 1,
                .sweetFxNostalgiaScanlines = 1,
                .sweetFxNostalgiaDither = 0.0f,
                .sweetFxNostalgiaStrength = 0.0f,
                .sweetFxCompareMode = 7,
                .sweetFxCompareDifferenceScale = 5.0f,
                .sweetFxCompareStrength = 0.0f,
                .sweetFxLayerPosition = {0.5f, 0.5f},
                .sweetFxLayerScale = 1.0f,
                .sweetFxLayerBlend = 0.0f,
                .sweetFxLayerTexWidth = 1280.0f,
                .sweetFxLayerTexHeight = 720.0f,
                .sweetFxFxaaSubpix = 0.25f,
                .sweetFxFxaaEdgeThreshold = 0.125f,
                .sweetFxFxaaEdgeThresholdMin = 0.0f,
                .sweetFxFxaaStrength = 0.0f,
                .sweetFxCrtAmount = 0.0f,
                .sweetFxCrtResolution = 1.15f,
                .sweetFxCrtGamma = 2.4f,
                .sweetFxCrtMonitorGamma = 2.2f,
                .sweetFxCrtBrightness = 0.9f,
                .sweetFxCrtScanlineIntensity = 2,
                .sweetFxCrtScanlineGaussian = 1.0f,
                .sweetFxCrtCurvature = 0.0f,
                .sweetFxCrtCurvatureRadius = 1.5f,
                .sweetFxCrtCornerSize = 0.01f,
                .sweetFxCrtViewerDistance = 2.0f,
                .sweetFxCrtAngle = {0.0f, 0.0f},
                .sweetFxCrtOverscan = 1.01f,
                .sweetFxCrtOversample = 1.0f,
                .sweetFxAsciiSpacing = 1,
                .sweetFxAsciiFont = 1,
                .sweetFxAsciiFontColorMode = 1,
                .sweetFxAsciiFontColor = {1.0f, 1.0f, 1.0f},
                .sweetFxAsciiBackgroundColor = {0.0f, 0.0f, 0.0f},
                .sweetFxAsciiSwapColors = 0.0f,
                .sweetFxAsciiInvertBrightness = 0.0f,
                .sweetFxAsciiDithering = 1.0f,
                .sweetFxAsciiDitheringIntensity = 2.0f,
                .sweetFxAsciiDitheringDebugGradient = 0.0f,
                .sweetFxAsciiStrength = 0.0f,
                .sweetFxSmaaEdgeDetectionType = 1,
                .sweetFxSmaaEdgeThreshold = 0.10f,
                .sweetFxSmaaDepthThreshold = 0.01f,
                .sweetFxSmaaMaxSearchSteps = 32,
                .sweetFxSmaaMaxSearchStepsDiagonal = 16,
                .sweetFxSmaaCornerRounding = 25,
                .sweetFxSmaaDebugOutput = 0.0f,
                .sweetFxSmaaStrength = 0.0f,
                .reshadeDaltonizeType = 0,
                .reshadeDaltonizeStrength = 0.0f,
                .reshadeDisplayDepthPresentType = 2,
                .reshadeDisplayDepthStrength = 0.0f,
                .reshadeLutAmountChroma = 1.0f,
                .reshadeLutAmountLuma = 1.0f,
                .reshadeLutStrength = 0.0f,
                .pd80TechnicolorStrength = 0.0f,
                .pd80TechnicolorRed2strip = {1.0f, 0.098f, 0.0f},
                .pd80TechnicolorCyan2strip = {0.0f, 0.988f, 1.0f},
                .pd80TechnicolorColorKey = {1.0f, 1.0f, 1.0f},
                .pd80TechnicolorSaturation2 = 1.5f,
                .pd80TechnicolorEnable3strip = 0.0f,
                .pd80Technicolor3ColorStrength = {0.2f, 0.2f, 0.2f},
                .pd80Technicolor3Brightness = 1.0f,
                .pd80Technicolor3Saturation = 1.0f,
                .pd80Technicolor3Strength = 1.0f,
                .pd80ColorTemperatureKelvin = 6500.0f,
                .pd80ColorTemperatureLuminancePreservation = 1.0f,
                .pd80ColorTemperatureMix = 1.0f,
                .pd80ColorTemperatureStrength = 0.0f,
                .pd80SaturationLimit = 1.0f,
                .pd80SaturationLimitStrength = 0.0f,
                .pd80ColorBalanceShadow = {},
                .pd80ColorBalanceMid = {},
                .pd80ColorBalanceHigh = {},
                .pd80ColorBalancePreserveLuma = 1.0f,
                .pd80ColorBalanceSeparationMode = 0.0f,
                .pd80ColorBalanceStrength = 0.0f,
                .pd80ColorIsolationHueMid = 0.0f,
                .pd80ColorIsolationHueRange = 0.167f,
                .pd80ColorIsolationSatLimit = 1.0f,
                .pd80ColorIsolationFxMix = 1.0f,
                .pd80ColorIsolationStrength = 0.0f,
                .pd80LevelsBlackIn = {},
                .pd80LevelsWhiteIn = {1.0f, 1.0f, 1.0f},
                .pd80LevelsBlackOut = {},
                .pd80LevelsWhiteOut = {1.0f, 1.0f, 1.0f},
                .pd80LevelsGamma = 1.0f,
                .pd80LevelsEnableDither = 1.0f,
                .pd80LevelsDitherStrength = 1.0f,
                .pd80LevelsStrength = 0.0f,
                .pd80BlackWhiteMode = 13.0f,
                .pd80BlackWhiteCurveStr = 1.5f,
                .pd80BlackWhiteEnableDither = 1.0f,
                .pd80BlackWhiteDitherStrength = 1.0f,
                .pd80BlackWhiteRedChannel = 0.2f,
                .pd80BlackWhiteYellowChannel = 0.4f,
                .pd80BlackWhiteGreenChannel = 0.6f,
                .pd80BlackWhiteCyanChannel = 0.0f,
                .pd80BlackWhiteBlueChannel = -0.6f,
                .pd80BlackWhiteMagentaChannel = -0.2f,
                .pd80BlackWhiteUseTint = 0.0f,
                .pd80BlackWhiteTintHue = 0.083f,
                .pd80BlackWhiteTintSat = 0.12f,
                .pd80BlackWhiteShowClip = 0.0f,
                .pd80BlackWhiteStrength = 0.0f,
                .pd80CbsEnableDither = 1.0f,
                .pd80CbsDitherStrength = 1.0f,
                .pd80CbsTint = 0.0f,
                .pd80CbsExposure = 0.0f,
                .pd80CbsContrast = 0.0f,
                .pd80CbsBrightness = 0.0f,
                .pd80CbsSaturation = 0.0f,
                .pd80CbsVibrance = 0.0f,
                .pd80CbsHueMid = 0.0f,
                .pd80CbsHueRange = 0.167f,
                .pd80CbsSatCustom = 0.0f,
                .pd80CbsSatR = 0.0f,
                .pd80CbsSatY = 0.0f,
                .pd80CbsSatG = 0.0f,
                .pd80CbsSatA = 0.0f,
                .pd80CbsSatB = 0.0f,
                .pd80CbsSatP = 0.0f,
                .pd80CbsSatM = 0.0f,
                .pd80CbsEnableDepth = 0.0f,
                .pd80CbsDisplayDepth = 0.0f,
                .pd80CbsDepthStart = 0.0f,
                .pd80CbsDepthEnd = 0.1f,
                .pd80CbsDepthCurve = 1.0f,
                .pd80CbsExposureFar = 0.0f,
                .pd80CbsContrastFar = 0.0f,
                .pd80CbsBrightnessFar = 0.0f,
                .pd80CbsSaturationFar = 0.0f,
                .pd80CbsVibranceFar = 0.0f,
                .pd80CbsStrength = 0.0f,
                .pd80CaMasterStrength = 0.0f,
                .pd80CaEffectStrength = 1.0f,
                .pd80CaGlobalWidth = -12.0f,
                .pd80CaSampleSteps = 24.0f,
                .pd80CaType = 0.0f,
                .pd80CaDegrees = 135.0f,
                .pd80CaWidth = 1.0f,
                .pd80CaCurve = 1.0f,
                .pd80CaOX = 0.0f,
                .pd80CaOY = 0.0f,
                .pd80CaShapeX = 1.0f,
                .pd80CaShapeY = 1.0f,
                .pd80CaVignetteColor = {},
                .pd80CaShowCa = 0.0f,
                .pd80CaEnableDepthInt = 0.0f,
                .pd80CaEnableDepthWidth = 0.0f,
                .pd80CaDisplayDepth = 0.0f,
                .pd80CaDepthStart = 0.0f,
                .pd80CaDepthEnd = 0.1f,
                .pd80CaDepthCurve = 1.0f,
                .pd80LsMasterStrength = 0.0f,
                .pd80LsBlurSigma = 0.45f,
                .pd80LsSharpening = 1.7f,
                .pd80LsThreshold = 0.0f,
                .pd80LsLimiter = 0.03f,
                .pd80LsShowEdges = 0.0f,
                .pd80LsEnableDepth = 0.0f,
                .pd80LsEnableReverse = 0.0f,
                .pd80LsDisplayDepth = 0.0f,
                .pd80LsDepthStart = 0.0f,
                .pd80LsDepthEnd = 0.1f,
                .pd80LsDepthCurve = 1.0f,
                .pd80FgMasterStrength = 0.0f,
                .pd80FgGrainAdjust = 1.0f,
                .pd80FgGrainSize = 1.0f,
                .pd80FgGrainMotion = 1.0f,
                .pd80FgGrainOrigColor = 1.0f,
                .pd80FgUseNegnoise = 0.0f,
                .pd80FgGrainColor = 1.0f,
                .pd80FgGrainAmount = 0.333f,
                .pd80FgGrainIntensity = 0.65f,
                .pd80FgGrainDensity = 10.0f,
                .pd80FgGrainIntHigh = 1.0f,
                .pd80FgGrainIntLow = 1.0f,
                .pd80FgEnableTest = 0.0f,
                .pd80FgEnableDepth = 0.0f,
                .pd80FgDisplayDepth = 0.0f,
                .pd80FgDepthStart = 0.0f,
                .pd80FgDepthEnd = 0.1f,
                .pd80FgDepthCurve = 1.0f,
                .pd80DsMasterStrength = 0.0f,
                .pd80DsDepthNear = 0.0f,
                .pd80DsDepthPos = 0.015f,
                .pd80DsDepthFar = 0.0f,
                .pd80DsDepthSmoothing = 0.005f,
                .pd80DsIntensity = 0.0f,
                .pd80DsHue = 0.083f,
                .pd80DsSaturation = 0.0f,
                .pd80DsBlendMode = 0.0f,
                .pd80DsOpacity = 1.0f,
                .pd80CgMasterStrength = 0.0f,
                .pd80ColorGamut = 0.0f,
                .pd80CscMasterStrength = 0.0f,
                .pd80CscEnableDither = 1.0f,
                .pd80CscDitherStrength = 1.0f,
                .pd80CscColorSpace = 1.0f,
                .pd80CscPos0ToeGrey = 0.2f,
                .pd80CscPos1ToeGrey = 0.2f,
                .pd80CscPos0ShoulderGrey = 0.8f,
                .pd80CscPos1ShoulderGrey = 0.8f,
                .pd80CscColorSat = 0.0f,
                .pd80SmhMasterStrength = 0.0f,
                .pd80SmhLumaMode = 2.0f,
                .pd80SmhSeparationMode = 0.0f,
                .pd80SmhEnableDither = 1.0f,
                .pd80SmhDitherStrength = 2.0f,
                .pd80SmhBlendColorShadow = {0.0f, 0.365f, 1.0f},
                .pd80SmhShadowExposure = 0.0f,
                .pd80SmhShadowContrast = 0.0f,
                .pd80SmhShadowBrightness = 0.0f,
                .pd80SmhShadowBlendMode = 0.0f,
                .pd80SmhShadowOpacity = 0.0f,
                .pd80SmhShadowTint = 0.0f,
                .pd80SmhShadowSaturation = 0.0f,
                .pd80SmhShadowVibrance = 0.0f,
                .pd80SmhBlendColorMid = {0.98f, 0.588f, 0.0f},
                .pd80SmhMidExposure = 0.0f,
                .pd80SmhMidContrast = 0.0f,
                .pd80SmhMidBrightness = 0.0f,
                .pd80SmhMidBlendMode = 0.0f,
                .pd80SmhMidOpacity = 0.0f,
                .pd80SmhMidTint = 0.0f,
                .pd80SmhMidSaturation = 0.0f,
                .pd80SmhMidVibrance = 0.0f,
                .pd80SmhBlendColorHighlight = {1.0f, 1.0f, 1.0f},
                .pd80SmhHighlightExposure = 0.0f,
                .pd80SmhHighlightContrast = 0.0f,
                .pd80SmhHighlightBrightness = 0.0f,
                .pd80SmhHighlightBlendMode = 0.0f,
                .pd80SmhHighlightOpacity = 0.0f,
                .pd80SmhHighlightTint = 0.0f,
                .pd80SmhHighlightSaturation = 0.0f,
                .pd80SmhHighlightVibrance = 0.0f,
                .pd80ClMasterStrength = 0.0f,
                .pd80ClEnableDither = 1.0f,
                .pd80ClDitherStrength = 1.0f,
                .pd80ClEnableRgb = 0.0f,
                .pd80ClGreyBlackIn = 0.0f,
                .pd80ClGreyWhiteIn = 255.0f,
                .pd80ClGreyBlackOut = 0.0f,
                .pd80ClGreyWhiteOut = 255.0f,
                .pd80ClGreyPos0Shoulder = 0.75f,
                .pd80ClGreyPos1Shoulder = 0.75f,
                .pd80ClGreyPos0Toe = 0.25f,
                .pd80ClGreyPos1Toe = 0.25f,
                .pd80ClRedBlackIn = 0.0f,
                .pd80ClRedWhiteIn = 255.0f,
                .pd80ClRedBlackOut = 0.0f,
                .pd80ClRedWhiteOut = 255.0f,
                .pd80ClRedPos0Shoulder = 0.75f,
                .pd80ClRedPos1Shoulder = 0.75f,
                .pd80ClRedPos0Toe = 0.25f,
                .pd80ClRedPos1Toe = 0.25f,
                .pd80ClGreenBlackIn = 0.0f,
                .pd80ClGreenWhiteIn = 255.0f,
                .pd80ClGreenBlackOut = 0.0f,
                .pd80ClGreenWhiteOut = 255.0f,
                .pd80ClGreenPos0Shoulder = 0.75f,
                .pd80ClGreenPos1Shoulder = 0.75f,
                .pd80ClGreenPos0Toe = 0.25f,
                .pd80ClGreenPos1Toe = 0.25f,
                .pd80ClBlueBlackIn = 0.0f,
                .pd80ClBlueWhiteIn = 255.0f,
                .pd80ClBlueBlackOut = 0.0f,
                .pd80ClBlueWhiteOut = 255.0f,
                .pd80ClBluePos0Shoulder = 0.75f,
                .pd80ClBluePos1Shoulder = 0.75f,
                .pd80ClBluePos0Toe = 0.25f,
                .pd80ClBluePos1Toe = 0.25f,
                .pd80ScMasterStrength = 0.0f,
                .pd80ScCorrectionMethod = 1.0f,
                .pd80ScCorrectionMethodSaturation = 1.0f,
                .pd80ScRedsCyan = 0.0f,
                .pd80ScRedsMagenta = 0.0f,
                .pd80ScRedsYellow = 0.0f,
                .pd80ScRedsBlack = 0.0f,
                .pd80ScRedsSaturation = 0.0f,
                .pd80ScRedsVibrance = 0.0f,
                .pd80ScYellowsCyan = 0.0f,
                .pd80ScYellowsMagenta = 0.0f,
                .pd80ScYellowsYellow = 0.0f,
                .pd80ScYellowsBlack = 0.0f,
                .pd80ScYellowsSaturation = 0.0f,
                .pd80ScYellowsVibrance = 0.0f,
                .pd80ScGreensCyan = 0.0f,
                .pd80ScGreensMagenta = 0.0f,
                .pd80ScGreensYellow = 0.0f,
                .pd80ScGreensBlack = 0.0f,
                .pd80ScGreensSaturation = 0.0f,
                .pd80ScGreensVibrance = 0.0f,
                .pd80ScCyansCyan = 0.0f,
                .pd80ScCyansMagenta = 0.0f,
                .pd80ScCyansYellow = 0.0f,
                .pd80ScCyansBlack = 0.0f,
                .pd80ScCyansSaturation = 0.0f,
                .pd80ScCyansVibrance = 0.0f,
                .pd80ScBluesCyan = 0.0f,
                .pd80ScBluesMagenta = 0.0f,
                .pd80ScBluesYellow = 0.0f,
                .pd80ScBluesBlack = 0.0f,
                .pd80ScBluesSaturation = 0.0f,
                .pd80ScBluesVibrance = 0.0f,
                .pd80ScMagentasCyan = 0.0f,
                .pd80ScMagentasMagenta = 0.0f,
                .pd80ScMagentasYellow = 0.0f,
                .pd80ScMagentasBlack = 0.0f,
                .pd80ScMagentasSaturation = 0.0f,
                .pd80ScMagentasVibrance = 0.0f,
                .pd80ScWhitesCyan = 0.0f,
                .pd80ScWhitesMagenta = 0.0f,
                .pd80ScWhitesYellow = 0.0f,
                .pd80ScWhitesBlack = 0.0f,
                .pd80ScWhitesSaturation = 0.0f,
                .pd80ScWhitesVibrance = 0.0f,
                .pd80ScNeutralsCyan = 0.0f,
                .pd80ScNeutralsMagenta = 0.0f,
                .pd80ScNeutralsYellow = 0.0f,
                .pd80ScNeutralsBlack = 0.0f,
                .pd80ScNeutralsSaturation = 0.0f,
                .pd80ScNeutralsVibrance = 0.0f,
                .pd80ScBlacksCyan = 0.0f,
                .pd80ScBlacksMagenta = 0.0f,
                .pd80ScBlacksYellow = 0.0f,
                .pd80ScBlacksBlack = 0.0f,
                .pd80ScBlacksSaturation = 0.0f,
                .pd80ScBlacksVibrance = 0.0f,
                .pd80PpMasterStrength = 0.0f,
                .pd80PpNumberOfLevels = 255.0f,
                .pd80PpPixelSize = 1.0f,
                .pd80PpBorderStrength = 0.0f,
                .pd80PpEnableDither = 0.0f,
                .pd80PpDitherMotion = 1.0f,
                .pd80PpDitherStrength = 1.0f,
                .pd80MrShape = 0.0f,
                .pd80MrInvertShape = 0.0f,
                .pd80MrRotation = 45.0f,
                .pd80MrCenter = {0.5f, 0.5f},
                .pd80MrSizeX = 0.125f,
                .pd80MrSizeY = 0.125f,
                .pd80MrDepthPosition = 0.0f,
                .pd80MrSmoothing = 0.01f,
                .pd80MrDepthSmoothing = 0.002f,
                .pd80MrDitherStrength = 0.0f,
                .pd80MrColor = {0.5f, 0.5f, 0.5f},
                .pd80MrExposure = 0.0f,
                .pd80MrContrast = 0.0f,
                .pd80MrBrightness = 0.0f,
                .pd80MrHue = 0.0f,
                .pd80MrSaturation = 0.0f,
                .pd80MrVibrance = 0.0f,
                .pd80MrEnableGradient = 0.0f,
                .pd80MrGradientType = 0.0f,
                .pd80MrGradientCurve = 0.25f,
                .pd80MrIntensityBoost = 1.0f,
                .pd80MrBlendMode = 0.0f,
                .pd80MrOpacity = 1.0f,
                .pd80BlpMasterStrength = 0.0f,
                .pd80BlpEnableDither = 1.0f,
                .pd80BlpDitherStrength = 1.0f,
                .pd80BlpLutSelector = 0.0f,
                .pd80BlpMixChroma = 1.0f,
                .pd80BlpMixLuma = 1.0f,
                .pd80BlpBlackIn = {0.0f, 0.0f, 0.0f},
                .pd80BlpWhiteIn = {1.0f, 1.0f, 1.0f},
                .pd80BlpBlackOut = {0.0f, 0.0f, 0.0f},
                .pd80BlpWhiteOut = {1.0f, 1.0f, 1.0f},
                .pd80BlpGamma = 1.0f,
                .pd80CltMasterStrength = 0.0f,
                .pd80CltEnableDither = 1.0f,
                .pd80CltDitherStrength = 1.0f,
                .pd80CltLutSelector = 0.0f,
                .pd80CltMixChroma = 1.0f,
                .pd80CltMixLuma = 1.0f,
                .pd80CltBlackIn = {0.0f, 0.0f, 0.0f},
                .pd80CltWhiteIn = {1.0f, 1.0f, 1.0f},
                .pd80CltBlackOut = {0.0f, 0.0f, 0.0f},
                .pd80CltWhiteOut = {1.0f, 1.0f, 1.0f},
                .pd80CltGamma = 1.0f,
                .pd80LcMasterStrength = 0.0f,
                .pd80LcTextureWidth = 512.0f,
                .pd80LcTextureHeight = 512.0f,
                .pd80LfMasterStrength = 0.0f,
                .pd80LfTransitionSpeed = 0.5f,
                .pd80LfMinLevel = 0.125f,
                .pd80LfMaxLevel = 0.3f,
                .pd80Cg4MasterStrength = 0.0f,
                .pd80Cg4LumaMode = 0.0f,
                .pd80Cg4SeparationMode = 0.0f,
                .pd80Cg4EnableDither = 1.0f,
                .pd80Cg4DitherStrength = 1.0f,
                .pd80Cg4DesaturateBase = 0.0f,
                .pd80Cg4FinalMix = 0.333f,
                .pd80Cg4LightSceneMidColor = {0.98f, 0.588f, 0.0f},
                .pd80Cg4LightSceneMidBlendMode = 10.0f,
                .pd80Cg4LightSceneMidOpacity = 1.0f,
                .pd80Cg4LightSceneShadowColor = {0.0f, 0.365f, 1.0f},
                .pd80Cg4LightSceneShadowBlendMode = 5.0f,
                .pd80Cg4LightSceneShadowOpacity = 0.3f,
                .pd80Cg4EnableDarkScene = 1.0f,
                .pd80Cg4DarkSceneMidColor = {0.0f, 0.365f, 1.0f},
                .pd80Cg4DarkSceneMidBlendMode = 10.0f,
                .pd80Cg4DarkSceneMidOpacity = 1.0f,
                .pd80Cg4DarkSceneShadowColor = {0.0f, 0.039f, 0.588f},
                .pd80Cg4DarkSceneShadowBlendMode = 10.0f,
                .pd80Cg4DarkSceneShadowOpacity = 1.0f,
                .pd80Cg4MinLevel = 0.125f,
                .pd80Cg4MaxLevel = 0.3f,
                .pd80CcMasterStrength = 0.0f,
                .pd80CcEnableWhitepoint = 0.0f,
                .pd80CcWhitepointStrength = 1.0f,
                .pd80CcEnableBlackpoint = 1.0f,
                .pd80CcBlackpointStrength = 1.0f,
                .pd80RccMasterStrength = 0.0f,
                .pd80RccEnableDither = 1.0f,
                .pd80RccDitherStrength = 1.0f,
                .pd80RccEnableWhitepoint = 1.0f,
                .pd80RccWhitepointRespectLuma = 1.0f,
                .pd80RccWhitepointMethod = 0.0f,
                .pd80RccWhitepointStrength = 1.0f,
                .pd80RccWhitepointLumaStrength = 1.0f,
                .pd80RccEnableBlackpoint = 1.0f,
                .pd80RccBlackpointRespectLuma = 0.0f,
                .pd80RccBlackpointMethod = 1.0f,
                .pd80RccBlackpointStrength = 1.0f,
                .pd80RccBlackpointLumaStrength = 1.0f,
                .pd80RccEnableMidpoint = 1.0f,
                .pd80RccMidpointRespectLuma = 1.0f,
                .pd80RccMidUseAltMethod = 1.0f,
                .pd80RccMidScale = 0.5f,
                .pd80FaMasterStrength = 0.0f,
                .pd80FaAdjustShoulder = 1.0f,
                .pd80FaAdjustLinear = 1.0f,
                .pd80FaAdjustToe = 1.0f,
                .pd80HbMasterStrength = 0.0f,
                .pd80HbDebugBloom = 0.0f,
                .pd80HbDitherStrength = 2.0f,
                .pd80HbMix = 0.5f,
                .pd80HbThreshold = 0.333f,
                .pd80HbGreyValue = 0.333f,
                .pd80HbExposure = 0.0f,
                .pd80HbBlurSigma = 30.0f,
                .pd80HbSaturation = 0.0f,
                .pd80Sc2MasterStrength = 0.0f,
                .pd80Sc2CorrectionMethod = 1.0f,
                .pd80Sc2SaturationScale = 1.0f,
                .pd80Sc2LightnessScale = 1.0f,
            };
    }
}

inline PostProcessParameters BlendPostProcessParameters(
    const PostProcessParameters& lhs,
    const PostProcessParameters& rhs,
    float alpha) {
    const float t = ClampUnit(alpha);
    const PostProcessParameters a = SanitizePostProcessParameters(lhs);
    const PostProcessParameters b = SanitizePostProcessParameters(rhs);
    return SanitizePostProcessParameters(PostProcessParameters{
        .timeSeconds = a.timeSeconds + ((b.timeSeconds - a.timeSeconds) * t),
        .noiseAmount = a.noiseAmount + ((b.noiseAmount - a.noiseAmount) * t),
        .scanlineAmount = a.scanlineAmount + ((b.scanlineAmount - a.scanlineAmount) * t),
        .barrelDistortion = a.barrelDistortion + ((b.barrelDistortion - a.barrelDistortion) * t),
        .chromaticAberration = a.chromaticAberration + ((b.chromaticAberration - a.chromaticAberration) * t),
        .tintColor = ri::math::Lerp(a.tintColor, b.tintColor, t),
        .tintStrength = a.tintStrength + ((b.tintStrength - a.tintStrength) * t),
        .blurAmount = a.blurAmount + ((b.blurAmount - a.blurAmount) * t),
        .staticFadeAmount = a.staticFadeAmount + ((b.staticFadeAmount - a.staticFadeAmount) * t),
        .casSharpenAmount = a.casSharpenAmount + ((b.casSharpenAmount - a.casSharpenAmount) * t),
        .casContrastAdaptation = a.casContrastAdaptation + ((b.casContrastAdaptation - a.casContrastAdaptation) * t),
        .bloomIntensity = a.bloomIntensity + ((b.bloomIntensity - a.bloomIntensity) * t),
        .bloomThreshold = a.bloomThreshold + ((b.bloomThreshold - a.bloomThreshold) * t),
        .debandStrength = a.debandStrength + ((b.debandStrength - a.debandStrength) * t),
        .toneCurveStrength = a.toneCurveStrength + ((b.toneCurveStrength - a.toneCurveStrength) * t),
        .outputDitherStrength =
            a.outputDitherStrength + ((b.outputDitherStrength - a.outputDitherStrength) * t),
        .vignetteStrength = a.vignetteStrength + ((b.vignetteStrength - a.vignetteStrength) * t),
        .filmGrainIntensity = a.filmGrainIntensity + ((b.filmGrainIntensity - a.filmGrainIntensity) * t),
        .liftRgb = ri::math::Lerp(a.liftRgb, b.liftRgb, t),
        .gammaRgb = ri::math::Lerp(a.gammaRgb, b.gammaRgb, t),
        .gainRgb = ri::math::Lerp(a.gainRgb, b.gainRgb, t),
        .liftGammaGainMix = a.liftGammaGainMix + ((b.liftGammaGainMix - a.liftGammaGainMix) * t),
        .vibrance = a.vibrance + ((b.vibrance - a.vibrance) * t),
        .vibranceRgbBalance = ri::math::Lerp(a.vibranceRgbBalance, b.vibranceRgbBalance, t),
        .technicolorPower = a.technicolorPower + ((b.technicolorPower - a.technicolorPower) * t),
        .technicolorRgbNegative = ri::math::Lerp(a.technicolorRgbNegative, b.technicolorRgbNegative, t),
        .technicolorStrength = a.technicolorStrength + ((b.technicolorStrength - a.technicolorStrength) * t),
        .technicolor2ColorStrength = ri::math::Lerp(a.technicolor2ColorStrength, b.technicolor2ColorStrength, t),
        .technicolor2Brightness = a.technicolor2Brightness + ((b.technicolor2Brightness - a.technicolor2Brightness) * t),
        .technicolor2Saturation = a.technicolor2Saturation + ((b.technicolor2Saturation - a.technicolor2Saturation) * t),
        .technicolor2Strength = a.technicolor2Strength + ((b.technicolor2Strength - a.technicolor2Strength) * t),
        .sepiaTint = ri::math::Lerp(a.sepiaTint, b.sepiaTint, t),
        .sepiaStrength = a.sepiaStrength + ((b.sepiaStrength - a.sepiaStrength) * t),
        .monochromePreset = std::clamp(
            static_cast<int>(std::lround(
                std::lerp(static_cast<float>(a.monochromePreset), static_cast<float>(b.monochromePreset), t))),
            0,
            17),
        .monochromeCustomCoeff =
            ri::math::Lerp(a.monochromeCustomCoeff, b.monochromeCustomCoeff, t),
        .monochromeColorSaturation =
            a.monochromeColorSaturation
            + ((b.monochromeColorSaturation - a.monochromeColorSaturation) * t),
        .dpxRgbCurve = ri::math::Lerp(a.dpxRgbCurve, b.dpxRgbCurve, t),
        .dpxRgbC = ri::math::Lerp(a.dpxRgbC, b.dpxRgbC, t),
        .dpxContrast = a.dpxContrast + ((b.dpxContrast - a.dpxContrast) * t),
        .dpxSaturation = a.dpxSaturation + ((b.dpxSaturation - a.dpxSaturation) * t),
        .dpxColorfulness = a.dpxColorfulness + ((b.dpxColorfulness - a.dpxColorfulness) * t),
        .dpxStrength = a.dpxStrength + ((b.dpxStrength - a.dpxStrength) * t),
        .colorMatrixRed = ri::math::Lerp(a.colorMatrixRed, b.colorMatrixRed, t),
        .colorMatrixGreen = ri::math::Lerp(a.colorMatrixGreen, b.colorMatrixGreen, t),
        .colorMatrixBlue = ri::math::Lerp(a.colorMatrixBlue, b.colorMatrixBlue, t),
        .colorMatrixStrength =
            a.colorMatrixStrength + ((b.colorMatrixStrength - a.colorMatrixStrength) * t),
        .fakeHdrPower = a.fakeHdrPower + ((b.fakeHdrPower - a.fakeHdrPower) * t),
        .fakeHdrRadius1 = a.fakeHdrRadius1 + ((b.fakeHdrRadius1 - a.fakeHdrRadius1) * t),
        .fakeHdrRadius2 = a.fakeHdrRadius2 + ((b.fakeHdrRadius2 - a.fakeHdrRadius2) * t),
        .fakeHdrStrength = a.fakeHdrStrength + ((b.fakeHdrStrength - a.fakeHdrStrength) * t),
        .levelsBlackPoint = a.levelsBlackPoint + ((b.levelsBlackPoint - a.levelsBlackPoint) * t),
        .levelsWhitePoint = a.levelsWhitePoint + ((b.levelsWhitePoint - a.levelsWhitePoint) * t),
        .levelsStrength = a.levelsStrength + ((b.levelsStrength - a.levelsStrength) * t),
        .levelsClipHighlight =
            std::lerp(a.levelsClipHighlight, b.levelsClipHighlight, t) >= 0.5f ? 1.0f : 0.0f,
        .lumaSharpenStrength = a.lumaSharpenStrength + ((b.lumaSharpenStrength - a.lumaSharpenStrength) * t),
        .lumaSharpenClamp = a.lumaSharpenClamp + ((b.lumaSharpenClamp - a.lumaSharpenClamp) * t),
        .lumaSharpenPattern = std::clamp(
            static_cast<int>(std::lround(
                std::lerp(static_cast<float>(a.lumaSharpenPattern), static_cast<float>(b.lumaSharpenPattern), t))),
            0,
            3),
        .lumaSharpenOffsetBias =
            a.lumaSharpenOffsetBias + ((b.lumaSharpenOffsetBias - a.lumaSharpenOffsetBias) * t),
        .lumaSharpenShowPattern =
            std::lerp(a.lumaSharpenShowPattern, b.lumaSharpenShowPattern, t) >= 0.5f ? 1.0f : 0.0f,
        .sweetFxCurvesMode = std::clamp(
            static_cast<int>(std::lround(
                std::lerp(static_cast<float>(a.sweetFxCurvesMode), static_cast<float>(b.sweetFxCurvesMode), t))),
            0,
            2),
        .sweetFxCurvesFormula = std::clamp(
            static_cast<int>(std::lround(std::lerp(
                static_cast<float>(a.sweetFxCurvesFormula),
                static_cast<float>(b.sweetFxCurvesFormula),
                t))),
            0,
            10),
        .sweetFxCurvesContrast =
            a.sweetFxCurvesContrast + ((b.sweetFxCurvesContrast - a.sweetFxCurvesContrast) * t),
        .sweetFxCurvesStrength =
            a.sweetFxCurvesStrength + ((b.sweetFxCurvesStrength - a.sweetFxCurvesStrength) * t),
        .sweetFxChromaticAberrationShiftX = a.sweetFxChromaticAberrationShiftX
            + ((b.sweetFxChromaticAberrationShiftX - a.sweetFxChromaticAberrationShiftX) * t),
        .sweetFxChromaticAberrationShiftY = a.sweetFxChromaticAberrationShiftY
            + ((b.sweetFxChromaticAberrationShiftY - a.sweetFxChromaticAberrationShiftY) * t),
        .sweetFxChromaticAberrationStrength = a.sweetFxChromaticAberrationStrength
            + ((b.sweetFxChromaticAberrationStrength - a.sweetFxChromaticAberrationStrength) * t),
        .sweetFxBorderWidthX =
            a.sweetFxBorderWidthX + ((b.sweetFxBorderWidthX - a.sweetFxBorderWidthX) * t),
        .sweetFxBorderWidthY =
            a.sweetFxBorderWidthY + ((b.sweetFxBorderWidthY - a.sweetFxBorderWidthY) * t),
        .sweetFxBorderRatio = a.sweetFxBorderRatio + ((b.sweetFxBorderRatio - a.sweetFxBorderRatio) * t),
        .sweetFxBorderColor = ri::math::Lerp(a.sweetFxBorderColor, b.sweetFxBorderColor, t),
        .sweetFxBorderStrength =
            a.sweetFxBorderStrength + ((b.sweetFxBorderStrength - a.sweetFxBorderStrength) * t),
        .sweetFxCartoonPower =
            a.sweetFxCartoonPower + ((b.sweetFxCartoonPower - a.sweetFxCartoonPower) * t),
        .sweetFxCartoonEdgeSlope =
            a.sweetFxCartoonEdgeSlope + ((b.sweetFxCartoonEdgeSlope - a.sweetFxCartoonEdgeSlope) * t),
        .sweetFxCartoonStrength =
            a.sweetFxCartoonStrength + ((b.sweetFxCartoonStrength - a.sweetFxCartoonStrength) * t),
        .sweetFxTonemapGamma =
            a.sweetFxTonemapGamma + ((b.sweetFxTonemapGamma - a.sweetFxTonemapGamma) * t),
        .sweetFxTonemapExposure =
            a.sweetFxTonemapExposure + ((b.sweetFxTonemapExposure - a.sweetFxTonemapExposure) * t),
        .sweetFxTonemapSaturation =
            a.sweetFxTonemapSaturation + ((b.sweetFxTonemapSaturation - a.sweetFxTonemapSaturation) * t),
        .sweetFxTonemapBleach =
            a.sweetFxTonemapBleach + ((b.sweetFxTonemapBleach - a.sweetFxTonemapBleach) * t),
        .sweetFxTonemapDefog =
            a.sweetFxTonemapDefog + ((b.sweetFxTonemapDefog - a.sweetFxTonemapDefog) * t),
        .sweetFxTonemapFogColor = ri::math::Lerp(a.sweetFxTonemapFogColor, b.sweetFxTonemapFogColor, t),
        .sweetFxTonemapStrength =
            a.sweetFxTonemapStrength + ((b.sweetFxTonemapStrength - a.sweetFxTonemapStrength) * t),
        .sweetFxSplitscreenMode = (t < 0.5f) ? a.sweetFxSplitscreenMode : b.sweetFxSplitscreenMode,
        .sweetFxSplitscreenStrength =
            a.sweetFxSplitscreenStrength + ((b.sweetFxSplitscreenStrength - a.sweetFxSplitscreenStrength) * t),
        .sweetFxNostalgiaPalette = (t < 0.5f) ? a.sweetFxNostalgiaPalette : b.sweetFxNostalgiaPalette,
        .sweetFxNostalgiaScanlines = (t < 0.5f) ? a.sweetFxNostalgiaScanlines : b.sweetFxNostalgiaScanlines,
        .sweetFxNostalgiaDither =
            a.sweetFxNostalgiaDither + ((b.sweetFxNostalgiaDither - a.sweetFxNostalgiaDither) * t),
        .sweetFxNostalgiaStrength =
            a.sweetFxNostalgiaStrength + ((b.sweetFxNostalgiaStrength - a.sweetFxNostalgiaStrength) * t),
        .sweetFxCompareMode = (t < 0.5f) ? a.sweetFxCompareMode : b.sweetFxCompareMode,
        .sweetFxCompareDifferenceScale =
            a.sweetFxCompareDifferenceScale + ((b.sweetFxCompareDifferenceScale - a.sweetFxCompareDifferenceScale) * t),
        .sweetFxCompareStrength =
            a.sweetFxCompareStrength + ((b.sweetFxCompareStrength - a.sweetFxCompareStrength) * t),
        .sweetFxLayerPosition =
            ri::math::Vec2{a.sweetFxLayerPosition.x
                               + ((b.sweetFxLayerPosition.x - a.sweetFxLayerPosition.x) * t),
                           a.sweetFxLayerPosition.y
                               + ((b.sweetFxLayerPosition.y - a.sweetFxLayerPosition.y) * t)},
        .sweetFxLayerScale =
            a.sweetFxLayerScale + ((b.sweetFxLayerScale - a.sweetFxLayerScale) * t),
        .sweetFxLayerBlend =
            a.sweetFxLayerBlend + ((b.sweetFxLayerBlend - a.sweetFxLayerBlend) * t),
        .sweetFxLayerTexWidth =
            a.sweetFxLayerTexWidth + ((b.sweetFxLayerTexWidth - a.sweetFxLayerTexWidth) * t),
        .sweetFxLayerTexHeight =
            a.sweetFxLayerTexHeight + ((b.sweetFxLayerTexHeight - a.sweetFxLayerTexHeight) * t),
        .sweetFxFxaaSubpix =
            a.sweetFxFxaaSubpix + ((b.sweetFxFxaaSubpix - a.sweetFxFxaaSubpix) * t),
        .sweetFxFxaaEdgeThreshold =
            a.sweetFxFxaaEdgeThreshold + ((b.sweetFxFxaaEdgeThreshold - a.sweetFxFxaaEdgeThreshold) * t),
        .sweetFxFxaaEdgeThresholdMin =
            a.sweetFxFxaaEdgeThresholdMin + ((b.sweetFxFxaaEdgeThresholdMin - a.sweetFxFxaaEdgeThresholdMin) * t),
        .sweetFxFxaaStrength =
            a.sweetFxFxaaStrength + ((b.sweetFxFxaaStrength - a.sweetFxFxaaStrength) * t),
        .sweetFxCrtAmount =
            a.sweetFxCrtAmount + ((b.sweetFxCrtAmount - a.sweetFxCrtAmount) * t),
        .sweetFxCrtResolution =
            a.sweetFxCrtResolution + ((b.sweetFxCrtResolution - a.sweetFxCrtResolution) * t),
        .sweetFxCrtGamma =
            a.sweetFxCrtGamma + ((b.sweetFxCrtGamma - a.sweetFxCrtGamma) * t),
        .sweetFxCrtMonitorGamma =
            a.sweetFxCrtMonitorGamma + ((b.sweetFxCrtMonitorGamma - a.sweetFxCrtMonitorGamma) * t),
        .sweetFxCrtBrightness =
            a.sweetFxCrtBrightness + ((b.sweetFxCrtBrightness - a.sweetFxCrtBrightness) * t),
        .sweetFxCrtScanlineIntensity = (t < 0.5f) ? a.sweetFxCrtScanlineIntensity : b.sweetFxCrtScanlineIntensity,
        .sweetFxCrtScanlineGaussian =
            a.sweetFxCrtScanlineGaussian + ((b.sweetFxCrtScanlineGaussian - a.sweetFxCrtScanlineGaussian) * t),
        .sweetFxCrtCurvature =
            a.sweetFxCrtCurvature + ((b.sweetFxCrtCurvature - a.sweetFxCrtCurvature) * t),
        .sweetFxCrtCurvatureRadius =
            a.sweetFxCrtCurvatureRadius + ((b.sweetFxCrtCurvatureRadius - a.sweetFxCrtCurvatureRadius) * t),
        .sweetFxCrtCornerSize =
            a.sweetFxCrtCornerSize + ((b.sweetFxCrtCornerSize - a.sweetFxCrtCornerSize) * t),
        .sweetFxCrtViewerDistance =
            a.sweetFxCrtViewerDistance + ((b.sweetFxCrtViewerDistance - a.sweetFxCrtViewerDistance) * t),
        .sweetFxCrtAngle = ri::math::Vec2{
            a.sweetFxCrtAngle.x + ((b.sweetFxCrtAngle.x - a.sweetFxCrtAngle.x) * t),
            a.sweetFxCrtAngle.y + ((b.sweetFxCrtAngle.y - a.sweetFxCrtAngle.y) * t)},
        .sweetFxCrtOverscan =
            a.sweetFxCrtOverscan + ((b.sweetFxCrtOverscan - a.sweetFxCrtOverscan) * t),
        .sweetFxCrtOversample =
            a.sweetFxCrtOversample + ((b.sweetFxCrtOversample - a.sweetFxCrtOversample) * t),
        .sweetFxAsciiSpacing = (t < 0.5f) ? a.sweetFxAsciiSpacing : b.sweetFxAsciiSpacing,
        .sweetFxAsciiFont = (t < 0.5f) ? a.sweetFxAsciiFont : b.sweetFxAsciiFont,
        .sweetFxAsciiFontColorMode = (t < 0.5f) ? a.sweetFxAsciiFontColorMode : b.sweetFxAsciiFontColorMode,
        .sweetFxAsciiFontColor = ri::math::Lerp(a.sweetFxAsciiFontColor, b.sweetFxAsciiFontColor, t),
        .sweetFxAsciiBackgroundColor = ri::math::Lerp(a.sweetFxAsciiBackgroundColor, b.sweetFxAsciiBackgroundColor, t),
        .sweetFxAsciiSwapColors =
            a.sweetFxAsciiSwapColors + ((b.sweetFxAsciiSwapColors - a.sweetFxAsciiSwapColors) * t),
        .sweetFxAsciiInvertBrightness =
            a.sweetFxAsciiInvertBrightness + ((b.sweetFxAsciiInvertBrightness - a.sweetFxAsciiInvertBrightness) * t),
        .sweetFxAsciiDithering =
            a.sweetFxAsciiDithering + ((b.sweetFxAsciiDithering - a.sweetFxAsciiDithering) * t),
        .sweetFxAsciiDitheringIntensity =
            a.sweetFxAsciiDitheringIntensity + ((b.sweetFxAsciiDitheringIntensity - a.sweetFxAsciiDitheringIntensity) * t),
        .sweetFxAsciiDitheringDebugGradient =
            a.sweetFxAsciiDitheringDebugGradient + ((b.sweetFxAsciiDitheringDebugGradient - a.sweetFxAsciiDitheringDebugGradient) * t),
        .sweetFxAsciiStrength =
            a.sweetFxAsciiStrength + ((b.sweetFxAsciiStrength - a.sweetFxAsciiStrength) * t),
        .sweetFxSmaaEdgeDetectionType = (t < 0.5f) ? a.sweetFxSmaaEdgeDetectionType : b.sweetFxSmaaEdgeDetectionType,
        .sweetFxSmaaEdgeThreshold =
            a.sweetFxSmaaEdgeThreshold + ((b.sweetFxSmaaEdgeThreshold - a.sweetFxSmaaEdgeThreshold) * t),
        .sweetFxSmaaDepthThreshold =
            a.sweetFxSmaaDepthThreshold + ((b.sweetFxSmaaDepthThreshold - a.sweetFxSmaaDepthThreshold) * t),
        .sweetFxSmaaMaxSearchSteps = (t < 0.5f) ? a.sweetFxSmaaMaxSearchSteps : b.sweetFxSmaaMaxSearchSteps,
        .sweetFxSmaaMaxSearchStepsDiagonal =
            (t < 0.5f) ? a.sweetFxSmaaMaxSearchStepsDiagonal : b.sweetFxSmaaMaxSearchStepsDiagonal,
        .sweetFxSmaaCornerRounding = (t < 0.5f) ? a.sweetFxSmaaCornerRounding : b.sweetFxSmaaCornerRounding,
        .sweetFxSmaaDebugOutput =
            a.sweetFxSmaaDebugOutput + ((b.sweetFxSmaaDebugOutput - a.sweetFxSmaaDebugOutput) * t),
        .sweetFxSmaaStrength =
            a.sweetFxSmaaStrength + ((b.sweetFxSmaaStrength - a.sweetFxSmaaStrength) * t),
        .reshadeDaltonizeType = (t < 0.5f) ? a.reshadeDaltonizeType : b.reshadeDaltonizeType,
        .reshadeDaltonizeStrength =
            a.reshadeDaltonizeStrength + ((b.reshadeDaltonizeStrength - a.reshadeDaltonizeStrength) * t),
        .reshadeDisplayDepthPresentType = (t < 0.5f) ? a.reshadeDisplayDepthPresentType : b.reshadeDisplayDepthPresentType,
        .reshadeDisplayDepthStrength =
            a.reshadeDisplayDepthStrength + ((b.reshadeDisplayDepthStrength - a.reshadeDisplayDepthStrength) * t),
        .reshadeLutAmountChroma =
            a.reshadeLutAmountChroma + ((b.reshadeLutAmountChroma - a.reshadeLutAmountChroma) * t),
        .reshadeLutAmountLuma =
            a.reshadeLutAmountLuma + ((b.reshadeLutAmountLuma - a.reshadeLutAmountLuma) * t),
        .reshadeLutStrength =
            a.reshadeLutStrength + ((b.reshadeLutStrength - a.reshadeLutStrength) * t),
        .pd80TechnicolorStrength =
            a.pd80TechnicolorStrength + ((b.pd80TechnicolorStrength - a.pd80TechnicolorStrength) * t),
        .pd80TechnicolorRed2strip = ri::math::Lerp(a.pd80TechnicolorRed2strip, b.pd80TechnicolorRed2strip, t),
        .pd80TechnicolorCyan2strip = ri::math::Lerp(a.pd80TechnicolorCyan2strip, b.pd80TechnicolorCyan2strip, t),
        .pd80TechnicolorColorKey = ri::math::Lerp(a.pd80TechnicolorColorKey, b.pd80TechnicolorColorKey, t),
        .pd80TechnicolorSaturation2 =
            a.pd80TechnicolorSaturation2 + ((b.pd80TechnicolorSaturation2 - a.pd80TechnicolorSaturation2) * t),
        .pd80TechnicolorEnable3strip =
            std::lerp(a.pd80TechnicolorEnable3strip, b.pd80TechnicolorEnable3strip, t) >= 0.5f ? 1.0f : 0.0f,
        .pd80Technicolor3ColorStrength =
            ri::math::Lerp(a.pd80Technicolor3ColorStrength, b.pd80Technicolor3ColorStrength, t),
        .pd80Technicolor3Brightness =
            a.pd80Technicolor3Brightness + ((b.pd80Technicolor3Brightness - a.pd80Technicolor3Brightness) * t),
        .pd80Technicolor3Saturation =
            a.pd80Technicolor3Saturation + ((b.pd80Technicolor3Saturation - a.pd80Technicolor3Saturation) * t),
        .pd80Technicolor3Strength =
            a.pd80Technicolor3Strength + ((b.pd80Technicolor3Strength - a.pd80Technicolor3Strength) * t),
        .pd80ColorTemperatureKelvin =
            a.pd80ColorTemperatureKelvin + ((b.pd80ColorTemperatureKelvin - a.pd80ColorTemperatureKelvin) * t),
        .pd80ColorTemperatureLuminancePreservation =
            a.pd80ColorTemperatureLuminancePreservation
            + ((b.pd80ColorTemperatureLuminancePreservation - a.pd80ColorTemperatureLuminancePreservation) * t),
        .pd80ColorTemperatureMix =
            a.pd80ColorTemperatureMix + ((b.pd80ColorTemperatureMix - a.pd80ColorTemperatureMix) * t),
        .pd80ColorTemperatureStrength =
            a.pd80ColorTemperatureStrength + ((b.pd80ColorTemperatureStrength - a.pd80ColorTemperatureStrength) * t),
        .pd80SaturationLimit =
            a.pd80SaturationLimit + ((b.pd80SaturationLimit - a.pd80SaturationLimit) * t),
        .pd80SaturationLimitStrength =
            a.pd80SaturationLimitStrength + ((b.pd80SaturationLimitStrength - a.pd80SaturationLimitStrength) * t),
        .pd80ColorBalanceShadow = ri::math::Lerp(a.pd80ColorBalanceShadow, b.pd80ColorBalanceShadow, t),
        .pd80ColorBalanceMid = ri::math::Lerp(a.pd80ColorBalanceMid, b.pd80ColorBalanceMid, t),
        .pd80ColorBalanceHigh = ri::math::Lerp(a.pd80ColorBalanceHigh, b.pd80ColorBalanceHigh, t),
        .pd80ColorBalancePreserveLuma =
            std::lerp(a.pd80ColorBalancePreserveLuma, b.pd80ColorBalancePreserveLuma, t),
        .pd80ColorBalanceSeparationMode =
            std::lerp(a.pd80ColorBalanceSeparationMode, b.pd80ColorBalanceSeparationMode, t),
        .pd80ColorBalanceStrength =
            a.pd80ColorBalanceStrength + ((b.pd80ColorBalanceStrength - a.pd80ColorBalanceStrength) * t),
        .pd80ColorIsolationHueMid =
            a.pd80ColorIsolationHueMid + ((b.pd80ColorIsolationHueMid - a.pd80ColorIsolationHueMid) * t),
        .pd80ColorIsolationHueRange =
            a.pd80ColorIsolationHueRange + ((b.pd80ColorIsolationHueRange - a.pd80ColorIsolationHueRange) * t),
        .pd80ColorIsolationSatLimit =
            a.pd80ColorIsolationSatLimit + ((b.pd80ColorIsolationSatLimit - a.pd80ColorIsolationSatLimit) * t),
        .pd80ColorIsolationFxMix =
            a.pd80ColorIsolationFxMix + ((b.pd80ColorIsolationFxMix - a.pd80ColorIsolationFxMix) * t),
        .pd80ColorIsolationStrength =
            a.pd80ColorIsolationStrength + ((b.pd80ColorIsolationStrength - a.pd80ColorIsolationStrength) * t),
        .pd80LevelsBlackIn = ri::math::Lerp(a.pd80LevelsBlackIn, b.pd80LevelsBlackIn, t),
        .pd80LevelsWhiteIn = ri::math::Lerp(a.pd80LevelsWhiteIn, b.pd80LevelsWhiteIn, t),
        .pd80LevelsBlackOut = ri::math::Lerp(a.pd80LevelsBlackOut, b.pd80LevelsBlackOut, t),
        .pd80LevelsWhiteOut = ri::math::Lerp(a.pd80LevelsWhiteOut, b.pd80LevelsWhiteOut, t),
        .pd80LevelsGamma = a.pd80LevelsGamma + ((b.pd80LevelsGamma - a.pd80LevelsGamma) * t),
        .pd80LevelsEnableDither = std::lerp(a.pd80LevelsEnableDither, b.pd80LevelsEnableDither, t),
        .pd80LevelsDitherStrength =
            a.pd80LevelsDitherStrength + ((b.pd80LevelsDitherStrength - a.pd80LevelsDitherStrength) * t),
        .pd80LevelsStrength = a.pd80LevelsStrength + ((b.pd80LevelsStrength - a.pd80LevelsStrength) * t),
        .pd80BlackWhiteMode =
            a.pd80BlackWhiteMode + ((b.pd80BlackWhiteMode - a.pd80BlackWhiteMode) * t),
        .pd80BlackWhiteCurveStr =
            a.pd80BlackWhiteCurveStr + ((b.pd80BlackWhiteCurveStr - a.pd80BlackWhiteCurveStr) * t),
        .pd80BlackWhiteEnableDither =
            std::lerp(a.pd80BlackWhiteEnableDither, b.pd80BlackWhiteEnableDither, t),
        .pd80BlackWhiteDitherStrength =
            a.pd80BlackWhiteDitherStrength
            + ((b.pd80BlackWhiteDitherStrength - a.pd80BlackWhiteDitherStrength) * t),
        .pd80BlackWhiteRedChannel =
            a.pd80BlackWhiteRedChannel + ((b.pd80BlackWhiteRedChannel - a.pd80BlackWhiteRedChannel) * t),
        .pd80BlackWhiteYellowChannel =
            a.pd80BlackWhiteYellowChannel
            + ((b.pd80BlackWhiteYellowChannel - a.pd80BlackWhiteYellowChannel) * t),
        .pd80BlackWhiteGreenChannel =
            a.pd80BlackWhiteGreenChannel
            + ((b.pd80BlackWhiteGreenChannel - a.pd80BlackWhiteGreenChannel) * t),
        .pd80BlackWhiteCyanChannel =
            a.pd80BlackWhiteCyanChannel + ((b.pd80BlackWhiteCyanChannel - a.pd80BlackWhiteCyanChannel) * t),
        .pd80BlackWhiteBlueChannel =
            a.pd80BlackWhiteBlueChannel + ((b.pd80BlackWhiteBlueChannel - a.pd80BlackWhiteBlueChannel) * t),
        .pd80BlackWhiteMagentaChannel =
            a.pd80BlackWhiteMagentaChannel
            + ((b.pd80BlackWhiteMagentaChannel - a.pd80BlackWhiteMagentaChannel) * t),
        .pd80BlackWhiteUseTint = std::lerp(a.pd80BlackWhiteUseTint, b.pd80BlackWhiteUseTint, t),
        .pd80BlackWhiteTintHue =
            a.pd80BlackWhiteTintHue + ((b.pd80BlackWhiteTintHue - a.pd80BlackWhiteTintHue) * t),
        .pd80BlackWhiteTintSat =
            a.pd80BlackWhiteTintSat + ((b.pd80BlackWhiteTintSat - a.pd80BlackWhiteTintSat) * t),
        .pd80BlackWhiteShowClip = std::lerp(a.pd80BlackWhiteShowClip, b.pd80BlackWhiteShowClip, t),
        .pd80BlackWhiteStrength =
            a.pd80BlackWhiteStrength + ((b.pd80BlackWhiteStrength - a.pd80BlackWhiteStrength) * t),
        .pd80CbsEnableDither = std::lerp(a.pd80CbsEnableDither, b.pd80CbsEnableDither, t),
        .pd80CbsDitherStrength =
            a.pd80CbsDitherStrength + ((b.pd80CbsDitherStrength - a.pd80CbsDitherStrength) * t),
        .pd80CbsTint = a.pd80CbsTint + ((b.pd80CbsTint - a.pd80CbsTint) * t),
        .pd80CbsExposure = a.pd80CbsExposure + ((b.pd80CbsExposure - a.pd80CbsExposure) * t),
        .pd80CbsContrast = a.pd80CbsContrast + ((b.pd80CbsContrast - a.pd80CbsContrast) * t),
        .pd80CbsBrightness = a.pd80CbsBrightness + ((b.pd80CbsBrightness - a.pd80CbsBrightness) * t),
        .pd80CbsSaturation = a.pd80CbsSaturation + ((b.pd80CbsSaturation - a.pd80CbsSaturation) * t),
        .pd80CbsVibrance = a.pd80CbsVibrance + ((b.pd80CbsVibrance - a.pd80CbsVibrance) * t),
        .pd80CbsHueMid = a.pd80CbsHueMid + ((b.pd80CbsHueMid - a.pd80CbsHueMid) * t),
        .pd80CbsHueRange = a.pd80CbsHueRange + ((b.pd80CbsHueRange - a.pd80CbsHueRange) * t),
        .pd80CbsSatCustom = a.pd80CbsSatCustom + ((b.pd80CbsSatCustom - a.pd80CbsSatCustom) * t),
        .pd80CbsSatR = a.pd80CbsSatR + ((b.pd80CbsSatR - a.pd80CbsSatR) * t),
        .pd80CbsSatY = a.pd80CbsSatY + ((b.pd80CbsSatY - a.pd80CbsSatY) * t),
        .pd80CbsSatG = a.pd80CbsSatG + ((b.pd80CbsSatG - a.pd80CbsSatG) * t),
        .pd80CbsSatA = a.pd80CbsSatA + ((b.pd80CbsSatA - a.pd80CbsSatA) * t),
        .pd80CbsSatB = a.pd80CbsSatB + ((b.pd80CbsSatB - a.pd80CbsSatB) * t),
        .pd80CbsSatP = a.pd80CbsSatP + ((b.pd80CbsSatP - a.pd80CbsSatP) * t),
        .pd80CbsSatM = a.pd80CbsSatM + ((b.pd80CbsSatM - a.pd80CbsSatM) * t),
        .pd80CbsEnableDepth = std::lerp(a.pd80CbsEnableDepth, b.pd80CbsEnableDepth, t),
        .pd80CbsDisplayDepth = std::lerp(a.pd80CbsDisplayDepth, b.pd80CbsDisplayDepth, t),
        .pd80CbsDepthStart = a.pd80CbsDepthStart + ((b.pd80CbsDepthStart - a.pd80CbsDepthStart) * t),
        .pd80CbsDepthEnd = a.pd80CbsDepthEnd + ((b.pd80CbsDepthEnd - a.pd80CbsDepthEnd) * t),
        .pd80CbsDepthCurve = a.pd80CbsDepthCurve + ((b.pd80CbsDepthCurve - a.pd80CbsDepthCurve) * t),
        .pd80CbsExposureFar = a.pd80CbsExposureFar + ((b.pd80CbsExposureFar - a.pd80CbsExposureFar) * t),
        .pd80CbsContrastFar = a.pd80CbsContrastFar + ((b.pd80CbsContrastFar - a.pd80CbsContrastFar) * t),
        .pd80CbsBrightnessFar = a.pd80CbsBrightnessFar + ((b.pd80CbsBrightnessFar - a.pd80CbsBrightnessFar) * t),
        .pd80CbsSaturationFar =
            a.pd80CbsSaturationFar + ((b.pd80CbsSaturationFar - a.pd80CbsSaturationFar) * t),
        .pd80CbsVibranceFar = a.pd80CbsVibranceFar + ((b.pd80CbsVibranceFar - a.pd80CbsVibranceFar) * t),
        .pd80CbsStrength = a.pd80CbsStrength + ((b.pd80CbsStrength - a.pd80CbsStrength) * t),
        .pd80CaMasterStrength =
            a.pd80CaMasterStrength + ((b.pd80CaMasterStrength - a.pd80CaMasterStrength) * t),
        .pd80CaEffectStrength =
            a.pd80CaEffectStrength + ((b.pd80CaEffectStrength - a.pd80CaEffectStrength) * t),
        .pd80CaGlobalWidth =
            a.pd80CaGlobalWidth + ((b.pd80CaGlobalWidth - a.pd80CaGlobalWidth) * t),
        .pd80CaSampleSteps =
            a.pd80CaSampleSteps + ((b.pd80CaSampleSteps - a.pd80CaSampleSteps) * t),
        .pd80CaType = a.pd80CaType + ((b.pd80CaType - a.pd80CaType) * t),
        .pd80CaDegrees = a.pd80CaDegrees + ((b.pd80CaDegrees - a.pd80CaDegrees) * t),
        .pd80CaWidth = a.pd80CaWidth + ((b.pd80CaWidth - a.pd80CaWidth) * t),
        .pd80CaCurve = a.pd80CaCurve + ((b.pd80CaCurve - a.pd80CaCurve) * t),
        .pd80CaOX = a.pd80CaOX + ((b.pd80CaOX - a.pd80CaOX) * t),
        .pd80CaOY = a.pd80CaOY + ((b.pd80CaOY - a.pd80CaOY) * t),
        .pd80CaShapeX = a.pd80CaShapeX + ((b.pd80CaShapeX - a.pd80CaShapeX) * t),
        .pd80CaShapeY = a.pd80CaShapeY + ((b.pd80CaShapeY - a.pd80CaShapeY) * t),
        .pd80CaVignetteColor = ri::math::Lerp(a.pd80CaVignetteColor, b.pd80CaVignetteColor, t),
        .pd80CaShowCa = std::lerp(a.pd80CaShowCa, b.pd80CaShowCa, t),
        .pd80CaEnableDepthInt = std::lerp(a.pd80CaEnableDepthInt, b.pd80CaEnableDepthInt, t),
        .pd80CaEnableDepthWidth = std::lerp(a.pd80CaEnableDepthWidth, b.pd80CaEnableDepthWidth, t),
        .pd80CaDisplayDepth = std::lerp(a.pd80CaDisplayDepth, b.pd80CaDisplayDepth, t),
        .pd80CaDepthStart = a.pd80CaDepthStart + ((b.pd80CaDepthStart - a.pd80CaDepthStart) * t),
        .pd80CaDepthEnd = a.pd80CaDepthEnd + ((b.pd80CaDepthEnd - a.pd80CaDepthEnd) * t),
        .pd80CaDepthCurve = a.pd80CaDepthCurve + ((b.pd80CaDepthCurve - a.pd80CaDepthCurve) * t),
        .pd80LsMasterStrength =
            a.pd80LsMasterStrength + ((b.pd80LsMasterStrength - a.pd80LsMasterStrength) * t),
        .pd80LsBlurSigma = a.pd80LsBlurSigma + ((b.pd80LsBlurSigma - a.pd80LsBlurSigma) * t),
        .pd80LsSharpening = a.pd80LsSharpening + ((b.pd80LsSharpening - a.pd80LsSharpening) * t),
        .pd80LsThreshold = a.pd80LsThreshold + ((b.pd80LsThreshold - a.pd80LsThreshold) * t),
        .pd80LsLimiter = a.pd80LsLimiter + ((b.pd80LsLimiter - a.pd80LsLimiter) * t),
        .pd80LsShowEdges = std::lerp(a.pd80LsShowEdges, b.pd80LsShowEdges, t),
        .pd80LsEnableDepth = std::lerp(a.pd80LsEnableDepth, b.pd80LsEnableDepth, t),
        .pd80LsEnableReverse = std::lerp(a.pd80LsEnableReverse, b.pd80LsEnableReverse, t),
        .pd80LsDisplayDepth = std::lerp(a.pd80LsDisplayDepth, b.pd80LsDisplayDepth, t),
        .pd80LsDepthStart = a.pd80LsDepthStart + ((b.pd80LsDepthStart - a.pd80LsDepthStart) * t),
        .pd80LsDepthEnd = a.pd80LsDepthEnd + ((b.pd80LsDepthEnd - a.pd80LsDepthEnd) * t),
        .pd80LsDepthCurve = a.pd80LsDepthCurve + ((b.pd80LsDepthCurve - a.pd80LsDepthCurve) * t),
        .pd80FgMasterStrength =
            a.pd80FgMasterStrength + ((b.pd80FgMasterStrength - a.pd80FgMasterStrength) * t),
        .pd80FgGrainAdjust =
            a.pd80FgGrainAdjust + ((b.pd80FgGrainAdjust - a.pd80FgGrainAdjust) * t),
        .pd80FgGrainSize = a.pd80FgGrainSize + ((b.pd80FgGrainSize - a.pd80FgGrainSize) * t),
        .pd80FgGrainMotion = std::lerp(a.pd80FgGrainMotion, b.pd80FgGrainMotion, t),
        .pd80FgGrainOrigColor = std::lerp(a.pd80FgGrainOrigColor, b.pd80FgGrainOrigColor, t),
        .pd80FgUseNegnoise = std::lerp(a.pd80FgUseNegnoise, b.pd80FgUseNegnoise, t),
        .pd80FgGrainColor = a.pd80FgGrainColor + ((b.pd80FgGrainColor - a.pd80FgGrainColor) * t),
        .pd80FgGrainAmount = a.pd80FgGrainAmount + ((b.pd80FgGrainAmount - a.pd80FgGrainAmount) * t),
        .pd80FgGrainIntensity =
            a.pd80FgGrainIntensity + ((b.pd80FgGrainIntensity - a.pd80FgGrainIntensity) * t),
        .pd80FgGrainDensity =
            a.pd80FgGrainDensity + ((b.pd80FgGrainDensity - a.pd80FgGrainDensity) * t),
        .pd80FgGrainIntHigh =
            a.pd80FgGrainIntHigh + ((b.pd80FgGrainIntHigh - a.pd80FgGrainIntHigh) * t),
        .pd80FgGrainIntLow =
            a.pd80FgGrainIntLow + ((b.pd80FgGrainIntLow - a.pd80FgGrainIntLow) * t),
        .pd80FgEnableTest = std::lerp(a.pd80FgEnableTest, b.pd80FgEnableTest, t),
        .pd80FgEnableDepth = std::lerp(a.pd80FgEnableDepth, b.pd80FgEnableDepth, t),
        .pd80FgDisplayDepth = std::lerp(a.pd80FgDisplayDepth, b.pd80FgDisplayDepth, t),
        .pd80FgDepthStart = a.pd80FgDepthStart + ((b.pd80FgDepthStart - a.pd80FgDepthStart) * t),
        .pd80FgDepthEnd = a.pd80FgDepthEnd + ((b.pd80FgDepthEnd - a.pd80FgDepthEnd) * t),
        .pd80FgDepthCurve = a.pd80FgDepthCurve + ((b.pd80FgDepthCurve - a.pd80FgDepthCurve) * t),
        .pd80DsMasterStrength =
            a.pd80DsMasterStrength + ((b.pd80DsMasterStrength - a.pd80DsMasterStrength) * t),
        .pd80DsDepthNear = a.pd80DsDepthNear + ((b.pd80DsDepthNear - a.pd80DsDepthNear) * t),
        .pd80DsDepthPos = a.pd80DsDepthPos + ((b.pd80DsDepthPos - a.pd80DsDepthPos) * t),
        .pd80DsDepthFar = a.pd80DsDepthFar + ((b.pd80DsDepthFar - a.pd80DsDepthFar) * t),
        .pd80DsDepthSmoothing =
            a.pd80DsDepthSmoothing + ((b.pd80DsDepthSmoothing - a.pd80DsDepthSmoothing) * t),
        .pd80DsIntensity = a.pd80DsIntensity + ((b.pd80DsIntensity - a.pd80DsIntensity) * t),
        .pd80DsHue = a.pd80DsHue + ((b.pd80DsHue - a.pd80DsHue) * t),
        .pd80DsSaturation = a.pd80DsSaturation + ((b.pd80DsSaturation - a.pd80DsSaturation) * t),
        .pd80DsBlendMode = static_cast<float>(std::clamp(
            static_cast<int>(std::lround(std::lerp(a.pd80DsBlendMode, b.pd80DsBlendMode, t))), 0, 20)),
        .pd80DsOpacity = a.pd80DsOpacity + ((b.pd80DsOpacity - a.pd80DsOpacity) * t),
        .pd80CgMasterStrength =
            a.pd80CgMasterStrength + ((b.pd80CgMasterStrength - a.pd80CgMasterStrength) * t),
        .pd80ColorGamut = static_cast<float>(std::clamp(
            static_cast<int>(std::lround(std::lerp(a.pd80ColorGamut, b.pd80ColorGamut, t))), 0, 15)),
        .pd80CscMasterStrength =
            a.pd80CscMasterStrength + ((b.pd80CscMasterStrength - a.pd80CscMasterStrength) * t),
        .pd80CscEnableDither = std::lerp(a.pd80CscEnableDither, b.pd80CscEnableDither, t),
        .pd80CscDitherStrength =
            a.pd80CscDitherStrength + ((b.pd80CscDitherStrength - a.pd80CscDitherStrength) * t),
        .pd80CscColorSpace = static_cast<float>(std::clamp(
            static_cast<int>(std::lround(std::lerp(a.pd80CscColorSpace, b.pd80CscColorSpace, t))), 0, 3)),
        .pd80CscPos0ToeGrey =
            a.pd80CscPos0ToeGrey + ((b.pd80CscPos0ToeGrey - a.pd80CscPos0ToeGrey) * t),
        .pd80CscPos1ToeGrey =
            a.pd80CscPos1ToeGrey + ((b.pd80CscPos1ToeGrey - a.pd80CscPos1ToeGrey) * t),
        .pd80CscPos0ShoulderGrey =
            a.pd80CscPos0ShoulderGrey + ((b.pd80CscPos0ShoulderGrey - a.pd80CscPos0ShoulderGrey) * t),
        .pd80CscPos1ShoulderGrey =
            a.pd80CscPos1ShoulderGrey + ((b.pd80CscPos1ShoulderGrey - a.pd80CscPos1ShoulderGrey) * t),
        .pd80CscColorSat = a.pd80CscColorSat + ((b.pd80CscColorSat - a.pd80CscColorSat) * t),
        .pd80SmhMasterStrength =
            a.pd80SmhMasterStrength + ((b.pd80SmhMasterStrength - a.pd80SmhMasterStrength) * t),
        .pd80SmhLumaMode = static_cast<float>(std::clamp(
            static_cast<int>(std::lround(std::lerp(a.pd80SmhLumaMode, b.pd80SmhLumaMode, t))), 0, 2)),
        .pd80SmhSeparationMode = static_cast<float>(std::clamp(
            static_cast<int>(std::lround(std::lerp(a.pd80SmhSeparationMode, b.pd80SmhSeparationMode, t))),
            0,
            1)),
        .pd80SmhEnableDither = std::lerp(a.pd80SmhEnableDither, b.pd80SmhEnableDither, t),
        .pd80SmhDitherStrength =
            a.pd80SmhDitherStrength + ((b.pd80SmhDitherStrength - a.pd80SmhDitherStrength) * t),
        .pd80SmhBlendColorShadow = ri::math::Lerp(a.pd80SmhBlendColorShadow, b.pd80SmhBlendColorShadow, t),
        .pd80SmhShadowExposure =
            a.pd80SmhShadowExposure + ((b.pd80SmhShadowExposure - a.pd80SmhShadowExposure) * t),
        .pd80SmhShadowContrast =
            a.pd80SmhShadowContrast + ((b.pd80SmhShadowContrast - a.pd80SmhShadowContrast) * t),
        .pd80SmhShadowBrightness =
            a.pd80SmhShadowBrightness + ((b.pd80SmhShadowBrightness - a.pd80SmhShadowBrightness) * t),
        .pd80SmhShadowBlendMode = static_cast<float>(std::clamp(
            static_cast<int>(
                std::lround(std::lerp(a.pd80SmhShadowBlendMode, b.pd80SmhShadowBlendMode, t))),
            0,
            20)),
        .pd80SmhShadowOpacity =
            a.pd80SmhShadowOpacity + ((b.pd80SmhShadowOpacity - a.pd80SmhShadowOpacity) * t),
        .pd80SmhShadowTint = a.pd80SmhShadowTint + ((b.pd80SmhShadowTint - a.pd80SmhShadowTint) * t),
        .pd80SmhShadowSaturation =
            a.pd80SmhShadowSaturation + ((b.pd80SmhShadowSaturation - a.pd80SmhShadowSaturation) * t),
        .pd80SmhShadowVibrance =
            a.pd80SmhShadowVibrance + ((b.pd80SmhShadowVibrance - a.pd80SmhShadowVibrance) * t),
        .pd80SmhBlendColorMid = ri::math::Lerp(a.pd80SmhBlendColorMid, b.pd80SmhBlendColorMid, t),
        .pd80SmhMidExposure = a.pd80SmhMidExposure + ((b.pd80SmhMidExposure - a.pd80SmhMidExposure) * t),
        .pd80SmhMidContrast = a.pd80SmhMidContrast + ((b.pd80SmhMidContrast - a.pd80SmhMidContrast) * t),
        .pd80SmhMidBrightness =
            a.pd80SmhMidBrightness + ((b.pd80SmhMidBrightness - a.pd80SmhMidBrightness) * t),
        .pd80SmhMidBlendMode = static_cast<float>(std::clamp(
            static_cast<int>(std::lround(std::lerp(a.pd80SmhMidBlendMode, b.pd80SmhMidBlendMode, t))),
            0,
            20)),
        .pd80SmhMidOpacity = a.pd80SmhMidOpacity + ((b.pd80SmhMidOpacity - a.pd80SmhMidOpacity) * t),
        .pd80SmhMidTint = a.pd80SmhMidTint + ((b.pd80SmhMidTint - a.pd80SmhMidTint) * t),
        .pd80SmhMidSaturation =
            a.pd80SmhMidSaturation + ((b.pd80SmhMidSaturation - a.pd80SmhMidSaturation) * t),
        .pd80SmhMidVibrance =
            a.pd80SmhMidVibrance + ((b.pd80SmhMidVibrance - a.pd80SmhMidVibrance) * t),
        .pd80SmhBlendColorHighlight =
            ri::math::Lerp(a.pd80SmhBlendColorHighlight, b.pd80SmhBlendColorHighlight, t),
        .pd80SmhHighlightExposure =
            a.pd80SmhHighlightExposure + ((b.pd80SmhHighlightExposure - a.pd80SmhHighlightExposure) * t),
        .pd80SmhHighlightContrast =
            a.pd80SmhHighlightContrast + ((b.pd80SmhHighlightContrast - a.pd80SmhHighlightContrast) * t),
        .pd80SmhHighlightBrightness =
            a.pd80SmhHighlightBrightness + ((b.pd80SmhHighlightBrightness - a.pd80SmhHighlightBrightness) * t),
        .pd80SmhHighlightBlendMode = static_cast<float>(std::clamp(
            static_cast<int>(std::lround(
                std::lerp(a.pd80SmhHighlightBlendMode, b.pd80SmhHighlightBlendMode, t))),
            0,
            20)),
        .pd80SmhHighlightOpacity =
            a.pd80SmhHighlightOpacity + ((b.pd80SmhHighlightOpacity - a.pd80SmhHighlightOpacity) * t),
        .pd80SmhHighlightTint =
            a.pd80SmhHighlightTint + ((b.pd80SmhHighlightTint - a.pd80SmhHighlightTint) * t),
        .pd80SmhHighlightSaturation =
            a.pd80SmhHighlightSaturation + ((b.pd80SmhHighlightSaturation - a.pd80SmhHighlightSaturation) * t),
        .pd80SmhHighlightVibrance =
            a.pd80SmhHighlightVibrance + ((b.pd80SmhHighlightVibrance - a.pd80SmhHighlightVibrance) * t),
        .pd80ClMasterStrength =
            a.pd80ClMasterStrength + ((b.pd80ClMasterStrength - a.pd80ClMasterStrength) * t),
        .pd80ClEnableDither = std::lerp(a.pd80ClEnableDither, b.pd80ClEnableDither, t),
        .pd80ClDitherStrength =
            a.pd80ClDitherStrength + ((b.pd80ClDitherStrength - a.pd80ClDitherStrength) * t),
        .pd80ClEnableRgb = std::lerp(a.pd80ClEnableRgb, b.pd80ClEnableRgb, t),
        .pd80ClGreyBlackIn = a.pd80ClGreyBlackIn + ((b.pd80ClGreyBlackIn - a.pd80ClGreyBlackIn) * t),
        .pd80ClGreyWhiteIn = a.pd80ClGreyWhiteIn + ((b.pd80ClGreyWhiteIn - a.pd80ClGreyWhiteIn) * t),
        .pd80ClGreyBlackOut = a.pd80ClGreyBlackOut + ((b.pd80ClGreyBlackOut - a.pd80ClGreyBlackOut) * t),
        .pd80ClGreyWhiteOut = a.pd80ClGreyWhiteOut + ((b.pd80ClGreyWhiteOut - a.pd80ClGreyWhiteOut) * t),
        .pd80ClGreyPos0Shoulder =
            a.pd80ClGreyPos0Shoulder + ((b.pd80ClGreyPos0Shoulder - a.pd80ClGreyPos0Shoulder) * t),
        .pd80ClGreyPos1Shoulder =
            a.pd80ClGreyPos1Shoulder + ((b.pd80ClGreyPos1Shoulder - a.pd80ClGreyPos1Shoulder) * t),
        .pd80ClGreyPos0Toe = a.pd80ClGreyPos0Toe + ((b.pd80ClGreyPos0Toe - a.pd80ClGreyPos0Toe) * t),
        .pd80ClGreyPos1Toe = a.pd80ClGreyPos1Toe + ((b.pd80ClGreyPos1Toe - a.pd80ClGreyPos1Toe) * t),
        .pd80ClRedBlackIn = a.pd80ClRedBlackIn + ((b.pd80ClRedBlackIn - a.pd80ClRedBlackIn) * t),
        .pd80ClRedWhiteIn = a.pd80ClRedWhiteIn + ((b.pd80ClRedWhiteIn - a.pd80ClRedWhiteIn) * t),
        .pd80ClRedBlackOut = a.pd80ClRedBlackOut + ((b.pd80ClRedBlackOut - a.pd80ClRedBlackOut) * t),
        .pd80ClRedWhiteOut = a.pd80ClRedWhiteOut + ((b.pd80ClRedWhiteOut - a.pd80ClRedWhiteOut) * t),
        .pd80ClRedPos0Shoulder =
            a.pd80ClRedPos0Shoulder + ((b.pd80ClRedPos0Shoulder - a.pd80ClRedPos0Shoulder) * t),
        .pd80ClRedPos1Shoulder =
            a.pd80ClRedPos1Shoulder + ((b.pd80ClRedPos1Shoulder - a.pd80ClRedPos1Shoulder) * t),
        .pd80ClRedPos0Toe = a.pd80ClRedPos0Toe + ((b.pd80ClRedPos0Toe - a.pd80ClRedPos0Toe) * t),
        .pd80ClRedPos1Toe = a.pd80ClRedPos1Toe + ((b.pd80ClRedPos1Toe - a.pd80ClRedPos1Toe) * t),
        .pd80ClGreenBlackIn = a.pd80ClGreenBlackIn + ((b.pd80ClGreenBlackIn - a.pd80ClGreenBlackIn) * t),
        .pd80ClGreenWhiteIn = a.pd80ClGreenWhiteIn + ((b.pd80ClGreenWhiteIn - a.pd80ClGreenWhiteIn) * t),
        .pd80ClGreenBlackOut = a.pd80ClGreenBlackOut + ((b.pd80ClGreenBlackOut - a.pd80ClGreenBlackOut) * t),
        .pd80ClGreenWhiteOut = a.pd80ClGreenWhiteOut + ((b.pd80ClGreenWhiteOut - a.pd80ClGreenWhiteOut) * t),
        .pd80ClGreenPos0Shoulder =
            a.pd80ClGreenPos0Shoulder + ((b.pd80ClGreenPos0Shoulder - a.pd80ClGreenPos0Shoulder) * t),
        .pd80ClGreenPos1Shoulder =
            a.pd80ClGreenPos1Shoulder + ((b.pd80ClGreenPos1Shoulder - a.pd80ClGreenPos1Shoulder) * t),
        .pd80ClGreenPos0Toe =
            a.pd80ClGreenPos0Toe + ((b.pd80ClGreenPos0Toe - a.pd80ClGreenPos0Toe) * t),
        .pd80ClGreenPos1Toe =
            a.pd80ClGreenPos1Toe + ((b.pd80ClGreenPos1Toe - a.pd80ClGreenPos1Toe) * t),
        .pd80ClBlueBlackIn = a.pd80ClBlueBlackIn + ((b.pd80ClBlueBlackIn - a.pd80ClBlueBlackIn) * t),
        .pd80ClBlueWhiteIn = a.pd80ClBlueWhiteIn + ((b.pd80ClBlueWhiteIn - a.pd80ClBlueWhiteIn) * t),
        .pd80ClBlueBlackOut = a.pd80ClBlueBlackOut + ((b.pd80ClBlueBlackOut - a.pd80ClBlueBlackOut) * t),
        .pd80ClBlueWhiteOut = a.pd80ClBlueWhiteOut + ((b.pd80ClBlueWhiteOut - a.pd80ClBlueWhiteOut) * t),
        .pd80ClBluePos0Shoulder =
            a.pd80ClBluePos0Shoulder + ((b.pd80ClBluePos0Shoulder - a.pd80ClBluePos0Shoulder) * t),
        .pd80ClBluePos1Shoulder =
            a.pd80ClBluePos1Shoulder + ((b.pd80ClBluePos1Shoulder - a.pd80ClBluePos1Shoulder) * t),
        .pd80ClBluePos0Toe = a.pd80ClBluePos0Toe + ((b.pd80ClBluePos0Toe - a.pd80ClBluePos0Toe) * t),
        .pd80ClBluePos1Toe = a.pd80ClBluePos1Toe + ((b.pd80ClBluePos1Toe - a.pd80ClBluePos1Toe) * t),
        .pd80ScMasterStrength =
            a.pd80ScMasterStrength + ((b.pd80ScMasterStrength - a.pd80ScMasterStrength) * t),
        .pd80ScCorrectionMethod = static_cast<float>(std::clamp(
            static_cast<int>(std::lround(std::lerp(a.pd80ScCorrectionMethod, b.pd80ScCorrectionMethod, t))), 0, 1)),
        .pd80ScCorrectionMethodSaturation = static_cast<float>(std::clamp(
            static_cast<int>(std::lround(std::lerp(
                a.pd80ScCorrectionMethodSaturation, b.pd80ScCorrectionMethodSaturation, t))),
            0,
            1)),
        .pd80ScRedsCyan = a.pd80ScRedsCyan + ((b.pd80ScRedsCyan - a.pd80ScRedsCyan) * t),
        .pd80ScRedsMagenta = a.pd80ScRedsMagenta + ((b.pd80ScRedsMagenta - a.pd80ScRedsMagenta) * t),
        .pd80ScRedsYellow = a.pd80ScRedsYellow + ((b.pd80ScRedsYellow - a.pd80ScRedsYellow) * t),
        .pd80ScRedsBlack = a.pd80ScRedsBlack + ((b.pd80ScRedsBlack - a.pd80ScRedsBlack) * t),
        .pd80ScRedsSaturation =
            a.pd80ScRedsSaturation + ((b.pd80ScRedsSaturation - a.pd80ScRedsSaturation) * t),
        .pd80ScRedsVibrance = a.pd80ScRedsVibrance + ((b.pd80ScRedsVibrance - a.pd80ScRedsVibrance) * t),
        .pd80ScYellowsCyan = a.pd80ScYellowsCyan + ((b.pd80ScYellowsCyan - a.pd80ScYellowsCyan) * t),
        .pd80ScYellowsMagenta =
            a.pd80ScYellowsMagenta + ((b.pd80ScYellowsMagenta - a.pd80ScYellowsMagenta) * t),
        .pd80ScYellowsYellow =
            a.pd80ScYellowsYellow + ((b.pd80ScYellowsYellow - a.pd80ScYellowsYellow) * t),
        .pd80ScYellowsBlack = a.pd80ScYellowsBlack + ((b.pd80ScYellowsBlack - a.pd80ScYellowsBlack) * t),
        .pd80ScYellowsSaturation =
            a.pd80ScYellowsSaturation + ((b.pd80ScYellowsSaturation - a.pd80ScYellowsSaturation) * t),
        .pd80ScYellowsVibrance =
            a.pd80ScYellowsVibrance + ((b.pd80ScYellowsVibrance - a.pd80ScYellowsVibrance) * t),
        .pd80ScGreensCyan = a.pd80ScGreensCyan + ((b.pd80ScGreensCyan - a.pd80ScGreensCyan) * t),
        .pd80ScGreensMagenta =
            a.pd80ScGreensMagenta + ((b.pd80ScGreensMagenta - a.pd80ScGreensMagenta) * t),
        .pd80ScGreensYellow = a.pd80ScGreensYellow + ((b.pd80ScGreensYellow - a.pd80ScGreensYellow) * t),
        .pd80ScGreensBlack = a.pd80ScGreensBlack + ((b.pd80ScGreensBlack - a.pd80ScGreensBlack) * t),
        .pd80ScGreensSaturation =
            a.pd80ScGreensSaturation + ((b.pd80ScGreensSaturation - a.pd80ScGreensSaturation) * t),
        .pd80ScGreensVibrance =
            a.pd80ScGreensVibrance + ((b.pd80ScGreensVibrance - a.pd80ScGreensVibrance) * t),
        .pd80ScCyansCyan = a.pd80ScCyansCyan + ((b.pd80ScCyansCyan - a.pd80ScCyansCyan) * t),
        .pd80ScCyansMagenta = a.pd80ScCyansMagenta + ((b.pd80ScCyansMagenta - a.pd80ScCyansMagenta) * t),
        .pd80ScCyansYellow = a.pd80ScCyansYellow + ((b.pd80ScCyansYellow - a.pd80ScCyansYellow) * t),
        .pd80ScCyansBlack = a.pd80ScCyansBlack + ((b.pd80ScCyansBlack - a.pd80ScCyansBlack) * t),
        .pd80ScCyansSaturation =
            a.pd80ScCyansSaturation + ((b.pd80ScCyansSaturation - a.pd80ScCyansSaturation) * t),
        .pd80ScCyansVibrance = a.pd80ScCyansVibrance + ((b.pd80ScCyansVibrance - a.pd80ScCyansVibrance) * t),
        .pd80ScBluesCyan = a.pd80ScBluesCyan + ((b.pd80ScBluesCyan - a.pd80ScBluesCyan) * t),
        .pd80ScBluesMagenta = a.pd80ScBluesMagenta + ((b.pd80ScBluesMagenta - a.pd80ScBluesMagenta) * t),
        .pd80ScBluesYellow = a.pd80ScBluesYellow + ((b.pd80ScBluesYellow - a.pd80ScBluesYellow) * t),
        .pd80ScBluesBlack = a.pd80ScBluesBlack + ((b.pd80ScBluesBlack - a.pd80ScBluesBlack) * t),
        .pd80ScBluesSaturation =
            a.pd80ScBluesSaturation + ((b.pd80ScBluesSaturation - a.pd80ScBluesSaturation) * t),
        .pd80ScBluesVibrance = a.pd80ScBluesVibrance + ((b.pd80ScBluesVibrance - a.pd80ScBluesVibrance) * t),
        .pd80ScMagentasCyan = a.pd80ScMagentasCyan + ((b.pd80ScMagentasCyan - a.pd80ScMagentasCyan) * t),
        .pd80ScMagentasMagenta =
            a.pd80ScMagentasMagenta + ((b.pd80ScMagentasMagenta - a.pd80ScMagentasMagenta) * t),
        .pd80ScMagentasYellow =
            a.pd80ScMagentasYellow + ((b.pd80ScMagentasYellow - a.pd80ScMagentasYellow) * t),
        .pd80ScMagentasBlack = a.pd80ScMagentasBlack + ((b.pd80ScMagentasBlack - a.pd80ScMagentasBlack) * t),
        .pd80ScMagentasSaturation =
            a.pd80ScMagentasSaturation + ((b.pd80ScMagentasSaturation - a.pd80ScMagentasSaturation) * t),
        .pd80ScMagentasVibrance =
            a.pd80ScMagentasVibrance + ((b.pd80ScMagentasVibrance - a.pd80ScMagentasVibrance) * t),
        .pd80ScWhitesCyan = a.pd80ScWhitesCyan + ((b.pd80ScWhitesCyan - a.pd80ScWhitesCyan) * t),
        .pd80ScWhitesMagenta = a.pd80ScWhitesMagenta + ((b.pd80ScWhitesMagenta - a.pd80ScWhitesMagenta) * t),
        .pd80ScWhitesYellow = a.pd80ScWhitesYellow + ((b.pd80ScWhitesYellow - a.pd80ScWhitesYellow) * t),
        .pd80ScWhitesBlack = a.pd80ScWhitesBlack + ((b.pd80ScWhitesBlack - a.pd80ScWhitesBlack) * t),
        .pd80ScWhitesSaturation =
            a.pd80ScWhitesSaturation + ((b.pd80ScWhitesSaturation - a.pd80ScWhitesSaturation) * t),
        .pd80ScWhitesVibrance = a.pd80ScWhitesVibrance + ((b.pd80ScWhitesVibrance - a.pd80ScWhitesVibrance) * t),
        .pd80ScNeutralsCyan = a.pd80ScNeutralsCyan + ((b.pd80ScNeutralsCyan - a.pd80ScNeutralsCyan) * t),
        .pd80ScNeutralsMagenta =
            a.pd80ScNeutralsMagenta + ((b.pd80ScNeutralsMagenta - a.pd80ScNeutralsMagenta) * t),
        .pd80ScNeutralsYellow =
            a.pd80ScNeutralsYellow + ((b.pd80ScNeutralsYellow - a.pd80ScNeutralsYellow) * t),
        .pd80ScNeutralsBlack = a.pd80ScNeutralsBlack + ((b.pd80ScNeutralsBlack - a.pd80ScNeutralsBlack) * t),
        .pd80ScNeutralsSaturation =
            a.pd80ScNeutralsSaturation + ((b.pd80ScNeutralsSaturation - a.pd80ScNeutralsSaturation) * t),
        .pd80ScNeutralsVibrance =
            a.pd80ScNeutralsVibrance + ((b.pd80ScNeutralsVibrance - a.pd80ScNeutralsVibrance) * t),
        .pd80ScBlacksCyan = a.pd80ScBlacksCyan + ((b.pd80ScBlacksCyan - a.pd80ScBlacksCyan) * t),
        .pd80ScBlacksMagenta = a.pd80ScBlacksMagenta + ((b.pd80ScBlacksMagenta - a.pd80ScBlacksMagenta) * t),
        .pd80ScBlacksYellow = a.pd80ScBlacksYellow + ((b.pd80ScBlacksYellow - a.pd80ScBlacksYellow) * t),
        .pd80ScBlacksBlack = a.pd80ScBlacksBlack + ((b.pd80ScBlacksBlack - a.pd80ScBlacksBlack) * t),
        .pd80ScBlacksSaturation =
            a.pd80ScBlacksSaturation + ((b.pd80ScBlacksSaturation - a.pd80ScBlacksSaturation) * t),
        .pd80ScBlacksVibrance = a.pd80ScBlacksVibrance + ((b.pd80ScBlacksVibrance - a.pd80ScBlacksVibrance) * t),
        .pd80PpMasterStrength = a.pd80PpMasterStrength + ((b.pd80PpMasterStrength - a.pd80PpMasterStrength) * t),
        .pd80PpNumberOfLevels = a.pd80PpNumberOfLevels + ((b.pd80PpNumberOfLevels - a.pd80PpNumberOfLevels) * t),
        .pd80PpPixelSize = a.pd80PpPixelSize + ((b.pd80PpPixelSize - a.pd80PpPixelSize) * t),
        .pd80PpBorderStrength = a.pd80PpBorderStrength + ((b.pd80PpBorderStrength - a.pd80PpBorderStrength) * t),
        .pd80PpEnableDither = a.pd80PpEnableDither + ((b.pd80PpEnableDither - a.pd80PpEnableDither) * t),
        .pd80PpDitherMotion = a.pd80PpDitherMotion + ((b.pd80PpDitherMotion - a.pd80PpDitherMotion) * t),
        .pd80PpDitherStrength = a.pd80PpDitherStrength + ((b.pd80PpDitherStrength - a.pd80PpDitherStrength) * t),
        .pd80MrShape = a.pd80MrShape + ((b.pd80MrShape - a.pd80MrShape) * t),
        .pd80MrInvertShape = a.pd80MrInvertShape + ((b.pd80MrInvertShape - a.pd80MrInvertShape) * t),
        .pd80MrRotation = a.pd80MrRotation + ((b.pd80MrRotation - a.pd80MrRotation) * t),
        .pd80MrCenter = ri::math::Vec2{
            a.pd80MrCenter.x + ((b.pd80MrCenter.x - a.pd80MrCenter.x) * t),
            a.pd80MrCenter.y + ((b.pd80MrCenter.y - a.pd80MrCenter.y) * t),
        },
        .pd80MrSizeX = a.pd80MrSizeX + ((b.pd80MrSizeX - a.pd80MrSizeX) * t),
        .pd80MrSizeY = a.pd80MrSizeY + ((b.pd80MrSizeY - a.pd80MrSizeY) * t),
        .pd80MrDepthPosition = a.pd80MrDepthPosition + ((b.pd80MrDepthPosition - a.pd80MrDepthPosition) * t),
        .pd80MrSmoothing = a.pd80MrSmoothing + ((b.pd80MrSmoothing - a.pd80MrSmoothing) * t),
        .pd80MrDepthSmoothing = a.pd80MrDepthSmoothing + ((b.pd80MrDepthSmoothing - a.pd80MrDepthSmoothing) * t),
        .pd80MrDitherStrength = a.pd80MrDitherStrength + ((b.pd80MrDitherStrength - a.pd80MrDitherStrength) * t),
        .pd80MrColor = ri::math::Lerp(a.pd80MrColor, b.pd80MrColor, t),
        .pd80MrExposure = a.pd80MrExposure + ((b.pd80MrExposure - a.pd80MrExposure) * t),
        .pd80MrContrast = a.pd80MrContrast + ((b.pd80MrContrast - a.pd80MrContrast) * t),
        .pd80MrBrightness = a.pd80MrBrightness + ((b.pd80MrBrightness - a.pd80MrBrightness) * t),
        .pd80MrHue = a.pd80MrHue + ((b.pd80MrHue - a.pd80MrHue) * t),
        .pd80MrSaturation = a.pd80MrSaturation + ((b.pd80MrSaturation - a.pd80MrSaturation) * t),
        .pd80MrVibrance = a.pd80MrVibrance + ((b.pd80MrVibrance - a.pd80MrVibrance) * t),
        .pd80MrEnableGradient = a.pd80MrEnableGradient + ((b.pd80MrEnableGradient - a.pd80MrEnableGradient) * t),
        .pd80MrGradientType = a.pd80MrGradientType + ((b.pd80MrGradientType - a.pd80MrGradientType) * t),
        .pd80MrGradientCurve = a.pd80MrGradientCurve + ((b.pd80MrGradientCurve - a.pd80MrGradientCurve) * t),
        .pd80MrIntensityBoost = a.pd80MrIntensityBoost + ((b.pd80MrIntensityBoost - a.pd80MrIntensityBoost) * t),
        .pd80MrBlendMode = a.pd80MrBlendMode + ((b.pd80MrBlendMode - a.pd80MrBlendMode) * t),
        .pd80MrOpacity = a.pd80MrOpacity + ((b.pd80MrOpacity - a.pd80MrOpacity) * t),
        .pd80BlpMasterStrength = a.pd80BlpMasterStrength + ((b.pd80BlpMasterStrength - a.pd80BlpMasterStrength) * t),
        .pd80BlpEnableDither = a.pd80BlpEnableDither + ((b.pd80BlpEnableDither - a.pd80BlpEnableDither) * t),
        .pd80BlpDitherStrength = a.pd80BlpDitherStrength + ((b.pd80BlpDitherStrength - a.pd80BlpDitherStrength) * t),
        .pd80BlpLutSelector = a.pd80BlpLutSelector + ((b.pd80BlpLutSelector - a.pd80BlpLutSelector) * t),
        .pd80BlpMixChroma = a.pd80BlpMixChroma + ((b.pd80BlpMixChroma - a.pd80BlpMixChroma) * t),
        .pd80BlpMixLuma = a.pd80BlpMixLuma + ((b.pd80BlpMixLuma - a.pd80BlpMixLuma) * t),
        .pd80BlpBlackIn = ri::math::Lerp(a.pd80BlpBlackIn, b.pd80BlpBlackIn, t),
        .pd80BlpWhiteIn = ri::math::Lerp(a.pd80BlpWhiteIn, b.pd80BlpWhiteIn, t),
        .pd80BlpBlackOut = ri::math::Lerp(a.pd80BlpBlackOut, b.pd80BlpBlackOut, t),
        .pd80BlpWhiteOut = ri::math::Lerp(a.pd80BlpWhiteOut, b.pd80BlpWhiteOut, t),
        .pd80BlpGamma = a.pd80BlpGamma + ((b.pd80BlpGamma - a.pd80BlpGamma) * t),
        .pd80CltMasterStrength = a.pd80CltMasterStrength + ((b.pd80CltMasterStrength - a.pd80CltMasterStrength) * t),
        .pd80CltEnableDither = a.pd80CltEnableDither + ((b.pd80CltEnableDither - a.pd80CltEnableDither) * t),
        .pd80CltDitherStrength = a.pd80CltDitherStrength + ((b.pd80CltDitherStrength - a.pd80CltDitherStrength) * t),
        .pd80CltLutSelector = a.pd80CltLutSelector + ((b.pd80CltLutSelector - a.pd80CltLutSelector) * t),
        .pd80CltMixChroma = a.pd80CltMixChroma + ((b.pd80CltMixChroma - a.pd80CltMixChroma) * t),
        .pd80CltMixLuma = a.pd80CltMixLuma + ((b.pd80CltMixLuma - a.pd80CltMixLuma) * t),
        .pd80CltBlackIn = ri::math::Lerp(a.pd80CltBlackIn, b.pd80CltBlackIn, t),
        .pd80CltWhiteIn = ri::math::Lerp(a.pd80CltWhiteIn, b.pd80CltWhiteIn, t),
        .pd80CltBlackOut = ri::math::Lerp(a.pd80CltBlackOut, b.pd80CltBlackOut, t),
        .pd80CltWhiteOut = ri::math::Lerp(a.pd80CltWhiteOut, b.pd80CltWhiteOut, t),
        .pd80CltGamma = a.pd80CltGamma + ((b.pd80CltGamma - a.pd80CltGamma) * t),
        .pd80LcMasterStrength = a.pd80LcMasterStrength + ((b.pd80LcMasterStrength - a.pd80LcMasterStrength) * t),
        .pd80LcTextureWidth = a.pd80LcTextureWidth + ((b.pd80LcTextureWidth - a.pd80LcTextureWidth) * t),
        .pd80LcTextureHeight = a.pd80LcTextureHeight + ((b.pd80LcTextureHeight - a.pd80LcTextureHeight) * t),
        .pd80LfMasterStrength = a.pd80LfMasterStrength + ((b.pd80LfMasterStrength - a.pd80LfMasterStrength) * t),
        .pd80LfTransitionSpeed = a.pd80LfTransitionSpeed + ((b.pd80LfTransitionSpeed - a.pd80LfTransitionSpeed) * t),
        .pd80LfMinLevel = a.pd80LfMinLevel + ((b.pd80LfMinLevel - a.pd80LfMinLevel) * t),
        .pd80LfMaxLevel = a.pd80LfMaxLevel + ((b.pd80LfMaxLevel - a.pd80LfMaxLevel) * t),
        .pd80Cg4MasterStrength = a.pd80Cg4MasterStrength + ((b.pd80Cg4MasterStrength - a.pd80Cg4MasterStrength) * t),
        .pd80Cg4LumaMode = a.pd80Cg4LumaMode + ((b.pd80Cg4LumaMode - a.pd80Cg4LumaMode) * t),
        .pd80Cg4SeparationMode = a.pd80Cg4SeparationMode + ((b.pd80Cg4SeparationMode - a.pd80Cg4SeparationMode) * t),
        .pd80Cg4EnableDither = a.pd80Cg4EnableDither + ((b.pd80Cg4EnableDither - a.pd80Cg4EnableDither) * t),
        .pd80Cg4DitherStrength = a.pd80Cg4DitherStrength + ((b.pd80Cg4DitherStrength - a.pd80Cg4DitherStrength) * t),
        .pd80Cg4DesaturateBase = a.pd80Cg4DesaturateBase + ((b.pd80Cg4DesaturateBase - a.pd80Cg4DesaturateBase) * t),
        .pd80Cg4FinalMix = a.pd80Cg4FinalMix + ((b.pd80Cg4FinalMix - a.pd80Cg4FinalMix) * t),
        .pd80Cg4LightSceneMidColor = ri::math::Lerp(a.pd80Cg4LightSceneMidColor, b.pd80Cg4LightSceneMidColor, t),
        .pd80Cg4LightSceneMidBlendMode =
            a.pd80Cg4LightSceneMidBlendMode + ((b.pd80Cg4LightSceneMidBlendMode - a.pd80Cg4LightSceneMidBlendMode) * t),
        .pd80Cg4LightSceneMidOpacity =
            a.pd80Cg4LightSceneMidOpacity + ((b.pd80Cg4LightSceneMidOpacity - a.pd80Cg4LightSceneMidOpacity) * t),
        .pd80Cg4LightSceneShadowColor =
            ri::math::Lerp(a.pd80Cg4LightSceneShadowColor, b.pd80Cg4LightSceneShadowColor, t),
        .pd80Cg4LightSceneShadowBlendMode =
            a.pd80Cg4LightSceneShadowBlendMode
            + ((b.pd80Cg4LightSceneShadowBlendMode - a.pd80Cg4LightSceneShadowBlendMode) * t),
        .pd80Cg4LightSceneShadowOpacity =
            a.pd80Cg4LightSceneShadowOpacity + ((b.pd80Cg4LightSceneShadowOpacity - a.pd80Cg4LightSceneShadowOpacity) * t),
        .pd80Cg4EnableDarkScene = a.pd80Cg4EnableDarkScene + ((b.pd80Cg4EnableDarkScene - a.pd80Cg4EnableDarkScene) * t),
        .pd80Cg4DarkSceneMidColor = ri::math::Lerp(a.pd80Cg4DarkSceneMidColor, b.pd80Cg4DarkSceneMidColor, t),
        .pd80Cg4DarkSceneMidBlendMode =
            a.pd80Cg4DarkSceneMidBlendMode + ((b.pd80Cg4DarkSceneMidBlendMode - a.pd80Cg4DarkSceneMidBlendMode) * t),
        .pd80Cg4DarkSceneMidOpacity =
            a.pd80Cg4DarkSceneMidOpacity + ((b.pd80Cg4DarkSceneMidOpacity - a.pd80Cg4DarkSceneMidOpacity) * t),
        .pd80Cg4DarkSceneShadowColor = ri::math::Lerp(a.pd80Cg4DarkSceneShadowColor, b.pd80Cg4DarkSceneShadowColor, t),
        .pd80Cg4DarkSceneShadowBlendMode =
            a.pd80Cg4DarkSceneShadowBlendMode
            + ((b.pd80Cg4DarkSceneShadowBlendMode - a.pd80Cg4DarkSceneShadowBlendMode) * t),
        .pd80Cg4DarkSceneShadowOpacity =
            a.pd80Cg4DarkSceneShadowOpacity + ((b.pd80Cg4DarkSceneShadowOpacity - a.pd80Cg4DarkSceneShadowOpacity) * t),
        .pd80Cg4MinLevel = a.pd80Cg4MinLevel + ((b.pd80Cg4MinLevel - a.pd80Cg4MinLevel) * t),
        .pd80Cg4MaxLevel = a.pd80Cg4MaxLevel + ((b.pd80Cg4MaxLevel - a.pd80Cg4MaxLevel) * t),
        .pd80CcMasterStrength = a.pd80CcMasterStrength + ((b.pd80CcMasterStrength - a.pd80CcMasterStrength) * t),
        .pd80CcEnableWhitepoint = a.pd80CcEnableWhitepoint + ((b.pd80CcEnableWhitepoint - a.pd80CcEnableWhitepoint) * t),
        .pd80CcWhitepointStrength =
            a.pd80CcWhitepointStrength + ((b.pd80CcWhitepointStrength - a.pd80CcWhitepointStrength) * t),
        .pd80CcEnableBlackpoint = a.pd80CcEnableBlackpoint + ((b.pd80CcEnableBlackpoint - a.pd80CcEnableBlackpoint) * t),
        .pd80CcBlackpointStrength =
            a.pd80CcBlackpointStrength + ((b.pd80CcBlackpointStrength - a.pd80CcBlackpointStrength) * t),
        .pd80RccMasterStrength = a.pd80RccMasterStrength + ((b.pd80RccMasterStrength - a.pd80RccMasterStrength) * t),
        .pd80RccEnableDither = a.pd80RccEnableDither + ((b.pd80RccEnableDither - a.pd80RccEnableDither) * t),
        .pd80RccDitherStrength = a.pd80RccDitherStrength + ((b.pd80RccDitherStrength - a.pd80RccDitherStrength) * t),
        .pd80RccEnableWhitepoint =
            a.pd80RccEnableWhitepoint + ((b.pd80RccEnableWhitepoint - a.pd80RccEnableWhitepoint) * t),
        .pd80RccWhitepointRespectLuma =
            a.pd80RccWhitepointRespectLuma + ((b.pd80RccWhitepointRespectLuma - a.pd80RccWhitepointRespectLuma) * t),
        .pd80RccWhitepointMethod = static_cast<float>(std::clamp(
            static_cast<int>(std::lround(std::lerp(a.pd80RccWhitepointMethod, b.pd80RccWhitepointMethod, t))), 0, 1)),
        .pd80RccWhitepointStrength =
            a.pd80RccWhitepointStrength + ((b.pd80RccWhitepointStrength - a.pd80RccWhitepointStrength) * t),
        .pd80RccWhitepointLumaStrength =
            a.pd80RccWhitepointLumaStrength + ((b.pd80RccWhitepointLumaStrength - a.pd80RccWhitepointLumaStrength) * t),
        .pd80RccEnableBlackpoint =
            a.pd80RccEnableBlackpoint + ((b.pd80RccEnableBlackpoint - a.pd80RccEnableBlackpoint) * t),
        .pd80RccBlackpointRespectLuma =
            a.pd80RccBlackpointRespectLuma + ((b.pd80RccBlackpointRespectLuma - a.pd80RccBlackpointRespectLuma) * t),
        .pd80RccBlackpointMethod = static_cast<float>(std::clamp(
            static_cast<int>(std::lround(std::lerp(a.pd80RccBlackpointMethod, b.pd80RccBlackpointMethod, t))), 0, 1)),
        .pd80RccBlackpointStrength =
            a.pd80RccBlackpointStrength + ((b.pd80RccBlackpointStrength - a.pd80RccBlackpointStrength) * t),
        .pd80RccBlackpointLumaStrength =
            a.pd80RccBlackpointLumaStrength + ((b.pd80RccBlackpointLumaStrength - a.pd80RccBlackpointLumaStrength) * t),
        .pd80RccEnableMidpoint =
            a.pd80RccEnableMidpoint + ((b.pd80RccEnableMidpoint - a.pd80RccEnableMidpoint) * t),
        .pd80RccMidpointRespectLuma =
            a.pd80RccMidpointRespectLuma + ((b.pd80RccMidpointRespectLuma - a.pd80RccMidpointRespectLuma) * t),
        .pd80RccMidUseAltMethod =
            a.pd80RccMidUseAltMethod + ((b.pd80RccMidUseAltMethod - a.pd80RccMidUseAltMethod) * t),
        .pd80RccMidScale = a.pd80RccMidScale + ((b.pd80RccMidScale - a.pd80RccMidScale) * t),
        .pd80FaMasterStrength = a.pd80FaMasterStrength + ((b.pd80FaMasterStrength - a.pd80FaMasterStrength) * t),
        .pd80FaAdjustShoulder = a.pd80FaAdjustShoulder + ((b.pd80FaAdjustShoulder - a.pd80FaAdjustShoulder) * t),
        .pd80FaAdjustLinear = a.pd80FaAdjustLinear + ((b.pd80FaAdjustLinear - a.pd80FaAdjustLinear) * t),
        .pd80FaAdjustToe = a.pd80FaAdjustToe + ((b.pd80FaAdjustToe - a.pd80FaAdjustToe) * t),
        .pd80HbMasterStrength = a.pd80HbMasterStrength + ((b.pd80HbMasterStrength - a.pd80HbMasterStrength) * t),
        .pd80HbDebugBloom = a.pd80HbDebugBloom + ((b.pd80HbDebugBloom - a.pd80HbDebugBloom) * t),
        .pd80HbDitherStrength = a.pd80HbDitherStrength + ((b.pd80HbDitherStrength - a.pd80HbDitherStrength) * t),
        .pd80HbMix = a.pd80HbMix + ((b.pd80HbMix - a.pd80HbMix) * t),
        .pd80HbThreshold = a.pd80HbThreshold + ((b.pd80HbThreshold - a.pd80HbThreshold) * t),
        .pd80HbGreyValue = a.pd80HbGreyValue + ((b.pd80HbGreyValue - a.pd80HbGreyValue) * t),
        .pd80HbExposure = a.pd80HbExposure + ((b.pd80HbExposure - a.pd80HbExposure) * t),
        .pd80HbBlurSigma = a.pd80HbBlurSigma + ((b.pd80HbBlurSigma - a.pd80HbBlurSigma) * t),
        .pd80HbSaturation = a.pd80HbSaturation + ((b.pd80HbSaturation - a.pd80HbSaturation) * t),
        .pd80Sc2MasterStrength = a.pd80Sc2MasterStrength + ((b.pd80Sc2MasterStrength - a.pd80Sc2MasterStrength) * t),
        .pd80Sc2CorrectionMethod = static_cast<float>(std::clamp(
            static_cast<int>(std::lround(std::lerp(a.pd80Sc2CorrectionMethod, b.pd80Sc2CorrectionMethod, t))), 0, 1)),
        .pd80Sc2SaturationScale = a.pd80Sc2SaturationScale + ((b.pd80Sc2SaturationScale - a.pd80Sc2SaturationScale) * t),
        .pd80Sc2LightnessScale = a.pd80Sc2LightnessScale + ((b.pd80Sc2LightnessScale - a.pd80Sc2LightnessScale) * t),
        .colourfulness = a.colourfulness + ((b.colourfulness - a.colourfulness) * t),
        .colourfulnessLimitLuma = a.colourfulnessLimitLuma + ((b.colourfulnessLimitLuma - a.colourfulnessLimitLuma) * t),
        .filmicPassStrength = a.filmicPassStrength + ((b.filmicPassStrength - a.filmicPassStrength) * t),
        .filmicPassFade = a.filmicPassFade + ((b.filmicPassFade - a.filmicPassFade) * t),
        .filmicPassBleach = a.filmicPassBleach + ((b.filmicPassBleach - a.filmicPassBleach) * t),
        .filmicPassSaturation = a.filmicPassSaturation + ((b.filmicPassSaturation - a.filmicPassSaturation) * t),
        .filmGrain2Amount = a.filmGrain2Amount + ((b.filmGrain2Amount - a.filmGrain2Amount) * t),
        .filmGrain2ColorAmount = a.filmGrain2ColorAmount + ((b.filmGrain2ColorAmount - a.filmGrain2ColorAmount) * t),
        .filmGrain2LuminanceAmount =
            a.filmGrain2LuminanceAmount + ((b.filmGrain2LuminanceAmount - a.filmGrain2LuminanceAmount) * t),
        .filmGrain2Size = a.filmGrain2Size + ((b.filmGrain2Size - a.filmGrain2Size) * t),
        .denoiseStrength = a.denoiseStrength + ((b.denoiseStrength - a.denoiseStrength) * t),
        .denoiseNoiseLevel = a.denoiseNoiseLevel + ((b.denoiseNoiseLevel - a.denoiseNoiseLevel) * t),
        .denoiseLerpCoefficient = a.denoiseLerpCoefficient + ((b.denoiseLerpCoefficient - a.denoiseLerpCoefficient) * t),
        .denoiseWeightThreshold = a.denoiseWeightThreshold + ((b.denoiseWeightThreshold - a.denoiseWeightThreshold) * t),
        .denoiseCounterThreshold = a.denoiseCounterThreshold + ((b.denoiseCounterThreshold - a.denoiseCounterThreshold) * t),
        .denoiseGaussianSigma = a.denoiseGaussianSigma + ((b.denoiseGaussianSigma - a.denoiseGaussianSigma) * t),
        .adaptiveSharpenStrength = a.adaptiveSharpenStrength + ((b.adaptiveSharpenStrength - a.adaptiveSharpenStrength) * t),
        .adaptiveSharpenCurveSlope = a.adaptiveSharpenCurveSlope + ((b.adaptiveSharpenCurveSlope - a.adaptiveSharpenCurveSlope) * t),
        .adaptiveSharpenLightOvershoot =
            a.adaptiveSharpenLightOvershoot + ((b.adaptiveSharpenLightOvershoot - a.adaptiveSharpenLightOvershoot) * t),
        .adaptiveSharpenDarkOvershoot =
            a.adaptiveSharpenDarkOvershoot + ((b.adaptiveSharpenDarkOvershoot - a.adaptiveSharpenDarkOvershoot) * t),
        .adaptiveSharpenLightComprLow =
            a.adaptiveSharpenLightComprLow + ((b.adaptiveSharpenLightComprLow - a.adaptiveSharpenLightComprLow) * t),
        .adaptiveSharpenLightComprHigh =
            a.adaptiveSharpenLightComprHigh + ((b.adaptiveSharpenLightComprHigh - a.adaptiveSharpenLightComprHigh) * t),
        .adaptiveSharpenDarkComprLow =
            a.adaptiveSharpenDarkComprLow + ((b.adaptiveSharpenDarkComprLow - a.adaptiveSharpenDarkComprLow) * t),
        .adaptiveSharpenDarkComprHigh =
            a.adaptiveSharpenDarkComprHigh + ((b.adaptiveSharpenDarkComprHigh - a.adaptiveSharpenDarkComprHigh) * t),
        .adaptiveSharpenScaleLim = a.adaptiveSharpenScaleLim + ((b.adaptiveSharpenScaleLim - a.adaptiveSharpenScaleLim) * t),
        .adaptiveSharpenScaleCs = a.adaptiveSharpenScaleCs + ((b.adaptiveSharpenScaleCs - a.adaptiveSharpenScaleCs) * t),
        .adaptiveSharpenPmP = a.adaptiveSharpenPmP + ((b.adaptiveSharpenPmP - a.adaptiveSharpenPmP) * t),
        .gaussianBlurStrength = a.gaussianBlurStrength + ((b.gaussianBlurStrength - a.gaussianBlurStrength) * t),
        .gaussianBlurOffset = a.gaussianBlurOffset + ((b.gaussianBlurOffset - a.gaussianBlurOffset) * t),
        .gaussianBlurRadius = (t < 0.5f) ? a.gaussianBlurRadius : b.gaussianBlurRadius,
        .fineSharpStrength = a.fineSharpStrength + ((b.fineSharpStrength - a.fineSharpStrength) * t),
        .fineSharpEqualization = a.fineSharpEqualization + ((b.fineSharpEqualization - a.fineSharpEqualization) * t),
        .fineSharpXStrength = a.fineSharpXStrength + ((b.fineSharpXStrength - a.fineSharpXStrength) * t),
        .fineSharpXRepair = a.fineSharpXRepair + ((b.fineSharpXRepair - a.fineSharpXRepair) * t),
        .fineSharpLStrength = a.fineSharpLStrength + ((b.fineSharpLStrength - a.fineSharpLStrength) * t),
        .fineSharpPStrength = a.fineSharpPStrength + ((b.fineSharpPStrength - a.fineSharpPStrength) * t),
        .fineSharpMode = (t < 0.5f) ? a.fineSharpMode : b.fineSharpMode,
        .martyBloomThreshold = a.martyBloomThreshold + ((b.martyBloomThreshold - a.martyBloomThreshold) * t),
        .martyBloomAmount = a.martyBloomAmount + ((b.martyBloomAmount - a.martyBloomAmount) * t),
        .martyBloomSaturation = a.martyBloomSaturation + ((b.martyBloomSaturation - a.martyBloomSaturation) * t),
        .martyBloomMixMode = (t < 0.5f) ? a.martyBloomMixMode : b.martyBloomMixMode,
        .martyBloomTint = {
            a.martyBloomTint.x + ((b.martyBloomTint.x - a.martyBloomTint.x) * t),
            a.martyBloomTint.y + ((b.martyBloomTint.y - a.martyBloomTint.y) * t),
            a.martyBloomTint.z + ((b.martyBloomTint.z - a.martyBloomTint.z) * t),
        },
        .creatorDofStrength = a.creatorDofStrength + ((b.creatorDofStrength - a.creatorDofStrength) * t),
        .creatorDofAutoFocus = (t < 0.5f) ? a.creatorDofAutoFocus : b.creatorDofAutoFocus,
        .creatorDofManualFocusDepth =
            a.creatorDofManualFocusDepth + ((b.creatorDofManualFocusDepth - a.creatorDofManualFocusDepth) * t),
        .creatorDofInfiniteFocus =
            a.creatorDofInfiniteFocus + ((b.creatorDofInfiniteFocus - a.creatorDofInfiniteFocus) * t),
        .creatorDofFocusPoint = {
            a.creatorDofFocusPoint.x + ((b.creatorDofFocusPoint.x - a.creatorDofFocusPoint.x) * t),
            a.creatorDofFocusPoint.y + ((b.creatorDofFocusPoint.y - a.creatorDofFocusPoint.y) * t),
        },
        .creatorDofFocusRadius = a.creatorDofFocusRadius + ((b.creatorDofFocusRadius - a.creatorDofFocusRadius) * t),
        .creatorDofFocusSamples = (t < 0.5f) ? a.creatorDofFocusSamples : b.creatorDofFocusSamples,
        .creatorDofNearBlurCurve = a.creatorDofNearBlurCurve + ((b.creatorDofNearBlurCurve - a.creatorDofNearBlurCurve) * t),
        .creatorDofFarBlurCurve = a.creatorDofFarBlurCurve + ((b.creatorDofFarBlurCurve - a.creatorDofFarBlurCurve) * t),
        .creatorDofBlurRadius = a.creatorDofBlurRadius + ((b.creatorDofBlurRadius - a.creatorDofBlurRadius) * t),
        .creatorDofRingSamples = (t < 0.5f) ? a.creatorDofRingSamples : b.creatorDofRingSamples,
        .creatorDofRingRings = (t < 0.5f) ? a.creatorDofRingRings : b.creatorDofRingRings,
        .creatorDofRingThreshold = a.creatorDofRingThreshold + ((b.creatorDofRingThreshold - a.creatorDofRingThreshold) * t),
        .creatorDofRingGain = a.creatorDofRingGain + ((b.creatorDofRingGain - a.creatorDofRingGain) * t),
        .creatorDofRingBias = a.creatorDofRingBias + ((b.creatorDofRingBias - a.creatorDofRingBias) * t),
        .creatorDofRingFringe = a.creatorDofRingFringe + ((b.creatorDofRingFringe - a.creatorDofRingFringe) * t),
        .ambientLightIntensity = a.ambientLightIntensity + ((b.ambientLightIntensity - a.ambientLightIntensity) * t),
        .ambientLightThreshold = a.ambientLightThreshold + ((b.ambientLightThreshold - a.ambientLightThreshold) * t),
        .ambientLightAdaptation = (t < 0.5f) ? a.ambientLightAdaptation : b.ambientLightAdaptation,
        .ambientLightAdapt = a.ambientLightAdapt + ((b.ambientLightAdapt - a.ambientLightAdapt) * t),
        .ambientLightAdaptBaseMult =
            a.ambientLightAdaptBaseMult + ((b.ambientLightAdaptBaseMult - a.ambientLightAdaptBaseMult) * t),
        .ambientLightAdaptBlackLevel = (t < 0.5f) ? a.ambientLightAdaptBlackLevel : b.ambientLightAdaptBlackLevel,
        .ambientLightDither = (t < 0.5f) ? a.ambientLightDither : b.ambientLightDither,
        .ambientLightDirt = (t < 0.5f) ? a.ambientLightDirt : b.ambientLightDirt,
        .ambientLightAdaptiveMode = (t < 0.5f) ? a.ambientLightAdaptiveMode : b.ambientLightAdaptiveMode,
        .ambientLightDirtInt = a.ambientLightDirtInt + ((b.ambientLightDirtInt - a.ambientLightDirtInt) * t),
        .ambientLightDirtOvrInt = a.ambientLightDirtOvrInt + ((b.ambientLightDirtOvrInt - a.ambientLightDirtOvrInt) * t),
        .fakeMotionBlurRecall = a.fakeMotionBlurRecall + ((b.fakeMotionBlurRecall - a.fakeMotionBlurRecall) * t),
        .fakeMotionBlurSoftness = a.fakeMotionBlurSoftness + ((b.fakeMotionBlurSoftness - a.fakeMotionBlurSoftness) * t),
        .reflectiveBumpMappingStrength =
            a.reflectiveBumpMappingStrength + ((b.reflectiveBumpMappingStrength - a.reflectiveBumpMappingStrength) * t),
        .reflectiveBumpMappingBlurWidthPixels = a.reflectiveBumpMappingBlurWidthPixels
            + ((b.reflectiveBumpMappingBlurWidthPixels - a.reflectiveBumpMappingBlurWidthPixels) * t),
        .reflectiveBumpMappingSampleCount =
            (t < 0.5f) ? a.reflectiveBumpMappingSampleCount : b.reflectiveBumpMappingSampleCount,
        .reflectiveBumpMappingReliefHeight = a.reflectiveBumpMappingReliefHeight
            + ((b.reflectiveBumpMappingReliefHeight - a.reflectiveBumpMappingReliefHeight) * t),
        .reflectiveBumpMappingFresnelReflectance = a.reflectiveBumpMappingFresnelReflectance
            + ((b.reflectiveBumpMappingFresnelReflectance - a.reflectiveBumpMappingFresnelReflectance) * t),
        .reflectiveBumpMappingFresnelMult = a.reflectiveBumpMappingFresnelMult
            + ((b.reflectiveBumpMappingFresnelMult - a.reflectiveBumpMappingFresnelMult) * t),
        .reflectiveBumpMappingLowerThreshold = a.reflectiveBumpMappingLowerThreshold
            + ((b.reflectiveBumpMappingLowerThreshold - a.reflectiveBumpMappingLowerThreshold) * t),
        .reflectiveBumpMappingUpperThreshold = a.reflectiveBumpMappingUpperThreshold
            + ((b.reflectiveBumpMappingUpperThreshold - a.reflectiveBumpMappingUpperThreshold) * t),
        .reflectiveBumpMappingColorMaskRed = a.reflectiveBumpMappingColorMaskRed
            + ((b.reflectiveBumpMappingColorMaskRed - a.reflectiveBumpMappingColorMaskRed) * t),
        .reflectiveBumpMappingColorMaskOrange = a.reflectiveBumpMappingColorMaskOrange
            + ((b.reflectiveBumpMappingColorMaskOrange - a.reflectiveBumpMappingColorMaskOrange) * t),
        .reflectiveBumpMappingColorMaskYellow = a.reflectiveBumpMappingColorMaskYellow
            + ((b.reflectiveBumpMappingColorMaskYellow - a.reflectiveBumpMappingColorMaskYellow) * t),
        .reflectiveBumpMappingColorMaskGreen = a.reflectiveBumpMappingColorMaskGreen
            + ((b.reflectiveBumpMappingColorMaskGreen - a.reflectiveBumpMappingColorMaskGreen) * t),
        .reflectiveBumpMappingColorMaskCyan = a.reflectiveBumpMappingColorMaskCyan
            + ((b.reflectiveBumpMappingColorMaskCyan - a.reflectiveBumpMappingColorMaskCyan) * t),
        .reflectiveBumpMappingColorMaskBlue = a.reflectiveBumpMappingColorMaskBlue
            + ((b.reflectiveBumpMappingColorMaskBlue - a.reflectiveBumpMappingColorMaskBlue) * t),
        .reflectiveBumpMappingColorMaskMagenta = a.reflectiveBumpMappingColorMaskMagenta
            + ((b.reflectiveBumpMappingColorMaskMagenta - a.reflectiveBumpMappingColorMaskMagenta) * t),
        .reflectiveBumpMappingDepthFarPlane = a.reflectiveBumpMappingDepthFarPlane
            + ((b.reflectiveBumpMappingDepthFarPlane - a.reflectiveBumpMappingDepthFarPlane) * t),
        .cropScaleContentWidth = a.cropScaleContentWidth + ((b.cropScaleContentWidth - a.cropScaleContentWidth) * t),
        .cropScaleContentHeight = a.cropScaleContentHeight + ((b.cropScaleContentHeight - a.cropScaleContentHeight) * t),
        .cropScaleIntermediateWidth = a.cropScaleIntermediateWidth
            + ((b.cropScaleIntermediateWidth - a.cropScaleIntermediateWidth) * t),
        .cropScaleIntermediateHeight = a.cropScaleIntermediateHeight
            + ((b.cropScaleIntermediateHeight - a.cropScaleIntermediateHeight) * t),
        .cropScaleFinalWidth = a.cropScaleFinalWidth + ((b.cropScaleFinalWidth - a.cropScaleFinalWidth) * t),
        .cropScaleFinalHeight = a.cropScaleFinalHeight + ((b.cropScaleFinalHeight - a.cropScaleFinalHeight) * t),
        .cropScaleFilter = (t < 0.5f) ? a.cropScaleFilter : b.cropScaleFilter,
        .cropScaleStrength = a.cropScaleStrength + ((b.cropScaleStrength - a.cropScaleStrength) * t),
        .barbatosFakeHdrPreset = (t < 0.5f) ? a.barbatosFakeHdrPreset : b.barbatosFakeHdrPreset,
        .barbatosFakeHdrStrength = a.barbatosFakeHdrStrength
            + ((b.barbatosFakeHdrStrength - a.barbatosFakeHdrStrength) * t),
        .riAdaptiveDebandStrength = a.riAdaptiveDebandStrength
            + ((b.riAdaptiveDebandStrength - a.riAdaptiveDebandStrength) * t),
        .riAdaptiveDebandRadius = a.riAdaptiveDebandRadius + ((b.riAdaptiveDebandRadius - a.riAdaptiveDebandRadius) * t),
        .riAdaptiveDebandThreshold = a.riAdaptiveDebandThreshold
            + ((b.riAdaptiveDebandThreshold - a.riAdaptiveDebandThreshold) * t),
        .riAdaptiveDebandIterations = (t < 0.5f) ? a.riAdaptiveDebandIterations : b.riAdaptiveDebandIterations,
        .riLocalSharpenStrength = a.riLocalSharpenStrength
            + ((b.riLocalSharpenStrength - a.riLocalSharpenStrength) * t),
        .riLocalSharpenRadius = a.riLocalSharpenRadius + ((b.riLocalSharpenRadius - a.riLocalSharpenRadius) * t),
        .riLocalSharpenClamp = a.riLocalSharpenClamp + ((b.riLocalSharpenClamp - a.riLocalSharpenClamp) * t),
        .riLocalSharpenEdgeLimit = a.riLocalSharpenEdgeLimit
            + ((b.riLocalSharpenEdgeLimit - a.riLocalSharpenEdgeLimit) * t),
        .riOutlineStrength = a.riOutlineStrength + ((b.riOutlineStrength - a.riOutlineStrength) * t),
        .riOutlineThickness = a.riOutlineThickness + ((b.riOutlineThickness - a.riOutlineThickness) * t),
        .riOutlineDepthSensitivity = a.riOutlineDepthSensitivity
            + ((b.riOutlineDepthSensitivity - a.riOutlineDepthSensitivity) * t),
        .riOutlineColorSensitivity = a.riOutlineColorSensitivity
            + ((b.riOutlineColorSensitivity - a.riOutlineColorSensitivity) * t),
        .riOutlineMethod = (t < 0.5f) ? a.riOutlineMethod : b.riOutlineMethod,
        .riOutlineColor = ri::math::Lerp(a.riOutlineColor, b.riOutlineColor, t),
        .riOutlineWobbleAmount = a.riOutlineWobbleAmount
            + ((b.riOutlineWobbleAmount - a.riOutlineWobbleAmount) * t),
        .riOutlineWobbleSpeed = a.riOutlineWobbleSpeed + ((b.riOutlineWobbleSpeed - a.riOutlineWobbleSpeed) * t),
        .riOutlineWobbleFrequency = a.riOutlineWobbleFrequency
            + ((b.riOutlineWobbleFrequency - a.riOutlineWobbleFrequency) * t),
        .riOutlineDebug = a.riOutlineDebug + ((b.riOutlineDebug - a.riOutlineDebug) * t),
    });
}

inline PostProcessParameters ComposePostProcessPresetStack(
    std::span<const PostProcessPresetLayer> layers,
    const PostProcessParameters& base = {}) {
    PostProcessParameters result = SanitizePostProcessParameters(base);
    for (const PostProcessPresetLayer& layer : layers) {
        const float blend = ClampUnit(layer.blend);
        if (blend <= 0.0f) {
            continue;
        }
        result = BlendPostProcessParameters(result, MakePostProcessPreset(layer.preset), blend);
    }
    return result;
}

} // namespace ri::render
