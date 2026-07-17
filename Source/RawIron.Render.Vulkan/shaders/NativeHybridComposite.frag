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

float LiteHash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

vec3 ApplyLiteLumaCurve(vec3 color, float strength) {
    float amount = clamp(strength, 0.0, 1.0);
    if (amount <= 1e-5) {
        return color;
    }
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float curved = luma * (luma * (1.5 - luma) + 0.5);
    return clamp(color * (mix(luma, curved, amount) / max(luma, 1e-5)), 0.0, 1.0);
}

vec3 ApplyLiteLiftGammaGain(vec3 color) {
    float amount = clamp(cameraData.lggLiftMix.w, 0.0, 1.0);
    if (amount <= 1e-5) {
        return color;
    }
    vec3 lift = cameraData.lggLiftMix.xyz;
    vec3 gamma = max(cameraData.lggGammaRgb.xyz, vec3(1e-4));
    vec3 gain = cameraData.lggGainRgb.xyz;
    vec3 graded = color * (1.5 - 0.5 * lift) + 0.5 * lift - 0.5;
    graded = pow(clamp(graded * gain, 0.0, 1.0), 1.0 / gamma);
    return clamp(mix(color, graded, amount), 0.0, 1.0);
}

vec3 ApplyLiteVibrance(vec3 color) {
    float amount = clamp(cameraData.vibranceBalanceAmount.w, -1.0, 1.0);
    if (abs(amount) <= 1e-5) {
        return color;
    }
    float luma = dot(color, vec3(0.212656, 0.715158, 0.072186));
    float colorRange = max(color.r, max(color.g, color.b)) - min(color.r, min(color.g, color.b));
    vec3 balance = cameraData.vibranceBalanceAmount.xyz * amount;
    vec3 channelMix = 1.0 + balance * (1.0 - sign(balance) * colorRange);
    return clamp(mix(vec3(luma), color, channelMix), 0.0, 1.0);
}

vec3 ApplyLitePresentationFx(vec3 color, vec2 uv) {
    float timeSeconds = cameraData.postProcessSecondary.z;
    float scanlineAmount = clamp(cameraData.postProcessPrimary.y, 0.0, 0.2);
    float noiseAmount = clamp(cameraData.postProcessPrimary.x, 0.0, 0.3);
    float staticFade = clamp(cameraData.postProcessSecondary.y, 0.0, 1.0);
    float filmGrain = clamp(cameraData.presentationExtra.x, 0.0, 0.5);
    float deband = clamp(cameraData.presentationColorGrading.z, 0.0, 0.12);

    float scanPhase = (gl_FragCoord.y + timeSeconds * 48.0) * 0.14;
    color *= 1.0 - scanlineAmount * (0.5 + 0.5 * sin(scanPhase));

    float noise = LiteHash21(gl_FragCoord.xy + vec2(timeSeconds * 39.0, timeSeconds * 17.0)) - 0.5;
    color += vec3(noise * noiseAmount * 1.7);
    if (filmGrain > 1e-5) {
        float inverseLuma = 1.0 - dot(color, vec3(0.333333));
        float grainMask = mix(0.35, 1.0, pow(clamp(inverseLuma, 0.0, 1.0), 3.0));
        color *= 1.0 + noise * filmGrain * grainMask * 1.4;
    }
    if (deband > 1e-5) {
        float triangular = noise - (LiteHash21(gl_FragCoord.yx + vec2(19.19, timeSeconds * 7.0)) - 0.5);
        color += vec3(triangular * deband * (1.0 / 255.0));
    }

    float tintMix = clamp(cameraData.postProcessTint.w, 0.0, 1.0);
    color = mix(color, color * clamp(cameraData.postProcessTint.rgb, 0.0, 1.0), tintMix);
    float staticPulse = 0.85 + 0.15 * LiteHash21(vec2(timeSeconds * 23.0, gl_FragCoord.y * 0.03125));
    color = mix(color, vec3(staticPulse), staticFade);
    return clamp(color, 0.0, 1.0);
}

vec3 LinearToSrgb(vec3 color) {
    vec3 c = max(color, vec3(0.0));
    return pow(c, vec3(1.0 / 2.2));
}

vec3 ApplyCasSharpen(vec2 uv, vec2 texel, vec3 center, float amount, float contrastAdaptation) {
    if (amount <= 1e-4) {
        return center;
    }
    vec3 north = texture(hdrSceneLinear, clamp(uv + vec2(0.0, texel.y), texel * 0.5, vec2(1.0) - texel * 0.5)).rgb;
    vec3 south = texture(hdrSceneLinear, clamp(uv - vec2(0.0, texel.y), texel * 0.5, vec2(1.0) - texel * 0.5)).rgb;
    vec3 east = texture(hdrSceneLinear, clamp(uv + vec2(texel.x, 0.0), texel * 0.5, vec2(1.0) - texel * 0.5)).rgb;
    vec3 west = texture(hdrSceneLinear, clamp(uv - vec2(texel.x, 0.0), texel * 0.5, vec2(1.0) - texel * 0.5)).rgb;
    vec3 blur = (north + south + east + west) * 0.25;
    float edge = length(center - blur);
    float edgeScale = mix(1.0, 0.55, clamp(contrastAdaptation, 0.0, 1.0) * smoothstep(0.02, 0.14, edge));
    vec3 sharpened = center + (center - blur) * amount * 2.2 * edgeScale;
    return max(sharpened, vec3(0.0));
}

// Native port of CropResize/Resizer.fx (Edward Jeffrey; MIT).
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

