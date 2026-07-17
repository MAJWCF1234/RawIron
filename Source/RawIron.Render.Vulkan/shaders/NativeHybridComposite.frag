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
} cameraData;

layout(set = 1, binding = 0) uniform sampler2D hdrSceneLinear;
layout(set = 1, binding = 1) uniform sampler2D sceneDepth;
layout(set = 1, binding = 2) uniform sampler2D fakeMotionBlurHistory;
layout(set = 2, binding = 0) uniform sampler2D sweetFxLayerTexture;
layout(set = 3, binding = 0) uniform sampler2D sweetFxSmaaAreaTexture;
layout(set = 4, binding = 0) uniform sampler2D sweetFxSmaaSearchTexture;
layout(set = 5, binding = 0) uniform sampler2D reshadeLutTexture;

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

void main() {
    vec2 sampleUv = vec2(vUv.x, 1.0 - vUv.y);
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

    mapped = ApplyLitePresentationFx(mapped, sampleUv);
    vec3 srgb = clamp(LinearToSrgb(mapped), 0.0, 1.0);
    float outputDither = clamp(cameraData.presentationColorGrading.y, 0.0, 1.0);
    if (outputDither > 1e-5) {
        float triangular = LiteHash21(gl_FragCoord.xy + 0.37)
            - LiteHash21(gl_FragCoord.yx + vec2(7.31, 13.17));
        srgb += vec3(triangular * outputDither * (1.0 / 255.0));
    }
    fragColor = vec4(clamp(srgb, 0.0, 1.0), 1.0);
}
