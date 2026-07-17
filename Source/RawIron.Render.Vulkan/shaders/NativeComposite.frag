#version 450

layout(location = 0) in vec2 vUv;

layout(location = 0) out vec4 fragColor;

layout(std140, set = 0, binding = 0) uniform CameraData {
    mat4 viewProjection;
    vec4 cameraWorldPosition;
    vec4 renderTuning;
    vec4 postProcessPrimary;
    vec4 postProcessTint;
    vec4 postProcessSecondary;
    mat4 lightViewProjection;
    vec4 lightDirectionIntensity;
    vec4 localLightPositionRange;
    vec4 localLightColorIntensity;
    vec4 directionalLightColorIntensity;
    vec4 viewportMetrics;
    /// x=CAS sharpen (0–1), y=CAS contrast adaptation, z=bloom strength, w=linear HDR bloom threshold.
    vec4 presentationTuning;
    /// x=luma tone curve, y=TriDither strength, z=deband, w=SweetFX vignette type 0 strength.
    vec4 presentationColorGrading;
    /// x=SweetFX-style film grain intensity, yzw reserved.
    vec4 presentationExtra;
    /// xyz=RGB_Lift, w=liftGammaGainMix (0–1).
    vec4 lggLiftMix;
    /// xyz=RGB_Gamma (SweetFX), w unused.
    vec4 lggGammaRgb;
    /// xyz=RGB_Gain, w unused.
    vec4 lggGainRgb;
    /// xyz=VibranceRGBBalance, w=Vibrance (-1..1).
    vec4 vibranceBalanceAmount;
    /// SweetFX Technicolor v1: x=Power, y=Strength, zw=RGBNegative.rg.
    vec4 technicolor1PowStrNegRg;
    vec4 technicolor1NegBPad;
    /// SweetFX Technicolor2: xyz=ColorStrength, w=Brightness.
    vec4 technicolor2ColBright;
    /// xy=Saturation,Strength.
    vec4 technicolor2SatStrPad;
    /// SweetFX Sepia.fx (`Tint`): xyz=tint rgb, w=strength.
    vec4 sepiaTintXyzStrength;
    /// x=preset (0–17), y=color saturation (1=identity), zw unused.
    vec4 monochromePresetSat;
    /// xyz=custom conversion coeffs when preset==0.
    vec4 monochromeCustomCoeff;
    /// SweetFX DPX.fx — xyz=RGB_Curve.
    vec4 dpxRgbCurvePad;
    /// xyz=RGB_C.
    vec4 dpxRgbCPad;
    /// x=Contrast, y=Saturation, z=Colorfulness, w=Strength.
    vec4 dpxContrastSatColorStr;
    /// SweetFX ColorMatrix.fx — xyz = matrix row for new R/G; `mul(rows, c)` per reference.
    vec4 colorMatrixRowR;
    vec4 colorMatrixRowG;
    /// xyz = row for new B, w = Strength.
    vec4 colorMatrixRowBStr;
    /// SweetFX FakeHDR.fx: x=HDRPower, y=radius1, z=radius2, w=strength (0=skip).
    vec4 fakeHdrPowerR1R2Str;
    /// SweetFX Levels.fx v1.2: x=black (0–255), y=white (0–255), z=strength, w=clip debug (0/1).
    vec4 levelsBlackWhiteStrClip;
    /// SweetFX LumaSharpen v1.5: xyz=strength,clamp,offset; w=pattern(0–3) + 4*show_debug.
    vec4 lumaSharpenPack;
    /// SweetFX Curves.fx: x=Contrast, y=Mode, z=Formula, w=strength (0=skip full pass).
    vec4 sweetFxCurvesPack;
    /// SweetFX ChromaticAberration.fx: xy=Shift (pixels, ref. ±10), z=Strength, w=0 (unused).
    vec4 sweetFxChromaticAberrationPack;
    /// SweetFX Border.fx: x=border_width.x px, y=border_width.y px, z=border_ratio, w=strength (0=off).
    vec4 sweetFxBorderPack;
    /// SweetFX Border.fx: xyz=border_color, w unused.
    vec4 sweetFxBorderColorPad;
    /// SweetFX Cartoon.fx: x=Power, y=EdgeSlope, z=strength (0=skip), w=0.
    vec4 sweetFxCartoonPack;
    /// SweetFX Tonemap.fx v1.1: x=Gamma, y=Exposure, z=Saturation, w=Bleach.
    vec4 sweetFxTonemapGammaExpSatBleach;
    /// xyz=FogColor, w=Defog.
    vec4 sweetFxTonemapFogColorDefog;
    /// x=strength (0=skip), yzw=0.
    vec4 sweetFxTonemapStrengthPad;
    /// x=mode (0..6 as float), y=strength, zw=0.
    vec4 sweetFxSplitscreenModeStrength;
    /// x=palette 0..14, y=scanlines 0..2, z=dither, w=strength.
    vec4 sweetFxNostalgiaPack;
    /// x=mode 0..8, y=difference_scale, z=strength, w=0.
    vec4 sweetFxComparePack;
    /// SweetFX Layer.fx v0.2: xy=Layer_Pos, z=Layer_Scale, w=Layer_Blend (0=skip).
    vec4 sweetFxLayerPosScaleBlend;
    /// x=LAYER_SIZE_X, y=LAYER_SIZE_Y, zw unused.
    vec4 sweetFxLayerTexSizePad;
    /// SweetFX FXAA 3.11: x=Subpix, y=EdgeThreshold, z=EdgeThresholdMin, w=Strength.
    vec4 sweetFxFxaaPack;
    /// SweetFX CRT.fx packs.
    vec4 sweetFxCrtPack0;
    vec4 sweetFxCrtPack1;
    vec4 sweetFxCrtPack2;
    vec4 sweetFxCrtPack3;
    vec4 sweetFxAsciiPack0;
    vec4 sweetFxAsciiPack1;
    vec4 sweetFxAsciiPack2;
    vec4 sweetFxAsciiFontColorPad;
    vec4 sweetFxAsciiBackgroundColorPad;
    vec4 sweetFxSmaaPack0;
    vec4 sweetFxSmaaPack1;
    vec4 reshadeDaltonizePack;
    vec4 reshadeDisplayDepthPack;
    vec4 reshadeLutPack;
    /// PD80_04_Technicolor.fx: xyz=Red2strip, w=master strength (0=skip; blends vs identity).
    vec4 pd80TcRedStrPad;
    /// xyz=Cyan2strip.
    vec4 pd80TcCyanPad;
    /// xyz=colorKey, w=Saturation2 (1–2).
    vec4 pd80TcKeySat2Pad;
    /// xyz=3-strip ColorStrength, w=Brightness.
    vec4 pd80Tc3ColBrightPad;
    /// x=3-strip Saturation, y=3-strip Strength, z=enable 3-strip (0/1), w unused.
    vec4 pd80Tc3SatStrEnPad;
    /// PD80_04_Color_Temperature.fx: x=Kelvin, y=Luminance Preservation, z=kMix, w=effect strength (0=skip).
    vec4 pd80ColorTempKelvinLumMixStr;
    /// PD80_04_Saturation_Limit.fx: x=saturation_limit (0–1), y=strength (0=skip), zw=0.
    vec4 pd80SatLimitCapStr;
    /// PD80_04_Color_Balance.fx: xyz=shadow RGB shifts (−1..1).
    vec4 pd80ColorBalanceShadowPad;
    /// xyz=midtone shifts.
    vec4 pd80ColorBalanceMidPad;
    /// xyz=highlight shifts.
    vec4 pd80ColorBalanceHighPad;
    /// x=preserve_luma (0/1), y=separation_mode (0=harsh, 1=smooth), z=strength, w=0.
    vec4 pd80ColorBalanceOptStr;
    /// PD80_04_Color_Isolation.fx: x=hueMid, y=hueRange, z=satLimit, w=fxcolorMix.
    vec4 pd80ColorIsolationHueRangeSatMix;
    /// x=strength (0=skip), yzw=0.
    vec4 pd80ColorIsolationStrPad;
    /// PD80_03_Levels.fx: xyz=black IN levels (color knobs).
    vec4 pd80LevelsIbPad;
    vec4 pd80LevelsIwPad;
    vec4 pd80LevelsObPad;
    vec4 pd80LevelsOwPad;
    /// x=gamma (`ig`), y=enable_dither (0/1), z=dither_strength, w=master strength (0=skip).
    vec4 pd80LevelsGammaDitherStr;
    /// PD80_04_BlacknWhite.fx: x=bw_mode (0–13), y=curve_str, z=enable_dither, w=dither_strength.
    vec4 pd80BwPack0;
    /// xyzw = red/yellow/green/cyan custom weights.
    vec4 pd80BwPack1;
    /// x=blue, y=magenta, z=strength (0=skip), w=show_clip (0/1).
    vec4 pd80BwPack2;
    /// x=use_tint (0/1), y=tinthue, z=tintsat, w unused.
    vec4 pd80BwPack3;
    /// PD80_04_Contrast_Brightness_Saturation.fx: x=enable_dither, y=dither_strength, z=tint, w=exposureN.
    vec4 pd80CbsPack0;
    /// x=contrast, y=brightness, z=saturation, w=vibrance.
    vec4 pd80CbsPack1;
    /// x=huemid, y=huerange, z=sat_custom, w=strength (0=skip).
    vec4 pd80CbsPack2;
    /// x–w = sat_r, sat_y, sat_g, sat_a.
    vec4 pd80CbsPack3;
    /// x–z = sat_b, sat_p, sat_m; w=enable_depth (0/1).
    vec4 pd80CbsPack4;
    /// x=display_depth, y=depthStart, z=depthEnd, w=depthCurve.
    vec4 pd80CbsPack5;
    /// Near/far split: x=exposureD, y=contrastD, z=brightnessD, w=saturationD.
    vec4 pd80CbsPack6;
    /// x=vibranceD; yzw unused.
    vec4 pd80CbsPack7;
    /// PD80_06_Chromatic_Aberration.fx: x=master (0=skip), y=CA_strength, z=CA global width, w=sample steps.
    vec4 pd80CaPack0;
    /// x=CA_type (0–3), y=degrees, z=CA_width, w=CA_curve.
    vec4 pd80CaPack1;
    /// x=oX, y=oY, z=CA_shapeX, w=CA_shapeY.
    vec4 pd80CaPack2;
    /// xyz=vignetteColor, w=show_CA (0/1).
    vec4 pd80CaPack3;
    /// x=enable_depth_int, y=enable_depth_width, z=display_depth, w unused.
    vec4 pd80CaPack4;
    /// xyz=depthStart, depthEnd, depthCurve; w unused.
    vec4 pd80CaPack5;
    /// PD80_05_Sharpening.fx: x=master (0=skip), y=BlurSigma, z=Sharpening, w=Threshold.
    vec4 pd80LsPack0;
    /// x=limiter, y=enable_show_edges, z=enable_depth, w=enable_reverse (0/1).
    vec4 pd80LsPack1;
    /// x=display_depth (0/1), y=depthStart, z=depthEnd, w=depthCurve.
    vec4 pd80LsPack2;
    /// PD80_06_Film_Grain.fx: x=master (0=skip), y=grainAdjust, z=grainSize (1–4), w=grainMotion (0/1).
    vec4 pd80FgPack0;
    /// x=grainOrigColor, y=use_negnoise, z=grainColor, w=grainAmount.
    vec4 pd80FgPack1;
    /// xyzw = grainIntensity, grainDensity, grainIntHigh, grainIntLow.
    vec4 pd80FgPack2;
    /// x=enable_test, y=enable_depth, z=display_depth, w unused.
    vec4 pd80FgPack3;
    /// xyz=depthStart, depthEnd, depthCurve; w unused.
    vec4 pd80FgPack4;
    /// PD80_06_Depth_Slicer.fx: x=master (0=skip), y=depth_near, z=depthpos, w=depth_far.
    vec4 pd80DsPack0;
    /// x=depth_smoothing, y=intensity, z=hue, w=saturation.
    vec4 pd80DsPack1;
    /// x=blendmode (0–20), y=opacity, zw unused.
    vec4 pd80DsPack2;
    /// PD80_01_Color_Gamut.fx: x=master (0=skip), y=colorgamut index (0–15), zw unused.
    vec4 pd80CgPack0;
    /// PD80_03_Color_Space_Curves.fx: x=master, y=enable_dither, z=dither_strength, w=color_space (0–3).
    vec4 pd80CscPack0;
    /// xyzw = pos0_toe, pos1_toe, pos0_shoulder, pos1_shoulder.
    vec4 pd80CscPack1;
    /// x=colorsat, yzw unused.
    vec4 pd80CscPack2;
    /// PD80_03_Shadows_Midtones_Highlights.fx: x=master, y=luma_mode (0–2), z=separation (0–1), w=enable_dither.
    vec4 pd80SmhPack0;
    /// x=dither_strength; yzw unused.
    vec4 pd80SmhPack1;
    /// Shadow: exposure, contrast, brightness, opacity.
    vec4 pd80SmhPack2;
    /// Shadow: blend rgb + blendmode (0–20 as float).
    vec4 pd80SmhPack3;
    /// Shadow: tint, saturation, vibrance, pad.
    vec4 pd80SmhPack4;
    /// Mid: exposure, contrast, brightness, opacity.
    vec4 pd80SmhPack5;
    vec4 pd80SmhPack6;
    vec4 pd80SmhPack7;
    /// Highlight: exposure, contrast, brightness, opacity.
    vec4 pd80SmhPack8;
    vec4 pd80SmhPack9;
    vec4 pd80SmhPack10;
    /// PD80_03_Curved_Levels.fx: x=master, y=enable_dither, z=dither_strength, w=enable_rgb.
    vec4 pd80ClPack0;
    /// Grey: black_in, white_in, black_out, white_out (0–255 as float).
    vec4 pd80ClPack1;
    /// Grey: shoulder0, shoulder1, toe0, toe1.
    vec4 pd80ClPack2;
    vec4 pd80ClPack3;
    vec4 pd80ClPack4;
    vec4 pd80ClPack5;
    vec4 pd80ClPack6;
    vec4 pd80ClPack7;
    vec4 pd80ClPack8;
    /// PD80_04_Selective_Color.fx: x=master, y=corr_method, z=corr_method2, w unused.
    vec4 pd80ScPack0;
    /// Per-range packs: (cya, mag, yel, bla), and (sat, vib, pad, pad).
    vec4 pd80ScPack1;
    vec4 pd80ScPack2;
    vec4 pd80ScPack3;
    vec4 pd80ScPack4;
    vec4 pd80ScPack5;
    vec4 pd80ScPack6;
    vec4 pd80ScPack7;
    vec4 pd80ScPack8;
    vec4 pd80ScPack9;
    vec4 pd80ScPack10;
    vec4 pd80ScPack11;
    vec4 pd80ScPack12;
    vec4 pd80ScPack13;
    vec4 pd80ScPack14;
    vec4 pd80ScPack15;
    vec4 pd80ScPack16;
    vec4 pd80ScPack17;
    vec4 pd80ScPack18;
    vec4 pd80PpPack0;
    vec4 pd80PpPack1;
    vec4 pd80MrPack0;
    vec4 pd80MrPack1;
    vec4 pd80MrPack2;
    vec4 pd80MrPack3;
    vec4 pd80MrPack4;
    vec4 pd80MrPack5;
    vec4 pd80MrPack6;
    vec4 pd80MrPack7;
    vec4 pd80BlpPack0;
    vec4 pd80BlpPack1;
    vec4 pd80BlpPack2;
    vec4 pd80BlpPack3;
    vec4 pd80BlpPack4;
    vec4 pd80BlpPack5;
    vec4 pd80CltPack0;
    vec4 pd80CltPack1;
    vec4 pd80CltPack2;
    vec4 pd80CltPack3;
    vec4 pd80CltPack4;
    vec4 pd80CltPack5;
    vec4 pd80LcPack0;
    vec4 pd80LfPack0;
    vec4 pd80Cg4Pack0;
    vec4 pd80Cg4Pack1;
    vec4 pd80Cg4Pack2;
    vec4 pd80Cg4Pack3;
    vec4 pd80Cg4Pack4;
    vec4 pd80Cg4Pack5;
    vec4 pd80Cg4Pack6;
    vec4 pd80Cg4Pack7;
    vec4 pd80Cg4Pack8;
    vec4 pd80CcPack0;
    vec4 pd80RccPack0;
    vec4 pd80RccPack1;
    vec4 pd80RccPack2;
    vec4 pd80RccPack3;
    vec4 pd80RccPack4;
    vec4 pd80FaPack0;
    vec4 pd80HbPack0;
    vec4 pd80HbPack1;
    vec4 pd80HbPack2;
    vec4 pd80Sc2Pack0;
    /// Colourfulness.fx: x=colourfulness, y=limit luma, zw unused.
    vec4 creatorColourfulnessPack;
    /// FilmicPass.fx: x=strength, y=fade, z=bleach, w=saturation.
    vec4 creatorFilmicPassPack;
    /// FilmGrain2.fx: x=amount, y=color amount, z=luminance amount, w=grain size.
    vec4 creatorFilmGrain2Pack;
    /// Denoise.fx KNN: x=strength, y=noise level, z=lerp coefficient, w=weight threshold.
    vec4 creatorDenoisePack;
    /// Denoise.fx KNN: x=counter threshold, y=gaussian sigma, zw unused.
    vec4 creatorDenoisePack2;
    /// AdaptiveSharpen.fx: x=curve_height, y=curve_slope, z=L_overshoot, w=D_overshoot.
    vec4 creatorAdaptiveSharpenPack0;
    /// AdaptiveSharpen.fx: x=L_compr_low, y=L_compr_high, z=D_compr_low, w=D_compr_high.
    vec4 creatorAdaptiveSharpenPack1;
    /// AdaptiveSharpen.fx: x=scale_lim, y=scale_cs, z=pm_p, w unused.
    vec4 creatorAdaptiveSharpenPack2;
    /// GaussianBlur.fx: x=strength, y=offset, z=radius (0–4), w unused.
    vec4 creatorGaussianBlurPack;
    /// FineSharp.fx: x=sstr, y=cstr, z=xstr, w=xrep.
    vec4 creatorFineSharpPack0;
    /// FineSharp.fx: x=lstr, y=pstr, z=mode (0–2), w unused.
    vec4 creatorFineSharpPack1;
    /// Bloom.fx Marty McFly: x=threshold, y=amount, z=saturation, w=mix mode (0–3).
    vec4 creatorMartyBloomPack0;
    /// Bloom.fx Marty McFly: xyz=tint, w unused.
    vec4 creatorMartyBloomPack1;
    /// DOF.fx RingDOF: x=strength, y=autoFocus, z=manualFocusDepth, w=infiniteFocus.
    vec4 creatorDofPack0;
    /// DOF.fx RingDOF: xy=focusPoint, z=focusRadius, w=focusSamples.
    vec4 creatorDofPack1;
    /// DOF.fx RingDOF: x=nearBlurCurve, y=farBlurCurve, z=blurRadius, w=ringSamples.
    vec4 creatorDofPack2;
    /// DOF.fx RingDOF: x=ringRings, y=ringThreshold, z=ringGain, w=ringFringe.
    vec4 creatorDofPack3;
    /// DOF.fx RingDOF: x=ringBias, yzw unused.
    vec4 creatorDofPack4;
    /// AmbientLight.fx: x=intensity, y=threshold, z=adapt, w=adaptBaseMult.
    vec4 creatorAmbientLightPack0;
    /// AmbientLight.fx: x=adaptBlackLevel, y=dither, z=dirt, w=adaptiveMode.
    vec4 creatorAmbientLightPack1;
    /// AmbientLight.fx: x=dirtInt, y=dirtOvrInt, z=timePhase, w unused.
    vec4 creatorAmbientLightPack2;
    /// FakeMotionBlur.fx: x=recall, y=softness, zw unused.
    vec4 creatorFakeMotionBlurPack0;
    /// ReflectiveBumpMapping.fx: x=strength, y=blurWidthPixels, z=reliefHeight, w=fresnelReflectance.
    vec4 creatorReflectiveBumpMappingPack0;
    /// ReflectiveBumpMapping.fx: x=fresnelMult, y=lowerThreshold, z=upperThreshold, w=sampleCount.
    vec4 creatorReflectiveBumpMappingPack1;
    /// ReflectiveBumpMapping.fx: rgba color masks for red/orange/yellow/green.
    vec4 creatorReflectiveBumpMappingPack2;
    /// ReflectiveBumpMapping.fx: rgb color masks for cyan/blue/magenta, w=depthFarPlane.
    vec4 creatorReflectiveBumpMappingPack3;
    /// Native CropResize: xy=content size pixels, zw=intermediate size pixels.
    vec4 cropScaleContentIntermediate;
    /// Native CropResize: xy=final size pixels, z=filter (0 point/1 linear), w=strength.
    vec4 cropScaleFinalFilterStrength;
    /// Barbatos uFakeHDR: x=preset atlas row (0..2), y=strength (0..2), zw unused.
    vec4 barbatosFakeHdrPack;
    vec4 riAdaptiveDebandPack;
    vec4 riLocalSharpenPack;
    vec4 riOutlinePack0;
    vec4 riOutlineColorMethod;
    vec4 riOutlineWobbleDebug;
    /// Raw Iron signal glitch: x=strength, y=block height px, z=color shift px, w=speed.
    vec4 riSignalGlitchPack;
    /// Raw Iron night vision: x=strength, y=gain, z=noise, w=vignette.
    vec4 riNightVisionPack;
    vec4 riHq4xPack0;
    vec4 riHq4xPack1;
    vec4 riHslAnchor0;
    vec4 riHslAnchor1;
    vec4 riHslAnchor2;
    vec4 riHslAnchor3;
    vec4 riHslAnchor4;
    vec4 riHslAnchor5;
    vec4 riHslAnchor6;
    vec4 riHslAnchor7;
    vec4 riLevelsPlusPack0;
    vec4 riLevelsPlusPack1;
    vec4 riLevelsPlusPack2;
    vec4 riLevelsPlusPack3;
    vec4 riLevelsPlusPack4;
    vec4 riLevelsPlusPack5;
    vec4 riLevelsPlusPack6;
    vec4 riLightDofPack0;
    vec4 riLightDofPack1;
    vec4 riLightDofPack2;
    vec4 riMagicBloomPack0;
    vec4 riMagicBloomPack1;
    vec4 riUiMaskPack0;
    vec4 riUiMaskPack1;
} cameraData;

layout(set = 1, binding = 0) uniform sampler2D hdrSceneLinear;
layout(set = 1, binding = 1) uniform sampler2D sceneDepth;
layout(set = 1, binding = 2) uniform sampler2D fakeMotionBlurHistory;
layout(set = 2, binding = 0) uniform sampler2D sweetFxLayerTexture;
layout(set = 3, binding = 0) uniform sampler2D sweetFxSmaaAreaTexture;
layout(set = 4, binding = 0) uniform sampler2D sweetFxSmaaSearchTexture;
layout(set = 5, binding = 0) uniform sampler2D reshadeLutTexture;
layout(set = 6, binding = 0) uniform sampler2D barbatosLutAtlas;
// Raw Iron-owned native resource bundle. These are linear-data textures, not color inputs.
layout(set = 7, binding = 0) uniform sampler2D nativePd80BlueNoiseTexture;
layout(set = 7, binding = 1) uniform sampler2D nativePd80PermTexture;
layout(set = 7, binding = 2) uniform sampler2D nativePd80CineLutTexture;
layout(set = 7, binding = 3) uniform sampler2D riMagicBloomDirtTexture;
layout(set = 7, binding = 4) uniform sampler2D riUiMaskTexture;
layout(set = 7, binding = 5) uniform sampler2D riFontAtlasTexture;

vec3 TonemapAcesApprox(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 ApplyColorGrade(vec3 color, float contrast, float saturation) {
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 saturated = mix(vec3(luma), color, saturation);
    return (saturated - 0.5) * contrast + 0.5;
}

float Hash11(float p) {
    return fract(sin(p * 91.3458) * 47453.5453);
}

float Hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

/// SweetFX Curves.fx formula 4 (simplified Catmull–Rom / Horner), luma-only blend — matches reference “Contrast_blend”.
float SweetFxSrgbLumaCurve(float x) {
    x = clamp(x, 0.0, 1.0);
    return x * (x * (1.5 - x) + 0.5);
}

vec3 ApplySweetFxLumaCurve(vec3 rgb, float strength) {
    float s = clamp(strength, 0.0, 1.0);
    if (s < 1e-6) {
        return rgb;
    }
    float lum = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
    float curved = SweetFxSrgbLumaCurve(lum);
    float newLum = mix(lum, curved, s);
    return rgb * (newLum / max(lum, 1e-5));
}

/// SweetFX/LiftGammaGain.fx v1.1 — lift (shadows), gain (highlights), then gamma; identity at RGB=(1,1,1).
vec3 ApplySweetFxLiftGammaGain(vec3 color, vec3 rgbLift, vec3 rgbGamma, vec3 rgbGain, float mixAmt) {
    float m = clamp(mixAmt, 0.0, 1.0);
    if (m < 1e-6) {
        return color;
    }
    vec3 lift = rgbLift;
    vec3 gamma = max(rgbGamma, vec3(1e-4));
    vec3 gain = rgbGain;
    vec3 lgg = color * (1.5 - 0.5 * lift) + 0.5 * lift - 0.5;
    lgg = clamp(lgg, 0.0, 1.0);
    lgg *= gain;
    lgg = pow(abs(lgg), 1.0 / gamma);
    lgg = clamp(lgg, 0.0, 1.0);
    return clamp(mix(color, lgg, m), 0.0, 1.0);
}

/// SweetFX/Vibrance.fx — luma-based saturation adjust; coeff matches reference.
vec3 ApplySweetFxVibrance(vec3 color, vec3 vibranceRgbBalance, float vibranceAmt) {
    float va = clamp(vibranceAmt, -1.0, 1.0);
    if (abs(va) < 1e-7) {
        return color;
    }
    const vec3 coefLuma = vec3(0.212656, 0.715158, 0.072186);
    float luma = dot(coefLuma, color);
    float maxColor = max(color.r, max(color.g, color.b));
    float minColor = min(color.r, min(color.g, color.b));
    float colorSaturation = maxColor - minColor;
    vec3 coeffVibrance = vibranceRgbBalance * va;
    vec3 t = 1.0 + coeffVibrance * (1.0 - sign(coeffVibrance) * colorSaturation);
    return clamp(mix(vec3(luma), color, t), 0.0, 1.0);
}

/// SweetFX/Technicolor.fx v1.1 — distinct from Technicolor2 (three-strip emulation).
vec3 ApplySweetFxTechnicolor(
    vec3 tcol, float powerIn, vec3 rgbNegativeAmount, float strength) {
    float s = clamp(strength, 0.0, 1.0);
    if (s < 1e-6) {
        return tcol;
    }
    float power = max(powerIn, 1e-4);
    vec3 rn = max(rgbNegativeAmount, vec3(1e-4));
    const vec3 cyanfilter = vec3(0.0, 1.30, 1.0);
    const vec3 magentafilter = vec3(1.0, 0.0, 1.05);
    const vec3 yellowfilter = vec3(1.6, 1.6, 0.05);
    const vec2 redorangefilter = vec2(1.05, 0.620);
    const vec2 greenfilter = vec2(0.30, 1.0);
    const vec2 magentafilter2 = magentafilter.rb;

    vec2 negative_mul_r = tcol.rg * (1.0 / (rn.r * power));
    vec2 negative_mul_g = tcol.rg * (1.0 / (rn.g * power));
    vec2 negative_mul_b = tcol.rb * (1.0 / (rn.b * power));
    float vr = dot(redorangefilter, negative_mul_r);
    float vg = dot(greenfilter, negative_mul_g);
    float vb = dot(magentafilter2, negative_mul_b);
    vec3 output_r = vec3(vr) + cyanfilter;
    vec3 output_g = vec3(vg) + magentafilter;
    vec3 output_b = vec3(vb) + yellowfilter;
    vec3 outc = clamp(output_r * output_g * output_b, 0.0, 1.0);
    return clamp(mix(tcol, outc, s), 0.0, 1.0);
}

/// SweetFX/Technicolor2.fx v1.0 — Prod80 path (not interchangeable with Technicolor v1).
vec3 Pd80TcQuaternionMulMat3Vec3(vec4 quat, vec3 v) {
    vec3 crossv = quat.yzx * quat.zxy;
    vec3 square = quat.xyz * quat.xyz;
    vec3 wimag = quat.w * quat.xyz;
    square = square + square.yzx;
    vec3 diag = 0.5 - square;
    vec3 a = crossv + wimag;
    vec3 b = crossv - wimag;
    vec3 r0 = 2.0 * vec3(diag.x, b.z, a.y);
    vec3 r1 = 2.0 * vec3(a.z, diag.y, b.x);
    vec3 r2 = 2.0 * vec3(b.y, a.x, diag.z);
    return vec3(dot(r0, v), dot(r1, v), dot(r2, v));
}

float Pd80TcGetLuminance(vec3 x) {
    return dot(x, vec3(0.212656, 0.715158, 0.072186));
}

/// PD80_04_Technicolor.fx — quaternion matrices match HLSL `QuaternionToMatrix` + `mul(M,v)`.
vec3 ApplyPd80Technicolor(vec3 colorIn, vec4 redStr, vec4 cyanPad, vec4 keySat2, vec4 colBright, vec4 satStrEn) {
    float master = clamp(redStr.w, 0.0, 1.0);
    if (master < 1e-6) {
        return colorIn;
    }
    vec3 Red2strip = clamp(redStr.xyz, 0.0, 1.0);
    vec3 Cyan2strip = clamp(cyanPad.xyz, 0.0, 1.0);
    vec3 colorKeyIn = clamp(keySat2.xyz, 0.0, 2.0);
    float Saturation2 = clamp(keySat2.w, 1.0, 2.0);
    vec3 ColorStrength = clamp(colBright.xyz, 0.0, 2.0);
    float Brightness = clamp(colBright.w, 0.5, 1.5);
    float Sat3 = clamp(satStrEn.x, 0.0, 1.5);
    float Str3 = clamp(satStrEn.y, 0.0, 1.0);
    bool enable3 = satStrEn.z > 0.5;

    vec3 color = clamp(colorIn, 0.0, 1.0);
    vec3 orig = color;
    vec3 root3 = vec3(0.57735);
    float negR = 1.0 - color.x;
    float negG = 1.0 - color.y;
    vec3 newR = 1.0 - negR * Cyan2strip;
    vec3 newC = 1.0 - negG * Red2strip;
    float halfAngle = 0.5 * radians(180.0);
    vec4 rotQuat = vec4(root3 * sin(halfAngle), cos(halfAngle));
    vec3 key = Pd80TcQuaternionMulMat3Vec3(rotQuat, colorKeyIn);
    key = max(color.yyy, key);
    color = newR * newC * key;
    float hueAdj = 0.52;
    halfAngle = 0.5 * radians(hueAdj * 360.0);
    rotQuat = vec4(root3 * sin(halfAngle), cos(halfAngle));
    color = Pd80TcQuaternionMulMat3Vec3(rotQuat, color);
    color = mix(vec3(Pd80TcGetLuminance(color)), color, Saturation2);

    if (enable3) {
        vec3 temp = 1.0 - orig;
        vec3 target = temp.grg;
        vec3 target2 = temp.bbr;
        vec3 temp2 = orig * target;
        temp2 *= target2;
        temp = temp2 * ColorStrength;
        temp2 *= Brightness;
        target = temp.yxy;
        target2 = temp.zzx;
        temp = orig - target;
        temp += temp2;
        temp2 = temp - target2;
        color = mix(orig, temp2, Str3);
        color = mix(vec3(Pd80TcGetLuminance(color)), color, Sat3);
    }
    color = clamp(color, 0.0, 1.0);
    return mix(colorIn, color, master);
}

/// PD80_00_Color_Spaces.fxh — RGB/HSL + Kelvin (matches prod80).
vec3 Pd80CtHueToRgb(float H) {
    return clamp(vec3(
        abs(H * 6.0 - 3.0) - 1.0,
        2.0 - abs(H * 6.0 - 2.0),
        2.0 - abs(H * 6.0 - 4.0)), 0.0, 1.0);
}

vec3 Pd80CtRgbToHcv(vec3 RGB) {
    vec4 P = (RGB.g < RGB.b) ? vec4(RGB.b, RGB.g, -1.0, 2.0 / 3.0) : vec4(RGB.g, RGB.b, 0.0, -1.0 / 3.0);
    vec4 Q = (RGB.r < P.x) ? vec4(P.xyw, RGB.r) : vec4(RGB.r, P.yzx);
    float C = Q.x - min(Q.w, Q.y);
    float H = abs((Q.w - Q.y) / (6.0 * C + 0.000001) + Q.z);
    return vec3(H, C, Q.x);
}

vec3 Pd80CtRgbToHsl(vec3 RGB) {
    RGB = max(RGB, vec3(0.000001));
    vec3 HCV = Pd80CtRgbToHcv(RGB);
    float L = HCV.z - HCV.y * 0.5;
    float S = HCV.y / (1.0 - abs(L * 2.0 - 1.0) + 0.000001);
    return vec3(HCV.x, S, L);
}

vec3 Pd80CtHslToRgb(vec3 HSL) {
    vec3 RGB = Pd80CtHueToRgb(HSL.x);
    float C = (1.0 - abs(2.0 * HSL.z - 1.0)) * HSL.y;
    return (RGB - 0.5) * C + HSL.z;
}

vec3 Pd80CtKelvinToRgb(float k) {
    float kelvin = clamp(k, 1000.0, 40000.0) / 100.0;
    vec3 ret;
    if (kelvin <= 66.0) {
        ret.r = 1.0;
        ret.g = clamp(0.39008157876901960784 * log(kelvin) - 0.63184144378862745098, 0.0, 1.0);
    } else {
        float t = max(kelvin - 60.0, 0.0);
        ret.r = clamp(1.29293618606274509804 * pow(t, -0.1332047592), 0.0, 1.0);
        ret.g = clamp(1.12989086089529411765 * pow(t, -0.0755148492), 0.0, 1.0);
    }
    if (kelvin >= 66.0) {
        ret.b = 1.0;
    } else if (kelvin < 19.0) {
        ret.b = 0.0;
    } else {
        ret.b = clamp(0.54320678911019607843 * log(kelvin - 10.0) - 1.19625408914, 0.0, 1.0);
    }
    return ret;
}

/// PD80_04_Color_Temperature.fx — `PS_ColorTemp` order (Kelvin multiply + optional HSL L restore).
vec3 ApplyPd80ColorTemperature(vec3 colorIn, vec4 pack) {
    float strength = clamp(pack.w, 0.0, 1.0);
    if (strength < 1e-6) {
        return colorIn;
    }
    float Kelvin = pack.x;
    float LumPreservation = clamp(pack.y, 0.0, 1.0);
    float kMix = clamp(pack.z, 0.0, 1.0);

    vec3 color = clamp(colorIn, 0.0, 1.0);
    vec3 kColor = Pd80CtKelvinToRgb(Kelvin);
    vec3 oLum = Pd80CtRgbToHsl(color);
    vec3 blended = mix(color, color * kColor, kMix);
    vec3 resHSV = Pd80CtRgbToHsl(blended);
    vec3 resRGB = Pd80CtHslToRgb(vec3(resHSV.xy, oLum.z));
    vec3 outc = mix(blended, resRGB, LumPreservation);
    outc = clamp(outc, 0.0, 1.0);
    return mix(colorIn, outc, strength);
}

/// PD80_04_Saturation_Limit.fx — `PS_Satlimit` (HSL.S capped; prod80 RGB↔HSL).
vec3 ApplyPd80SaturationLimit(vec3 colorIn, vec4 pack) {
    float cap = clamp(pack.x, 0.0, 1.0);
    float strength = clamp(pack.y, 0.0, 1.0);
    if (strength < 1e-6) {
        return colorIn;
    }
    vec3 hsl = Pd80CtRgbToHsl(clamp(colorIn, 0.0, 1.0));
    hsl.y = min(hsl.y, cap);
    vec3 limited = clamp(Pd80CtHslToRgb(hsl), 0.0, 1.0);
    return mix(colorIn, limited, strength);
}

/// PD80_04_Color_Balance.fx — `ColorBalance()` (curve separation + optional ES_RGB / ES_CMY luma preservation).
vec3 Pd80CbSmoothCurve(vec3 x) {
    return x * x * (3.0 - 2.0 * x);
}

vec3 ApplyPd80ColorBalance(
    vec3 colorIn,
    vec4 shadowPad,
    vec4 midPad,
    vec4 highPad,
    vec4 optPack) {
    vec3 shadows = shadowPad.xyz;
    vec3 midtones = midPad.xyz;
    vec3 highlights = highPad.xyz;
    bool preserveLuma = optPack.x > 0.5;
    int separationMode = int(optPack.y + 0.5);
    float strength = clamp(optPack.z, 0.0, 1.0);
    if (strength < 1e-6) {
        return colorIn;
    }

    vec3 c = clamp(colorIn, 0.0, 1.0);
    float luma = dot(c, vec3(0.333333));

    vec3 dist_s;
    vec3 dist_h;
    if (separationMode == 0) {
        dist_s = Pd80CbSmoothCurve(max(vec3(1.0) - c * 2.0, vec3(0.0)));
        dist_h = Pd80CbSmoothCurve(max((c - 0.5) * 2.0, vec3(0.0)));
    } else {
        dist_s = pow(vec3(1.0) - c, vec3(4.0));
        dist_h = pow(c, vec3(4.0));
    }

    const vec3 esRgb = vec3(1.0) - vec3(0.299, 0.587, 0.114);
    const vec3 esCmy = vec3(
        dot(esRgb.yz, vec2(0.5)),
        dot(esRgb.xz, vec2(0.5)),
        dot(esRgb.xy, vec2(0.5)));

    vec3 s_rgb = vec3(1.0);
    vec3 m_rgb = vec3(1.0);
    vec3 h_rgb = vec3(1.0);
    if (preserveLuma) {
        s_rgb = mix(esCmy * abs(shadows), esRgb * shadows, vec3(greaterThan(shadows, vec3(0.0))));
        m_rgb = mix(esCmy * abs(midtones), esRgb * midtones, vec3(greaterThan(midtones, vec3(0.0))));
        h_rgb = mix(esCmy * abs(highlights), esRgb * highlights, vec3(greaterThan(highlights, vec3(0.0))));
    }

    vec3 mids = clamp(vec3(1.0) - dist_s - dist_h, 0.0, 1.0);
    vec3 highs = dist_h * (highlights * h_rgb * (1.0 - luma));
    vec3 newc = c * (dist_s * shadows * s_rgb + mids * midtones * m_rgb) * (1.0 - c) + highs;
    vec3 outc = clamp(c + newc, 0.0, 1.0);
    return mix(colorIn, outc, strength);
}

/// PD80_04_Color_Isolation.fx — `RGBToHSV` from PD80_00_Color_Spaces.fxh (lolengine path).
vec3 Pd80CiRgbToHsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = c.g < c.b ? vec4(c.b, c.g, K.wz) : vec4(c.g, c.b, K.xy);
    vec4 q = c.r < p.x ? vec4(p.xyw, c.r) : vec4(c.r, p.yzx);
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

float Pd80CiSmootherstep(float x) {
    return x * x * x * (x * (x * 6.0 - 15.0) + 10.0);
}

/// PD80_04_Color_Isolation.fx — `PS_ColorIso`.
vec3 ApplyPd80ColorIsolation(vec3 colorIn, vec4 isoPack, vec4 strPack) {
    float strength = clamp(strPack.x, 0.0, 1.0);
    if (strength < 1e-6) {
        return colorIn;
    }
    float hueMid = clamp(isoPack.x, 0.0, 1.0);
    float hueRange = max(isoPack.y, 1.0e-5);
    float satLimit = clamp(isoPack.z, 0.0, 1.0);
    float fxMix = clamp(isoPack.w, 0.0, 1.0);

    vec3 color = clamp(colorIn, 0.0, 1.0);
    float grey = dot(color, vec3(0.212656, 0.715158, 0.072186));
    float hue = Pd80CiRgbToHsv(color).x;
    float r = 1.0 / hueRange;
    vec3 w = max(vec3(1.0) - abs(vec3(hue - hueMid, hue + 1.0 - hueMid, hue - 1.0 - hueMid)) * r, vec3(0.0));
    float weight = w.x + w.y + w.z;
    vec3 newc = mix(vec3(grey), color, Pd80CiSmootherstep(weight) * satLimit);
    vec3 outc = mix(color, newc, fxMix);
    return mix(colorIn, outc, strength);
}

/// PD80_04_Contrast_Brightness_Saturation.fx — `PD80_00_Base_Effects.fxh` + selective sat + depth (`sceneDepth`).
float Pd80CbsGetAvgColor(vec3 col) {
    return dot(col, vec3(0.333333, 0.333334, 0.333333));
}

vec3 Pd80CbsClipColor(vec3 color) {
    float lum = Pd80CbsGetAvgColor(color);
    float mincol = min(min(color.x, color.y), color.z);
    float maxcol = max(max(color.x, color.y), color.z);
    color = (mincol < 0.0) ? lum + ((color - vec3(lum)) * lum) / (lum - mincol) : color;
    mincol = min(min(color.x, color.y), color.z);
    maxcol = max(max(color.x, color.y), color.z);
    color = (maxcol > 1.0) ? lum + ((color - vec3(lum)) * (1.0 - lum)) / (maxcol - lum) : color;
    return color;
}

vec3 Pd80CbsBlendLuma(vec3 base, vec3 blend) {
    float lumbase = Pd80CbsGetAvgColor(base);
    float lumblend = Pd80CbsGetAvgColor(blend);
    float ldiff = lumblend - lumbase;
    return Pd80CbsClipColor(base + vec3(ldiff));
}

vec3 Pd80CbsSoftLight(vec3 c, vec3 b) {
    vec3 low = 2.0 * c * b + c * c * (1.0 - 2.0 * b);
    vec3 high = sqrt(c) * (2.0 * b - 1.0) + 2.0 * c * (1.0 - b);
    return mix(low, high, step(vec3(0.5), b));
}

vec3 Pd80CbsExposure(vec3 res, float x) {
    x = (x < 0.0) ? x * 0.333 : x;
    return clamp(res * (x * (1.0 - res) + 1.0), 0.0, 1.0);
}

vec3 Pd80CbsCon(vec3 res, float x) {
    vec3 c = Pd80CbsSoftLight(res, res);
    x = (x < 0.0) ? x * 0.5 : x;
    return clamp(mix(res, c, x), 0.0, 1.0);
}

vec3 Pd80CbsBri(vec3 res, float x) {
    vec3 c = vec3(1.0) - (vec3(1.0) - res) * (vec3(1.0) - res);
    x = (x < 0.0) ? x * 0.5 : x;
    return clamp(mix(res, c, x), 0.0, 1.0);
}

vec3 Pd80CbsSat(vec3 res, float x) {
    float lum = Pd80TcGetLuminance(res);
    return clamp(mix(vec3(lum), res, x + 1.0), 0.0, 1.0);
}

vec3 Pd80CbsVib(vec3 res, float x) {
    float mn = min(min(res.x, res.y), res.z);
    float mx = max(max(res.x, res.y), res.z);
    float satSpan = mx - mn;
    float lum = Pd80TcGetLuminance(res);
    return clamp(mix(vec3(lum), res, 1.0 + (x * (1.0 - satSpan))), 0.0, 1.0);
}

float Pd80CbsSmoothCurve(float x) {
    return x * x * (3.0 - 2.0 * x);
}

vec3 Pd80CbsChannelSat(
    vec3 col,
    float r,
    float y,
    float g,
    float a,
    float b,
    float p,
    float m,
    float hue) {
    float desat = Pd80TcGetLuminance(col);
    float weight_r = Pd80CbsSmoothCurve(max(1.0 - abs(hue * 6.0), 0.0))
        + Pd80CbsSmoothCurve(max(1.0 - abs((hue - 1.0) * 6.0), 0.0));
    float weight_y = Pd80CbsSmoothCurve(max(1.0 - abs((hue - 0.166667) * 6.0), 0.0));
    float weight_g = Pd80CbsSmoothCurve(max(1.0 - abs((hue - 0.333333) * 6.0), 0.0));
    float weight_a = Pd80CbsSmoothCurve(max(1.0 - abs((hue - 0.5) * 6.0), 0.0));
    float weight_b = Pd80CbsSmoothCurve(max(1.0 - abs((hue - 0.666667) * 6.0), 0.0));
    float weight_p = Pd80CbsSmoothCurve(max(1.0 - abs((hue - 0.75) * 6.0), 0.0));
    float weight_m = Pd80CbsSmoothCurve(max(1.0 - abs((hue - 0.833333) * 6.0), 0.0));
    col = mix(vec3(desat), col, clamp(1.0 + r * weight_r, 0.0, 2.0));
    col = mix(vec3(desat), col, clamp(1.0 + y * weight_y, 0.0, 2.0));
    col = mix(vec3(desat), col, clamp(1.0 + g * weight_g, 0.0, 2.0));
    col = mix(vec3(desat), col, clamp(1.0 + a * weight_a, 0.0, 2.0));
    col = mix(vec3(desat), col, clamp(1.0 + b * weight_b, 0.0, 2.0));
    col = mix(vec3(desat), col, clamp(1.0 + p * weight_p, 0.0, 2.0));
    col = mix(vec3(desat), col, clamp(1.0 + m * weight_m, 0.0, 2.0));
    return clamp(col, 0.0, 1.0);
}

vec3 Pd80CbsCustomSat(vec3 col, float h, float range, float sat, float hue) {
    float desat = Pd80TcGetLuminance(col);
    float rcpR = 1.0 / max(range, 1e-5);
    vec3 w = vec3(
        max(1.0 - abs((hue - h) * rcpR), 0.0),
        max(1.0 - abs((hue + 1.0 - h) * rcpR), 0.0),
        max(1.0 - abs((hue - 1.0 - h) * rcpR), 0.0));
    float weight = Pd80CbsSmoothCurve(dot(w, vec3(1.0))) * sat;
    return clamp(mix(vec3(desat), col, clamp(1.0 + weight, 0.0, 2.0)), 0.0, 1.0);
}

vec3 ApplyPd80ContrastBriSat(vec3 colorIn, vec2 sampleUv) {
    vec4 p0 = cameraData.pd80CbsPack0;
    vec4 p1 = cameraData.pd80CbsPack1;
    vec4 p2 = cameraData.pd80CbsPack2;
    vec4 p3 = cameraData.pd80CbsPack3;
    vec4 p4 = cameraData.pd80CbsPack4;
    vec4 p5 = cameraData.pd80CbsPack5;
    vec4 p6 = cameraData.pd80CbsPack6;
    vec4 p7 = cameraData.pd80CbsPack7;
    float strength = clamp(p2.w, 0.0, 1.0);
    if (strength < 1e-6) {
        return colorIn;
    }

    vec3 color = clamp(colorIn, 0.0, 1.0);
    if (p0.x > 0.5) {
        vec2 dUv = sampleUv * cameraData.viewportMetrics.xy / 512.0;
        float mot = fract(cameraData.postProcessSecondary.z * 12.9898 + 93.0);
        vec3 dn = vec3(
            Hash21(dUv + vec2(4.2, 1.1)),
            Hash21(dUv + vec2(19.4, 7.7)),
            Hash21(dUv + vec2(3.8, 22.2)));
        dn = fract(dn + 0.61803398875 * mot * 128.0);
        dn = (dn * 2.0 - 1.0) * 0.5;
        dn *= (p0.y / 255.0);
        color = clamp(color + dn, 0.0, 1.0);
    }

    float depthLin = texture(sceneDepth, sampleUv).r;
    float dSmooth = smoothstep(p5.y, p5.z, depthLin);
    dSmooth = pow(clamp(dSmooth, 0.0, 1.0), p5.w);
    float dn2 = fract(sin(dot(sampleUv * 4096.0 + vec2(3.1, 9.7), vec2(12.9898, 78.233))) * 43758.5453);
    dSmooth = clamp(dSmooth + (dn2 - 0.5) * 0.002, 0.0, 1.0);

    vec3 cold = vec3(0.0, 0.365, 1.0);
    vec3 warm = vec3(0.98, 0.588, 0.0);
    float tAmt = p0.z;
    color = (tAmt < 0.0)
        ? mix(color, Pd80CbsBlendLuma(cold, color), abs(tAmt))
        : mix(color, Pd80CbsBlendLuma(warm, color), tAmt);

    vec3 dcolor = color;
    color = Pd80CbsExposure(color, p0.w);
    color = Pd80CbsCon(color, p1.x);
    color = Pd80CbsBri(color, p1.y);
    color = Pd80CbsSat(color, p1.z);
    color = Pd80CbsVib(color, p1.w);

    dcolor = Pd80CbsExposure(dcolor, p6.x);
    dcolor = Pd80CbsCon(dcolor, p6.y);
    dcolor = Pd80CbsBri(dcolor, p6.z);
    dcolor = Pd80CbsSat(dcolor, p6.w);
    dcolor = Pd80CbsVib(dcolor, p7.x);

    color = mix(color, dcolor, p4.w * dSmooth);

    float chue = Pd80CtRgbToHsl(color).x;
    color = Pd80CbsChannelSat(color, p3.x, p3.y, p3.z, p3.w, p4.x, p4.y, p4.z, chue);
    color = Pd80CbsCustomSat(color, p2.x, p2.y, p2.z, chue);

    if (p5.x > 0.5) {
        color = vec3(depthLin);
    }

    return mix(colorIn, color, strength);
}

/// PD80_03_Levels.fx — `levels()` plus native blue-noise dithering.
vec3 Pd80LevelsTransfer(vec3 color, vec3 blackIn, vec3 whiteIn, float gamma, vec3 blackOut, vec3 whiteOut) {
    vec3 ret = clamp(color - blackIn, 0.0, 1.0) / max(whiteIn - blackIn, vec3(0.000001));
    ret = pow(max(ret, vec3(0.0)), vec3(gamma));
    ret = ret * clamp(whiteOut - blackOut, 0.0, 1.0) + blackOut;
    return ret;
}

vec3 ApplyPd80Levels(
    vec3 colorIn,
    vec2 sampleUv,
    vec4 ibPad,
    vec4 iwPad,
    vec4 obPad,
    vec4 owPad,
    vec4 gammaDitherStr) {
    float gamma = clamp(gammaDitherStr.x, 0.05, 10.0);
    bool useDither = gammaDitherStr.y > 0.5;
    float ditherStrength = max(gammaDitherStr.z, 0.0);
    float strength = clamp(gammaDitherStr.w, 0.0, 1.0);
    if (strength < 1e-6) {
        return colorIn;
    }

    vec3 ib = ibPad.xyz;
    vec3 iw = iwPad.xyz;
    vec3 ob = obPad.xyz;
    vec3 ow = owPad.xyz;

    vec3 color = clamp(colorIn, 0.0, 1.0);
    vec4 dnoise = vec4(0.0);
    if (useDither) {
        vec2 dUv = sampleUv * cameraData.viewportMetrics.xy / 512.0;
        float mot = fract(cameraData.postProcessSecondary.z * 12.9898);
        vec4 raw = vec4(
            Hash21(dUv + vec2(0.11, 0.23)),
            Hash21(dUv + vec2(17.1, 42.3)),
            Hash21(dUv + vec2(91.7, 3.14)),
            Hash21(dUv + vec2(2.71, 88.8)));
        dnoise = fract(raw + 0.61803398875 * mot * 128.0);
        dnoise = (dnoise * 2.0 - 1.0) * 0.5;
        dnoise *= (ditherStrength / 255.0);
    }

    color = clamp(color + dnoise.w, 0.0, 1.0);
    vec3 outc = Pd80LevelsTransfer(
        color,
        clamp(ib + dnoise.xyz, 0.0, 1.0),
        clamp(iw + dnoise.yzx, 0.0, 1.0),
        gamma,
        clamp(ob + dnoise.zxy, 0.0, 1.0),
        clamp(ow + dnoise.wxz, 0.0, 1.0));
    return mix(colorIn, outc, strength);
}

/// PD80_03_Curved_Levels.fx — ishiyama hyperbolic curve + in/out points (optional per-channel branch).
struct Pd80ClTonemapParams {
    vec3 mToe;
    vec2 mMid;
    vec3 mShoulder;
    vec2 mBx;
};

vec3 Pd80ClTonemap(Pd80ClTonemapParams tc, vec3 x) {
    vec3 toe = -tc.mToe.x / (x + tc.mToe.y) + tc.mToe.z;
    vec3 mid = tc.mMid.x * x + tc.mMid.y;
    vec3 shoulder = -tc.mShoulder.x / (x + tc.mShoulder.y) + tc.mShoulder.z;
    vec3 result = mix(toe, mid, step(vec3(tc.mBx.x), x));
    result = mix(result, shoulder, step(vec3(tc.mBx.y), x));
    return result;
}

vec3 Pd80ClBlackWhiteIn(vec3 c, float b, float w) {
    return clamp(c - vec3(b), 0.0, 1.0) / max(vec3(w - b), vec3(0.000001));
}

float Pd80ClBlackWhiteIn(float c, float b, float w) {
    return clamp(c - b, 0.0, 1.0) / max(w - b, 0.000001);
}

vec3 Pd80ClBlackWhiteOut(vec3 c, float b, float w) {
    return c * clamp(w - b, 0.0, 1.0) + vec3(b);
}

float Pd80ClBlackWhiteOut(float c, float b, float w) {
    return c * clamp(w - b, 0.0, 1.0) + b;
}

vec4 Pd80ClSetBoundaries(float tx, float ty, float sx, float sy) {
    if (tx > sx) {
        tx = sx;
    }
    if (ty > sy) {
        ty = sy;
    }
    return vec4(tx, ty, sx, sy);
}

Pd80ClTonemapParams Pd80ClPrepareTonemap(vec2 p1, vec2 p2, vec2 p3) {
    Pd80ClTonemapParams tc;
    float denom = p2.x - p1.x;
    denom = abs(denom) > 1e-5 ? denom : 1e-5;
    float slope = (p2.y - p1.y) / denom;
    tc.mMid.x = slope;
    tc.mMid.y = p1.y - slope * p1.x;
    {
        float denom2 = p1.y - slope * p1.x;
        denom2 = abs(denom2) > 1e-5 ? denom2 : 1e-5;
        tc.mToe.x = slope * p1.x * p1.x * p1.y * p1.y / (denom2 * denom2);
        tc.mToe.y = slope * p1.x * p1.x / denom2;
        tc.mToe.z = p1.y * p1.y / denom2;
    }
    {
        float denom2 = slope * (p2.x - p3.x) - p2.y + p3.y;
        denom2 = abs(denom2) > 1e-5 ? denom2 : 1e-5;
        tc.mShoulder.x = slope * pow(p2.x - p3.x, 2.0) * pow(p2.y - p3.y, 2.0) / (denom2 * denom2);
        tc.mShoulder.y = (slope * p2.x * (p3.x - p2.x) + p3.x * (p2.y - p3.y)) / denom2;
        tc.mShoulder.z = (-p2.y * p2.y + p3.y * (slope * (p2.x - p3.x) + p2.y)) / denom2;
    }
    tc.mBx = vec2(p1.x, p2.x);
    return tc;
}

vec4 Pd80ClRgbNoiseDither(vec2 sampleUv, float enableDither, float strength, float timeSec) {
    if (enableDither < 0.5) {
        return vec4(0.0);
    }
    vec2 dUv = sampleUv * cameraData.viewportMetrics.xy / 512.0;
    float mot = fract(timeSec * 12.9898 + 11.0);
    vec4 raw = vec4(
        Hash21(dUv + vec2(0.41, 0.13)),
        Hash21(dUv + vec2(7.11, 2.31)),
        Hash21(dUv + vec2(3.17, 9.01)),
        Hash21(dUv + vec2(8.27, 4.67)));
    vec4 dn = fract(raw + 0.61803398875 * mot * 128.0);
    dn = (dn * 2.0 - 1.0) * 0.5;
    return dn * (strength / 255.0);
}

vec3 ApplyPd80CurvedLevels(vec2 sampleUv, vec3 colorIn) {
    vec4 pk0 = cameraData.pd80ClPack0;
    float master = clamp(pk0.x, 0.0, 1.0);
    if (master < 1e-6) {
        return colorIn;
    }
    bool enableDither = pk0.y > 0.5;
    float ditherStrength = clamp(pk0.z, 0.0, 10.0);
    bool enableRgb = pk0.w > 0.5;

    float timer = cameraData.postProcessSecondary.z;
    vec4 dnoise = Pd80ClRgbNoiseDither(sampleUv, enableDither ? 1.0 : 0.0, ditherStrength, timer);
    vec3 color = clamp(colorIn, 0.0, 1.0);
    color = clamp(color + dnoise.yzx, 0.0, 1.0);

    Pd80ClTonemapParams tc;
    vec4 gInOut = cameraData.pd80ClPack1;
    vec4 gCurve = cameraData.pd80ClPack2;
    float bigr = clamp(gInOut.x / 255.0 + dnoise.w, 0.0, 1.0);
    float wigr = clamp(gInOut.y / 255.0 + dnoise.w, 0.0, 1.0);
    float bogr = clamp(gInOut.z / 255.0 + dnoise.w, 0.0, 1.0);
    float wogr = clamp(gInOut.w / 255.0 + dnoise.w, 0.0, 1.0);

    vec4 grey = Pd80ClSetBoundaries(gCurve.z, gCurve.w, gCurve.x, gCurve.y);
    tc = Pd80ClPrepareTonemap(grey.xy, grey.zw, vec2(1.0, 1.0));
    vec3 outc = Pd80ClBlackWhiteIn(color, bigr, wigr);
    outc = Pd80ClTonemap(tc, outc);
    outc = Pd80ClBlackWhiteOut(outc, bogr, wogr);

    if (enableRgb) {
        vec4 rInOut = cameraData.pd80ClPack3;
        vec4 rCurve = cameraData.pd80ClPack4;
        float bir = clamp(rInOut.x / 255.0 + dnoise.x, 0.0, 1.0);
        float wir = clamp(rInOut.y / 255.0 + dnoise.x, 0.0, 1.0);
        float bor = clamp(rInOut.z / 255.0 + dnoise.x, 0.0, 1.0);
        float wor = clamp(rInOut.w / 255.0 + dnoise.x, 0.0, 1.0);
        vec4 red = Pd80ClSetBoundaries(rCurve.z, rCurve.w, rCurve.x, rCurve.y);
        tc = Pd80ClPrepareTonemap(red.xy, red.zw, vec2(1.0, 1.0));
        float rx = Pd80ClBlackWhiteIn(outc.x, bir, wir);
        rx = Pd80ClTonemap(tc, vec3(rx)).x;
        outc.x = Pd80ClBlackWhiteOut(rx, bor, wor);

        vec4 ggInOut = cameraData.pd80ClPack5;
        vec4 ggCurve = cameraData.pd80ClPack6;
        float big = clamp(ggInOut.x / 255.0 + dnoise.y, 0.0, 1.0);
        float wig = clamp(ggInOut.y / 255.0 + dnoise.y, 0.0, 1.0);
        float bog = clamp(ggInOut.z / 255.0 + dnoise.y, 0.0, 1.0);
        float wog = clamp(ggInOut.w / 255.0 + dnoise.y, 0.0, 1.0);
        vec4 green = Pd80ClSetBoundaries(ggCurve.z, ggCurve.w, ggCurve.x, ggCurve.y);
        tc = Pd80ClPrepareTonemap(green.xy, green.zw, vec2(1.0, 1.0));
        float gx = Pd80ClBlackWhiteIn(outc.y, big, wig);
        gx = Pd80ClTonemap(tc, vec3(gx)).y;
        outc.y = Pd80ClBlackWhiteOut(gx, bog, wog);

        vec4 bInOut = cameraData.pd80ClPack7;
        vec4 bCurve = cameraData.pd80ClPack8;
        float bib = clamp(bInOut.x / 255.0 + dnoise.z, 0.0, 1.0);
        float wib = clamp(bInOut.y / 255.0 + dnoise.z, 0.0, 1.0);
        float bob = clamp(bInOut.z / 255.0 + dnoise.z, 0.0, 1.0);
        float wob = clamp(bInOut.w / 255.0 + dnoise.z, 0.0, 1.0);
        vec4 blue = Pd80ClSetBoundaries(bCurve.z, bCurve.w, bCurve.x, bCurve.y);
        tc = Pd80ClPrepareTonemap(blue.xy, blue.zw, vec2(1.0, 1.0));
        float bx = Pd80ClBlackWhiteIn(outc.z, bib, wib);
        bx = Pd80ClTonemap(tc, vec3(bx)).z;
        outc.z = Pd80ClBlackWhiteOut(bx, bob, wob);
    }

    return mix(colorIn, clamp(outc, 0.0, 1.0), master);
}

/// PD80_04_Selective_Color.fx — Photoshop selective-color style CMYK shifts (absolute/relative) + sat/vib per region.
float Pd80ScMid3(vec3 c) {
    float sum = c.x + c.y + c.z;
    float mn = min(min(c.x, c.y), c.z);
    float mx = max(max(c.x, c.y), c.z);
    return sum - mn - mx;
}

float Pd80ScAdjustColor(float scale, float colorvalue, float adjust, float bk, int method) {
    return clamp(
               (((-1.0 - adjust) * bk - adjust) * (1.0 - colorvalue * float(method))),
               -colorvalue,
               1.0 - colorvalue)
        * scale;
}

vec4 Pd80CscDither(vec2 uv, bool en, float str, float timeSec);

vec3 ApplyPd80SelectiveColor(vec3 colorIn) {
    float master = clamp(cameraData.pd80ScPack0.x, 0.0, 1.0);
    if (master < 1e-6) {
        return colorIn;
    }
    int corr_method = clamp(int(cameraData.pd80ScPack0.y + 0.5), 0, 1);
    int corr_method2 = clamp(int(cameraData.pd80ScPack0.z + 0.5), 0, 1);

    vec3 color = clamp(colorIn, 0.0, 1.0);
    vec3 orig = color;

    float min_value = min(min(color.x, color.y), color.z);
    float max_value = max(max(color.x, color.y), color.z);
    float mid_value = Pd80ScMid3(color);

    float sRGB = max_value - mid_value;
    float sCMY = mid_value - min_value;
    float sNeutrals = 1.0 - (abs(max_value - 0.5) + abs(min_value - 0.5));
    float sWhites = (min_value - 0.5) * 2.0;
    float sBlacks = (0.5 - max_value) * 2.0;

    float r_d_m = orig.x - orig.z;
    float r_d_y = orig.x - orig.y;
    float y_d = mid_value - orig.z;
    float g_d_y = orig.y - orig.x;
    float g_d_c = orig.y - orig.z;
    float c_d = mid_value - orig.x;
    float b_d_c = orig.z - orig.y;
    float b_d_m = orig.z - orig.x;
    float m_d = mid_value - orig.y;

    float r_delta = 1.0;
    float y_delta = 1.0;
    float g_delta = 1.0;
    float c_delta = 1.0;
    float b_delta = 1.0;
    float m_delta = 1.0;
    if (corr_method2 != 0) {
        r_delta = min(r_d_m, r_d_y);
        y_delta = y_d;
        g_delta = min(g_d_y, g_d_c);
        c_delta = c_d;
        b_delta = min(b_d_c, b_d_m);
        m_delta = m_d;
    }

    vec4 r0 = cameraData.pd80ScPack1;
    vec4 r1 = cameraData.pd80ScPack2;
    vec4 y0 = cameraData.pd80ScPack3;
    vec4 y1 = cameraData.pd80ScPack4;
    vec4 g0 = cameraData.pd80ScPack5;
    vec4 g1 = cameraData.pd80ScPack6;
    vec4 c0 = cameraData.pd80ScPack7;
    vec4 c1 = cameraData.pd80ScPack8;
    vec4 b0 = cameraData.pd80ScPack9;
    vec4 b1 = cameraData.pd80ScPack10;
    vec4 m0 = cameraData.pd80ScPack11;
    vec4 m1 = cameraData.pd80ScPack12;
    vec4 w0 = cameraData.pd80ScPack13;
    vec4 w1 = cameraData.pd80ScPack14;
    vec4 n0 = cameraData.pd80ScPack15;
    vec4 n1 = cameraData.pd80ScPack16;
    vec4 bk0 = cameraData.pd80ScPack17;
    vec4 bk1 = cameraData.pd80ScPack18;

    if (max_value == orig.x) {
        color.x += Pd80ScAdjustColor(sRGB, color.x, r0.x, r0.w, corr_method);
        color.y += Pd80ScAdjustColor(sRGB, color.y, r0.y, r0.w, corr_method);
        color.z += Pd80ScAdjustColor(sRGB, color.z, r0.z, r0.w, corr_method);
        color = Pd80CbsSat(color, r1.x * r_delta);
        color = Pd80CbsVib(color, r1.y * r_delta);
    }
    if (min_value == orig.z) {
        color.x += Pd80ScAdjustColor(sCMY, color.x, y0.x, y0.w, corr_method);
        color.y += Pd80ScAdjustColor(sCMY, color.y, y0.y, y0.w, corr_method);
        color.z += Pd80ScAdjustColor(sCMY, color.z, y0.z, y0.w, corr_method);
        color = Pd80CbsSat(color, y1.x * y_delta);
        color = Pd80CbsVib(color, y1.y * y_delta);
    }
    if (max_value == orig.y) {
        color.x += Pd80ScAdjustColor(sRGB, color.x, g0.x, g0.w, corr_method);
        color.y += Pd80ScAdjustColor(sRGB, color.y, g0.y, g0.w, corr_method);
        color.z += Pd80ScAdjustColor(sRGB, color.z, g0.z, g0.w, corr_method);
        color = Pd80CbsSat(color, g1.x * g_delta);
        color = Pd80CbsVib(color, g1.y * g_delta);
    }
    if (min_value == orig.x) {
        color.x += Pd80ScAdjustColor(sCMY, color.x, c0.x, c0.w, corr_method);
        color.y += Pd80ScAdjustColor(sCMY, color.y, c0.y, c0.w, corr_method);
        color.z += Pd80ScAdjustColor(sCMY, color.z, c0.z, c0.w, corr_method);
        color = Pd80CbsSat(color, c1.x * c_delta);
        color = Pd80CbsVib(color, c1.y * c_delta);
    }
    if (max_value == orig.z) {
        color.x += Pd80ScAdjustColor(sRGB, color.x, b0.x, b0.w, corr_method);
        color.y += Pd80ScAdjustColor(sRGB, color.y, b0.y, b0.w, corr_method);
        color.z += Pd80ScAdjustColor(sRGB, color.z, b0.z, b0.w, corr_method);
        color = Pd80CbsSat(color, b1.x * b_delta);
        color = Pd80CbsVib(color, b1.y * b_delta);
    }
    if (min_value == orig.y) {
        color.x += Pd80ScAdjustColor(sCMY, color.x, m0.x, m0.w, corr_method);
        color.y += Pd80ScAdjustColor(sCMY, color.y, m0.y, m0.w, corr_method);
        color.z += Pd80ScAdjustColor(sCMY, color.z, m0.z, m0.w, corr_method);
        color = Pd80CbsSat(color, m1.x * m_delta);
        color = Pd80CbsVib(color, m1.y * m_delta);
    }
    if (min_value >= 0.5) {
        color.x += Pd80ScAdjustColor(sWhites, color.x, w0.x, w0.w, corr_method);
        color.y += Pd80ScAdjustColor(sWhites, color.y, w0.y, w0.w, corr_method);
        color.z += Pd80ScAdjustColor(sWhites, color.z, w0.z, w0.w, corr_method);
        float wSmooth = smoothstep(0.5, 1.0, min_value);
        color = Pd80CbsSat(color, w1.x * wSmooth);
        color = Pd80CbsVib(color, w1.y * wSmooth);
    }
    if (max_value > 0.0 && min_value < 1.0) {
        color.x += Pd80ScAdjustColor(sNeutrals, color.x, n0.x, n0.w, corr_method);
        color.y += Pd80ScAdjustColor(sNeutrals, color.y, n0.y, n0.w, corr_method);
        color.z += Pd80ScAdjustColor(sNeutrals, color.z, n0.z, n0.w, corr_method);
        color = Pd80CbsSat(color, n1.x);
        color = Pd80CbsVib(color, n1.y);
    }
    if (max_value < 0.5) {
        color.x += Pd80ScAdjustColor(sBlacks, color.x, bk0.x, bk0.w, corr_method);
        color.y += Pd80ScAdjustColor(sBlacks, color.y, bk0.y, bk0.w, corr_method);
        color.z += Pd80ScAdjustColor(sBlacks, color.z, bk0.z, bk0.w, corr_method);
        float bSmooth = smoothstep(0.5, 0.0, max_value);
        color = Pd80CbsSat(color, bk1.x * bSmooth);
        color = Pd80CbsVib(color, bk1.y * bSmooth);
    }

    return mix(colorIn, clamp(color, 0.0, 1.0), master);
}

/// PD80_01A_RT_Correct_Contrast.fx — local min/max normalization with optional black/white point correction.
vec3 ApplyPd80RtCorrectContrast(vec2 sampleUv, vec3 colorIn) {
    vec4 p0 = cameraData.pd80CcPack0;
    float master = clamp(p0.x, 0.0, 1.0);
    if (master <= 0.0) {
        return colorIn;
    }
    float enableWp = p0.y >= 0.5 ? 1.0 : 0.0;
    float wpStr = clamp(p0.z, 0.0, 1.0);
    float bpMix = clamp(p0.w, 0.0, 1.0);

    vec2 px = cameraData.viewportMetrics.zw;
    vec3 localMin = vec3(1.0);
    vec3 localMax = vec3(0.0);
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 uv = clamp(sampleUv + vec2(float(x), float(y)) * px, vec2(0.0), vec2(1.0));
            vec3 s = clamp(texture(hdrSceneLinear, uv).rgb, 0.0, 1.0);
            localMin = min(localMin, s);
            localMax = max(localMax, s);
        }
    }

    vec3 color = clamp(colorIn, 0.0, 1.0);
    vec3 denom = max(localMax - localMin, vec3(1e-4));
    vec3 corrected = clamp((color - localMin) / denom, 0.0, 1.0);
    vec3 wpOnly = clamp(color / max(localMax, vec3(1e-4)), 0.0, 1.0);
    corrected = mix(corrected, wpOnly, enableWp * wpStr);
    corrected = mix(color, corrected, bpMix);
    return mix(colorIn, corrected, master);
}

/// PD80_01B_RT_Correct_Color.fx — adaptive black/mid/white tint removal.
vec3 ApplyPd80RtCorrectColor(vec2 sampleUv, vec3 colorIn) {
    vec4 p0 = cameraData.pd80RccPack0;
    float master = clamp(p0.x, 0.0, 1.0);
    if (master <= 0.0) {
        return colorIn;
    }
    bool enableDither = p0.y >= 0.5;
    float ditherStrength = clamp(p0.z, 0.0, 10.0);
    bool enableWp = p0.w >= 0.5;
    vec4 p1 = cameraData.pd80RccPack1;
    vec4 p2 = cameraData.pd80RccPack2;
    vec4 p3 = cameraData.pd80RccPack3;
    vec4 p4 = cameraData.pd80RccPack4;

    bool wpRespectLuma = p1.x >= 0.5;
    int wpMethod = clamp(int(p1.y + 0.5), 0, 1);
    float wpStrength = clamp(p1.z, 0.0, 1.0);
    float wpLumaStr = clamp(p1.w, 0.0, 1.0);
    bool enableBp = p2.x >= 0.5;
    bool bpRespectLuma = p2.y >= 0.5;
    int bpMethod = clamp(int(p2.z + 0.5), 0, 1);
    float bpStrength = clamp(p2.w, 0.0, 1.0);
    float bpLumaStr = clamp(p3.x, 0.0, 1.0);
    bool enableMid = p3.y >= 0.5;
    bool midRespectLuma = p3.z >= 0.5;
    bool midUseAltMethod = p3.w >= 0.5;
    float midScale = clamp(p4.x, 0.0, 5.0);

    vec2 px = cameraData.viewportMetrics.zw;
    vec3 minPerChannel = vec3(1.0);
    vec3 maxPerChannel = vec3(0.0);
    vec3 darkestColor = vec3(1.0);
    vec3 lightestColor = vec3(0.0);
    vec3 midAccum = vec3(0.0);
    float midCount = 0.0;

    float bestDarkScore = 1e9;
    float bestLightScore = -1e9;
    float midRef = 0.5;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 uv = clamp(sampleUv + vec2(float(x), float(y)) * px, vec2(0.0), vec2(1.0));
            vec3 s = clamp(texture(hdrSceneLinear, uv).rgb, 0.0, 1.0);
            minPerChannel = min(minPerChannel, s);
            maxPerChannel = max(maxPerChannel, s);
            float darkScore = max(max(s.r, s.g), s.b) + dot(s, vec3(1.0));
            if (darkScore < bestDarkScore) {
                bestDarkScore = darkScore;
                darkestColor = s;
            }
            float lightScore = dot(s, vec3(1.0));
            if (lightScore > bestLightScore) {
                bestLightScore = lightScore;
                lightestColor = s;
            }
            midAccum += s;
            midCount += 1.0;
        }
    }

    vec3 minValue = bpMethod == 1 ? darkestColor : minPerChannel;
    vec3 maxValue = wpMethod == 1 ? lightestColor : maxPerChannel;
    vec3 midValue = midAccum / max(midCount, 1.0);
    if (midUseAltMethod) {
        midRef = dot(minValue + maxValue, vec3(0.1666667));
    }
    maxValue = max(maxValue, minValue + vec3(1e-4));

    vec3 color = clamp(colorIn, 0.0, 1.0);
    vec4 dnoise = Pd80CscDither(sampleUv, enableDither, ditherStrength, 0.0);
    color = clamp(color + dnoise.xyz, 0.0, 1.0);

    minValue = mix(vec3(0.0), minValue, enableBp ? bpStrength : 0.0);
    maxValue = mix(vec3(1.0), maxValue, enableWp ? wpStrength : 0.0);
    midValue = (midValue - vec3(midRef)) * (enableMid ? midScale : 0.0);

    color = clamp((color - minValue) / max(maxValue - minValue, vec3(1e-4)), 0.0, 1.0);
    float avgMax = dot(maxValue, vec3(0.333333));
    if (wpRespectLuma) {
        color = mix(color, color * avgMax, wpLumaStr);
    }
    float avgMin = dot(minValue, vec3(0.333333));
    if (bpRespectLuma) {
        color = mix(color, color * (1.0 - avgMin) + avgMin, bpLumaStr);
    }
    float avgCol = dot(color, vec3(0.333333));
    float avgMid = dot(midValue, vec3(0.333333));
    avgCol = 1.0 - abs(avgCol * 2.0 - 1.0);
    float midLumaGate = midRespectLuma ? 1.0 : 0.0;
    color = clamp(color - midValue * avgCol + vec3(avgMid * avgCol * midLumaGate), 0.0, 1.0);
    return mix(colorIn, color, master);
}

vec3 Pd80FaFilmic(vec3 c, float A, float B, float C, float D, float E, float F, float whitePoint) {
    vec3 num = ((c * (A * c + C * B) + D * E) / (c * (A * c + B) + D * F)) - E / F;
    float denom = ((whitePoint * (A * whitePoint + C * B) + D * E) / (whitePoint * (A * whitePoint + B) + D * F)) - E / F;
    return num / max(denom, 1e-4);
}

/// PD80_03_Filmic_Adaptation.fx — filmic curve with scene-luminance driven toe increase.
vec3 ApplyPd80FilmicAdaptation(vec2 sampleUv, vec3 colorIn) {
    vec4 p0 = cameraData.pd80FaPack0;
    float master = clamp(p0.x, 0.0, 1.0);
    if (master <= 0.0) {
        return colorIn;
    }
    float adjShoulder = clamp(p0.y, 1.0, 5.0);
    float adjLinear = clamp(p0.z, 1.0, 10.0);
    float adjToe = clamp(p0.w, 1.0, 5.0);

    vec2 px = cameraData.viewportMetrics.zw;
    float logLumaAccum = 0.0;
    float sampleCount = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 uv = clamp(sampleUv + vec2(float(x), float(y)) * px, vec2(0.0), vec2(1.0));
            vec3 s = clamp(texture(hdrSceneLinear, uv).rgb, 0.0, 1.0);
            float luma = max(dot(s, vec3(0.212656, 0.715158, 0.072186)), 0.06);
            logLumaAccum += log2(luma);
            sampleCount += 1.0;
        }
    }
    float avgLuma = exp2(logLumaAccum / max(sampleCount, 1.0));

    float A = 0.65 * adjShoulder;
    float B = 0.085 * adjLinear;
    float C = 1.83;
    float D = 0.55 * adjToe;
    float E = 0.05;
    float F = 0.57;
    float W = 1.0;
    float toeExp = mix(1.0, 8.0, clamp(avgLuma, 0.0, 1.0));
    float toe = max(D * toeExp, D);
    vec3 mapped = clamp(Pd80FaFilmic(clamp(colorIn, 0.0, 1.0), A, B, C, toe, E, F, W), 0.0, 1.0);
    return mix(colorIn, mapped, master);
}

/// PD80_02_Bloom.fx — threshold/exposure bloom with gaussian taps and screen blend.
vec3 ApplyPd80HqBloom(vec2 sampleUv, vec2 px, vec3 colorIn) {
    vec4 p0 = cameraData.pd80HbPack0;
    vec4 p1 = cameraData.pd80HbPack1;
    vec4 p2 = cameraData.pd80HbPack2;
    float master = clamp(p0.x, 0.0, 1.0);
    if (master <= 0.0) return colorIn;
    bool debugBloom = p0.y >= 0.5;
    float ditherStrength = clamp(p0.z, 0.0, 10.0);
    float bloomMix = clamp(p0.w, 0.0, 1.0);
    float threshold = clamp(p1.x, 0.0, 1.0);
    float grey = clamp(p1.y, 0.000001, 1.0);
    float exposureBias = clamp(p1.z, -1.0, 5.0);
    float sigma = clamp(p1.w, 10.0, 300.0);
    float sat = clamp(p2.x, 0.0, 2.0);

    float lumaCenter = max(dot(colorIn, vec3(0.212656, 0.715158, 0.072186)), threshold);
    vec3 accum = vec3(0.0);
    float wsum = 0.0;
    float radius = clamp(sigma / 18.0, 1.0, 8.0);
    for (int y = -4; y <= 4; ++y) {
        for (int x = -4; x <= 4; ++x) {
            vec2 ofs = vec2(float(x), float(y));
            if (length(ofs) > radius) continue;
            vec2 uv = clamp(sampleUv + ofs * px, vec2(0.0), vec2(1.0));
            vec3 s = clamp(texture(hdrSceneLinear, uv).rgb, 0.0, 1.0);
            float l = max(dot(s, vec3(0.212656, 0.715158, 0.072186)), threshold);
            vec3 thr = clamp((s - lumaCenter) / max(1.0 - lumaCenter, 1e-4), 0.0, 1.0);
            float expMul = exp2(log2(grey / max(l, 1e-6)) + exposureBias);
            thr *= expMul;
            float w = exp(-dot(ofs, ofs) / max(2.0 * radius * radius, 1e-4));
            accum += thr * w;
            wsum += w;
        }
    }
    vec3 bloom = accum / max(wsum, 1e-5);
    vec4 dnoise = Pd80CscDither(sampleUv, true, ditherStrength, 0.0);
    vec3 steps = smoothstep(vec3(0.0), vec3(0.012), bloom);
    bloom = clamp(bloom + dnoise.xyz * steps, 0.0, 1.0);
    bloom = Pd80CbsVib(bloom, sat);
    vec3 screened = 1.0 - (1.0 - clamp(colorIn, 0.0, 1.0)) * (1.0 - bloom);
    vec3 outColor = debugBloom ? bloom : mix(colorIn, screened, bloomMix);
    return mix(colorIn, clamp(outColor, 0.0, 1.0), master);
}

const float kCreatorMartyGaussWeights[11] = float[11](
    0.082607, 0.080977, 0.076276, 0.069041, 0.060049,
    0.050187, 0.040306, 0.031105, 0.023066, 0.016436, 0.011254);

vec3 EvaluatePreMartyBloomSource(vec2 sampleUv, vec2 px);

vec4 CreatorMartyBloomPass0(vec2 uv, vec2 px, float threshold) {
    vec4 bloom = vec4(0.0);
    const vec2 offsets[4] = vec2[4](
        vec2(1.0, 1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0), vec2(-1.0, -1.0));
    for (int i = 0; i < 4; ++i) {
        vec2 bloomuv = uv + offsets[i] * px * 2.0;
        vec3 src = EvaluatePreMartyBloomSource(bloomuv, px);
        vec4 tempbloom = vec4(src, 0.0);
        tempbloom.w = max(0.0, dot(tempbloom.xyz, vec3(0.333333)) - threshold);
        tempbloom.xyz = max(vec3(0.0), tempbloom.xyz - vec3(threshold));
        bloom += tempbloom;
    }
    return bloom * 0.25;
}

vec4 CreatorMartyBloomPass1(vec2 uv, vec2 px) {
    vec4 bloom = vec4(0.0);
    const vec2 offsets[8] = vec2[8](
        vec2(1.0, 1.0), vec2(0.0, -1.0), vec2(-1.0, 1.0), vec2(-1.0, -1.0),
        vec2(0.0, 1.0), vec2(0.0, -1.0), vec2(1.0, 0.0), vec2(-1.0, 0.0));
    float threshold = cameraData.creatorMartyBloomPack0.x;
    for (int i = 0; i < 8; ++i) {
        bloom += CreatorMartyBloomPass0(uv + offsets[i] * px * 4.0, px, threshold);
    }
    return bloom * 0.125;
}

vec4 CreatorMartyBloomPass2(vec2 uv, vec2 px) {
    vec4 bloom = vec4(0.0);
    const vec2 offsets[8] = vec2[8](
        vec2(0.707, 0.707), vec2(0.707, -0.707), vec2(-0.707, 0.707), vec2(-0.707, -0.707),
        vec2(0.0, 1.0), vec2(0.0, -1.0), vec2(1.0, 0.0), vec2(-1.0, 0.0));
    for (int i = 0; i < 8; ++i) {
        bloom += CreatorMartyBloomPass1(uv + offsets[i] * px * 8.0, px);
    }
    return bloom * 0.5;
}

vec4 CreatorMartyGaussBlurPass2(vec2 coord, vec2 px, float mult, bool isBlurVert) {
    vec4 sum = vec4(0.0);
    vec2 axis = isBlurVert ? vec2(0.0, 1.0) : vec2(1.0, 0.0);
    for (int i = -10; i <= 10; ++i) {
        float currweight = kCreatorMartyGaussWeights[abs(i)];
        vec2 sampleCoord = coord + axis * float(i) * px * mult;
        sum += CreatorMartyBloomPass2(sampleCoord, px) * currweight;
    }
    return sum;
}

vec4 CreatorMartyBloomPass3(vec2 uv, vec2 px, float bloomAmount) {
    vec4 bloom = CreatorMartyGaussBlurPass2(uv, px, 16.0, false);
    bloom.xyz *= bloomAmount;
    return bloom;
}

vec4 CreatorMartyGaussBlurPass3(vec2 coord, vec2 px, float mult, bool isBlurVert) {
    vec4 sum = vec4(0.0);
    vec2 axis = isBlurVert ? vec2(0.0, 1.0) : vec2(1.0, 0.0);
    float bloomAmount = cameraData.creatorMartyBloomPack0.y;
    for (int i = -10; i <= 10; ++i) {
        float currweight = kCreatorMartyGaussWeights[abs(i)];
        vec2 sampleCoord = coord + axis * float(i) * px * mult;
        sum += CreatorMartyBloomPass3(sampleCoord, px, bloomAmount) * currweight;
    }
    return sum;
}

vec4 CreatorMartyBloomPass4(vec2 uv, vec2 px) {
    vec4 bloom;
    bloom.xyz = CreatorMartyGaussBlurPass3(uv, px, 16.0, true).xyz * 2.5;
    bloom.w = 0.0;
    return bloom;
}

/// Bloom.fx Marty McFly — fused pyramid bloom + LightingCombine bloom section.
vec3 ApplyCreatorMartyBloom(vec2 sampleUv, vec2 px, vec3 colorIn) {
    vec4 pack0 = cameraData.creatorMartyBloomPack0;
    float threshold = pack0.x;
    float bloomAmount = pack0.y;
    if (bloomAmount <= 1e-6) {
        return colorIn;
    }
    float saturation = pack0.z;
    int mixMode = clamp(int(pack0.w + 0.5), 0, 3);
    vec3 tint = cameraData.creatorMartyBloomPack1.xyz;

    vec3 colorbloom = CreatorMartyBloomPass2(sampleUv, px).rgb
        + CreatorMartyBloomPass4(sampleUv, px).rgb * 9.0;
    colorbloom *= 0.1;
    colorbloom = clamp(colorbloom, 0.0, 1.0);
    float colorbloomgray = dot(colorbloom, vec3(0.333333));
    colorbloom = mix(vec3(colorbloomgray), colorbloom, saturation);
    colorbloom *= tint;

    vec3 color = colorIn;
    if (mixMode == 0) {
        color += colorbloom;
    } else if (mixMode == 1) {
        color = 1.0 - (1.0 - color) * (1.0 - colorbloom);
    } else if (mixMode == 2) {
        color = max(
            vec3(0.0),
            max(color, mix(color, (1.0 - (1.0 - clamp(colorbloom, 0.0, 1.0)) * (1.0 - clamp(colorbloom, 0.0, 1.0))), 1.0)));
    } else {
        color = max(color, colorbloom);
    }
    return clamp(color, 0.0, 1.0);
}

vec3 Pd80DsHsvToRgb(vec3 c);

/// PD80_04_Selective_Color_v2.fx — v2 path layered atop base selective-color output.
vec3 ApplyPd80SelectiveColorV2(vec3 colorIn) {
    vec4 p0 = cameraData.pd80Sc2Pack0;
    float master = clamp(p0.x, 0.0, 1.0);
    if (master <= 0.0) return colorIn;
    int corrMethod = clamp(int(p0.y + 0.5), 0, 1);
    float satScale = clamp(p0.z, 0.0, 2.0);
    float ligScale = clamp(p0.w, 0.0, 2.0);

    vec3 base = ApplyPd80SelectiveColor(colorIn);
    vec3 col = clamp(base, 0.0, 1.0);
    float mn = min(min(col.r, col.g), col.b);
    float mx = max(max(col.r, col.g), col.b);
    float md = Pd80ScMid3(col);
    float scalar = max(mx - mn, 1e-4);
    float altScalar = max((md - mn) * 0.5, 0.0);
    float h = Pd80CiRgbToHsv(col).x;
    float oWeight = smoothstep(0.04, 0.16, h) * (1.0 - smoothstep(0.16, 0.28, h));
    float ygWeight = smoothstep(0.20, 0.33, h) * (1.0 - smoothstep(0.33, 0.45, h));
    float gcWeight = smoothstep(0.37, 0.50, h) * (1.0 - smoothstep(0.50, 0.62, h));
    float cbWeight = smoothstep(0.54, 0.66, h) * (1.0 - smoothstep(0.66, 0.79, h));
    float bmWeight = smoothstep(0.70, 0.83, h) * (1.0 - smoothstep(0.83, 0.95, h));
    float mrWeight = smoothstep(0.87, 0.98, h) + smoothstep(0.0, 0.05, h) * 0.5;
    float bk = mn;
    col.r += Pd80ScAdjustColor(altScalar * oWeight, col.r, cameraData.pd80ScPack3.x * 0.5, bk, corrMethod);
    col.g += Pd80ScAdjustColor(altScalar * oWeight, col.g, cameraData.pd80ScPack3.y * 0.5, bk, corrMethod);
    col.b += Pd80ScAdjustColor(altScalar * oWeight, col.b, cameraData.pd80ScPack3.z * 0.5, bk, corrMethod);
    col += vec3(Pd80ScAdjustColor(altScalar * ygWeight, dot(col, vec3(0.333333)), cameraData.pd80ScPack5.x * 0.35, bk, corrMethod));
    col += vec3(Pd80ScAdjustColor(altScalar * gcWeight, dot(col, vec3(0.333333)), cameraData.pd80ScPack7.x * 0.35, bk, corrMethod));
    col += vec3(Pd80ScAdjustColor(altScalar * cbWeight, dot(col, vec3(0.333333)), cameraData.pd80ScPack9.x * 0.35, bk, corrMethod));
    col += vec3(Pd80ScAdjustColor(altScalar * bmWeight, dot(col, vec3(0.333333)), cameraData.pd80ScPack11.x * 0.35, bk, corrMethod));
    col += vec3(Pd80ScAdjustColor(altScalar * mrWeight, dot(col, vec3(0.333333)), cameraData.pd80ScPack1.x * 0.35, bk, corrMethod));
    col = Pd80CbsSat(clamp(col, 0.0, 1.0), (scalar - 0.5) * satScale);
    vec3 hsv = Pd80CiRgbToHsv(clamp(col, 0.0, 1.0));
    hsv.z = clamp(pow(hsv.z, 1.0 / max(ligScale, 1e-4)), 0.0, 1.0);
    col = Pd80DsHsvToRgb(hsv);
    return mix(colorIn, clamp(col, 0.0, 1.0), master);
}

/// PD80_06_Posterize_Pixelate.fx — quantized levels + cell border darkening + optional dither.
vec3 ApplyPd80PosterizePixelate(vec2 sampleUv, vec3 colorIn) {
    vec4 p0 = cameraData.pd80PpPack0;
    vec4 p1 = cameraData.pd80PpPack1;
    float master = clamp(p0.x, 0.0, 1.0);
    if (master <= 0.0) {
        return colorIn;
    }

    float levels = clamp(round(p0.y), 2.0, 255.0);
    float pixelSize = clamp(round(p0.z), 1.0, 9.0);
    float borderStrength = clamp(p0.w, 0.0, 1.0);
    float enableDither = p1.x >= 0.5 ? 1.0 : 0.0;
    float ditherMotion = p1.y >= 0.5 ? 1.0 : 0.0;
    float ditherStrength = clamp(p1.z, 0.0, 10.0);

    vec2 cells = max(cameraData.viewportMetrics.xy / exp2(pixelSize - 1.0), vec2(1.0));
    vec2 scaledUv = sampleUv * cells;
    vec2 fracUv = fract(scaledUv);
    vec2 snappedUv = floor(scaledUv) / cells;

    float q = max(levels - 1.0, 1.0);
    vec3 color = floor(clamp(colorIn, 0.0, 1.0) * q + 0.5) / q;

    float borderMask = step(fracUv.x, 0.008) + step(fracUv.y, 0.008);
    float borderFade = 1.0 - clamp(borderMask, 0.0, 1.0) * borderStrength;
    color *= borderFade;

    if (enableDither > 0.5) {
        float motion = ditherMotion > 0.5 ? 0.6180339 : 0.0;
        float n = Hash21(snappedUv * cameraData.viewportMetrics.xy + vec2(motion, motion * 1.6180339)) - 0.5;
        color += vec3(n) * (ditherStrength / 255.0);
    }

    return mix(colorIn, clamp(color, 0.0, 1.0), master);
}

vec3 Pd80DsHsvToRgb(vec3 c);
vec3 Pd80DsBlendmode(vec3 c, vec3 b, int mode, float o);
vec4 Pd80CscDither(vec2 uv, bool en, float str, float timeSec);

/// PD80_04_Magical_Rectangle.fx — rotated rectangle/circle mask, optional depth fade, region grading + blend mode.
vec3 ApplyPd80MagicalRectangle(vec2 sampleUv, vec3 colorIn) {
    vec4 p0 = cameraData.pd80MrPack0;
    vec4 p1 = cameraData.pd80MrPack1;
    vec4 p2 = cameraData.pd80MrPack2;
    vec4 p3 = cameraData.pd80MrPack3;
    vec4 p4 = cameraData.pd80MrPack4;
    vec4 p5 = cameraData.pd80MrPack5;
    vec3 recColor = clamp(cameraData.pd80MrPack6.xyz, 0.0, 1.0);

    int shape = clamp(int(p0.x + 0.5), 0, 1);
    bool invertShape = p0.y >= 0.5;
    float rotation = radians(clamp(p0.z, 0.0, 360.0));
    vec2 center = vec2(clamp(p0.w, 0.0, 1.0), clamp(p1.x, 0.0, 1.0));
    float sizeX = clamp(p1.y, 0.0, 0.5);
    float sizeY = clamp(p1.z, 0.0, 0.5);
    float depthPos = clamp(p1.w, 0.0, 1.0);
    float smoothing = clamp(p2.x, 0.0, 1.0);
    float depthSmoothing = clamp(p2.y, 0.0, 1.0);
    float ditherStrength = clamp(p2.z, 0.0, 10.0);

    vec3 color = clamp(colorIn, 0.0, 1.0);
    float depth = texture(sceneDepth, sampleUv).r;
    float hasDepth = step(1.0e-5, depth) * step(depth, 0.99999);

    vec2 uv = sampleUv - center;
    uv.y /= max(cameraData.viewportMetrics.x * cameraData.viewportMetrics.w, 1.0e-6);
    float sn = sin(rotation);
    float cs = cos(rotation);
    uv = vec2(cs * uv.x - sn * uv.y, sn * uv.x + cs * uv.y);
    uv = uv / max(vec2(sizeX + sizeX * smoothing, sizeY + sizeY * smoothing), vec2(1.0e-4));
    if (shape == 1) {
        float r = length(uv);
        uv = vec2(r, r);
    }
    vec2 suv = (uv + 1.0) * 0.5;
    vec2 bl = smoothstep(vec2(0.0), vec2(smoothing), suv);
    vec2 tr = smoothstep(vec2(0.0), vec2(smoothing), vec2(1.0) - suv);
    if (p4.y >= 0.5) {
        if (p4.z >= 0.5) {
            bl *= pow(abs(suv.yx), vec2(clamp(p4.w, 0.001, 2.0)));
        }
        tr *= pow(abs(vec2(1.0) - suv.xy), vec2(clamp(p4.w, 0.001, 2.0)));
    }
    float depthFade = smoothstep(depthPos - depthSmoothing, depthPos + depthSmoothing, depth);
    depthFade = mix(1.0, depthFade, hasDepth);
    float mask = bl.x * bl.y * tr.x * tr.y * depthFade;
    if (invertShape) {
        mask = 1.0 - mask;
    }

    if (ditherStrength > 0.0) {
        float n = Hash21(sampleUv * cameraData.viewportMetrics.xy) - 0.5;
        color += vec3(n) * (ditherStrength / 255.0);
    }
    color = Pd80CbsCon(color, p3.x * mask);
    color = Pd80CbsBri(color, p3.y * mask);

    vec3 hsl = Pd80CtRgbToHsl(color);
    hsl.x = fract(hsl.x + ((p3.z + 1.0) * 0.5 - 0.5) * mask);
    color = Pd80CtHslToRgb(hsl);
    color = Pd80CbsSat(color, p3.w * mask);
    color = Pd80CbsVib(color, p4.x * mask);

    float intensity = Pd80CiRgbToHsv(recColor).z;
    vec3 layer = clamp(color * (1.0 - mask) + vec3(mask * intensity), 0.0, 1.0);
    vec3 hsvLayer = Pd80CiRgbToHsv(clamp(layer * max(p5.x, 1.0), 0.0, 1.0));
    vec2 hs = Pd80CiRgbToHsv(recColor).xy;
    vec3 blendColor = clamp(Pd80DsHsvToRgb(vec3(hs, hsvLayer.z)), 0.0, 1.0);
    int blendMode = clamp(int(p5.y + 0.5), 0, 20);
    float opacity = clamp(p5.z, 0.0, 1.0) * clamp(mask, 0.0, 1.0);
    return clamp(Pd80DsBlendmode(color, blendColor, blendMode, opacity), 0.0, 1.0);
}

/// PD80_04_BlacknWhite.fx — iq `curve`, `ProcessBW`, tint (`use_tint`), clipping overlay; dither ≈ RGB noise.
float Pd80BwCurveX(float x, float k) {
    float s = sign(x - 0.5);
    float o = (1.0 + s) / 2.0;
    return o - 0.5 * s * pow(max(2.0 * (o - s * x), 0.0), k);
}

vec4 Pd80BwRgbNoiseDither(vec2 sampleUv, float enableDither, float ditherStrength) {
    if (enableDither < 0.5) {
        return vec4(0.0);
    }
    vec2 dUv = sampleUv * cameraData.viewportMetrics.xy / 512.0;
    float mot = fract(cameraData.postProcessSecondary.z * 12.9898 + 41.0);
    vec4 raw = vec4(
        Hash21(dUv + vec2(2.1, 4.3)),
        Hash21(dUv + vec2(9.2, 1.7)),
        Hash21(dUv + vec2(6.6, 8.8)),
        Hash21(dUv + vec2(3.3, 5.5)));
    vec4 dnoise = fract(raw + 0.61803398875 * mot * 128.0);
    dnoise = (dnoise * 2.0 - 1.0) * 0.5;
    dnoise *= (ditherStrength / 255.0);
    return dnoise;
}

void Pd80BwGetPresetWeights(int mode, vec4 custRygC, vec2 custBm, out float wr, out float wy, out float wg, out float wc, out float wb, out float wm) {
    if (mode >= 13) {
        wr = custRygC.x;
        wy = custRygC.y;
        wg = custRygC.z;
        wc = custRygC.w;
        wb = custBm.x;
        wm = custBm.y;
        return;
    }
    switch (mode) {
        case 0:
            wr = 0.2;
            wy = 0.5;
            wg = -0.2;
            wc = -0.6;
            wb = -1.0;
            wm = -0.2;
            break;
        case 1:
            wr = -0.5;
            wy = 0.5;
            wg = 1.2;
            wc = -0.2;
            wb = -1.0;
            wm = -0.5;
            break;
        case 2:
            wr = -0.2;
            wy = 0.4;
            wg = -0.6;
            wc = 0.5;
            wb = 1.0;
            wm = -0.2;
            break;
        case 3:
            wr = 0.5;
            wy = 1.2;
            wg = -0.5;
            wc = -1.0;
            wb = -1.5;
            wm = -1.0;
            break;
        case 4:
            wr = -1.0;
            wy = 1.0;
            wg = 1.2;
            wc = -0.2;
            wb = -1.5;
            wm = -1.0;
            break;
        case 5:
            wr = -0.7;
            wy = 0.4;
            wg = -1.2;
            wc = 0.7;
            wb = 1.2;
            wm = -0.2;
            break;
        case 6:
            wr = -1.35;
            wy = 2.35;
            wg = 1.35;
            wc = -1.35;
            wb = -1.6;
            wm = -1.07;
            break;
        case 7:
            wr = -1.0;
            wy = -1.0;
            wg = -1.0;
            wc = -1.0;
            wb = -1.0;
            wm = -1.0;
            break;
        case 8:
            wr = 1.0;
            wy = 1.0;
            wg = 1.0;
            wc = 1.0;
            wb = 1.0;
            wm = 1.0;
            break;
        case 9:
            wr = -0.7;
            wy = 0.9;
            wg = 0.6;
            wc = 0.1;
            wb = -0.4;
            wm = -0.4;
            break;
        case 10:
            wr = 0.2;
            wy = 0.4;
            wg = 0.6;
            wc = 0.0;
            wb = -0.6;
            wm = -0.2;
            break;
        case 11:
            wr = -0.3;
            wy = 1.0;
            wg = -0.3;
            wc = -0.6;
            wb = -1.0;
            wm = -0.6;
            break;
        case 12:
            wr = -0.3;
            wy = 2.6;
            wg = -0.3;
            wc = -1.2;
            wb = -0.6;
            wm = -0.4;
            break;
        default:
            wr = custRygC.x;
            wy = custRygC.y;
            wg = custRygC.z;
            wc = custRygC.w;
            wb = custBm.x;
            wm = custBm.y;
            break;
    }
}

vec3 Pd80BwProcess(vec3 col, float wr, float wy, float wg, float wc, float wb, float wm, float curveStr) {
    vec3 hsl = Pd80CtRgbToHsl(col);
    float lum = 1.0 - hsl.z;
    float weight_r = Pd80BwCurveX(max(1.0 - abs(hsl.x * 6.0), 0.0), curveStr)
        + Pd80BwCurveX(max(1.0 - abs((hsl.x - 1.0) * 6.0), 0.0), curveStr);
    float weight_y = Pd80BwCurveX(max(1.0 - abs((hsl.x - 0.166667) * 6.0), 0.0), curveStr);
    float weight_g = Pd80BwCurveX(max(1.0 - abs((hsl.x - 0.333333) * 6.0), 0.0), curveStr);
    float weight_c = Pd80BwCurveX(max(1.0 - abs((hsl.x - 0.5) * 6.0), 0.0), curveStr);
    float weight_b = Pd80BwCurveX(max(1.0 - abs((hsl.x - 0.666667) * 6.0), 0.0), curveStr);
    float weight_m = Pd80BwCurveX(max(1.0 - abs((hsl.x - 0.833333) * 6.0), 0.0), curveStr);
    float sat = hsl.y * (1.0 - hsl.y) + hsl.y;
    float ret = hsl.z;
    ret += hsl.z * (weight_r * wr) * sat * lum;
    ret += hsl.z * (weight_y * wy) * sat * lum;
    ret += hsl.z * (weight_g * wg) * sat * lum;
    ret += hsl.z * (weight_c * wc) * sat * lum;
    ret += hsl.z * (weight_b * wb) * sat * lum;
    ret += hsl.z * (weight_m * wm) * sat * lum;
    float v = clamp(ret, 0.0, 1.0);
    return vec3(v);
}

vec3 ApplyPd80BlackWhite(vec3 colorIn, vec2 sampleUv, vec4 p0, vec4 p1, vec4 p2, vec4 p3) {
    float strength = clamp(p2.z, 0.0, 1.0);
    if (strength < 1e-6) {
        return colorIn;
    }
    int mode = clamp(int(p0.x + 0.5), 0, 13);
    float curveStr = clamp(p0.y, 1.0, 4.0);
    vec4 dnoise = Pd80BwRgbNoiseDither(sampleUv, p0.z, p0.w);

    vec3 color = clamp(colorIn, 0.0, 1.0);
    color = clamp(color + dnoise.zyx, 0.0, 1.0);

    float wr;
    float wy;
    float wg;
    float wc;
    float wb;
    float wm;
    Pd80BwGetPresetWeights(mode, p1, p2.xy, wr, wy, wg, wc, wb, wm);
    color = Pd80BwProcess(color, wr, wy, wg, wc, wb, wm, curveStr);

    color = mix(color, Pd80CtHslToRgb(vec3(p3.y, p3.z, color.x)), p3.x);

    if (p2.w > 0.5) {
        float hClip = 0.98;
        float lClip = 0.01;
        float mn = min(min(color.x, color.y), color.z);
        float mx = max(max(color.x, color.y), color.z);
        color = mn >= hClip ? mix(color, vec3(1.0, 0.0, 0.0), smoothstep(hClip, 1.0, mn)) : color;
        color = mx <= lClip ? mix(vec3(0.0, 0.0, 1.0), color, smoothstep(0.0, lClip, mx)) : color;
    }

    color = clamp(color + dnoise.xyz, 0.0, 1.0);
    return mix(colorIn, color, strength);
}

vec3 ApplySweetFxTechnicolor2(vec3 colorIn,
    vec3 colorStrength,
    float brightnessIn,
    float saturationIn,
    float strengthIn) {
    vec3 color = clamp(colorIn, 0.0, 1.0);
    vec3 temp = 1.0 - color;
    vec3 target = vec3(temp.g, temp.r, temp.g);
    vec3 target2 = vec3(temp.b, temp.b, temp.r);
    vec3 temp2 = color * target;
    temp2 *= target2;

    temp = temp2 * colorStrength;
    temp2 *= brightnessIn;

    target = vec3(temp.g, temp.r, temp.g);
    target2 = vec3(temp.b, temp.b, temp.r);

    temp = color - target;
    temp += temp2;
    temp2 = temp - target2;

    color = mix(color, temp2, clamp(strengthIn, 0.0, 1.0));
    float lum = dot(color, vec3(0.33333333));
    color = mix(vec3(lum), color, saturationIn);
    return clamp(color, 0.0, 1.0);
}

/// SweetFX/Sepia.fx — technique `Tint` (`lerp(col, col * Tint * 2.55, Strength)`); not scene tint.
vec3 ApplySweetFxSepiaTint(vec3 c, vec3 tint, float strength) {
    float s = clamp(strength, 0.0, 1.0);
    if (s < 1e-6) {
        return c;
    }
    return clamp(mix(c, c * tint * 2.55, s), 0.0, 1.0);
}

/// SweetFX/Monochrome.fx v1.1 — preset table matches reference; saturation 1 = full color (pass-through).
vec3 ApplySweetFxMonochrome(vec3 c, float presetF, vec3 custom, float colorSat) {
    float sat = clamp(colorSat, 0.0, 1.0);
    if (sat >= 0.99999) {
        return c;
    }
    int p = clamp(int(floor(presetF + 0.5)), 0, 17);
    vec3 coeff;
    if (p == 0) {
        coeff = custom;
    } else {
        const vec3 kMonoFixed[17] = vec3[](
            vec3(0.21, 0.72, 0.07),
            vec3(0.3333333, 0.3333334, 0.3333333),
            vec3(0.18, 0.41, 0.41),
            vec3(0.25, 0.39, 0.36),
            vec3(0.21, 0.40, 0.39),
            vec3(0.20, 0.41, 0.39),
            vec3(0.21, 0.42, 0.37),
            vec3(0.22, 0.42, 0.36),
            vec3(0.31, 0.36, 0.33),
            vec3(0.28, 0.41, 0.31),
            vec3(0.23, 0.37, 0.40),
            vec3(0.33, 0.36, 0.31),
            vec3(0.36, 0.31, 0.33),
            vec3(0.21, 0.42, 0.37),
            vec3(0.24, 0.37, 0.39),
            vec3(0.27, 0.36, 0.37),
            vec3(0.25, 0.35, 0.40));
        coeff = kMonoFixed[p - 1];
    }
    vec3 grey = vec3(dot(coeff, c));
    return clamp(mix(grey, c, sat), 0.0, 1.0);
}

/// SweetFX/DPX.fx (Loadus) — row-vector * matrix matches `mul(XYZ/RGB, float3)` in reference.
vec3 ApplySweetFxDpx(vec3 inputColor,
    vec3 rgbCurve,
    vec3 rgbC,
    float contrastAmt,
    float saturationAmt,
    float colorfulnessAmt,
    float strengthAmt) {
    float s = clamp(strengthAmt, 0.0, 1.0);
    if (s < 1e-6) {
        return inputColor;
    }
    vec3 B = inputColor;
    float cMix = clamp(contrastAmt, 0.0, 1.0);
    B = B * (1.0 - cMix) + vec3(0.5 * cMix);
    vec3 Btemp = vec3(1.0) / (vec3(1.0) + exp(rgbCurve * 0.5));
    vec3 denom = -2.0 * Btemp + vec3(1.0);
    vec3 invDenom = vec3(1.0) / max(denom, vec3(1e-6));
    B = ((vec3(1.0) / (vec3(1.0) + exp(-rgbCurve * (B - rgbC)))) * invDenom)
        + ((-Btemp) * invDenom);
    float value = max(max(B.r, B.g), B.b);
    vec3 color = B / max(value, 1e-6);
    float cf = max(colorfulnessAmt, 1e-4);
    color = pow(max(abs(color), vec3(1e-6)), vec3(1.0 / cf));
    vec3 c0 = color * value;
    const vec3 xyzRow0 = vec3(0.5003033835433160, 0.3380975732227390, 0.1645897795458570);
    const vec3 xyzRow1 = vec3(0.2579688942747580, 0.6761952591447060, 0.0658358459823868);
    const vec3 xyzRow2 = vec3(0.0234517888692628, 0.1126992737203000, 0.8668396731242010);
    c0 = vec3(dot(xyzRow0, c0), dot(xyzRow1, c0), dot(xyzRow2, c0));
    float luma = dot(c0, vec3(0.30, 0.59, 0.11));
    float sat = clamp(saturationAmt, 0.0, 8.0);
    c0 = (1.0 - sat) * vec3(luma) + sat * c0;
    const vec3 rgbRow0 = vec3(2.6714711726599600, -1.2672360578624100, -0.4109956021722270);
    const vec3 rgbRow1 = vec3(-1.0251070293466400, 1.9840911624108900, 0.0439502493584124);
    const vec3 rgbRow2 = vec3(0.0610009456429445, -0.2236707508128630, 1.1590210416706100);
    c0 = vec3(dot(rgbRow0, c0), dot(rgbRow1, c0), dot(rgbRow2, c0));
    return clamp(mix(inputColor, c0, s), 0.0, 1.0);
}

/// SweetFX/ColorMatrix.fx v1.0 — rows build `float3x3`; `lerp(c, mul(M,c), Strength)`.
vec3 ApplySweetFxColorMatrix(vec3 c, vec3 rowR, vec3 rowG, vec3 rowB, float strength) {
    float s = clamp(strength, 0.0, 1.0);
    if (s < 1e-6) {
        return c;
    }
    vec3 mc = vec3(dot(rowR, c), dot(rowG, c), dot(rowB, c));
    return clamp(mix(c, mc, s), 0.0, 1.0);
}

float TriD_Rand11(float x) {
    return fract(x * 0.024390243);
}
float TriD_Permute(float x) {
    return mod((34.0 * x + 1.0) * x, 289.0);
}
float TriD_Rand21(vec2 uv) {
    float a = fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
    float b = fract(sin(dot(uv, vec2(39.346, 11.135))) * 26974.324);
    return (a + b) * 0.5;
}

/// TriDither.fxh (8-bit), amplitude scaled by `strength` (0–1).
/// SweetFX/Vignette.fx type 0 — elliptical distance vignette (LDR).
vec3 ApplySweetFxVignetteType0(vec3 color, vec2 uv01, float strength, float aspectView) {
    float s = clamp(strength, 0.0, 1.0);
    if (s < 1e-5) {
        return color;
    }
    vec2 distance_xy = uv01 - vec2(0.5);
    distance_xy *= vec2(aspectView, 1.0);
    const float Ratio = 1.0;
    distance_xy *= vec2(1.0, Ratio);
    const float Radius = 2.0;
    distance_xy /= Radius;
    float dist = dot(distance_xy, distance_xy);
    const float Slope = 2.0;
    float amount = -1.0 * s;
    color.rgb *= (1.0 + pow(dist, Slope * 0.5) * amount);
    return clamp(color, 0.0, 1.0);
}

/// SweetFX Border.fx v1.4.1 — `border_width.x == -border_width.y` (includes 0,0) selects ratio-based bars; else pixel widths.
vec3 ApplySweetFxBorder(
    vec3 color,
    vec2 uv,
    vec4 viewportWhPx,
    vec2 px,
    vec4 borderSzRatioStr,
    vec4 borderRgbPad) {
    float str = clamp(borderSzRatioStr.w, 0.0, 1.0);
    if (str < 1e-6) {
        return color;
    }
    float wx = borderSzRatioStr.x;
    float wy = borderSzRatioStr.y;
    float bRatio = max(borderSzRatioStr.z, 1e-4);
    vec3 borderCol = clamp(borderRgbPad.xyz, vec3(0.0), vec3(1.0));

    float bufferWidth = viewportWhPx.x;
    float bufferHeight = viewportWhPx.y;
    float bufferAspect = bufferWidth / max(bufferHeight, 1.0);

    vec2 border_width_variable = vec2(wx, wy);
    if (wx == -wy) {
        if (bufferAspect < bRatio) {
            border_width_variable =
                vec2(0.0, (bufferHeight - (bufferWidth / bRatio)) * 0.5);
        } else {
            border_width_variable =
                vec2((bufferWidth - (bufferHeight * bRatio)) * 0.5, 0.0);
        }
    }

    vec2 border = px * border_width_variable;
    vec2 within_border = clamp(
        (-uv * uv + uv) - (-border * border + border),
        vec2(0.0),
        vec2(1.0));
    vec3 masked =
        (all(greaterThan(within_border, vec2(0.0)))) ? color : borderCol;
    return mix(color, masked, str);
}

/// SweetFX/FilmGrain.fx — multiplicative grain with SNR toward brighter pixels (simplified Box–Muller).
vec3 ApplySweetFxFilmGrain(vec3 color, vec2 uv01, float timeSeconds, float intensity) {
    float g = clamp(intensity, 0.0, 0.5);
    if (g < 1e-5) {
        return color;
    }
    float inv_luma = dot(color, vec3(-1.0 / 3.0, -1.0 / 3.0, -1.0 / 3.0)) + 1.0;
    float stn = pow(max(abs(inv_luma), 1e-4), 6.0);
    float t = timeSeconds * 0.0022337;
    float seed = dot(uv01, vec2(12.9898, 78.233));
    float sine = sin(seed + t * 16.1823);
    float cosine = cos(seed - t * 9.251);
    float uniform_noise1 = fract(sine * 43758.5453 + t);
    float uniform_noise2 = fract(cosine * 53758.5453 - t);
    uniform_noise1 = max(uniform_noise1, 1e-4);
    float r = sqrt(-log(uniform_noise1));
    float theta = 6.28318530718 * uniform_noise2;
    const float varianceBase = 0.16;
    float variance = varianceBase * stn * g * g * 4.0;
    float gauss_noise = variance * r * cos(theta) + 0.5;
    float grain = mix(1.0 + g * 2.0, 1.0 - g * 2.0, gauss_noise);
    return clamp(color * grain, 0.0, 1.0);
}

vec3 TriangularDitherRgb(vec3 color, vec2 uv01, float timeSeconds, float strength) {
    if (strength < 1e-5) {
        return color;
    }
    const int kBits = 8;
    float bitstep = exp2(float(kBits)) - 1.0;
    float lsb = 1.0 / bitstep;
    float lobit = 0.5 / bitstep;
    float hibit = (bitstep - 0.5) / bitstep;

    vec3 m = vec3(uv01, TriD_Rand21(uv01 + timeSeconds * 0.001)) + 1.0;
    float hh = TriD_Permute(TriD_Permute(TriD_Permute(m.x) + m.y) + m.z);

    float n1x = TriD_Rand11(hh);
    hh = TriD_Permute(hh);
    float n2x = TriD_Rand11(hh);
    hh = TriD_Permute(hh);
    float n1y = TriD_Rand11(hh);
    hh = TriD_Permute(hh);
    float n2y = TriD_Rand11(hh);
    hh = TriD_Permute(hh);
    float n1z = TriD_Rand11(hh);
    hh = TriD_Permute(hh);
    float n2z = TriD_Rand11(hh);
    vec3 noise1 = vec3(n1x, n1y, n1z);
    vec3 noise2 = vec3(n2x, n2y, n2z);

    vec3 lo = clamp(color / lobit, 0.0, 1.0);
    vec3 hi = clamp((color - 1.0) / (hibit - 1.0), 0.0, 1.0);
    vec3 uni = noise1 - 0.5;
    vec3 tri = noise1 - noise2;
    vec3 dith = mix(uni, tri, min(lo, hi)) * lsb;
    return color + dith * clamp(strength, 0.0, 1.0);
}

/// Ported from SweetFX/CAS.fx (AMD GCNLPHLSL DX9 path — nine explicit taps). All taps use exposure-scaled scene-linear radiance.
vec3 CasSharpenHdr9Tap(vec2 tc, vec2 px, float sharpenAmount, float contrastAdapt, float exposureMul) {
    vec3 a = texture(hdrSceneLinear, tc + vec2(-px.x, -px.y)).rgb * exposureMul;
    vec3 b = texture(hdrSceneLinear, tc + vec2(0.0, -px.y)).rgb * exposureMul;
    vec3 c = texture(hdrSceneLinear, tc + vec2(px.x, -px.y)).rgb * exposureMul;
    vec3 d = texture(hdrSceneLinear, tc + vec2(-px.x, 0.0)).rgb * exposureMul;
    vec3 e = texture(hdrSceneLinear, tc).rgb * exposureMul;
    vec3 f = texture(hdrSceneLinear, tc + vec2(px.x, 0.0)).rgb * exposureMul;
    vec3 g = texture(hdrSceneLinear, tc + vec2(-px.x, px.y)).rgb * exposureMul;
    vec3 h = texture(hdrSceneLinear, tc + vec2(0.0, px.y)).rgb * exposureMul;
    vec3 i = texture(hdrSceneLinear, tc + vec2(px.x, px.y)).rgb * exposureMul;

    vec3 mnRGB = min(min(min(d, e), min(f, b)), h);
    vec3 mnRGB2 = min(mnRGB, min(min(a, c), min(g, i)));
    mnRGB = mnRGB + mnRGB2;

    vec3 mxRGB = max(max(max(d, e), max(f, b)), h);
    vec3 mxRGB2 = max(mxRGB, max(max(a, c), max(g, i)));
    mxRGB = mxRGB + mxRGB2;

    vec3 rcpMRGB = 1.0 / max(mxRGB, vec3(1e-5));
    vec3 ampRGB = clamp(min(mnRGB, 2.0 - mxRGB) * rcpMRGB, 0.0, 1.0);
    ampRGB = inversesqrt(max(ampRGB, vec3(1e-6)));

    float peak = -3.0 * clamp(contrastAdapt, 0.0, 1.0) + 8.0;
    vec3 wRGB = -1.0 / max(ampRGB * peak, vec3(1e-5));
    vec3 rcpWeightRGB = 1.0 / max(4.0 * wRGB + vec3(1.0), vec3(1e-5));
    vec3 window = b + d + f + h;
    vec3 outColor = clamp((window * wRGB + e) * rcpWeightRGB, 0.0, 1.0);
    return mix(e, outColor, clamp(sharpenAmount, 0.0, 1.0));
}

/// Cheap HDR bloom from exposed neighbors (scene-linear, same space as CAS output).
vec3 BloomAccumExposed(vec2 tc, vec2 px, float kneeLinear, float exposureMul, float intensity) {
    if (intensity < 1e-6 || kneeLinear < 0.0) {
        return vec3(0.0);
    }
    vec2 o[8] = vec2[](
        vec2(1.0, 0.0), vec2(-1.0, 0.0), vec2(0.0, 1.0), vec2(0.0, -1.0),
        vec2(1.0, 1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(-1.0, -1.0));
    vec3 acc = vec3(0.0);
    const float spread = 1.35;
    for (int k = 0; k < 8; ++k) {
        vec3 s = texture(hdrSceneLinear, tc + o[k] * px * spread).rgb * exposureMul;
        float lum = dot(s, vec3(0.2126, 0.7152, 0.0722));
        float over = max(lum - kneeLinear, 0.0);
        acc += s * (over / max(lum, 1e-4));
    }
    return acc * (1.0 / 8.0);
}

vec3 ApplyPostProcessFx(vec3 color, vec2 uv01) {
    float noiseAmount = clamp(cameraData.postProcessPrimary.x, 0.0, 0.3);
    float scanlineAmount = clamp(cameraData.postProcessPrimary.y, 0.0, 0.2);
    float barrelDistortion = clamp(cameraData.postProcessPrimary.z, 0.0, 0.2);
    float chromaticAberration = clamp(cameraData.postProcessPrimary.w, 0.0, 0.05);
    vec3 tintColor = clamp(cameraData.postProcessTint.rgb, 0.0, 1.0);
    float tintStrength = clamp(cameraData.postProcessTint.a, 0.0, 1.0);
    float blurAmount = clamp(cameraData.postProcessSecondary.x, 0.0, 0.05);
    float staticFadeAmount = clamp(cameraData.postProcessSecondary.y, 0.0, 1.0);
    float timeSeconds = cameraData.postProcessSecondary.z;

    vec2 centered = uv01 * 2.0 - 1.0;
    float aspect = cameraData.viewportMetrics.x / max(cameraData.viewportMetrics.y, 1.0);
    centered.x *= aspect;
    float radial = clamp(dot(centered, centered), 0.0, 1.0);

    float vignette = 1.0 - (barrelDistortion * radial * radial * 1.6);
    color *= clamp(vignette, 0.0, 1.0);

    float scanPhase = (gl_FragCoord.y + timeSeconds * 48.0) * 0.14;
    float scanline = 1.0 - (scanlineAmount * (0.5 + 0.5 * sin(scanPhase)));
    color *= clamp(scanline, 0.0, 1.0);

    float grainSeed = Hash21(gl_FragCoord.xy + vec2(timeSeconds * 39.0, timeSeconds * 17.0));
    float grain = (grainSeed - 0.5) * noiseAmount * 1.7;
    color = clamp(color + vec3(grain), 0.0, 1.0);

    float fringe = chromaticAberration * radial * 24.0;
    color.r = clamp(color.r + fringe, 0.0, 1.0);
    color.b = clamp(color.b - fringe * 0.75, 0.0, 1.0);

    color = mix(color, color * tintColor, tintStrength);
    float softDesat = clamp(1.0 - blurAmount * 10.0, 0.0, 1.0);
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, softDesat);
    float staticPulse = 0.85 + 0.15 * Hash11(timeSeconds * 23.0 + gl_FragCoord.y * 0.03125);
    color = mix(color, vec3(staticPulse), staticFadeAmount);

    return clamp(color, 0.0, 1.0);
}

vec3 ApplyPd80ShadowsMidtonesHighlights(vec2 sampleUv, vec3 colorIn);

vec3 ApplyPd80LumaSharpen(vec2 sampleUv, vec2 px);

/// Pre-stack through CAS … PD80 HQ bloom (reference BackBuffer input for Marty bloom pyramid).
vec3 EvaluatePrePostProcessChainThroughHqBloom(vec2 sampleUv, vec2 px) {
    float exposure = max(cameraData.renderTuning.x, 1e-5);
    float contrast = cameraData.renderTuning.y;
    float saturation = cameraData.renderTuning.z;

    float casAmt = clamp(cameraData.presentationTuning.x, 0.0, 1.0);
    float contrastAd = clamp(cameraData.presentationTuning.y, 0.0, 1.0);
    float bloomInt = clamp(cameraData.presentationTuning.z, 0.0, 0.5);
    float bloomThresh = max(cameraData.presentationTuning.w, 0.0);
    float curveAmt = clamp(cameraData.presentationColorGrading.x, 0.0, 1.0);

    vec3 casHdr = CasSharpenHdr9Tap(sampleUv, px, casAmt, contrastAd, exposure);
    vec3 bloom = BloomAccumExposed(sampleUv, px, bloomThresh, exposure, bloomInt);
    vec3 hdrLinear = casHdr + bloomInt * bloom;

    vec3 mapped = TonemapAcesApprox(hdrLinear);
    mapped = ApplyColorGrade(mapped, contrast, saturation);
    mapped = ApplySweetFxLumaCurve(mapped, curveAmt);
    mapped = ApplySweetFxLiftGammaGain(
        mapped,
        cameraData.lggLiftMix.xyz,
        cameraData.lggGammaRgb.xyz,
        cameraData.lggGainRgb.xyz,
        cameraData.lggLiftMix.w);
    mapped = ApplySweetFxVibrance(mapped, cameraData.vibranceBalanceAmount.xyz, cameraData.vibranceBalanceAmount.w);
    mapped = ApplySweetFxTechnicolor(
        mapped,
        cameraData.technicolor1PowStrNegRg.x,
        vec3(
            cameraData.technicolor1PowStrNegRg.z,
            cameraData.technicolor1PowStrNegRg.w,
            cameraData.technicolor1NegBPad.x),
        cameraData.technicolor1PowStrNegRg.y);
    mapped = ApplySweetFxTechnicolor2(
        mapped,
        cameraData.technicolor2ColBright.xyz,
        cameraData.technicolor2ColBright.w,
        cameraData.technicolor2SatStrPad.x,
        cameraData.technicolor2SatStrPad.y);
    mapped = ApplyPd80Technicolor(
        mapped,
        cameraData.pd80TcRedStrPad,
        cameraData.pd80TcCyanPad,
        cameraData.pd80TcKeySat2Pad,
        cameraData.pd80Tc3ColBrightPad,
        cameraData.pd80Tc3SatStrEnPad);
    mapped = ApplyPd80ColorTemperature(mapped, cameraData.pd80ColorTempKelvinLumMixStr);
    mapped = ApplyPd80SaturationLimit(mapped, cameraData.pd80SatLimitCapStr);
    mapped = ApplyPd80ColorBalance(
        mapped,
        cameraData.pd80ColorBalanceShadowPad,
        cameraData.pd80ColorBalanceMidPad,
        cameraData.pd80ColorBalanceHighPad,
        cameraData.pd80ColorBalanceOptStr);
    mapped = ApplyPd80ColorIsolation(mapped, cameraData.pd80ColorIsolationHueRangeSatMix, cameraData.pd80ColorIsolationStrPad);
    mapped = ApplyPd80ContrastBriSat(mapped, sampleUv);
    mapped = ApplyPd80Levels(
        mapped,
        sampleUv,
        cameraData.pd80LevelsIbPad,
        cameraData.pd80LevelsIwPad,
        cameraData.pd80LevelsObPad,
        cameraData.pd80LevelsOwPad,
        cameraData.pd80LevelsGammaDitherStr);
    mapped = ApplyPd80CurvedLevels(sampleUv, mapped);
    mapped = ApplyPd80SelectiveColor(mapped);
    mapped = ApplyPd80SelectiveColorV2(mapped);
    mapped = ApplyPd80RtCorrectContrast(sampleUv, mapped);
    mapped = ApplyPd80RtCorrectColor(sampleUv, mapped);
    mapped = ApplyPd80FilmicAdaptation(sampleUv, mapped);
    mapped = ApplyPd80HqBloom(sampleUv, px, mapped);
    return mapped;
}

vec3 EvaluatePreMartyBloomSource(vec2 sampleUv, vec2 px) {
    return EvaluatePrePostProcessChainThroughHqBloom(sampleUv, px);
}

vec3 EvaluatePrePostProcessChainBaseInner(vec2 sampleUv, vec2 px) {
    vec3 mapped = EvaluatePrePostProcessChainThroughHqBloom(sampleUv, px);
    mapped = ApplyCreatorMartyBloom(sampleUv, px, mapped);
    mapped = ApplyPd80PosterizePixelate(sampleUv, mapped);
    mapped = ApplyPd80MagicalRectangle(sampleUv, mapped);
    mapped = ApplyPd80ShadowsMidtonesHighlights(sampleUv, mapped);
    mapped = ApplyPd80BlackWhite(
        mapped,
        sampleUv,
        cameraData.pd80BwPack0,
        cameraData.pd80BwPack1,
        cameraData.pd80BwPack2,
        cameraData.pd80BwPack3);
    mapped = ApplySweetFxSepiaTint(
        mapped,
        cameraData.sepiaTintXyzStrength.xyz,
        cameraData.sepiaTintXyzStrength.w);
    mapped = ApplySweetFxMonochrome(
        mapped,
        cameraData.monochromePresetSat.x,
        cameraData.monochromeCustomCoeff.xyz,
        cameraData.monochromePresetSat.y);
    mapped = ApplySweetFxDpx(
        mapped,
        cameraData.dpxRgbCurvePad.xyz,
        cameraData.dpxRgbCPad.xyz,
        cameraData.dpxContrastSatColorStr.x,
        cameraData.dpxContrastSatColorStr.y,
        cameraData.dpxContrastSatColorStr.z,
        cameraData.dpxContrastSatColorStr.w);
    mapped = ApplySweetFxColorMatrix(
        mapped,
        cameraData.colorMatrixRowR.xyz,
        cameraData.colorMatrixRowG.xyz,
        cameraData.colorMatrixRowBStr.xyz,
        cameraData.colorMatrixRowBStr.w);
    return mapped;
}

vec3 CreatorChainSample(vec2 sampleUv, vec2 px);

/// Denoise.fx (NVIDIA KNN, WindowRadius=3) on the graded LDR chain.
vec3 ApplyCreatorDenoiseKnnLdr(vec2 sampleUv, vec2 px, vec3 orig, vec4 pack, vec4 pack2) {
    float master = pack.x;
    if (master <= 1e-6) {
        return orig;
    }
    const int kWindowRadius = 3;
    const float noiseLevel = max(pack.y, 0.01);
    const float lerpCoeff = clamp(pack.z, 0.0, 1.0);
    const float weightThreshold = clamp(pack.w, 0.0, 1.0);
    const float counterThreshold = clamp(pack2.x, 0.0, 1.0);
    const float gaussianSigma = max(pack2.y, 1.0);

    float iWindowArea = float(2 * kWindowRadius + 1);
    iWindowArea *= iWindowArea;

    vec3 result = vec3(0.0);
    float counter = 0.0;
    float sumW = 0.0;
    for (int j = -kWindowRadius; j <= kWindowRadius; ++j) {
        for (int i = -kWindowRadius; i <= kWindowRadius; ++i) {
            vec2 uv = sampleUv + px * vec2(float(i), float(j));
            vec3 texIJ = CreatorChainSample(uv, px);
            float weight = dot(orig - texIJ, orig - texIJ);
            weight = exp(-(weight / noiseLevel + float(i * i + j * j) / gaussianSigma));
            counter += float(weight > weightThreshold);
            sumW += weight;
            result += texIJ * weight;
        }
    }
    result /= max(sumW, 1e-6);
    float lerpQ = (counter > (counterThreshold * iWindowArea)) ? (1.0 - lerpCoeff) : lerpCoeff;
    result = mix(result, orig, lerpQ);
    return mix(orig, result, master);
}

vec3 EvaluatePrePostProcessChainBase(vec2 sampleUv, vec2 px) {
    return EvaluatePrePostProcessChainBaseInner(sampleUv, px);
}

vec3 CreatorGaussianHorizAt(vec2 uv, vec2 px, float blurOffset, int tapCount, float offsets[18], float weights[18]) {
    vec3 color = EvaluatePrePostProcessChainBaseInner(uv, px) * weights[0];
    for (int i = 1; i < 18; ++i) {
        if (i >= tapCount) {
            break;
        }
        vec2 delta = vec2(offsets[i] * px.x * blurOffset, 0.0);
        color += EvaluatePrePostProcessChainBaseInner(uv + delta, px) * weights[i];
        color += EvaluatePrePostProcessChainBaseInner(uv - delta, px) * weights[i];
    }
    return color;
}

vec3 CreatorGaussianBlurVertical(vec2 uv, vec2 px, float blurOffset, int tapCount, float vOffsets[18], float vWeights[18], float hOffsets[18], float hWeights[18]) {
    vec3 color = CreatorGaussianHorizAt(uv, px, blurOffset, tapCount, hOffsets, hWeights);
    color *= vWeights[0];
    for (int i = 1; i < 18; ++i) {
        if (i >= tapCount) {
            break;
        }
        vec2 delta = vec2(0.0, vOffsets[i] * px.y * blurOffset);
        color += CreatorGaussianHorizAt(uv + delta, px, blurOffset, tapCount, hOffsets, hWeights) * vWeights[i];
        color += CreatorGaussianHorizAt(uv - delta, px, blurOffset, tapCount, hOffsets, hWeights) * vWeights[i];
    }
    return color;
}

vec3 CreatorGaussianBlurAt(vec2 uv, vec2 px) {
    vec4 pack = cameraData.creatorGaussianBlurPack;
    float strength = pack.x;
    if (strength <= 1e-6) {
        return EvaluatePrePostProcessChainBaseInner(uv, px);
    }
    float blurOffset = max(pack.y, 0.0);
    int radius = clamp(int(pack.z + 0.5), 0, 4);
    vec3 orig = EvaluatePrePostProcessChainBaseInner(uv, px);
    vec3 color = orig;

    if (radius == 0) {
        float offsets[18] = float[18](0.0, 1.1824255238, 3.0293122308, 5.0040701377, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        float weights[18] = float[18](0.39894, 0.2959599993, 0.0045656525, 0.00000149278686458842, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        color = CreatorGaussianBlurVertical(uv, px, blurOffset, 4, offsets, weights, offsets, weights);
    } else if (radius == 1) {
        float offsets[18] = float[18](0.0, 1.4584295168, 3.40398480678, 5.3518057801, 7.302940716, 9.2581597095, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        float weights[18] = float[18](0.13298, 0.23227575, 0.1353261595, 0.0511557427, 0.01253922, 0.0019913644, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        color = CreatorGaussianBlurVertical(uv, px, blurOffset, 6, offsets, weights, offsets, weights);
    } else if (radius == 2) {
        float offsets[18] = float[18](0.0, 1.4895848401, 3.4757135714, 5.4618796741, 7.4481042327, 9.4344079746, 11.420811147, 13.4073334, 15.3939936778, 17.3808101174, 19.3677999584, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        float weights[18] = float[18](0.06649, 0.1284697563, 0.111918249, 0.0873132676, 0.0610011113, 0.0381655709, 0.0213835661, 0.0107290241, 0.0048206869, 0.0019396469, 0.0006988718, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        color = CreatorGaussianBlurVertical(uv, px, blurOffset, 11, offsets, weights, offsets, weights);
    } else if (radius == 3) {
        float offsets[18] = float[18](0.0, 1.4953705027, 3.4891992113, 5.4830312105, 7.4768683759, 9.4707125766, 11.4645656736, 13.4584295168, 15.4523059431, 17.4461967743, 19.4401038149, 21.43402885, 23.4279736431, 25.4219399344, 27.4159294386, 0.0, 0.0, 0.0);
        float weights[18] = float[18](0.0443266667, 0.0872994708, 0.0820892038, 0.0734818355, 0.0626171681, 0.0507956191, 0.0392263968, 0.0288369812, 0.0201808877, 0.0134446557, 0.0085266392, 0.0051478359, 0.0029586248, 0.0016187257, 0.0008430913, 0.0, 0.0, 0.0);
        color = CreatorGaussianBlurVertical(uv, px, blurOffset, 15, offsets, weights, offsets, weights);
    } else {
        float offsets[18] = float[18](0.0, 1.4953705027, 3.4891992113, 5.4830312105, 7.4768683759, 9.4707125766, 11.4645656736, 13.4584295168, 15.4523059431, 17.4461967743, 19.4661974725, 21.4627427973, 23.4592916956, 25.455844494, 27.4524015179, 29.4489630909, 31.445529535, 33.4421011704);
        float weights[18] = float[18](0.033245, 0.0659162217, 0.0636705814, 0.0598194658, 0.0546642566, 0.0485871646, 0.0420045997, 0.0353207015, 0.0288880982, 0.0229808311, 0.0177815511, 0.013382297, 0.0097960001, 0.0069746748, 0.0048301008, 0.0032534598, 0.0021315311, 0.0013582974);
        color = CreatorGaussianBlurVertical(uv, px, blurOffset, 18, offsets, weights, offsets, weights);
    }
    return mix(orig, clamp(color, 0.0, 1.0), strength);
}

vec3 CreatorChainSample(vec2 sampleUv, vec2 px) {
    return CreatorGaussianBlurAt(sampleUv, px);
}

vec3 CreatorAsGetB(vec2 sampleUv, vec2 px) {
    return clamp(CreatorChainSample(sampleUv, px), 0.0, 1.0);
}

vec2 CreatorAsPass0At(vec2 centerUv, vec2 px) {
    vec3 c[13];
    c[0] = CreatorAsGetB(centerUv, px);
    c[1] = CreatorAsGetB(centerUv + px * vec2(-1.0, -1.0), px);
    c[2] = CreatorAsGetB(centerUv + px * vec2(0.0, -1.0), px);
    c[3] = CreatorAsGetB(centerUv + px * vec2(1.0, -1.0), px);
    c[4] = CreatorAsGetB(centerUv + px * vec2(-1.0, 0.0), px);
    c[5] = CreatorAsGetB(centerUv + px * vec2(1.0, 0.0), px);
    c[6] = CreatorAsGetB(centerUv + px * vec2(-1.0, 1.0), px);
    c[7] = CreatorAsGetB(centerUv + px * vec2(0.0, 1.0), px);
    c[8] = CreatorAsGetB(centerUv + px * vec2(1.0, 1.0), px);
    c[9] = CreatorAsGetB(centerUv + px * vec2(0.0, -2.0), px);
    c[10] = CreatorAsGetB(centerUv + px * vec2(-2.0, 0.0), px);
    c[11] = CreatorAsGetB(centerUv + px * vec2(2.0, 0.0), px);
    c[12] = CreatorAsGetB(centerUv + px * vec2(0.0, 2.0), px);

    float luma = sqrt(dot(vec3(0.2558, 0.6511, 0.0931), c[0] * c[0]));
    vec3 blur = (2.0 * (c[2] + c[4] + c[5] + c[7]) + (c[1] + c[3] + c[6] + c[8]) + 4.0 * c[0]) / 16.0;
    float cComp = clamp(4.0 / 15.0 + 0.9 * exp2(dot(blur, vec3(-37.0 / 15.0))), 0.0, 1.0);

    float edge = length(
        1.38 * abs(blur - c[0])
        + 1.15 * (abs(blur - c[2]) + abs(blur - c[4]) + abs(blur - c[5]) + abs(blur - c[7]))
        + 0.92 * (abs(blur - c[1]) + abs(blur - c[3]) + abs(blur - c[6]) + abs(blur - c[8]))
        + 0.23 * (abs(blur - c[9]) + abs(blur - c[10]) + abs(blur - c[11]) + abs(blur - c[12])));
    return vec2(edge * cComp, luma);
}

float CreatorAsSoftLim(float v, float s) {
    float r = v / s;
    return clamp(abs(r) * (27.0 + r * r) / (27.0 + 9.0 * r * r), 0.0, 1.0) * s;
}

float CreatorAsWpmean(float a, float b, float w, float p) {
    return pow(abs(w) * pow(abs(a), p) + abs(1.0 - w) * pow(abs(b), p), 1.0 / p);
}

float CreatorAsMdiff(float l0, float l1, float l2, float l3, float l4, float l5, float l6, float lg) {
    return abs(lg - l1) + abs(lg - l2) + abs(lg - l3) + abs(lg - l4)
        + 0.5 * (abs(lg - l5) + abs(lg - l6));
}

/// AdaptiveSharpen.fx (bacondither, fast_ops path) fused into one composite evaluation.
vec3 ApplyCreatorAdaptiveSharpen(vec2 sampleUv, vec2 px) {
    vec4 p0 = cameraData.creatorAdaptiveSharpenPack0;
    float curveHeight = p0.x;
    if (curveHeight <= 1e-6) {
        return ApplyPd80LumaSharpen(sampleUv, px);
    }
    float curveSlope = max(p0.y, 0.01);
    float L_overshoot = max(p0.z, 0.001);
    float D_overshoot = max(p0.w, 0.001);
    vec4 p1 = cameraData.creatorAdaptiveSharpenPack1;
    float L_compr_low = p1.x;
    float L_compr_high = p1.y;
    float D_compr_low = p1.z;
    float D_compr_high = p1.w;
    vec4 p2 = cameraData.creatorAdaptiveSharpenPack2;
    float scale_lim = max(p2.x, 0.01);
    float scale_cs = p2.y;
    float pm_p = max(p2.z, 0.01);

    vec3 origsat = clamp(ApplyPd80LumaSharpen(sampleUv, px), 0.0, 1.0);

    const ivec2 dOff[25] = ivec2[25](
        ivec2(0, 0), ivec2(-1, -1), ivec2(0, -1), ivec2(1, -1), ivec2(-1, 0),
        ivec2(1, 0), ivec2(-1, 1), ivec2(0, 1), ivec2(1, 1), ivec2(0, -2),
        ivec2(-2, 0), ivec2(2, 0), ivec2(0, 2), ivec2(0, 3), ivec2(1, 2),
        ivec2(-1, 2), ivec2(3, 0), ivec2(2, 1), ivec2(2, -1), ivec2(-3, 0),
        ivec2(-2, 1), ivec2(-2, -1), ivec2(0, -3), ivec2(1, -2), ivec2(-1, -2));

    vec2 d[25];
    for (int k = 0; k < 25; ++k) {
        d[k] = CreatorAsPass0At(sampleUv + px * vec2(dOff[k]), px);
    }

    float maxedge = max(
        max(max(max(d[1].x, d[2].x), max(d[3].x, d[4].x)), max(max(d[5].x, d[6].x), max(d[7].x, d[8].x))),
        max(max(max(d[9].x, d[10].x), max(d[11].x, d[12].x)), d[0].x));

    float sbe = clamp((d[2].x + d[9].x + d[22].x + 0.056) / (abs(maxedge) + 0.03) - 0.85, 0.0, 1.0)
        * clamp((d[7].x + d[12].x + d[13].x + 0.056) / (abs(maxedge) + 0.03) - 0.85, 0.0, 1.0)
        + clamp((d[4].x + d[10].x + d[19].x + 0.056) / (abs(maxedge) + 0.03) - 0.85, 0.0, 1.0)
          * clamp((d[5].x + d[11].x + d[16].x + 0.056) / (abs(maxedge) + 0.03) - 0.85, 0.0, 1.0)
        + clamp((d[1].x + d[24].x + d[21].x + 0.056) / (abs(maxedge) + 0.03) - 0.85, 0.0, 1.0)
          * clamp((d[8].x + d[14].x + d[17].x + 0.056) / (abs(maxedge) + 0.03) - 0.85, 0.0, 1.0)
        + clamp((d[3].x + d[23].x + d[18].x + 0.056) / (abs(maxedge) + 0.03) - 0.85, 0.0, 1.0)
          * clamp((d[6].x + d[20].x + d[15].x + 0.056) / (abs(maxedge) + 0.03) - 0.85, 0.0, 1.0);

    vec2 cs = mix(vec2(L_compr_low, D_compr_low), vec2(L_compr_high, D_compr_high), clamp(1.091 * sbe - 2.282, 0.0, 1.0));

    float luma[25];
    for (int k = 0; k < 25; ++k) {
        luma[k] = d[k].y;
    }

    const vec3 W1 = vec3(0.5, 1.0, 1.41421356237);
    const vec3 W2 = vec3(0.86602540378, 1.0, 0.54772255751);
    vec3 dW = (mix(W1, W2, clamp(2.4 * d[0].x - 0.82, 0.0, 1.0)));
    dW = dW * dW;

    float mdiffC0 = 0.02 + 3.0 * (abs(luma[0] - luma[2]) + abs(luma[0] - luma[4]) + abs(luma[0] - luma[5])
        + abs(luma[0] - luma[7])
        + 0.25 * (abs(luma[0] - luma[1]) + abs(luma[0] - luma[3]) + abs(luma[0] - luma[6]) + abs(luma[0] - luma[8])));

    float weights[12];
    weights[0] = min(mdiffC0 / CreatorAsMdiff(luma[24], luma[21], luma[2], luma[4], luma[9], luma[10], luma[1], luma[0]), dW.y);
    weights[1] = dW.x;
    weights[2] = min(mdiffC0 / CreatorAsMdiff(luma[23], luma[18], luma[5], luma[2], luma[9], luma[11], luma[3], luma[0]), dW.y);
    weights[3] = dW.x;
    weights[4] = dW.x;
    weights[5] = min(mdiffC0 / CreatorAsMdiff(luma[4], luma[20], luma[15], luma[7], luma[10], luma[12], luma[6], luma[0]), dW.y);
    weights[6] = dW.x;
    weights[7] = min(mdiffC0 / CreatorAsMdiff(luma[5], luma[7], luma[17], luma[14], luma[12], luma[11], luma[8], luma[0]), dW.y);
    weights[8] = min(mdiffC0 / CreatorAsMdiff(luma[2], luma[24], luma[23], luma[22], luma[1], luma[3], luma[9], luma[0]), dW.z);
    weights[9] = min(mdiffC0 / CreatorAsMdiff(luma[20], luma[19], luma[21], luma[4], luma[1], luma[6], luma[10], luma[0]), dW.z);
    weights[10] = min(mdiffC0 / CreatorAsMdiff(luma[17], luma[5], luma[18], luma[16], luma[3], luma[8], luma[11], luma[0]), dW.z);
    weights[11] = min(mdiffC0 / CreatorAsMdiff(luma[13], luma[15], luma[7], luma[14], luma[6], luma[8], luma[12], luma[0]), dW.z);

    weights[0] = (max(max((weights[8] + weights[9]) / 4.0, weights[0]), 0.25) + weights[0]) / 2.0;
    weights[2] = (max(max((weights[8] + weights[10]) / 4.0, weights[2]), 0.25) + weights[2]) / 2.0;
    weights[5] = (max(max((weights[9] + weights[11]) / 4.0, weights[5]), 0.25) + weights[5]) / 2.0;
    weights[7] = (max(max((weights[10] + weights[11]) / 4.0, weights[7]), 0.25) + weights[7]) / 2.0;

    float lowthrsum = 0.0;
    float weightsum = 0.0;
    float negLaplace = 0.0;
    for (int pix = 0; pix < 12; ++pix) {
        float lowthr = clamp(13.2 * d[pix + 1].x - 0.221, 0.01, 1.0);
        negLaplace += (luma[pix + 1] * luma[pix + 1]) * (weights[pix] * lowthr);
        weightsum += weights[pix] * lowthr;
        lowthrsum += lowthr / 12.0;
    }
    negLaplace = sqrt(negLaplace / max(weightsum, 1e-6));

    float sharpenVal = curveHeight / (curveHeight * curveSlope * pow(abs(d[0].x), 3.5) + 0.625);
    float sharpdiff = (d[0].y - negLaplace) * (lowthrsum * sharpenVal + 0.01);

    float minOvershoot = min(abs(L_overshoot), abs(D_overshoot));
    float fskipTh = 0.114 * pow(minOvershoot, 0.676) + 3.20e-4;

    if (abs(sharpdiff) > fskipTh) {
        for (int i = 0; i < 24; i += 2) {
            float temp = luma[i];
            luma[i] = min(luma[i], luma[i + 1]);
            luma[i + 1] = max(temp, luma[i + 1]);
        }
        for (int ii = 24; ii > 0; ii -= 2) {
            float temp = luma[0];
            luma[0] = min(luma[0], luma[ii]);
            luma[ii] = max(temp, luma[ii]);
            temp = luma[24];
            luma[24] = max(luma[24], luma[ii - 1]);
            luma[ii - 1] = min(temp, luma[ii - 1]);
        }
        for (int i = 1; i < 23; i += 2) {
            float temp = luma[i];
            luma[i] = min(luma[i], luma[i + 1]);
            luma[i + 1] = max(temp, luma[i + 1]);
        }
        for (int ii = 23; ii > 1; ii -= 2) {
            float temp = luma[1];
            luma[1] = min(luma[1], luma[ii]);
            luma[ii] = max(temp, luma[ii]);
            temp = luma[23];
            luma[23] = max(luma[23], luma[ii - 1]);
            luma[ii - 1] = min(temp, luma[ii - 1]);
        }

        float nmax = (max(luma[23], d[0].y) * 2.0 + luma[24]) / 3.0;
        float nmin = (min(luma[1], d[0].y) * 2.0 + luma[0]) / 3.0;
        float minDist = min(abs(nmax - d[0].y), abs(d[0].y - nmin));
        float posScale = minDist + L_overshoot;
        float negScale = minDist + D_overshoot;
        posScale = min(posScale, scale_lim * (1.0 - scale_cs) + posScale * scale_cs);
        negScale = min(negScale, scale_lim * (1.0 - scale_cs) + negScale * scale_cs);

        float posPart = max(sharpdiff, 0.0);
        float negPart = min(sharpdiff, 0.0);
        sharpdiff = CreatorAsWpmean(posPart, CreatorAsSoftLim(posPart, posScale), cs.x, pm_p)
            - CreatorAsWpmean(negPart, CreatorAsSoftLim(negPart, negScale), cs.y, pm_p);
    }

    float sharpdiffLim = clamp(d[0].y + sharpdiff, 0.0, 1.0) - d[0].y;
    float satmul = (d[0].y + max(sharpdiffLim * 0.9, sharpdiffLim) * 1.03 + 0.03) / (d[0].y + 0.03);
    vec3 res = vec3(d[0].y) + (sharpdiffLim * 3.0 + sharpdiff) / 4.0 + (origsat - vec3(d[0].y)) * satmul;
    return clamp(res, 0.0, 1.0);
}

const mat3 kCreatorFineSharpRgbToYuv = mat3(
    vec3(0.2126, 0.7152, 0.0722),
    vec3(-0.1145721061, -0.3854278940, 0.4999999999),
    vec3(0.4999999999, -0.4541529069, -0.0458470931));
const mat3 kCreatorFineSharpYuvToRgb = mat3(
    vec3(1.0, 0.0, 1.5748),
    vec3(1.0, -0.187348, -0.467626),
    vec3(1.0, 1.8556, 0.0));

void CreatorFineSharpSort(inout float a1, inout float a2) {
    float t = min(a1, a2);
    a2 = max(a1, a2);
    a1 = t;
}

float CreatorFineSharpMedian3(float a1, float a2, float a3) {
    CreatorFineSharpSort(a2, a3);
    CreatorFineSharpSort(a1, a2);
    return min(a2, a3);
}

float CreatorFineSharpMedian5(float a1, float a2, float a3, float a4, float a5) {
    CreatorFineSharpSort(a1, a2);
    CreatorFineSharpSort(a3, a4);
    CreatorFineSharpSort(a1, a3);
    CreatorFineSharpSort(a2, a4);
    return CreatorFineSharpMedian3(a2, a3, a5);
}

float CreatorFineSharpMedian9(float a1, float a2, float a3, float a4, float a5, float a6, float a7, float a8, float a9) {
    CreatorFineSharpSort(a1, a2);
    CreatorFineSharpSort(a3, a4);
    CreatorFineSharpSort(a5, a6);
    CreatorFineSharpSort(a7, a8);
    CreatorFineSharpSort(a1, a3);
    CreatorFineSharpSort(a5, a7);
    CreatorFineSharpSort(a1, a5);
    CreatorFineSharpSort(a3, a5);
    CreatorFineSharpSort(a3, a7);
    CreatorFineSharpSort(a2, a4);
    CreatorFineSharpSort(a6, a8);
    CreatorFineSharpSort(a4, a8);
    CreatorFineSharpSort(a4, a6);
    CreatorFineSharpSort(a2, a6);
    return CreatorFineSharpMedian5(a2, a4, a5, a7, a9);
}

void CreatorFineSharpSortMinMax7(inout float a1, inout float a2, inout float a3, inout float a4, inout float a5, inout float a6, inout float a7) {
    CreatorFineSharpSort(a1, a2);
    CreatorFineSharpSort(a3, a4);
    CreatorFineSharpSort(a5, a6);
    CreatorFineSharpSort(a1, a3);
    CreatorFineSharpSort(a1, a5);
    CreatorFineSharpSort(a2, a6);
    CreatorFineSharpSort(a4, a5);
    CreatorFineSharpSort(a1, a7);
    CreatorFineSharpSort(a6, a7);
}

void CreatorFineSharpSortMinMax9(inout float a1, inout float a2, inout float a3, inout float a4, inout float a5, inout float a6, inout float a7, inout float a8, inout float a9) {
    CreatorFineSharpSort(a1, a2);
    CreatorFineSharpSort(a3, a4);
    CreatorFineSharpSort(a5, a6);
    CreatorFineSharpSort(a7, a8);
    CreatorFineSharpSort(a1, a3);
    CreatorFineSharpSort(a5, a7);
    CreatorFineSharpSort(a1, a5);
    CreatorFineSharpSort(a2, a4);
    CreatorFineSharpSort(a6, a8);
    CreatorFineSharpSort(a4, a8);
    CreatorFineSharpSort(a1, a9);
    CreatorFineSharpSort(a8, a9);
}

void CreatorFineSharpSort9Partial2(inout float a1, inout float a2, inout float a3, inout float a4, inout float a5, inout float a6, inout float a7, inout float a8, inout float a9) {
    CreatorFineSharpSortMinMax9(a1, a2, a3, a4, a5, a6, a7, a8, a9);
    CreatorFineSharpSortMinMax7(a2, a3, a4, a5, a6, a7, a8);
}

vec4 CreatorFineSharpP0At(vec2 uv, vec2 px) {
    vec3 rgb = CreatorChainSample(uv, px);
    vec3 yuv = kCreatorFineSharpRgbToYuv * rgb + vec3(0.0, 0.5, 0.5);
    return vec4(yuv, yuv.x);
}

float CreatorFineSharpY0(vec2 uv, vec2 px) {
    return CreatorFineSharpP0At(uv, px).x;
}

vec4 CreatorFineSharpP1FromP0(vec2 uv, vec2 px) {
    vec4 o = CreatorFineSharpP0At(uv, px);
    o.x += o.x;
    o.x += CreatorFineSharpY0(uv + px * vec2(0.0, -1.0), px)
        + CreatorFineSharpY0(uv + px * vec2(-1.0, 0.0), px)
        + CreatorFineSharpY0(uv + px * vec2(1.0, 0.0), px)
        + CreatorFineSharpY0(uv + px * vec2(0.0, 1.0), px);
    o.x += o.x;
    o.x += CreatorFineSharpY0(uv + px * vec2(-1.0, -1.0), px)
        + CreatorFineSharpY0(uv + px * vec2(1.0, -1.0), px)
        + CreatorFineSharpY0(uv + px * vec2(-1.0, 1.0), px)
        + CreatorFineSharpY0(uv + px * vec2(1.0, 1.0), px);
    o.x *= 0.0625;
    return o;
}

float CreatorFineSharpY1(vec2 uv, vec2 px) {
    return CreatorFineSharpP1FromP0(uv, px).x;
}

vec4 CreatorFineSharpP2FromP1(vec2 uv, vec2 px) {
    vec4 o = CreatorFineSharpP1FromP0(uv, px);
    o.x = CreatorFineSharpMedian9(
        CreatorFineSharpY1(uv + px * vec2(-1.0, -1.0), px),
        CreatorFineSharpY1(uv + px * vec2(0.0, -1.0), px),
        CreatorFineSharpY1(uv + px * vec2(1.0, -1.0), px),
        CreatorFineSharpY1(uv + px * vec2(-1.0, 0.0), px),
        o.x,
        CreatorFineSharpY1(uv + px * vec2(1.0, 0.0), px),
        CreatorFineSharpY1(uv + px * vec2(-1.0, 1.0), px),
        CreatorFineSharpY1(uv + px * vec2(0.0, 1.0), px),
        CreatorFineSharpY1(uv + px * vec2(1.0, 1.0), px));
    return o;
}

vec4 CreatorFineSharpP2FromP0Direct(vec2 uv, vec2 px) {
    vec4 o = CreatorFineSharpP0At(uv, px);
    o.x = CreatorFineSharpMedian9(
        CreatorFineSharpY0(uv + px * vec2(-1.0, -1.0), px),
        CreatorFineSharpY0(uv + px * vec2(0.0, -1.0), px),
        CreatorFineSharpY0(uv + px * vec2(1.0, -1.0), px),
        CreatorFineSharpY0(uv + px * vec2(-1.0, 0.0), px),
        o.x,
        CreatorFineSharpY0(uv + px * vec2(1.0, 0.0), px),
        CreatorFineSharpY0(uv + px * vec2(-1.0, 1.0), px),
        CreatorFineSharpY0(uv + px * vec2(0.0, 1.0), px),
        CreatorFineSharpY0(uv + px * vec2(1.0, 1.0), px));
    return o;
}

float CreatorFineSharpY2p0(vec2 uv, vec2 px) {
    return CreatorFineSharpP2FromP0Direct(uv, px).x;
}

vec4 CreatorFineSharpP1FromP2FromP0(vec2 uv, vec2 px) {
    vec4 o = CreatorFineSharpP2FromP0Direct(uv, px);
    o.x += o.x;
    o.x += CreatorFineSharpY2p0(uv + px * vec2(0.0, -1.0), px)
        + CreatorFineSharpY2p0(uv + px * vec2(-1.0, 0.0), px)
        + CreatorFineSharpY2p0(uv + px * vec2(1.0, 0.0), px)
        + CreatorFineSharpY2p0(uv + px * vec2(0.0, 1.0), px);
    o.x += o.x;
    o.x += CreatorFineSharpY2p0(uv + px * vec2(-1.0, -1.0), px)
        + CreatorFineSharpY2p0(uv + px * vec2(1.0, -1.0), px)
        + CreatorFineSharpY2p0(uv + px * vec2(-1.0, 1.0), px)
        + CreatorFineSharpY2p0(uv + px * vec2(1.0, 1.0), px);
    o.x *= 0.0625;
    return o;
}

float CreatorFineSharpY1p2p0(vec2 uv, vec2 px) {
    return CreatorFineSharpP1FromP2FromP0(uv, px).x;
}

vec4 CreatorFineSharpP2FromP1FromP2FromP0(vec2 uv, vec2 px) {
    vec4 o = CreatorFineSharpP1FromP2FromP0(uv, px);
    o.x = CreatorFineSharpMedian9(
        CreatorFineSharpY1p2p0(uv + px * vec2(-1.0, -1.0), px),
        CreatorFineSharpY1p2p0(uv + px * vec2(0.0, -1.0), px),
        CreatorFineSharpY1p2p0(uv + px * vec2(1.0, -1.0), px),
        CreatorFineSharpY1p2p0(uv + px * vec2(-1.0, 0.0), px),
        o.x,
        CreatorFineSharpY1p2p0(uv + px * vec2(1.0, 0.0), px),
        CreatorFineSharpY1p2p0(uv + px * vec2(-1.0, 1.0), px),
        CreatorFineSharpY1p2p0(uv + px * vec2(0.0, 1.0), px),
        CreatorFineSharpY1p2p0(uv + px * vec2(1.0, 1.0), px));
    return o;
}

vec4 CreatorFineSharpGrainAt(vec2 uv, vec2 px, int mode) {
    if (mode == 0) {
        return CreatorFineSharpP2FromP1(uv, px);
    }
    if (mode == 1) {
        return CreatorFineSharpP1FromP2FromP0(uv, px);
    }
    return CreatorFineSharpP2FromP1FromP2FromP0(uv, px);
}

float CreatorFineSharpSharpDiff(vec4 c, float sstr, float lstr, float pstr, float ldmp) {
    float t = c.a - c.x;
    return sign(t) * (sstr / 255.0) * pow(abs(t) / (lstr / 255.0), 1.0 / pstr) * ((t * t) / (t * t + ldmp / 65025.0));
}

vec4 CreatorFineSharpP3At(vec2 uv, vec2 px, float sstr, float cstr, float lstr, float pstr, float ldmp, int mode) {
    vec4 o = CreatorFineSharpGrainAt(uv, px, mode);
    float sd = CreatorFineSharpSharpDiff(o, sstr, lstr, pstr, ldmp);
    o.x = o.a + sd;
    sd += sd;
    sd += CreatorFineSharpSharpDiff(CreatorFineSharpGrainAt(uv + px * vec2(0.0, -1.0), px, mode), sstr, lstr, pstr, ldmp)
        + CreatorFineSharpSharpDiff(CreatorFineSharpGrainAt(uv + px * vec2(-1.0, 0.0), px, mode), sstr, lstr, pstr, ldmp)
        + CreatorFineSharpSharpDiff(CreatorFineSharpGrainAt(uv + px * vec2(1.0, 0.0), px, mode), sstr, lstr, pstr, ldmp)
        + CreatorFineSharpSharpDiff(CreatorFineSharpGrainAt(uv + px * vec2(0.0, 1.0), px, mode), sstr, lstr, pstr, ldmp);
    sd += sd;
    sd += CreatorFineSharpSharpDiff(CreatorFineSharpGrainAt(uv + px * vec2(-1.0, -1.0), px, mode), sstr, lstr, pstr, ldmp)
        + CreatorFineSharpSharpDiff(CreatorFineSharpGrainAt(uv + px * vec2(1.0, -1.0), px, mode), sstr, lstr, pstr, ldmp)
        + CreatorFineSharpSharpDiff(CreatorFineSharpGrainAt(uv + px * vec2(-1.0, 1.0), px, mode), sstr, lstr, pstr, ldmp)
        + CreatorFineSharpSharpDiff(CreatorFineSharpGrainAt(uv + px * vec2(1.0, 1.0), px, mode), sstr, lstr, pstr, ldmp);
    sd *= 0.0625;
    o.x -= cstr * sd;
    o.a = o.x;
    return o;
}

vec4 CreatorFineSharpP4At(vec2 uv, vec2 px, float sstr, float cstr, float lstr, float pstr, float ldmp, int mode) {
    vec4 o = CreatorFineSharpP3At(uv, px, sstr, cstr, lstr, pstr, ldmp, mode);
    float t1 = CreatorFineSharpP3At(uv + px * vec2(-1.0, -1.0), px, sstr, cstr, lstr, pstr, ldmp, mode).a;
    float t2 = CreatorFineSharpP3At(uv + px * vec2(0.0, -1.0), px, sstr, cstr, lstr, pstr, ldmp, mode).a;
    float t3 = CreatorFineSharpP3At(uv + px * vec2(1.0, -1.0), px, sstr, cstr, lstr, pstr, ldmp, mode).a;
    float t4 = CreatorFineSharpP3At(uv + px * vec2(-1.0, 0.0), px, sstr, cstr, lstr, pstr, ldmp, mode).a;
    float t5 = o.a;
    float t6 = CreatorFineSharpP3At(uv + px * vec2(1.0, 0.0), px, sstr, cstr, lstr, pstr, ldmp, mode).a;
    float t7 = CreatorFineSharpP3At(uv + px * vec2(-1.0, 1.0), px, sstr, cstr, lstr, pstr, ldmp, mode).a;
    float t8 = CreatorFineSharpP3At(uv + px * vec2(0.0, 1.0), px, sstr, cstr, lstr, pstr, ldmp, mode).a;
    float t9 = CreatorFineSharpP3At(uv + px * vec2(1.0, 1.0), px, sstr, cstr, lstr, pstr, ldmp, mode).a;
    o.x += t1 + t2 + t3 + t4 + t6 + t7 + t8 + t9;
    o.x /= 9.0;
    o.x = 9.9 * (o.a - o.x) + o.a;
    CreatorFineSharpSort9Partial2(t1, t2, t3, t4, t5, t6, t7, t8, t9);
    o.x = max(o.x, min(t2, o.a));
    o.x = min(o.x, max(t8, o.a));
    return o;
}

vec4 CreatorFineSharpP5At(vec2 uv, vec2 px, float sstr, float cstr, float xstr, float xrep, float lstr, float pstr, float ldmp, int mode) {
    vec4 o = CreatorFineSharpP4At(uv, px, sstr, cstr, lstr, pstr, ldmp, mode);
    float edge = abs(
        CreatorFineSharpP4At(uv + px * vec2(0.0, -1.0), px, sstr, cstr, lstr, pstr, ldmp, mode).x
        + CreatorFineSharpP4At(uv + px * vec2(-1.0, 0.0), px, sstr, cstr, lstr, pstr, ldmp, mode).x
        + CreatorFineSharpP4At(uv + px * vec2(1.0, 0.0), px, sstr, cstr, lstr, pstr, ldmp, mode).x
        + CreatorFineSharpP4At(uv + px * vec2(0.0, 1.0), px, sstr, cstr, lstr, pstr, ldmp, mode).x
        - 4.0 * o.x);
    o.x = mix(o.a, o.x, xstr * (1.0 - clamp(edge * xrep, 0.0, 1.0)));
    return o;
}

vec3 CreatorFineSharpP6At(vec2 uv, vec2 px, float sstr, float cstr, float xstr, float xrep, float lstr, float pstr, float ldmp, int mode) {
    vec4 yuv = CreatorFineSharpP5At(uv, px, sstr, cstr, xstr, xrep, lstr, pstr, ldmp, mode);
    return kCreatorFineSharpYuvToRgb * (yuv.xyz - vec3(0.0, 0.5, 0.5));
}

/// FineSharp.fx (Didée / JPulowski) — fused 7-pass YUV sharpen chain (Mode1/2/3).
vec3 ApplyCreatorFineSharp(vec2 sampleUv, vec2 px) {
    vec4 p0 = cameraData.creatorFineSharpPack0;
    float sstr = p0.x;
    if (sstr <= 1e-6) {
        return ApplyPd80LumaSharpen(sampleUv, px);
    }
    float cstr = p0.y;
    float xstr = p0.z;
    float xrep = p0.w;
    vec4 p1 = cameraData.creatorFineSharpPack1;
    float lstr = max(p1.x, 1e-4);
    float pstr = max(p1.y, 1e-4);
    int mode = clamp(int(p1.z + 0.5), 0, 2);
    float ldmp = sstr + 0.1;
    return clamp(
        CreatorFineSharpP6At(sampleUv, px, sstr, cstr, xstr, xrep, lstr, pstr, ldmp, mode),
        0.0,
        1.0);
}

const float kPd80LsPi = 3.14159265359;

float Pd80LsGetAvgColor(vec3 col) {
    return dot(col, vec3(0.333333, 0.333334, 0.333333));
}

/// PD80 sharpen — `ClipColor` / luminosity blend (same as CBS helpers).
vec3 Pd80LsClipColor(vec3 color) {
    float lum = Pd80LsGetAvgColor(color);
    float mincol = min(min(color.x, color.y), color.z);
    float maxcol = max(max(color.x, color.y), color.z);
    color = (mincol < 0.0)
        ? lum + ((color - lum) * lum) / (lum - mincol + 1e-8)
        : color;
    color = (maxcol > 1.0)
        ? lum + ((color - lum) * (1.0 - lum)) / (maxcol - lum + 1e-8)
        : color;
    return color;
}

vec3 Pd80LsBlendLuma(vec3 base, vec3 blend) {
    float lumbase = Pd80LsGetAvgColor(base);
    float lumblend = Pd80LsGetAvgColor(blend);
    float ldiff = lumblend - lumbase;
    return Pd80LsClipColor(base + ldiff);
}

vec3 Pd80LsScreen(vec3 c, vec3 b) {
    return 1.0 - (1.0 - c) * (1.0 - b);
}

/// Horizontal gaussian of `EvaluatePrePostProcessChainBase` (PS_GaussianH).
vec3 Pd80LsGaussianBlurH(vec2 texcoord, vec2 px, float blurSigma) {
    float vw = cameraData.viewportMetrics.x;
    float vh = cameraData.viewportMetrics.y;
    float maxDim = max(vw, vh);
    int loops = clamp(int(maxDim / 1920.0 * 4.0 + 0.5), 1, 12);
    float pxw = px.x;
    float pxlOffset = 1.0;
    float bSigma = blurSigma * (maxDim / 1920.0);
    bSigma = max(bSigma, 1e-4);
    float sigmaX = 1.0 / (sqrt(2.0 * kPd80LsPi) * bSigma);
    float sigmaY = exp(-0.5 / (bSigma * bSigma));
    float sigmaZ = sigmaY * sigmaY;
    vec3 color = EvaluatePrePostProcessChainBaseInner(texcoord, px) * sigmaX;
    float sigmaSum = sigmaX;
    sigmaX *= sigmaY;
    sigmaY *= sigmaZ;
    for (int i = 0; i < 12; ++i) {
        if (i >= loops) {
            break;
        }
        color += EvaluatePrePostProcessChainBaseInner(texcoord + vec2(pxlOffset * pxw, 0.0), px) * sigmaX;
        color += EvaluatePrePostProcessChainBaseInner(texcoord - vec2(pxlOffset * pxw, 0.0), px) * sigmaX;
        sigmaSum += 2.0 * sigmaX;
        pxlOffset += 1.0;
        sigmaX *= sigmaY;
        sigmaY *= sigmaZ;
    }
    return color / max(sigmaSum, 1e-8);
}

/// Vertical gaussian of `Pd80LsGaussianBlurH` (PS_GaussianV).
vec3 Pd80LsGaussianBlurHV(vec2 texcoord, vec2 px, float blurSigma) {
    float vw = cameraData.viewportMetrics.x;
    float vh = cameraData.viewportMetrics.y;
    float maxDim = max(vw, vh);
    int loops = clamp(int(maxDim / 1920.0 * 4.0 + 0.5), 1, 12);
    float pyh = px.y;
    float pxlOffset = 1.0;
    float bSigma = blurSigma * (maxDim / 1920.0);
    bSigma = max(bSigma, 1e-4);
    float sigmaX = 1.0 / (sqrt(2.0 * kPd80LsPi) * bSigma);
    float sigmaY = exp(-0.5 / (bSigma * bSigma));
    float sigmaZ = sigmaY * sigmaY;
    vec3 color = Pd80LsGaussianBlurH(texcoord, px, blurSigma) * sigmaX;
    float sigmaSum = sigmaX;
    sigmaX *= sigmaY;
    sigmaY *= sigmaZ;
    for (int i = 0; i < 12; ++i) {
        if (i >= loops) {
            break;
        }
        color += Pd80LsGaussianBlurH(texcoord + vec2(0.0, pxlOffset * pyh), px, blurSigma) * sigmaX;
        color += Pd80LsGaussianBlurH(texcoord - vec2(0.0, pxlOffset * pyh), px, blurSigma) * sigmaX;
        sigmaSum += 2.0 * sigmaX;
        pxlOffset += 1.0;
        sigmaX *= sigmaY;
        sigmaY *= sigmaZ;
    }
    return color / max(sigmaSum, 1e-8);
}

/// PD80_05_Sharpening.fx — all blur taps use `EvaluatePrePostProcessChainBase` only.
vec3 ApplyPd80LumaSharpen(vec2 sampleUv, vec2 px) {
    vec4 pk0 = cameraData.pd80LsPack0;
    float master = clamp(pk0.x, 0.0, 1.0);
    if (master < 1e-6) {
        return EvaluatePrePostProcessChainBaseInner(sampleUv, px);
    }
    float blurSigma = max(pk0.y, 1e-4);
    float Sharpening = pk0.z;
    float Threshold = pk0.w;
    vec4 pk1 = cameraData.pd80LsPack1;
    float limiter = pk1.x;
    bool enableShowEdges = pk1.y > 0.5;
    bool enable_depth = pk1.z > 0.5;
    bool enable_reverse = pk1.w > 0.5;
    vec4 pk2 = cameraData.pd80LsPack2;
    bool display_depth = pk2.x > 0.5;
    float depthStart = pk2.y;
    float depthEnd = pk2.z;
    float depthCurve = max(pk2.w, 0.05);

    vec3 orig = EvaluatePrePostProcessChainBaseInner(sampleUv, px);
    vec3 gaussian = Pd80LsGaussianBlurHV(sampleUv, px, blurSigma);

    float depthLin = texture(sceneDepth, sampleUv).r;
    float depth = smoothstep(depthStart, depthEnd, depthLin);
    depth = pow(clamp(depth, 0.0, 1.0), depthCurve);
    depth = enable_reverse ? (1.0 - depth) : depth;

    vec3 edges = max(clamp(orig - gaussian, 0.0, 1.0) - Threshold, vec3(0.0));
    vec3 invGauss = clamp(1.0 - gaussian, 0.0, 1.0);
    vec3 oInvGauss = clamp(orig + invGauss, 0.0, 1.0);
    vec3 invOGauss = max(clamp(1.0 - oInvGauss, 0.0, 1.0) - Threshold, vec3(0.0));
    edges = max((clamp(Sharpening * edges, 0.0, 1.0)) - (clamp(Sharpening * invOGauss, 0.0, 1.0)), vec3(0.0));
    vec3 edgeClamp = min(edges, vec3(limiter));
    float depthScreenMix = enable_depth ? depth : 0.0;
    vec3 blendScrArg = Pd80LsScreen(orig, mix(edgeClamp, vec3(0.0), depthScreenMix));
    vec3 color = Pd80LsBlendLuma(orig, blendScrArg);

    if (display_depth) {
        return vec3(depthLin);
    }
    if (enableShowEdges) {
        vec3 edgeShow = mix(edgeClamp, edgeClamp * vec3(depth), enable_depth ? 1.0 : 0.0);
        return mix(orig, edgeShow, master);
    }
    return mix(orig, color, master);
}

vec3 EvaluatePrePostProcessChainCore(vec2 sampleUv, vec2 px) {
    vec3 center;
    if (cameraData.creatorFineSharpPack0.x > 1e-6) {
        center = ApplyCreatorFineSharp(sampleUv, px);
    } else if (cameraData.creatorAdaptiveSharpenPack0.x > 1e-6) {
        center = ApplyCreatorAdaptiveSharpen(sampleUv, px);
    } else {
        center = ApplyPd80LumaSharpen(sampleUv, px);
    }
    return ApplyCreatorDenoiseKnnLdr(
        sampleUv, px, center, cameraData.creatorDenoisePack, cameraData.creatorDenoisePack2);
}

vec3 Pd80CaHueToRgb(float H) {
    return clamp(
        vec3(abs(H * 6.0 - 3.0) - 1.0, 2.0 - abs(H * 6.0 - 2.0), 2.0 - abs(H * 6.0 - 4.0)),
        0.0,
        1.0);
}

/// PD80_06_Chromatic_Aberration.fx — multi-tap spectral ring; taps use graded chain + PD80 sharpen (no CA recursion).
vec3 ApplyPd80ChromaticAberration(vec2 sampleUv, vec2 px) {
    vec4 pk0 = cameraData.pd80CaPack0;
    float master = clamp(pk0.x, 0.0, 1.0);
    if (master < 1e-6) {
        return EvaluatePrePostProcessChainCore(sampleUv, px);
    }

    vec3 orig = EvaluatePrePostProcessChainCore(sampleUv, px);
    float caStr = clamp(pk0.y, 0.0, 1.0);
    float caGlobal = pk0.z;
    int sampleSteps = clamp(int(pk0.w + 0.5), 8, 96);

    vec4 pk1 = cameraData.pd80CaPack1;
    int caType = clamp(int(pk1.x + 0.5), 0, 3);
    float degrees = pk1.y;
    float CA_width_param = pk1.z;
    float CA_curve = pk1.w;

    vec4 pk2 = cameraData.pd80CaPack2;
    float oX = pk2.x;
    float oY = pk2.y;
    float CA_shapeX = pk2.z;
    float CA_shapeY = pk2.w;

    vec4 pk3 = cameraData.pd80CaPack3;
    vec3 vignetteColor = clamp(pk3.xyz, 0.0, 1.0);
    float showCaAmt = step(0.5, pk3.w);

    vec4 pk4 = cameraData.pd80CaPack4;
    bool enable_depth_int = pk4.x > 0.5;
    bool enable_depth_width = pk4.y > 0.5;
    bool display_depth = pk4.z > 0.5;

    vec4 pk5 = cameraData.pd80CaPack5;
    float depthStart = pk5.x;
    float depthEnd = pk5.y;
    float depthCurve = pk5.z;

    float vw = cameraData.viewportMetrics.x;
    float vh = cameraData.viewportMetrics.y;
    float pxw = px.x;
    float pxh = px.y;
    float aspect = vw / max(vh, 1.0);

    float depthLin = texture(sceneDepth, sampleUv).r;
    float depthBlend = smoothstep(depthStart, depthEnd, depthLin);
    depthBlend = pow(clamp(depthBlend, 0.0, 1.0), depthCurve);

    float CA_width_n = CA_width_param;
    if (enable_depth_width) {
        CA_width_n *= depthBlend;
    }

    vec2 coords = sampleUv * 2.0 - vec2(oX + 1.0, oY + 1.0);
    vec2 uv = coords;
    coords /= vec2(CA_shapeX / aspect, CA_shapeY);

    float lenW = length(coords) * CA_width_n;
    vec2 caintensity = vec2(lenW);
    caintensity.y = caintensity.x * caintensity.x + 1.0;
    caintensity.x = 1.0 - (1.0 / (caintensity.y * caintensity.y));
    caintensity.x = pow(caintensity.x, CA_curve);

    if (caType == 2 || caType == 3) {
        caintensity.x = 1.0;
    }

    if (enable_depth_int) {
        caintensity.x *= depthBlend;
    }

    float degreesY = degrees;
    float c = 0.0;
    float s = 0.0;
    if (caType == 0) {
        degreesY = degrees + 90.0;
        if (degreesY > 360.0) {
            degreesY -= 360.0;
        }
        c = cos(radians(degrees)) * uv.x;
        s = sin(radians(degreesY)) * uv.y;
    } else if (caType == 1) {
        c = cos(radians(degrees));
        s = sin(radians(degrees));
    } else if (caType == 2) {
        degreesY = degrees + 90.0;
        if (degreesY > 360.0) {
            degreesY -= 360.0;
        }
        c = cos(radians(degrees)) * uv.x;
        s = sin(radians(degreesY)) * uv.y;
    } else {
        c = cos(radians(degrees));
        s = sin(radians(degrees));
    }

    float caWidth = caGlobal * (max(vw, vh) / 1920.0);
    float offsetX = pxw * c * caintensity.x;
    float offsetY = pxh * s * caintensity.x;

    vec3 colorAcc = vec3(0.0);
    vec3 dAcc = vec3(0.0);
    float o1 = float(max(sampleSteps - 1, 1));
    float sampst = float(sampleSteps);

    for (int i = 0; i < 96; ++i) {
        if (i >= sampleSteps) {
            break;
        }
        float fi = float(i);
        vec3 huecolor = Pd80CaHueToRgb(fi / sampst);
        float o2 = mix(-caWidth, caWidth, fi / o1);
        vec2 duv = sampleUv + vec2(o2 * offsetX, o2 * offsetY);
        vec3 temp = EvaluatePrePostProcessChainCore(duv, px);
        colorAcc += temp * huecolor;
        dAcc += huecolor;
    }

    vec3 ring = colorAcc / max(dot(dAcc, vec3(0.333333)), 1e-5);
    vec3 afterCaStr = mix(orig, ring, caStr);
    vec3 vigBlend = vignetteColor * caintensity.x + (1.0 - caintensity.x) * afterCaStr;
    vec3 afterVig = mix(afterCaStr, vigBlend, showCaAmt);
    if (display_depth) {
        afterVig = vec3(depthLin);
    }
    return mix(orig, afterVig, master);
}

vec3 EvaluatePrePostProcessChain(vec2 sampleUv, vec2 px) {
    return ApplyPd80ChromaticAberration(sampleUv, px);
}

/// --- PD80_06_Film_Grain.fx (perm texture replaced by procedural gradient lookup; math otherwise matches reference). ---
float pd80FgFade(float t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float pd80FgCurveScalar(float x) {
    return x * x * (3.0 - 2.0 * x);
}

vec4 pd80FgRnm(vec2 tc, float t, float grainAdjust) {
    float noise = sin(dot(tc, vec2(12.9898, 78.233))) * (43758.5453 + t);
    float noiseR = fract(noise * grainAdjust) * 2.0 - 1.0;
    float noiseG = fract(noise * 1.2154 * grainAdjust) * 2.0 - 1.0;
    float noiseB = fract(noise * 1.3453 * grainAdjust) * 2.0 - 1.0;
    float noiseA = fract(noise * 1.3647 * grainAdjust) * 2.0 - 1.0;
    return vec4(noiseR, noiseG, noiseB, noiseA);
}

vec3 pd80FgPermTex(vec2 hz) {
    return texture(nativePd80PermTexture, hz).rgb;
}

float pd80FgPnoise3D(vec3 p, float t, float grainAdjust, float grainSize) {
    const float permTexSize = 256.0;
    const float permONE = 1.0 / permTexSize;
    const float permHALF = 0.5 * permONE;
    vec3 pi = permONE * floor(p) + permHALF;
    pi.xy *= permTexSize;
    pi.xy = round((pi.xy - permHALF) / grainSize) * grainSize;
    pi.xy /= permTexSize;
    vec3 pf = fract(p);

    float perm00 = pd80FgRnm(pi.xy, t, grainAdjust).x;
    vec3 grad000 = pd80FgPermTex(vec2(perm00, pi.z)).xyz * 4.0 - 1.0;
    float n000 = dot(grad000, pf);
    vec3 grad001 = pd80FgPermTex(vec2(perm00, pi.z + permONE)).xyz * 4.0 - 1.0;
    float n001 = dot(grad001, pf - vec3(0.0, 0.0, 1.0));

    float perm01 = pd80FgRnm(pi.xy + vec2(0.0, permONE), t, grainAdjust).y;
    vec3 grad010 = pd80FgPermTex(vec2(perm01, pi.z)).xyz * 4.0 - 1.0;
    float n010 = dot(grad010, pf - vec3(0.0, 1.0, 0.0));
    vec3 grad011 = pd80FgPermTex(vec2(perm01, pi.z + permONE)).xyz * 4.0 - 1.0;
    float n011 = dot(grad011, pf - vec3(0.0, 1.0, 1.0));

    float perm10 = pd80FgRnm(pi.xy + vec2(permONE, 0.0), t, grainAdjust).z;
    vec3 grad100 = pd80FgPermTex(vec2(perm10, pi.z)).xyz * 4.0 - 1.0;
    float n100 = dot(grad100, pf - vec3(1.0, 0.0, 0.0));
    vec3 grad101 = pd80FgPermTex(vec2(perm10, pi.z + permONE)).xyz * 4.0 - 1.0;
    float n101 = dot(grad101, pf - vec3(1.0, 0.0, 1.0));

    float perm11 = pd80FgRnm(pi.xy + vec2(permONE, permONE), t, grainAdjust).w;
    vec3 grad110 = pd80FgPermTex(vec2(perm11, pi.z)).xyz * 4.0 - 1.0;
    float n110 = dot(grad110, pf - vec3(1.0, 1.0, 0.0));
    vec3 grad111 = pd80FgPermTex(vec2(perm11, pi.z + permONE)).xyz * 4.0 - 1.0;
    float n111 = dot(grad111, pf - vec3(1.0, 1.0, 1.0));

    vec4 n_x = mix(vec4(n000, n001, n010, n011), vec4(n100, n101, n110, n111), pd80FgFade(pf.x));
    vec2 n_xy = mix(n_x.xy, n_x.zw, pd80FgFade(pf.y));
    return mix(n_xy.x, n_xy.y, pd80FgFade(pf.z));
}

/// PD80_06_Film_Grain.fx — `PS_FilmGrain` + `PS_MergeNoise` (FG_GRAIN_SMOOTHING off).
vec3 ApplyPd80FilmGrain(vec2 sampleUv, vec2 px, vec3 colorIn) {
    vec4 pk0 = cameraData.pd80FgPack0;
    float master = clamp(pk0.x, 0.0, 1.0);
    if (master < 1e-6) {
        return colorIn;
    }
    float grainAdjust = clamp(pk0.y, 1.0, 2.0);
    float grainSize = clamp(pk0.z, 1.0, 4.0);
    bool grainMotion = pk0.w > 0.5;

    vec4 pk1 = cameraData.pd80FgPack1;
    float grainOrigColor = pk1.x > 0.5 ? 1.0 : 0.0;
    bool use_negnoise = pk1.y > 0.5;
    float grainColorAmt = clamp(pk1.z, 0.0, 1.0);
    float grainAmount = clamp(pk1.w, 0.0, 1.0);

    vec4 pk2 = cameraData.pd80FgPack2;
    float grainIntensity = clamp(pk2.x, 0.0, 1.0);
    float grainDensity = clamp(pk2.y, 0.0, 10.0);
    float grainIntHigh = clamp(pk2.z, 0.0, 1.0);
    float grainIntLow = clamp(pk2.w, 0.0, 1.0);

    vec4 pk3 = cameraData.pd80FgPack3;
    bool enable_test = pk3.x > 0.5;
    bool enable_depth = pk3.y > 0.5;
    bool display_depth = pk3.z > 0.5;

    vec4 pk4 = cameraData.pd80FgPack4;
    float depthStart = pk4.x;
    float depthEnd = pk4.y;
    float depthCurve = max(pk4.z, 0.05);

    float timer = grainMotion ? mod(cameraData.postProcessSecondary.z, 1000.0) : 1.0;
    float vw = cameraData.viewportMetrics.x;
    float vh = cameraData.viewportMetrics.y;
    vec2 uvPix = sampleUv * vec2(vw, vh);

    vec3 noise;
    noise.x = pd80FgPnoise3D(vec3(uvPix.xy, 1.0), timer, grainAdjust, grainSize);
    noise.y = pd80FgPnoise3D(vec3(uvPix.xy, 2.0), timer, grainAdjust, grainSize);
    noise.z = pd80FgPnoise3D(vec3(uvPix.xy, 3.0), timer, grainAdjust, grainSize);

    noise *= grainIntensity;
    noise = mix(noise.xxx, noise, grainColorAmt);
    noise =
        pow(abs(noise), vec3(max(11.0 - grainDensity, 0.1))) * sign(noise);
    noise = clamp((noise + 1.0) * 0.5, 0.0, 1.0);

    float depthLin = texture(sceneDepth, sampleUv).r;
    float depth = smoothstep(depthStart, depthEnd, depthLin);
    depth = pow(clamp(depth, 0.0, 1.0), depthCurve);
    float d = enable_depth ? depth : 1.0;

    vec3 testenv = (sampleUv.y < 0.25) ? sampleUv.xxx
        : (sampleUv.y < 0.5) ? vec3(sampleUv.x, 0.0, 0.0)
        : (sampleUv.y < 0.75) ? vec3(0.0, sampleUv.x, 0.0)
                              : vec3(0.0, 0.0, sampleUv.x);
    vec3 color = enable_test ? testenv : colorIn;

    vec3 origHSV = Pd80CiRgbToHsv(color.xyz);
    vec3 orig = color.xyz;
    float maxc = max(max(color.x, color.y), color.z);
    float minc = min(min(color.x, color.y), color.z);

    float lum = maxc;
    noise = mix(noise * grainIntLow, noise * grainIntHigh, pd80FgFade(lum));
    vec3 negnoise = -abs(noise);
    lum *= lum;
    negnoise = mix(noise, negnoise.zxy * 0.5, lum);
    noise = use_negnoise ? negnoise : noise;

    const float kFac = 1.2;
    float weight = max(1.0 - abs((origHSV.x - 0.166667) * 6.0), 0.0) * kFac;
    weight += max(1.0 - abs((origHSV.x - 0.333333) * 6.0), 0.0) / kFac;
    weight = clamp(pd80FgCurveScalar(weight / kFac), 0.0, 1.0);
    weight *= clamp((maxc + 1.0e-10 - minc) / (maxc + 1.0e-10), 0.0, 1.0);
    float adj = clamp((maxc - 0.2) * 1.25, 0.0, 1.0) + clamp(1.0 - maxc * 5.0, 0.0, 1.0);
    adj = 1.0 - pd80FgCurveScalar(adj);
    weight *= adj;

    float adjNoise = mix(1.0, 0.5, grainOrigColor * weight);
    color.xyz = mix(color.xyz, color.xyz + (noise.xyz * d), grainAmount * adjNoise);
    color.xyz = clamp(color.xyz, 0.0, 1.0);

    vec3 col = Pd80CbsBlendLuma(orig.xyz, color.xyz);
    color.xyz = grainOrigColor > 0.5 ? col.xyz : color.xyz;

    if (display_depth) {
        color.xyz = vec3(depthLin);
    }
    return mix(colorIn, color.xyz, master);
}

/// PD80_00_Color_Spaces.fxh — `HSVToRGB` (lolengine path).
vec3 Pd80DsHsvToRgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

/// PD80_00_Blend_Modes.fxh — helpers used only by `Pd80DsBlendmode`.
vec3 Pd80DsBlendColor(vec3 base, vec3 blend, vec3 lum) {
    float minbase = min(min(base.x, base.y), base.z);
    float maxbase = max(max(base.x, base.y), base.z);
    float satbase = maxbase - minbase;
    float minblend = min(min(blend.x, blend.y), blend.z);
    float maxblend = max(max(blend.x, blend.y), blend.z);
    float satblend = maxblend - minblend;
    vec3 color = (satbase > 0.0) ? (base - vec3(minbase)) * (satblend / satbase) : vec3(0.0);
    return Pd80CbsBlendLuma(color, lum);
}

vec3 Pd80DsBlendHue(vec3 c, vec3 b) {
    return Pd80DsBlendColor(b, c, c);
}
vec3 Pd80DsBlendSaturation(vec3 c, vec3 b) {
    return Pd80DsBlendColor(c, b, c);
}
vec3 Pd80DsBlendColorLum(vec3 c, vec3 b) {
    return Pd80CbsBlendLuma(b, c);
}
vec3 Pd80DsBlendLuminosity(vec3 c, vec3 b) {
    return Pd80CbsBlendLuma(c, b);
}

vec3 Pd80DsDarken(vec3 c, vec3 b) {
    return min(c, b);
}
vec3 Pd80DsMultiply(vec3 c, vec3 b) {
    return c * b;
}
vec3 Pd80DsLinearBurn(vec3 c, vec3 b) {
    return max(c + b - vec3(1.0), vec3(0.0));
}
vec3 Pd80DsColorBurn(vec3 c, vec3 b) {
    return vec3(
        b.x <= 0.0 ? b.x : clamp(1.0 - ((1.0 - c.x) / b.x), 0.0, 1.0),
        b.y <= 0.0 ? b.y : clamp(1.0 - ((1.0 - c.y) / b.y), 0.0, 1.0),
        b.z <= 0.0 ? b.z : clamp(1.0 - ((1.0 - c.z) / b.z), 0.0, 1.0));
}
vec3 Pd80DsLighten(vec3 c, vec3 b) {
    return max(b, c);
}
vec3 Pd80DsScreen(vec3 c, vec3 b) {
    return vec3(1.0) - (vec3(1.0) - c) * (vec3(1.0) - b);
}
vec3 Pd80DsColorDodge(vec3 c, vec3 b) {
    return vec3(
        b.x >= 1.0 ? b.x : clamp(c.x / (1.0 - b.x), 0.0, 1.0),
        b.y >= 1.0 ? b.y : clamp(c.y / (1.0 - b.y), 0.0, 1.0),
        b.z >= 1.0 ? b.z : clamp(c.z / (1.0 - b.z), 0.0, 1.0));
}
vec3 Pd80DsLinearDodge(vec3 c, vec3 b) {
    return min(c + b, vec3(1.0));
}
vec3 Pd80DsOverlay(vec3 c, vec3 b) {
    return vec3(
        c.x < 0.5 ? 2.0 * c.x * b.x : (1.0 - 2.0 * (1.0 - c.x) * (1.0 - b.x)),
        c.y < 0.5 ? 2.0 * c.y * b.y : (1.0 - 2.0 * (1.0 - c.y) * (1.0 - b.y)),
        c.z < 0.5 ? 2.0 * c.z * b.z : (1.0 - 2.0 * (1.0 - c.z) * (1.0 - b.z)));
}
vec3 Pd80DsSoftLight(vec3 c, vec3 b) {
    return vec3(
        b.x < 0.5 ? (2.0 * c.x * b.x + c.x * c.x * (1.0 - 2.0 * b.x))
                  : (sqrt(max(c.x, 0.0)) * (2.0 * b.x - 1.0) + 2.0 * c.x * (1.0 - b.x)),
        b.y < 0.5 ? (2.0 * c.y * b.y + c.y * c.y * (1.0 - 2.0 * b.y))
                  : (sqrt(max(c.y, 0.0)) * (2.0 * b.y - 1.0) + 2.0 * c.y * (1.0 - b.y)),
        b.z < 0.5 ? (2.0 * c.z * b.z + c.z * c.z * (1.0 - 2.0 * b.z))
                  : (sqrt(max(c.z, 0.0)) * (2.0 * b.z - 1.0) + 2.0 * c.z * (1.0 - b.z)));
}
vec3 Pd80DsVividLight(vec3 c, vec3 b) {
    vec3 low = Pd80DsColorBurn(c, b * 2.0);
    vec3 high = Pd80DsColorDodge(c, (b - vec3(0.5)) * 2.0);
    return mix(high, low, vec3(lessThan(b, vec3(0.5))));
}
vec3 Pd80DsLinearLight(vec3 c, vec3 b) {
    vec3 low = Pd80DsLinearBurn(c, b * 2.0);
    vec3 high = Pd80DsLinearDodge(c, (b - vec3(0.5)) * 2.0);
    return mix(high, low, vec3(lessThan(b, vec3(0.5))));
}
vec3 Pd80DsPinLight(vec3 c, vec3 b) {
    vec3 low = Pd80DsDarken(c, b * 2.0);
    vec3 high = Pd80DsLighten(c, (b - vec3(0.5)) * 2.0);
    return mix(high, low, vec3(lessThan(b, vec3(0.5))));
}
vec3 Pd80DsHardMix(vec3 c, vec3 b) {
    vec3 v = Pd80DsVividLight(c, b);
    return vec3(
        v.x < 0.5 ? 0.0 : 1.0,
        v.y < 0.5 ? 0.0 : 1.0,
        v.z < 0.5 ? 0.0 : 1.0);
}
vec3 Pd80DsReflect(vec3 c, vec3 b) {
    return vec3(
        b.x >= 1.0 ? b.x : clamp((c.x * c.x) / (1.0 - b.x), 0.0, 1.0),
        b.y >= 1.0 ? b.y : clamp((c.y * c.y) / (1.0 - b.y), 0.0, 1.0),
        b.z >= 1.0 ? b.z : clamp((c.z * c.z) / (1.0 - b.z), 0.0, 1.0));
}
vec3 Pd80DsGlow(vec3 c, vec3 b) {
    return Pd80DsReflect(b, c);
}

vec3 Pd80DsBlendmode(vec3 c, vec3 b, int mode, float o) {
    vec3 ret = b;
    switch (mode) {
    case 1:
        ret = Pd80DsDarken(c, b);
        break;
    case 2:
        ret = Pd80DsMultiply(c, b);
        break;
    case 3:
        ret = Pd80DsLinearBurn(c, b);
        break;
    case 4:
        ret = Pd80DsColorBurn(c, b);
        break;
    case 5:
        ret = Pd80DsLighten(c, b);
        break;
    case 6:
        ret = Pd80DsScreen(c, b);
        break;
    case 7:
        ret = Pd80DsColorDodge(c, b);
        break;
    case 8:
        ret = Pd80DsLinearDodge(c, b);
        break;
    case 9:
        ret = Pd80DsOverlay(c, b);
        break;
    case 10:
        ret = Pd80DsSoftLight(c, b);
        break;
    case 11:
        ret = Pd80DsVividLight(c, b);
        break;
    case 12:
        ret = Pd80DsLinearLight(c, b);
        break;
    case 13:
        ret = Pd80DsPinLight(c, b);
        break;
    case 14:
        ret = Pd80DsHardMix(c, b);
        break;
    case 15:
        ret = Pd80DsReflect(c, b);
        break;
    case 16:
        ret = Pd80DsGlow(c, b);
        break;
    case 17:
        ret = Pd80DsBlendHue(c, b);
        break;
    case 18:
        ret = Pd80DsBlendSaturation(c, b);
        break;
    case 19:
        ret = Pd80DsBlendColorLum(c, b);
        break;
    case 20:
        ret = Pd80DsBlendLuminosity(c, b);
        break;
    default:
        ret = b;
        break;
    }
    return clamp(mix(c, ret, o), 0.0, 1.0);
}

/// PD80_06_Depth_Slicer.fx — `PS_DepthSlice`.
vec3 ApplyPd80DepthSlicer(vec2 sampleUv, vec3 colorIn) {
    vec4 pk0 = cameraData.pd80DsPack0;
    float master = clamp(pk0.x, 0.0, 1.0);
    if (master < 1e-6) {
        return colorIn;
    }
    float depth_near = clamp(pk0.y, 0.0, 1.0);
    float depthpos = clamp(pk0.z, 0.0, 1.0);
    float depth_far = clamp(pk0.w, 0.0, 1.0);

    vec4 pk1 = cameraData.pd80DsPack1;
    float depth_smoothing = clamp(pk1.x, 0.0, 1.0);
    float intensity = clamp(pk1.y, 0.0, 1.0);
    float hue = clamp(pk1.z, 0.0, 1.0);
    float saturation = clamp(pk1.w, 0.0, 1.0);

    vec4 pk2 = cameraData.pd80DsPack2;
    int blendmode = clamp(int(pk2.x + 0.5), 0, 20);
    float opacity = clamp(pk2.y, 0.0, 1.0);

    vec3 color = colorIn;
    float depth = clamp(texture(sceneDepth, sampleUv).r, 0.0, 1.0);

    float depth_np = depthpos - depth_near;
    float depth_fp = depthpos + depth_far;
    float dn = smoothstep(depth_np - depth_smoothing, depth_np, depth);
    float df = 1.0 - smoothstep(depth_fp, depth_fp + depth_smoothing, depth);

    float colorize = 1.0 - (dn * df);
    float a = colorize;
    colorize *= intensity;
    vec3 bh = Pd80DsHsvToRgb(vec3(hue, saturation, colorize));
    color.xyz = Pd80DsBlendmode(color.xyz, bh, blendmode, opacity * a);
    return mix(colorIn, color.xyz, master);
}

/// PD80_01_Color_Gamut.fx — `LinearTosRGB` / `SRGBToLinear` (exact HLSL branches).
vec3 Pd80CgSRGBToLinear(vec3 color) {
    vec3 x = color / 12.92;
    vec3 y = pow(max((color + 0.055) / 1.055, 0.0), vec3(2.4));
    return vec3(
        color.r <= 0.04045 ? x.r : y.r,
        color.g <= 0.04045 ? x.g : y.g,
        color.b <= 0.04045 ? x.b : y.b);
}

vec3 Pd80CgLinearTosRGB(vec3 color) {
    vec3 x = color * 12.92;
    vec3 y = 1.055 * pow(clamp(color, 0.0, 1.0), vec3(1.0 / 2.4)) - 0.055;
    return vec3(
        color.r < 0.0031308 ? x.r : y.r,
        color.g < 0.0031308 ? x.g : y.g,
        color.b < 0.0031308 ? x.b : y.b);
}

mat3 Pd80CgRefWhiteMat(int colorgamut) {
    if (colorgamut == 3 || colorgamut == 4 || colorgamut == 7 || colorgamut == 8 || colorgamut == 9
        || colorgamut == 10 || colorgamut == 13 || colorgamut == 15) {
        return mat3(
            1.0478112, 0.0295424, -0.0092345,
            0.0228866, 0.9904844, 0.0150436,
            -0.0501270, -0.0170491, 0.7521316);
    }
    if (colorgamut == 6) {
        return mat3(
            1.0502614, 0.0390650, -0.0024047,
            0.0270757, 0.9729502, 0.0026446,
            -0.0232523, -0.0092579, 0.918087);
    }
    if (colorgamut == 11) {
        return mat3(
            1.0097788, 0.0123113, 0.0038284,
            0.0070419, 0.9847094, -0.0072331,
            0.0127971, 0.0032962, 1.0891639);
    }
    return mat3(1.0);
}

mat3 Pd80CgGamutMat(int colorgamut) {
    switch (colorgamut) {
    case 1:
        return mat3(
            2.0413690, -0.9692660, 0.0134474,
            -0.5649464, 1.8760108, -0.1183897,
            -0.3446944, 0.0415560, 1.0154096);
    case 2:
        return mat3(
            2.9515373, -1.0851093, 0.0854934,
            -1.2894116, 1.9908566, -0.2694964,
            -0.4738445, 0.0372026, 1.0912975);
    case 3:
        return mat3(
            1.7552599, -0.5441336, 0.0063467,
            -0.4836786, 1.5068789, -0.0175761,
            -0.2530000, 0.0215528, 1.2256959);
    case 4:
        return mat3(
            1.6832270, -0.7710229, 0.0400013,
            -0.4282363, 1.7065571, -0.0885376,
            -0.2360185, 0.0446900, 1.2723640);
    case 5:
        return mat3(
            2.7454669, -0.9692660, 0.0112723,
            -1.1358136, 1.8760108, -0.1139754,
            -0.4350269, 0.0415560, 1.0132541);
    case 6:
        return mat3(
            2.3706743, -0.5138850, 0.0052982,
            -0.9000405, 1.4253036, -0.0146949,
            -0.4706338, 0.0885814, 1.0093968);
    case 7:
        return mat3(
            2.6422874, -1.1119763, 0.0821699,
            -1.2234270, 2.0590183, -0.2807254,
            -0.3930143, 0.0159614, 1.4559877);
    case 8:
        return mat3(
            1.7603900, -0.7126288, 0.0078207,
            -0.4881198, 1.6527432, -0.0347411,
            -0.2536126, 0.0416715, 1.2447743);
    case 9:
        return mat3(
            1.7827618, -0.9593623, 0.0859317,
            -0.4969847, 1.9477962, -0.1744674,
            -0.2690101, -0.0275807, 1.3228273);
    case 10:
        return mat3(
            2.0043819, -0.7110285, 0.0381263,
            -0.7304844, 1.6202126, -0.0868780,
            -0.2450052, 0.0792227, 1.2725438);
    case 11:
        return mat3(
            1.9099960, -0.9846663, 0.0583056,
            -0.5324542, 1.9991710, -0.1183781,
            -0.2882091, -0.0283082, 0.8975535);
    case 12:
        return mat3(
            3.0628971, -0.9692660, 0.0678775,
            -1.3931791, 1.8760108, -0.2288548,
            -0.4757517, 0.0415560, 1.0693490);
    case 13:
        return mat3(
            1.3459433, -0.5445989, 0.0,
            -0.2556075, 1.5081673, 0.0,
            -0.0511118, 0.0205351, 1.2118128);
    case 14:
        return mat3(
            3.5053960, -1.0690722, 0.0563200,
            -1.7394894, 1.9778245, -0.1970226,
            -0.5439640, 0.0351722, 1.0502026);
    case 15:
        return mat3(
            1.4628067, -0.5217933, 0.0349342,
            -0.1840623, 1.4472381, -0.0968930,
            -0.2743606, 0.0677227, 1.2884099);
    default:
        return mat3(
            3.2404542, -0.9692660, 0.0556434,
            -1.5371385, 1.8760108, -0.2040259,
            -0.4985314, 0.0415560, 1.0572252);
    }
}

vec3 ApplyPd80ColorGamut(vec3 colorIn) {
    vec4 pk = cameraData.pd80CgPack0;
    float master = clamp(pk.x, 0.0, 1.0);
    if (master < 1e-6) {
        return colorIn;
    }
    int colorgamut = clamp(int(pk.y + 0.5), 0, 15);
    vec3 color = clamp(colorIn, 0.0, 1.0);
    vec3 linear = Pd80CgSRGBToLinear(color);
    mat3 sRgbToXyz = mat3(
        0.4124564, 0.2126729, 0.0193339,
        0.3575761, 0.7151522, 0.1191920,
        0.1804375, 0.0721750, 0.9503041);
    vec3 xyz = sRgbToXyz * linear;
    mat3 gamut = Pd80CgGamutMat(colorgamut);
    mat3 refWh = Pd80CgRefWhiteMat(colorgamut);
    vec3 conv = gamut * (refWh * xyz);
    vec3 outc = Pd80CgLinearTosRGB(clamp(conv, 0.0, 1.0));
    return mix(colorIn, clamp(outc, 0.0, 1.0), master);
}

/// PD80_03_Color_Space_Curves.fx — `PS_CSCurves` with the native PD80 texture bundle.
struct Pd80CscTonemapParams {
    vec3 mToe;
    vec2 mMid;
    vec3 mShoulder;
    vec2 mBx;
};

vec4 Pd80CscSetBoundaries(float tx, float ty, float sx, float sy) {
    tx = min(tx, sx);
    ty = min(ty, sy);
    return vec4(tx, ty, sx, sy);
}

Pd80CscTonemapParams Pd80CscPrepareTonemap(vec2 p1, vec2 p2, vec2 p3) {
    Pd80CscTonemapParams tc;
    float denom = p2.x - p1.x;
    denom = abs(denom) > 1e-5 ? denom : 1e-5;
    float slope = (p2.y - p1.y) / denom;
    tc.mMid = vec2(slope, p1.y - slope * p1.x);
    {
        float dnom = p1.y - slope * p1.x;
        dnom = abs(dnom) > 1e-5 ? dnom : 1e-5;
        tc.mToe.x = slope * p1.x * p1.x * p1.y * p1.y / (dnom * dnom);
        tc.mToe.y = slope * p1.x * p1.x / dnom;
        tc.mToe.z = p1.y * p1.y / dnom;
    }
    {
        float dnom = slope * (p2.x - p3.x) - p2.y + p3.y;
        dnom = abs(dnom) > 1e-5 ? dnom : 1e-5;
        tc.mShoulder.x = slope * pow(p2.x - p3.x, 2.0) * pow(p2.y - p3.y, 2.0) / (dnom * dnom);
        tc.mShoulder.y = (slope * p2.x * (p3.x - p2.x) + p3.x * (p2.y - p3.y)) / dnom;
        tc.mShoulder.z = (-p2.y * p2.y + p3.y * (slope * (p2.x - p3.x) + p2.y)) / dnom;
    }
    tc.mBx = vec2(p1.x, p2.x);
    return tc;
}

vec3 Pd80CscTonemap(Pd80CscTonemapParams tc, vec3 x) {
    vec3 toe = -tc.mToe.x / (x + tc.mToe.y) + tc.mToe.z;
    vec3 mid = tc.mMid.x * x + tc.mMid.y;
    vec3 shoulder = -tc.mShoulder.x / (x + tc.mShoulder.y) + tc.mShoulder.z;
    vec3 result = mix(toe, mid, greaterThanEqual(x, vec3(tc.mBx.x)));
    result = mix(result, shoulder, greaterThanEqual(x, vec3(tc.mBx.y)));
    return result;
}

vec3 Pd80CscHueToRgb(float H) {
    return clamp(
        vec3(abs(H * 6.0 - 3.0) - 1.0, 2.0 - abs(H * 6.0 - 2.0), 2.0 - abs(H * 6.0 - 4.0)),
        0.0,
        1.0);
}

vec3 Pd80CscRgbToHcv(vec3 RGB) {
    vec4 P = (RGB.g < RGB.b) ? vec4(RGB.b, RGB.g, -1.0, 2.0 / 3.0) : vec4(RGB.g, RGB.b, 0.0, -1.0 / 3.0);
    vec4 Q1 = (RGB.r < P.x) ? vec4(P.xyw, RGB.r) : vec4(RGB.r, P.yzx);
    float C = Q1.x - min(Q1.w, Q1.y);
    float H = abs((Q1.w - Q1.y) / (6.0 * C + 0.000001) + Q1.z);
    return vec3(H, C, Q1.x);
}

vec3 Pd80CscRgbToHsl(vec3 RGB) {
    RGB = max(RGB, vec3(0.000001));
    vec3 HCV = Pd80CscRgbToHcv(RGB);
    float L = HCV.z - HCV.y * 0.5;
    float S = HCV.y / (1.0 - abs(L * 2.0 - 1.0) + 0.000001);
    return vec3(HCV.x, S, L);
}

vec3 Pd80CscHslToRgb(vec3 HSL) {
    vec3 RGB = Pd80CscHueToRgb(HSL.x);
    float C = (1.0 - abs(2.0 * HSL.z - 1.0)) * HSL.y;
    return (RGB - 0.5) * C + HSL.z;
}

vec3 Pd80CscSrgbToXyz(vec3 c) {
    mat3 m = mat3(
        0.4124564, 0.2126729, 0.0193339,
        0.3575761, 0.7151522, 0.1191920,
        0.1804375, 0.0721750, 0.9503041);
    return m * c;
}

vec3 Pd80CscXyzToLinearRgb(vec3 c) {
    mat3 m = mat3(
        3.2404542, -0.9692660, 0.0556434,
        -1.5371385, 1.8760108, -0.2040259,
        -0.4985314, 0.0415560, 1.0572252);
    return m * c;
}

vec3 Pd80CscXyzToLab(vec3 c) {
    const vec3 ref = vec3(0.95047, 1.0, 1.08883);
    const float K = 24389.0 / 27.0;
    const float E = 216.0 / 24389.0;
    vec3 w = max(c / ref, 0.0);
    vec3 v;
    v.x = (w.x > E) ? pow(w.x, 1.0 / 3.0) : (K * w.x + 16.0) / 116.0;
    v.y = (w.y > E) ? pow(w.y, 1.0 / 3.0) : (K * w.y + 16.0) / 116.0;
    v.z = (w.z > E) ? pow(w.z, 1.0 / 3.0) : (K * w.z + 16.0) / 116.0;
    return vec3(116.0 * v.y - 16.0, 500.0 * (v.x - v.y), 200.0 * (v.y - v.z));
}

vec3 Pd80CscLabToXyz(vec3 c) {
    const vec3 ref = vec3(0.95047, 1.0, 1.08883);
    const float K = 24389.0 / 27.0;
    const float E = 216.0 / 24389.0;
    vec3 v;
    v.y = (c.x + 16.0) / 116.0;
    v.x = c.y / 500.0 + v.y;
    v.z = v.y - c.z / 200.0;
    vec3 o = vec3(
        (v.x * v.x * v.x > E) ? v.x * v.x * v.x : (116.0 * v.x - 16.0) / K,
        (c.x > K * E) ? v.y * v.y * v.y : c.x / K,
        (v.z * v.z * v.z > E) ? v.z * v.z * v.z : (116.0 * v.z - 16.0) / K);
    return o * ref;
}

vec3 Pd80CscSrgbToLab(vec3 c) {
    vec3 lab = Pd80CscXyzToLab(Pd80CscSrgbToXyz(c));
    return lab / vec3(100.0, 108.0, 108.0);
}

vec3 Pd80CscLabToSrgb(vec3 c) {
    const vec3 ref = vec3(0.95047, 1.0, 1.08883);
    vec3 xyz = clamp(Pd80CscLabToXyz(c * vec3(100.0, 108.0, 108.0)), vec3(0.0), ref);
    return clamp(Pd80CscXyzToLinearRgb(xyz), 0.0, 1.0);
}

vec4 Pd80CscDither(vec2 uv, bool en, float str, float timeSec) {
    if (!en) {
        return vec4(0.0);
    }
    // Match the native PD80 sampler contract: one 512x512 blue-noise tile in screen
    // pixel space, wrapped by the Vulkan sampler. Keeping this resource-driven avoids
    // the directional artifacts and frame correlation of the former sine hash.
    vec2 noiseUv = uv * cameraData.viewportMetrics.xy / 512.0;
    float mot = timeSec + 1.0;
    vec4 noise = texture(nativePd80BlueNoiseTexture, noiseUv);
    noise = fract(noise + 0.61803398875 * mot);
    noise = (noise * 2.0 - 1.0) * 0.5;
    return noise * (str / 255.0);
}

vec3 ApplyPd80ColorSpaceCurves(vec2 sampleUv, vec3 colorIn) {
    vec4 p0 = cameraData.pd80CscPack0;
    float master = clamp(p0.x, 0.0, 1.0);
    if (master < 1e-6) {
        return colorIn;
    }
    bool enable_dither = p0.y > 0.5;
    float dither_strength = clamp(p0.z, 0.0, 10.0);
    int color_space = clamp(int(p0.w + 0.5), 0, 3);
    vec4 p1 = cameraData.pd80CscPack1;
    float colorsat = clamp(cameraData.pd80CscPack2.x, -1.0, 1.0);

    float timer = cameraData.postProcessSecondary.z;
    vec4 dnoise = Pd80CscDither(sampleUv, enable_dither, dither_strength, timer);
    vec3 color = clamp(colorIn + dnoise.xyz, 0.0, 1.0);

    vec4 grey = Pd80CscSetBoundaries(p1.x, p1.y, p1.z, p1.w);
    Pd80CscTonemapParams tc = Pd80CscPrepareTonemap(grey.xy, grey.zw, vec2(1.0, 1.0));

    float rgb_luma = min(min(color.x, color.y), color.z);
    vec3 rgb_chroma = color - rgb_luma;
    rgb_luma = Pd80CscTonemap(tc, vec3(rgb_luma)).x;
    rgb_chroma *= (colorsat + 1.0);

    vec3 lab_color = Pd80CscSrgbToLab(color);
    lab_color.x = Pd80CscTonemap(tc, vec3(lab_color.x)).x;
    lab_color.yz *= (colorsat + 1.0);

    vec3 hsl_color = Pd80CscRgbToHsl(color);
    hsl_color.z = Pd80CscTonemap(tc, vec3(hsl_color.z)).z;
    hsl_color.y *= (colorsat + 1.0);

    vec3 hsv_color = Pd80CiRgbToHsv(color);
    hsv_color.z = Pd80CscTonemap(tc, vec3(hsv_color.z)).z;
    hsv_color.y *= (colorsat + 1.0);

    if (color_space == 0) {
        color = clamp(rgb_chroma + rgb_luma, 0.0, 1.0);
    } else if (color_space == 1) {
        color = Pd80CscLabToSrgb(lab_color);
    } else if (color_space == 2) {
        color = Pd80CscHslToRgb(clamp(hsl_color, 0.0, 1.0));
    } else {
        color = Pd80DsHsvToRgb(clamp(hsv_color, 0.0, 1.0));
    }

    color = clamp(color + vec3(dnoise.w, dnoise.x, dnoise.y), 0.0, 1.0);
    return mix(colorIn, color, master);
}

/// PD80_03_Shadows_Midtones_Highlights.fx — `PS_SMH` (dither precedes luma weights; `PD80_00_Base_Effects` + `blendmode`).
float Pd80SmhCurve(float x) {
    return x * x * x * (x * (x * 6.0 - 15.0) + 10.0);
}

vec3 Pd80SmhApplyBand(
    vec3 color,
    float w,
    float ex,
    float co,
    float br,
    vec3 bcol,
    int bmode,
    float op,
    float tint,
    float sat,
    float vib,
    vec3 cold,
    vec3 warm) {
    color = Pd80CbsExposure(color, ex * w);
    color = Pd80CbsCon(color, co * w);
    color = Pd80CbsBri(color, br * w);
    color = Pd80DsBlendmode(color, bcol, bmode, op * w);
    if (tint < 0.0) {
        color = mix(color, Pd80CbsSoftLight(color, cold), abs(tint * w));
    } else {
        color = mix(color, Pd80CbsSoftLight(color, warm), tint * w);
    }
    color = Pd80CbsSat(color, sat * w);
    color = Pd80CbsVib(color, vib * w);
    return color;
}

vec3 ApplyPd80ShadowsMidtonesHighlights(vec2 sampleUv, vec3 colorIn) {
    vec4 pk0 = cameraData.pd80SmhPack0;
    float master = clamp(pk0.x, 0.0, 1.0);
    if (master < 1e-6) {
        return colorIn;
    }
    int luma_mode = clamp(int(pk0.y + 0.5), 0, 2);
    int separation_mode = clamp(int(pk0.z + 0.5), 0, 1);
    bool use_dither = pk0.w > 0.5;
    float dith_str = clamp(cameraData.pd80SmhPack1.x, 0.0, 10.0);

    vec3 color = clamp(colorIn, 0.0, 1.0);
    if (use_dither) {
        vec2 dUv = sampleUv * cameraData.viewportMetrics.xy / 512.0;
        float mot = fract(cameraData.postProcessSecondary.z * 12.9898 + 93.0);
        vec3 dn = vec3(
            Hash21(dUv + vec2(4.2, 1.1)),
            Hash21(dUv + vec2(19.4, 7.7)),
            Hash21(dUv + vec2(3.8, 22.2)));
        dn = fract(dn + 0.61803398875 * mot * 128.0);
        dn = (dn * 2.0 - 1.0) * 0.5;
        dn *= (dith_str / 255.0);
        color = clamp(color + dn, 0.0, 1.0);
    }

    float pLuma = 0.0;
    if (luma_mode == 0) {
        pLuma = dot(color, vec3(0.333333, 0.333334, 0.333333));
    } else if (luma_mode == 1) {
        pLuma = Pd80TcGetLuminance(color);
    } else {
        pLuma = max(max(color.x, color.y), color.z);
    }

    float weight_s;
    float weight_h;
    float weight_m;
    if (separation_mode == 0) {
        weight_s = Pd80SmhCurve(max(1.0 - pLuma * 2.0, 0.0));
        weight_h = Pd80SmhCurve(max((pLuma - 0.5) * 2.0, 0.0));
        weight_m = clamp(1.0 - weight_s - weight_h, 0.0, 1.0);
    } else {
        weight_s = pow(1.0 - pLuma, 4.0);
        weight_h = pow(pLuma, 4.0);
        weight_m = clamp(1.0 - weight_s - weight_h, 0.0, 1.0);
    }

    vec3 cold = vec3(0.0, 0.365, 1.0);
    vec3 warm = vec3(0.98, 0.588, 0.0);

    vec4 s2 = cameraData.pd80SmhPack2;
    vec4 s3 = cameraData.pd80SmhPack3;
    vec4 s4 = cameraData.pd80SmhPack4;
    color = Pd80SmhApplyBand(
        color,
        weight_s,
        s2.x,
        s2.y,
        s2.z,
        clamp(s3.xyz, 0.0, 1.0),
        clamp(int(s3.w + 0.5), 0, 20),
        s2.w,
        s4.x,
        s4.y,
        s4.z,
        cold,
        warm);

    vec4 m5 = cameraData.pd80SmhPack5;
    vec4 m6 = cameraData.pd80SmhPack6;
    vec4 m7 = cameraData.pd80SmhPack7;
    color = Pd80SmhApplyBand(
        color,
        weight_m,
        m5.x,
        m5.y,
        m5.z,
        clamp(m6.xyz, 0.0, 1.0),
        clamp(int(m6.w + 0.5), 0, 20),
        m5.w,
        m7.x,
        m7.y,
        m7.z,
        cold,
        warm);

    vec4 h8 = cameraData.pd80SmhPack8;
    vec4 h9 = cameraData.pd80SmhPack9;
    vec4 h10 = cameraData.pd80SmhPack10;
    color = Pd80SmhApplyBand(
        color,
        weight_h,
        h8.x,
        h8.y,
        h8.z,
        clamp(h9.xyz, 0.0, 1.0),
        clamp(int(h9.w + 0.5), 0, 20),
        h8.w,
        h10.x,
        h10.y,
        h10.z,
        cold,
        warm);

    return mix(colorIn, color, master);
}

/// SweetFX FakeHDR.fx — eight-offset rings at radius1/radius2 with weights 0.005 / 0.010.
vec3 ApplySweetFxFakeHdr(vec2 sampleUv, vec2 px, vec3 centerColor, vec4 fh) {
    float strength = fh.w;
    if (strength < 1e-6) {
        return centerColor;
    }
    float HDRPower = fh.x;
    float radius1 = fh.y;
    float radius2 = fh.z;
    vec2 ps = px;

    vec3 bloom_sum1 = EvaluatePrePostProcessChainCore(sampleUv + vec2(1.5, -1.5) * radius1 * ps, px);
    bloom_sum1 += EvaluatePrePostProcessChainCore(sampleUv + vec2(-1.5, -1.5) * radius1 * ps, px);
    bloom_sum1 += EvaluatePrePostProcessChainCore(sampleUv + vec2(1.5, 1.5) * radius1 * ps, px);
    bloom_sum1 += EvaluatePrePostProcessChainCore(sampleUv + vec2(-1.5, 1.5) * radius1 * ps, px);
    bloom_sum1 += EvaluatePrePostProcessChainCore(sampleUv + vec2(0.0, -2.5) * radius1 * ps, px);
    bloom_sum1 += EvaluatePrePostProcessChainCore(sampleUv + vec2(0.0, 2.5) * radius1 * ps, px);
    bloom_sum1 += EvaluatePrePostProcessChainCore(sampleUv + vec2(-2.5, 0.0) * radius1 * ps, px);
    bloom_sum1 += EvaluatePrePostProcessChainCore(sampleUv + vec2(2.5, 0.0) * radius1 * ps, px);
    bloom_sum1 *= 0.005;

    vec3 bloom_sum2 = EvaluatePrePostProcessChainCore(sampleUv + vec2(1.5, -1.5) * radius2 * ps, px);
    bloom_sum2 += EvaluatePrePostProcessChainCore(sampleUv + vec2(-1.5, -1.5) * radius2 * ps, px);
    bloom_sum2 += EvaluatePrePostProcessChainCore(sampleUv + vec2(1.5, 1.5) * radius2 * ps, px);
    bloom_sum2 += EvaluatePrePostProcessChainCore(sampleUv + vec2(-1.5, 1.5) * radius2 * ps, px);
    bloom_sum2 += EvaluatePrePostProcessChainCore(sampleUv + vec2(0.0, -2.5) * radius2 * ps, px);
    bloom_sum2 += EvaluatePrePostProcessChainCore(sampleUv + vec2(0.0, 2.5) * radius2 * ps, px);
    bloom_sum2 += EvaluatePrePostProcessChainCore(sampleUv + vec2(-2.5, 0.0) * radius2 * ps, px);
    bloom_sum2 += EvaluatePrePostProcessChainCore(sampleUv + vec2(2.5, 0.0) * radius2 * ps, px);
    bloom_sum2 *= 0.010;

    float dist = radius2 - radius1;
    vec3 color = centerColor;
    vec3 HDR = (color + (bloom_sum2 - bloom_sum1)) * dist;
    vec3 blend = HDR + color;
    float pw = abs(HDRPower);
    vec3 outc = pow(abs(blend), vec3(pw)) + HDR;
    outc = clamp(outc, 0.0, 1.0);
    return mix(centerColor, outc, strength);
}

/// SweetFX Levels.fx v1.2 — linear remap using UI black/white (0–255); optional clipping debug colors.
vec3 ApplySweetFxLevels(vec3 color, vec4 lv) {
    float bp = lv.x;
    float wp = lv.y;
    float black_point_float = bp / 255.0;
    float wpd = wp - bp;
    float white_point_float = (abs(wpd) < 1e-5) ? (255.0 / 0.00025) : (255.0 / wpd);
    vec3 leveled = color * white_point_float - (black_point_float * white_point_float);

    if (lv.w > 0.5) {
        vec3 sat = clamp(leveled, 0.0, 1.0);
        vec3 clipped_colors = any(greaterThan(leveled, sat)) ? vec3(1.0, 0.0, 0.0) : leveled;
        clipped_colors = all(greaterThan(leveled, sat)) ? vec3(1.0, 1.0, 0.0) : clipped_colors;
        clipped_colors = any(lessThan(leveled, sat)) ? vec3(0.0, 0.0, 1.0) : clipped_colors;
        clipped_colors = all(lessThan(leveled, sat)) ? vec3(0.0, 1.0, 1.0) : clipped_colors;
        return clipped_colors;
    }
    float str = lv.z;
    if (str < 1e-6) {
        return color;
    }
    return mix(color, leveled, str);
}

/// SweetFX Curves.fx — Mode × Formula (11 curves); `sweetFxCurvesStrength` mixes toward identity.
vec3 ApplySweetFxCurvesFull(vec3 colorInput, vec4 pk) {
    float mixAmt = pk.w;
    if (mixAmt < 1e-6) {
        return colorInput;
    }
    float contrast = clamp(pk.x, -1.0, 1.0);
    int mode = clamp(int(pk.y + 0.5), 0, 2);
    int formula = clamp(int(pk.z + 0.5), 0, 10);

    vec3 lumCoeff = vec3(0.2126, 0.7152, 0.0722);
    float luma = dot(lumCoeff, colorInput);
    vec3 chroma = colorInput - vec3(luma);
    float contrastBlend = contrast;

    vec3 x;
    if (mode == 0) {
        x = vec3(luma);
    } else if (mode == 1) {
        x = chroma * 0.5 + 0.5;
    } else {
        x = colorInput;
    }

    const float PI = 3.1415927;

    if (formula == 0) {
        x = sin(PI * 0.5 * x);
        x *= x;
    } else if (formula == 1) {
        x = x - 0.5;
        x = (x / (0.5 + abs(x))) + 0.5;
    } else if (formula == 2) {
        x = x * x * (3.0 - 2.0 * x);
    } else if (formula == 3) {
        x = (1.0524 * exp(6.0 * x) - vec3(1.05248)) / (exp(6.0 * x) + vec3(20.0855));
    } else if (formula == 4) {
        x = x * (x * (1.5 - x) + 0.5);
        contrastBlend = contrast * 2.0;
    } else if (formula == 5) {
        x = x*x*x*(x*(x*6.0 - 15.0) + 10.0);
    } else if (formula == 6) {
        x = x - 0.5;
        x = x / ((abs(x) * 1.25) + vec3(0.375)) + 0.5;
    } else if (formula == 7) {
        x = (x * (x * (x * (x * (x * (x * (1.6 * x - 7.2) + 10.8) - 4.2) - 3.6) + 2.7) - 1.8) + 2.7) * x * x;
    } else if (formula == 8) {
        x = -0.5 * (x * 2.0 - 1.0) * (abs(x * 2.0 - 1.0) - 2.0) + 0.5;
    } else if (formula == 9) {
        vec3 xstep = step(x, vec3(0.5));
        vec3 xstep_shift = xstep - 0.5;
        vec3 shifted_x = x + xstep_shift;
        vec3 inner = -shifted_x * shifted_x + shifted_x;
        x = abs(xstep - sqrt(max(inner, vec3(0.0)))) - xstep_shift;
        contrastBlend = contrast * 0.5;
    } else {
        vec3 a = x * x * 2.0;
        vec3 b = (2.0 * -x + 4.0) * x - 1.0;
        x = mix(b, a, lessThan(x, vec3(0.5)));
    }

    vec3 result;
    if (mode == 0) {
        x = mix(vec3(luma), x, contrastBlend);
        result = x + chroma;
    } else if (mode == 1) {
        x = x * 2.0 - 1.0;
        vec3 color = vec3(luma) + x;
        result = mix(colorInput, color, contrastBlend);
    } else {
        result = mix(colorInput, x, contrastBlend);
    }

    return mix(colorInput, result, mixAmt);
}

/// Tap path matching ReShade BackBuffer after prior SweetFX stages (FakeHDR → Levels).
vec3 EvalSweetFxStack(vec2 uv, vec2 px) {
    vec3 base = EvaluatePrePostProcessChainCore(uv, px);
    base = ApplySweetFxFakeHdr(uv, px, base, cameraData.fakeHdrPowerR1R2Str);
    base = ApplySweetFxLevels(base, cameraData.levelsBlackWhiteStrClip);
    return base;
}

vec3 EvalSweetFxCurvesAt(vec2 uv, vec2 px) {
    return ApplySweetFxCurvesFull(EvalSweetFxStack(uv, px), cameraData.sweetFxCurvesPack);
}

/// SweetFX ChromaticAberration.fx — R/B from ± `BUFFER_PIXEL_SIZE * Shift`, G from center; Strength lerps vs identity.
vec3 ApplySweetFxChromaticAberrationFx(vec2 uv, vec2 px, vec4 caPack) {
    float str = clamp(caPack.z, 0.0, 1.0);
    vec2 shift = caPack.xy;
    if (str < 1e-6) {
        return EvalSweetFxCurvesAt(uv, px);
    }
    vec3 colorInput = EvalSweetFxCurvesAt(uv, px);
    vec3 color;
    color.r = EvalSweetFxCurvesAt(uv + px * shift, px).r;
    color.g = colorInput.g;
    color.b = EvalSweetFxCurvesAt(uv - px * shift, px).b;
    return mix(colorInput, color, str);
}

vec3 EvalPostStackNoSharpen(vec2 uv, vec2 px) {
    return ApplySweetFxChromaticAberrationFx(uv, px, cameraData.sweetFxChromaticAberrationPack);
}

/// SweetFX LumaSharpen.fx v1.5 — taps match Curves+stack ordering before sharpen.
vec3 ApplySweetFxLumaSharpen(vec2 sampleUv, vec2 px, vec3 ori, vec4 lp) {
    float sharp_strength = lp.x;
    if (sharp_strength < 1e-6) {
        return ori;
    }
    float sharp_clamp = max(lp.y, 1e-5);
    float offset_bias = lp.z;
    int packedPat = int(lp.w + 0.5);
    bool showSharp = packedPat >= 4;
    int pattern = showSharp ? (packedPat - 4) : packedPat;

    vec3 sharp_strength_luma = vec3(0.2126, 0.7152, 0.0722) * sharp_strength;
    vec3 blur_ori = vec3(0.0);

    if (pattern == 0) {
        blur_ori = EvalPostStackNoSharpen(sampleUv + (px / 3.0) * offset_bias, px);
        blur_ori += EvalPostStackNoSharpen(sampleUv + (-px / 3.0) * offset_bias, px);
        blur_ori *= 0.5;
        sharp_strength_luma *= 1.5;
    } else if (pattern == 1) {
        blur_ori = EvalPostStackNoSharpen(sampleUv + vec2(px.x, -px.y) * 0.5 * offset_bias, px);
        blur_ori += EvalPostStackNoSharpen(sampleUv - px * 0.5 * offset_bias, px);
        blur_ori += EvalPostStackNoSharpen(sampleUv + px * 0.5 * offset_bias, px);
        blur_ori += EvalPostStackNoSharpen(sampleUv - vec2(px.x, -px.y) * 0.5 * offset_bias, px);
        blur_ori *= 0.25;
    } else if (pattern == 2) {
        blur_ori = EvalPostStackNoSharpen(sampleUv + px * vec2(0.4, -1.2) * offset_bias, px);
        blur_ori += EvalPostStackNoSharpen(sampleUv - px * vec2(1.2, 0.4) * offset_bias, px);
        blur_ori += EvalPostStackNoSharpen(sampleUv + px * vec2(1.2, 0.4) * offset_bias, px);
        blur_ori += EvalPostStackNoSharpen(sampleUv - px * vec2(0.4, -1.2) * offset_bias, px);
        blur_ori *= 0.25;
        sharp_strength_luma *= 0.51;
    } else {
        blur_ori = EvalPostStackNoSharpen(sampleUv + vec2(0.5 * px.x, -px.y * offset_bias), px);
        blur_ori += EvalPostStackNoSharpen(sampleUv + vec2(offset_bias * -px.x, 0.5 * -px.y), px);
        blur_ori += EvalPostStackNoSharpen(sampleUv + vec2(offset_bias * px.x, 0.5 * px.y), px);
        blur_ori += EvalPostStackNoSharpen(sampleUv + vec2(0.5 * -px.x, px.y * offset_bias), px);
        blur_ori *= 0.25;
        sharp_strength_luma *= 0.666;
    }

    vec3 sharp = ori - blur_ori;
    vec4 sharp_strength_luma_clamp = vec4(sharp_strength_luma * (0.5 / sharp_clamp), 0.5);
    float sharp_luma = clamp(dot(vec4(sharp, 1.0), sharp_strength_luma_clamp), 0.0, 1.0);
    sharp_luma = (sharp_clamp * 2.0) * sharp_luma - sharp_clamp;

    vec3 outputcolor = ori + vec3(sharp_luma);
    if (showSharp) {
        float v = clamp(0.5 + (sharp_luma * 4.0), 0.0, 1.0);
        outputcolor = vec3(v);
    }
    return clamp(outputcolor, vec3(0.0), vec3(1.0));
}

/// Sampling path matching ReShade BackBuffer before Cartoon (after stack + LumaSharpen + engine `ApplyPostProcessFx`).
vec3 SweetFxRgbBeforeCartoon(vec2 uv, vec2 px) {
    vec3 base = EvalPostStackNoSharpen(uv, px);
    vec3 sharp = ApplySweetFxLumaSharpen(uv, px, base, cameraData.lumaSharpenPack);
    return ApplyPostProcessFx(sharp, uv);
}

/// SweetFX Cartoon.fx — diagonal luma differences; `pow(abs(edge), EdgeSlope) * (-Power) + color`.
vec3 ApplySweetFxCartoon(vec3 color, vec2 uv, vec2 px, vec4 pack) {
    float str = clamp(pack.z, 0.0, 1.0);
    if (str < 1e-6) {
        return color;
    }
    float power = max(pack.x, 1e-4);
    float edgeSlope = max(pack.y, 1e-4);
    vec3 coefLuma = vec3(0.2126, 0.7152, 0.0722);

    vec3 rgbDiagP = SweetFxRgbBeforeCartoon(uv + px, px);
    float diff1A = dot(coefLuma, rgbDiagP);
    vec3 rgbDiagN = SweetFxRgbBeforeCartoon(uv - px, px);
    float diff1 = dot(coefLuma, rgbDiagN) - diff1A;

    vec2 offA = px * vec2(1.0, -1.0);
    vec2 offB = px * vec2(-1.0, 1.0);
    vec3 rgbA = SweetFxRgbBeforeCartoon(uv + offA, px);
    float diff2A = dot(coefLuma, rgbA);
    vec3 rgbB = SweetFxRgbBeforeCartoon(uv + offB, px);
    float diff2 = dot(coefLuma, rgbB) - diff2A;

    float edge = diff1 * diff1 + diff2 * diff2;
    vec3 cartooned =
        clamp(pow(abs(edge), edgeSlope) * (-power) + color, vec3(0.0), vec3(1.0));
    return mix(color, cartooned, str);
}

/// SweetFX Tonemap.fx v1.1 — matches reference `TonemapPass` (defog, exposure, gamma, bleach, saturation).
vec3 ApplySweetFxTonemap(
    vec3 color,
    vec4 mainPack,
    vec4 fogDefogPack,
    vec4 strPack) {
    float str = clamp(strPack.x, 0.0, 1.0);
    if (str < 1e-6) {
        return color;
    }
    float g = max(mainPack.x, 1e-5);
    float exposure = mainPack.y;
    float saturation = mainPack.z;
    float bleach = clamp(mainPack.w, 0.0, 1.0);
    vec3 fogColor = fogDefogPack.xyz;
    float defogAmt = clamp(fogDefogPack.w, 0.0, 1.0);

    vec3 c = color;
    c = clamp(c - defogAmt * fogColor * 2.55, 0.0, 1.0);
    c *= pow(2.0, exposure);
    c = pow(max(c, vec3(0.0)), vec3(g));

    vec3 coefLuma = vec3(0.2126, 0.7152, 0.0722);
    float lum = dot(coefLuma, c);
    float L = clamp(10.0 * (lum - 0.45), 0.0, 1.0);
    vec3 A2 = bleach * c;
    vec3 result1 = 2.0 * c * lum;
    vec3 result2 = 1.0 - 2.0 * (1.0 - lum) * (1.0 - c);
    vec3 newColor = mix(result1, result2, L);
    vec3 mixRGB = A2 * newColor;
    c += ((1.0 - A2) * mixRGB);

    float middlegray = dot(c, vec3(1.0 / 3.0));
    vec3 diffcolor = c - vec3(middlegray);
    c = (c + diffcolor * saturation) / (1.0 + (diffcolor * saturation));

    return mix(color, c, str);
}

/// SweetFX Splitscreen.fx v2.0 — mode-driven pre/post compare; strength blends with normal output.
vec3 ApplySweetFxSplitscreen(vec3 beforeColor, vec3 afterColor, vec2 uv, vec4 modeStrength) {
    float str = clamp(modeStrength.y, 0.0, 1.0);
    if (str < 1e-6) {
        return afterColor;
    }
    int mode = clamp(int(modeStrength.x + 0.5), 0, 6);
    vec3 split = afterColor;
    if (mode == 0) {
        split = (uv.x < 0.5) ? beforeColor : afterColor;
    } else if (mode == 1) {
        float dist = clamp(abs(uv.x - 0.5) - 0.25, 0.0, 1.0);
        split = (dist > 0.0) ? beforeColor : afterColor;
    } else if (mode == 2) {
        float dist = ((uv.x - (3.0 / 8.0)) + (uv.y * 0.25));
        dist = clamp(dist - 0.25, 0.0, 1.0);
        split = (dist > 0.0) ? afterColor : beforeColor;
    } else if (mode == 3) {
        float dist = ((uv.x - (3.0 / 8.0)) + (uv.y * 0.25));
        dist = abs(dist - 0.25);
        dist = clamp(dist - 0.25, 0.0, 1.0);
        split = (dist > 0.0) ? beforeColor : afterColor;
    } else if (mode == 4) {
        split = (uv.y < 0.5) ? beforeColor : afterColor;
    } else if (mode == 5) {
        float dist = clamp(abs(uv.y - 0.5) - 0.25, 0.0, 1.0);
        split = (dist > 0.0) ? beforeColor : afterColor;
    } else {
        split = ((uv.x + uv.y) < 1.0) ? beforeColor : afterColor;
    }
    return mix(afterColor, split, str);
}

void FillSweetFxNostalgiaPalette(int paletteId, out vec3 palette[16], out int colorCount) {
    palette[0] = vec3(0.0, 0.0, 0.0);
    palette[1] = vec3(1.0, 1.0, 1.0);
    palette[2] = vec3(136.0, 0.0, 0.0) / 255.0;
    palette[3] = vec3(170.0, 255.0, 238.0) / 255.0;
    palette[4] = vec3(204.0, 68.0, 204.0) / 255.0;
    palette[5] = vec3(0.0, 204.0, 85.0) / 255.0;
    palette[6] = vec3(0.0, 0.0, 170.0) / 255.0;
    palette[7] = vec3(238.0, 238.0, 119.0) / 255.0;
    palette[8] = vec3(221.0, 136.0, 85.0) / 255.0;
    palette[9] = vec3(102.0, 68.0, 0.0) / 255.0;
    palette[10] = vec3(255.0, 119.0, 119.0) / 255.0;
    palette[11] = vec3(51.0, 51.0, 51.0) / 255.0;
    palette[12] = vec3(119.0, 119.0, 119.0) / 255.0;
    palette[13] = vec3(170.0, 255.0, 102.0) / 255.0;
    palette[14] = vec3(0.0, 136.0, 255.0) / 255.0;
    palette[15] = vec3(187.0, 187.0, 187.0) / 255.0;
    colorCount = 16;

    if (paletteId == 2) { // EGA
        palette[0] = vec3(0.0, 0.0, 0.0);
        palette[1] = vec3(0.0, 0.0, 0.666667);
        palette[2] = vec3(0.0, 0.666667, 0.0);
        palette[3] = vec3(0.0, 0.666667, 0.666667);
        palette[4] = vec3(0.666667, 0.0, 0.0);
        palette[5] = vec3(0.666667, 0.0, 0.666667);
        palette[6] = vec3(0.666667, 0.333333, 0.0);
        palette[7] = vec3(0.666667, 0.666667, 0.666667);
        palette[8] = vec3(0.333333, 0.333333, 0.333333);
        palette[9] = vec3(0.333333, 0.333333, 1.0);
        palette[10] = vec3(0.333333, 1.0, 0.333333);
        palette[11] = vec3(0.333333, 1.0, 1.0);
        palette[12] = vec3(1.0, 0.333333, 0.333333);
        palette[13] = vec3(1.0, 0.333333, 1.0);
        palette[14] = vec3(1.0, 1.0, 0.333333);
        palette[15] = vec3(1.0, 1.0, 1.0);
    } else if (paletteId == 3) { // IBMPC
        palette[0] = vec3(0.0, 0.0, 0.0);
        palette[1] = vec3(0.0, 0.0, 0.8);
        palette[2] = vec3(0.0, 0.6, 0.0);
        palette[3] = vec3(0.0, 0.6, 0.8);
        palette[4] = vec3(0.8, 0.0, 0.0);
        palette[5] = vec3(0.8, 0.0, 0.8);
        palette[6] = vec3(0.8, 0.6, 0.0);
        palette[7] = vec3(0.8, 0.8, 0.8);
        palette[8] = vec3(0.4, 0.4, 0.4);
        palette[9] = vec3(0.4, 0.4, 1.0);
        palette[10] = vec3(0.4, 1.0, 0.4);
        palette[11] = vec3(0.4, 1.0, 1.0);
        palette[12] = vec3(0.99, 0.4, 0.4);
        palette[13] = vec3(1.0, 0.4, 1.0);
        palette[14] = vec3(1.0, 1.0, 0.4);
        palette[15] = vec3(1.0, 1.0, 1.0);
    } else if (paletteId == 13) { // Gameboy
        palette[0] = vec3(0.05882353, 0.21960784, 0.05882353);
        palette[1] = vec3(0.60784316, 0.7372549, 0.05882353);
        palette[2] = vec3(0.1882353, 0.38431373, 0.1882353);
        palette[3] = vec3(0.54509807, 0.6745098, 0.05882353);
        colorCount = 4;
    } else if (paletteId == 14) { // aek16
        palette[0] = vec3(0.247059, 0.196078, 0.682353);
        palette[1] = vec3(0.890196, 0.054902, 0.760784);
        palette[2] = vec3(0.729412, 0.666667, 1.0);
        palette[3] = vec3(1.0, 1.0, 1.0);
        palette[4] = vec3(1.0, 0.580392, 0.615686);
        palette[5] = vec3(0.909804, 0.007843, 0.0);
        palette[6] = vec3(0.478431, 0.141176, 0.239216);
        palette[7] = vec3(0.0, 0.0, 0.0);
        palette[8] = vec3(0.098039, 0.337255, 0.282353);
        palette[9] = vec3(0.415686, 0.537255, 0.152941);
        palette[10] = vec3(0.086275, 0.929412, 0.458824);
        palette[11] = vec3(0.196078, 0.756863, 0.764706);
        palette[12] = vec3(0.019608, 0.498039, 0.756863);
        palette[13] = vec3(0.431373, 0.305882, 0.137255);
        palette[14] = vec3(0.937255, 0.890196, 0.019608);
        palette[15] = vec3(0.788235, 0.560784, 0.298039);
    }
}

vec3 ApplySweetFxNostalgia(vec3 color, vec2 uv, vec4 nostalgiaPack, vec2 viewportWh) {
    float str = clamp(nostalgiaPack.w, 0.0, 1.0);
    if (str < 1e-6) {
        return color;
    }
    int paletteId = clamp(int(nostalgiaPack.x + 0.5), 0, 14);
    int scanlines = clamp(int(nostalgiaPack.y + 0.5), 0, 2);
    float dither = clamp(nostalgiaPack.z, 0.0, 1.0);

    vec3 c = color;
    if (dither > 0.0) {
        float gridPosition = fract(dot(uv, viewportWh * 0.5) + 0.25);
        float ditherShift = 0.25 * (1.0 / (pow(2.0, 2.0) - 1.0));
        vec3 shift = vec3(ditherShift);
        shift = mix(2.0 * shift, -2.0 * shift, gridPosition);
        c += shift * dither;
    }

    vec3 palette[16];
    int colorCount = 16;
    FillSweetFxNostalgiaPalette(paletteId, palette, colorCount);

    vec3 diff = c - palette[0];
    float closestDist = dot(diff, diff);
    vec3 closest = palette[0];
    for (int i = 1; i < colorCount; ++i) {
        diff = c - palette[i];
        float dist = dot(diff, diff);
        if (dist < closestDist) {
            closestDist = dist;
            closest = palette[i];
        }
    }
    c = closest;

    if (scanlines == 1) {
        c *= fract(uv.y * (viewportWh.y * 0.5)) + 0.5;
    } else if (scanlines == 2) {
        float grey = dot(c, vec3(1.0 / 3.0));
        c = (fract(uv.y * (viewportWh.y * 0.5)) < 0.25)
            ? c
            : c * (((-grey * grey + grey + grey) * 0.5) + 0.5);
    }
    return mix(color, c, str);
}

vec3 ApplySweetFxCompare(vec3 effectA, vec3 effectB, vec2 uv, vec4 comparePack) {
    int mode = clamp(int(comparePack.x + 0.5), 0, 8);
    float differenceScale = max(comparePack.y, 1.0);
    float str = clamp(comparePack.z, 0.0, 1.0);
    if (str < 1e-6) {
        return effectB;
    }
    vec3 original = effectA;
    vec3 color = effectB;
    if (mode == 0) {
        color = (uv.x < 0.5) ? effectA : effectB;
    } else if (mode == 1) {
        if (uv.x < 0.333) {
            color = effectA;
        } else if (uv.x < 0.666) {
            color = original;
        } else {
            color = effectB;
        }
    } else if (mode == 2) {
        float dist = ((uv.x - (3.0 / 8.0)) + (uv.y * 0.25));
        dist = clamp(dist - 0.25, 0.0, 1.0);
        color = (dist > 0.0) ? effectB : effectA;
    } else if (mode == 3) {
        float angle = uv.x + uv.y * 0.5;
        if (angle < 0.5) {
            color = effectA;
        } else if (angle < 1.0) {
            color = original;
        } else {
            color = effectB;
        }
    } else if (mode == 4) {
        color = (uv.y < 0.5) ? effectA : effectB;
    } else if (mode == 5) {
        if (uv.y < 0.333) {
            color = effectA;
        } else if (uv.y < 0.666) {
            color = original;
        } else {
            color = effectB;
        }
    } else if (mode == 6) {
        color = ((uv.x + uv.y) < 1.0) ? effectA : effectB;
    } else if (mode == 7) {
        color = abs(effectB - effectA) * differenceScale;
    } else {
        color = (effectB - effectA) * differenceScale + 0.5;
    }
    return mix(effectB, color, str);
}

vec3 ApplySweetFxLayer(vec3 backColor, vec2 texCoord, vec2 bufferScreenSize, vec4 posScaleBlend, vec4 texSizePad) {
    float blendAmt = clamp(posScaleBlend.w, 0.0, 1.0);
    if (blendAmt < 1e-6) {
        return backColor;
    }
    vec2 layerPos = clamp(posScaleBlend.xy, 0.0, 1.0);
    float layerScale = max(posScaleBlend.z, 0.01);
    vec2 layerDim = max(texSizePad.xy, vec2(1.0));
    vec2 pixelSize = 1.0 / (layerDim * layerScale / bufferScreenSize);
    vec2 layerUv = clamp(texCoord * pixelSize + layerPos * (1.0 - pixelSize), 0.0, 1.0);
    vec4 layer = texture(sweetFxLayerTexture, layerUv);
    float t = clamp(layer.a * blendAmt, 0.0, 1.0);
    return mix(backColor, layer.rgb, t);
}

float SweetFxCrtCorner(vec2 coord, vec2 textureSize, float overscan, float cornerSize) {
    vec2 c = (coord - 0.5) * overscan + 0.5;
    vec2 m = min(c, 1.0 - c) * vec2(1.0, 0.75);
    vec2 cd = vec2(cornerSize);
    vec2 q = cd - min(m, cd);
    float dist = length(q);
    return clamp((cd.x - dist) * 1000.0, 0.0, 1.0);
}

vec2 SweetFxCrtTransform(vec2 uv, vec4 crtPack2, vec4 crtPack3) {
    float curvature = clamp(cameraData.sweetFxCrtPack1.w, 0.0, 1.0);
    if (curvature < 1e-6) {
        return uv;
    }
    float radius = max(crtPack2.x, 1e-4);
    vec2 angle = clamp(crtPack3.xy, vec2(-0.2), vec2(0.2));
    vec2 p = uv - 0.5;
    p.x *= 1.0 + angle.y;
    p.y *= 1.0 + angle.x;
    float r2 = dot(p, p);
    float k = curvature * (0.35 / max(radius, 1e-4));
    p *= (1.0 + r2 * k);
    p /= max(crtPack2.w, 1.0);
    return p + 0.5;
}

vec3 ApplySweetFxCrt(vec2 sampleUv, vec2 px, vec3 baseColor, vec4 crtPack0, vec4 crtPack1, vec4 crtPack2, vec4 crtPack3) {
    float amount = clamp(crtPack0.x, 0.0, 1.0);
    if (amount < 1e-6) {
        return baseColor;
    }
    float resolutionScale = max(crtPack0.y, 1.0);
    float gammaCrt = max(crtPack0.z, 1e-4);
    float monitorGamma = max(crtPack0.w, 1e-4);
    float brightness = clamp(crtPack1.x, 0.0, 3.0);
    int scanlineIntensity = clamp(int(crtPack1.y + 0.5), 2, 4);
    float scanlineGaussian = clamp(crtPack1.z, 0.0, 1.0);
    float cornerSize = clamp(crtPack2.y, 0.0, 0.02);
    float oversample = clamp(crtPack3.z, 0.0, 1.0);

    float inputRatio = ceil(256.0 * resolutionScale);
    vec2 rubyTextureSize = vec2(inputRatio);
    vec2 rubyOutputSize = cameraData.viewportMetrics.xy;
    vec2 uv = SweetFxCrtTransform(sampleUv, crtPack2, crtPack3);
    float cval = SweetFxCrtCorner(uv, rubyTextureSize, clamp(crtPack2.w, 1.0, 1.10), cornerSize);

    vec2 ratioScale = uv * rubyTextureSize - 0.5;
    vec2 uvRatio = fract(ratioScale);
    vec2 xy = (floor(ratioScale) + 0.5) / rubyTextureSize;
    vec2 coone = 1.0 / rubyTextureSize;

    vec4 coeffs = 3.1415927 * vec4(1.0 + uvRatio.x, uvRatio.x, 1.0 - uvRatio.x, 2.0 - uvRatio.x);
    coeffs = max(abs(coeffs), vec4(1e-5));
    coeffs = 2.0 * sin(coeffs) * sin(coeffs * 0.5) / (coeffs * coeffs);
    coeffs /= max(dot(coeffs, vec4(1.0)), 1e-5);

    vec3 s0 = pow(abs(texture(hdrSceneLinear, xy + vec2(-coone.x, 0.0)).rgb), vec3(gammaCrt));
    vec3 s1 = pow(abs(texture(hdrSceneLinear, xy).rgb), vec3(gammaCrt));
    vec3 s2 = pow(abs(texture(hdrSceneLinear, xy + vec2(coone.x, 0.0)).rgb), vec3(gammaCrt));
    vec3 s3 = pow(abs(texture(hdrSceneLinear, xy + vec2(2.0 * coone.x, 0.0)).rgb), vec3(gammaCrt));
    vec3 t0 = pow(abs(texture(hdrSceneLinear, xy + vec2(-coone.x, coone.y)).rgb), vec3(gammaCrt));
    vec3 t1 = pow(abs(texture(hdrSceneLinear, xy + vec2(0.0, coone.y)).rgb), vec3(gammaCrt));
    vec3 t2 = pow(abs(texture(hdrSceneLinear, xy + coone).rgb), vec3(gammaCrt));
    vec3 t3 = pow(abs(texture(hdrSceneLinear, xy + vec2(2.0 * coone.x, coone.y)).rgb), vec3(gammaCrt));

    vec3 col = clamp(s0 * coeffs.x + s1 * coeffs.y + s2 * coeffs.z + s3 * coeffs.w, 0.0, 1.0);
    vec3 col2 = clamp(t0 * coeffs.x + t1 * coeffs.y + t2 * coeffs.z + t3 * coeffs.w, 0.0, 1.0);

    float d = uvRatio.y;
    vec3 widA = mix((2.0 * pow(abs(col), vec3(4.0)) + 2.0), (0.3 + 0.1 * pow(abs(col), vec3(3.0))), scanlineGaussian);
    vec3 widB = mix((2.0 * pow(abs(col2), vec3(4.0)) + 2.0), (0.3 + 0.1 * pow(abs(col2), vec3(3.0))), scanlineGaussian);
    vec3 wa = exp(-pow(abs((vec3(d) / 0.3) * inversesqrt(max(0.5 * widA, vec3(1e-4)))), widA));
    vec3 wb = exp(-pow(abs((vec3(1.0 - d) / 0.3) * inversesqrt(max(0.5 * widB, vec3(1e-4)))), widB));
    wa = mix(1.4 * wa / (0.2 * widA + 0.6), 0.4 * exp(-pow(vec3(d) / max(widA, vec3(1e-4)), vec3(2.0))) / max(widA, vec3(1e-4)), scanlineGaussian);
    wb = mix(1.4 * wb / (0.2 * widB + 0.6), 0.4 * exp(-pow(vec3(1.0 - d) / max(widB, vec3(1e-4)), vec3(2.0))) / max(widB, vec3(1e-4)), scanlineGaussian);

    if (oversample > 0.5) {
        float beamFilter = max(fwidth(ratioScale.y), px.y);
        float d1 = d + (beamFilter / 3.0);
        float d2 = d - (2.0 * beamFilter / 3.0);
        vec3 wa1 = exp(-pow(abs((vec3(d1) / 0.3) * inversesqrt(max(0.5 * widA, vec3(1e-4)))), widA));
        vec3 wb1 = exp(-pow(abs((vec3(1.0 - d1) / 0.3) * inversesqrt(max(0.5 * widB, vec3(1e-4)))), widB));
        vec3 wa2 = exp(-pow(abs((vec3(abs(d2)) / 0.3) * inversesqrt(max(0.5 * widA, vec3(1e-4)))), widA));
        vec3 wb2 = exp(-pow(abs((vec3(abs(1.0 - d2)) / 0.3) * inversesqrt(max(0.5 * widB, vec3(1e-4)))), widB));
        wa = (wa + wa1 + wa2) / 3.0;
        wb = (wb + wb1 + wb2) / 3.0;
    }

    vec3 crt = (col * wa + col2 * wb) * cval;
    float modFactor = uv.x * rubyTextureSize.x * rubyOutputSize.x / max(inputRatio, 1.0);
    float parity = floor(mod(modFactor, float(scanlineIntensity)));
    vec3 dotMask = mix(vec3(1.0, 0.7, 1.0), vec3(0.7, 1.0, 0.7), parity > 0.5 ? 1.0 : 0.0);
    crt *= dotMask * vec3(0.83, 0.83, 1.0) * brightness;
    crt = pow(abs(crt), vec3(1.0 / monitorGamma));
    return mix(baseColor, clamp(crt, 0.0, 1.0), amount);
}

float SweetFxAsciiGlyphBit(float bitfield, float x, float lit) {
    float signbit = (bitfield < 0.0) ? lit : 0.0;
    signbit = (x > 23.5) ? signbit : 0.0;
    float v = fract(abs(bitfield * exp2(-x - 1.0)));
    return (v >= 0.5) ? lit : signbit;
}

float SweetFxAsciiSelectGlyph(float gray, int font) {
    if (font == 1) {
        float quant = 1.0 / 16.0;
        float n12 = (gray < (2.0 * quant)) ? 4194304.0 : 131200.0;
        float n34 = (gray < (4.0 * quant)) ? 324.0 : 330.0;
        float n56 = (gray < (6.0 * quant)) ? 283712.0 : 12650880.0;
        float n78 = (gray < (8.0 * quant)) ? 4532768.0 : 13191552.0;
        float n910 = (gray < (10.0 * quant)) ? 10648704.0 : 11195936.0;
        float n1112 = (gray < (12.0 * quant)) ? 15218734.0 : 15255086.0;
        float n1314 = (gray < (14.0 * quant)) ? 15252014.0 : 32294446.0;
        float n1516 = (gray < (16.0 * quant)) ? 15324974.0 : 11512810.0;
        float n1234 = (gray < (3.0 * quant)) ? n12 : n34;
        float n5678 = (gray < (7.0 * quant)) ? n56 : n78;
        float n9101112 = (gray < (11.0 * quant)) ? n910 : n1112;
        float n13141516 = (gray < (15.0 * quant)) ? n1314 : n1516;
        float n12345678 = (gray < (5.0 * quant)) ? n1234 : n5678;
        float n910111213141516 = (gray < (13.0 * quant)) ? n9101112 : n13141516;
        return (gray < (9.0 * quant)) ? n12345678 : n910111213141516;
    }
    float quant = 1.0 / 13.0;
    float n12 = (gray < (2.0 * quant)) ? 4096.0 : 1040.0;
    float n34 = (gray < (4.0 * quant)) ? 5136.0 : 5200.0;
    float n56 = (gray < (6.0 * quant)) ? 2728.0 : 11088.0;
    float n78 = (gray < (8.0 * quant)) ? 14478.0 : 11114.0;
    float n910 = (gray < (10.0 * quant)) ? 23213.0 : 15211.0;
    float n1112 = (gray < (12.0 * quant)) ? 23533.0 : 31599.0;
    float n13 = 31727.0;
    float n1234 = (gray < (3.0 * quant)) ? n12 : n34;
    float n5678 = (gray < (7.0 * quant)) ? n56 : n78;
    float n9101112 = (gray < (11.0 * quant)) ? n910 : n1112;
    float n12345678 = (gray < (5.0 * quant)) ? n1234 : n5678;
    float n910111213 = (gray < (13.0 * quant)) ? n9101112 : n13;
    return (gray < (9.0 * quant)) ? n12345678 : n910111213;
}

vec3 ApplySweetFxAscii(vec2 sampleUv, vec2 px, vec3 inputColor, vec4 asciiPack0, vec4 asciiPack1, vec4 asciiPack2, vec3 fontColor, vec3 backgroundColor) {
    float strength = clamp(asciiPack0.w, 0.0, 1.0);
    if (strength < 1e-6) {
        return inputColor;
    }
    int spacing = clamp(int(asciiPack0.x + 0.5), 0, 5);
    int font = clamp(int(asciiPack0.y + 0.5), 0, 1);
    int colorMode = clamp(int(asciiPack0.z + 0.5), 0, 2);
    float swapColors = clamp(asciiPack1.x, 0.0, 1.0);
    float invertBrightness = clamp(asciiPack1.y, 0.0, 1.0);
    float dithering = clamp(asciiPack1.z, 0.0, 1.0);
    float ditheringIntensity = clamp(asciiPack1.w, 0.0, 4.0);
    float debugGradient = clamp(asciiPack2.x, 0.0, 1.0);

    vec2 fontSize = (font == 1) ? vec2(5.0, 5.0) : vec2(3.0, 5.0);
    float numChars = (font == 1) ? 17.0 : 14.0;
    float quant = 1.0 / (numChars - 1.0);
    vec2 asciiBlock = fontSize + vec2(float(spacing));
    vec2 cursorPos = trunc((cameraData.viewportMetrics.xy / asciiBlock) * sampleUv) * (asciiBlock / cameraData.viewportMetrics.xy);

    vec3 color = texture(hdrSceneLinear, cursorPos + vec2(1.5, 1.5) * px).rgb;
    color += texture(hdrSceneLinear, cursorPos + vec2(1.5, 3.5) * px).rgb;
    color += texture(hdrSceneLinear, cursorPos + vec2(1.5, 5.5) * px).rgb;
    color += texture(hdrSceneLinear, cursorPos + vec2(3.5, 1.5) * px).rgb;
    color += texture(hdrSceneLinear, cursorPos + vec2(3.5, 3.5) * px).rgb;
    color += texture(hdrSceneLinear, cursorPos + vec2(3.5, 5.5) * px).rgb;
    color += texture(hdrSceneLinear, cursorPos + vec2(5.5, 1.5) * px).rgb;
    color += texture(hdrSceneLinear, cursorPos + vec2(5.5, 3.5) * px).rgb;
    color += texture(hdrSceneLinear, cursorPos + vec2(5.5, 5.5) * px).rgb;
    color /= 9.0;

    float gray = dot(color, vec3(0.2126, 0.7152, 0.0722));
    gray = mix(gray, 1.0 - gray, invertBrightness);
    gray = mix(gray, cursorPos.x, debugGradient);
    if (dithering > 0.5) {
        float seed = dot(cursorPos, vec2(12.9898, 78.233));
        float noise = fract(sin(seed) * 43758.5453 + cursorPos.y);
        float ditherShift = quant * ditheringIntensity;
        gray += (ditherShift * noise) - (ditherShift * 0.5);
    }
    gray = clamp(gray, 0.0, 1.0);

    vec2 p = fract((cameraData.viewportMetrics.xy / asciiBlock) * sampleUv);
    p = trunc(p * asciiBlock);
    float x = (fontSize.x * p.y + p.x);
    float bitfield = SweetFxAsciiSelectGlyph(gray, font);
    float lit = (gray <= (1.0 * quant)) ? 0.0 : 1.0;
    float character = SweetFxAsciiGlyphBit(bitfield, x, lit);
    if (p.x < 0.0 || p.y < 0.0 || p.x > (fontSize.x - 1.0) || p.y > (fontSize.y - 1.0)) {
        character = 0.0;
    }

    vec3 asciiColor = color;
    if (swapColors > 0.5) {
        if (colorMode == 2) {
            asciiColor = (character > 0.5) ? (character * color) : fontColor;
        } else if (colorMode == 1) {
            asciiColor = (character > 0.5) ? (backgroundColor * gray) : fontColor;
        } else {
            asciiColor = (character > 0.5) ? backgroundColor : fontColor;
        }
    } else {
        if (colorMode == 2) {
            asciiColor = (character > 0.5) ? (character * color) : backgroundColor;
        } else if (colorMode == 1) {
            asciiColor = (character > 0.5) ? (fontColor * gray) : backgroundColor;
        } else {
            asciiColor = (character > 0.5) ? fontColor : backgroundColor;
        }
    }
    return mix(inputColor, clamp(asciiColor, 0.0, 1.0), strength);
}

vec3 EvaluateMappedCompositeCore(
    vec2 sampleUv, vec2 px, float ditherAmt, float debandAmt, float vignetteAmt, float filmGrainAmt, float aspectView) {
    vec3 mapped = EvalPostStackNoSharpen(sampleUv, px);
    mapped = ApplySweetFxLumaSharpen(sampleUv, px, mapped, cameraData.lumaSharpenPack);
    mapped = ApplyPostProcessFx(mapped, sampleUv);
    mapped = ApplySweetFxTonemap(
        mapped,
        cameraData.sweetFxTonemapGammaExpSatBleach,
        cameraData.sweetFxTonemapFogColorDefog,
        cameraData.sweetFxTonemapStrengthPad);
    mapped = ApplySweetFxCartoon(mapped, sampleUv, px, cameraData.sweetFxCartoonPack);
    mapped = ApplySweetFxVignetteType0(mapped, sampleUv, vignetteAmt, aspectView);
    mapped = ApplySweetFxFilmGrain(mapped, sampleUv, cameraData.postProcessSecondary.z, filmGrainAmt);

    if (debandAmt > 1e-5) {
        vec3 mappedN = EvalPostStackNoSharpen(sampleUv + vec2(0.0, -px.y), px);
        mappedN = ApplySweetFxLumaSharpen(sampleUv + vec2(0.0, -px.y), px, mappedN, cameraData.lumaSharpenPack);
        mappedN = ApplyPostProcessFx(mappedN, sampleUv + vec2(0.0, -px.y));
        mappedN = ApplySweetFxTonemap(
            mappedN,
            cameraData.sweetFxTonemapGammaExpSatBleach,
            cameraData.sweetFxTonemapFogColorDefog,
            cameraData.sweetFxTonemapStrengthPad);
        mappedN = ApplySweetFxCartoon(mappedN, sampleUv + vec2(0.0, -px.y), px, cameraData.sweetFxCartoonPack);
        mappedN = ApplySweetFxVignetteType0(mappedN, sampleUv + vec2(0.0, -px.y), vignetteAmt, aspectView);
        mappedN = ApplySweetFxFilmGrain(mappedN, sampleUv + vec2(0.0, -px.y), cameraData.postProcessSecondary.z, filmGrainAmt);

        vec3 mappedS = EvalPostStackNoSharpen(sampleUv + vec2(0.0, px.y), px);
        mappedS = ApplySweetFxLumaSharpen(sampleUv + vec2(0.0, px.y), px, mappedS, cameraData.lumaSharpenPack);
        mappedS = ApplyPostProcessFx(mappedS, sampleUv + vec2(0.0, px.y));
        mappedS = ApplySweetFxTonemap(
            mappedS,
            cameraData.sweetFxTonemapGammaExpSatBleach,
            cameraData.sweetFxTonemapFogColorDefog,
            cameraData.sweetFxTonemapStrengthPad);
        mappedS = ApplySweetFxCartoon(mappedS, sampleUv + vec2(0.0, px.y), px, cameraData.sweetFxCartoonPack);
        mappedS = ApplySweetFxVignetteType0(mappedS, sampleUv + vec2(0.0, px.y), vignetteAmt, aspectView);
        mappedS = ApplySweetFxFilmGrain(mappedS, sampleUv + vec2(0.0, px.y), cameraData.postProcessSecondary.z, filmGrainAmt);

        float luma = dot(mapped, vec3(0.2126, 0.7152, 0.0722));
        float lumaN = dot(mappedN, vec3(0.2126, 0.7152, 0.0722));
        float lumaS = dot(mappedS, vec3(0.2126, 0.7152, 0.0722));
        float gradMag = clamp(abs(lumaN - lumaS) * 48.0, 0.0, 1.0);
        float rnd = Hash21(sampleUv * cameraData.viewportMetrics.xy + vec2(cameraData.postProcessSecondary.z * 13.7, cameraData.postProcessSecondary.z * 9.1)) - 0.5;
        mapped += rnd * debandAmt * (0.12 + gradMag * 0.55);
    }
    mapped = TriangularDitherRgb(mapped, sampleUv, cameraData.postProcessSecondary.z, ditherAmt);
    mapped = ApplySweetFxBorder(
        mapped,
        sampleUv,
        cameraData.viewportMetrics,
        px,
        cameraData.sweetFxBorderPack,
        cameraData.sweetFxBorderColorPad);
    mapped = ApplySweetFxNostalgia(mapped, sampleUv, cameraData.sweetFxNostalgiaPack, cameraData.viewportMetrics.xy);
    mapped = ApplySweetFxLayer(
        mapped,
        sampleUv,
        cameraData.viewportMetrics.xy,
        cameraData.sweetFxLayerPosScaleBlend,
        cameraData.sweetFxLayerTexSizePad);
    mapped = ApplySweetFxCrt(
        sampleUv,
        px,
        mapped,
        cameraData.sweetFxCrtPack0,
        cameraData.sweetFxCrtPack1,
        cameraData.sweetFxCrtPack2,
        cameraData.sweetFxCrtPack3);
    mapped = ApplySweetFxAscii(
        sampleUv,
        px,
        mapped,
        cameraData.sweetFxAsciiPack0,
        cameraData.sweetFxAsciiPack1,
        cameraData.sweetFxAsciiPack2,
        cameraData.sweetFxAsciiFontColorPad.rgb,
        cameraData.sweetFxAsciiBackgroundColorPad.rgb);
    return mapped;
}

vec3 ApplySweetFxFxaa(
    vec2 sampleUv, vec2 px, float ditherAmt, float debandAmt, float vignetteAmt, float filmGrainAmt, float aspectView, vec4 fxaaPack) {
    float subpix = clamp(fxaaPack.x, 0.0, 1.0);
    float edgeThreshold = clamp(fxaaPack.y, 0.0, 1.0);
    float edgeThresholdMin = clamp(fxaaPack.z, 0.0, 1.0);
    float strength = clamp(fxaaPack.w, 0.0, 1.0);

    vec3 rgbM = EvaluateMappedCompositeCore(sampleUv, px, ditherAmt, debandAmt, vignetteAmt, filmGrainAmt, aspectView);
    if (strength < 1e-6) {
        return rgbM;
    }

    vec3 rgbN = EvaluateMappedCompositeCore(sampleUv + vec2(0.0, -px.y), px, ditherAmt, debandAmt, vignetteAmt, filmGrainAmt, aspectView);
    vec3 rgbS = EvaluateMappedCompositeCore(sampleUv + vec2(0.0, px.y), px, ditherAmt, debandAmt, vignetteAmt, filmGrainAmt, aspectView);
    vec3 rgbW = EvaluateMappedCompositeCore(sampleUv + vec2(-px.x, 0.0), px, ditherAmt, debandAmt, vignetteAmt, filmGrainAmt, aspectView);
    vec3 rgbE = EvaluateMappedCompositeCore(sampleUv + vec2(px.x, 0.0), px, ditherAmt, debandAmt, vignetteAmt, filmGrainAmt, aspectView);

    float lumaM = sqrt(dot(rgbM * rgbM, vec3(0.299, 0.587, 0.114)));
    float lumaN = sqrt(dot(rgbN * rgbN, vec3(0.299, 0.587, 0.114)));
    float lumaS = sqrt(dot(rgbS * rgbS, vec3(0.299, 0.587, 0.114)));
    float lumaW = sqrt(dot(rgbW * rgbW, vec3(0.299, 0.587, 0.114)));
    float lumaE = sqrt(dot(rgbE * rgbE, vec3(0.299, 0.587, 0.114)));

    float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
    float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));
    float range = lumaMax - lumaMin;
    float threshold = max(edgeThresholdMin, lumaMax * edgeThreshold);
    if (range < threshold) {
        return rgbM;
    }

    float edgeH = abs(lumaW + lumaE - 2.0 * lumaM);
    float edgeV = abs(lumaN + lumaS - 2.0 * lumaM);
    vec2 dir = (edgeH >= edgeV) ? vec2(px.x, 0.0) : vec2(0.0, px.y);

    vec3 rgbNeg = EvaluateMappedCompositeCore(sampleUv - dir * 0.5, px, ditherAmt, debandAmt, vignetteAmt, filmGrainAmt, aspectView);
    vec3 rgbPos = EvaluateMappedCompositeCore(sampleUv + dir * 0.5, px, ditherAmt, debandAmt, vignetteAmt, filmGrainAmt, aspectView);
    vec3 rgbSpan = 0.5 * (rgbNeg + rgbPos);

    float subpixAlias = clamp((range / max(lumaMax, 1e-4)) - edgeThresholdMin, 0.0, 1.0);
    subpixAlias = subpixAlias * subpixAlias * subpix;
    vec3 aa = mix(rgbM, rgbSpan, clamp(subpixAlias + 0.5, 0.0, 1.0));
    return mix(rgbM, aa, strength);
}

vec3 ApplySweetFxSmaa(vec2 sampleUv, vec2 px, vec3 baseColor, vec4 smaaPack0, vec4 smaaPack1) {
    float strength = clamp(smaaPack0.w, 0.0, 1.0);
    if (strength < 1e-6) {
        return baseColor;
    }

    int edgeType = clamp(int(smaaPack0.x + 0.5), 0, 2);
    float edgeThreshold = clamp(smaaPack0.y, 0.01, 0.5);
    float depthThreshold = clamp(smaaPack0.z, 0.001, 0.5);
    int maxSearch = clamp(int(smaaPack1.x + 0.5), 1, 112);
    int maxDiag = clamp(int(smaaPack1.y + 0.5), 0, 20);
    float cornerRounding = clamp(smaaPack1.z * 0.01, 0.0, 1.0);
    int debugOutput = clamp(int(smaaPack1.w + 0.5), 0, 2);

    vec3 c = baseColor;
    vec3 n = EvaluateMappedCompositeCore(sampleUv + vec2(0.0, -px.y), px, 0.0, 0.0, 0.0, 0.0, 1.0);
    vec3 s = EvaluateMappedCompositeCore(sampleUv + vec2(0.0, px.y), px, 0.0, 0.0, 0.0, 0.0, 1.0);
    vec3 w = EvaluateMappedCompositeCore(sampleUv + vec2(-px.x, 0.0), px, 0.0, 0.0, 0.0, 0.0, 1.0);
    vec3 e = EvaluateMappedCompositeCore(sampleUv + vec2(px.x, 0.0), px, 0.0, 0.0, 0.0, 0.0, 1.0);

    float lc = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float ln = dot(n, vec3(0.2126, 0.7152, 0.0722));
    float ls = dot(s, vec3(0.2126, 0.7152, 0.0722));
    float lw = dot(w, vec3(0.2126, 0.7152, 0.0722));
    float le = dot(e, vec3(0.2126, 0.7152, 0.0722));

    float edgeH = max(abs(lw - lc), abs(le - lc));
    float edgeV = max(abs(ln - lc), abs(ls - lc));
    float t = (edgeType == 2) ? depthThreshold : edgeThreshold;
    float edgeMaskH = step(t, edgeH);
    float edgeMaskV = step(t, edgeV);
    float edgeMask = max(edgeMaskH, edgeMaskV);

    float searchScale = clamp(float(maxSearch) / 32.0, 0.1, 3.5);
    float diagScale = clamp(float(maxDiag) / 16.0, 0.0, 2.0);
    vec2 areaUv = vec2(clamp(edgeMask, 0.0, 1.0), clamp(abs(edgeH - edgeV), 0.0, 1.0));
    vec2 areaSample = texture(sweetFxSmaaAreaTexture, areaUv).rg;
    vec2 searchUv = vec2(clamp(max(edgeH, edgeV), 0.0, 1.0), 0.5);
    float searchSample = texture(sweetFxSmaaSearchTexture, searchUv).r;
    searchScale *= (0.6 + areaSample.x * 0.8 + searchSample * 0.6);
    diagScale *= (0.6 + areaSample.y * 0.8);
    vec2 dir = vec2(le - lw, ls - ln);
    dir = normalize(dir + vec2(1e-5)) * px;
    vec3 a = EvaluateMappedCompositeCore(sampleUv + dir * (0.5 * searchScale), px, 0.0, 0.0, 0.0, 0.0, 1.0);
    vec3 b = EvaluateMappedCompositeCore(sampleUv - dir * (0.5 * searchScale), px, 0.0, 0.0, 0.0, 0.0, 1.0);
    vec3 blend = 0.5 * (a + b);

    if (maxDiag > 0) {
        vec2 d = vec2(px.x, px.y) * (0.5 + 0.25 * diagScale);
        vec3 d1 = EvaluateMappedCompositeCore(sampleUv + d, px, 0.0, 0.0, 0.0, 0.0, 1.0);
        vec3 d2 = EvaluateMappedCompositeCore(sampleUv - d, px, 0.0, 0.0, 0.0, 0.0, 1.0);
        blend = mix(blend, 0.5 * (d1 + d2), 0.35 * edgeMask);
    }

    float corner = smoothstep(0.0, 1.0, min(edgeH, edgeV) / max(max(edgeH, edgeV), 1e-4));
    float cornerReduce = mix(1.0, 1.0 - corner, cornerRounding);
    float aaWeight = edgeMask * cornerReduce * strength;
    vec3 outColor = mix(c, blend, clamp(aaWeight, 0.0, 1.0));

    if (debugOutput == 1) {
        return vec3(edgeMaskH, edgeMaskV, edgeMask);
    }
    if (debugOutput == 2) {
        return vec3(clamp(aaWeight, 0.0, 1.0));
    }
    return outColor;
}

vec3 ApplyReShadeDaltonize(vec3 inputColor, vec4 daltonizePack) {
    int mode = clamp(int(daltonizePack.x + 0.5), 0, 2);
    float strength = clamp(daltonizePack.y, 0.0, 1.0);
    if (strength < 1e-6) {
        return inputColor;
    }

    float onizeL = (17.8824 * inputColor.r) + (43.5161 * inputColor.g) + (4.11935 * inputColor.b);
    float onizeM = (3.45565 * inputColor.r) + (27.1554 * inputColor.g) + (3.86714 * inputColor.b);
    float onizeS = (0.0299566 * inputColor.r) + (0.184309 * inputColor.g) + (1.46709 * inputColor.b);

    float daltL = onizeL;
    float daltM = onizeM;
    float daltS = onizeS;
    if (mode == 0) {
        daltL = (2.02344 * onizeM) + (-2.52581 * onizeS);
    } else if (mode == 1) {
        daltM = (0.494207 * onizeL) + (1.24827 * onizeS);
    } else {
        daltS = (-0.395913 * onizeL) + (0.801109 * onizeM);
    }

    vec3 simulated;
    simulated.r = (0.0809444479 * daltL) + (-0.130504409 * daltM) + (0.116721066 * daltS);
    simulated.g = (-0.0102485335 * daltL) + (0.0540193266 * daltM) + (-0.113614708 * daltS);
    simulated.b = (-0.000365296938 * daltL) + (-0.00412161469 * daltM) + (0.693511405 * daltS);

    vec3 err = inputColor - simulated;
    vec3 correction;
    correction.r = 0.0;
    correction.g = (err.r * 0.7) + err.g;
    correction.b = (err.r * 0.7) + err.b;
    vec3 corrected = inputColor + correction;
    return mix(inputColor, clamp(corrected, 0.0, 1.0), strength);
}

vec3 ApplyReShadeDisplayDepth(vec2 sampleUv, vec2 px, vec3 baseColor, vec4 displayDepthPack) {
    int presentType = clamp(int(displayDepthPack.x + 0.5), 0, 2);
    float strength = clamp(displayDepthPack.y, 0.0, 1.0);
    if (strength < 1e-6) {
        return baseColor;
    }

    float depth = clamp(texture(sceneDepth, sampleUv).r, 0.0, 1.0);
    vec3 depthColor = vec3(depth);
    float dN = texture(sceneDepth, sampleUv + vec2(0.0, -px.y)).r;
    float dE = texture(sceneDepth, sampleUv + vec2(px.x, 0.0)).r;
    vec3 n = normalize(vec3(dN - depth, dE - depth, 0.4));
    vec3 normalColor = n * 0.5 + 0.5;

    vec3 displayColor = depthColor;
    if (presentType == 1) {
        displayColor = normalColor;
    } else if (presentType == 2) {
        displayColor = mix(normalColor, depthColor, step(0.5, sampleUv.x));
    }
    return mix(baseColor, clamp(displayColor, 0.0, 1.0), strength);
}

vec3 ApplyReShadeLut(vec3 color, vec4 lutPack) {
    float amountChroma = clamp(lutPack.x, 0.0, 1.0);
    float amountLuma = clamp(lutPack.y, 0.0, 1.0);
    float strength = clamp(lutPack.z, 0.0, 1.0);
    if (strength < 1e-6) {
        return color;
    }
    const float tileSize = 32.0;
    const float tileCount = 32.0;
    vec2 texelSize = vec2(1.0 / tileSize / tileCount, 1.0 / tileSize);
    vec3 lutcoord = vec3((color.xy * tileSize - color.xy + 0.5) * texelSize.xy, color.z * tileSize - color.z);
    float lerpfact = fract(lutcoord.z);
    lutcoord.x += (lutcoord.z - lerpfact) * texelSize.y;
    vec3 lut0 = texture(reshadeLutTexture, lutcoord.xy).rgb;
    vec3 lut1 = texture(reshadeLutTexture, vec2(lutcoord.x + texelSize.y, lutcoord.y)).rgb;
    vec3 lutColor = mix(lut0, lut1, lerpfact);
    vec3 chroma = mix(normalize(max(color, vec3(1e-5))), normalize(max(lutColor, vec3(1e-5))), amountChroma);
    float luma = mix(length(color), length(lutColor), amountLuma);
    vec3 outColor = chroma * luma;
    return mix(color, clamp(outColor, 0.0, 1.0), strength);
}

vec3 ApplyPd80BonusLutPack(vec2 sampleUv, vec3 colorIn) {
    vec4 p0 = cameraData.pd80BlpPack0;
    vec4 p1 = cameraData.pd80BlpPack1;
    vec4 p2 = cameraData.pd80BlpPack2;
    vec4 p3 = cameraData.pd80BlpPack3;
    vec4 p4 = cameraData.pd80BlpPack4;
    vec4 p5 = cameraData.pd80BlpPack5;
    float master = clamp(p0.x, 0.0, 1.0);
    if (master <= 0.0) {
        return colorIn;
    }

    bool enableDither = p0.y >= 0.5;
    float ditherStrength = clamp(p0.z, 0.0, 10.0);
    float lutSelector = clamp(round(p0.w), 0.0, 49.0);
    float mixChroma = clamp(p1.x, 0.0, 1.0);
    float mixLuma = clamp(p1.y, 0.0, 1.0);
    float gamma = clamp(p1.z, 0.05, 10.0);

    vec3 color = clamp(colorIn, 0.0, 1.0);
    float timer = 0.0;
    vec4 dnoise = Pd80CscDither(sampleUv, enableDither, ditherStrength, timer);

    const float tileSize = 64.0;
    const float tileCount = 64.0;
    const float lutCount = 50.0;
    vec2 texelSize = vec2(1.0 / tileSize / tileCount, 1.0 / tileSize);

    vec3 lutcoord = vec3((color.xy * tileSize - color.xy + 0.5) * texelSize.xy, color.z * tileSize - color.z);
    lutcoord.y /= lutCount;
    lutcoord.y += lutSelector / lutCount;
    float lerpfact = fract(lutcoord.z);
    lutcoord.x += (lutcoord.z - lerpfact) * texelSize.y;
    vec3 lut0 = texture(reshadeLutTexture, lutcoord.xy).rgb;
    vec3 lut1 = texture(reshadeLutTexture, vec2(lutcoord.x + texelSize.y, lutcoord.y)).rgb;
    vec3 lutColor = mix(lut0, lut1, lerpfact);

    vec3 blackIn = clamp(p2.xyz + dnoise.xyz, 0.0, 1.0);
    vec3 whiteIn = clamp(p3.xyz + dnoise.yzx, 0.0, 1.0);
    vec3 blackOut = clamp(p4.xyz + dnoise.zxy, 0.0, 1.0);
    vec3 whiteOut = clamp(vec3(p4.w, p5.x, p5.y) + dnoise.wxz, 0.0, 1.0);
    lutColor = clamp((clamp(lutColor - blackIn, 0.0, 1.0) / max(whiteIn - blackIn, vec3(1.0e-6))), 0.0, 1.0);
    lutColor = pow(lutColor, vec3(gamma));
    lutColor = lutColor * clamp(whiteOut - blackOut, 0.0, 1.0) + blackOut;

    vec3 labLut = Pd80CscSrgbToLab(lutColor);
    vec3 labCol = Pd80CscSrgbToLab(color);
    float newLuma = mix(labCol.x, labLut.x, mixLuma);
    vec2 newAB = mix(labCol.yz, labLut.yz, mixChroma);
    vec3 outColor = Pd80CscLabToSrgb(vec3(newLuma, newAB));
    outColor = clamp(outColor + dnoise.wzx, 0.0, 1.0);
    return mix(color, outColor, master);
}

vec3 ApplyPd80CinetoolsLut(vec2 sampleUv, vec3 colorIn) {
    vec4 p0 = cameraData.pd80CltPack0;
    vec4 p1 = cameraData.pd80CltPack1;
    vec4 p2 = cameraData.pd80CltPack2;
    vec4 p3 = cameraData.pd80CltPack3;
    vec4 p4 = cameraData.pd80CltPack4;
    vec4 p5 = cameraData.pd80CltPack5;
    float master = clamp(p0.x, 0.0, 1.0);
    if (master <= 0.0) {
        return colorIn;
    }

    bool enableDither = p0.y >= 0.5;
    float ditherStrength = clamp(p0.z, 0.0, 10.0);
    float lutSelector = clamp(round(p0.w), 0.0, 30.0);
    float mixChroma = clamp(p1.x, 0.0, 1.0);
    float mixLuma = clamp(p1.y, 0.0, 1.0);
    float gamma = clamp(p1.z, 0.05, 10.0);

    vec3 color = clamp(colorIn, 0.0, 1.0);
    vec4 dnoise = Pd80CscDither(sampleUv, enableDither, ditherStrength, 0.0);

    const float tileSize = 64.0;
    const float tileCount = 64.0;
    const float lutCount = 31.0;
    vec2 texelSize = vec2(1.0 / tileSize / tileCount, 1.0 / tileSize);

    vec3 lutcoord = vec3((color.xy * tileSize - color.xy + 0.5) * texelSize.xy, color.z * tileSize - color.z);
    lutcoord.y /= lutCount;
    lutcoord.y += lutSelector / lutCount;
    float lerpfact = fract(lutcoord.z);
    lutcoord.x += (lutcoord.z - lerpfact) * texelSize.y;
    vec3 lut0 = texture(nativePd80CineLutTexture, lutcoord.xy).rgb;
    vec3 lut1 = texture(nativePd80CineLutTexture, vec2(lutcoord.x + texelSize.y, lutcoord.y)).rgb;
    vec3 lutColor = mix(lut0, lut1, lerpfact);

    vec3 blackIn = clamp(p2.xyz + dnoise.xyz, 0.0, 1.0);
    vec3 whiteIn = clamp(p3.xyz + dnoise.yzx, 0.0, 1.0);
    vec3 blackOut = clamp(p4.xyz + dnoise.zxy, 0.0, 1.0);
    vec3 whiteOut = clamp(vec3(p4.w, p5.x, p5.y) + dnoise.wxz, 0.0, 1.0);
    lutColor = clamp((clamp(lutColor - blackIn, 0.0, 1.0) / max(whiteIn - blackIn, vec3(1.0e-6))), 0.0, 1.0);
    lutColor = pow(lutColor, vec3(gamma));
    lutColor = lutColor * clamp(whiteOut - blackOut, 0.0, 1.0) + blackOut;

    vec3 labLut = Pd80CscSrgbToLab(lutColor);
    vec3 labCol = Pd80CscSrgbToLab(color);
    float newLuma = mix(labCol.x, labLut.x, mixLuma);
    vec2 newAB = mix(labCol.yz, labLut.yz, mixChroma);
    vec3 outColor = Pd80CscLabToSrgb(vec3(newLuma, newAB));
    outColor = clamp(outColor + dnoise.wzx, 0.0, 1.0);
    return mix(color, outColor, master);
}

vec3 ApplyPd80LutCreator(vec2 sampleUv, vec3 colorIn) {
    vec4 p0 = cameraData.pd80LcPack0;
    float master = clamp(p0.x, 0.0, 1.0);
    if (master <= 0.0) {
        return colorIn;
    }
    float texW = max(p0.y, 1.0);
    float texH = max(p0.z, 1.0);
    vec2 coords = (cameraData.viewportMetrics.xy / vec2(texW, texH)) * sampleUv;
    vec3 lut = texture(reshadeLutTexture, coords).rgb;
    vec2 cutoff = cameraData.viewportMetrics.zw * vec2(texW, texH);
    vec3 outColor = (sampleUv.y > cutoff.y || sampleUv.x > cutoff.x) ? colorIn : lut;
    return mix(colorIn, clamp(outColor, 0.0, 1.0), master);
}

vec3 ApplyPd80LumaFade(vec3 preColor, vec3 postColor) {
    vec4 p0 = cameraData.pd80LfPack0;
    float master = clamp(p0.x, 0.0, 1.0);
    if (master <= 0.0) {
        return postColor;
    }
    float transitionSpeed = clamp(p0.y, 0.0, 1.0);
    float minLevel = clamp(p0.z, 0.0, 1.0);
    float maxLevel = clamp(p0.w, 0.0, 1.0);
    if (minLevel >= maxLevel) {
        maxLevel = min(minLevel + 0.01, 1.0);
    }
    float luma = dot(clamp(postColor, 0.0, 1.0), vec3(0.333333, 0.333333, 0.333334));
    float fade = smoothstep(minLevel, maxLevel, luma);
    float speedShape = transitionSpeed * 4.0 + 1.0;
    fade = pow(fade, 1.0 / speedShape);
    vec3 mixed = mix(preColor, postColor, fade);
    return mix(postColor, mixed, master);
}

vec3 ApplyPd80ColorGradients(vec2 sampleUv, vec3 colorIn) {
    vec4 p0 = cameraData.pd80Cg4Pack0;
    float master = clamp(p0.x, 0.0, 1.0);
    if (master <= 0.0) {
        return colorIn;
    }
    int lumaMode = clamp(int(p0.y + 0.5), 0, 2);
    int sepMode = clamp(int(p0.z + 0.5), 0, 1);
    bool enableDither = p0.w >= 0.5;
    vec4 p1 = cameraData.pd80Cg4Pack1;
    vec4 p2 = cameraData.pd80Cg4Pack2;
    vec4 p3 = cameraData.pd80Cg4Pack3;
    vec4 p4 = cameraData.pd80Cg4Pack4;
    vec3 lsM = cameraData.pd80Cg4Pack5.xyz;
    vec3 lsS = cameraData.pd80Cg4Pack6.xyz;
    vec3 dsM = cameraData.pd80Cg4Pack7.xyz;
    vec3 dsS = cameraData.pd80Cg4Pack8.xyz;

    float sceneluma = max(max(colorIn.r, colorIn.g), colorIn.b);
    float minlevel = clamp(p4.x, 0.0, 1.0);
    float maxlevel = clamp(p4.y, 0.0, 1.0);
    if (minlevel >= maxlevel) maxlevel = min(minlevel + 0.01, 1.0);
    sceneluma = smoothstep(minlevel, maxlevel, sceneluma);

    vec3 color = clamp(colorIn, 0.0, 1.0);
    vec4 dnoise = Pd80CscDither(sampleUv, enableDither, clamp(p1.x, 0.0, 10.0), 0.0);
    color = clamp(color + dnoise.xyz, 0.0, 1.0);

    float cWeight = dot(color, vec3(0.333333, 0.333334, 0.333333));
    if (lumaMode == 1) cWeight = dot(color, vec3(0.212656, 0.715158, 0.072186));
    else if (lumaMode == 2) cWeight = max(max(color.r, color.g), color.b);

    float w_s = 0.0;
    float w_h = 0.0;
    float w_m = 0.0;
    if (sepMode == 0) {
        w_s = Pd80SmhCurve(max(1.0 - cWeight * 2.0, 0.0));
        w_h = Pd80SmhCurve(max((cWeight - 0.5) * 2.0, 0.0));
        w_m = clamp(1.0 - w_s - w_h, 0.0, 1.0);
    } else {
        w_s = pow(1.0 - cWeight, 4.0);
        w_h = pow(cWeight, 4.0);
        w_m = clamp(1.0 - w_s - w_h, 0.0, 1.0);
    }

    color = mix(color, vec3(cWeight), clamp(p1.y, 0.0, 1.0));
    vec3 ls_b_s = Pd80DsBlendmode(color, lsS, clamp(int(p2.y + 0.5), 0, 20), clamp(p2.z, 0.0, 1.0));
    vec3 ls_b_m = Pd80DsBlendmode(color, lsM, clamp(int(p1.w + 0.5), 0, 20), clamp(p2.x, 0.0, 1.0));
    vec3 ls_col = ls_b_s * w_s + ls_b_m * w_m + vec3(w_h);
    vec3 ds_b_s = Pd80DsBlendmode(color, dsS, clamp(int(p3.z + 0.5), 0, 20), clamp(p3.w, 0.0, 1.0));
    vec3 ds_b_m = Pd80DsBlendmode(color, dsM, clamp(int(p3.x + 0.5), 0, 20), clamp(p3.y, 0.0, 1.0));
    vec3 ds_col = ds_b_s * w_s + ds_b_m * w_m + vec3(w_h);
    vec3 new_c = mix(ds_col, ls_col, sceneluma);
    if (p2.w < 0.5) new_c = ls_col;
    color = mix(color, new_c, clamp(p1.z, 0.0, 1.0));
    return mix(colorIn, clamp(color, 0.0, 1.0), master);
}

/// Colourfulness.fx (bacondither) — perceptual saturation with near-clip soft limit (LDR gamma input).
vec3 ApplyCreatorColourfulness(vec3 c0, vec4 packIn) {
    float amt = packIn.x;
    if (abs(amt) < 1e-4) {
        return c0;
    }
    float limLuma = clamp(packIn.y, 0.1, 1.0);
    const vec3 lumacoeff = vec3(0.2558, 0.6511, 0.0931);
    c0 = clamp(c0, 0.0, 1.0);
    float luma = sqrt(dot(clamp(c0 * abs(c0), 0.0, 1.0), lumacoeff));
    vec3 diffLuma = c0 - luma;
    vec3 cDiff = diffLuma * (amt + 1.0) - diffLuma;
    if (amt > 0.0) {
        vec3 rlcDiff = clamp((cDiff * 1.2) + c0, -0.0001, 1.0001) - c0;
        float maxd = max(diffLuma.r, max(diffLuma.g, diffLuma.b));
        float mind = min(diffLuma.r, min(diffLuma.g, diffLuma.b));
        float poslim = (1.0002 - luma) / (abs(maxd) + 0.0001);
        float neglim = (luma + 0.0002) / (abs(mind) + 0.0001);
        vec3 diffmax = diffLuma * min(min(poslim, neglim), 32.0) - diffLuma;
        // wpmean(diffmax, rlcDiff, limLuma): pow(|w|*sqrt|a| + |1-w|*sqrt|b|, 2)
        vec3 wp = abs(limLuma) * sqrt(abs(diffmax)) + abs(1.0 - limLuma) * sqrt(abs(rlcDiff));
        vec3 lim = max(wp * wp, vec3(1e-7));
        // soft_lim(cDiff, lim) = (cDiff*lim) / sqrt(lim*lim + cDiff*cDiff)
        cDiff = (cDiff * lim) / sqrt(lim * lim + cDiff * cDiff);
    }
    return clamp(c0 + cDiff, 0.0, 1.0);
}

/// FilmicPass.fx — cinematic tone/colour curve. Author defaults baked as constants; creator drives
/// strength/fade/bleach/saturation.
vec3 ApplyCreatorFilmicPass(vec3 inColor, vec4 packIn) {
    float Strength = packIn.x;
    if (Strength <= 0.0) {
        return inColor;
    }
    float Fade = packIn.y;
    float Bleach = packIn.z;
    float Saturation = packIn.w;
    const float Contrast = 1.0;
    const float Linearization = 0.5;
    const float BaseGamma = 1.0;
    const float EffectGamma = 0.65;
    const float a = 1.0; // RedCurve
    const float b = 1.0; // GreenCurve
    const float cc = 1.0; // BlueCurve
    const float d = 1.5; // BaseCurve
    const vec3 LumCoeff = vec3(0.212656, 0.715158, 0.072186);

    vec3 B = clamp(inColor, 0.0, 1.0);
    vec3 H = vec3(0.01);
    B = pow(B, vec3(Linearization));
    B = mix(H, B, Contrast);
    float A = dot(B, LumCoeff);
    vec3 D = vec3(A);
    B = pow(abs(B), vec3(1.0 / BaseGamma));

    float y = 1.0 / (1.0 + exp(a / 2.0));
    float z = 1.0 / (1.0 + exp(b / 2.0));
    float w = 1.0 / (1.0 + exp(cc / 2.0));
    float v = 1.0 / (1.0 + exp(d / 2.0));

    vec3 C = B;
    D.r = (1.0 / (1.0 + exp(-a * (D.r - 0.5))) - y) / (1.0 - 2.0 * y);
    D.g = (1.0 / (1.0 + exp(-b * (D.g - 0.5))) - z) / (1.0 - 2.0 * z);
    D.b = (1.0 / (1.0 + exp(-cc * (D.b - 0.5))) - w) / (1.0 - 2.0 * w);
    D = pow(abs(D), vec3(1.0 / EffectGamma));
    vec3 Di = vec3(1.0) - D;
    D = mix(D, Di, Bleach);
    // EffectGammaR/G/B == 1 → no per-channel gamma here.

    C.r = (D.r < 0.5) ? (2.0 * D.r - 1.0) * (B.r - B.r * B.r) + B.r
                      : (2.0 * D.r - 1.0) * (sqrt(B.r) - B.r) + B.r;
    C.g = (D.g < 0.5) ? (2.0 * D.g - 1.0) * (B.g - B.g * B.g) + B.g
                      : (2.0 * D.g - 1.0) * (sqrt(B.g) - B.g) + B.g;
    C.b = (D.b < 0.5) ? (2.0 * D.b - 1.0) * (B.b - B.b * B.b) + B.b
                      : (2.0 * D.b - 1.0) * (sqrt(B.b) - B.b) + B.b;

    vec3 F = mix(B, C, Strength);
    F = (1.0 / (1.0 + exp(-d * (F - 0.5))) - vec3(v)) / (1.0 - 2.0 * v);

    float r2R = 1.0 - Saturation;
    float g2R = Saturation;
    float b2R = Saturation;
    float r2G = Saturation;
    float g2G = (1.0 - Fade) - Saturation;
    float b2G = Fade + Saturation;
    float r2B = Saturation;
    float g2B = Fade + Saturation;
    float b2B = (1.0 - Fade) - Saturation;

    vec3 iF = F;
    F.r = iF.r * r2R + iF.g * g2R + iF.b * b2R;
    F.g = iF.r * r2G + iF.g * g2G + iF.b * b2G;
    F.b = iF.r * r2B + iF.g * g2B + iF.b * b2B;

    float N = dot(F, LumCoeff);
    vec3 Cn = (N < 0.5) ? (2.0 * N - 1.0) * (F - F * F) + F
                        : (2.0 * N - 1.0) * (sqrt(max(F, vec3(0.0))) - F) + F;
    Cn = pow(max(Cn, vec3(0.0)), vec3(1.0 / Linearization));
    return mix(B, Cn, Strength);
}

vec4 CreatorGrainRnm(vec2 tc) {
    float noise = sin(dot(tc, vec2(12.9898, 78.233))) * 43758.5453;
    float nr = fract(noise) * 2.0 - 1.0;
    float ng = fract(noise * 1.2154) * 2.0 - 1.0;
    float nb = fract(noise * 1.3453) * 2.0 - 1.0;
    float na = fract(noise * 1.3647) * 2.0 - 1.0;
    return vec4(nr, ng, nb, na);
}

float CreatorGrainPnoise3D(vec3 p) {
    const float permTexUnit = 1.0 / 256.0;
    const float permTexUnitHalf = 0.5 / 256.0;
    vec3 pi = permTexUnit * floor(p) + permTexUnitHalf;
    vec3 pf = fract(p);
    float perm00 = CreatorGrainRnm(pi.xy).a;
    vec3 grad000 = CreatorGrainRnm(vec2(perm00, pi.z)).rgb * 4.0 - 1.0;
    float n000 = dot(grad000, pf);
    vec3 grad001 = CreatorGrainRnm(vec2(perm00, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
    float n001 = dot(grad001, pf - vec3(0.0, 0.0, 1.0));
    float perm01 = CreatorGrainRnm(pi.xy + vec2(0.0, permTexUnit)).a;
    vec3 grad010 = CreatorGrainRnm(vec2(perm01, pi.z)).rgb * 4.0 - 1.0;
    float n010 = dot(grad010, pf - vec3(0.0, 1.0, 0.0));
    vec3 grad011 = CreatorGrainRnm(vec2(perm01, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
    float n011 = dot(grad011, pf - vec3(0.0, 1.0, 1.0));
    float perm10 = CreatorGrainRnm(pi.xy + vec2(permTexUnit, 0.0)).a;
    vec3 grad100 = CreatorGrainRnm(vec2(perm10, pi.z)).rgb * 4.0 - 1.0;
    float n100 = dot(grad100, pf - vec3(1.0, 0.0, 0.0));
    vec3 grad101 = CreatorGrainRnm(vec2(perm10, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
    float n101 = dot(grad101, pf - vec3(1.0, 0.0, 1.0));
    float perm11 = CreatorGrainRnm(pi.xy + vec2(permTexUnit, permTexUnit)).a;
    vec3 grad110 = CreatorGrainRnm(vec2(perm11, pi.z)).rgb * 4.0 - 1.0;
    float n110 = dot(grad110, pf - vec3(1.0, 1.0, 0.0));
    vec3 grad111 = CreatorGrainRnm(vec2(perm11, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
    float n111 = dot(grad111, pf - vec3(1.0, 1.0, 1.0));
    float fadeX = pf.x * pf.x * pf.x * (pf.x * (pf.x * 6.0 - 15.0) + 10.0);
    vec4 nX = mix(vec4(n000, n001, n010, n011), vec4(n100, n101, n110, n111), fadeX);
    float fadeY = pf.y * pf.y * pf.y * (pf.y * (pf.y * 6.0 - 15.0) + 10.0);
    vec2 nXy = mix(nX.xy, nX.zw, fadeY);
    float fadeZ = pf.z * pf.z * pf.z * (pf.z * (pf.z * 6.0 - 15.0) + 10.0);
    return mix(nXy.x, nXy.y, fadeZ);
}

vec2 CreatorGrainCoordRot(vec2 tc, float angle, float aspect) {
    float rotX = ((tc.x * 2.0 - 1.0) * aspect * cos(angle)) - ((tc.y * 2.0 - 1.0) * sin(angle));
    float rotY = ((tc.y * 2.0 - 1.0) * cos(angle)) + ((tc.x * 2.0 - 1.0) * aspect * sin(angle));
    rotX = (rotX / aspect) * 0.5 + 0.5;
    rotY = rotY * 0.5 + 0.5;
    return vec2(rotX, rotY);
}

/// FilmGrain2.fx (martinsh) — animated 3D-Perlin grain, luminance-masked, optional coloured noise.
vec3 ApplyCreatorFilmGrain2(vec2 texCoord, vec3 col, vec4 packIn, vec2 screenSize, float aspect, float timer) {
    float grainamount = packIn.x;
    if (grainamount <= 0.0) {
        return col;
    }
    float coloramount = clamp(packIn.y, 0.0, 1.0);
    float lumamount = clamp(packIn.z, 0.0, 1.0);
    float grainsize = max(packIn.w, 0.5);
    vec3 rotOffset = vec3(1.425, 3.892, 5.835);
    vec2 rotCoordsR = CreatorGrainCoordRot(texCoord, timer + rotOffset.x, aspect);
    vec3 noise = vec3(CreatorGrainPnoise3D(vec3(rotCoordsR * screenSize / grainsize, 0.0)));
    if (coloramount > 0.0) {
        vec2 rotCoordsG = CreatorGrainCoordRot(texCoord, timer + rotOffset.y, aspect);
        vec2 rotCoordsB = CreatorGrainCoordRot(texCoord, timer + rotOffset.z, aspect);
        noise.g = mix(noise.r, CreatorGrainPnoise3D(vec3(rotCoordsG * screenSize / grainsize, 1.0)), coloramount);
        noise.b = mix(noise.r, CreatorGrainPnoise3D(vec3(rotCoordsB * screenSize / grainsize, 2.0)), coloramount);
    }
    const vec3 lumcoeff = vec3(0.299, 0.587, 0.114);
    float luminance = mix(0.0, dot(col, lumcoeff), lumamount);
    float lum = smoothstep(0.2, 0.0, luminance);
    lum += luminance;
    noise = mix(noise, vec3(0.0), pow(lum, 4.0));
    return col + noise * grainamount;
}

/// DOF.fx Marty McFly RingDOF — fused focus + fringe + ring bokeh.
float CreatorDofGetCoC(vec2 coords, vec2 px) {
    float scenedepth = texture(sceneDepth, coords).r;
    float scenefocus = 0.0;
    vec4 pack0 = cameraData.creatorDofPack0;
    vec4 pack1 = cameraData.creatorDofPack1;
    float infiniteFocus = max(pack0.w, 0.01);
    if (pack0.y > 0.5) {
        vec2 focusPoint = pack1.xy;
        int focusSamples = clamp(int(pack1.w + 0.5), 3, 10);
        float focusRadius = pack1.z;
        for (int r = 0; r < focusSamples; ++r) {
            float angle = (6.2831853 / float(focusSamples)) * float(r);
            vec2 offset = vec2(cos(angle), sin(angle));
            offset.y *= cameraData.viewportMetrics.x / max(cameraData.viewportMetrics.y, 1.0);
            scenefocus += texture(sceneDepth, offset * focusRadius + focusPoint).r;
        }
        scenefocus /= float(focusSamples);
    } else {
        scenefocus = pack0.z;
    }
    scenefocus = smoothstep(0.0, infiniteFocus, scenefocus);
    scenedepth = smoothstep(0.0, infiniteFocus, scenedepth);
    float farBlurDepth = scenefocus * pow(4.0, cameraData.creatorDofPack2.y);
    float scenecoc;
    if (scenedepth < scenefocus) {
        scenecoc = (scenedepth - scenefocus) / max(scenefocus, 1e-4);
    } else {
        scenecoc = (scenedepth - scenefocus) / max(farBlurDepth - scenefocus, 1e-4);
        scenecoc = clamp(scenecoc, 0.0, 1.0);
    }
    return clamp(scenecoc * 0.5 + 0.5, 0.0, 1.0);
}

vec3 ApplyCreatorRingDof(vec2 sampleUv, vec2 px, vec3 colorIn) {
    float strength = cameraData.creatorDofPack0.x;
    if (strength <= 1e-6) {
        return colorIn;
    }
    vec4 pack2 = cameraData.creatorDofPack2;
    vec4 pack3 = cameraData.creatorDofPack3;
    float centerDepth = CreatorDofGetCoC(sampleUv, px);
    float blurAmount = abs(centerDepth * 2.0 - 1.0);
    float discRadius = blurAmount * pack2.z;
    discRadius *= (centerDepth < 0.5) ? (1.0 / max(pack2.x * 2.0, 1.0)) : 1.0;
    if (discRadius < 1.2) {
        return colorIn;
    }

    vec3 scenecolor = colorIn;
    float fringe = pack3.w;
    vec2 fringeScale = fringe * discRadius * px;
    scenecolor.r = texture(hdrSceneLinear, sampleUv + vec2(0.0, 1.0) * fringeScale).r;
    scenecolor.g = texture(hdrSceneLinear, sampleUv + vec2(-0.866, -0.5) * fringeScale).g;
    scenecolor.b = texture(hdrSceneLinear, sampleUv + vec2(0.866, -0.5) * fringeScale).b;

    vec3 blurcolor = scenecolor;
    float blurWeight = 1.0;
    int ringRings = clamp(int(pack3.x + 0.5), 1, 8);
    int ringSamplesBase = clamp(int(pack2.w + 0.5), 5, 30);
    float ringThreshold = pack3.y;
    float ringGain = pack3.z;
    float ringBias = cameraData.creatorDofPack4.x;

    for (int g = 1; g <= ringRings; ++g) {
        int ringsamples = g * ringSamplesBase;
        for (int j = 0; j < ringsamples; ++j) {
            float stepAngle = 6.2831853 / float(ringsamples);
            vec2 sampleoffset = vec2(cos(float(j) * stepAngle), sin(float(j) * stepAngle));
            vec3 tap = texture(hdrSceneLinear, sampleUv + sampleoffset * px * discRadius * float(g) / float(ringRings)).rgb;
            float tapluma = dot(tap, vec3(0.333));
            float tapthresh = max((tapluma - ringThreshold) * ringGain, 0.0);
            tap *= 1.0 + tapthresh * blurAmount;
            float tapDepth = CreatorDofGetCoC(sampleUv + sampleoffset * px * discRadius * float(g) / float(ringRings), px);
            float tapw = (tapDepth >= centerDepth * 0.99) ? 1.0 : pow(abs(tapDepth * 2.0 - 1.0), 4.0);
            tapw *= mix(1.0, float(g) / float(ringRings), ringBias);
            blurcolor += tap * tapw;
            blurWeight += tapw;
        }
    }
    blurcolor /= max(blurWeight, 1e-4);
    vec3 result = mix(colorIn, blurcolor, smoothstep(1.2, 2.0, discRadius));
    return mix(colorIn, result, strength);
}

vec3 CreatorAmbientLightDetectHigh(vec2 uv) {
    vec3 x = texture(hdrSceneLinear, uv).rgb;
    x *= pow(max(max(x.r, max(x.g, x.b)), 0.0), 2.0);
    float base = (x.r + x.g + x.b) / 3.0;
    float nR = (x.r * 2.0) - base;
    float nG = (x.g * 2.0) - base;
    float nB = (x.b * 2.0) - base;
    if (nR < 0.0) { nG += nR / 2.0; nB += nR / 2.0; nR = 0.0; }
    if (nG < 0.0) { nB += nG / 2.0; nR = (nR > -nG / 2.0) ? nR + nG / 2.0 : 0.0; nG = 0.0; }
    if (nB < 0.0) { nR = (nR > -nB / 2.0) ? nR + nB / 2.0 : 0.0; nG = (nG > -nB / 2.0) ? nG + nB / 2.0 : 0.0; nB = 0.0; }
    if (nR > 1.0) { nG += (nR - 1.0) / 2.0; nB += (nR - 1.0) / 2.0; nR = 1.0; }
    if (nG > 1.0) { nB += (nG - 1.0) / 2.0; nR = (nR + (nG - 1.0) < 1.0) ? nR + (nG - 1.0) / 2.0 : 1.0; nG = 1.0; }
    if (nB > 1.0) { nR = (nR + (nB - 1.0) < 1.0) ? nR + (nB - 1.0) / 2.0 : 1.0; nG = (nG + (nB - 1.0) < 1.0) ? nG + (nB - 1.0) / 2.0 : 1.0; nB = 1.0; }
    return vec3(nR, nG, nB);
}

vec3 CreatorAmbientLightBlurAt(vec2 uv, vec2 px, bool horizontal, float threshold, float stepMult) {
    const float sampleOffsets[5] = float[5](0.0, 2.4347826, 4.3478260, 6.2608695, 8.1739130);
    const float sampleWeights[5] = float[5](0.16818994, 0.27276957, 0.111690125, 0.024067905, 0.0021112196);
    vec2 blurPx = px * 16.0;
    vec3 accum = CreatorAmbientLightDetectHigh(uv) * sampleWeights[0];
    accum = max(accum - threshold, 0.0);
    for (int i = 1; i < 5; ++i) {
        vec2 delta = horizontal ? vec2(sampleOffsets[i] * blurPx.x, 0.0) : vec2(0.0, sampleOffsets[i] * blurPx.y);
        vec3 tap = CreatorAmbientLightDetectHigh(uv + delta);
        tap = max(tap - threshold, 0.0);
        accum += tap * sampleWeights[i] * stepMult;
        tap = CreatorAmbientLightDetectHigh(uv - delta);
        tap = max(tap - threshold, 0.0);
        accum += tap * sampleWeights[i] * stepMult;
    }
    return accum;
}

vec3 CreatorAmbientLightBlurredHigh(vec2 uv, vec2 px, float threshold, float timePhase) {
    float stepMult = 1.08 + mod(timePhase, 6.28) * 0.0002;
    vec3 color = CreatorAmbientLightDetectHigh(uv);
    for (int passIndex = 0; passIndex < 6; ++passIndex) {
        color = CreatorAmbientLightBlurAt(uv, px, (passIndex % 2) == 0, threshold, stepMult);
    }
    return color;
}

float CreatorAmbientLightDetectLow(vec2 px) {
    vec3 detectLow = vec3(0.0);
    int sampleCount = 0;
    for (float i = 0.0; i <= 1.0; i += 0.0625) {
        for (float j = 0.0; j <= 1.0; j += 0.0625) {
            detectLow += CreatorAmbientLightDetectHigh(vec2(i, j));
            sampleCount += 1;
        }
    }
    detectLow /= float(sampleCount);
    return sqrt(0.241 * detectLow.r * detectLow.r + 0.691 * detectLow.g * detectLow.g + 0.068 * detectLow.b * detectLow.b);
}

vec3 CreatorAmbientLightProceduralDirt(vec2 uv) {
    vec2 p = uv * vec2(13.0, 7.0);
    float n = fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
    float streak = pow(max(1.0 - abs(uv.y - 0.5) * 1.6, 0.0), 3.0);
    return vec3(0.55 + 0.35 * n) * (0.35 + 0.65 * streak);
}

vec3 CreatorAmbientLightProceduralDirtOvr(vec2 uv, vec3 tint, float timePhase) {
    vec2 p = uv * 8.0 + vec2(timePhase * 0.01, 0.0);
    float n = fract(sin(dot(p, vec2(41.23, 17.91))) * 1031.73);
    return tint * (0.25 + 0.75 * n) * pow(max(1.0 - length(uv - 0.5), 0.0), 1.5);
}

vec3 ApplyCreatorAmbientLight(vec2 sampleUv, vec2 px, vec3 colorIn) {
    vec4 pack0 = cameraData.creatorAmbientLightPack0;
    float alInt = pack0.x;
    if (alInt <= 1e-6) {
        return colorIn;
    }
    vec4 pack1 = cameraData.creatorAmbientLightPack1;
    vec4 pack2 = cameraData.creatorAmbientLightPack2;
    float threshold = pack0.y;
    bool useAdaptation = pack2.w > 0.5;
    bool useDither = pack1.y > 0.5;
    bool useDirt = pack1.z > 0.5;
    int adaptiveMode = clamp(int(pack1.w + 0.5), 0, 2);
    float adaptAmount = 0.0;
    if (useAdaptation) {
        float low = pow(CreatorAmbientLightDetectLow(px) * 1.25, 2.0);
        adaptAmount = low * (low + 1.0) * pack0.z * alInt * 5.0;
    }

    vec3 high = min(CreatorAmbientLightBlurredHigh(sampleUv, px, threshold, pack2.z), vec3(0.0325)) * 1.15;
    vec3 highOrig = high;
    if (useDirt) {
        vec3 dirt = CreatorAmbientLightProceduralDirt(sampleUv);
        vec3 dirtOvrWarm = CreatorAmbientLightProceduralDirtOvr(sampleUv, vec3(1.0, 0.55, 0.25), pack2.z);
        vec3 dirtOvrCold = CreatorAmbientLightProceduralDirtOvr(sampleUv, vec3(0.25, 0.45, 1.0), pack2.z);
        float maxhigh = max(high.r, max(high.g, high.b));
        float threshDiff = maxhigh - 3.2;
        if (threshDiff > 0.0) {
            high = (high / maxhigh) * 3.2;
        }
        vec3 highDirt = highOrig * high * pack2.x;
        float highMix = max(highOrig.r + highOrig.g + highOrig.b, 1e-4);
        float red = highOrig.r / highMix;
        float green = highOrig.g / highMix;
        float blue = highOrig.b / highMix;
        highOrig = highOrig + highDirt;
        if (adaptiveMode == 2) {
            high = high + high * dirtOvrWarm * pack2.y * green;
            high = high + highDirt;
            high = high + highOrig * dirtOvrCold * pack2.y * blue;
            high = high + highOrig * dirtOvrWarm * pack2.y * red;
        } else if (adaptiveMode == 1) {
            high = high + highDirt;
            high = high + highOrig * dirtOvrCold * pack2.y;
        } else {
            high = high + highDirt;
            high = high + highOrig * dirtOvrWarm * pack2.y;
        }
    }

    vec3 base = colorIn;
    float dither = 0.0;
    if (useDither) {
        dither = 0.15 * (1.0 / (pow(2.0, 10.0) - 1.0));
        dither = mix(2.0 * dither, -2.0 * dither, fract(dot(sampleUv, cameraData.viewportMetrics.xy * vec2(1.0 / 16.0, 10.0 / 36.0)) + 0.25));
    }

    if (useAdaptation) {
        base *= max(0.0, 1.0 - adaptAmount * 0.75 * pack0.w * pow(abs(1.0 - (base.x + base.y + base.z) / 3.0), pack1.x));
        vec3 highSampleMix = (1.0 - ((1.0 - base) * (1.0 - high))) + dither;
        vec3 baseSample = mix(base, highSampleMix, max(0.0, alInt - adaptAmount));
        float baseSampleMix = baseSample.r + baseSample.g + baseSample.b;
        return baseSampleMix > 0.008 ? baseSample : mix(base, highSampleMix, max(0.0, (alInt - adaptAmount) * 0.85) * baseSampleMix);
    }
    vec3 highSampleMix = (1.0 - ((1.0 - base) * (1.0 - high))) + dither;
    vec3 baseSample = mix(base, highSampleMix, alInt);
    float baseSampleMix = baseSample.r + baseSample.g + baseSample.b;
    return baseSampleMix > 0.008 ? baseSample : mix(base, highSampleMix, max(0.0, alInt * 0.85) * baseSampleMix);
}

float CreatorRbmGetLinearDepth(vec2 coords) {
    return texture(sceneDepth, coords).r;
}

vec3 CreatorRbmGetPosition(vec2 coords, float depthFarPlane) {
    float eyeDepth = CreatorRbmGetLinearDepth(coords) * depthFarPlane;
    return vec3((coords * 2.0 - 1.0) * eyeDepth, eyeDepth);
}

vec3 CreatorRbmGetNormalFromDepth(vec2 coords, vec2 px, float depthFarPlane) {
    vec3 centerPos = CreatorRbmGetPosition(coords, depthFarPlane);
    vec2 offs = px * 1.0;
    vec3 ddx1 = CreatorRbmGetPosition(coords + vec2(offs.x, 0.0), depthFarPlane) - centerPos;
    vec3 ddx2 = centerPos - CreatorRbmGetPosition(coords - vec2(offs.x, 0.0), depthFarPlane);
    vec3 ddy1 = CreatorRbmGetPosition(coords + vec2(0.0, offs.y), depthFarPlane) - centerPos;
    vec3 ddy2 = centerPos - CreatorRbmGetPosition(coords - vec2(0.0, offs.y), depthFarPlane);
    ddx1 = mix(ddx1, ddx2, step(abs(ddx2.z), abs(ddx1.z)));
    ddy1 = mix(ddy1, ddy2, step(abs(ddy2.z), abs(ddy1.z)));
    return normalize(cross(ddy1, ddx1));
}

vec3 CreatorRbmGetNormalFromColor(vec2 coords, vec2 offset, float scale, float sharpness, float scenedepth) {
    const vec3 lumCoeff = vec3(0.299, 0.587, 0.114);
    float hpx = dot(texture(hdrSceneLinear, coords + vec2(offset.x, 0.0)).rgb, lumCoeff) * scale;
    float hmx = dot(texture(hdrSceneLinear, coords - vec2(offset.x, 0.0)).rgb, lumCoeff) * scale;
    float hpy = dot(texture(hdrSceneLinear, coords + vec2(0.0, offset.y)).rgb, lumCoeff) * scale;
    float hmy = dot(texture(hdrSceneLinear, coords - vec2(0.0, offset.y)).rgb, lumCoeff) * scale;
    float dpx = CreatorRbmGetLinearDepth(coords + vec2(offset.x, 0.0));
    float dmx = CreatorRbmGetLinearDepth(coords - vec2(offset.x, 0.0));
    float dpy = CreatorRbmGetLinearDepth(coords + vec2(0.0, offset.y));
    float dmy = CreatorRbmGetLinearDepth(coords - vec2(0.0, offset.y));
    vec2 xymult = vec2(abs(dmx - dpx), abs(dmy - dpy)) * sharpness;
    xymult = max(vec2(0.0), vec2(1.0) - xymult);
    float ddx = (hmx - hpx) / (2.0 * offset.x) * xymult.x;
    float ddy = (hmy - hpy) / (2.0 * offset.y) * xymult.y;
    return normalize(vec3(ddx, ddy, 1.0));
}

vec3 CreatorRbmBlendNormals(vec3 n1, vec3 n2) {
    return normalize(vec3(n1.xy * n2.z + n2.xy * n1.z, n1.z * n2.z));
}

vec3 CreatorRbmRgb2Hsv(vec3 rgb) {
    vec4 k = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = rgb.g < rgb.b ? vec4(rgb.bg, k.wz) : vec4(rgb.gb, k.xy);
    vec4 q = rgb.r < p.x ? vec4(p.xyw, rgb.r) : vec4(rgb.r, p.yzx);
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

float CreatorRbmGetHueMaskFull(float hue, vec4 pack2, vec4 pack3) {
    float sMod = 0.0;
    sMod += pack2.x * (1.0 - min(1.0, abs(hue / 0.08333333)));
    sMod += pack2.y * (1.0 - min(1.0, abs((0.08333333 - hue) / (-0.08333333))));
    sMod += pack2.z * (1.0 - min(1.0, abs((0.16666667 - hue) / (-0.16666667))));
    sMod += pack2.w * (1.0 - min(1.0, abs((0.33333333 - hue) / 0.16666667)));
    sMod += pack3.x * (1.0 - min(1.0, abs((0.5 - hue) / 0.16666667)));
    sMod += pack3.y * (1.0 - min(1.0, abs((0.66666667 - hue) / 0.16666667)));
    sMod += pack3.z * (1.0 - min(1.0, abs((0.83333333 - hue) / 0.16666667)));
    sMod += pack2.x * (1.0 - min(1.0, abs((1.0 - hue) / 0.16666667)));
    return sMod;
}

/// ReflectiveBumpMapping.fx Marty McFly — screen-space glossy relief reflections.
vec3 ApplyCreatorReflectiveBumpMapping(vec2 sampleUv, vec2 px, vec3 colorIn) {
    vec4 pack0 = cameraData.creatorReflectiveBumpMappingPack0;
    float strength = pack0.x;
    if (strength <= 1e-6) {
        return colorIn;
    }
    vec4 pack1 = cameraData.creatorReflectiveBumpMappingPack1;
    vec4 pack2 = cameraData.creatorReflectiveBumpMappingPack2;
    vec4 pack3 = cameraData.creatorReflectiveBumpMappingPack3;
    float depthFarPlane = max(pack3.w, 1.0);
    float scenedepth = CreatorRbmGetLinearDepth(sampleUv);
    vec3 surfaceNormals = CreatorRbmGetNormalFromDepth(sampleUv, px, depthFarPlane);
    vec2 colorOffset = 0.01 * px / max(scenedepth, 1e-4);
    float colorScale = 0.0002 / max(scenedepth, 1e-4) + 0.1;
    vec3 textureNormals = CreatorRbmGetNormalFromColor(sampleUv, colorOffset, colorScale, 1000.0, scenedepth);
    vec3 sceneNormals = CreatorRbmBlendNormals(surfaceNormals, textureNormals);
    sceneNormals = normalize(mix(surfaceNormals, sceneNormals, pack0.z));
    vec3 screenSpacePosition = CreatorRbmGetPosition(sampleUv, depthFarPlane);
    vec3 viewDirection = normalize(screenSpacePosition);
    vec3 color = colorIn;
    vec3 bump = vec3(0.0);
    int sampleCount = clamp(int(pack1.w + 0.5), 16, 128);
    float blurWidth = max(pack0.y, 0.0);
    for (int i = 1; i <= 128; ++i) {
        if (i > sampleCount) {
            break;
        }
        vec2 currentOffset = sampleUv
            + sceneNormals.xy * px * (float(i) / float(sampleCount)) * blurWidth;
        vec3 texelSample = texture(hdrSceneLinear, currentOffset).rgb;
        float depthDiff = smoothstep(0.005, 0.0, scenedepth - CreatorRbmGetLinearDepth(currentOffset));
        float colorWeight = smoothstep(pack1.y, pack1.z + 0.00001, dot(texelSample, vec3(0.299, 0.587, 0.114)));
        bump += mix(color, texelSample, depthDiff * colorWeight);
    }
    bump /= float(sampleCount);
    float cosphi = dot(-viewDirection, sceneNormals);
    float schlickReflectance = mix(pow(1.0 - cosphi, 5.0), 1.0, pack0.w);
    schlickReflectance = clamp(schlickReflectance * pack1.x, 0.0, 1.0);
    vec3 hsvcol = CreatorRbmRgb2Hsv(color);
    float colorMask = CreatorRbmGetHueMaskFull(hsvcol.x, pack2, pack3);
    colorMask = mix(1.0, colorMask, smoothstep(0.0, 0.2, hsvcol.y) * smoothstep(0.0, 0.1, hsvcol.z));
    vec3 reflected = mix(color, bump, schlickReflectance * colorMask);
    return mix(colorIn, reflected, strength);
}

vec3 ApplyCreatorFakeMotionBlur(vec2 sampleUv, vec2 px, vec3 colorIn) {
    vec4 pack = cameraData.creatorFakeMotionBlurPack0;
    float recall = pack.x;
    if (recall <= 1e-6) {
        return colorIn;
    }
    float softness = max(pack.y, 0.0);
    vec3 curr = colorIn;
    vec3 prevSingle = texture(fakeMotionBlurHistory, sampleUv).rgb;
    vec3 prev = prevSingle;
    vec3 diff3 = abs(prevSingle - curr) * 2.0;
    float diff = min(diff3.r + diff3.g + diff3.b, recall);
    const float weight[11] = float[11](
        0.082607, 0.040484, 0.038138, 0.034521, 0.030025,
        0.025094, 0.020253, 0.015553, 0.011533, 0.008218, 0.005627);
    vec3 blurredPrev = prev * weight[0];
    float pixelBlur = softness * 13.0 * diff * px.x;
    float pixelBlur2 = softness * 11.0 * diff * px.y;
    for (int z = 1; z < 11; ++z) {
        blurredPrev += texture(fakeMotionBlurHistory, sampleUv + vec2(float(z) * pixelBlur, 0.0)).rgb * weight[z];
        blurredPrev += texture(fakeMotionBlurHistory, sampleUv - vec2(float(z) * pixelBlur, 0.0)).rgb * weight[z];
        blurredPrev += texture(fakeMotionBlurHistory, sampleUv + vec2(0.0, float(z) * pixelBlur2)).rgb * weight[z];
        blurredPrev += texture(fakeMotionBlurHistory, sampleUv - vec2(0.0, float(z) * pixelBlur2)).rgb * weight[z];
    }
    return mix(curr, blurredPrev, diff + 0.1);
}

// Native port of CropResize/Resizer.fx (Edward Jeffrey; MIT). The original two centered
// resize passes collapse to one coordinate transform; point filtering preserves its
// intermediate virtual-pixel grid while linear mode leaves hardware interpolation active.
vec2 ApplyNativeCropScale(vec2 uv, out float coverage) {
    vec2 viewportSize = max(cameraData.viewportMetrics.xy, vec2(1.0));
    vec4 ci = cameraData.cropScaleContentIntermediate;
    vec4 ff = cameraData.cropScaleFinalFilterStrength;
    float strength = clamp(ff.w, 0.0, 1.0);
    coverage = 1.0;
    if (strength <= 1e-6) {
        return uv;
    }
    vec2 contentSize = vec2(ci.x > 0.5 ? ci.x : viewportSize.x,
                            ci.y > 0.5 ? ci.y : viewportSize.y);
    vec2 finalSize = vec2(ff.x > 0.5 ? ff.x : viewportSize.x,
                          ff.y > 0.5 ? ff.y : viewportSize.y);
    vec2 intermediateSize = vec2(ci.z > 0.5 ? ci.z : finalSize.x,
                                 ci.w > 0.5 ? ci.w : finalSize.y);
    contentSize = clamp(contentSize, vec2(1.0), viewportSize);
    finalSize = clamp(finalSize, vec2(1.0), viewportSize);
    intermediateSize = max(intermediateSize, vec2(1.0));
    vec2 finalUvSize = finalSize / viewportSize;
    vec2 finalMin = vec2(0.5) - finalUvSize * 0.5;
    vec2 finalMax = vec2(0.5) + finalUvSize * 0.5;
    coverage = (all(greaterThanEqual(uv, finalMin)) && all(lessThanEqual(uv, finalMax))) ? 1.0 : 0.0;
    vec2 localUv = (uv - finalMin) / max(finalUvSize, vec2(1e-6));
    if (ff.z < 0.5) {
        localUv = (floor(localUv * intermediateSize) + vec2(0.5)) / intermediateSize;
    }
    vec2 contentUvSize = contentSize / viewportSize;
    vec2 sourceUv = vec2(0.5) - contentUvSize * 0.5 + localUv * contentUvSize;
    vec2 texel = cameraData.viewportMetrics.zw;
    return clamp(mix(uv, sourceUv, strength), texel * 0.5, vec2(1.0) - texel * 0.5);
}

// Native port of Barbatos uFakeHDR v3.2 (CC0). The 4096x192 atlas contains
// three 64^3 strip LUTs stacked vertically: Natural, Vivid, and FakeHDR.
vec3 ApplyBarbatosFakeHdr(vec3 color) {
    float strength = clamp(cameraData.barbatosFakeHdrPack.y, 0.0, 2.0);
    if (strength <= 1e-6) {
        return color;
    }
    const float lutSize = 64.0;
    vec3 clampedColor = clamp(color, 0.0, 1.0);
    vec3 uvw = clampedColor * ((lutSize - 1.0) / lutSize) + (0.5 / lutSize);
    float slice = clampedColor.b * (lutSize - 1.0);
    float slice0 = floor(slice);
    float u0 = (slice0 + uvw.r) / lutSize;
    float u1 = (min(slice0 + 1.0, lutSize - 1.0) + uvw.r) / lutSize;
    float preset = clamp(floor(cameraData.barbatosFakeHdrPack.x + 0.5), 0.0, 2.0);
    float atlasV = (uvw.g + preset) / 3.0;
    vec3 lut0 = texture(barbatosLutAtlas, vec2(u0, atlasV)).rgb;
    vec3 lut1 = texture(barbatosLutAtlas, vec2(u1, atlasV)).rgb;
    vec3 graded = mix(color, mix(lut0, lut1, fract(slice)), strength);
    float magicDot = dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715));
    float noise1 = fract(52.9829189 * fract(magicDot));
    float noise2 = fract(52.9829189 * fract(magicDot + 0.036473855));
    return graded + vec3((noise1 + noise2 - 1.0) * (1.0 / 255.0));
}

// Raw Iron-owned post math. Reference packs are used only as a capability checklist;
// these operators use the engine's HDR/depth resources and bounded scene-linear guides.
vec3 RiBoundedSceneColor(vec2 uv) {
    vec3 hdr = max(texture(hdrSceneLinear, clamp(uv, vec2(0.0), vec2(1.0))).rgb, vec3(0.0));
    return hdr / (vec3(1.0) + hdr);
}

vec3 ApplyRiAdaptiveDeband(vec2 uv, vec2 px, vec3 color) {
    vec4 p = cameraData.riAdaptiveDebandPack;
    float strength = clamp(p.x, 0.0, 1.0);
    if (strength <= 1e-6) return color;
    float radius = clamp(p.y, 0.0, 128.0);
    float threshold = clamp(p.z, 0.0001, 0.1);
    int iterations = clamp(int(p.w + 0.5), 1, 3);
    vec3 guide = RiBoundedSceneColor(uv);
    vec3 correction = vec3(0.0);
    float phase = 6.28318530718 * fract(dot(gl_FragCoord.xy, vec2(0.754877666, 0.569840296)));
    for (int i = 0; i < 3; ++i) {
        if (i >= iterations) break;
        float angle = phase + float(i) * 2.39996323;
        vec2 direction = vec2(cos(angle), sin(angle));
        vec2 offset = direction * px * radius * (1.0 + float(i) * 0.618034);
        vec3 averageGuide = 0.5 * (RiBoundedSceneColor(uv + offset) + RiBoundedSceneColor(uv - offset));
        vec3 delta = averageGuide - guide;
        float difference = dot(abs(delta), vec3(0.299, 0.587, 0.114)) + length(delta.gb) * 0.25;
        float smoothRegion = 1.0 - smoothstep(threshold * 0.35, threshold * 1.8, difference);
        correction += delta * smoothRegion;
    }
    correction /= float(iterations);
    return clamp(color + correction * strength, 0.0, 1.0);
}

vec3 ApplyRiLocalSharpen(vec2 uv, vec2 px, vec3 color) {
    vec4 p = cameraData.riLocalSharpenPack;
    float strength = clamp(p.x, 0.0, 2.0);
    if (strength <= 1e-6) return color;
    vec2 d = px * clamp(p.y, 0.5, 4.0);
    vec3 centerGuide = RiBoundedSceneColor(uv);
    vec3 blurGuide = 0.25 * (
        RiBoundedSceneColor(uv + vec2(d.x, 0.0))
        + RiBoundedSceneColor(uv - vec2(d.x, 0.0))
        + RiBoundedSceneColor(uv + vec2(0.0, d.y))
        + RiBoundedSceneColor(uv - vec2(0.0, d.y)));
    vec3 highPass = centerGuide - blurGuide;
    float clampLimit = clamp(p.z, 0.0, 0.25);
    highPass = clamp(highPass, vec3(-clampLimit), vec3(clampLimit));
    float edgeMagnitude = length(highPass);
    float edgeLimiter = mix(1.0, 1.0 - smoothstep(0.04, 0.20, edgeMagnitude), clamp(p.w, 0.0, 1.0));
    return clamp(color + highPass * strength * edgeLimiter, 0.0, 1.0);
}

vec3 ApplyRiInkOutline(vec2 uv, vec2 px, vec3 color) {
    vec4 p0 = cameraData.riOutlinePack0;
    float strength = clamp(p0.x, 0.0, 1.0);
    if (strength <= 1e-6) return color;
    vec4 wobble = cameraData.riOutlineWobbleDebug;
    float time = cameraData.postProcessSecondary.z * clamp(wobble.y, 0.0, 5.0);
    vec2 warpedUv = uv + vec2(
        sin(time + uv.y * clamp(wobble.z, 1.0, 50.0)),
        cos(time + uv.x * clamp(wobble.z, 1.0, 50.0)))
        * px * clamp(wobble.x, 0.0, 10.0);
    vec2 d = px * clamp(p0.y, 0.0, 10.0);
    float centerDepth = texture(sceneDepth, warpedUv).r;
    vec3 centerGuide = RiBoundedSceneColor(warpedUv);
    const vec2 directions[8] = vec2[8](
        vec2(-1.0, 0.0), vec2(1.0, 0.0), vec2(0.0, -1.0), vec2(0.0, 1.0),
        vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));
    float depthDifference = 0.0;
    float colorDifference = 0.0;
    for (int i = 0; i < 8; ++i) {
        vec2 tapUv = warpedUv + directions[i] * d;
        depthDifference = max(depthDifference, abs(texture(sceneDepth, tapUv).r - centerDepth));
        colorDifference = max(colorDifference, length(RiBoundedSceneColor(tapUv) - centerGuide));
    }
    float depthEdge = smoothstep(p0.z, max(p0.z * 4.0, p0.z + 1e-5), depthDifference);
    float colorEdge = smoothstep(p0.w, max(p0.w * 2.0, p0.w + 1e-5), colorDifference);
    int method = clamp(int(cameraData.riOutlineColorMethod.w + 0.5), 0, 3);
    float mask = method == 0 ? depthEdge
        : method == 1 ? colorEdge
        : method == 2 ? min(depthEdge, colorEdge)
        : max(depthEdge, colorEdge);
    mask = clamp(mask * strength, 0.0, 1.0);
    if (wobble.w > 0.5) return vec3(mask);
    return mix(color, clamp(cameraData.riOutlineColorMethod.rgb, 0.0, 1.0), mask);
}

float RiOwnedHash21(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec3 ApplyRiSignalGlitch(vec2 uv, vec2 px, vec3 color) {
    vec4 p = cameraData.riSignalGlitchPack;
    float strength = clamp(p.x, 0.0, 1.0);
    if (strength <= 1e-6) return color;
    float blockSize = clamp(p.y, 2.0, 128.0);
    float timeCell = floor(cameraData.postProcessSecondary.z * clamp(p.w, 0.0, 12.0) * 12.0);
    float row = floor(gl_FragCoord.y / blockSize);
    float eventNoise = RiOwnedHash21(vec2(row, timeCell));
    float burst = smoothstep(0.58, 0.96, eventNoise);
    float signedNoise = RiOwnedHash21(vec2(row + 19.17, timeCell + 7.31)) * 2.0 - 1.0;
    vec2 offset = vec2(signedNoise * clamp(p.z, 0.0, 32.0) * px.x * strength * (0.25 + burst), 0.0);
    vec3 guide = RiBoundedSceneColor(uv);
    vec3 guidePositive = RiBoundedSceneColor(uv + offset);
    vec3 guideNegative = RiBoundedSceneColor(uv - offset);
    vec3 shifted = color;
    shifted.r += guidePositive.r - guide.r;
    shifted.b += guideNegative.b - guide.b;
    float steps = mix(255.0, mix(28.0, 9.0, strength), burst);
    shifted = floor(clamp(shifted, 0.0, 1.0) * steps + 0.5) / steps;
    return mix(color, shifted, strength * mix(0.20, 1.0, burst));
}

vec3 ApplyRiNightVision(vec2 uv, vec3 color) {
    vec4 p = cameraData.riNightVisionPack;
    float strength = clamp(p.x, 0.0, 1.0);
    if (strength <= 1e-6) return color;
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float timeCell = floor(cameraData.postProcessSecondary.z * 30.0);
    float grain = RiOwnedHash21(gl_FragCoord.xy + vec2(timeCell * 17.0, timeCell * 7.0)) - 0.5;
    vec2 centered = uv * 2.0 - 1.0;
    centered.x *= cameraData.viewportMetrics.x / max(cameraData.viewportMetrics.y, 1.0);
    float radial = dot(centered, centered);
    float vignette = 1.0 - clamp(p.w, 0.0, 1.0) * smoothstep(0.22, 1.45, radial);
    float scan = 0.985 + 0.015 * sin(gl_FragCoord.y * 1.5707963 + timeCell * 0.13);
    vec3 phosphor = vec3(0.10, 1.0, 0.24) * luma * clamp(p.y, 0.1, 4.0);
    phosphor += vec3(0.08, 0.42, 0.12) * grain * clamp(p.z, 0.0, 0.5);
    phosphor *= vignette * scan;
    return mix(color, clamp(phosphor, 0.0, 1.0), strength);
}

float RiSafeReciprocal(float value) {
    return 1.0 / (abs(value) < 1e-6 ? (value < 0.0 ? -1e-6 : 1e-6) : value);
}

vec3 RiSafePow(vec3 value, vec3 exponentValue) {
    return pow(max(value, vec3(0.0)), max(exponentValue, vec3(0.01)));
}

float RiLuma(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

float RiUvInside(vec2 uv) {
    return step(0.0, uv.x) * step(0.0, uv.y) * step(uv.x, 1.0) * step(uv.y, 1.0);
}

float RiBlendLum(vec3 color) {
    return dot(color, vec3(0.30, 0.59, 0.11));
}

float RiBlendSat(vec3 color) {
    return max(color.r, max(color.g, color.b)) - min(color.r, min(color.g, color.b));
}

vec3 RiClipBlendColor(vec3 color) {
    float luma = RiBlendLum(color);
    float minimum = min(color.r, min(color.g, color.b));
    float maximum = max(color.r, max(color.g, color.b));
    if (minimum < 0.0) color = vec3(luma) + (color - luma) * (luma * RiSafeReciprocal(luma - minimum));
    if (maximum > 1.0) color = vec3(luma) + (color - luma) * ((1.0 - luma) * RiSafeReciprocal(maximum - luma));
    return color;
}

vec3 RiSetBlendLum(vec3 color, float luma) {
    return RiClipBlendColor(color + (luma - RiBlendLum(color)));
}

vec3 RiSetBlendSat(vec3 color, float saturation) {
    float minimum = min(color.r, min(color.g, color.b));
    float maximum = max(color.r, max(color.g, color.b));
    float span = maximum - minimum;
    if (span <= 1e-6) return vec3(0.0);
    return (color - minimum) * (saturation * RiSafeReciprocal(span));
}

vec3 RiBlendLayer(int mode, vec3 base, vec3 layer, float opacity) {
    vec3 blended = layer;
    if (mode == 1) blended = min(base, layer);
    else if (mode == 2) blended = base * layer;
    else if (mode == 3) blended = 1.0 - (1.0 - base) / max(layer, vec3(1e-5));
    else if (mode == 4) blended = base + layer - 1.0;
    else if (mode == 5) blended = max(base, layer);
    else if (mode == 6) blended = 1.0 - (1.0 - base) * (1.0 - layer);
    else if (mode == 7) blended = base / max(1.0 - layer, vec3(1e-5));
    else if (mode == 8 || mode == 9) blended = base + layer;
    else if (mode == 10) blended = layer * layer / max(1.0 - base, vec3(1e-5));
    else if (mode == 11) {
        blended = mix(2.0 * base * layer, 1.0 - 2.0 * (1.0 - base) * (1.0 - layer), step(vec3(0.5), base));
    } else if (mode == 12) {
        blended = mix(base - (1.0 - 2.0 * layer) * base * (1.0 - base),
                      base + (2.0 * layer - 1.0) * (sqrt(max(base, vec3(0.0))) - base),
                      step(vec3(0.5), layer));
    } else if (mode == 13) {
        blended = mix(2.0 * base * layer, 1.0 - 2.0 * (1.0 - base) * (1.0 - layer), step(vec3(0.5), layer));
    } else if (mode == 14) {
        vec3 burn = 1.0 - (1.0 - base) / max(2.0 * layer, vec3(1e-5));
        vec3 dodge = base / max(2.0 * (1.0 - layer), vec3(1e-5));
        blended = mix(burn, dodge, step(vec3(0.5), layer));
    } else if (mode == 15) blended = base + 2.0 * layer - 1.0;
    else if (mode == 16) {
        blended = mix(min(base, 2.0 * layer), max(base, 2.0 * layer - 1.0), step(vec3(0.5), layer));
    } else if (mode == 17) {
        vec3 vivid = mix(1.0 - (1.0 - base) / max(2.0 * layer, vec3(1e-5)),
                         base / max(2.0 * (1.0 - layer), vec3(1e-5)),
                         step(vec3(0.5), layer));
        blended = step(vec3(0.5), vivid);
    } else if (mode == 18) blended = abs(base - layer);
    else if (mode == 19) blended = base + layer - 2.0 * base * layer;
    else if (mode == 20) blended = base - layer;
    else if (mode == 21) blended = base / max(layer, vec3(1e-5));
    else if (mode == 22) blended = layer / max(base, vec3(1e-5));
    else if (mode == 23) blended = (1.0 - base) / max(layer, vec3(1e-5));
    else if (mode == 24) blended = base * base / max(1.0 - layer, vec3(1e-5));
    else if (mode == 25) blended = base + layer - 0.5;
    else if (mode == 26) blended = base - layer + 0.5;
    else if (mode == 27) blended = RiSetBlendLum(RiSetBlendSat(layer, RiBlendSat(base)), RiBlendLum(base));
    else if (mode == 28) blended = RiSetBlendLum(RiSetBlendSat(base, RiBlendSat(layer)), RiBlendLum(base));
    else if (mode == 29) blended = RiSetBlendLum(layer, RiBlendLum(base));
    else if (mode == 30) blended = RiSetBlendLum(base, RiBlendLum(layer));
    return mix(base, blended, clamp(opacity, 0.0, 1.0));
}

float RiFontGlyphCoverage(int asciiCode, vec2 glyphUv) {
    int glyph = clamp(asciiCode - 32, 0, 95);
    vec2 cell = vec2(float(glyph % 14), float(glyph / 14));
    vec2 atlasUv = (cell + clamp(glyphUv, vec2(0.0), vec2(1.0))) / vec2(14.0, 7.0);
    return texture(riFontAtlasTexture, atlasUv).r * RiUvInside(glyphUv);
}

vec3 ApplyRiHq4x(vec2 uv, vec2 px, vec3 color) {
    vec4 p0 = cameraData.riHq4xPack0;
    vec4 p1 = cameraData.riHq4xPack1;
    float strength = clamp(p0.x, 0.0, 1.0);
    if (strength <= 1e-6) return color;
    vec2 d = px * clamp(p0.y, 0.1, 10.0);
    const vec2 direction[8] = vec2[8](
        vec2(-1.0, 0.0), vec2(1.0, 0.0), vec2(0.0, -1.0), vec2(0.0, 1.0),
        vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));
    vec3 center = RiBoundedSceneColor(uv);
    vec3 sum = center;
    float weightSum = 1.0;
    float minWeight = clamp(p1.x, 0.0, 1.0);
    float maxWeight = max(minWeight, clamp(p1.y, 0.0, 1.0));
    float hardness = clamp(p0.w, 0.0, 2.0);
    float lumaBias = max(p1.z, 0.001);
    for (int i = 0; i < 8; ++i) {
        vec3 tap = RiBoundedSceneColor(uv + direction[i] * d);
        float difference = dot(abs(tap - center), vec3(0.333333));
        float luminanceScale = lumaBias + 0.125 * (RiLuma(tap) + RiLuma(center));
        float weight = exp(-difference * hardness * 8.0 * RiSafeReciprocal(luminanceScale));
        weight = clamp(weight + clamp(p0.z, 0.0, 1.0) * 0.12, minWeight, maxWeight);
        sum += tap * weight;
        weightSum += weight;
    }
    vec3 filtered = sum * RiSafeReciprocal(weightSum);
    vec3 reconstructed = color + (filtered - center);
    return mix(color, clamp(reconstructed, 0.0, 1.0), strength);
}

vec3 RiRgbToHsl(vec3 color) {
    float maximum = max(color.r, max(color.g, color.b));
    float minimum = min(color.r, min(color.g, color.b));
    float chroma = maximum - minimum;
    float lightness = 0.5 * (maximum + minimum);
    float hue = 0.0;
    if (chroma > 1e-6) {
        if (maximum == color.r) hue = mod((color.g - color.b) / chroma, 6.0);
        else if (maximum == color.g) hue = (color.b - color.r) / chroma + 2.0;
        else hue = (color.r - color.g) / chroma + 4.0;
        hue = fract(hue / 6.0);
    }
    float saturation = chroma * RiSafeReciprocal(1.0 - abs(2.0 * lightness - 1.0));
    return vec3(hue, clamp(saturation, 0.0, 1.0), lightness);
}

vec3 RiHueToRgb(float hue) {
    return clamp(vec3(abs(hue * 6.0 - 3.0) - 1.0,
                      2.0 - abs(hue * 6.0 - 2.0),
                      2.0 - abs(hue * 6.0 - 4.0)), 0.0, 1.0);
}

vec3 RiHslToRgb(vec3 hsl) {
    float chroma = (1.0 - abs(2.0 * hsl.z - 1.0)) * hsl.y;
    return (RiHueToRgb(fract(hsl.x)) - 0.5) * chroma + hsl.z;
}

vec3 RiHslAnchor(int index) {
    if (index == 0 || index == 8) return cameraData.riHslAnchor0.rgb;
    if (index == 1) return cameraData.riHslAnchor1.rgb;
    if (index == 2) return cameraData.riHslAnchor2.rgb;
    if (index == 3) return cameraData.riHslAnchor3.rgb;
    if (index == 4) return cameraData.riHslAnchor4.rgb;
    if (index == 5) return cameraData.riHslAnchor5.rgb;
    if (index == 6) return cameraData.riHslAnchor6.rgb;
    return cameraData.riHslAnchor7.rgb;
}

vec3 ApplyRiHslShift(vec3 color) {
    float strength = clamp(cameraData.riHslAnchor0.w, 0.0, 1.0);
    if (strength <= 1e-6) return color;
    vec3 sourceHsl = RiRgbToHsl(clamp(color, 0.0, 1.0));
    float hueDegrees = sourceHsl.x * 360.0;
    const float boundary[9] = float[9](0.0, 30.0, 60.0, 120.0, 180.0, 240.0, 270.0, 300.0, 360.0);
    int segment = 0;
    for (int i = 0; i < 8; ++i) {
        if (hueDegrees >= boundary[i]) segment = i;
    }
    float segmentWeight = clamp((hueDegrees - boundary[segment])
        * RiSafeReciprocal(boundary[segment + 1] - boundary[segment]), 0.0, 1.0);
    vec3 anchor0 = RiRgbToHsl(RiHslAnchor(segment));
    vec3 anchor1 = RiRgbToHsl(RiHslAnchor(segment + 1));
    if (anchor1.x < anchor0.x) anchor1.x += 1.0;
    vec3 target = mix(anchor0, anchor1, segmentWeight);
    float chromaInfluence = sourceHsl.y * (1.0 - sourceHsl.z);
    vec3 shiftedHsl;
    shiftedHsl.x = fract(target.x);
    shiftedHsl.y = clamp(sourceHsl.y * mix(1.0, target.y * 2.0, chromaInfluence), 0.0, 1.0);
    shiftedHsl.z = clamp(sourceHsl.z * (1.0 + (target.z - 0.5) * 2.0 * chromaInfluence), 0.0, 1.0);
    return mix(color, clamp(RiHslToRgb(shiftedHsl), 0.0, 1.0), strength);
}

vec3 RiAcesFitted(vec3 color) {
    vec3 inputColor = vec3(
        dot(color, vec3(0.59719, 0.35458, 0.04823)),
        dot(color, vec3(0.07600, 0.90834, 0.01566)),
        dot(color, vec3(0.02840, 0.13383, 0.83777)));
    vec3 a = inputColor * (inputColor + 0.0245786) - 0.000090537;
    vec3 b = inputColor * (0.983729 * inputColor + 0.432951) + 0.238081;
    vec3 fitted = a / max(b, vec3(1e-6));
    return clamp(vec3(
        dot(fitted, vec3(1.60475, -0.53108, -0.07367)),
        dot(fitted, vec3(-0.10208, 1.10813, -0.00605)),
        dot(fitted, vec3(-0.00327, -0.07276, 1.07602))), 0.0, 1.0);
}

vec3 ApplyRiLevelsPlus(vec3 color) {
    vec4 p0 = cameraData.riLevelsPlusPack0;
    float strength = clamp(p0.w, 0.0, 1.0);
    if (strength <= 1e-6) return color;
    vec3 denominator = cameraData.riLevelsPlusPack1.rgb - p0.rgb;
    denominator = sign(denominator + vec3(1e-8)) * max(abs(denominator), vec3(1e-5));
    vec3 shifted = color + cameraData.riLevelsPlusPack5.rgb * cameraData.riLevelsPlusPack1.w;
    vec3 normalized = (shifted - p0.rgb) / denominator;
    vec3 mapped = RiSafePow(normalized, cameraData.riLevelsPlusPack2.rgb);
    mapped = mapped * (cameraData.riLevelsPlusPack4.rgb - cameraData.riLevelsPlusPack3.rgb)
        + cameraData.riLevelsPlusPack3.rgb;
    vec3 unclipped = mapped;
    int acesMode = clamp(int(cameraData.riLevelsPlusPack2.w + 0.5), 0, 3);
    vec3 acesInput = max(mapped * cameraData.riLevelsPlusPack6.rgb, vec3(0.0));
    if (acesMode == 1) {
        mapped = (acesInput * (15.8 * acesInput + 2.12))
            / max(acesInput * (1.2 * acesInput + 5.92) + 1.9, vec3(1e-6));
    } else if (acesMode == 2) {
        mapped = (acesInput * (0.98 * acesInput + 0.3))
            / max(acesInput * (0.22 * acesInput) + 0.025, vec3(1e-6));
    } else if (acesMode == 3) {
        mapped = RiAcesFitted(acesInput);
    }
    if (cameraData.riLevelsPlusPack3.w > 0.5) {
        bool someHigh = any(greaterThan(unclipped, vec3(1.0)));
        bool allHigh = all(greaterThan(unclipped, vec3(1.0)));
        bool someLow = any(lessThan(unclipped, vec3(0.0)));
        bool allLow = all(lessThan(unclipped, vec3(0.0)));
        if (someHigh) mapped = vec3(1.0, 1.0, 0.0);
        if (allHigh) mapped = vec3(1.0, 0.0, 0.0);
        if (someLow) mapped = vec3(0.0, 1.0, 1.0);
        if (allLow) mapped = vec3(0.0, 0.0, 1.0);
    }
    return mix(color, clamp(mapped, 0.0, 1.0), strength);
}

vec3 ApplyRiLightDof(vec2 uv, vec2 px, vec3 color) {
    vec4 p0 = cameraData.riLightDofPack0;
    float strength = clamp(p0.x, 0.0, 1.0);
    if (strength <= 1e-6) return color;
    vec4 p1 = cameraData.riLightDofPack1;
    float focusDepth = mix(clamp(p0.w, 0.0, 1.0),
                           texture(sceneDepth, clamp(p1.yz, vec2(0.0), vec2(1.0))).r,
                           step(0.5, p1.x));
    float centerDepth = texture(sceneDepth, uv).r;
    float coc = clamp(abs(centerDepth - focusDepth) * clamp(p0.z, 0.0, 10.0), 0.0, 1.0) * strength;
    if (coc <= 1e-5) return color;
    const vec2 disk[8] = vec2[8](
        vec2(-0.326, -0.406), vec2(-0.840, -0.074), vec2(-0.696, 0.457), vec2(-0.203, 0.621),
        vec2(0.962, -0.195), vec2(0.473, -0.480), vec2(0.519, 0.767), vec2(0.185, -0.893));
    vec3 centerGuide = RiBoundedSceneColor(uv);
    vec3 blurGuide = vec3(0.0);
    vec2 spread = px * clamp(p0.y, 1.0, 25.0) * coc;
    for (int i = 0; i < 8; ++i) {
        blurGuide += RiBoundedSceneColor(uv + disk[i] * spread);
    }
    blurGuide *= 0.125;
    float fringe = centerDepth < focusDepth ? cameraData.riLightDofPack2.x : p1.w;
    vec2 caOffset = vec2(spread.x * clamp(fringe, 0.0, 1.0), 0.0);
    blurGuide.r = RiBoundedSceneColor(uv + caOffset).r;
    blurGuide.b = RiBoundedSceneColor(uv - caOffset).b;
    return mix(color, clamp(color + (blurGuide - centerGuide), 0.0, 1.0), coc);
}

vec3 ApplyRiMagicBloom(vec2 uv, vec2 px, vec3 color) {
    vec4 p0 = cameraData.riMagicBloomPack0;
    float strength = clamp(p0.x, 0.0, 1.0);
    if (strength <= 1e-6) return color;
    vec4 p1 = cameraData.riMagicBloomPack1;
    const vec2 taps[12] = vec2[12](
        vec2(-1.0, 0.0), vec2(1.0, 0.0), vec2(0.0, -1.0), vec2(0.0, 1.0),
        vec2(-0.707, -0.707), vec2(0.707, -0.707), vec2(-0.707, 0.707), vec2(0.707, 0.707),
        vec2(-2.4, 0.0), vec2(2.4, 0.0), vec2(0.0, -2.4), vec2(0.0, 2.4));
    vec3 bloom = vec3(0.0);
    vec2 radius = px * clamp(p1.y, 0.5, 64.0);
    float thresholdPower = clamp(p0.z, 1.0, 10.0);
    for (int i = 0; i < 12; ++i) {
        vec3 sampleColor = RiBoundedSceneColor(uv + taps[i] * radius);
        bloom += pow(max(sampleColor, vec3(0.0)), vec3(thresholdPower));
    }
    bloom *= (1.0 / 12.0) * clamp(p0.y, 0.0, 10.0);
    float adaptation = max(RiLuma(RiBoundedSceneColor(uv)) * clamp(p1.z, 0.0, 3.0), 0.05);
    bloom *= clamp(p0.w, 0.0, 4.0) * RiSafeReciprocal(adaptation);
    bloom = bloom / (vec3(1.0) + bloom);
    vec3 dirt = texture(riMagicBloomDirtTexture, uv).rgb;
    bloom *= 1.0 + dirt * clamp(p1.x, 0.0, 1.0);
    return RiBlendLayer(6, color, clamp(bloom, 0.0, 1.0), strength);
}

vec3 ApplyRiUiMask(vec2 uv, vec3 backup, vec3 color) {
    vec4 p0 = cameraData.riUiMaskPack0;
    float strength = clamp(p0.x, 0.0, 1.0);
    if (strength <= 1e-6) return color;
    vec3 maskRgb = clamp(texture(riUiMaskTexture, uv).rgb, 0.0, 1.0);
    vec3 enabled = clamp(vec3(p0.z, p0.w, cameraData.riUiMaskPack1.x), 0.0, 1.0);
    float mask = 1.0 - (1.0 - maskRgb.r * enabled.r)
        * (1.0 - maskRgb.g * enabled.g)
        * (1.0 - maskRgb.b * enabled.b);
    mask *= clamp(p0.y, 0.0, 1.0) * strength;
    if (cameraData.riUiMaskPack1.y > 0.5) return vec3(mask);
    return mix(color, backup, clamp(mask, 0.0, 1.0));
}


void main() {
    vec2 sampleUv = vec2(vUv.x, 1.0 - vUv.y);
    float cropCoverage = 1.0;
    sampleUv = ApplyNativeCropScale(sampleUv, cropCoverage);
    vec2 px = vec2(cameraData.viewportMetrics.z, cameraData.viewportMetrics.w);
    if (cameraData.reshadeDisplayDepthPack.y > 1.5) {
        float cropStrength = clamp(cameraData.cropScaleFinalFilterStrength.w, 0.0, 1.0);
        fragColor = vec4(texture(hdrSceneLinear, sampleUv).rgb * mix(1.0, cropCoverage, cropStrength), 1.0);
        return;
    }
    float ditherAmt = clamp(cameraData.presentationColorGrading.y, 0.0, 1.0);
    float debandAmt = clamp(cameraData.presentationColorGrading.z, 0.0, 0.12);
    float vignetteAmt = clamp(cameraData.presentationColorGrading.w, 0.0, 1.0);
    float filmGrainAmt = clamp(cameraData.presentationExtra.x, 0.0, 0.5);
    float aspectView = cameraData.viewportMetrics.x / max(cameraData.viewportMetrics.y, 1.0);
    vec3 beforeSplit = EvaluatePrePostProcessChain(sampleUv, px);
    vec3 mapped = ApplySweetFxFxaa(
        sampleUv,
        px,
        ditherAmt,
        debandAmt,
        vignetteAmt,
        filmGrainAmt,
        aspectView,
        cameraData.sweetFxFxaaPack);
    mapped = ApplySweetFxSmaa(sampleUv, px, mapped, cameraData.sweetFxSmaaPack0, cameraData.sweetFxSmaaPack1);
    mapped = ApplyReShadeDaltonize(mapped, cameraData.reshadeDaltonizePack);
    mapped = ApplyReShadeDisplayDepth(sampleUv, px, mapped, cameraData.reshadeDisplayDepthPack);
    mapped = ApplyReShadeLut(mapped, cameraData.reshadeLutPack);
    mapped = ApplyBarbatosFakeHdr(mapped);
    mapped = ApplyPd80BonusLutPack(sampleUv, mapped);
    mapped = ApplyPd80CinetoolsLut(sampleUv, mapped);
    mapped = ApplyPd80LutCreator(sampleUv, mapped);
    mapped = ApplyPd80ColorGradients(sampleUv, mapped);
    mapped = ApplyPd80RtCorrectContrast(sampleUv, mapped);
    mapped = ApplyPd80RtCorrectColor(sampleUv, mapped);
    mapped = ApplyPd80FilmicAdaptation(sampleUv, mapped);
    mapped = ApplyPd80HqBloom(sampleUv, px, mapped);
    mapped = ApplyPd80SelectiveColorV2(mapped);
    mapped = ApplyPd80LumaFade(beforeSplit, mapped);
    mapped = ApplyPd80FilmGrain(sampleUv, px, mapped);
    mapped = ApplyPd80DepthSlicer(sampleUv, mapped);
    mapped = ApplyPd80ColorGamut(mapped);
    mapped = ApplyPd80ColorSpaceCurves(sampleUv, mapped);
    mapped = ApplyCreatorFilmicPass(mapped, cameraData.creatorFilmicPassPack);
    mapped = ApplyCreatorColourfulness(mapped, cameraData.creatorColourfulnessPack);
    mapped = ApplyCreatorFilmGrain2(
        sampleUv,
        mapped,
        cameraData.creatorFilmGrain2Pack,
        cameraData.viewportMetrics.xy,
        aspectView,
        cameraData.postProcessSecondary.z);
    mapped = ApplyCreatorReflectiveBumpMapping(sampleUv, px, mapped);
    mapped = ApplyCreatorAmbientLight(sampleUv, px, mapped);
    mapped = ApplyCreatorRingDof(sampleUv, px, mapped);
    mapped = ApplyCreatorFakeMotionBlur(sampleUv, px, mapped);
    mapped = ApplySweetFxCompare(beforeSplit, mapped, sampleUv, cameraData.sweetFxComparePack);
    mapped = ApplySweetFxSplitscreen(beforeSplit, mapped, sampleUv, cameraData.sweetFxSplitscreenModeStrength);
    mapped = ApplyRiLightDof(sampleUv, px, mapped);
    mapped = ApplyRiMagicBloom(sampleUv, px, mapped);
    mapped = ApplyRiAdaptiveDeband(sampleUv, px, mapped);
    mapped = ApplyRiLocalSharpen(sampleUv, px, mapped);
    mapped = ApplyRiHq4x(sampleUv, px, mapped);
    mapped = ApplyRiHslShift(mapped);
    mapped = ApplyRiLevelsPlus(mapped);
    mapped = ApplyRiSignalGlitch(sampleUv, px, mapped);
    mapped = ApplyRiNightVision(sampleUv, mapped);
    mapped = ApplyRiInkOutline(sampleUv, px, mapped);
    mapped = ApplyRiUiMask(sampleUv, beforeSplit, mapped);
    mapped *= mix(1.0, cropCoverage, clamp(cameraData.cropScaleFinalFilterStrength.w, 0.0, 1.0));
    fragColor = vec4(clamp(mapped, 0.0, 1.0), 1.0);
}