// Native port of Barbatos uFakeHDR v3.2 (CC0), including its three-row 64^3
// strip LUT selection, blue-slice interpolation, strength, and triangular dither.
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
    vec2 texel = vec2(cameraData.viewportMetrics.z, cameraData.viewportMetrics.w);
    vec2 centeredUv = sampleUv * 2.0 - 1.0;
    float radial = dot(centeredUv, centeredUv);
    float barrelDistortion = clamp(cameraData.postProcessPrimary.z, 0.0, 0.2);
    sampleUv = clamp(sampleUv + centeredUv * radial * barrelDistortion * 0.08,
                     texel * 0.5,
                     vec2(1.0) - texel * 0.5);
    vec3 hdr = max(texture(hdrSceneLinear, sampleUv).rgb, vec3(0.0));
    float chromaticAberration = clamp(cameraData.postProcessPrimary.w, 0.0, 0.05);
    if (chromaticAberration > 1e-5) {
        vec2 fringeOffset = centeredUv * chromaticAberration * 0.08;
        hdr.r = texture(hdrSceneLinear, clamp(sampleUv + fringeOffset, vec2(0.0), vec2(1.0))).r;
        hdr.b = texture(hdrSceneLinear, clamp(sampleUv - fringeOffset, vec2(0.0), vec2(1.0))).b;
    }
    float blurAmount = clamp(cameraData.postProcessSecondary.x, 0.0, 0.05);
    if (blurAmount > 1e-5) {
        vec2 spread = texel * (1.0 + blurAmount * 36.0);
        vec3 crossBlur = texture(hdrSceneLinear, sampleUv + vec2(spread.x, 0.0)).rgb
            + texture(hdrSceneLinear, sampleUv - vec2(spread.x, 0.0)).rgb
            + texture(hdrSceneLinear, sampleUv + vec2(0.0, spread.y)).rgb
            + texture(hdrSceneLinear, sampleUv - vec2(0.0, spread.y)).rgb;
        hdr = mix(hdr, crossBlur * 0.25, clamp(blurAmount * 12.0, 0.0, 0.65));
    }
    float casAmount = clamp(cameraData.presentationTuning.x, 0.0, 1.0);
    float casContrastAdaptation = clamp(cameraData.presentationTuning.y, 0.0, 1.0);
    hdr = ApplyCasSharpen(sampleUv, texel, hdr, casAmount, casContrastAdaptation);

    // Hybrid HDR radiance already includes renderTuning.x exposure in NativeScenePreview.frag.
    const bool hybridHdrRadiance = cameraData.postProcessSecondary.w > 0.5;
    float exposure = max(cameraData.renderTuning.x, 0.01);
    float contrast = max(cameraData.renderTuning.y, 0.01);
    float saturation = max(cameraData.renderTuning.z, 0.01);
    if (!hybridHdrRadiance) {
        hdr *= exposure;
    }

    float bloomStrength = clamp(cameraData.presentationTuning.z, 0.0, 1.0);
    float bloomThreshold = max(cameraData.presentationTuning.w, 0.0);
    if (bloomStrength > 1e-4) {
        float luma = dot(hdr, vec3(0.2126, 0.7152, 0.0722));
        float bloom = max(luma - bloomThreshold, 0.0) * bloomStrength;
        hdr += hdr * bloom * 0.35;
    }

    vec3 mapped = TonemapAcesApprox(hdr);
    mapped = ApplyColorGrade(mapped, contrast, saturation);
    mapped = ApplyLiteLumaCurve(mapped, cameraData.presentationColorGrading.x);
    mapped = ApplyLiteLiftGammaGain(mapped);
    mapped = ApplyLiteVibrance(mapped);

    float vignetteAmt = clamp(cameraData.presentationColorGrading.w, 0.0, 1.0);
    if (vignetteAmt > 1e-4) {
        vec2 centered = sampleUv * 2.0 - 1.0;
        float aspect = cameraData.viewportMetrics.x / max(cameraData.viewportMetrics.y, 1.0);
        centered.x *= aspect;
        float vig = 1.0 - dot(centered, centered) * vignetteAmt * 0.45;
        mapped *= clamp(vig, 0.0, 1.0);
    }

    vec3 uiBackup = clamp(LinearToSrgb(mapped), 0.0, 1.0);
    mapped = ApplyLitePresentationFx(mapped, sampleUv);
    mapped = ApplyRiLightDof(sampleUv, texel, mapped);
    mapped = ApplyRiMagicBloom(sampleUv, texel, mapped);
    mapped = ApplyRiAdaptiveDeband(sampleUv, texel, mapped);
    mapped = ApplyRiLocalSharpen(sampleUv, texel, mapped);
    vec3 srgb = clamp(LinearToSrgb(mapped), 0.0, 1.0);
    srgb = ApplyBarbatosFakeHdr(srgb);
    srgb = ApplyRiHq4x(sampleUv, texel, srgb);
    srgb = ApplyRiHslShift(srgb);
    srgb = ApplyRiLevelsPlus(srgb);
    srgb = ApplyRiSignalGlitch(sampleUv, texel, srgb);
    srgb = ApplyRiNightVision(sampleUv, srgb);
    srgb = ApplyRiInkOutline(sampleUv, texel, srgb);
    float outputDither = clamp(cameraData.presentationColorGrading.y, 0.0, 1.0);
    if (outputDither > 1e-5) {
        float triangular = LiteHash21(gl_FragCoord.xy + 0.37)
            - LiteHash21(gl_FragCoord.yx + vec2(7.31, 13.17));
        srgb += vec3(triangular * outputDither * (1.0 / 255.0));
    }
    srgb = ApplyRiUiMask(sampleUv, uiBackup, srgb);
    srgb *= mix(1.0, cropCoverage, clamp(cameraData.cropScaleFinalFilterStrength.w, 0.0, 1.0));
    fragColor = vec4(clamp(srgb, 0.0, 1.0), 1.0);
}
